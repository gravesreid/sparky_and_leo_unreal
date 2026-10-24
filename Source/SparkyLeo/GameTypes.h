#pragma once

#include "CoreMinimal.h"

/** Who is talking (picks the dialog portrait). */
enum class EWho : uint8
{
	None, Sparky, Leo, Elder, Local, Captain, Meatball, Crowd,
	Fluff, Chief, Scientist, Princess, Guardian, Entity, Echo, Being
};

/** One line of dialog. */
struct FLine
{
	EWho Who = EWho::None;
	FString Text;
};

enum class EPickup : uint8
{
	Catnip, Styrofoam, PeanutButter, CatFood, Tang, Soup, DrivePart, Welder, Fish,
	Berry,   // chapter 5: moonberries, Fluffernutter's favourite
	Shard,   // chapter 7: time shards
	Key,     // chapter 8: the ancients' keys (one per trial)
	Mote,    // chapter 9: motes of living light
	Sprite   // chapter 9: energy sprites that run away
};

inline const TCHAR* PickupName(EPickup P)
{
	switch (P)
	{
	case EPickup::Catnip: return TEXT("CATNIP");
	case EPickup::Styrofoam: return TEXT("STYROFOAM");
	case EPickup::PeanutButter: return TEXT("PEANUT BUTTER");
	case EPickup::CatFood: return TEXT("CAT FOOD");
	case EPickup::Tang: return TEXT("TANG");
	case EPickup::Soup: return TEXT("CANNED SOUP");
	case EPickup::DrivePart: return TEXT("ATOM CRUNCH PART");
	case EPickup::Welder: return TEXT("WELDER");
	case EPickup::Fish: return TEXT("FISH");
	case EPickup::Berry: return TEXT("MOONBERRY");
	case EPickup::Shard: return TEXT("TIME SHARD");
	case EPickup::Key: return TEXT("ANCIENT KEY");
	case EPickup::Mote: return TEXT("MOTE OF LIGHT");
	case EPickup::Sprite: return TEXT("ENERGY SPRITE");
	}
	return TEXT("");
}

enum class EGameState : uint8
{
	Title, Story, Playing, Paused, Calibrate, Cutscene, Question, ChapterClear, GameOver, End
};
