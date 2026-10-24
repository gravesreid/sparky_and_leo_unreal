#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameTypes.h"
#include "Sfx.h"
#include "SparkyGameMode.generated.h"

class AEnemy;
class AFloorPad;
class AGoal;
class ALevelGeo;
class ANpc;
class APickup;
class APod;
class ARepairPoint;
class ASparkyCharacter;
class UAudioComponent;
class USoundAttenuation;
class USoundBase;
struct FControls;

/**
 * Runs the game: title, story pages, the chapters and their scripts (what each NPC says, what
 * each pickup unlocks, when a chapter is won), dialog, pause menu, music and sound.
 * The chapters' worlds are built by Levels.cpp.
 */
UCLASS()
class SPARKYLEO_API ASparkyGameMode : public AGameModeBase
{
	GENERATED_BODY()

	friend class ASparkyHUD;

public:
	ASparkyGameMode();

	static ASparkyGameMode* Get(const UObject* Ctx);

	virtual void StartPlay() override;
	virtual void Tick(float Dt) override;

	EGameState GetState() const { return State; }
	bool IsPlaying() const { return State == EGameState::Playing; }
	int32 GetChapter() const { return Chapter; }
	/** Keyboard-only controls: arrows steer the camera, the mouse is left alone. */
	bool IsKeyboardOnly() const { return bKeyboardOnly; }

	/** Plays a sound effect; at a place (3D) or, with no place, flat. */
	static void Sfx(const UObject* Ctx, ESfx S, const FVector& At = FVector(NAN), float Volume = 1.f);

	// ------------------------------------------------ events from the world
	void OnPickup(APickup* P);
	void OnEnemyKilled(AEnemy* E);
	void OnTalk(ANpc* N);
	void OnRepaired(ARepairPoint* R);
	void OnPodDetached(APod* P);
	void OnGoal(AGoal* G);
	void OnCheckpoint();
	/** Sparky ran out of health. */
	void OnPlayerDown();
	/** Sparky fell out of the world (bog, space). */
	void OnFell();
	/** Chapter 8: Sparky stepped onto a puzzle tile. */
	void OnPadStep(AFloorPad* Pad);
	/** Chapter 8: the Guardian changed shape (1 or 2). */
	void OnGuardianShift(int32 NewForm);

	// ------------------------------------------------ dialog
	/** A conversation: the game waits while it plays, A/B advances, then Then runs. */
	void Converse(const TArray<FLine>& Lines, TFunction<void()> Then = nullptr);
	/** A line over the radio / in passing: doesn't stop the game. */
	void Radio(EWho Who, const FString& Text);
	/** A multiple-choice question (the riddles): the game waits; Then gets the chosen option. */
	void Ask(EWho Who, const FString& Question, const TArray<FString>& Options, TFunction<void(int32)> Then);
	/** A short centre-screen notice ("STYROFOAM 3/5"). */
	void Message(const FString& Text, const FLinearColor& Color = FLinearColor(1.f, 0.85f, 0.3f));

	// ------------------------------------------------ menus
	void MenuInput(const FControls& C);
	void TogglePause();
	void BeginCalibration();
	void EndCalibration(bool bSaved, const FString& DeviceName);
	bool IsCalibratingFromPause() const { return State == EGameState::Calibrate && CalibrateReturn == EGameState::Paused; }

	// ------------------------------------------------ building chapters (Levels.cpp)
	template <class T>
	T* Spawn(const FVector& Loc, float Yaw = 0.f)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		T* A = GetWorld()->SpawnActor<T>(T::StaticClass(), Loc, FRotator(0, Yaw, 0), P);
		if (A)
		{
			ChapterActors.Add(A);
		}
		return A;
	}
	ALevelGeo* GetGeo() const { return Geo; }

	/** Total catnip in the chapter (the HUD shows "12 / 40"). */
	int32 CatnipTotal = 0;

private:
	EGameState State = EGameState::Title;
	EGameState CalibrateReturn = EGameState::Title;
	int32 Chapter = 1;
	int32 ReachedChapter = 1;
	float StateTime = 0.f;
	float ChapterTime = 0.f;

	UPROPERTY()
	ALevelGeo* Geo = nullptr;
	UPROPERTY()
	TArray<AActor*> ChapterActors;

	// story pages / outro
	TArray<FLine> Pages;
	int32 Page = 0;
	enum class EAfterStory : uint8 { Play, NextChapter, End } AfterStory = EAfterStory::Play;

	// dialog
	TArray<FLine> Convo;
	int32 ConvoIndex = 0;
	float ConvoTime = 0.f;
	TFunction<void()> ConvoThen;
	FLine RadioLine;
	float RadioTime = 0.f;
	TArray<FLine> RadioQueue;
	FString MessageText;
	FLinearColor MessageColor;
	float MessageTime = 0.f;

	// menu
	int32 MenuIndex = 0;
	int32 SelectChapter = 1;

	// player progress
	int32 Lives = 9;
	int32 Catnip = 0;
	int32 CatnipCombo = 0;
	float DownTimer = 0.f;
	bool bMusicOn = true;
	bool bKeyboardOnly = false;

	// chapter 1
	int32 Styro = 0;
	bool bHat = false;
	int32 SuppliesMask = 0;
	bool bDrivePart = false;
	bool bMetElder = false;
	// chapter 2
	int32 Wave = 0;
	bool bCageShut = false;
	float WaveDelay = 0.f;
	UPROPERTY()
	AEnemy* Boss = nullptr;
	// chapter 3
	int32 PodsLeft = 0;
	float LaserTimer = 5.f;
	float HyperTime = -1.f;
	float CaptainTaunt = 20.f;
	// the question being asked (EGameState::Question)
	EWho QuestionWho = EWho::None;
	FString QuestionText;
	TArray<FString> QuestionOptions;
	int32 QuestionChoice = 0;
	TFunction<void(int32)> QuestionThen;
	/** Shown over the boss health bar. */
	FString BossName;

	// chapter 5
	int32 Ch5Phase = 0;     // 0 explore, 1 moonberries, 2 sneak up on Fluffernutter, 3 back to the ship
	int32 Berries = 0;
	float Trust = 0.f;
	int32 FluffSpot = 0;
	// chapter 6
	int32 Ch6Wave = 0;
	// chapter 7
	int32 Shards = 0;
	// chapter 8
	int32 Keys = 0;
	bool bPadsSolved = false;
	int32 PrincessWave = 0;
	int32 RiddlesSolved = 0;  // bit per terminal
	bool bGuardianUp = false;
	// chapter 9
	int32 Motes = 0;
	int32 Sprites = 0;

	// chapter 4
	float Oxygen = 1.f;
	int32 Repairs = 0;
	int32 RepairsNeeded = 5;
	int32 LeoReport = 0;
	float CrisisTime = 0.f;
	bool bCrisisOn = false;

	// sound
	UPROPERTY()
	UAudioComponent* Music = nullptr;
	UPROPERTY()
	USoundAttenuation* Falloff = nullptr;
	EMusic CurrentMusic = EMusic::None;
	EMusic ChapterMusic = EMusic::None;
	UPROPERTY()
	TArray<USoundBase*> SoundCache;
	TArray<TArray<USoundBase*>> SfxSounds;
	FVector2D LastListener;

	void LoadSounds();
	void SetMusic(EMusic M);
	void UpdateMusicVolume();
	void SetState(EGameState S);

	void GoToTitle();
	void ShowStory(const TArray<FLine>& InPages, EAfterStory After);
	void StartChapter(int32 N, bool bWithStory);
	void BuildChapter(int32 N);
	void ClearChapter();
	void BeginPlayChapter();
	void ChapterWon();
	void TickChapter(float Dt);
	void TickDialog(float Dt);
	void AdvanceConvo();

	void Ch1Talk(ANpc* N);
	void Ch1Pickup(APickup* P);
	void Ch2StartWave(int32 N);
	void Ch3Tick(float Dt);
	void Ch4Tick(float Dt);

	// chapters 5-9 (SparkyGameModeChapters.cpp)
	void LateReset();
	void LateBegin();
	void LateTick(float Dt);
	void LatePickup(APickup* P);
	void LateTalk(ANpc* N);
	void LateGoal(AGoal* G);
	void LateEnemyKilled(AEnemy* E);
	FString LateObjective() const;
	int32 FoesLeft() const;
	void Ch5Tick(float Dt);
	void Ch6StartWave(int32 N);
	void Ch8PrincessWave(int32 N);
	void Ch8Riddle(ANpc* Terminal, int32 Index);
	void Ch8KeyFound();
	void Activate(FName GoalId);
	ANpc* FindNpc(FName Id) const;

	TArray<FString> MenuItems() const;
	void MenuSelect(int32 Index);
	void LoadSettings();
	void SaveSettings() const;

	ASparkyCharacter* Player() const;
	FString Objective() const;
	/** Chapter 2: enemies still standing inside the cage. */
	int32 CageFoes() const;
};
