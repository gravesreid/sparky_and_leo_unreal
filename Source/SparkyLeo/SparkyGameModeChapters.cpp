// Chapters 5-9: Fluffer-Nutter, Fluffernutter's Revenge, The Labyrinth of Time, Riddles of the
// Ancients, Another Dimension. The worlds are in Levels.cpp; this is what happens in them.

#include "SparkyGameMode.h"
#include "CreatureRig.h"
#include "Enemy.h"
#include "LevelGeo.h"
#include "Levels.h"
#include "Props.h"
#include "SparkyCharacter.h"
#include "SparkyLeo.h"

#include "EngineUtils.h"

namespace
{
	struct FRiddle
	{
		const TCHAR* Question;
		TArray<const TCHAR*> Options;
		int32 Answer;
	};

	const FRiddle& TerminalRiddle(int32 I)
	{
		static const TArray<FRiddle> Riddles = {
			{TEXT("A riddle in the language of the cosmos: Sparky walks on four legs, Leo on two, Fluffernutter on four. How many legs walk the Labyrinth?"),
			 {TEXT("EIGHT"), TEXT("TEN"), TEXT("TWELVE")}, 1},
			{TEXT("What grows bigger the more you take away from it?"),
			 {TEXT("A PIRATE'S APPETITE"), TEXT("A STYROFOAM HAT"), TEXT("A HOLE")}, 2},
			{TEXT("I am the beginning of eternity, the end of time and space, the start of every end. What am I?"),
			 {TEXT("THE LETTER E"), TEXT("A BLACK HOLE"), TEXT("TUESDAY")}, 0},
		};
		return Riddles[I];
	}

	TArray<FString> Options(const FRiddle& R)
	{
		TArray<FString> Out;
		for (const TCHAR* O : R.Options)
		{
			Out.Add(O);
		}
		return Out;
	}

	/** The scientist's tiles: a 3x3 grid, index = row * 3 + column. */
	void Flip(TArray<bool>& Lit, int32 I)
	{
		const int32 R = I / 3, C = I % 3;
		Lit[I] = !Lit[I];
		if (R > 0) Lit[I - 3] = !Lit[I - 3];
		if (R < 2) Lit[I + 3] = !Lit[I + 3];
		if (C > 0) Lit[I - 1] = !Lit[I - 1];
		if (C < 2) Lit[I + 1] = !Lit[I + 1];
	}

	const TCHAR* CrouchKey(const ASparkyCharacter* P)
	{
		return P && P->HasN64Pad() ? TEXT("Z") : TEXT("SHIFT");
	}
}

// ---------------------------------------------------------------- shared

void ASparkyGameMode::LateReset()
{
	Ch5Phase = 0;
	Berries = 0;
	Trust = 0.f;
	FluffSpot = 0;
	Ch6Wave = 0;
	Shards = 0;
	Keys = 0;
	bPadsSolved = false;
	PrincessWave = 0;
	RiddlesSolved = 0;
	bGuardianUp = false;
	Motes = 0;
	Sprites = 0;
	BossName.Reset();
}

int32 ASparkyGameMode::FoesLeft() const
{
	int32 N = 0;
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		N += It->IsDead() ? 0 : 1;
	}
	return N;
}

void ASparkyGameMode::Activate(FName GoalId)
{
	for (TActorIterator<AGoal> It(GetWorld()); It; ++It)
	{
		if (It->GetId() == GoalId)
		{
			It->SetActive(true);
		}
	}
}

ANpc* ASparkyGameMode::FindNpc(FName Id) const
{
	for (TActorIterator<ANpc> It(GetWorld()); It; ++It)
	{
		if (It->GetId() == Id)
		{
			return *It;
		}
	}
	return nullptr;
}

void ASparkyGameMode::LateBegin()
{
	switch (Chapter)
	{
	case 5:
		if (FParse::Value(FCommandLine::Get(), TEXT("Berries="), Berries) && Berries >= 3)
		{
			// dev: straight to sneaking up on Fluffernutter
			Ch5Phase = 2;
			if (ANpc* F = FindNpc(TEXT("Fluff")))
			{
				F->SetActorLocation(FVector(Levels::Spots::Ch5Fluff[0].X, Levels::Spots::Ch5Fluff[0].Y, F->GetActorLocation().Z));
			}
		}
		Radio(EWho::Sparky, TEXT("A lush, green planet teeming with life. A primitive society: simple tools, basic agriculture. Fascinating."));
		Radio(EWho::Leo, TEXT("Look at all the trees, Sparky! Let's go see the forest!"));
		break;
	case 6:
		Sfx(this, ESfx::Alarm);
		if (ASparkyCharacter* P = Player())
		{
			P->HideModel(true);  // little Fluffernutter stands where the monster will be
		}
		Converse({
			{EWho::Captain, TEXT("You thought you could escape me, eh? You were wrong. Prepare to meet your doom!")},
			{EWho::Sparky, TEXT("(They followed us through hyperspace. Boarded, outnumbered, outgunned... I'm sorry, Leo.)")},
			{EWho::Leo, TEXT("Fluffernutter! Run, buddy! Hide!")},
			{EWho::Fluff, TEXT("*...grrr... grrRRRRR...*")},
			{EWho::None, TEXT("But just when all seemed lost, an unexpected hero emerged. With a sudden, startling transformation, Fluffernutter's body expanded, his muscles bulging, his eyes glowing with an otherworldly light.")},
		}, [this]()
		{
			if (ANpc* Small = FindNpc(TEXT("FluffSmall")))
			{
				Small->Destroy();
			}
			if (ASparkyCharacter* P = Player())
			{
				P->HideModel(false);
				P->SetForm(true);
				AFx::Spawn(GetWorld(), EFx::Burst, P->GetActorLocation(), FLinearColor(1.f, 1.f, 0.5f), 4.f);
				AFx::Spawn(GetWorld(), EFx::Poof, P->GetActorLocation(), FLinearColor(1.f, 0.9f, 0.7f), 4.f);
				P->Shake(15.f);
			}
			Sfx(this, ESfx::Roar);
			Message(TEXT("YOU ARE FLUFFERNUTTER!"), FLinearColor(1.f, 0.9f, 0.4f));
			Radio(EWho::Captain, TEXT("W-what is THAT?! Get it! GET IT!"));
			int32 StartWave = 1;
			FParse::Value(FCommandLine::Get(), TEXT("Wave="), StartWave);  // dev: -Wave=3 for the Captain
			Ch6StartWave(FMath::Clamp(StartWave, 1, 3));
		});
		break;
	case 7:
		Converse({
			{EWho::Sparky, TEXT("The Labyrinth of Time. Gravitational distortions, wormholes, temporal rifts... echoes of the past and the future.")},
			{EWho::Leo, TEXT("Is 'fascinating' the word you're looking for? Because I was going to go with 'terrifying'.")},
			{EWho::Sparky, TEXT("Three time shards hold the way into the heart of the Labyrinth. West, east, and through the wormhole. Stay close.")},
			{EWho::Fluff, TEXT("*brave chirp!*")},
		});
		break;
	case 8:
		OnPadStep(nullptr);  // scramble the scientist's tiles
		FParse::Value(FCommandLine::Get(), TEXT("Keys="), Keys);  // dev: -Keys=3 for the Guardian
		Converse({
			{EWho::Sparky, TEXT("The Riddles of the Ancients. Challenges left by a long-lost civilization, to test the worthiness of those who seek the Labyrinth's secrets.")},
			{EWho::Leo, TEXT("Sparky, do you think we can solve these?")},
			{EWho::Sparky, TEXT("We have to. Three trials, three keys: the lost scientist, the warrior princess, and the riddle stones. Each portal leads to one.")},
		});
		break;
	case 9:
		Converse({
			{EWho::Sparky, TEXT("Floating islands... a sea of glowing energy... It's a living dimension, Leo. A conscious universe, responding to our thoughts and emotions.")},
			{EWho::Leo, TEXT("Hi, universe. I'm Leo. Big fan.")},
			{EWho::Sparky, TEXT("The temple is sealed by light. We'll need three motes of it. The luminous beings may help us - they speak in colours.")},
		});
		break;
	default:
		break;
	}
}

void ASparkyGameMode::LateTick(float Dt)
{
	if (Chapter == 5)
	{
		Ch5Tick(Dt);
	}
	else if (Chapter == 8 && Keys >= 3 && !bGuardianUp)
	{
		// the Guardian waits for Sparky back in the hub
		ASparkyCharacter* P = Player();
		if (P && FVector::Dist2D(P->GetActorLocation(), Levels::Spots::Ch8Hub) < 1100.f && State == EGameState::Playing)
		{
			bGuardianUp = true;
			Converse({
				{EWho::Guardian, TEXT("Seekers. We have watched you free the scientist, honour the princess, and answer the stones.")},
				{EWho::Guardian, TEXT("Now face the Guardian of the Labyrinth: a battle of wits, and of will. In all of our shapes.")},
				{EWho::Sparky, TEXT("Leo, Fluffernutter, stand back. This one's mine.")},
			}, [this]()
			{
				Boss = Spawn<AEnemy>(Levels::Spots::Ch8Hub + FVector(400, 0, 200), 180.f);
				Boss->Init(EEnemy::Guardian);
				BossName = TEXT("THE GUARDIAN");
				AFx::Spawn(GetWorld(), EFx::Poof, Boss->GetActorLocation(), FLinearColor(1.f, 0.85f, 0.4f), 4.f);
				Sfx(this, ESfx::Portal);
			});
		}
	}
}

// ---------------------------------------------------------------- pickups

void ASparkyGameMode::LatePickup(APickup* Pk)
{
	switch (Pk->GetType())
	{
	case EPickup::Berry:
		++Berries;
		Sfx(this, ESfx::Pickup);
		Message(FString::Printf(TEXT("MOONBERRY %d / 3"), Berries), FLinearColor(0.6f, 0.6f, 1.f));
		if (Berries >= 3 && Ch5Phase == 1)
		{
			Ch5Phase = 2;
			Radio(EWho::Sparky, FString::Printf(TEXT("Three moonberries. Now... slowly. Gently. Hold %s to sneak up on him in the village. No pouncing."), CrouchKey(Player())));
			Radio(EWho::Leo, TEXT("He's hiding by the gardens, I think!"));
		}
		break;
	case EPickup::Shard:
		++Shards;
		Sfx(this, ESfx::Pickup);
		Message(FString::Printf(TEXT("TIME SHARD %d / 3"), Shards), FLinearColor(0.5f, 1.f, 1.f));
		if (Shards >= 3)
		{
			Activate(TEXT("Core"));
			Radio(EWho::Sparky, TEXT("All three shards! The way to the heart of the Labyrinth is open. Back to the central platform."));
		}
		break;
	case EPickup::Key:
		Ch8KeyFound();
		break;
	case EPickup::Mote:
		++Motes;
		Sfx(this, ESfx::Pickup);
		Message(FString::Printf(TEXT("MOTE OF LIGHT %d / 3"), Motes), FLinearColor(1.f, 0.95f, 0.6f));
		if (Motes >= 3)
		{
			Activate(TEXT("Temple"));
			Sfx(this, ESfx::Objective);
			Radio(EWho::Sparky, TEXT("Three motes of light. The temple is opening - south, in the crystal cavern!"));
		}
		break;
	case EPickup::Sprite:
		++Sprites;
		Sfx(this, ESfx::Chirp);
		Message(FString::Printf(TEXT("ENERGY SPRITE %d / 5"), Sprites), FLinearColor(1.f, 0.6f, 0.95f));
		if (Sprites == 5)
		{
			APickup* M = Spawn<APickup>(Levels::Spots::Ch9SpriteMote + FVector(0, 0, 80.f));
			M->Init(EPickup::Mote);
			AFx::Spawn(GetWorld(), EFx::Burst, M->GetActorLocation(), FLinearColor(1.f, 0.9f, 0.6f), 3.f);
			Radio(EWho::Being, TEXT("(The playful ones chime with delight. Where they gathered, a mote of light glows.)"));
		}
		break;
	default:
		Sfx(this, ESfx::Pickup);
		break;
	}
}

// ---------------------------------------------------------------- talking

void ASparkyGameMode::LateTalk(ANpc* N)
{
	const FString Id = N->GetId().ToString();
	switch (Chapter)
	{
	case 5:
		if (Id == TEXT("Chief"))
		{
			if (Ch5Phase == 0)
			{
				Converse({
					{EWho::Chief, TEXT("Welcome, sky-travellers! We are simple people. Stone tools, our crops, our songs. Our home is your home.")},
					{EWho::Chief, TEXT("The forest to the north is full of wonders. Go, look! Only mind the spore-puffs. They sting.")},
				});
			}
			else
			{
				Converse({
					{EWho::Chief, TEXT("The little furry ones? They are shy, but they love moonberries more than anything.")},
					{EWho::Chief, TEXT("Moonberries grow at the top of the great tree in the forest, on the ledge beside the falls, and on the rock out in the lagoon.")},
					{EWho::Chief, FString::Printf(TEXT("And when you approach one: go slowly. Crouch low (%s). Quick paws frighten them."), CrouchKey(Player()))},
				});
			}
		}
		else if (Id.StartsWith(TEXT("Villager")))
		{
			static const TCHAR* Lines[] = {
				TEXT("You came down from the sky in a shiny seed! Can you go back up? Can I come?"),
				TEXT("This is my best digging stick. I made it myself. Do you have a digging stick?"),
				TEXT("The harvest was good this year. Have a turnip! ...Oh. You're a cat."),
				TEXT("The forest is that way. Past the big trees. You can't miss it. Well, you might. It's a forest."),
			};
			Converse({{EWho::Chief, Lines[GetTypeHash(N->GetId()) % 4]}});
		}
		break;
	case 6:
		if (Id == TEXT("Leo"))
		{
			Converse({{EWho::Leo, TEXT("Go get 'em, Fluffernutter! Who's a good monster? You are! YES YOU ARE!")}});
		}
		else if (Id == TEXT("Sparky"))
		{
			Converse({{EWho::Sparky, TEXT("(I owe that little creature an apology. And possibly my life.)")}});
		}
		break;
	case 7:
		if (Id == TEXT("EchoSparky"))
		{
			Converse({
				{EWho::Echo, TEXT("I am you, Sparky. A Sparky who went west first, in another timeline. You made the right call... probably.")},
				{EWho::Echo, TEXT("The rifts drift back and forth. Watch them, count, then go. And don't stop on the narrow parts.")},
			});
		}
		else if (Id == TEXT("EchoLeo"))
		{
			Converse({
				{EWho::Echo, TEXT("Hey. I'm Leo. From, like, a Tuesday that hasn't happened yet. The purple bubble makes you float!")},
				{EWho::Echo, TEXT("Jump inside it and you'll drift right across the gap. Future me says it's fun. Future me is never wrong. Mostly.")},
			});
		}
		else if (Id == TEXT("EchoFluff"))
		{
			Converse({{EWho::Echo, TEXT("*a ghostly chirp that sounds like it came from a thousand years away... it points at the wormhole*")}});
		}
		break;
	case 8:
		if (Id == TEXT("Scientist"))
		{
			if (bPadsSolved)
			{
				Converse({{EWho::Scientist, TEXT("Free at last. My research is yours, little genius. Use it well.")}});
			}
			else
			{
				Converse({
					{EWho::Scientist, TEXT("Trapped... in the loop of my final moments. Help me, and my research is yours.")},
					{EWho::Scientist, TEXT("A puzzle of quantum states: step onto a tile and it flips - and so do its neighbours. Light every tile to break the loop.")},
					{EWho::Scientist, TEXT("(The tiles reset to how they began.)")},
				}, [this]() { OnPadStep(nullptr); });
			}
		}
		else if (Id == TEXT("Princess"))
		{
			if (PrincessWave == 0)
			{
				Converse({
					{EWho::Princess, TEXT("I am frozen in a moment of triumph and defeat. A thousand years I have stood my last stand.")},
					{EWho::Princess, TEXT("Show me your courage, small one, and I will show you the hidden arsenal of my people. Face the wraiths of my final battle!")},
				}, [this]() { Ch8PrincessWave(1); });
			}
			else if (PrincessWave >= 3)
			{
				Converse({{EWho::Princess, TEXT("Your bravery honours my memory. Go now, with my key, and my thanks.")}});
			}
			else
			{
				Converse({{EWho::Princess, TEXT("Fight! They will not wait for you!")}});
			}
		}
		else if (Id.StartsWith(TEXT("Riddle")))
		{
			const int32 I = FCString::Atoi(*Id.RightChop(6));
			if (RiddlesSolved & (1 << I))
			{
				Converse({{EWho::Being, TEXT("(The stone hums contentedly. It has been answered.)")}});
			}
			else
			{
				Ch8Riddle(N, I);
			}
		}
		break;
	case 9:
	{
		static const TCHAR* Hints[] = {
			TEXT("(The being pulses pink and blue. Small, playful lights dart round the island behind it. They want to be chased.)"),
			TEXT("(It glows a slow upward gold: something waits at the top of the crystal spire. The moving crystals will carry you.)"),
			TEXT("(It shimmers like a beam of light rising. Step into the beams to the west and ride them up to the island in the sky.)"),
			TEXT("(It turns the colour of warm stone and points south: the temple in the crystal cavern, sealed until three motes return.)"),
		};
		if (Id.StartsWith(TEXT("Being")))
		{
			Converse({{EWho::Being, Hints[FCString::Atoi(*Id.RightChop(5)) % 4]}});
		}
		break;
	}
	default:
		break;
	}
}

// ---------------------------------------------------------------- goals

void ASparkyGameMode::LateGoal(AGoal* G)
{
	const FName Id = G->GetId();
	if (Chapter == 5 && Id == TEXT("Forest"))
	{
		Ch5Phase = 1;
		Converse({
			{EWho::Leo, TEXT("Look at this little guy, Sparky! Isn't he adorable?")},
			{EWho::Sparky, TEXT("(Small. Furry. Moving. Every instinct I have says: stalk it, play with it... then eat it.)")},
			{EWho::Sparky, TEXT("Leo, put that thing down. It's prey, not a pet.")},
			{EWho::Leo, TEXT("Prey?! You're not thinking of... you wouldn't EAT him, would you?")},
			{EWho::Sparky, TEXT("It's my nature, Leo. I can't help it.")},
			{EWho::Leo, TEXT("Well, I'm calling him Fluffernutter. And he's coming with us.")},
			{EWho::Fluff, TEXT("*EEEEEP!*")},
			{EWho::Leo, TEXT("You scared him! He ran off toward the village... Sparky, please. Find something he likes, and be gentle.")},
		}, [this]()
		{
			if (ANpc* F = FindNpc(TEXT("Fluff")))
			{
				AFx::Spawn(GetWorld(), EFx::Poof, F->GetActorLocation(), FLinearColor(1.f, 0.9f, 0.8f));
				const FVector S = Levels::Spots::Ch5Fluff[0];
				F->SetActorLocation(FVector(S.X, S.Y, F->GetActorLocation().Z));
			}
			Message(TEXT("FIND 3 MOONBERRIES - ASK THE CHIEF"));
		});
	}
	else if (Chapter == 5 && Id == TEXT("Ship"))
	{
		Converse({
			{EWho::Sparky, TEXT("All right. A compromise. He can stay, as long as he's kept away from the sensitive areas of the ship.")},
			{EWho::Leo, TEXT("I'll watch over him, I promise. He won't get in your way.")},
			{EWho::Fluff, TEXT("*chirp!*")},
			{EWho::Sparky, TEXT("...And no chewing the wiring.")},
		}, [this]() { ChapterWon(); });
	}
	else if (Chapter == 7 && Id == TEXT("Core"))
	{
		Converse({
			{EWho::None, TEXT("The three shards rose and spun together, and the heart of the Labyrinth opened like an eye.")},
			{EWho::None, TEXT("Inside: a chamber suspended in a timeless void, and at its centre a holographic interface projecting ancient symbols and cryptic riddles.")},
			{EWho::Sparky, TEXT("Leo... I think the Labyrinth wants to test us.")},
		}, [this]() { ChapterWon(); });
	}
	else if (Chapter == 9 && Id == TEXT("Temple"))
	{
		Converse({
			{EWho::Entity, TEXT("Travellers. You are part of the cosmic dance, and your journey has brought you here to learn and grow.")},
			{EWho::Entity, TEXT("Every living thing is connected: the cat, the human, the small furry one, the stars. Embrace the unknown, and you will find your true selves.")},
			{EWho::Sparky, TEXT("Thank you. I... think I understand. Or I'm starting to.")},
			{EWho::Leo, TEXT("I understood about half. Which, for me, is a personal best.")},
			{EWho::Fluff, TEXT("*a chirp of pure, uncomplicated joy*")},
		}, [this]() { ChapterWon(); });
	}
}

// ---------------------------------------------------------------- fights

void ASparkyGameMode::LateEnemyKilled(AEnemy* E)
{
	if (Chapter == 6)
	{
		if (FoesLeft() > 0)
		{
			return;
		}
		if (Ch6Wave < 3)
		{
			Sfx(this, ESfx::Roar, FVector(NAN), 0.7f);
			Ch6StartWave(Ch6Wave + 1);
			return;
		}
		Converse({
			{EWho::Captain, TEXT("This isn't over! I'll be back, you overgrown dust bunny! ARRR-ROOOOO!")},
			{EWho::None, TEXT("The pirate leader's once-mighty crew was a whimpering, cowering handful, their ship a smoking ruin. With a triumphant roar, Fluffernutter returned to his normal form.")},
		}, [this]()
		{
			if (ASparkyCharacter* P = Player())
			{
				AFx::Spawn(GetWorld(), EFx::Poof, P->GetActorLocation(), FLinearColor(1.f, 0.95f, 0.8f), 3.f);
			}
			Converse({
				{EWho::Leo, TEXT("Let's get out of here, Sparky!")},
				{EWho::Sparky, TEXT("You don't have to tell me twice!")},
				{EWho::Sparky, TEXT("You saved us, little buddy. I guess you're not just prey after all.")},
				{EWho::Fluff, TEXT("*happy chirp, snuggling against Leo*")},
				{EWho::Leo, TEXT("You're a hero, Fluffernutter. A true friend.")},
			}, [this]() { ChapterWon(); });
		});
		return;
	}
	if (Chapter == 8)
	{
		if (E->GetType() == EEnemy::Guardian)
		{
			Converse({
				{EWho::Guardian, TEXT("Enough. We have not seen your like in eons.")},
				{EWho::Guardian, TEXT("You have proven yourselves worthy of the Labyrinth's secrets. Go forth with our blessing.")},
				{EWho::None, TEXT("Ahead of them, at the very heart of the Labyrinth, a shimmering wormhole opened. Its surface rippled with colours beyond the visible spectrum.")},
			}, [this]() { ChapterWon(); });
			return;
		}
		if (PrincessWave == 1 || PrincessWave == 2)
		{
			int32 Wraiths = 0;
			for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
			{
				Wraiths += !It->IsDead() && It->GetType() == EEnemy::Wraith ? 1 : 0;
			}
			if (Wraiths == 0)
			{
				if (PrincessWave == 1)
				{
					Message(TEXT("THE WRAITHS RETURN!"));
					Ch8PrincessWave(2);
				}
				else
				{
					PrincessWave = 3;
					Sfx(this, ESfx::Objective);
					APickup* K = Spawn<APickup>(Levels::Spots::Ch8Arena + FVector(0, 0, 100.f));
					K->Init(EPickup::Key);
					Converse({{EWho::Princess, TEXT("Well fought! The arsenal of my people opens to you - and with it, my key.")}});
				}
			}
		}
	}
}

// ---------------------------------------------------------------- chapter 5

void ASparkyGameMode::Ch5Tick(float Dt)
{
	if (Ch5Phase != 2 || State != EGameState::Playing)
	{
		return;
	}
	ASparkyCharacter* P = Player();
	ANpc* F = FindNpc(TEXT("Fluff"));
	if (!P || !F)
	{
		return;
	}
	const float D = FVector::Dist2D(P->GetActorLocation(), F->GetActorLocation());
	const float Speed = P->GetVelocity().Size2D();
	if (D > 550.f)
	{
		return;
	}
	if (P->IsSneaking() || Speed < 100.f)
	{
		Trust = FMath::Min(1.f, Trust + Dt / 3.5f * (D < 300.f ? 1.5f : 1.f));
	}
	else if (Speed > 330.f && D < 480.f)
	{
		// too fast: he bolts to another hiding place
		AFx::Spawn(GetWorld(), EFx::Poof, F->GetActorLocation(), FLinearColor(1.f, 0.9f, 0.8f));
		FluffSpot = (FluffSpot + 1) % 3;
		const FVector S = Levels::Spots::Ch5Fluff[FluffSpot];
		F->SetActorLocation(FVector(S.X, S.Y, F->GetActorLocation().Z));
		Trust = FMath::Max(0.f, Trust - 0.35f);
		Sfx(this, ESfx::Chirp, S);
		Message(FString::Printf(TEXT("TOO FAST! HOLD %s TO SNEAK"), CrouchKey(P)), FLinearColor(1.f, 0.6f, 0.4f));
		return;
	}
	if (Trust >= 1.f)
	{
		Ch5Phase = 3;
		Converse({
			{EWho::Fluff, TEXT("*sniff sniff... chirp?*")},
			{EWho::Sparky, TEXT("(Easy, little one. Easy. I brought you moonberries.)")},
			{EWho::Fluff, TEXT("*nom nom nom... happy chirrup!*")},
			{EWho::Leo, TEXT("He likes you! See, Sparky? Not prey. Friend.")},
			{EWho::Sparky, TEXT("...Fine. He can come. Back to the ship, both of you.")},
		}, [this, F]()
		{
			const FVector At = F->GetActorLocation();
			F->Destroy();
			ALeoBuddy* B = Spawn<ALeoBuddy>(At + FVector(0, 0, 20.f));
			B->Init(ECreature::Fluffernutter, 160.f);
			Activate(TEXT("Ship"));
			Sfx(this, ESfx::Chirp);
			Message(TEXT("FLUFFERNUTTER JOINS THE CREW!"), FLinearColor(1.f, 0.85f, 0.5f));
		});
	}
}

// ---------------------------------------------------------------- chapter 6

void ASparkyGameMode::Ch6StartWave(int32 N)
{
	Ch6Wave = N;
	auto Dog = [this](const FVector& At, int32 Var)
	{
		AEnemy* E = Spawn<AEnemy>(At + FVector(0, 0, 90.f), 180.f);
		E->Init(EEnemy::PirateDog, Var);
		E->SetAggro(4000.f);
		E->SetLeash(0.f);
		AFx::Spawn(GetWorld(), EFx::Poof, At + FVector(0, 0, 60.f), FLinearColor(0.8f, 0.8f, 0.8f), 1.5f);
	};
	if (N == 1)
	{
		for (int32 i = 0; i < 4; ++i)
		{
			Dog(Levels::Spots::Ch6Wave1[i], i);
		}
	}
	else if (N == 2)
	{
		Radio(EWho::Leo, TEXT("More of them, in the corridor and the engine room! Go, Fluffernutter!"));
		for (int32 i = 0; i < 4; ++i)
		{
			Dog(Levels::Spots::Ch6Wave2[i], i);
		}
	}
	else
	{
		Converse({
			{EWho::Captain, TEXT("Useless! The lot of you! Fine. If ye want something done right...")},
			{EWho::Captain, TEXT("...ye sniff it out yerself. Come here, ye overgrown hamster!")},
		}, [this]()
		{
			Boss = Spawn<AEnemy>(Levels::Spots::Ch6Captain + FVector(0, 0, 100.f), 180.f);
			Boss->Init(EEnemy::Captain);
			BossName = TEXT("CAPT. CROTCH SNIFFER");
			AFx::Spawn(GetWorld(), EFx::Poof, Boss->GetActorLocation(), FLinearColor(1.f, 0.6f, 0.5f), 3.f);
			Message(TEXT("THE CAPTAIN IS IN THE ENGINE ROOM!"));
		});
	}
}

// ---------------------------------------------------------------- chapter 8

void ASparkyGameMode::OnPadStep(AFloorPad* Pad)
{
	if (Chapter != 8 || bPadsSolved)
	{
		return;
	}
	TArray<AFloorPad*> Pads;
	Pads.SetNumZeroed(9);
	for (TActorIterator<AFloorPad> It(GetWorld()); It; ++It)
	{
		if (It->GetIndex() >= 0 && It->GetIndex() < 9)
		{
			Pads[It->GetIndex()] = *It;
		}
	}
	TArray<bool> Lit;
	Lit.Init(true, 9);
	if (!Pad)
	{
		// reset: everything lit, then scrambled by three presses (so three presses solve it)
		for (int32 I : {1, 3, 8})
		{
			Flip(Lit, I);
		}
	}
	else
	{
		for (int32 i = 0; i < 9; ++i)
		{
			Lit[i] = Pads[i] && Pads[i]->IsLit();
		}
		Flip(Lit, Pad->GetIndex());
		Sfx(this, ESfx::Zap, Pad->GetActorLocation(), 0.5f);
	}
	bool bAll = true;
	for (int32 i = 0; i < 9; ++i)
	{
		if (Pads[i])
		{
			Pads[i]->SetLit(Lit[i]);
		}
		bAll &= Lit[i];
	}
	if (bAll && Pad)
	{
		bPadsSolved = true;
		for (AFloorPad* P : Pads)
		{
			if (P)
			{
				P->Lock();
			}
		}
		Sfx(this, ESfx::Correct);
		APickup* K = Spawn<APickup>(Levels::Spots::Ch8Lab + FVector(0, 600, 120.f));
		K->Init(EPickup::Key);
		Converse({{EWho::Scientist, TEXT("The loop... is broken. After so long. Take this key, little genius, and my research with it.")}});
	}
}

void ASparkyGameMode::Ch8PrincessWave(int32 N)
{
	PrincessWave = N;
	const int32 Count = N == 1 ? 3 : 5;
	for (int32 i = 0; i < Count; ++i)
	{
		const float A = 2 * PI * i / Count;
		const FVector At = Levels::Spots::Ch8Arena + FVector(FMath::Cos(A) * 650.f, FMath::Sin(A) * 650.f, 120.f);
		AEnemy* E = Spawn<AEnemy>(At, FMath::RadiansToDegrees(A) + 180.f);
		E->Init(EEnemy::Wraith, i);
		AFx::Spawn(GetWorld(), EFx::Poof, At, FLinearColor(0.5f, 0.9f, 1.f), 1.5f);
	}
	Sfx(this, ESfx::Portal);
}

void ASparkyGameMode::Ch8Riddle(ANpc* Terminal, int32 Index)
{
	const FRiddle& R = TerminalRiddle(FMath::Clamp(Index, 0, 2));
	Ask(EWho::Being, R.Question, Options(R), [this, Index, Answer = R.Answer, Terminal](int32 Choice)
	{
		if (Choice == Answer)
		{
			RiddlesSolved |= 1 << Index;
			Sfx(this, ESfx::Correct);
			Message(TEXT("CORRECT!"), FLinearColor(0.5f, 1.f, 0.6f));
			if (Terminal)
			{
				Terminal->GetRig()->SetTint(FLinearColor(0.6f, 1.2f, 0.6f));
			}
			if (RiddlesSolved == 7)
			{
				APickup* K = Spawn<APickup>(Levels::Spots::Ch8Riddles + FVector(0, 0, 120.f));
				K->Init(EPickup::Key);
				Radio(EWho::Sparky, TEXT("All three stones answered. And there's the key."));
			}
		}
		else
		{
			Sfx(this, ESfx::Wrong);
			Message(TEXT("WRONG! THE STONE CRACKLES"), FLinearColor(1.f, 0.4f, 0.3f));
			if (ASparkyCharacter* P = Player())
			{
				P->Hurt(1, Terminal ? Terminal->GetActorLocation() : P->GetActorLocation());
			}
		}
	});
}

void ASparkyGameMode::Ch8KeyFound()
{
	++Keys;
	Sfx(this, ESfx::Pickup);
	Message(FString::Printf(TEXT("ANCIENT KEY %d / 3"), Keys), FLinearColor(1.f, 0.85f, 0.4f));
	if (Keys >= 3)
	{
		Sfx(this, ESfx::Objective);
		Radio(EWho::Sparky, TEXT("Three keys. Something is stirring back in the hub..."));
	}
}

void ASparkyGameMode::OnGuardianShift(int32 NewForm)
{
	struct FShift { const TCHAR* Taunt; FRiddle Riddle; };
	static const FShift Shifts[] = {
		{TEXT(""), {TEXT(""), {}, 0}},
		{TEXT("You are quick. But are you clever? Answer, or we grow stronger."),
		 {TEXT("I can be cracked, made, told, and played. What am I?"), {TEXT("AN EGG"), TEXT("A JOKE"), TEXT("A SECRET CODE")}, 1}},
		{TEXT("One last question, the oldest one. Answer truly."),
		 {TEXT("Why do you do what you do? What is it all for?"), {TEXT("CAT FOOD. MOSTLY CAT FOOD."), TEXT("DISCOVERY - AND THE FRIENDS YOU SHARE IT WITH"), TEXT("NOTHING AT ALL")}, 1}},
	};
	if (NewForm < 1 || NewForm > 2)
	{
		return;
	}
	const FShift& S = Shifts[NewForm];
	Converse({{EWho::Guardian, S.Taunt}}, [this, NewForm]()
	{
		const FRiddle& R = Shifts[NewForm].Riddle;
		Ask(EWho::Guardian, R.Question, Options(R), [this, Answer = R.Answer](int32 Choice)
		{
			if (Choice == Answer)
			{
				Sfx(this, ESfx::Correct);
				Message(TEXT("THE GUARDIAN IS IMPRESSED"), FLinearColor(0.5f, 1.f, 0.6f));
			}
			else
			{
				Sfx(this, ESfx::Wrong);
				Message(TEXT("WRONG! THE GUARDIAN GROWS STRONGER"), FLinearColor(1.f, 0.4f, 0.3f));
				if (Boss)
				{
					Boss->Heal(3);
				}
			}
		});
	});
}

// ---------------------------------------------------------------- objectives

FString ASparkyGameMode::LateObjective() const
{
	switch (Chapter)
	{
	case 5:
		switch (Ch5Phase)
		{
		case 0: return TEXT("EXPLORE THE FOREST TO THE NORTH");
		case 1: return FString::Printf(TEXT("FIND MOONBERRIES: %d / 3"), Berries);
		case 2: return FString::Printf(TEXT("SNEAK UP ON FLUFFERNUTTER (HOLD %s)"), CrouchKey(Player()));
		default: return TEXT("BACK TO THE SHIP");
		}
	case 6:
		if (Ch6Wave == 0) return TEXT("");
		if (Ch6Wave < 3) return FString::Printf(TEXT("SMASH THE PIRATES: %d LEFT"), FoesLeft());
		return TEXT("DEFEAT CAPTAIN CROTCH SNIFFER");
	case 7:
		return Shards < 3 ? FString::Printf(TEXT("FIND THE TIME SHARDS: %d / 3"), Shards) : FString(TEXT("ENTER THE HEART OF THE LABYRINTH"));
	case 8:
		if (Keys < 3) return FString::Printf(TEXT("COMPLETE THE TRIALS: KEYS %d / 3"), Keys);
		return bGuardianUp ? TEXT("DEFEAT THE GUARDIAN") : TEXT("RETURN TO THE HUB");
	case 9:
		return Motes < 3 ? FString::Printf(TEXT("GATHER MOTES OF LIGHT: %d / 3"), Motes) : FString(TEXT("ENTER THE TEMPLE"));
	default:
		return TEXT("");
	}
}
