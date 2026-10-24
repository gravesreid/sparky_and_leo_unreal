#include "Enemy.h"
#include "LevelGeo.h"
#include "Props.h"
#include "CreatureRig.h"
#include "SparkyCharacter.h"
#include "SparkyGameMode.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AEnemy::AEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	bUseControllerRotationYaw = false;
	UCharacterMovementComponent* M = GetCharacterMovement();
	M->bOrientRotationToMovement = false;
	M->GravityScale = 2.4f;
	M->AirControl = 0.3f;
	M->BrakingDecelerationWalking = 1500.f;
	Rig = CreateDefaultSubobject<UCreatureRig>(TEXT("Rig"));
	Rig->SetupAttachment(RootComponent);
}

void AEnemy::Init(EEnemy InType, int32 Variant)
{
	Type = InType;
	Home = GetActorLocation();
	WanderYaw = FMath::FRandRange(0.f, 360.f);
	UCharacterMovementComponent* M = GetCharacterMovement();
	UCapsuleComponent* Cap = GetCapsuleComponent();
	float Half = 40.f;
	switch (Type)
	{
	case EEnemy::BogBlob:
		Cap->SetCapsuleSize(40.f, Half = 36.f);
		MaxHP = 1;
		M->MaxWalkSpeed = 260.f;
		Rig->Build(ECreature::BogBlob);
		Aggro = 800.f;
		break;
	case EEnemy::AlleyCat:
		Cap->SetCapsuleSize(32.f, Half = 34.f);
		MaxHP = 2;
		M->MaxWalkSpeed = 560.f;
		Rig->Build(ECreature::AlleyCat, Variant);
		Aggro = 2000.f;
		Leash = 0.f;
		break;
	case EEnemy::Meatball:
		Cap->SetCapsuleSize(72.f, Half = 74.f);
		MaxHP = 12;
		M->MaxWalkSpeed = 500.f;
		Rig->Build(ECreature::Meatball);
		Aggro = 3000.f;
		Leash = 0.f;
		break;
	case EEnemy::Captain:
		Cap->SetCapsuleSize(42.f, Half = 88.f);
		MaxHP = 16;
		M->MaxWalkSpeed = 480.f;
		Rig->Build(ECreature::Captain);
		Aggro = 4000.f;
		Leash = 0.f;
		break;
	case EEnemy::Wraith:
		Cap->SetCapsuleSize(32.f, Half = 34.f);
		MaxHP = 2;
		M->MaxWalkSpeed = 600.f;
		Rig->Build(ECreature::AlleyCat, Variant);
		Rig->SetTint(FLinearColor(0.55f, 1.f, 1.3f));
		Aggro = 3000.f;
		Leash = 0.f;
		break;
	case EEnemy::Guardian:
		MaxHP = 18;
		HP = MaxHP;
		Aggro = 4000.f;
		Leash = 0.f;
		SetGuardianForm(0);
		if (ALevelGeo* Geo = ALevelGeo::Get(this))
		{
			M->GravityScale = Geo->Theme.Gravity;
		}
		return;
	case EEnemy::PirateDog:
		Cap->SetCapsuleSize(36.f, Half = 76.f);
		MaxHP = 3;
		M->MaxWalkSpeed = 420.f;
		Rig->Build(ECreature::PirateDog, Variant);
		Aggro = 1100.f;
		break;
	}
	HP = MaxHP;
	Act = Type == EEnemy::Captain ? EEnemy::PirateDog : Type == EEnemy::Wraith ? EEnemy::AlleyCat : Type;
	Rig->SetBaseZ(-Half);
	if (ALevelGeo* Geo = ALevelGeo::Get(this))
	{
		M->GravityScale = Geo->Theme.Gravity;
	}
}

float AEnemy::GetTop() const
{
	return GetActorLocation().Z + GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
}

float AEnemy::GetRadius() const
{
	return GetCapsuleComponent()->GetScaledCapsuleRadius();
}

void AEnemy::SetState(EState S)
{
	State = S;
	StateTime = 0.f;
	bHitThisAttack = false;
}

void AEnemy::Tick(float Dt)
{
	Super::Tick(Dt);
	Flash = FMath::Max(0.f, Flash - Dt * 4.f);
	Rig->SetFlash(Flash, FLinearColor(1.f, 1.f, 1.f));
	if (bDead)
	{
		DeadTime += Dt;
		Rig->SetRelativeScale3D(FVector(FMath::Max(0.01f, 1.f - DeadTime * 2.5f)));
		if (DeadTime > 0.4f)
		{
			Destroy();
		}
		return;
	}
	const ASparkyGameMode* Mode = ASparkyGameMode::Get(this);
	if (Mode && Mode->IsPlaying())
	{
		StateTime += Dt;
		Cooldown = FMath::Max(0.f, Cooldown - Dt);
		Think(Dt);
	}
	ALevelGeo* Geo = ALevelGeo::Get(this);
	if (Geo && GetActorLocation().Z < Geo->Theme.KillZ)
	{
		Die();  // fell off: counts as beaten
		return;
	}
	FRigPose P;
	P.Speed = GetVelocity().Size2D();
	P.bAir = GetCharacterMovement()->IsFalling();
	P.VZ = GetVelocity().Z;
	P.bDizzy = State == EState::Dizzy;
	if (State == EState::Windup)
	{
		P.Crouch = Act == EEnemy::PirateDog ? 0.f : FMath::Min(1.f, StateTime * 3.f) * 0.6f;
		if (Act == EEnemy::PirateDog)
		{
			P.Attack = FMath::Min(StateTime / 0.45f, 1.f) * 0.4f;
		}
	}
	if (State == EState::Attack)
	{
		P.Attack = Act == EEnemy::PirateDog ? FMath::Min(0.4f + StateTime / 0.4f * 0.6f, 1.f) : FMath::Min(StateTime / 0.5f, 1.f);
	}
	Rig->Animate(Dt, P);
}

void AEnemy::FacePlayer(float Dt, float Rate)
{
	if (ASparkyCharacter* P = ASparkyCharacter::Get(this))
	{
		const float Want = (P->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
		SetActorRotation(FRotator(0, FMath::FixedTurn(GetActorRotation().Yaw, Want, Rate * Dt), 0));
	}
}

void AEnemy::TouchDamage()
{
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	if (!P || P->IsDown())
	{
		return;
	}
	const float PR = P->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float PH = P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector D = P->GetActorLocation() - GetActorLocation();
	const float Feet = P->GetActorLocation().Z - PH;
	if (Feet > GetTop() - 25.f)
	{
		return;  // he's on top of us: that's a stomp, not a bite
	}
	if (D.Size2D() < PR + GetRadius() + 12.f && FMath::Abs(D.Z) < PH + GetCapsuleComponent()->GetScaledCapsuleHalfHeight())
	{
		P->Hurt(1, GetActorLocation());
	}
}

void AEnemy::Think(float Dt)
{
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	UCharacterMovementComponent* M = GetCharacterMovement();
	const bool bAir = M->IsFalling();
	const FVector Me = GetActorLocation();
	FVector ToP = P ? P->GetActorLocation() - Me : FVector::ZeroVector;
	const float Dist = ToP.Size2D();
	const bool bCanSee = P && !P->IsDown() && FMath::Abs(ToP.Z) < 450.f && Dist < Aggro
		&& (Leash <= 0.f || FVector::Dist2D(P->GetActorLocation(), Home) < Leash);

	if (Act == EEnemy::BogBlob || bCharge || ((State == EState::Attack || State == EState::Leap) && Act != EEnemy::PirateDog))
	{
		TouchDamage();
	}

	switch (State)
	{
	case EState::Idle:
	{
		WanderTime -= Dt;
		if (WanderTime <= 0.f)
		{
			WanderTime = FMath::FRandRange(1.5f, 3.f);
			WanderYaw = FMath::FRandRange(0.f, 360.f);
		}
		FVector Dir = FRotator(0, WanderYaw, 0).Vector();
		if (FVector::Dist2D(Me, Home) > 300.f)
		{
			Dir = (Home - Me).GetSafeNormal2D();
		}
		AddMovementInput(Dir, 0.3f);
		SetActorRotation(FRotator(0, FMath::FixedTurn(GetActorRotation().Yaw, Dir.Rotation().Yaw, 200.f * Dt), 0));
		if (bCanSee)
		{
			SetState(EState::Chase);
			if (Act == EEnemy::PirateDog)
			{
				ASparkyGameMode::Sfx(this, ESfx::Bark, Me);
			}
		}
		break;
	}
	case EState::Chase:
		if (!bCanSee)
		{
			SetState(EState::Idle);
			break;
		}
		FacePlayer(Dt);
		switch (Act)
		{
		case EEnemy::BogBlob:
			if (!bAir && Cooldown <= 0.f)
			{
				const bool bGiant = Type == EEnemy::Guardian;
				LaunchCharacter(ToP.GetSafeNormal2D() * (bGiant ? FMath::Min(Dist, 650.f) : 380.f) + FVector(0, 0, bGiant ? 1100.f : 700.f), true, true);
				Cooldown = bGiant ? 1.6f : FMath::FRandRange(0.9f, 1.4f);
				ASparkyGameMode::Sfx(this, ESfx::Boing, Me, 0.4f);
			}
			break;
		case EEnemy::AlleyCat:
			AddMovementInput(ToP.GetSafeNormal2D(), Dist > 350.f ? 1.f : 0.4f);
			if (Dist < 340.f && Cooldown <= 0.f && !bAir)
			{
				SetState(EState::Windup);
				ASparkyGameMode::Sfx(this, ESfx::Hiss, Me, 0.7f);
			}
			break;
		case EEnemy::PirateDog:
			if (Dist > 150.f)
			{
				AddMovementInput(ToP.GetSafeNormal2D(), 1.f);
			}
			if (Type == EEnemy::Captain && Lunges >= 3 && Cooldown <= 0.f && !bAir && Dist < 1100.f)
			{
				// "Arrr-ROOO!" lowers his head and charges
				SetState(EState::Windup);
				bCharge = true;
				ASparkyGameMode::Sfx(this, ESfx::Bark, Me);
			}
			else if (Dist < (Type == EEnemy::Captain ? 230.f : 200.f) && Cooldown <= 0.f && !bAir)
			{
				SetState(EState::Windup);
				ASparkyGameMode::Sfx(this, ESfx::Growl, Me, 0.6f);
			}
			break;
		case EEnemy::Meatball:
			AddMovementInput(ToP.GetSafeNormal2D(), Dist > 500.f ? 1.f : 0.5f);
			if (Cooldown <= 0.f && !bAir)
			{
				if (Lunges >= 2)
				{
					SetState(EState::Leap);
					LaunchCharacter(ToP.GetSafeNormal2D() * FMath::Min(Dist, 700.f) + FVector(0, 0, 1500.f), true, true);
					ASparkyGameMode::Sfx(this, ESfx::Growl, Me);
				}
				else if (Dist < 520.f)
				{
					SetState(EState::Windup);
					ASparkyGameMode::Sfx(this, ESfx::Hiss, Me);
				}
			}
			break;
		}
		break;
	case EState::Windup:
	{
		FacePlayer(Dt, Act == EEnemy::Meatball ? 300.f : 600.f);
		const float Wind = bCharge ? 0.75f : Act == EEnemy::Meatball ? 0.7f : Act == EEnemy::PirateDog ? 0.45f : 0.55f;
		if (StateTime > Wind)
		{
			SetState(EState::Attack);
			if (Act == EEnemy::AlleyCat || Act == EEnemy::Meatball || bCharge)
			{
				const float Speed = bCharge ? 1500.f : Act == EEnemy::Meatball ? 1350.f : 1050.f;
				LaunchCharacter(GetActorForwardVector() * Speed + FVector(0, 0, 320.f), true, true);
				ASparkyGameMode::Sfx(this, ESfx::Swipe, Me, 0.8f);
			}
		}
		break;
	}
	case EState::Attack:
	{
		if (Act == EEnemy::PirateDog && !bCharge)
		{
			// the cutlass comes down at 0.2 s
			if (!bHitThisAttack && StateTime > 0.2f && P)
			{
				bHitThisAttack = true;
				ASparkyGameMode::Sfx(this, ESfx::Swipe, Me, 0.8f);
				const FVector D = P->GetActorLocation() - Me;
				const float Reach = Type == EEnemy::Captain ? 260.f : Type == EEnemy::Guardian ? 300.f : 210.f;
				if (D.Size2D() < Reach && FVector::DotProduct(D.GetSafeNormal2D(), GetActorForwardVector()) > 0.2f && FMath::Abs(D.Z) < 160.f)
				{
					P->Hurt(1, Me);
				}
			}
			if (StateTime > 0.45f)
			{
				SetState(EState::Recover);
				++Lunges;
			}
		}
		else if (StateTime > 0.3f && !bAir)
		{
			SetState(EState::Recover);
			if (bCharge)
			{
				bCharge = false;
				Lunges = 0;
			}
			else
			{
				++Lunges;
			}
		}
		break;
	}
	case EState::Recover:
		if (StateTime > (Act == EEnemy::Meatball ? 0.6f : 0.8f))
		{
			Cooldown = Act == EEnemy::Meatball ? 0.6f : FMath::FRandRange(0.8f, 1.6f);
			SetState(EState::Chase);
		}
		break;
	case EState::Hurt:
		if (StateTime > 0.45f && !bAir)
		{
			Cooldown = FMath::Max(Cooldown, 0.4f);
			SetState(EState::Chase);
		}
		break;
	case EState::Leap:
		if (StateTime > 3.f)
		{
			SetState(EState::Chase);  // never landed?
		}
		break;
	case EState::Dizzy:
		if (FMath::Fmod(StateTime, 0.5f) < Dt)
		{
			AFx::Spawn(GetWorld(), EFx::Stars, FVector(Me.X, Me.Y, GetTop() + 20.f), FLinearColor(1.f, 0.9f, 0.3f), 2.f);
		}
		if (StateTime > 2.4f)
		{
			Cooldown = 0.5f;
			SetState(EState::Chase);
		}
		break;
	}
}

void AEnemy::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (Type == EEnemy::Guardian && Act == EEnemy::BogBlob && !bDead && State == EState::Chase)
	{
		State = EState::Leap;  // every giant hop lands with a shockwave
	}
	if (State == EState::Leap && !bDead)
	{
		// SLAM. A ring of force rolls out across the mat: jump it
		const FVector Feet = GetActorLocation() - FVector(0, 0, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
		if (ASparkyGameMode* Mode = ASparkyGameMode::Get(this))
		{
			AHazard* H = Mode->Spawn<AHazard>(Feet);
			H->Init(EHazard::Shockwave);
		}
		AFx::Spawn(GetWorld(), EFx::Poof, Feet, FLinearColor(0.9f, 0.8f, 0.6f), 3.f);
		ASparkyGameMode::Sfx(this, ESfx::GroundPound, Feet);
		if (ASparkyCharacter* P = ASparkyCharacter::Get(this))
		{
			P->Shake(14.f);
		}
		Lunges = 0;
		SetState(EState::Dizzy);
	}
}

void AEnemy::TakeHit(int32 Damage, const FVector& From, bool bStomp)
{
	if (bDead)
	{
		return;
	}
	if (Act == EEnemy::Meatball && State == EState::Dizzy)
	{
		Damage *= 2;  // hit him while he's seeing stars
	}
	HP -= Damage;
	Flash = 1.f;
	if (Type == EEnemy::Guardian && HP > 0)
	{
		const int32 NewForm = HP > 12 ? 0 : HP > 6 ? 1 : 2;
		if (NewForm != Form)
		{
			SetGuardianForm(NewForm);
			AFx::Spawn(GetWorld(), EFx::Poof, GetActorLocation(), FLinearColor(1.f, 0.85f, 0.4f), 4.f);
			ASparkyGameMode::Sfx(this, ESfx::Portal, GetActorLocation());
			if (ASparkyGameMode* Mode = ASparkyGameMode::Get(this))
			{
				Mode->OnGuardianShift(NewForm);
			}
			return;
		}
	}
	if (Type == EEnemy::Captain && !bSummoned && HP <= MaxHP / 2)
	{
		bSummoned = true;
		if (ASparkyGameMode* Mode = ASparkyGameMode::Get(this))
		{
			Mode->Radio(EWho::Captain, TEXT("Lads! LADS! Get this overgrown hamster off me!"));
			for (float S : {-1.f, 1.f})
			{
				const FVector At = GetActorLocation() + FRotator(0, GetActorRotation().Yaw + 90.f * S, 0).Vector() * 400.f;
				AEnemy* E = Mode->Spawn<AEnemy>(At + FVector(0, 0, 60.f));
				E->Init(EEnemy::PirateDog, S > 0 ? 1 : 0);
				AFx::Spawn(GetWorld(), EFx::Poof, At, FLinearColor(0.8f, 0.8f, 0.8f), 2.f);
			}
		}
	}
	ASparkyGameMode::Sfx(this, ESfx::Hit, GetActorLocation());
	AFx::Spawn(GetWorld(), EFx::Burst, GetActorLocation() + FVector(0, 0, 20), FLinearColor(1.f, 0.95f, 0.6f), 1.2f);
	if (HP <= 0)
	{
		Die();
		return;
	}
	const FVector Away = (GetActorLocation() - From).GetSafeNormal2D();
	if (IsBoss())
	{
		LaunchCharacter(Away * 250.f, true, false);
		if (State != EState::Dizzy && State != EState::Leap)
		{
			Cooldown = FMath::Max(Cooldown, 0.3f);
		}
		ASparkyGameMode::Sfx(this, ESfx::Hiss, GetActorLocation(), 0.8f);
		return;
	}
	LaunchCharacter(Away * (bStomp ? 200.f : 600.f) + FVector(0, 0, bStomp ? 0.f : 350.f), true, true);
	SetState(EState::Hurt);
	if (Act == EEnemy::AlleyCat)
	{
		ASparkyGameMode::Sfx(this, ESfx::Hiss, GetActorLocation(), 0.6f);
	}
	else if (Act == EEnemy::PirateDog)
	{
		ASparkyGameMode::Sfx(this, ESfx::Bark, GetActorLocation(), 0.8f);
	}
}

void AEnemy::Die()
{
	if (bDead)
	{
		return;
	}
	bDead = true;
	HP = 0;
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	AFx::Spawn(GetWorld(), EFx::Poof, GetActorLocation(), FLinearColor(0.9f, 0.9f, 1.f), IsBoss() ? 3.f : 1.2f);
	ASparkyGameMode::Sfx(this, ESfx::Poof, GetActorLocation());
	if (ASparkyGameMode* Mode = ASparkyGameMode::Get(this))
	{
		Mode->OnEnemyKilled(this);
	}
}

void AEnemy::SetGuardianForm(int32 NewForm)
{
	// the Guardian of the Labyrinth wears three shapes: a great cat, a swordsman, a living hill of jelly
	Form = NewForm;
	static const EEnemy Acts[] = {EEnemy::Meatball, EEnemy::PirateDog, EEnemy::BogBlob};
	static const ECreature Looks[] = {ECreature::Meatball, ECreature::Captain, ECreature::BogBlob};
	static const float Radii[] = {72.f, 55.f, 105.f}, Halves[] = {74.f, 125.f, 100.f}, Scales[] = {1.f, 1.45f, 2.8f};
	Act = Acts[Form];
	const float OldHalf = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	GetCapsuleComponent()->SetCapsuleSize(Radii[Form], Halves[Form]);
	AddActorWorldOffset(FVector(0, 0, Halves[Form] - OldHalf + 5.f));
	Rig->Build(Looks[Form]);
	Rig->SetScale(Scales[Form]);
	Rig->SetBaseZ(-Halves[Form]);
	Rig->SetTint(FLinearColor(1.15f, 0.85f, 1.35f));
	GetCharacterMovement()->MaxWalkSpeed = Form == 1 ? 560.f : 480.f;
	Lunges = 0;
	bCharge = false;
	Cooldown = 1.f;
	SetState(EState::Chase);
}
