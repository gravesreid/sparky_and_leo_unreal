#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Enemy.generated.h"

class UCreatureRig;

enum class EEnemy : uint8
{
	BogBlob,    // chapter 1: a hopping lump of cranberry jelly
	AlleyCat,   // chapter 2: crouches, hisses, lunges
	Meatball,   // chapter 2 boss: lunges, and leaps to slam out shockwaves
	PirateDog,  // chapter 3: Captain Crotch Sniffer's boarders, with cutlasses
	Captain,    // chapter 6 boss: Captain Crotch Sniffer himself
	Wraith,     // chapter 8: ghostly cats in the warrior princess's trial
	Guardian    // chapter 8 boss: the shapeshifting guardian of the Labyrinth
};

/**
 * Everything that wants to hurt Sparky. Swipe or pounce them, or land on their heads. Simple
 * state machines in Tick: idle near home, chase when Sparky comes close, wind up, attack, recover.
 */
UCLASS()
class SPARKYLEO_API AEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	AEnemy();
	void Init(EEnemy InType, int32 Variant = 0);
	virtual void Tick(float Dt) override;
	virtual void Landed(const FHitResult& Hit) override;

	/** bStomp: landed on from above. */
	void TakeHit(int32 Damage, const FVector& From, bool bStomp);
	bool IsDead() const { return bDead; }
	EEnemy GetType() const { return Type; }
	int32 GetHP() const { return HP; }
	int32 GetMaxHP() const { return MaxHP; }
	float GetTop() const;
	float GetRadius() const;
	/** How far it chases from home before giving up (0 = anywhere). */
	void SetLeash(float R) { Leash = R; }
	void SetAggro(float R) { Aggro = R; }
	/** The Guardian grows stronger (a wrong answer): back up, but not past its current shape's share. */
	void Heal(int32 N) { HP = FMath::Min(HP + N, Type == EEnemy::Guardian ? 18 - 6 * Form : MaxHP); }
	bool IsBoss() const { return Type == EEnemy::Meatball || Type == EEnemy::Captain || Type == EEnemy::Guardian; }

private:
	enum class EState : uint8 { Idle, Chase, Windup, Attack, Recover, Hurt, Leap, Dizzy };

	UPROPERTY()
	UCreatureRig* Rig;
	EEnemy Type = EEnemy::BogBlob;
	EEnemy Act = EEnemy::BogBlob;  // how it fights (the Captain fights like his crew, wraiths like alley cats...)
	int32 Form = 0;                // the Guardian's current shape
	bool bCharge = false;          // the Captain's charge
	bool bSummoned = false;        // the Captain has called for backup
	EState State = EState::Idle;
	float StateTime = 0.f;
	int32 HP = 1;
	int32 MaxHP = 1;
	bool bDead = false;
	float DeadTime = 0.f;
	FVector Home;
	float Leash = 1500.f;
	float Aggro = 900.f;
	float Flash = 0.f;
	float Cooldown = 0.f;
	bool bHitThisAttack = false;
	int32 Lunges = 0;
	float WanderYaw = 0.f;
	float WanderTime = 0.f;

	void SetState(EState S);
	void Think(float Dt);
	void FacePlayer(float Dt, float Rate = 540.f);
	void TouchDamage();
	void Die();
	void SetGuardianForm(int32 NewForm);
};
