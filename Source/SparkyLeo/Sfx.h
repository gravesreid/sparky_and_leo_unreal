#pragma once

#include "CoreMinimal.h"

/** Sound effects, made by Scripts/make_audio.py and imported to /Game/Audio. */
enum class ESfx : uint8
{
	Jump, DoubleJump, Land, Footstep, Swipe, Pounce, GroundPound, Hit, Poof, Hurt, Meow, Purr, Hiss,
	Catnip, Pickup, Styro, Velcro, Talk, MenuMove, MenuSelect, Pause, Objective, Checkpoint,
	Bark, Growl, LaserWarn, LaserBlast, Explosion, Alarm, Weld, Zap, Steam, Hyperspace, Cheer, Boing, Splash,
	HatCraft, ChapterStart, ChapterComplete, Death, Clang, Gun, OneUp,
	Chirp, Roar, Portal, Correct, Wrong,
	Count
};

/** Asset names per effect; several names = variants picked at random. */
inline const TArray<const TCHAR*>& SfxNames(ESfx S)
{
	static const TArray<TArray<const TCHAR*>> Names = {
		{TEXT("Jump")}, {TEXT("DoubleJump")}, {TEXT("Land")}, {TEXT("Footstep1"), TEXT("Footstep2")}, {TEXT("Swipe")},
		{TEXT("Pounce")}, {TEXT("GroundPound")}, {TEXT("Hit")}, {TEXT("Poof")}, {TEXT("Hurt")},
		{TEXT("Meow1"), TEXT("Meow2"), TEXT("Meow3")}, {TEXT("Purr")}, {TEXT("Hiss")},
		{TEXT("Catnip")}, {TEXT("Pickup")}, {TEXT("Styro")}, {TEXT("Velcro")}, {TEXT("Talk")}, {TEXT("MenuMove")},
		{TEXT("MenuSelect")}, {TEXT("Pause")}, {TEXT("Objective")}, {TEXT("Checkpoint")},
		{TEXT("Bark")}, {TEXT("Growl")}, {TEXT("LaserWarn")}, {TEXT("LaserBlast")}, {TEXT("Explosion")}, {TEXT("Alarm")},
		{TEXT("Weld")}, {TEXT("Zap")}, {TEXT("Steam")}, {TEXT("Hyperspace")}, {TEXT("Cheer")}, {TEXT("Boing")},
		{TEXT("Splash")}, {TEXT("HatCraft")}, {TEXT("ChapterStart")}, {TEXT("ChapterComplete")}, {TEXT("Death")},
		{TEXT("Clang")}, {TEXT("Gun")}, {TEXT("OneUp")},
		{TEXT("Chirp1"), TEXT("Chirp2")}, {TEXT("Roar")}, {TEXT("Portal")}, {TEXT("Correct")}, {TEXT("Wrong")},
	};
	check(Names.Num() == int32(ESfx::Count));
	return Names[int32(S)];
}

enum class EMusic : uint8
{
	None, Title, Cranberry, Chinatown, Pirates, Crisis, Jungle, Labyrinth, Dimension
};

inline const TCHAR* MusicName(EMusic M)
{
	switch (M)
	{
	case EMusic::Title: return TEXT("MusicTitle");
	case EMusic::Cranberry: return TEXT("MusicCranberry");
	case EMusic::Chinatown: return TEXT("MusicChinatown");
	case EMusic::Pirates: return TEXT("MusicPirates");
	case EMusic::Crisis: return TEXT("MusicCrisis");
	case EMusic::Jungle: return TEXT("MusicJungle");
	case EMusic::Labyrinth: return TEXT("MusicLabyrinth");
	case EMusic::Dimension: return TEXT("MusicDimension");
	default: return nullptr;
	}
}
