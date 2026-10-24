#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameTypes.h"
#include "CreatureRig.h"
#include "Props.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class ASparkyCharacter;
struct FRetroMesh;

/** Something to collect: catnip, styrofoam, supplies, the drive part, the welder, a fish. */
UCLASS()
class SPARKYLEO_API APickup : public AActor
{
	GENERATED_BODY()

public:
	APickup();
	void Init(EPickup InType, bool bInActive = true);
	void SetActive(bool bOn);
	bool IsActive() const { return bActive; }
	EPickup GetType() const { return Type; }
	virtual void Tick(float Dt) override;

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	EPickup Type = EPickup::Catnip;
	bool bActive = true;
	FVector Home;
	FVector Origin;  // energy sprites run about, but stay near here
	float Time = 0.f;
};

/** Someone to talk to, or to look at: locals, the elder, the crowd, Leo at the controls. */
UCLASS()
class SPARKYLEO_API ANpc : public AActor
{
	GENERATED_BODY()

public:
	ANpc();
	void Init(ECreature Kind, int32 Variant, FName InId, bool bInTalk, bool bInDance = false);
	virtual void Tick(float Dt) override;
	FName GetId() const { return Id; }
	bool CanTalk() const { return bTalk; }
	void SetTalking(float Seconds) { TalkTime = Seconds; }
	float GetHeight() const;

private:
	UPROPERTY()
	UCreatureRig* Rig;
	FName Id;
	bool bTalk = false;
public:
	UCreatureRig* GetRig() const { return Rig; }
private:
	bool bDance = false;
	float TalkTime = 0.f;
	float Time = 0.f;
	float HomeYaw = 0.f;
};

/**
 * A friend tagging along behind Sparky: Leo (chapters 1, 7-9) or Fluffernutter (5, 7-9). Catches up
 * by walking, or by "taking a shortcut" if left far behind.
 */
UCLASS()
class SPARKYLEO_API ALeoBuddy : public ACharacter
{
	GENERATED_BODY()

public:
	ALeoBuddy();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	void SetTalking(float Seconds) { TalkTime = Seconds; }
	/** Call right after spawning to make it Fluffernutter instead of Leo. Offset: where it trails, sideways. */
	void Init(ECreature InKind, float InSide = 0.f);
	ECreature GetKind() const { return Kind; }

private:
	UPROPERTY()
	UCreatureRig* Rig;
	ECreature Kind = ECreature::Leo;
	bool bBuilt = false;
	float Side = 0.f;
	float TalkTime = 0.f;
	float StuckTime = 0.f;
	float TeleportCooldown = 0.f;
	float IdleTime = 0.f;
	int32 NextQuip = 0;
};

enum class EMove : uint8
{
	Linear,  // ping-pong between start and start+Offset
	Spin,    // turns on the spot
	Bob,     // gently up and down
	Sinker,  // sinks while stood on, rises when left
	Float    // drifts as gravity fails (chapter 4 crates)
};

/** A moving platform. */
UCLASS()
class SPARKYLEO_API AMover : public AActor
{
	GENERATED_BODY()

public:
	AMover();
	void Init(const FVector& Half, const TCHAR* Tex, const FLinearColor& Color, EMove InMove, const FVector& InOffset,
	          float InPeriod, float InPhase = 0.f, bool bGlowEdge = false);
	virtual void Tick(float Dt) override;

	/** 0..1: how far EMove::Float platforms have drifted up (chapter 4's failing gravity). */
	static float FloatAmount;

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UProceduralMeshComponent* Edge;
	EMove Move = EMove::Linear;
	FVector Start;
	FVector Offset;
	float Period = 4.f;
	float Time = 0.f;
	float Sink = 0.f;
	FRotator StartRot;
};

/** A wall Sparky can stick to (velcro, chapter 1). */
UCLASS()
class SPARKYLEO_API AVelcroWall : public AActor
{
	GENERATED_BODY()

public:
	AVelcroWall();
	void Init(const FVector& Half);

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
};

enum class EHazard : uint8
{
	Laser,      // pirate laser strike: warning ring, then the beam
	Sparks,     // live wire arcing between two points, on and off
	Steam,      // vent that throws Sparky upward while blowing
	Shockwave,  // Meatball's ground slam ring: jump it
	Rift,       // a temporal rift: a crackling orb drifting back and forth along Offset
	Gravity,    // a gravity distortion: inside the sphere (radius Offset.X) everything floats
	Lift        // a beam of light that carries Sparky up (always on; Offset.Z is its height)
};

UCLASS()
class SPARKYLEO_API AHazard : public AActor
{
	GENERATED_BODY()

public:
	AHazard();
	/** Sparks: Offset is the other end of the arc. Steam: Offset.Z is the column height. */
	void Init(EHazard InType, const FVector& InOffset = FVector::ZeroVector, float InPeriod = 3.f, float InPhase = 0.f);
	virtual void Tick(float Dt) override;
	bool IsOn() const { return bOn; }

private:
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMid;
	EHazard Type = EHazard::Laser;
	FVector Offset;
	float Period = 3.f;
	float Time = 0.f;
	bool bOn = false;
	bool bDone = false;
	void RebuildSparks();
};

/** Touch to set the respawn point. */
UCLASS()
class SPARKYLEO_API ACheckpoint : public AActor
{
	GENERATED_BODY()

public:
	ACheckpoint();
	void Init(const TCHAR* InTex);
	virtual void Tick(float Dt) override;

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMid;
	bool bLit = false;
	float Time = 0.f;
};

/** A glowing pad that ends a chapter step when Sparky reaches it while it's lit. */
UCLASS()
class SPARKYLEO_API AGoal : public AActor
{
	GENERATED_BODY()

public:
	AGoal();
	void Init(FName InId, float InRadius, bool bInActive);
	void SetActive(bool bOn);
	bool IsActive() const { return bActive; }
	FName GetId() const { return Id; }
	virtual void Tick(float Dt) override;

private:
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMid;
	FName Id;
	float Radius = 200.f;
	bool bActive = false;
	float Time = 0.f;
};

/** A broken conduit, welded shut by holding the attack button next to it (chapter 4). */
UCLASS()
class SPARKYLEO_API ARepairPoint : public AActor
{
	GENERATED_BODY()

public:
	ARepairPoint();
	void Init(FName InId, bool bBig);
	virtual void Tick(float Dt) override;
	bool IsFixed() const { return bFixed; }
	float GetProgress() const { return Progress; }
	FName GetId() const { return Id; }
	/** The unfixed repair point in reach of Sparky, if any. */
	static ARepairPoint* FindNear(const ASparkyCharacter* P);
	void SetEnabled(bool bOn) { bEnabled = bOn; }
	bool IsEnabled() const { return bEnabled; }

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMid;
	FName Id;
	bool bBig = false;
	bool bFixed = false;
	bool bEnabled = true;
	float Progress = 0.f;
	float Time = 0.f;
	float SparkTimer = 0.f;
};

/** A pirate boarding pod clamped to the hull. Ground-pound the clamp to knock it off (chapter 3). */
UCLASS()
class SPARKYLEO_API APod : public AActor
{
	GENERATED_BODY()

public:
	APod();
	void Init(const FVector& OutDir);
	virtual void Tick(float Dt) override;
	/** Sparky slammed down at Where: detach if it's on the clamp. */
	bool TrySlam(const FVector& Where);
	bool IsAttached() const { return !bLoose; }

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	FVector Out;
	FVector Vel;
	bool bLoose = false;
	float Time = 0.f;
};

/** The pirate ship cruising alongside in chapter 3, with Leo's shots hitting it. */
UCLASS()
class SPARKYLEO_API APirateShip : public AActor
{
	GENERATED_BODY()

public:
	APirateShip();
	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	void FleeToHyperspace() { bGone = true; }

	/** Where Leo's gun turret is (set by the chapter): his tracers start here. */
	static FVector TurretPos;

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	FVector Home;
	float Time = 0.f;
	float ShotTimer = 2.f;
	bool bGone = false;
};

/** A wormhole: step in and come out at the other one with the same Id. */
UCLASS()
class SPARKYLEO_API ATeleporter : public AActor
{
	GENERATED_BODY()

public:
	ATeleporter();
	void Init(FName InId, const FLinearColor& InColor);
	virtual void Tick(float Dt) override;
	FName GetId() const { return Id; }

private:
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	FName Id;
	float Time = 0.f;
	static float Cooldown;
};

/**
 * A floor tile in the scientist's quantum puzzle (chapter 8): landing on one flips it and its
 * neighbours. Light them all.
 */
UCLASS()
class SPARKYLEO_API AFloorPad : public AActor
{
	GENERATED_BODY()

public:
	AFloorPad();
	void Init(int32 InIndex, bool bInLit);
	virtual void Tick(float Dt) override;
	int32 GetIndex() const { return Index; }
	bool IsLit() const { return bLit; }
	void SetLit(bool bOn);
	void Lock() { bLocked = true; }

private:
	UPROPERTY()
	UProceduralMeshComponent* Mesh;
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	UPROPERTY()
	UMaterialInstanceDynamic* GlowMid;
	int32 Index = 0;
	bool bLit = false;
	bool bOn = false;      // Sparky standing on it
	bool bLocked = false;
};

enum class EFx : uint8
{
	Poof, Burst, Sparks, Tracer, Splash, Stars
};

/** Short-lived glow effects. */
UCLASS()
class SPARKYLEO_API AFx : public AActor
{
	GENERATED_BODY()

public:
	AFx();
	static AFx* Spawn(UWorld* W, EFx Type, const FVector& At, const FLinearColor& Color, float Size = 1.f, const FVector& To = FVector::ZeroVector);
	virtual void Tick(float Dt) override;

private:
	UPROPERTY()
	UProceduralMeshComponent* GlowMesh;
	UPROPERTY()
	UMaterialInstanceDynamic* Mid;
	EFx Type = EFx::Poof;
	float Life = 0.5f;
	float Age = 0.f;
	float Size = 1.f;
};

/** Mesh helpers shared by the props. */
namespace PropMesh
{
	UProceduralMeshComponent* Make(AActor* Owner, const TCHAR* Name, USceneComponent* Parent);
	void Commit(AActor* Owner, UProceduralMeshComponent* C, const FRetroMesh& M, const TCHAR* Tex = TEXT("T_White"), bool bCollide = false);
	UMaterialInstanceDynamic* CommitGlow(AActor* Owner, UProceduralMeshComponent* C, FRetroMesh& M, const FLinearColor& Color = FLinearColor::White, float Intensity = 1.f);
}
