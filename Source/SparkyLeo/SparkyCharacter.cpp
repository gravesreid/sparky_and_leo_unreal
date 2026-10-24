#include "SparkyCharacter.h"
#include "Enemy.h"
#include "LevelGeo.h"
#include "Props.h"
#include "CreatureRig.h"
#include "SparkyGameMode.h"
#include "SparkyLeo.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	constexpr float HalfHeight = 44.f;
	constexpr float SwipeTime = 0.3f;
	constexpr float PounceTime = 0.45f;
	constexpr float FlipDuration = 0.5f;
	const float Zooms[] = {560.f, 820.f, 1200.f};
}

ASparkyCharacter::ASparkyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;  // menus and the camera still run while paused
	GetCapsuleComponent()->InitCapsuleSize(36.f, HalfHeight);
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* M = GetCharacterMovement();
	M->MaxWalkSpeed = 720.f;
	M->MaxAcceleration = 2600.f;
	M->BrakingDecelerationWalking = 3000.f;
	M->GroundFriction = 8.f;
	M->JumpZVelocity = 760.f;
	M->AirControl = 0.8f;
	M->GravityScale = 2.4f;
	M->bOrientRotationToMovement = true;
	M->RotationRate = FRotator(0, 1100.f, 0);
	M->MaxStepHeight = 30.f;
	M->SetWalkableFloorAngle(50.f);
	M->bCanWalkOffLedges = true;
	JumpMaxHoldTime = 0.14f;
	JumpMaxCount = 1;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(RootComponent);
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->SetFieldOfView(65.f);

	Rig = CreateDefaultSubobject<UCreatureRig>(TEXT("Rig"));
	Rig->SetupAttachment(RootComponent);
}

ASparkyCharacter* ASparkyCharacter::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	APlayerController* PC = W ? W->GetFirstPlayerController() : nullptr;
	return PC ? Cast<ASparkyCharacter>(PC->GetPawn()) : nullptr;
}

void ASparkyCharacter::BeginPlay()
{
	Super::BeginPlay();
	Rig->Build(ECreature::Sparky);
	Rig->SetBaseZ(-HalfHeight);
	OpenPad();
	bPadWasOpen = Pad.IsOpen();
	bGod = FParse::Param(FCommandLine::Get(), TEXT("God"));
	bFreeMouse = FParse::Param(FCommandLine::Get(), TEXT("FreeMouse"));
	FParse::Value(FCommandLine::Get(), TEXT("AutoShot="), AutoShotInterval);
	if (FParse::Param(FCommandLine::Get(), TEXT("AutoPlay")))
	{
		AutoPlayTime = 0.001f;
	}
	CamPos = GetActorLocation();
	CamFocus = GetActorLocation();
}

void ASparkyCharacter::OpenPad()
{
	FString Spec = TEXT("auto");
	FParse::Value(FCommandLine::Get(), TEXT("N64Pad="), Spec);
	Pad.Open(Spec);  // loads the controller's saved calibration, if it has one
	FString Map;
	if (Pad.IsOpen() && FParse::Value(FCommandLine::Get(), TEXT("N64Map="), Map))
	{
		Pad.SetMap(Map);
	}
}

void ASparkyCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
	Super::SetupPlayerInputComponent(Input);
	// read back with GetInputAxisValue; buttons are polled in ReadControls
	for (const TCHAR* Axis : {TEXT("MoveForward"), TEXT("MoveRight"), TEXT("Turn"), TEXT("LookUp"), TEXT("PadForward"), TEXT("PadRight"), TEXT("PadTurn"), TEXT("PadLook")})
	{
		Input->BindAxis(Axis);
	}
}

// ---------------------------------------------------------------- input

FControls ASparkyCharacter::ReadControls(float Dt)
{
	FControls C;
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return C;
	}
	auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
	auto Hit = [PC](const FKey& K) { return PC->WasInputKeyJustPressed(K); };
	auto Dead = [](float V) { return FMath::Abs(V) < 0.2f ? 0.f : V; };

	C.Move = FVector2D(GetInputAxisValue("MoveRight"), GetInputAxisValue("MoveForward"));
	C.Move += FVector2D(Dead(GetInputAxisValue("PadRight")), Dead(GetInputAxisValue("PadForward")));
	C.Cam.X = (Down(EKeys::E) ? 1.f : 0.f) - (Down(EKeys::Q) ? 1.f : 0.f) + Dead(GetInputAxisValue("PadTurn"));
	C.Cam.Y = Dead(GetInputAxisValue("PadLook"));
	C.Mouse = FVector2D(GetInputAxisValue("Turn"), GetInputAxisValue("LookUp"));
	// arrow keys: move (keys + mouse) or steer the camera (keyboard only)
	const ASparkyGameMode* Mode = ASparkyGameMode::Get(this);
	const FVector2D Arrows((Down(EKeys::Right) ? 1.f : 0.f) - (Down(EKeys::Left) ? 1.f : 0.f), (Down(EKeys::Up) ? 1.f : 0.f) - (Down(EKeys::Down) ? 1.f : 0.f));
	if (Mode && Mode->IsKeyboardOnly())
	{
		C.Cam += Arrows;
		C.Mouse = FVector2D::ZeroVector;
	}
	else
	{
		C.Move += Arrows;
	}

	C.bJump = Hit(EKeys::SpaceBar) || Hit(EKeys::Gamepad_FaceButton_Bottom);
	C.bJumpHeld = Down(EKeys::SpaceBar) || Down(EKeys::Gamepad_FaceButton_Bottom);
	C.bAttack = Hit(EKeys::J) || Hit(EKeys::F) || Hit(EKeys::LeftMouseButton) || Hit(EKeys::Gamepad_FaceButton_Left);
	C.bAttackHeld = Down(EKeys::J) || Down(EKeys::F) || Down(EKeys::LeftMouseButton) || Down(EKeys::Gamepad_FaceButton_Left);
	C.bCrouch = Hit(EKeys::LeftShift) || Hit(EKeys::K) || Hit(EKeys::RightMouseButton) || Hit(EKeys::Gamepad_LeftTrigger) || Hit(EKeys::Gamepad_RightTrigger);
	C.bCrouchHeld = Down(EKeys::LeftShift) || Down(EKeys::K) || Down(EKeys::RightMouseButton) || Down(EKeys::Gamepad_LeftTrigger) || Down(EKeys::Gamepad_RightTrigger);
	C.bRecenter = Hit(EKeys::Tab) || Hit(EKeys::L) || Hit(EKeys::Gamepad_RightShoulder) || Hit(EKeys::Gamepad_LeftShoulder);
	C.Zoom = Hit(EKeys::MouseScrollUp) || Hit(EKeys::I) || Hit(EKeys::Hyphen) ? -1 : Hit(EKeys::MouseScrollDown) || Hit(EKeys::O) || Hit(EKeys::Equals) ? 1 : 0;
	C.bStart = Hit(EKeys::Enter) || Hit(EKeys::Escape) || Hit(EKeys::P) || Hit(EKeys::Gamepad_Special_Right);
	C.bConfirm = Hit(EKeys::Enter) || Hit(EKeys::SpaceBar) || Hit(EKeys::J) || Hit(EKeys::Gamepad_FaceButton_Bottom) || Hit(EKeys::LeftMouseButton);
	C.bBack = Hit(EKeys::Escape) || Hit(EKeys::BackSpace) || Hit(EKeys::Gamepad_FaceButton_Right);
	C.bKeyEscape = Hit(EKeys::Escape);

	int32 Dir = 0;
	if (Down(EKeys::Up) || Down(EKeys::W) || Down(EKeys::Gamepad_DPad_Up) || GetInputAxisValue("PadForward") > 0.6f) Dir = 1;
	else if (Down(EKeys::Down) || Down(EKeys::S) || Down(EKeys::Gamepad_DPad_Down) || GetInputAxisValue("PadForward") < -0.6f) Dir = 2;
	else if (Down(EKeys::Left) || Down(EKeys::A) || Down(EKeys::Gamepad_DPad_Left) || GetInputAxisValue("PadRight") < -0.6f) Dir = 3;
	else if (Down(EKeys::Right) || Down(EKeys::D) || Down(EKeys::Gamepad_DPad_Right) || GetInputAxisValue("PadRight") > 0.6f) Dir = 4;

	if (Pad.IsOpen())
	{
		const FVector2D Stick = Pad.GetStick();
		const FVector2D DPad((Pad.IsDown(EN64::DRight) ? 1.f : 0.f) - (Pad.IsDown(EN64::DLeft) ? 1.f : 0.f),
		                     (Pad.IsDown(EN64::DUp) ? 1.f : 0.f) - (Pad.IsDown(EN64::DDown) ? 1.f : 0.f));
		C.Move += Stick + DPad;
		const bool bA = Pad.WasPressed(EN64::A), bB = Pad.WasPressed(EN64::B), bZ = Pad.WasPressed(EN64::Z);
		const bool bStart = Pad.WasPressed(EN64::Start), bR = Pad.WasPressed(EN64::R), bL = Pad.WasPressed(EN64::L);
		C.bJump |= bA;
		C.bJumpHeld |= Pad.IsDown(EN64::A);
		C.bAttack |= bB;
		C.bAttackHeld |= Pad.IsDown(EN64::B);
		C.bCrouch |= bZ;
		C.bCrouchHeld |= Pad.IsDown(EN64::Z);
		C.bRecenter |= bR || bL;
		C.bStart |= bStart;
		C.bConfirm |= bA || bStart;
		C.bBack |= bB;
		if (Pad.WasPressed(EN64::CLeft)) C.CamSnap = -1.f;
		if (Pad.WasPressed(EN64::CRight)) C.CamSnap = 1.f;
		if (Pad.WasPressed(EN64::CUp)) C.Zoom = -1;
		if (Pad.WasPressed(EN64::CDown)) C.Zoom = 1;
		if (Dir == 0)
		{
			if (Stick.Y > 0.6f || Pad.IsDown(EN64::DUp)) Dir = 1;
			else if (Stick.Y < -0.6f || Pad.IsDown(EN64::DDown)) Dir = 2;
			else if (Stick.X < -0.6f || Pad.IsDown(EN64::DLeft)) Dir = 3;
			else if (Stick.X > 0.6f || Pad.IsDown(EN64::DRight)) Dir = 4;
		}
	}
	if (C.Move.SizeSquared() > 1.f)
	{
		C.Move.Normalize();
	}

	// menu directions repeat while held
	if (Dir != LastMenuDir)
	{
		C.MenuDir = Dir;
		MenuRepeat = 0.4f;
	}
	else if (Dir != 0)
	{
		MenuRepeat -= Dt;
		if (MenuRepeat <= 0.f)
		{
			C.MenuDir = Dir;
			MenuRepeat = 0.12f;
		}
	}
	LastMenuDir = Dir;
	return C;
}

void ASparkyCharacter::UpdateMouse(const ASparkyGameMode* Mode)
{
	// hold the mouse only while it's steering the camera: in play, with no N64 pad
	const bool bWant = !bFreeMouse && Mode && Mode->GetState() == EGameState::Playing && !Pad.IsOpen() && !Mode->IsKeyboardOnly();
	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC || (bMouseCaptured == bWant && bMouseModeSet))
	{
		return;
	}
	bMouseModeSet = true;
	bMouseCaptured = bWant;
	if (bWant)
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
	else
	{
		FInputModeGameAndUI M;
		M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		M.SetHideCursorDuringCapture(false);
		PC->SetInputMode(M);
		PC->bShowMouseCursor = true;
	}
}

// ---------------------------------------------------------------- tick

void ASparkyCharacter::Tick(float Dt)
{
	Super::Tick(Dt);
	const float RealDt = FMath::Min(float(FApp::GetDeltaTime()), 0.1f);

	if (!Pad.IsOpen())
	{
		PadRetry -= RealDt;
		if (PadRetry <= 0.f)
		{
			PadRetry = 2.f;
			OpenPad();
		}
	}
	Pad.Poll();
	ASparkyGameMode* Mode = ASparkyGameMode::Get(this);
	if (Mode && Pad.IsOpen() != bPadWasOpen)
	{
		Mode->Message(Pad.IsOpen() ? TEXT("N64 CONTROLLER CONNECTED") : TEXT("N64 CONTROLLER UNPLUGGED"), FLinearColor(0.6f, 0.9f, 1.f));
	}
	bPadWasOpen = Pad.IsOpen();

	// gravity distortions (chapter 7) only hold while Sparky's inside them
	ZoneTime = FMath::Max(0.f, ZoneTime - RealDt);
	if (!bCling)
	{
		GetCharacterMovement()->GravityScale = BaseGravity * (ZoneTime > 0.f ? ZoneGravity : 1.f);
	}
	FControls C = ReadControls(RealDt);
	if (AutoPlayTime > 0.f)
	{
		AutoPlay(C, RealDt);
	}
	if (Mode && Mode->GetState() == EGameState::Calibrate)
	{
		if (C.bKeyEscape)
		{
			Calibrator.Stop();
			Mode->EndCalibration(false, FString());
		}
		else if (Calibrator.Tick(RealDt, Pad))
		{
			Mode->EndCalibration(true, Pad.GetName());
		}
		else if (!Calibrator.IsActive())
		{
			Mode->EndCalibration(false, FString());
		}
		C = FControls();
	}
	else if (Mode && Mode->GetState() == EGameState::Playing && !bCinematic)
	{
		if (C.bStart && !bDown)
		{
			Mode->TogglePause();
		}
		else
		{
			HandleMoves(C, Dt);
		}
	}
	else
	{
		bWelding = false;
		if (Mode)
		{
			Mode->MenuInput(C);
		}
		C.Cam = FVector2D::ZeroVector;
		C.Mouse = FVector2D::ZeroVector;
		C.CamSnap = 0.f;
	}
	if (Mode && Mode->GetState() == EGameState::Playing)
	{
		DownTime = bDown ? DownTime + Dt : 0.f;
	}
	UpdateCamera(C, RealDt);
	UpdateRig(Dt);
	UpdateMouse(Mode);
	DevTick(RealDt);
}

void ASparkyCharacter::HandleMoves(const FControls& C, float Dt)
{
	UCharacterMovementComponent* M = GetCharacterMovement();
	ASparkyGameMode* Mode = ASparkyGameMode::Get(this);
	if (bDown)
	{
		return;
	}
	Invuln = FMath::Max(0.f, Invuln - Dt);
	AttackTime = FMath::Max(0.f, AttackTime - Dt);
	FlipTime = FMath::Max(0.f, FlipTime - Dt);
	ClingCooldown = FMath::Max(0.f, ClingCooldown - Dt);
	if (AttackTime <= 0.f)
	{
		bPounce = false;
	}
	const bool bAir = M->IsFalling();

	// someone to talk to?
	Interactable = nullptr;
	float Best = 240.f;
	for (TActorIterator<ANpc> It(GetWorld()); It; ++It)
	{
		if (It->CanTalk())
		{
			const float D = FVector::Dist2D(It->GetActorLocation(), GetActorLocation());
			if (D < Best && FMath::Abs(It->GetActorLocation().Z - GetActorLocation().Z) < 200.f)
			{
				Best = D;
				Interactable = *It;
			}
		}
	}

	const FRotator YawRot(0, CamYaw, 0);
	const FVector Fwd = YawRot.Vector();
	const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
	const FVector Wish = Fwd * C.Move.Y + Right * C.Move.X;

	// welding: hold B at a broken conduit
	ARepairPoint* Rep = bWelder && !bAir ? ARepairPoint::FindNear(this) : nullptr;
	bWelding = Rep && C.bAttackHeld;
	if (bWelding)
	{
		const FVector To = Rep->GetActorLocation() - GetActorLocation();
		SetActorRotation(FRotator(0, FMath::FixedTurn(GetActorRotation().Yaw, To.Rotation().Yaw, 720.f * Dt), 0));
		UpdateStomp();
		return;
	}

	// stuck to velcro
	if (bCling)
	{
		M->Velocity = FVector::ZeroVector;
		if (C.bJump)
		{
			EndCling(true);
		}
		else if (C.bCrouch)
		{
			EndCling(false);
		}
		return;
	}

	// ground pound: hang a moment, then drop like a stone
	if (SlamState == 1)
	{
		SlamTime += Dt;
		M->Velocity = FVector::ZeroVector;
		if (SlamTime > 0.2f)
		{
			SlamState = 2;
			M->Velocity = FVector(0, 0, -2600.f);
		}
		return;
	}
	if (SlamState == 2)
	{
		M->Velocity = FVector(0, 0, FMath::Min(M->Velocity.Z, -2600.f));
		UpdateStomp();
		if (!bAir)
		{
			FinishSlam();
		}
		return;
	}

	if (!bPounce)
	{
		AddMovementInput(Wish, 1.f);
	}

	// jumps
	if (C.bJump)
	{
		if (!bAir)
		{
			if (C.bCrouchHeld && Wish.SizeSquared() < 0.1f)
			{
				// backflip: straight up, very high
				LaunchCharacter(-GetActorForwardVector() * 250.f + FVector(0, 0, 1250.f), true, true);
				FlipTime = FlipDuration;
				ASparkyGameMode::Sfx(this, ESfx::DoubleJump, GetActorLocation(), 0.8f);
			}
			else if (C.bCrouchHeld)
			{
				// long jump
				const FVector Dir = Wish.GetSafeNormal2D();
				SetActorRotation(Dir.Rotation());
				LaunchCharacter(Dir * 1350.f + FVector(0, 0, 650.f), true, true);
				ASparkyGameMode::Sfx(this, ESfx::Pounce, GetActorLocation(), 0.8f);
			}
			else
			{
				Jump();
				ASparkyGameMode::Sfx(this, ESfx::Jump, GetActorLocation(), 0.7f);
			}
			bDoubleUsed = false;
			bPounceUsed = false;
		}
		else if (!bDoubleUsed)
		{
			// somersault
			bDoubleUsed = true;
			M->Velocity.Z = 880.f;
			if (Wish.SizeSquared() > 0.1f)
			{
				const FVector H = Wish.GetSafeNormal2D() * FMath::Max(M->Velocity.Size2D(), 450.f);
				M->Velocity.X = H.X;
				M->Velocity.Y = H.Y;
			}
			FlipTime = FlipDuration;
			ASparkyGameMode::Sfx(this, ESfx::DoubleJump, GetActorLocation(), 0.8f);
		}
	}
	if (!C.bJumpHeld)
	{
		StopJumping();
	}

	// B: talk, swipe, or pounce
	if (C.bAttack && AttackTime <= 0.f)
	{
		if (Interactable.IsValid() && !bAir && Mode)
		{
			Mode->OnTalk(Interactable.Get());
		}
		else if (bAir && !bPounceUsed)
		{
			bPounce = true;
			bPounceUsed = true;
			AttackTime = PounceTime;
			HitThisSwing.Reset();
			const FVector Dir = Wish.SizeSquared() > 0.1f ? Wish.GetSafeNormal2D() : GetActorForwardVector();
			SetActorRotation(Dir.Rotation());
			LaunchCharacter(Dir * 1150.f + FVector(0, 0, 280.f), true, true);
			ASparkyGameMode::Sfx(this, ESfx::Pounce, GetActorLocation());
		}
		else if (!bAir)
		{
			bPounce = false;
			AttackTime = SwipeTime;
			HitThisSwing.Reset();
			LaunchCharacter(GetActorForwardVector() * 380.f, true, false);
			ASparkyGameMode::Sfx(this, ESfx::Swipe, GetActorLocation());
		}
	}
	if (C.bCrouch && bAir && !bCling)
	{
		StartSlam();
	}
	CrouchAmount = FMath::FInterpTo(CrouchAmount, C.bCrouchHeld && !bAir ? 1.f : 0.f, Dt, 14.f);
	// crouch-walking: slow and quiet (sneaking up on Fluffernutter)
	bSneaking = C.bCrouchHeld && !bAir;
	M->MaxWalkSpeed = bSneaking ? 230.f : (bBig ? 640.f : 720.f);

	UpdateAttack(Dt);
	UpdateStomp();

	// footsteps
	if (!bAir && M->Velocity.Size2D() > 150.f)
	{
		StepTime -= Dt * M->Velocity.Size2D() / 450.f;
		if (StepTime <= 0.f)
		{
			StepTime = 0.3f;
			ASparkyGameMode::Sfx(this, ESfx::Footstep, GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 0.25f);
		}
	}

	// off the edge of the world
	ALevelGeo* Geo = ALevelGeo::Get(this);
	if (Geo && GetActorLocation().Z < Geo->Theme.KillZ && Mode)
	{
		Mode->OnFell();
	}
}

void ASparkyCharacter::UpdateAttack(float Dt)
{
	if (AttackTime <= 0.f)
	{
		return;
	}
	const FVector Center = GetActorLocation() + GetActorForwardVector() * (bPounce ? 30.f : 60.f);
	const float Radius = (bPounce ? 95.f : 105.f) * (bBig ? 1.7f : 1.f);
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		AEnemy* E = *It;
		if (E->IsDead() || HitThisSwing.Contains(E))
		{
			continue;
		}
		const FVector EC = E->GetActorLocation();
		if (FVector::Dist2D(EC, Center) < Radius + E->GetRadius() && FMath::Abs(EC.Z - Center.Z) < 60.f + E->GetTop() - EC.Z)
		{
			HitThisSwing.Add(E);
			E->TakeHit(bBig ? 2 : 1, GetActorLocation(), false);
			Shake(3.f);
			if (bPounce)
			{
				// bounce back off whatever we pounced on
				LaunchCharacter(-GetActorForwardVector() * 350.f + FVector(0, 0, 650.f), true, true);
				AttackTime = 0.f;
				bPounce = false;
				bPounceUsed = false;
				bDoubleUsed = false;
			}
		}
	}
}

void ASparkyCharacter::UpdateStomp()
{
	UCharacterMovementComponent* M = GetCharacterMovement();
	if (M->Velocity.Z > -80.f)
	{
		return;
	}
	const FVector F = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		AEnemy* E = *It;
		if (E->IsDead())
		{
			continue;
		}
		const float Top = E->GetTop();
		if (FVector::Dist2D(F, E->GetActorLocation()) < E->GetRadius() + 28.f && F.Z > Top - 35.f && F.Z < Top + 50.f)
		{
			const bool bSlam = SlamState == 2;
			E->TakeHit(bSlam ? 3 : 1, GetActorLocation(), true);
			SlamState = 0;
			Bounce(bSlam ? 1100.f : 950.f);
			ASparkyGameMode::Sfx(this, ESfx::Boing, F, 0.8f);
			return;
		}
	}
}

void ASparkyCharacter::StartSlam()
{
	SlamState = 1;
	SlamTime = 0.f;
	FlipTime = 0.f;
	AttackTime = 0.f;
	bPounce = false;
	ASparkyGameMode::Sfx(this, ESfx::Swipe, GetActorLocation(), 0.6f);
}

void ASparkyCharacter::FinishSlam()
{
	SlamState = 0;
	const FVector F = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	ASparkyGameMode::Sfx(this, ESfx::GroundPound, F);
	AFx::Spawn(GetWorld(), EFx::Poof, F, FLinearColor(0.9f, 0.85f, 0.75f), 1.6f);
	Shake(9.f);
	for (TActorIterator<AEnemy> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && FVector::Dist2D(It->GetActorLocation(), F) < (bBig ? 400.f : 230.f) && FMath::Abs(It->GetActorLocation().Z - F.Z) < 150.f)
		{
			It->TakeHit(bBig ? 3 : 2, F, false);
		}
	}
	for (TActorIterator<APod> It(GetWorld()); It; ++It)
	{
		if (It->TrySlam(F))
		{
			break;
		}
	}
}

void ASparkyCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (SlamState == 2)
	{
		FinishSlam();
	}
	else if (SlamState == 0)
	{
		ASparkyGameMode::Sfx(this, ESfx::Land, GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), 0.5f);
	}
	FlipTime = 0.f;
	bDoubleUsed = false;
	bPounceUsed = false;
	if (bPounce)
	{
		bPounce = false;
		AttackTime = 0.f;
	}
}

void ASparkyCharacter::NotifyHit(UPrimitiveComponent* MyComp, AActor* Other, UPrimitiveComponent* OtherComp, bool bSelfMoved,
                                 FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	if (bCling || ClingCooldown > 0.f || bDown || !GetCharacterMovement()->IsFalling() || !Cast<AVelcroWall>(Other) || FMath::Abs(HitNormal.Z) > 0.35f)
	{
		return;
	}
	// stick!
	bCling = true;
	ClingNormal = FVector(HitNormal.X, HitNormal.Y, 0).GetSafeNormal();
	SlamState = 0;
	bPounce = false;
	AttackTime = 0.f;
	FlipTime = 0.f;
	bDoubleUsed = false;
	bPounceUsed = false;
	GetCharacterMovement()->GravityScale = 0.f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	SetActorRotation((-ClingNormal).Rotation());
	ASparkyGameMode::Sfx(this, ESfx::Velcro, GetActorLocation());
	UE_LOG(LogSparky, Verbose, TEXT("Cling at %s"), *GetActorLocation().ToCompactString());
	if (FParse::Param(FCommandLine::Get(), TEXT("LogPos")))
	{
		UE_LOG(LogSparky, Log, TEXT("Cling at %s"), *GetActorLocation().ToCompactString());
	}
}

void ASparkyCharacter::EndCling(bool bJumpOff)
{
	bCling = false;
	ClingCooldown = 0.2f;
	GetCharacterMovement()->GravityScale = BaseGravity;
	SetActorRotation(ClingNormal.Rotation());
	if (bJumpOff)
	{
		LaunchCharacter(ClingNormal * 620.f + FVector(0, 0, 1080.f), true, true);
		FlipTime = FlipDuration;
		ASparkyGameMode::Sfx(this, ESfx::Velcro, GetActorLocation(), 0.7f);
		ASparkyGameMode::Sfx(this, ESfx::Jump, GetActorLocation(), 0.6f);
	}
}

// ---------------------------------------------------------------- damage & respawn

void ASparkyCharacter::Hurt(int32 Amount, const FVector& From)
{
	ASparkyGameMode* Mode = ASparkyGameMode::Get(this);
	if (IsInvulnerable() || bDown || !Mode || !Mode->IsPlaying())
	{
		return;
	}
	Health = FMath::Max(0, Health - Amount);
	Invuln = 1.6f;
	if (bCling)
	{
		EndCling(false);
	}
	SlamState = 0;
	bWelding = false;
	Knockback(From, 650.f);
	Shake(6.f);
	Rig->SetFlash(0.8f);
	ASparkyGameMode::Sfx(this, ESfx::Hurt, GetActorLocation());
	if (Health <= 0)
	{
		bDown = true;
		DownTime = 0.f;
		ASparkyGameMode::Sfx(this, ESfx::Death);
		Mode->OnPlayerDown();
	}
}

void ASparkyCharacter::Heal(int32 Amount)
{
	Health = FMath::Min(MaxHealth, Health + Amount);
}

void ASparkyCharacter::Knockback(const FVector& From, float Strength)
{
	const FVector Dir = (GetActorLocation() - From).GetSafeNormal2D();
	LaunchCharacter((Dir.IsNearlyZero() ? -GetActorForwardVector() : Dir) * Strength + FVector(0, 0, 520.f), true, true);
}

void ASparkyCharacter::Bounce(float VZ, bool bResetJumps)
{
	UCharacterMovementComponent* M = GetCharacterMovement();
	if (SlamState == 1)
	{
		SlamState = 0;
	}
	if (M->Velocity.Z < VZ)
	{
		LaunchCharacter(FVector(0, 0, VZ), false, true);
	}
	if (bResetJumps)
	{
		bDoubleUsed = false;
		bPounceUsed = false;
	}
}

void ASparkyCharacter::SetCheckpoint(const FVector& Loc, float Yaw)
{
	Checkpoint = Loc;
	CheckpointYaw = Yaw;
}

void ASparkyCharacter::Respawn()
{
	if (bDown)
	{
		Health = MaxHealth;
	}
	bDown = false;
	Invuln = 2.f;
	PlaceAt(Checkpoint, CheckpointYaw);
	Rig->SetFlash(0.f);
}

void ASparkyCharacter::FellOut()
{
	if (bDown)
	{
		return;
	}
	Health = FMath::Max(0, Health - 1);
	if (Health <= 0)
	{
		bDown = true;
		DownTime = 0.f;
		ASparkyGameMode::Sfx(this, ESfx::Death);
		if (ASparkyGameMode* Mode = ASparkyGameMode::Get(this))
		{
			Mode->OnPlayerDown();
		}
		return;
	}
	ASparkyGameMode::Sfx(this, ESfx::Hurt);
	Respawn();
}

void ASparkyCharacter::PlaceAt(const FVector& Loc, float Yaw)
{
	if (bCling)
	{
		EndCling(false);
	}
	SlamState = 0;
	AttackTime = 0.f;
	bPounce = false;
	FlipTime = 0.f;
	SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorRotation(FRotator(0, Yaw, 0));
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
	GetCharacterMovement()->GravityScale = BaseGravity;
	CamYaw = CamYawTarget = Yaw;
	CamPitch = -24.f;
	CamFocus = Loc + FVector(0, 0, 45);
	CamPos = CamFocus - FRotator(CamPitch, CamYaw, 0).Vector() * CamDist;
	CamPitchUsed = CamPitch;
	CamDistUsed = CamDist;
	if (Checkpoint.IsZero() || Checkpoint == FVector(0, 0, 200))
	{
		SetCheckpoint(Loc, Yaw);
	}
}

void ASparkyCharacter::ResetForChapter()
{
	SetForm(false);
	bModelHidden = false;
	Health = MaxHealth;
	Invuln = 0.f;
	bDown = false;
	bWelding = false;
	SetHat(false);
	bWelder = false;
	SetGravity(2.4f);
	SetIce(0.f);
	Checkpoint = FVector(0, 0, 200);
	Rig->SetFlash(0.f);
	bCinematic = false;
}

void ASparkyCharacter::SetHat(bool bOn)
{
	bHat = bOn;
	Rig->SetHat(bOn);
}

void ASparkyCharacter::SetZoneGravity(float Scale)
{
	ZoneGravity = Scale;
	ZoneTime = 0.1f;
}

void ASparkyCharacter::SetForm(bool bFluffernutter)
{
	if (bFluffernutter == bBig)
	{
		return;
	}
	bBig = bFluffernutter;
	// keep the feet where they are while the capsule changes size
	const float OldHalf = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float NewHalf = bBig ? 82.f : HalfHeight;
	GetCapsuleComponent()->SetCapsuleSize(bBig ? 58.f : 36.f, NewHalf);
	AddActorWorldOffset(FVector(0, 0, NewHalf - OldHalf));
	Rig->Build(bBig ? ECreature::FluffBig : ECreature::Sparky);
	Rig->SetBaseZ(-NewHalf);
	Rig->SetHat(bHat && !bBig);
	GetCharacterMovement()->JumpZVelocity = bBig ? 700.f : 760.f;
	ZoomLevel = bBig ? 2 : 1;
}

void ASparkyCharacter::SetGravity(float Scale)
{
	BaseGravity = Scale;
	if (!bCling)
	{
		GetCharacterMovement()->GravityScale = Scale;
	}
}

void ASparkyCharacter::SetIce(float A)
{
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->GroundFriction = FMath::Lerp(8.f, 0.5f, A);
	M->BrakingDecelerationWalking = FMath::Lerp(3000.f, 200.f, A);
	M->MaxAcceleration = FMath::Lerp(2600.f, 900.f, A);
}

void ASparkyCharacter::SetCinematic(bool bOn, const FVector& Pos, const FVector& LookAt)
{
	bCinematic = bOn;
	CinePos = Pos;
	CineLook = LookAt;
	if (bOn)
	{
		bWelding = false;
		GetCharacterMovement()->Velocity *= FVector(0, 0, 1);
	}
}

void ASparkyCharacter::SetAttract(bool bOn, const FVector& Center, float Radius)
{
	bAttract = bOn;
	AttractCenter = Center;
	AttractRadius = Radius;
	Rig->SetHidden(bOn);
	SetActorEnableCollision(!bOn);
	GetCharacterMovement()->SetMovementMode(bOn ? MOVE_None : MOVE_Falling);
}

// ---------------------------------------------------------------- camera & model

void ASparkyCharacter::UpdateCamera(const FControls& C, float Dt)
{
	if (bAttract)
	{
		AttractAngle += Dt * 5.f;
		const float A = FMath::DegreesToRadians(AttractAngle);
		const FVector Pos = AttractCenter + FVector(FMath::Cos(A), FMath::Sin(A), 0.f) * AttractRadius + FVector(0, 0, AttractRadius * 0.35f);
		Camera->SetWorldLocationAndRotation(Pos, (AttractCenter + FVector(0, 0, 250.f) - Pos).Rotation());
		return;
	}
	if (bCinematic && !CinePos.IsZero())
	{
		CamPos = FMath::VInterpTo(CamPos, CinePos, Dt, 3.f);
		Camera->SetWorldLocationAndRotation(CamPos, (CineLook - CamPos).Rotation());
		return;
	}

	const ASparkyGameMode* Mode = ASparkyGameMode::Get(this);
	const bool bPlaying = Mode && Mode->IsPlaying() && !bCinematic;
	if (bPlaying)
	{
		if (C.CamSnap != 0.f)
		{
			CamYawTarget += C.CamSnap * 45.f;
			CamIdle = 0.f;
		}
		if (FMath::Abs(C.Cam.X) > 0.1f)
		{
			CamYawTarget += C.Cam.X * 160.f * Dt;
			CamIdle = 0.f;
		}
		if (FMath::Abs(C.Cam.Y) > 0.1f)
		{
			CamPitch = FMath::Clamp(CamPitch + C.Cam.Y * 90.f * Dt, -60.f, 10.f);
		}
		if (bMouseCaptured && !C.Mouse.IsNearlyZero())
		{
			CamYawTarget += C.Mouse.X * 2.5f;
			CamYaw += C.Mouse.X * 2.5f;
			CamPitch = FMath::Clamp(CamPitch + C.Mouse.Y * 2.f, -60.f, 10.f);
			CamIdle = 0.f;
		}
		if (C.bRecenter)
		{
			CamYawTarget = CamYaw + FMath::FindDeltaAngleDegrees(CamYaw, GetActorRotation().Yaw);
			CamPitch = -24.f;
		}
		if (C.Zoom != 0)
		{
			ZoomLevel = FMath::Clamp(ZoomLevel + C.Zoom, 0, 2);
		}
		// lazily swing round behind Sparky while he runs on the ground (never mid-jump: the stick is
		// camera-relative, so a turning camera bends jumps) and not when he runs at the camera
		CamIdle += Dt;
		const FVector V = GetVelocity();
		if (CamIdle > 1.2f && V.Size2D() > 250.f && !bCling && GetCharacterMovement()->IsMovingOnGround())
		{
			const float Delta = FMath::FindDeltaAngleDegrees(CamYawTarget, V.Rotation().Yaw);
			if (FMath::Abs(Delta) < 110.f)
			{
				CamYawTarget += Delta * FMath::Clamp(Dt * 0.7f * V.Size2D() / 720.f, 0.f, 1.f);
			}
		}
	}
	CamYaw += FMath::FindDeltaAngleDegrees(CamYaw, CamYawTarget) * FMath::Clamp(Dt * 8.f, 0.f, 1.f);
	CamDist = FMath::FInterpTo(CamDist, Zooms[ZoomLevel], Dt, 5.f);

	// follow: snappy sideways, a little lazy vertically while jumping, but never letting Sparky
	// get far above or below the middle of the screen
	const FVector Target = GetActorLocation() + FVector(0, 0, 45.f);
	const bool bGround = GetCharacterMovement()->IsMovingOnGround() || bCling;
	CamFocus.X = FMath::FInterpTo(CamFocus.X, Target.X, Dt, 14.f);
	CamFocus.Y = FMath::FInterpTo(CamFocus.Y, Target.Y, Dt, 14.f);
	CamFocus.Z = FMath::FInterpTo(CamFocus.Z, Target.Z, Dt, bGround ? 8.f : 3.f);
	CamFocus.Z = FMath::Clamp(CamFocus.Z, Target.Z - 140.f, Target.Z + 100.f);

	// keep Sparky in view: if something's in the way, tilt up over it first; only pull in as a
	// last resort. Pulling in is instant, easing back out is slow, so the camera doesn't pump.
	FCollisionQueryParams Q(SCENE_QUERY_STAT(SparkyCam), false, this);
	const FCollisionShape Ball = FCollisionShape::MakeSphere(20.f);
	auto Blocked = [&](const FVector& To, FHitResult* Out = nullptr)
	{
		FHitResult H;
		const bool bHit = GetWorld()->SweepSingleByChannel(H, CamFocus, To, FQuat::Identity, ECC_Camera, Ball, Q);
		if (Out)
		{
			*Out = H;
		}
		return bHit;
	};
	float WantPitch = CamPitch;
	float WantDist = CamDist;
	bool bClear = false;
	for (float Extra : {0.f, 12.f, 24.f, 36.f, 48.f})
	{
		const float Pt = FMath::Max(CamPitch - Extra, -78.f);
		if (!Blocked(CamFocus - FRotator(Pt, CamYaw, 0).Vector() * CamDist))
		{
			WantPitch = Pt;
			bClear = true;
			break;
		}
	}
	if (!bClear)
	{
		FHitResult Hit;
		WantPitch = FMath::Max(CamPitch - 24.f, -78.f);
		if (Blocked(CamFocus - FRotator(WantPitch, CamYaw, 0).Vector() * CamDist, &Hit))
		{
			WantDist = FMath::Max(Hit.Distance - 10.f, 120.f);
		}
	}
	CamPitchUsed = FMath::FInterpTo(CamPitchUsed, WantPitch, Dt, WantPitch < CamPitchUsed ? 6.f : 2.5f);
	CamDistUsed = WantDist < CamDistUsed ? WantDist : FMath::FInterpTo(CamDistUsed, WantDist, Dt, 2.5f);
	FVector Desired = CamFocus - FRotator(CamPitchUsed, CamYaw, 0).Vector() * CamDistUsed;
	FHitResult Hit;
	if (Blocked(Desired, &Hit))
	{
		Desired = Hit.Location;  // still easing: never end up inside a wall
	}
	CamPos = Desired;
	FVector ShakeOff = FVector::ZeroVector;
	if (ShakeAmount > 0.f)
	{
		ShakeOff = FMath::VRand() * ShakeAmount;
		ShakeAmount = FMath::Max(0.f, ShakeAmount - Dt * 30.f);
	}
	Camera->SetWorldLocationAndRotation(CamPos + ShakeOff, (CamFocus - CamPos).Rotation());
}

void ASparkyCharacter::UpdateRig(float Dt)
{
	if (bAttract)
	{
		return;
	}
	UCharacterMovementComponent* M = GetCharacterMovement();
	FRigPose P;
	P.Speed = M->Velocity.Size2D();
	P.bAir = M->IsFalling() || bCling;
	P.VZ = M->Velocity.Z;
	P.bCling = bCling;
	P.bWeld = bWelding;
	if (AttackTime > 0.f)
	{
		P.Attack = bPounce ? 0.5f : 1.f - AttackTime / SwipeTime;
	}
	if (FlipTime > 0.f)
	{
		P.Flip = 1.f - FlipTime / FlipDuration;
	}
	P.Crouch = SlamState == 1 ? 0.8f : bDown ? 0.9f : CrouchAmount * 0.5f;
	Rig->Animate(Dt, P);
	// blink while invulnerable
	const bool bBlink = Invuln > 0.f && !bDown && FMath::Fmod(Invuln, 0.16f) < 0.06f;
	Rig->SetHidden(bBlink || bModelHidden);
	if (!bDown)
	{
		Rig->SetFlash(FMath::Max(0.f, Invuln - 1.3f) * 2.5f);
	}
}

// ---------------------------------------------------------------- dev

void ASparkyCharacter::AutoPlay(FControls& C, float Dt)
{
	// dev (-AutoPlay): wander, jump and swipe at random, for soak tests and screenshots
	AutoPlayTime += Dt;
	static const bool bWeldTest = FParse::Param(FCommandLine::Get(), TEXT("AutoWeld"));
	if (bWeldTest)
	{
		// dev (-AutoPlay -AutoWeld): get the welder and hold B (weld test)
		bWelder = true;
		C.Move = FVector2D::ZeroVector;
		C.bAttackHeld = true;
		C.bConfirm |= FMath::Fmod(AutoPlayTime, 1.5f) < Dt;
		return;
	}
	static const bool bClimb = FParse::Param(FCommandLine::Get(), TEXT("AutoClimb"));
	if (bClimb)
	{
		// dev (-AutoPlay -AutoClimb): back into the wall behind, then keep jumping (velcro test)
		C.Move = AutoPlayTime < 0.6f ? FVector2D(0, -1) : FVector2D::ZeroVector;
		C.bJump = FMath::Fmod(AutoPlayTime, 0.55f) < Dt && AutoPlayTime > 1.f;
		C.bJumpHeld = true;
		C.bConfirm |= FMath::Fmod(AutoPlayTime, 1.5f) < Dt;
		return;
	}
	static FVector2D Dir(0, 1);
	if (FMath::Fmod(AutoPlayTime, 2.f) < Dt)
	{
		Dir = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-0.2f, 1.f)).GetSafeNormal();
	}
	C.Move = Dir;
	C.bJump |= FMath::FRand() < Dt * 0.8f;
	C.bJumpHeld = true;
	C.bAttack |= FMath::FRand() < Dt * 0.5f;
	C.bConfirm |= FMath::Fmod(AutoPlayTime, 1.5f) < Dt;
}

void ASparkyCharacter::DevTick(float Dt)
{
	static const bool bLogPos = FParse::Param(FCommandLine::Get(), TEXT("LogPos"));
	static float PosTimer = 0.f;
	if (bLogPos && (PosTimer += Dt) > 2.f)
	{
		PosTimer = 0.f;
		const ASparkyGameMode* Mode = ASparkyGameMode::Get(this);
		UE_LOG(LogSparky, Log, TEXT("Pos %s health %d state %d"), *GetActorLocation().ToCompactString(), Health, Mode ? int32(Mode->GetState()) : -1);
	}
	if (AutoShotInterval <= 0.f)
	{
		return;
	}
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (Now >= NextAutoShot)
	{
		NextAutoShot = Now + AutoShotInterval;
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ConsoleCommand(TEXT("shot showui"));
		}
	}
}
