#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "N64Pad.h"
#include "SparkyCharacter.generated.h"

class UCameraComponent;
class UCreatureRig;
class ANpc;
class ASparkyGameMode;

/** One frame of input from whatever is plugged in. Bools without "Held" are presses. */
struct FControls
{
	FVector2D Move = FVector2D::ZeroVector;  // +X right, +Y forward (relative to the camera)
	FVector2D Cam = FVector2D::ZeroVector;   // continuous orbit: +X right, +Y up (deg/s scale)
	FVector2D Mouse = FVector2D::ZeroVector; // raw mouse delta
	float CamSnap = 0.f;                     // -1 / +1: a 45 degree step (C-Left / C-Right)
	int32 Zoom = 0;                          // -1 closer, +1 further
	bool bJump = false, bJumpHeld = false;
	bool bAttack = false, bAttackHeld = false;
	bool bCrouch = false, bCrouchHeld = false;
	bool bRecenter = false;
	bool bStart = false;
	bool bConfirm = false, bBack = false;
	int32 MenuDir = 0;                       // 1 up, 2 down, 3 left, 4 right
	bool bKeyEscape = false;
};

/**
 * Sparky. Third-person, N64 style: the stick moves relative to the camera, A jumps (again in the
 * air for a somersault, with Z held for a backflip), B swipes (in the air: a pounce), Z in the
 * air ground-pounds. Sticks to velcro walls and jumps off them. Holds B at a broken conduit to
 * weld it once he has the welder.
 */
UCLASS()
class SPARKYLEO_API ASparkyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASparkyCharacter();

	static ASparkyCharacter* Get(const UObject* Ctx);

	virtual void BeginPlay() override;
	virtual void Tick(float Dt) override;
	virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
	                       FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit) override;

	// ------------------------------------------------ gameplay
	static constexpr int32 MaxHealth = 5;
	int32 GetHealth() const { return Health; }
	void Hurt(int32 Amount, const FVector& From);
	void Heal(int32 Amount);
	bool IsInvulnerable() const { return Invuln > 0.f || bGod; }
	bool IsDown() const { return bDown; }
	bool IsWelding() const { return bWelding; }
	bool IsSlamming() const { return SlamState > 0; }
	bool IsClinging() const { return bCling; }
	/** Bounce up (stomping an enemy, a steam vent). */
	void Bounce(float VZ, bool bResetJumps = true);
	void Knockback(const FVector& From, float Strength);

	void SetCheckpoint(const FVector& Loc, float Yaw);
	/** Back at the checkpoint (full health if he was down). */
	void Respawn();
	/** Fell in the bog / off into space: lose a paw and go back to the checkpoint. */
	void FellOut();
	/** Put Sparky somewhere (chapter start), camera behind him. */
	void PlaceAt(const FVector& Loc, float Yaw);
	void ResetForChapter();

	void SetHat(bool bOn);
	bool HasHat() const { return bHat; }
	void SetWelder(bool bOn) { bWelder = bOn; }
	bool HasWelder() const { return bWelder; }

	/** Gravity for this chapter (and, in chapter 4, how badly it's failing). */
	void SetGravity(float Scale);
	/** Inside a gravity distortion: gravity x Scale, for this frame (call every frame). */
	void SetZoneGravity(float Scale);
	/** Chapter 6: play as transformed Fluffernutter (true) or Sparky (false). */
	void SetForm(bool bFluffernutter);
	bool IsFluffernutter() const { return bBig; }
	/** Crouch-walking (slow and quiet). */
	bool IsSneaking() const { return bSneaking; }
	/** Hide the player's model (a cutscene shows someone else in his place). */
	void HideModel(bool bHide) { bModelHidden = bHide; }
	/** 0 normal floor .. 1 ice rink. */
	void SetIce(float Amount);

	/** Cutscenes: no control, camera on a fixed shot (or behind Sparky if Pos is zero). */
	void SetCinematic(bool bOn, const FVector& Pos = FVector::ZeroVector, const FVector& LookAt = FVector::ZeroVector);
	/** Title screen: hide Sparky and orbit the camera round a point. */
	void SetAttract(bool bOn, const FVector& Center = FVector::ZeroVector, float Radius = 2500.f);
	void Shake(float Amount) { ShakeAmount = FMath::Max(ShakeAmount, Amount); }

	/** The talkable NPC in reach, for the "B: TALK" prompt. */
	ANpc* GetInteractable() const { return Interactable.Get(); }
	float GetCamYaw() const { return CamYaw; }

	// ------------------------------------------------ controller
	FN64Pad& GetPad() { return Pad; }
	bool HasN64Pad() const { return Pad.IsOpen(); }
	const FPadCalibrator& GetCalibrator() const { return Calibrator; }
	void StartCalibration() { Calibrator.Start(); }

private:
	UPROPERTY()
	UCameraComponent* Camera;
	UPROPERTY()
	UCreatureRig* Rig;

	// input
	FN64Pad Pad;
	FPadCalibrator Calibrator;
	float PadRetry = 0.f;
	bool bPadWasOpen = false;
	float InForward = 0.f, InRight = 0.f, InTurn = 0.f, InLook = 0.f;
	float InPadForward = 0.f, InPadRight = 0.f, InPadTurn = 0.f, InPadLook = 0.f;
	int32 LastMenuDir = 0;
	float MenuRepeat = 0.f;
	bool bMouseCaptured = false;
	bool bMouseModeSet = false;
	bool bFreeMouse = false;
	bool bGod = false;
	float AutoShotInterval = 0.f;
	float NextAutoShot = 0.f;
	float AutoPlayTime = 0.f;

	// camera
	float CamYaw = 0.f;
	float CamYawTarget = 0.f;
	float CamPitch = -24.f;
	int32 ZoomLevel = 1;
	float CamDist = 800.f;
	float CamPitchUsed = -24.f;  // after tilting up over obstacles
	float CamDistUsed = 800.f;   // after pulling in for obstacles
	float CamIdle = 0.f;   // seconds since the player last turned the camera
	FVector CamPos = FVector::ZeroVector;
	FVector CamFocus = FVector::ZeroVector;
	bool bCinematic = false;
	FVector CinePos, CineLook;
	bool bAttract = false;
	FVector AttractCenter;
	float AttractRadius = 2500.f;
	float AttractAngle = 0.f;
	float ShakeAmount = 0.f;

	// moves
	int32 Health = MaxHealth;
	float Invuln = 0.f;
	bool bDown = false;
	float DownTime = 0.f;
	float AttackTime = 0.f;    // >0 while a swipe / pounce is live
	bool bPounce = false;
	bool bPounceUsed = false;
	bool bDoubleUsed = false;
	bool bBig = false;          // playing as transformed Fluffernutter
	bool bSneaking = false;
	bool bModelHidden = false;
	float ZoneGravity = 1.f;
	float ZoneTime = 0.f;
	int32 SlamState = 0;       // 0 none, 1 hanging, 2 falling
	float SlamTime = 0.f;
	float FlipTime = 0.f;
	bool bCling = false;
	FVector ClingNormal;
	float ClingCooldown = 0.f;
	bool bWelding = false;
	bool bHat = false;
	bool bWelder = false;
	float BaseGravity = 2.4f;
	float StepTime = 0.f;
	float CrouchAmount = 0.f;
	float TalkPrompt = 0.f;
	TArray<TWeakObjectPtr<AActor>> HitThisSwing;
	TWeakObjectPtr<ANpc> Interactable;
	FVector Checkpoint = FVector(0, 0, 200);
	float CheckpointYaw = 0.f;
	float FallStartZ = 0.f;

	void OpenPad();
	FControls ReadControls(float Dt);
	void UpdateMouse(const ASparkyGameMode* Mode);
	void HandleMoves(const FControls& C, float Dt);
	void UpdateAttack(float Dt);
	void UpdateStomp();
	void UpdateCamera(const FControls& C, float Dt);
	void UpdateRig(float Dt);
	void StartSlam();
	void FinishSlam();
	void EndCling(bool bJumpOff);
	void DevTick(float Dt);
	void AutoPlay(FControls& C, float Dt);
};
