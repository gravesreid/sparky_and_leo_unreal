#include "SparkyGameMode.h"
#include "Enemy.h"
#include "LevelGeo.h"
#include "Levels.h"
#include "Props.h"
#include "SparkyCharacter.h"
#include "SparkyHUD.h"
#include "SparkyLeo.h"
#include "Story.h"

#include "Components/AudioComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float MusicVolume = 0.5f;
	enum class EItem : uint8 { Start, Chapter, Controls, Music, Calibrate, Quit, Resume, Restart, ToTitle, Retry };

	TArray<EItem> Items(EGameState S)
	{
		TArray<EItem> Out;
		switch (S)
		{
		case EGameState::Title: Out = {EItem::Start, EItem::Chapter, EItem::Controls, EItem::Music, EItem::Calibrate, EItem::Quit}; break;
		case EGameState::Paused: Out = {EItem::Resume, EItem::Restart, EItem::Controls, EItem::Music, EItem::Calibrate, EItem::ToTitle}; break;
		case EGameState::GameOver: Out = {EItem::Retry, EItem::ToTitle}; break;
		default: break;
		}
#if !PLATFORM_LINUX
		Out.Remove(EItem::Calibrate);  // the N64 pad is read from /dev/input, Linux only; elsewhere gamepads go through Unreal
#endif
		return Out;
	}

	FString SettingsFile() { return FPaths::ProjectSavedDir() / TEXT("SparkyLeo.ini"); }
}

ASparkyGameMode::ASparkyGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
	DefaultPawnClass = ASparkyCharacter::StaticClass();
	HUDClass = ASparkyHUD::StaticClass();
}

ASparkyGameMode* ASparkyGameMode::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	return W ? Cast<ASparkyGameMode>(W->GetAuthGameMode()) : nullptr;
}

ASparkyCharacter* ASparkyGameMode::Player() const
{
	return ASparkyCharacter::Get(this);
}

int32 ASparkyGameMode::CageFoes() const
{
	// only fighters inside the cage count: alley cats left behind in the alley don't hold up the rounds
	int32 N = 0;
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && It->GetActorLocation().Size2D() < 1100.f)
		{
			++N;
		}
	}
	return N;
}

void ASparkyGameMode::StartPlay()
{
	Super::StartPlay();
	LoadSounds();
	LoadSettings();
	if (FParse::Param(FCommandLine::Get(), TEXT("NoMusic")))
	{
		bMusicOn = false;
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("KeyboardOnly")))
	{
		bKeyboardOnly = true;
	}
	int32 Start = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("Chapter="), Start))
	{
		StartChapter(FMath::Clamp(Start, 1, Story::NumChapters), !FParse::Param(FCommandLine::Get(), TEXT("SkipStory")));
	}
	else
	{
		GoToTitle();
	}
}

// ---------------------------------------------------------------- sound

void ASparkyGameMode::LoadSounds()
{
	Falloff = NewObject<USoundAttenuation>(this);  // 3D sounds fade out over 40 m
	Falloff->Attenuation.bAttenuate = true;
	Falloff->Attenuation.bSpatialize = true;
	Falloff->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
	Falloff->Attenuation.AttenuationShapeExtents = FVector(600.f, 0, 0);
	Falloff->Attenuation.FalloffDistance = 4000.f;
	SfxSounds.SetNum(int32(ESfx::Count));
	for (int32 i = 0; i < int32(ESfx::Count); ++i)
	{
		for (const TCHAR* Name : SfxNames(ESfx(i)))
		{
			if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), Name, Name)))
			{
				SfxSounds[i].Add(S);
				SoundCache.Add(S);
			}
			else
			{
				UE_LOG(LogSparky, Warning, TEXT("Missing sound %s - run ./build_content.sh"), Name);
			}
		}
	}
	Music = NewObject<UAudioComponent>(this);
	Music->bAutoDestroy = false;
	Music->bIsUISound = true;
	Music->RegisterComponentWithWorld(GetWorld());
}

void ASparkyGameMode::Sfx(const UObject* Ctx, ESfx S, const FVector& At, float Volume)
{
	ASparkyGameMode* M = Get(Ctx);
	if (!M || !M->SfxSounds.IsValidIndex(int32(S)) || M->SfxSounds[int32(S)].Num() == 0)
	{
		return;
	}
	static const bool bLogSfx = FParse::Param(FCommandLine::Get(), TEXT("LogSfx"));
	if (bLogSfx)
	{
		UE_LOG(LogSparky, Log, TEXT("Sfx %s"), SfxNames(S)[0]);  // dev: catch sounds that repeat every frame
	}
	const TArray<USoundBase*>& Variants = M->SfxSounds[int32(S)];
	USoundBase* Sound = Variants[FMath::RandRange(0, Variants.Num() - 1)];
	if (At.ContainsNaN())
	{
		UGameplayStatics::PlaySound2D(M, Sound, Volume);
	}
	else
	{
		UGameplayStatics::PlaySoundAtLocation(M, Sound, At, Volume, 1.f, 0.f, M->Falloff);
	}
}

void ASparkyGameMode::SetMusic(EMusic Mu)
{
	if (Mu == CurrentMusic || !Music)
	{
		return;
	}
	CurrentMusic = Mu;
	Music->Stop();
	if (const TCHAR* Name = MusicName(Mu))
	{
		if (USoundBase* S = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Audio/%s.%s"), Name, Name)))
		{
			Music->SetSound(S);
			Music->Play();
		}
	}
	UpdateMusicVolume();
}

void ASparkyGameMode::UpdateMusicVolume()
{
	if (Music)
	{
		const float V = !bMusicOn ? 0.f : State == EGameState::Paused || IsCalibratingFromPause() ? 0.35f : 1.f;
		Music->SetVolumeMultiplier(FMath::Max(V * MusicVolume, 0.001f));
	}
}

// ---------------------------------------------------------------- states

void ASparkyGameMode::SetState(EGameState S)
{
	State = S;
	StateTime = 0.f;
	MenuIndex = 0;
	const bool bFrozen = S == EGameState::Paused || (S == EGameState::Calibrate && CalibrateReturn == EGameState::Paused);
	UGameplayStatics::SetGamePaused(this, bFrozen);
	UpdateMusicVolume();
}

void ASparkyGameMode::GoToTitle()
{
	ClearChapter();
	Chapter = 1;
	BuildChapter(1);
	if (ASparkyCharacter* P = Player())
	{
		P->SetAttract(true, FVector(3500, 0, 0), 6000.f);
	}
	SelectChapter = ReachedChapter;
	SetMusic(EMusic::Title);
	SetState(EGameState::Title);
}

void ASparkyGameMode::ShowStory(const TArray<FLine>& InPages, EAfterStory After)
{
	Pages = InPages;
	Page = 0;
	AfterStory = After;
	SetState(EGameState::Story);
}

void ASparkyGameMode::ClearChapter()
{
	for (AActor* A : ChapterActors)
	{
		if (IsValid(A))
		{
			A->Destroy();
		}
	}
	ChapterActors.Reset();
	// effects and hazards spawned along the way
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (Cast<AFx>(*It) || Cast<AHazard>(*It) || Cast<AEnemy>(*It) || Cast<APickup>(*It))
		{
			It->Destroy();
		}
	}
	if (Geo)
	{
		Geo->Destroy();
		Geo = nullptr;
	}
	Boss = nullptr;
	Convo.Reset();
	RadioQueue.Reset();
	RadioTime = 0.f;
	MessageTime = 0.f;
	AMover::FloatAmount = 0.f;
	APirateShip::TurretPos = FVector::ZeroVector;
}

void ASparkyGameMode::BuildChapter(int32 N)
{
	Geo = GetWorld()->SpawnActor<ALevelGeo>(ALevelGeo::StaticClass(), FTransform::Identity);
	const Levels::FSetup Setup = Levels::Build(N, this, Geo);
	Geo->Commit();
	CatnipTotal = 0;
	for (TActorIterator<APickup> It(GetWorld()); It; ++It)
	{
		CatnipTotal += It->GetType() == EPickup::Catnip ? 1 : 0;
	}
	if (ASparkyCharacter* P = Player())
	{
		P->ResetForChapter();
		P->SetGravity(Geo->Theme.Gravity);
		FVector Start = Setup.Start;
		FString SpawnAt;
		if (FParse::Value(FCommandLine::Get(), TEXT("Spawn="), SpawnAt, false))
		{
			TArray<FString> XYZ;  // dev: -Spawn=x,y,z
			if (SpawnAt.ParseIntoArray(XYZ, TEXT(",")) == 3)
			{
				Start = FVector(FCString::Atof(*XYZ[0]), FCString::Atof(*XYZ[1]), FCString::Atof(*XYZ[2]));
			}
		}
		P->PlaceAt(Start, Setup.Yaw);
		P->SetCheckpoint(Start, Setup.Yaw);
		if (FParse::Param(FCommandLine::Get(), TEXT("Hat")))
		{
			P->SetHat(true);  // dev: see the hat without the styrofoam hunt
		}
	}
	ChapterMusic = Setup.Music;
}

void ASparkyGameMode::StartChapter(int32 N, bool bWithStory)
{
	ClearChapter();
	Chapter = N;
	ChapterTime = 0.f;
	Catnip = 0;
	CatnipCombo = 0;
	Lives = 9;
	DownTimer = 0.f;
	Styro = 0;
	bHat = false;
	SuppliesMask = 0;
	bDrivePart = false;
	bMetElder = false;
	Wave = 0;
	bCageShut = false;
	WaveDelay = 0.f;
	PodsLeft = 3;
	LaserTimer = 6.f;
	HyperTime = -1.f;
	CaptainTaunt = 25.f;
	Oxygen = 1.f;
	Repairs = 0;
	LateReset();
	LeoReport = 0;
	CrisisTime = 0.f;
	bCrisisOn = false;
	BuildChapter(N);
	if (ASparkyCharacter* P = Player())
	{
		P->SetAttract(false);
	}
	SetMusic(EMusic::None);
	if (bWithStory)
	{
		ShowStory(Story::Get(N).Intro, EAfterStory::Play);
	}
	else
	{
		BeginPlayChapter();
	}
}

void ASparkyGameMode::BeginPlayChapter()
{
	SetState(EGameState::Playing);
	SetMusic(ChapterMusic);
	Sfx(this, ESfx::ChapterStart);
	Message(FString::Printf(TEXT("CHAPTER %d: %s"), Chapter, Story::Get(Chapter).Title), FLinearColor(1.f, 0.85f, 0.3f));
	switch (Chapter)
	{
	case 1:
		Radio(EWho::Leo, TEXT("Whoa. Sparky, look at the hats on those guys."));
		Radio(EWho::Sparky, TEXT("Fascinating. Let's find whoever's in charge. The big building, straight ahead."));
		break;
	case 2:
	{
		int32 StartWave = 0;
		if (FParse::Value(FCommandLine::Get(), TEXT("Wave="), StartWave))
		{
			// dev: straight into the cage at round N
			bCageShut = true;
			if (ASparkyCharacter* P = Player())
			{
				P->PlaceAt(FVector(-600, 0, 90), 0.f);
				P->SetCheckpoint(FVector(-600, 0, 90), 0.f);
			}
			for (TActorIterator<AGoal> It(GetWorld()); It; ++It)
			{
				It->SetActive(false);
			}
			Ch2StartWave(FMath::Clamp(StartWave, 1, 3));
			break;
		}
		Radio(EWho::Crowd, TEXT("TONIGHT! IN THE CAGE! THE TOUGHEST CAT IN CHINATOWN!"));
		Radio(EWho::Sparky, TEXT("(Years ago. Hiding in plain sight. To the cage, then. Again.)"));
		break;
	}
	case 3:
		Converse({
			{EWho::Captain, TEXT("Arrr-ROOO! This be Captain Crotch Sniffer! Hand over yer ship! It's nicer than mine!")},
			{EWho::Sparky, TEXT("Not today. Leo, keep their ship busy. I'll knock their boarding pods off our hull.")},
			{EWho::Leo, TEXT("On it! ...Which one's the fire button?")},
			{EWho::Sparky, TEXT("Ground-pound the clamp on top of each pod. Z in the air. Mind the laser fire!")},
		});
		break;
	case 4:
		Sfx(this, ESfx::Alarm);
		Converse({
			{EWho::Sparky, TEXT("Mrrrow?! What's happening, Leo?")},
			{EWho::Leo, TEXT("I don't know, Sparky! Everything was fine, and then these alarms started going off!")},
			{EWho::Sparky, TEXT("The ship was damaged in the battle and we didn't notice. The climate control module... we'll lose oxygen, gravity, and warmth.")},
			{EWho::Sparky, TEXT("My welder. It's on the shelf above my bed. Leo - keep an eye on the oxygen levels!")},
		}, [this]() { bCrisisOn = true; });
		break;
	default:
		LateBegin();
		break;
	}
}

void ASparkyGameMode::ChapterWon()
{
	ReachedChapter = FMath::Max(ReachedChapter, FMath::Min(Chapter + 1, Story::NumChapters));
	SaveSettings();
	SetMusic(EMusic::None);
	Sfx(this, ESfx::ChapterComplete);
	if (ASparkyCharacter* P = Player())
	{
		P->SetCinematic(false);
	}
	SetState(EGameState::ChapterClear);
}

// ---------------------------------------------------------------- tick

void ASparkyGameMode::Tick(float Dt)
{
	Super::Tick(Dt);
	const float RealDt = FMath::Min(float(FApp::GetDeltaTime()), 0.1f);
	StateTime += RealDt;
	MessageTime = FMath::Max(0.f, MessageTime - RealDt);
	if (State == EGameState::Cutscene)
	{
		ConvoTime += RealDt;
	}
	if (State == EGameState::Playing)
	{
		ChapterTime += Dt;
		TickDialog(Dt);
		TickChapter(Dt);
		if (DownTimer > 0.f)
		{
			DownTimer -= Dt;
			if (DownTimer <= 0.f)
			{
				if (Lives > 0)
				{
					if (ASparkyCharacter* P = Player())
					{
						P->Respawn();
					}
				}
				else
				{
					SetState(EGameState::GameOver);
					SetMusic(EMusic::None);
				}
			}
		}
	}
	if (State == EGameState::Cutscene && HyperTime >= 0.f)
	{
		TickChapter(Dt);  // the hyperspace jump plays out during its cutscene
	}
}

void ASparkyGameMode::TickDialog(float Dt)
{
	RadioTime = FMath::Max(0.f, RadioTime - Dt);
	if (RadioTime <= 0.f && RadioQueue.Num() > 0)
	{
		RadioLine = RadioQueue[0];
		RadioQueue.RemoveAt(0);
		RadioTime = FMath::Clamp(1.8f + RadioLine.Text.Len() * 0.055f, 3.f, 8.f);
		Sfx(this, ESfx::Talk, FVector(NAN), 0.5f);
	}
}

void ASparkyGameMode::TickChapter(float Dt)
{
	switch (Chapter)
	{
	case 2:
		if (WaveDelay > 0.f)
		{
			WaveDelay -= Dt;
			if (WaveDelay <= 0.f)
			{
				Ch2StartWave(Wave + 1);
			}
		}
		break;
	case 3:
		Ch3Tick(Dt);
		break;
	case 4:
		Ch4Tick(Dt);
		break;
	default:
		LateTick(Dt);
		break;
	}
}

// ---------------------------------------------------------------- dialog

void ASparkyGameMode::Converse(const TArray<FLine>& Lines, TFunction<void()> Then)
{
	if (Lines.Num() == 0)
	{
		if (Then)
		{
			Then();
		}
		return;
	}
	Convo = Lines;
	ConvoIndex = 0;
	ConvoTime = 0.f;
	ConvoThen = MoveTemp(Then);
	SetState(EGameState::Cutscene);
	Sfx(this, ESfx::Talk, FVector(NAN), 0.6f);
}

void ASparkyGameMode::AdvanceConvo()
{
	++ConvoIndex;
	ConvoTime = 0.f;
	if (ConvoIndex < Convo.Num())
	{
		Sfx(this, ESfx::Talk, FVector(NAN), 0.6f);
		return;
	}
	Convo.Reset();
	SetState(EGameState::Playing);
	if (ConvoThen)
	{
		TFunction<void()> Then = MoveTemp(ConvoThen);
		ConvoThen = nullptr;
		Then();
	}
}

void ASparkyGameMode::Ask(EWho Who, const FString& Question, const TArray<FString>& Options, TFunction<void(int32)> Then)
{
	QuestionWho = Who;
	QuestionText = Question;
	QuestionOptions = Options;
	QuestionChoice = 0;
	QuestionThen = MoveTemp(Then);
	SetState(EGameState::Question);
	Sfx(this, ESfx::Talk, FVector(NAN), 0.6f);
}

void ASparkyGameMode::Radio(EWho Who, const FString& Text)
{
	RadioQueue.Add({Who, Text});
}

void ASparkyGameMode::Message(const FString& Text, const FLinearColor& Color)
{
	MessageText = Text;
	MessageColor = Color;
	MessageTime = 2.8f;
}

// ---------------------------------------------------------------- menus

TArray<FString> ASparkyGameMode::MenuItems() const
{
	TArray<FString> Out;
	for (EItem I : Items(State))
	{
		switch (I)
		{
		case EItem::Start: Out.Add(TEXT("NEW GAME")); break;
		case EItem::Chapter: Out.Add(FString::Printf(TEXT("< CHAPTER %d >"), SelectChapter)); break;
		case EItem::Controls: Out.Add(bKeyboardOnly ? TEXT("CONTROLS: KEYBOARD ONLY") : TEXT("CONTROLS: KEYS + MOUSE")); break;
		case EItem::Music: Out.Add(bMusicOn ? TEXT("MUSIC: ON") : TEXT("MUSIC: OFF")); break;
		case EItem::Calibrate: Out.Add(TEXT("CALIBRATE CONTROLLER")); break;
		case EItem::Quit: Out.Add(TEXT("QUIT")); break;
		case EItem::Resume: Out.Add(TEXT("RESUME")); break;
		case EItem::Restart: Out.Add(TEXT("RESTART CHAPTER")); break;
		case EItem::ToTitle: Out.Add(TEXT("QUIT TO TITLE")); break;
		case EItem::Retry: Out.Add(TEXT("TRY AGAIN")); break;
		}
	}
	return Out;
}

void ASparkyGameMode::MenuSelect(int32 Index)
{
	const TArray<EItem> It = Items(State);
	if (!It.IsValidIndex(Index))
	{
		return;
	}
	Sfx(this, ESfx::MenuSelect);
	switch (It[Index])
	{
	case EItem::Start:
		StartChapter(1, true);
		break;
	case EItem::Chapter:
		StartChapter(SelectChapter, true);
		break;
	case EItem::Controls:
		bKeyboardOnly = !bKeyboardOnly;
		SaveSettings();
		break;
	case EItem::Music:
		bMusicOn = !bMusicOn;
		UpdateMusicVolume();
		SaveSettings();
		break;
	case EItem::Calibrate:
		BeginCalibration();
		break;
	case EItem::Quit:
		UKismetSystemLibrary::QuitGame(this, nullptr, EQuitPreference::Quit, false);
		break;
	case EItem::Resume:
		TogglePause();
		break;
	case EItem::Restart:
	case EItem::Retry:
		UGameplayStatics::SetGamePaused(this, false);
		StartChapter(Chapter, false);
		break;
	case EItem::ToTitle:
		UGameplayStatics::SetGamePaused(this, false);
		GoToTitle();
		break;
	}
}

void ASparkyGameMode::MenuInput(const FControls& C)
{
	switch (State)
	{
	case EGameState::Title:
	case EGameState::Paused:
	case EGameState::GameOver:
	{
		const TArray<EItem> It = Items(State);
		if (State == EGameState::Paused && (C.bStart || C.bBack))
		{
			TogglePause();
			return;
		}
		if (C.MenuDir == 1 || C.MenuDir == 2)
		{
			MenuIndex = (MenuIndex + (C.MenuDir == 1 ? -1 : 1) + It.Num()) % It.Num();
			Sfx(this, ESfx::MenuMove);
		}
		if ((C.MenuDir == 3 || C.MenuDir == 4) && It.IsValidIndex(MenuIndex) && It[MenuIndex] == EItem::Chapter)
		{
			SelectChapter = FMath::Clamp(SelectChapter + (C.MenuDir == 3 ? -1 : 1), 1, ReachedChapter);
			Sfx(this, ESfx::MenuMove);
		}
		if (C.bConfirm && StateTime > 0.3f)
		{
			MenuSelect(MenuIndex);
		}
		break;
	}
	case EGameState::Story:
		if ((C.bConfirm || C.bBack) && Pages.IsValidIndex(Page) && StateTime * 70.f < Pages[Page].Text.Len())
		{
			StateTime = 1000.f;  // first press: finish typing the page
		}
		else if ((C.bConfirm || C.bBack) && StateTime > 0.25f)
		{
			StateTime = 0.f;
			Sfx(this, ESfx::MenuMove);
			if (++Page >= Pages.Num())
			{
				switch (AfterStory)
				{
				case EAfterStory::Play:
					BeginPlayChapter();
					break;
				case EAfterStory::NextChapter:
					if (Chapter < Story::NumChapters)
					{
						StartChapter(Chapter + 1, true);
					}
					else
					{
						SetState(EGameState::End);
						SetMusic(EMusic::Title);
					}
					break;
				case EAfterStory::End:
					SetState(EGameState::End);
					break;
				}
			}
		}
		break;
	case EGameState::Cutscene:
		if ((C.bConfirm || C.bBack) && Convo.IsValidIndex(ConvoIndex) && ConvoTime * 60.f < Convo[ConvoIndex].Text.Len() && ConvoTime > 0.1f)
		{
			ConvoTime = 1000.f;  // first press: finish typing the line
		}
		else if ((C.bConfirm || C.bBack) && ConvoTime > 0.25f && Convo.Num() > 0)
		{
			AdvanceConvo();
		}
		break;
	case EGameState::Question:
		if (C.MenuDir == 1 || C.MenuDir == 2)
		{
			QuestionChoice = (QuestionChoice + (C.MenuDir == 1 ? -1 : 1) + QuestionOptions.Num()) % QuestionOptions.Num();
			Sfx(this, ESfx::MenuMove);
		}
		if (C.bConfirm && StateTime > 0.6f)
		{
			SetState(EGameState::Playing);
			TFunction<void(int32)> Then = MoveTemp(QuestionThen);
			QuestionThen = nullptr;
			if (Then)
			{
				Then(QuestionChoice);
			}
		}
		break;
	case EGameState::ChapterClear:
		if ((C.bConfirm || C.bBack) && StateTime > 1.5f)
		{
			ShowStory(Story::Get(Chapter).Outro, EAfterStory::NextChapter);
		}
		break;
	case EGameState::End:
		if ((C.bConfirm || C.bBack) && StateTime > 2.f)
		{
			GoToTitle();
		}
		break;
	default:
		break;
	}
}

void ASparkyGameMode::TogglePause()
{
	if (State == EGameState::Playing)
	{
		SetState(EGameState::Paused);
		Sfx(this, ESfx::Pause);
	}
	else if (State == EGameState::Paused)
	{
		SetState(EGameState::Playing);
		Sfx(this, ESfx::Pause);
	}
}

void ASparkyGameMode::BeginCalibration()
{
	ASparkyCharacter* P = Player();
	if (!P)
	{
		return;
	}
	CalibrateReturn = State;
	SetState(EGameState::Calibrate);
	P->StartCalibration();
}

void ASparkyGameMode::EndCalibration(bool bSaved, const FString& DeviceName)
{
	if (State != EGameState::Calibrate)
	{
		return;
	}
	SetState(CalibrateReturn);
	if (bSaved)
	{
		Message(TEXT("CONTROLLER SAVED"), FLinearColor(0.5f, 1.f, 0.6f));
		Sfx(this, ESfx::Objective);
	}
}

// ---------------------------------------------------------------- settings

void ASparkyGameMode::LoadSettings()
{
	FString S;
	if (!FFileHelper::LoadFileToString(S, *SettingsFile()))
	{
		return;
	}
	TArray<FString> Lines;
	S.ParseIntoArrayLines(Lines);
	for (const FString& L : Lines)
	{
		FString K, V;
		if (L.Split(TEXT("="), &K, &V))
		{
			if (K == TEXT("Music")) bMusicOn = V.ToBool();
			if (K == TEXT("KeyboardOnly")) bKeyboardOnly = V.ToBool();
			if (K == TEXT("Reached")) ReachedChapter = FMath::Clamp(FCString::Atoi(*V), 1, Story::NumChapters);
		}
	}
}

void ASparkyGameMode::SaveSettings() const
{
	FFileHelper::SaveStringToFile(FString::Printf(TEXT("Music=%s\nKeyboardOnly=%s\nReached=%d\n"), bMusicOn ? TEXT("true") : TEXT("false"),
	                                              bKeyboardOnly ? TEXT("true") : TEXT("false"), ReachedChapter), *SettingsFile());
}

// ---------------------------------------------------------------- world events

void ASparkyGameMode::OnPickup(APickup* Pk)
{
	ASparkyCharacter* P = Player();
	switch (Pk->GetType())
	{
	case EPickup::Catnip:
		++Catnip;
		Sfx(this, ESfx::Catnip, FVector(NAN), 0.6f);
		if (P && Catnip % 10 == 0)
		{
			P->Heal(1);
			Sfx(this, ESfx::Purr, FVector(NAN), 0.7f);
		}
		if (Catnip % 50 == 0)
		{
			++Lives;
			Sfx(this, ESfx::OneUp);
			Message(TEXT("EXTRA LIFE!"), FLinearColor(0.5f, 1.f, 0.5f));
		}
		return;
	case EPickup::Fish:
		if (P)
		{
			P->Heal(2);
		}
		Sfx(this, ESfx::Purr);
		return;
	default:
		break;
	}
	if (Chapter == 1)
	{
		Ch1Pickup(Pk);
	}
	else if (Chapter >= 5)
	{
		LatePickup(Pk);
	}
	else if (Pk->GetType() == EPickup::Welder)
	{
		Sfx(this, ESfx::Pickup);
		if (P)
		{
			P->SetWelder(true);
		}
		Message(TEXT("GOT THE WELDER!"));
		Radio(EWho::Sparky, TEXT("Four conduits feed the climate module: hold B at each break to weld it. Then the module itself, in the engine room."));
	}
}

void ASparkyGameMode::OnEnemyKilled(AEnemy* E)
{
	UE_LOG(LogSparky, Log, TEXT("Enemy down (%d)"), int32(E->GetType()));
	if (E == Boss)
	{
		Boss = nullptr;
	}
	if (FMath::FRand() < 0.25f && Chapter != 1)
	{
		APickup* F = Spawn<APickup>(E->GetActorLocation() + FVector(0, 0, 10.f));
		F->Init(EPickup::Fish);
	}
	if (Chapter >= 5)
	{
		LateEnemyKilled(E);
	}
	if (Chapter == 2 && bCageShut)
	{
		// the dying one is already marked dead
		if (CageFoes() == 0)
		{
			Sfx(this, ESfx::Cheer);
			if (Wave >= 3)
			{
				Converse({
					{EWho::Crowd, TEXT("SPARKY! SPARKY! SPARKY WINS!")},
					{EWho::Sparky, TEXT("(Every night, a new challenger. Every night, the same hiding place.)")},
				}, [this]()
				{
					for (TActorIterator<ANpc> It(GetWorld()); It; ++It)
					{
						if (It->GetId() == TEXT("YoungLeo"))
						{
							It->SetActorLocation(FVector(-1250, 0, It->GetActorLocation().Z));
						}
					}
					Message(TEXT("SOMEONE IS WAITING BY THE GATE"));
					for (TActorIterator<AMover> It(GetWorld()); It; ++It)
					{
						if (It->ActorHasTag(TEXT("CageGate")))
						{
							It->Destroy();
						}
					}
				});
			}
			else
			{
				WaveDelay = 2.5f;
				Message(TEXT("WAVE CLEARED!"));
			}
		}
	}
}

void ASparkyGameMode::OnTalk(ANpc* N)
{
	N->SetTalking(3.f);
	const FName Id = N->GetId();
	switch (Chapter)
	{
	case 1:
		Ch1Talk(N);
		break;
	case 2:
		if (Id == TEXT("YoungLeo"))
		{
			if (Wave >= 3 && !Boss && CageFoes() == 0)
			{
				Converse({
					{EWho::Leo, TEXT("That was amazing! You beat Meatball! Nobody beats Meatball!")},
					{EWho::Leo, TEXT("Hey... can I buy you a can of tuna? I'm Leo. I'm... pretty average, really.")},
					{EWho::Sparky, TEXT("Sparky. Tuna is acceptable.")},
					{EWho::Sparky, TEXT("Tell me, Leo. Have you ever wanted to see the stars? I'm building a spaceship. I could use someone with... thumbs.")},
					{EWho::Leo, TEXT("A spaceship? You? ...Sure, why not. I'm not doing anything else.")},
				}, [this]() { ChapterWon(); });
			}
			else
			{
				Converse({{EWho::Leo, TEXT("Go get 'em, kitty! I put my last five bucks on you!")}});
			}
		}
		else if (Id == TEXT("Bookie"))
		{
			Converse({{EWho::Crowd, TEXT("You're on, cat. The cage is through the gate at the end of the alley. Don't keep the crowd waiting.")}});
		}
		break;
	case 4:
		if (Id == TEXT("Leo"))
		{
			Converse({{EWho::Leo, FString::Printf(TEXT("Oxygen's at %d percent, Sparky! I'm keeping an eye on it, like you said! ...It's going down."), FMath::RoundToInt(Oxygen * 100.f))}});
		}
		break;
	default:
		LateTalk(N);
		break;
	}
}

void ASparkyGameMode::OnRepaired(ARepairPoint* R)
{
	++Repairs;
	UE_LOG(LogSparky, Log, TEXT("Repaired %s (%d)"), *R->GetId().ToString(), Repairs);
	Sfx(this, ESfx::Objective);
	if (R->GetId() == TEXT("Module"))
	{
		Sfx(this, ESfx::Purr);
		Converse({{EWho::Sparky, TEXT("Purrrrr... And... reconnected. The module's humming again.")}}, [this]() { ChapterWon(); });
		return;
	}
	const int32 Conduits = Repairs;
	Message(FString::Printf(TEXT("CONDUIT %d / 4 SEALED"), Conduits), FLinearColor(0.5f, 1.f, 0.6f));
	if (Conduits >= 4)
	{
		for (TActorIterator<ARepairPoint> It(GetWorld()); It; ++It)
		{
			It->SetEnabled(true);
		}
		Radio(EWho::Sparky, TEXT("Conduits sealed. Now the module itself, in the engine room. Through the cargo bay!"));
	}
}

void ASparkyGameMode::OnPodDetached(APod* Pd)
{
	--PodsLeft;
	Sfx(this, ESfx::Cheer, FVector(NAN), 0.4f);
	if (PodsLeft > 0)
	{
		Message(FString::Printf(TEXT("POD KNOCKED OFF! %d LEFT"), PodsLeft));
		static const TCHAR* Taunts[] = {
			TEXT("Hey! That was my second-best pod! Get that cat!"),
			TEXT("Arrr-ROO! Ye'll pay for that, ye flea-bitten furball!"),
		};
		Radio(EWho::Captain, Taunts[FMath::Clamp(2 - PodsLeft, 0, 1)]);
		return;
	}
	Converse({
		{EWho::Captain, TEXT("All me pods! This ain't over, cat! I'll follow ye to the ends of the galaxy!")},
		{EWho::Sparky, TEXT("That's the last of them. Leo - I'm heading for the cockpit. Hold on to something!")},
		{EWho::Leo, TEXT("Holding on to my soup!")},
	}, [this]()
	{
		for (TActorIterator<AGoal> It(GetWorld()); It; ++It)
		{
			if (It->GetId() == TEXT("Cockpit"))
			{
				It->SetActive(true);
			}
		}
		Message(TEXT("GET TO THE COCKPIT HATCH!"));
	});
}

void ASparkyGameMode::OnGoal(AGoal* G)
{
	if (Chapter >= 5)
	{
		LateGoal(G);
		return;
	}
	const FName Id = G->GetId();
	if (Id == TEXT("Ship"))
	{
		Converse({
			{EWho::Leo, TEXT("All aboard! Peanut butter for the power plant, cat food, Tang, and soup. My soup.")},
			{EWho::Sparky, TEXT("And one atom crunch part, courtesy of my hat. Let's go, Leo.")},
		}, [this]() { ChapterWon(); });
	}
	else if (Id == TEXT("Cage") && !bCageShut)
	{
		bCageShut = true;
		ASparkyCharacter* P = Player();
		// shut the gate behind him
		AMover* Gate = Spawn<AMover>(FVector(-975, 0, 200));
		Gate->Init(FVector(25, 180, 200), TEXT("T_Cage"), FLinearColor(0.7f, 0.7f, 0.75f), EMove::Bob, FVector::ZeroVector, 10.f);
		Gate->Tags.Add(TEXT("CageGate"));
		Sfx(this, ESfx::Clang);
		Sfx(this, ESfx::Cheer);
		if (P)
		{
			P->SetCheckpoint(FVector(-600, 0, 90), 0.f);
		}
		Converse({
			{EWho::Crowd, TEXT("IN THIS CORNER... THE MYSTERY CAT WHO NEVER LOSES... SPARKY!")},
			{EWho::Crowd, TEXT("AND TONIGHT'S CHALLENGERS: THE FISHERMAN'S WHARF GANG!")},
			{EWho::Sparky, TEXT("(Swipe with B. Pounce in the air. Or simply land on their heads.)")},
		}, [this]() { Ch2StartWave(1); });
	}
	else if (Id == TEXT("Cockpit"))
	{
		HyperTime = 0.f;
		Sfx(this, ESfx::Hyperspace);
		if (ASparkyCharacter* P = Player())
		{
			P->SetCinematic(true, FVector(2600, -1600, 900), FVector(1000, 0, 300));
		}
		Converse({
			{EWho::Sparky, TEXT("Hold on, Leo!")},
			{EWho::None, TEXT("In a desperate bid to escape, Sparky activated the hyperspace drive, making an uncalculated leap into the unknown.")},
		}, [this]() { ChapterWon(); });
	}
}

void ASparkyGameMode::OnCheckpoint()
{
	Sfx(this, ESfx::Checkpoint);
	Message(TEXT("CHECKPOINT"), FLinearColor(0.4f, 0.9f, 1.f));
	if (ASparkyCharacter* P = Player())
	{
		P->Heal(ASparkyCharacter::MaxHealth);
	}
}

void ASparkyGameMode::OnPlayerDown()
{
	--Lives;
	DownTimer = 2.2f;
	Message(Lives > 0 ? FString::Printf(TEXT("ONE OF NINE LIVES GONE! %d LEFT"), Lives) : FString(TEXT("OUT OF LIVES!")), FLinearColor(1.f, 0.4f, 0.3f));
}

void ASparkyGameMode::OnFell()
{
	ASparkyCharacter* P = Player();
	if (!P || P->IsDown())
	{
		return;
	}
	const FVector At = P->GetActorLocation();
	if (Chapter == 1)
	{
		AFx::Spawn(GetWorld(), EFx::Splash, FVector(At.X, At.Y, Geo ? Geo->Theme.KillZ + 60.f : At.Z), FLinearColor(1.f, 0.3f, 0.5f), 1.5f);
		Sfx(this, ESfx::Splash);
		Radio(EWho::Leo, TEXT("Careful, Sparky! That bog is all cranberry jelly!"));
	}
	else if (Chapter == 3)
	{
		Radio(EWho::Leo, TEXT("Sparky! You floated off into space! ...Okay, I pulled you back in with the tractor thingy."));
	}
	P->FellOut();
}

// ---------------------------------------------------------------- chapter 1: Cranberry Plasma

void ASparkyGameMode::Ch1Pickup(APickup* Pk)
{
	ASparkyCharacter* P = Player();
	switch (Pk->GetType())
	{
	case EPickup::Styrofoam:
		++Styro;
		Sfx(this, ESfx::Styro);
		Message(FString::Printf(TEXT("STYROFOAM %d / 5"), Styro), FLinearColor(1.f, 1.f, 0.95f));
		if (Styro == 5)
		{
			Radio(EWho::Sparky, bMetElder ? TEXT("Five blocks. Now, back to the elder. Time for some engineering.") : TEXT("Five blocks of styrofoam. Everyone here seems to want hats... I should find whoever's in charge."));
		}
		else if (Styro == 1 && !bMetElder)
		{
			Radio(EWho::Leo, TEXT("Styrofoam? What do you want with that, Sparky?"));
		}
		break;
	case EPickup::PeanutButter:
	case EPickup::CatFood:
	case EPickup::Tang:
	case EPickup::Soup:
	case EPickup::DrivePart:
	{
		Sfx(this, ESfx::Pickup);
		if (Pk->GetType() == EPickup::DrivePart)
		{
			bDrivePart = true;
			Message(TEXT("GOT THE ATOM CRUNCH DRIVE PART!"), FLinearColor(0.4f, 1.f, 1.f));
		}
		else
		{
			SuppliesMask |= 1 << (int32(Pk->GetType()) - int32(EPickup::PeanutButter));
			Message(FString::Printf(TEXT("GOT %s!"), PickupName(Pk->GetType())));
			if (Pk->GetType() == EPickup::Soup)
			{
				Radio(EWho::Leo, TEXT("Soup! Oh, thank goodness."));
			}
			else if (Pk->GetType() == EPickup::PeanutButter)
			{
				Radio(EWho::Sparky, TEXT("Peanut butter. For the power plant, naturally."));
			}
		}
		if (bDrivePart && SuppliesMask == 15)
		{
			Radio(EWho::Leo, TEXT("That's everything! Let's get back to the ship, Sparky."));
			for (TActorIterator<AGoal> It(GetWorld()); It; ++It)
			{
				if (It->GetId() == TEXT("Ship"))
				{
					It->SetActive(true);
				}
			}
		}
		break;
	}
	default:
		break;
	}
	(void)P;
}

void ASparkyGameMode::Ch1Talk(ANpc* N)
{
	const FString Id = N->GetId().ToString();
	ASparkyCharacter* P = Player();
	if (Id == TEXT("Elder"))
	{
		if (!bMetElder)
		{
			bMetElder = true;
			Converse({
				{EWho::Elder, TEXT("Strangers! Welcome to Cranberry Plasma. You've arrived in time for the velcro party.")},
				{EWho::Sparky, TEXT("Thank you. We need parts for our atom crunch drive 6000. And cat food. And Tang.")},
				{EWho::Leo, TEXT("And soup.")},
				{EWho::Elder, TEXT("Parts? Ha! Here, one's standing is the complexity and design of one's styrofoam hat. Only the high-hatted may trade.")},
				{EWho::Elder, TEXT("And you, little cat, have no hat at all.")},
				{EWho::Sparky, TEXT("(Leo... I believe I can design a hat that is both aesthetically pleasing and mechanically brilliant.)")},
				{EWho::Sparky, TEXT("(I'll need five blocks of styrofoam.)")},
				{EWho::Elder, TEXT("Styrofoam? It blows all over the island. Up the old hat tower, the velcro hall, out on the bog, the floating rocks, jelly hill...")},
			}, [this]() { Sfx(this, ESfx::Objective); });
		}
		else if (Styro < 5 && !bHat)
		{
			Converse({{EWho::Elder, FString::Printf(TEXT("Hat-less still, little cat? Try the hat tower, the velcro hall, the bog, the floating rocks and jelly hill. You have %d of five."), Styro)}});
		}
		else if (!bHat)
		{
			Converse({
				{EWho::Sparky, TEXT("Stand back, everyone. This calls for a little engineering.")},
			}, [this, P]()
			{
				bHat = true;
				if (P)
				{
					P->SetHat(true);
					AFx::Spawn(GetWorld(), EFx::Burst, P->GetActorLocation() + FVector(0, 0, 60), FLinearColor(1.f, 1.f, 0.8f), 3.f);
					AFx::Spawn(GetWorld(), EFx::Poof, P->GetActorLocation(), FLinearColor(1.f, 1.f, 1.f), 2.f);
				}
				Sfx(this, ESfx::HatCraft);
				Sfx(this, ESfx::Cheer);
				for (TActorIterator<APickup> It(GetWorld()); It; ++It)
				{
					if (It->GetType() != EPickup::Catnip && It->GetType() != EPickup::Styrofoam)
					{
						It->SetActive(true);
					}
				}
				Converse({
					{EWho::Elder, TEXT("By the great foam! Tiers! Gears! An ANTENNA! It is... it is higher than mine.")},
					{EWho::Elder, TEXT("Exalted one! Take the atom crunch part with our blessing. And the merchants' stalls are yours: take whatever you need.")},
					{EWho::Leo, TEXT("Ha! Nice hat, Sparky.")},
				}, [this]() { Message(TEXT("COLLECT THE DRIVE PART AND THE SUPPLIES!")); });
			});
		}
		else
		{
			Converse({{EWho::Elder, bDrivePart && SuppliesMask == 15 ? TEXT("Your ship awaits, Exalted One. Will you not stay for the feast? ...Another time, then.") : TEXT("The merchants' stalls are in the plaza, Exalted One. Take what you need.")}});
		}
		return;
	}
	if (Id.StartsWith(TEXT("Merchant")))
	{
		Converse({{EWho::Local, bHat ? TEXT("For one so high-hatted? Free! Take it, take it!") : TEXT("I only trade with the well-hatted. Come back when you have something on your head.")}});
		return;
	}
	if (Id == TEXT("Velcro"))
	{
		Converse({
			{EWho::Local, TEXT("Welcome to the velcro party! Jump at the fuzzy walls and you'll stick fast!")},
			{EWho::Local, TEXT("Press A to jump off again. Wall to wall, all the way up! There's styrofoam at the top, I think.")},
		});
		return;
	}
	if (Id == TEXT("Tower"))
	{
		Converse({{EWho::Local, TEXT("The old hat tower! Each tier's a hat from the old days. Jump twice in the air to reach the higher ones.")}});
		return;
	}
	if (Id == TEXT("Bog"))
	{
		Converse({{EWho::Local, TEXT("The lily pads sink if you stand on them too long. And the bog blobs bite! Stomp them, or swipe them with B.")}});
		return;
	}
	static const TCHAR* Snub[] = {
		TEXT("Your head is so... bare. How do you live like that?"),
		TEXT("No hat? At the velcro party? The nerve."),
		TEXT("My hat has four tiers. FOUR. What does yours have? Oh. Nothing."),
		TEXT("Watch out for the bog blobs on jelly hill. They're more jelly than blob."),
	};
	static const TCHAR* Praise[] = {
		TEXT("Exalted one! May your antenna never droop!"),
		TEXT("Such gears! Such tiers! I must tell my cousins!"),
		TEXT("I'll never wear this old thing again. Look at it. Two tiers. Pathetic."),
		TEXT("Will you be giving a speech at the feast? Please say yes."),
	};
	const int32 K = GetTypeHash(N->GetId()) % 4;
	Converse({{EWho::Local, bHat ? Praise[K] : Snub[K]}});
}

// ---------------------------------------------------------------- chapter 2: the cage

void ASparkyGameMode::Ch2StartWave(int32 N)
{
	Wave = N;
	Sfx(this, ESfx::Cheer, FVector(NAN), 0.7f);
	auto Cat = [this](const FVector& At, int32 Variant)
	{
		AEnemy* E = Spawn<AEnemy>(At, (FVector(0, 0, 0) - At).Rotation().Yaw);
		E->Init(EEnemy::AlleyCat, Variant);
		AFx::Spawn(GetWorld(), EFx::Poof, At, FLinearColor(0.8f, 0.8f, 0.8f), 1.5f);
		return E;
	};
	if (N == 1)
	{
		Message(TEXT("ROUND 1!"));
		Cat(FVector(500, 300, 80), 0);
		Cat(FVector(500, -300, 80), 1);
	}
	else if (N == 2)
	{
		Message(TEXT("ROUND 2!"));
		Cat(FVector(600, 0, 80), 2);
		Cat(FVector(0, 600, 80), 3);
		Cat(FVector(0, -600, 80), 0);
		Cat(FVector(300, 450, 80), 1);
	}
	else if (N == 3)
	{
		Converse({
			{EWho::Crowd, TEXT("AND NOW... THE MAIN EVENT! FROM THE MEAN STREETS OF THE TENDERLOIN... MEEEEATBALL!")},
			{EWho::Meatball, TEXT("Heh. You're the little genius they all talk about? Let's see how smart you are on your back.")},
			{EWho::Sparky, TEXT("(He'll leap and slam the mat. Jump the shockwave, then strike while he's dizzy.)")},
		}, [this]()
		{
			Boss = Spawn<AEnemy>(FVector(600, 0, 120), 180.f);
			Boss->Init(EEnemy::Meatball);
			BossName = TEXT("MEATBALL");
			AFx::Spawn(GetWorld(), EFx::Poof, FVector(600, 0, 120), FLinearColor(1.f, 0.8f, 0.6f), 3.f);
			Message(TEXT("FINAL ROUND!"));
		});
	}
}

// ---------------------------------------------------------------- chapter 3: pirates

void ASparkyGameMode::Ch3Tick(float Dt)
{
	ASparkyCharacter* P = Player();
	if (HyperTime >= 0.f)
	{
		HyperTime += Dt;
		for (TActorIterator<APirateShip> It(GetWorld()); It; ++It)
		{
			if (HyperTime > 1.5f)
			{
				It->FleeToHyperspace();
			}
		}
		return;
	}
	if (State != EGameState::Playing || !P)
	{
		return;
	}
	// the pirate ship's laser fire: aimed where Sparky is going
	LaserTimer -= Dt;
	if (LaserTimer <= 0.f)
	{
		LaserTimer = FMath::FRandRange(2.5f, 4.5f);
		FVector Aim = P->GetActorLocation() + P->GetVelocity() * 1.2f + FVector(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0);
		FHitResult Hit;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(Laser), false, P);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Aim + FVector(0, 0, 600), Aim - FVector(0, 0, 1500), ECC_Visibility, Q))
		{
			AHazard* H = Spawn<AHazard>(Hit.Location);
			H->Init(EHazard::Laser);
		}
	}
	CaptainTaunt -= Dt;
	if (CaptainTaunt <= 0.f)
	{
		CaptainTaunt = FMath::FRandRange(22.f, 35.f);
		static const TCHAR* Taunts[] = {
			TEXT("Arrr-roo! Sniff 'em out, lads! Sniff 'em ALL out!"),
			TEXT("A cat! Captaining a ship! In MY sector! Fire everything!"),
			TEXT("When I get me paws on that ship, the first thing I'm doing is... rolling in something."),
		};
		Radio(EWho::Captain, Taunts[FMath::RandRange(0, 2)]);
	}
}

// ---------------------------------------------------------------- chapter 4: crisis

void ASparkyGameMode::Ch4Tick(float Dt)
{
	if (!bCrisisOn || State != EGameState::Playing)
	{
		return;
	}
	CrisisTime += Dt;
	const int32 Conduits = FMath::Min(Repairs, 4);
	Oxygen = FMath::Max(0.f, Oxygen - Dt / 260.f * (1.f - 0.15f * Conduits));
	const float Fail = 1.f - Oxygen;
	if (ASparkyCharacter* P = Player())
	{
		// gravity fades, floors frost over
		const float G = FMath::Lerp(2.4f, 0.75f, FMath::Clamp(Fail * 1.3f, 0.f, 1.f));
		P->SetGravity(G);
		P->SetIce(FMath::Clamp((Fail - 0.15f) * 1.6f, 0.f, 1.f));
		Geo->Theme.Gravity = G;
	}
	AMover::FloatAmount = FMath::Clamp(Fail * 1.4f, 0.f, 1.f);
	if (Geo)
	{
		const FLinearColor Warm(0.05f, 0.05f, 0.08f), Cold(0.55f, 0.75f, 0.95f);
		Geo->SetFog(FMath::Lerp(Warm, Cold, FMath::Clamp(Fail, 0.f, 1.f)), FMath::Lerp(2500.f, 300.f, Fail), FMath::Lerp(9000.f, 3500.f, Fail));
	}
	static const float Marks[] = {0.75f, 0.5f, 0.25f, 0.1f};
	static const TCHAR* Reports[] = {
		TEXT("Sparky! Oxygen's at seventy-five percent! Is that... good?"),
		TEXT("Fifty percent, Sparky! And my soup is floating away!"),
		TEXT("Twenty-five percent! It's getting really cold up here!"),
		TEXT("Ten percent! Sparky, I can see my breath! Hurry!"),
	};
	if (LeoReport < 4 && Oxygen < Marks[LeoReport])
	{
		Radio(EWho::Leo, Reports[LeoReport]);
		Sfx(this, ESfx::Alarm, FVector(NAN), 0.5f);
		++LeoReport;
	}
	if (Oxygen <= 0.f)
	{
		bCrisisOn = false;
		SetMusic(EMusic::None);
		Message(TEXT("OUT OF OXYGEN"), FLinearColor(1.f, 0.4f, 0.3f));
		Sfx(this, ESfx::Death);
		SetState(EGameState::GameOver);
	}
}

// ---------------------------------------------------------------- objective text

FString ASparkyGameMode::Objective() const
{
	switch (Chapter)
	{
	case 1:
		if (!bMetElder && Styro < 5) return TEXT("FIND WHOEVER IS IN CHARGE");
		if (Styro < 5 && !bHat) return FString::Printf(TEXT("FIND STYROFOAM: %d / 5"), Styro);
		if (!bHat) return TEXT("BRING THE STYROFOAM TO THE ELDER");
		if (!(bDrivePart && SuppliesMask == 15))
		{
			return FString::Printf(TEXT("DRIVE PART %s   SUPPLIES %d / 4"), bDrivePart ? TEXT("OK") : TEXT("--"), FMath::CountBits(SuppliesMask));
		}
		return TEXT("BACK TO THE SHIP");
	case 2:
		if (!bCageShut) return TEXT("GET TO THE CAGE");
		if (Wave >= 3 && CageFoes() == 0) return TEXT("TALK TO THE KID BY THE GATE");
		return Wave >= 3 ? TEXT("BEAT MEATBALL") : FString::Printf(TEXT("ROUND %d"), FMath::Max(1, Wave));
	case 3:
		if (PodsLeft > 0) return FString::Printf(TEXT("KNOCK OFF THE BOARDING PODS: %d LEFT"), PodsLeft);
		return TEXT("GET TO THE COCKPIT HATCH");
	case 4:
		if (!bCrisisOn && Repairs == 0) return TEXT("");
		if (Player() && !Player()->HasWelder()) return TEXT("GET THE WELDER FROM THE SHELF");
		if (Repairs < 4) return FString::Printf(TEXT("WELD THE CONDUITS: %d / 4"), Repairs);
		return TEXT("REPAIR THE CLIMATE MODULE");
	default:
		return LateObjective();
	}
}
