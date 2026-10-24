#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CreatureRig.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
struct FRetroMesh;

enum class ECreature : uint8
{
	Sparky, Leo, Local, Elder, AlleyCat, Meatball, PirateDog, Captain, BogBlob,
	Fluffernutter,  // small, furry, big soulful eyes
	FluffBig,       // ...transformed: muscles bulging, eyes glowing
	Villager,       // the green planet's people (chapter 5)
	Being           // a luminous floating being (chapter 9), also the riddle terminals
};

enum class EPart : uint8
{
	Body, Head, Tail, LegFL, LegFR, LegBL, LegBR, Hat, Prop,
	Count
};

/** What the owner is doing this frame; the rig turns it into a pose. */
struct FRigPose
{
	float Speed = 0.f;      // cm/s over the ground
	bool bAir = false;
	float VZ = 0.f;
	float Attack = 0.f;     // 0..1 through a swipe / swing, 0 = not attacking
	float Flip = 0.f;       // 0..1 through a double-jump somersault
	bool bCling = false;    // stuck to a velcro wall
	float Crouch = 0.f;     // 0..1 (ground pound wind-up, wind-up before a lunge)
	bool bWeld = false;
	bool bDizzy = false;
	float Talk = 0.f;       // >0 while speaking: head bob
};

/**
 * A low-poly creature made of rigid parts (body, head, tail, four limbs, hat, held prop), each
 * its own procedural mesh pivoting at the joint, animated in code: N64 characters were mostly
 * rigid pieces like this. Models face +X with the feet at the component origin.
 */
UCLASS()
class SPARKYLEO_API UCreatureRig : public USceneComponent
{
	GENERATED_BODY()

public:
	UCreatureRig();

	/** Variant picks fur colours / hat sizes. */
	void Build(ECreature Kind, int32 Variant = 0);
	/** Sparky's crafted hat on or off. Locals: Variant is their hat level. */
	void SetHat(bool bOn);

	void Animate(float Dt, const FRigPose& Pose);
	void SetFlash(float Amount, const FLinearColor& Color = FLinearColor(1.f, 0.2f, 0.1f));
	void SetHidden(bool bHide);
	/** Tints the whole model (ghosts, echoes). White = normal. */
	void SetTint(const FLinearColor& Tint);
	/** Draws the model bigger (the Guardian's giant forms). */
	void SetScale(float S) { ScaleMul = S; }
	/** Where the feet are relative to the parent (minus the capsule's half height). */
	void SetBaseZ(float Z) { BaseZ = Z; }

	ECreature GetKind() const { return Kind; }
	/** Height of the model's top (for name tags, stomp checks). */
	float GetHeight() const { return Height; }

private:
	UPROPERTY()
	TArray<UProceduralMeshComponent*> Parts;
	UPROPERTY()
	UMaterialInstanceDynamic* Mid = nullptr;

	ECreature Kind = ECreature::Sparky;
	int32 Variant = 0;
	float Height = 50.f;
	float Phase = 0.f;
	float Time = 0.f;
	float BaseZ = 0.f;
	float ScaleMul = 1.f;
	FVector Pivot[int32(EPart::Count)];
	bool bHatOn = false;

	UProceduralMeshComponent* Part(EPart P) const { return Parts.IsValidIndex(int32(P)) ? Parts[int32(P)] : nullptr; }
	void MakePart(EPart P, const FVector& InPivot, const FRetroMesh& M);
	void BuildCat(float Scale, const FLinearColor& Fur, const FLinearColor& Stripe, const FLinearColor& Light, bool bSparky, bool bScarred);
	void BuildHuman(bool bLeo, bool bElder);
	void BuildDog(bool bCaptain);
	void BuildBlob();
	void BuildFluff(bool bBig);
	void BuildBeing();
	void BuildHat();
};
