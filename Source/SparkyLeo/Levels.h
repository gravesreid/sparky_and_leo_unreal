#pragma once

#include "CoreMinimal.h"
#include "Sfx.h"

class ALevelGeo;
class ASparkyGameMode;

/** The chapters' worlds, built in code: blocks into the level geometry, then the actors. */
namespace Levels
{
	struct FSetup
	{
		FVector Start = FVector(0, 0, 100);
		float Yaw = 0.f;
		EMusic Music = EMusic::None;
	};

	FSetup Build(int32 Chapter, ASparkyGameMode* GM, ALevelGeo* Geo);
}

/** Places the chapter scripts (SparkyGameModeChapters.cpp) need to know about. */
namespace Levels::Spots
{
	// chapter 5: the forest clearing, and where Fluffernutter hides in the village
	inline const FVector Ch5Forest(3000, 1100, 0);
	inline const FVector Ch5Fluff[3] = {FVector(1450, -1350, 0), FVector(2300, -1500, 0), FVector(2150, -500, 0)};
	// chapter 6: where the pirates come from
	inline const FVector Ch6Wave1[4] = {FVector(1700, 500, 0), FVector(1700, -500, 0), FVector(2000, 0, 0), FVector(1200, -700, 0)};
	inline const FVector Ch6Wave2[4] = {FVector(3000, 0, 0), FVector(4300, 450, 0), FVector(4300, -450, 0), FVector(4800, 0, 0)};
	inline const FVector Ch6Captain(4500, 0, 0);
	// chapter 8: the hub and the three trial rooms
	inline const FVector Ch8Hub(0, 0, 0);
	inline const FVector Ch8Lab(0, -6000, 0);
	inline const FVector Ch8Arena(6000, 0, 0);
	inline const FVector Ch8Riddles(-6000, 0, 0);
	// chapter 9: where the mote appears once every energy sprite is caught
	inline const FVector Ch9SpriteMote(3500, 0, 100);
}

