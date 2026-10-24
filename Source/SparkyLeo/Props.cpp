#include "Props.h"
#include "Assets.h"
#include "LevelGeo.h"
#include "RetroMesh.h"
#include "SparkyCharacter.h"
#include "SparkyGameMode.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const FLinearColor White(0.92f, 0.92f, 0.9f);
	const FLinearColor Black(0.06f, 0.06f, 0.07f);
	const FLinearColor Steel(0.45f, 0.48f, 0.52f);
	const FLinearColor DarkSteel(0.18f, 0.19f, 0.22f);
	const FLinearColor Brass(0.85f, 0.65f, 0.25f);

	FLinearColor Flat(const FVector&, const FVector&) { return FLinearColor::White; }

	/** A mesh whose vertex colours are exactly the colours given (for glow). */
	FRetroMesh GlowBuilder()
	{
		FRetroMesh M;
		M.Light = Flat;
		return M;
	}

	ASparkyGameMode* Mode(const UObject* O) { return ASparkyGameMode::Get(O); }
	bool Playing(const UObject* O)
	{
		const ASparkyGameMode* M = Mode(O);
		return M && M->IsPlaying();
	}

	/** Distance from P to the segment A-B. */
	float SegDist(const FVector& P, const FVector& A, const FVector& B)
	{
		return FMath::PointDistToSegment(P, A, B);
	}

	/** Sparky's feet. */
	FVector Feet(const ASparkyCharacter* P)
	{
		return P->GetActorLocation() - FVector(0, 0, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
}

// ---------------------------------------------------------------- mesh helpers

UProceduralMeshComponent* PropMesh::Make(AActor* Owner, const TCHAR* Name, USceneComponent* Parent)
{
	UProceduralMeshComponent* C = Owner->CreateDefaultSubobject<UProceduralMeshComponent>(Name);
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->CastShadow = false;
	if (Parent)
	{
		C->SetupAttachment(Parent);
	}
	return C;
}

void PropMesh::Commit(AActor* Owner, UProceduralMeshComponent* C, const FRetroMesh& M, const TCHAR* Tex, bool bCollide)
{
	C->ClearAllMeshSections();
	M.Commit(C, 0, bCollide);
	if (bCollide)
	{
		C->bUseComplexAsSimpleCollision = true;
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	ALevelGeo* Geo = ALevelGeo::Get(Owner);
	UMaterialInstanceDynamic* Mid = Geo ? Geo->MakeMaterial(Owner, Tex) : UMaterialInstanceDynamic::Create(Assets::RetroMaterial(), Owner);
	if (!Geo)
	{
		Mid->SetTextureParameterValue("Tex", Assets::Texture(Tex));
	}
	C->SetMaterial(0, Mid);
}

UMaterialInstanceDynamic* PropMesh::CommitGlow(AActor* Owner, UProceduralMeshComponent* C, FRetroMesh& M, const FLinearColor& Color, float Intensity)
{
	for (FLinearColor& Col : M.Colors)
	{
		Col = Col * 2.f;
		Col.A = 1.f;
	}
	C->ClearAllMeshSections();
	M.Commit(C, 0, false);
	UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Assets::GlowMaterial(), Owner);
	Mid->SetVectorParameterValue("Color", Color);
	Mid->SetScalarParameterValue("Intensity", Intensity);
	C->SetMaterial(0, Mid);
	return Mid;
}

// ---------------------------------------------------------------- pickups

APickup::APickup()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = PropMesh::Make(this, TEXT("Mesh"), RootComponent);
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

void APickup::Init(EPickup InType, bool bInActive)
{
	Type = InType;
	Home = GetActorLocation();
	Origin = Home;
	Time = FMath::FRand() * 10.f;
	FRetroMesh M;
	FRetroMesh G = GlowBuilder();
	FLinearColor Aura(1.f, 1.f, 1.f);
	switch (Type)
	{
	case EPickup::Catnip:
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = 2 * PI * i / 5;
			M.Box(FVector(FMath::Cos(A) * 9, FMath::Sin(A) * 9, 2), FVector(9, 4, 1.5f), FRotator(20.f, FMath::RadiansToDegrees(A), 0), FLinearColor(0.3f, 0.85f, 0.25f));
		}
		M.Prism(FVector(0, 0, -10), 1.5f, 12.f, 4, FLinearColor(0.2f, 0.5f, 0.15f));
		M.Box(FVector(0, 0, 8), FVector(4, 4, 6), FLinearColor(0.45f, 0.95f, 0.35f));
		G.Diamond(FVector::ZeroVector, 14.f, FLinearColor(0.15f, 0.45f, 0.1f));
		break;
	case EPickup::Styrofoam:
		M.Box(FVector::ZeroVector, FVector(22, 22, 16), FLinearColor(0.97f, 0.97f, 0.93f));
		for (int32 i = 0; i < 6; ++i)
		{
			M.Sphere(FVector((i % 3 - 1) * 13.f, (i / 3) * 18.f - 9.f, 16.f), FVector(5.f), 2, 5, FLinearColor(0.97f, 0.97f, 0.93f));
		}
		G.Diamond(FVector::ZeroVector, 40.f, FLinearColor(0.25f, 0.25f, 0.3f), 0.6f);
		break;
	case EPickup::PeanutButter:
		M.Prism(FVector(0, 0, -14), 14.f, 22.f, 10, FLinearColor(0.72f, 0.48f, 0.22f));
		M.Prism(FVector(0, 0, -6), 14.4f, 8.f, 10, FLinearColor(0.95f, 0.85f, 0.4f));
		M.Prism(FVector(0, 0, 8), 15.f, 6.f, 10, FLinearColor(0.85f, 0.15f, 0.12f));
		G.Diamond(FVector::ZeroVector, 32.f, FLinearColor(0.3f, 0.22f, 0.05f), 0.8f);
		break;
	case EPickup::CatFood:
		M.Prism(FVector(0, 0, -7), 14.f, 14.f, 10, FLinearColor(0.75f, 0.78f, 0.82f));
		M.Prism(FVector(0, 0, -4), 14.4f, 8.f, 10, FLinearColor(0.2f, 0.35f, 0.85f));
		M.Box(FVector(14.5f, 0, 0), FVector(1, 6, 3), FLinearColor(1.f, 0.55f, 0.1f));
		G.Diamond(FVector::ZeroVector, 32.f, FLinearColor(0.08f, 0.14f, 0.3f), 0.8f);
		break;
	case EPickup::Tang:
		M.Prism(FVector(0, 0, -15), 12.f, 26.f, 10, FLinearColor(1.f, 0.55f, 0.08f));
		M.Prism(FVector(0, 0, 11), 12.5f, 5.f, 10, White);
		M.Box(FVector(12.2f, 0, -3), FVector(1, 6, 6), FLinearColor(0.95f, 0.85f, 0.2f));
		G.Diamond(FVector::ZeroVector, 32.f, FLinearColor(0.35f, 0.18f, 0.02f), 0.8f);
		break;
	case EPickup::Soup:
		M.Prism(FVector(0, 0, -12), 11.f, 12.f, 10, FLinearColor(0.85f, 0.12f, 0.1f));
		M.Prism(FVector(0, 0, 0), 11.f, 12.f, 10, White);
		G.Diamond(FVector::ZeroVector, 30.f, FLinearColor(0.3f, 0.05f, 0.05f), 0.8f);
		break;
	case EPickup::DrivePart:
	{
		G.Diamond(FVector::ZeroVector, 20.f, FLinearColor(0.3f, 0.95f, 1.f));
		G.Diamond(FVector::ZeroVector, 45.f, FLinearColor(0.05f, 0.2f, 0.3f), 0.7f);
		for (int32 r = 0; r < 2; ++r)
		{
			const FTransform Saved = M.Xf;
			M.Xf = FTransform(FRotator(r ? 70.f : -20.f, r ? 0.f : 60.f, 0));
			for (int32 i = 0; i < 12; ++i)
			{
				const float A = 2 * PI * i / 12;
				M.Box(FVector(FMath::Cos(A) * 30, FMath::Sin(A) * 30, 0), FVector(6, 3, 3), FRotator(0, FMath::RadiansToDegrees(A) + 90.f, 0), Brass);
			}
			M.Xf = Saved;
		}
		break;
	}
	case EPickup::Welder:
		M.Box(FVector(0, 0, 0), FVector(18, 6, 8), FLinearColor(0.85f, 0.75f, 0.1f));
		M.PrismX(FVector(18, 0, 2), 3.f, 18.f, 6, Steel);
		M.Box(FVector(-6, 0, -12), FVector(5, 5, 8), DarkSteel);
		M.Prism(FVector(-10, 0, -14), 7.f, 26.f, 8, FLinearColor(0.9f, 0.3f, 0.1f));
		G.Diamond(FVector(38, 0, 2), 6.f, FLinearColor(0.5f, 0.8f, 1.f));
		G.Diamond(FVector::ZeroVector, 40.f, FLinearColor(0.15f, 0.2f, 0.3f), 0.7f);
		break;
	case EPickup::Berry:
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = i * 1.7f;
			M.Sphere(FVector(FMath::Cos(A) * 9.f, FMath::Sin(A) * 9.f, -i * 4.f), FVector(10.f), 3, 6, FLinearColor(0.45f, 0.4f, 0.95f));
		}
		M.Box(FVector(0, 0, 12), FVector(12, 4, 1.5f), FRotator(0, 30.f, 15.f), FLinearColor(0.3f, 0.75f, 0.3f));
		G.Diamond(FVector::ZeroVector, 36.f, FLinearColor(0.15f, 0.15f, 0.4f), 0.8f);
		break;
	case EPickup::Shard:
		G.Diamond(FVector::ZeroVector, 16.f, FLinearColor(0.4f, 0.95f, 1.f), 2.4f);
		G.Diamond(FVector::ZeroVector, 40.f, FLinearColor(0.05f, 0.2f, 0.35f), 1.2f);
		M.Diamond(FVector(0, 0, -30), 10.f, FLinearColor(0.6f, 0.65f, 0.75f), 0.6f);
		break;
	case EPickup::Key:
		M.Prism(FVector(0, 0, -20), 3.f, 34.f, 6, FLinearColor(0.95f, 0.78f, 0.25f));
		M.Box(FVector(0, 0, 20), FVector(4, 12, 12), FLinearColor(0.95f, 0.78f, 0.25f));
		M.Box(FVector(0, 6, -16), FVector(3, 6, 3), FLinearColor(0.95f, 0.78f, 0.25f));
		M.Box(FVector(0, 6, -8), FVector(3, 4, 3), FLinearColor(0.95f, 0.78f, 0.25f));
		G.Diamond(FVector(4, 0, 20), 6.f, FLinearColor(0.4f, 1.f, 0.9f));
		G.Diamond(FVector::ZeroVector, 40.f, FLinearColor(0.3f, 0.22f, 0.05f), 1.f);
		break;
	case EPickup::Mote:
		G.Sphere(FVector::ZeroVector, FVector(16.f), 4, 8, FLinearColor(1.f, 0.95f, 0.75f));
		G.Diamond(FVector::ZeroVector, 46.f, FLinearColor(0.35f, 0.3f, 0.15f), 1.f);
		break;
	case EPickup::Sprite:
		G.Diamond(FVector::ZeroVector, 14.f, FLinearColor(1.f, 0.5f, 0.9f));
		G.Box(FVector(0, 16, 6), FVector(4, 14, 1), FRotator(0, 0, 20.f), FLinearColor(0.5f, 0.8f, 1.f));
		G.Box(FVector(0, -16, 6), FVector(4, 14, 1), FRotator(0, 0, -20.f), FLinearColor(0.5f, 0.8f, 1.f));
		G.Diamond(FVector::ZeroVector, 32.f, FLinearColor(0.25f, 0.1f, 0.25f), 1.f);
		break;
	case EPickup::Fish:
		M.Sphere(FVector::ZeroVector, FVector(22, 6, 11), 4, 8, FLinearColor(0.55f, 0.7f, 0.85f));
		M.Tri(FVector(-18, 0, 0), FVector(-34, 0, 12), FVector(-34, 0, -12), FVector(0, 1, 0), FLinearColor(0.4f, 0.55f, 0.75f));
		M.Tri(FVector(-18, 0, 0), FVector(-34, 0, -12), FVector(-34, 0, 12), FVector(0, -1, 0), FLinearColor(0.4f, 0.55f, 0.75f));
		M.Box(FVector(14, 5, 3), FVector(2, 1.5f, 2), FLinearColor(0.05f, 0.05f, 0.05f));
		M.Box(FVector(14, -5, 3), FVector(2, 1.5f, 2), FLinearColor(0.05f, 0.05f, 0.05f));
		G.Diamond(FVector::ZeroVector, 30.f, FLinearColor(0.3f, 0.1f, 0.15f), 0.8f);
		break;
	}
	PropMesh::Commit(this, Mesh, M);
	PropMesh::CommitGlow(this, GlowMesh, G);
	SetActive(bInActive);
}

void APickup::SetActive(bool bOn)
{
	bActive = bOn;
	SetActorHiddenInGame(!bOn);
}

void APickup::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bActive)
	{
		return;
	}
	Time += Dt;
	const bool bBig = Type != EPickup::Catnip;
	if (Type == EPickup::Sprite && Playing(this))
	{
		// run away from Sparky, darting sideways at the edge of its patch
		if (ASparkyCharacter* Chaser = ASparkyCharacter::Get(this))
		{
			FVector Away = Home - Chaser->GetActorLocation();
			Away.Z = 0.f;
			if (Away.Size() < 600.f)
			{
				FVector Dir = Away.GetSafeNormal() + FVector(FMath::Sin(Time * 3.f), FMath::Cos(Time * 2.3f), 0) * 0.4f;
				const FVector Next = Home + Dir.GetSafeNormal() * 430.f * Dt;
				if (FVector::Dist2D(Next, Origin) > 850.f)
				{
					Dir = FVector::CrossProduct(FVector::UpVector, (Origin - Home).GetSafeNormal2D()) * (FMath::Sin(Time * 0.7f) > 0 ? 1.f : -1.f)
					      + (Origin - Home).GetSafeNormal2D() * 0.6f;
				}
				Home += Dir.GetSafeNormal() * 430.f * Dt;
			}
		}
	}
	SetActorLocation(Home + FVector(0, 0, FMath::Sin(Time * 2.5f) * (bBig ? 10.f : 6.f)));
	Mesh->SetRelativeRotation(FRotator(0, Time * (bBig ? 90.f : 160.f), 0));
	GlowMesh->SetRelativeRotation(FRotator(0, -Time * 60.f, 0));
	GlowMesh->SetRelativeScale3D(FVector(1.f + 0.15f * FMath::Sin(Time * 5.f)));
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	if (P && Playing(this) && !P->IsDown() && FVector::Dist(P->GetActorLocation(), GetActorLocation()) < (bBig ? 135.f : 95.f))
	{
		bActive = false;
		AFx::Spawn(GetWorld(), EFx::Burst, GetActorLocation(), Type == EPickup::Catnip ? FLinearColor(0.4f, 1.f, 0.3f) : FLinearColor(1.f, 0.9f, 0.4f), bBig ? 1.5f : 0.8f);
		if (ASparkyGameMode* M = Mode(this))
		{
			M->OnPickup(this);
		}
		Destroy();
	}
}

// ---------------------------------------------------------------- NPCs

ANpc::ANpc()
{
	PrimaryActorTick.bCanEverTick = true;
	UCapsuleComponent* Cap = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Cap->InitCapsuleSize(38.f, 90.f);
	Cap->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);  // never shove the camera in
	RootComponent = Cap;
	Rig = CreateDefaultSubobject<UCreatureRig>(TEXT("Rig"));
	Rig->SetupAttachment(RootComponent);
}

void ANpc::Init(ECreature Kind, int32 Variant, FName InId, bool bInTalk, bool bInDance)
{
	Id = InId;
	bTalk = bInTalk;
	bDance = bInDance;
	HomeYaw = GetActorRotation().Yaw;
	Time = FMath::FRand() * 5.f;
	Rig->Build(Kind, Variant);
	UCapsuleComponent* Cap = CastChecked<UCapsuleComponent>(RootComponent);
	const float Half = FMath::Max(30.f, Rig->GetHeight() * 0.5f);
	Cap->SetCapsuleSize(Kind == ECreature::Leo || Kind == ECreature::Local || Kind == ECreature::Elder ? 38.f : 30.f, Half);
	Rig->SetBaseZ(-Half);
	AddActorWorldOffset(FVector(0, 0, Half));
}

float ANpc::GetHeight() const
{
	return Rig->GetHeight();
}

void ANpc::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	TalkTime = FMath::Max(0.f, TalkTime - Dt);
	FRigPose Pose;
	Pose.Talk = TalkTime;
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	float WantYaw = HomeYaw;
	if (P && bTalk && FVector::Dist(P->GetActorLocation(), GetActorLocation()) < 700.f)
	{
		WantYaw = (P->GetActorLocation() - GetActorLocation()).Rotation().Yaw;
	}
	if (bDance)
	{
		// the velcro party: bounce, sway, wave
		WantYaw = HomeYaw + FMath::Sin(Time * 1.7f) * 60.f;
		Pose.Speed = 200.f + 150.f * FMath::Sin(Time * 3.f);
		Pose.Attack = FMath::Fmod(Time * 0.6f, 1.f);
		Rig->SetRelativeLocation(FVector(0, 0, -CastChecked<UCapsuleComponent>(RootComponent)->GetScaledCapsuleHalfHeight() + FMath::Abs(FMath::Sin(Time * 6.f)) * 12.f));
	}
	SetActorRotation(FRotator(0, FMath::FixedTurn(GetActorRotation().Yaw, WantYaw, 240.f * Dt), 0));
	Rig->Animate(Dt, Pose);
}

// ---------------------------------------------------------------- Leo

namespace
{
	const TCHAR* LeoQuips[] = {
		TEXT("Do we have to stop somewhere? I'm getting hungry. Soup hungry."),
		TEXT("These hats are something else, huh Sparky?"),
		TEXT("I'm just along for the ride, buddy. Lead the way."),
		TEXT("Hey Sparky, do you think they have soup here? Space soup?"),
		TEXT("I could get used to velcro parties."),
		TEXT("You know, for a sketchy planet, the people are pretty nice."),
	};
}

ALeoBuddy::ALeoBuddy()
{
	PrimaryActorTick.bCanEverTick = true;
	GetCapsuleComponent()->InitCapsuleSize(34.f, 89.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);  // never in Sparky's way
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = 650.f;
	Move->bOrientRotationToMovement = true;
	Move->RotationRate = FRotator(0, 600.f, 0);
	Move->GravityScale = 2.4f;
	Move->bCanWalkOffLedges = false;
	bUseControllerRotationYaw = false;
	Rig = CreateDefaultSubobject<UCreatureRig>(TEXT("Rig"));
	Rig->SetupAttachment(RootComponent);
}

void ALeoBuddy::BeginPlay()
{
	Super::BeginPlay();
	if (ALevelGeo* Geo = ALevelGeo::Get(this))
	{
		GetCharacterMovement()->GravityScale = Geo->Theme.Gravity;
	}
	NextQuip = FMath::RandRange(0, 5);
}

void ALeoBuddy::Init(ECreature InKind, float InSide)
{
	Kind = InKind;
	Side = InSide;
	if (Kind == ECreature::Fluffernutter)
	{
		GetCapsuleComponent()->SetCapsuleSize(22.f, 22.f);
		GetCharacterMovement()->MaxWalkSpeed = 700.f;
	}
}

namespace
{
	/** What the friend says when things go quiet, by who and where. */
	TArray<FLine> Quips(ECreature Kind, int32 Chapter)
	{
		if (Kind == ECreature::Fluffernutter)
		{
			return {{EWho::Fluff, TEXT("*chirp!*")}, {EWho::Fluff, TEXT("*chirrup chirrup*")}, {EWho::Fluff, TEXT("*happy squeak*")},
			        {EWho::Fluff, TEXT("*sniff sniff... chirp?*")}};
		}
		switch (Chapter)
		{
		case 5:
			return {{EWho::Leo, TEXT("It's so green here. Like a salad, but you can walk in it.")},
			        {EWho::Leo, TEXT("Everyone's so friendly. And they have bowls! Bowls are great for soup.")},
			        {EWho::Leo, TEXT("I wonder what else lives in that forest...")}};
		case 7:
		case 8:
			return {{EWho::Leo, TEXT("Sparky, I just saw myself walk past. Except older. And with a beard.")},
			        {EWho::Leo, TEXT("Does time feel... stretchy to you?")},
			        {EWho::Leo, TEXT("I'm average in every timeline, aren't I. Just checking.")}};
		case 9:
			return {{EWho::Leo, TEXT("The air tastes like colours, Sparky. Is that normal?")},
			        {EWho::Leo, TEXT("I think that island just waved at me.")},
			        {EWho::Leo, TEXT("Fluffernutter loves it here. Look at him bounce!")}};
		default:
			return {{EWho::Leo, LeoQuips[0]}, {EWho::Leo, LeoQuips[1]}, {EWho::Leo, LeoQuips[2]}, {EWho::Leo, LeoQuips[3]},
			        {EWho::Leo, LeoQuips[4]}, {EWho::Leo, LeoQuips[5]}};
		}
	}
}

void ALeoBuddy::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bBuilt)
	{
		bBuilt = true;
		Rig->Build(Kind);
		Rig->SetBaseZ(-GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	}
	TalkTime = FMath::Max(0.f, TalkTime - Dt);
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	ASparkyGameMode* M = Mode(this);
	if (P && M && M->IsPlaying())
	{
		// trail a little to one side, so two friends don't stand in each other
		// (and a bit ahead of him, out of the camera's line of sight)
		const FVector Spot = P->GetActorLocation() + FRotator(0, P->GetCamYaw() + 90.f, 0).Vector() * (Side * 1.4f)
		                     + FRotator(0, P->GetCamYaw(), 0).Vector() * (Side != 0.f ? 120.f : 0.f);
		const FVector To = Spot - GetActorLocation();
		const float Dist = To.Size2D();
		if (Dist > 260.f)
		{
			AddMovementInput(FVector(To.X, To.Y, 0).GetSafeNormal(), FMath::Clamp((Dist - 200.f) / 300.f, 0.4f, 1.f));
			IdleTime = 0.f;
		}
		else
		{
			IdleTime += Dt;
		}
		const bool bStuck = (Dist > 450.f && GetVelocity().Size2D() < 80.f) || FMath::Abs(To.Z) > 350.f;
		StuckTime = bStuck ? StuckTime + Dt : 0.f;
		ALevelGeo* Geo = ALevelGeo::Get(this);
		// fell in: his middle is below the bog / out of the world (chapter 1's bog is just under the ground)
		const bool bFell = Geo && GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight() < Geo->Theme.KillZ;
		TeleportCooldown = FMath::Max(0.f, TeleportCooldown - Dt);
		if ((Dist > 2600.f || StuckTime > 3.f || bFell) && TeleportCooldown <= 0.f && P->GetCharacterMovement()->IsMovingOnGround())
		{
			TeleportCooldown = 2.f;
			// "I took a shortcut." Poof, there he is.
			AFx::Spawn(GetWorld(), EFx::Poof, GetActorLocation(), FLinearColor(0.8f, 0.8f, 1.f));
			// beside Sparky (not between him and the camera), somewhere with ground under it
			const FVector Side = FRotator(0, P->GetCamYaw() + 90.f, 0).Vector();
			const FVector Back = -FRotator(0, P->GetCamYaw(), 0).Vector();
			FVector Behind = P->GetActorLocation() + FVector(0, 0, 60.f);
			for (const FVector& Off : {Side * 220.f, Side * -220.f, (Side + Back) * 160.f, (Back - Side) * 160.f, Back * 200.f})
			{
				const FVector Try = P->GetActorLocation() + Off + FVector(0, 0, 60.f);
				FHitResult Hit;
				FCollisionQueryParams Q(SCENE_QUERY_STAT(LeoSpot), false, this);
				Q.AddIgnoredActor(P);
				if (GetWorld()->LineTraceSingleByChannel(Hit, Try, Try - FVector(0, 0, 400.f), ECC_Visibility, Q) && !GetWorld()->LineTraceTestByChannel(P->GetActorLocation(), Try, ECC_Visibility, Q))
				{
					Behind = Try;
					break;
				}
			}
			SetActorLocation(Behind, false, nullptr, ETeleportType::TeleportPhysics);
			GetCharacterMovement()->Velocity = FVector::ZeroVector;
			AFx::Spawn(GetWorld(), EFx::Poof, Behind, FLinearColor(0.8f, 0.8f, 1.f));
			ASparkyGameMode::Sfx(this, ESfx::Poof, Behind, 0.6f);
			StuckTime = 0.f;
		}
		if (IdleTime > 14.f)
		{
			IdleTime = -10.f;
			const TArray<FLine> Lines = Quips(Kind, M->GetChapter());
			const FLine& L = Lines[NextQuip % Lines.Num()];
			M->Radio(L.Who, L.Text);
			if (Kind == ECreature::Fluffernutter)
			{
				ASparkyGameMode::Sfx(this, ESfx::Chirp, GetActorLocation());
				IdleTime = -20.f;
			}
			++NextQuip;
			TalkTime = 3.f;
		}
	}
	FRigPose Pose;
	Pose.Speed = GetVelocity().Size2D();
	Pose.bAir = GetCharacterMovement()->IsFalling();
	Pose.VZ = GetVelocity().Z;
	Pose.Talk = TalkTime;
	Rig->Animate(Dt, Pose);
}

// ---------------------------------------------------------------- moving platforms

AMover::AMover()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->CastShadow = false;
	RootComponent = Mesh;
	Edge = PropMesh::Make(this, TEXT("Edge"), RootComponent);
}

void AMover::Init(const FVector& Half, const TCHAR* Tex, const FLinearColor& Color, EMove InMove, const FVector& InOffset,
                  float InPeriod, float InPhase, bool bGlowEdge)
{
	Move = InMove;
	Offset = InOffset;
	Period = FMath::Max(0.5f, InPeriod);
	Time = InPhase * Period;
	Start = GetActorLocation();
	StartRot = GetActorRotation();
	FRetroMesh M;
	const float TU = FMath::Max(1.f, FMath::RoundToFloat(Half.X / 128.f)), TV = FMath::Max(1.f, FMath::RoundToFloat(Half.Y / 128.f));
	M.Grid(FVector(-Half.X, -Half.Y, Half.Z), FVector(Half.X * 2, 0, 0), FVector(0, Half.Y * 2, 0), 2, 2, FVector(0, 0, 1), Color, FVector2D(0, 0), FVector2D(TU, TV));
	M.Box(FVector::ZeroVector, Half, Color * 0.8f, 16);
	PropMesh::Commit(this, Mesh, M, Tex, true);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);  // platforms drifting past mustn't make the camera jump
	if (bGlowEdge)
	{
		FRetroMesh G = GlowBuilder();
		const FLinearColor C(0.9f, 0.3f, 0.8f);
		G.Box(FVector(0, Half.Y, Half.Z), FVector(Half.X + 2, 3, 3), C);
		G.Box(FVector(0, -Half.Y, Half.Z), FVector(Half.X + 2, 3, 3), C);
		G.Box(FVector(Half.X, 0, Half.Z), FVector(3, Half.Y + 2, 3), C);
		G.Box(FVector(-Half.X, 0, Half.Z), FVector(3, Half.Y + 2, 3), C);
		G.Box(FVector(0, 0, -Half.Z - 6), FVector(Half.X * 0.5f, Half.Y * 0.5f, 3), C * 0.6f);
		PropMesh::CommitGlow(this, Edge, G);
	}
}

void AMover::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	const float T = 2 * PI * Time / Period;
	switch (Move)
	{
	case EMove::Linear:
		SetActorLocation(Start + Offset * (0.5f - 0.5f * FMath::Cos(T)));
		break;
	case EMove::Spin:
		SetActorRotation(StartRot + FRotator(0, 360.f * Time / Period, 0));
		break;
	case EMove::Bob:
		SetActorLocation(Start + Offset * FMath::Sin(T));
		break;
	case EMove::Sinker:
	{
		ASparkyCharacter* P = ASparkyCharacter::Get(this);
		const bool bOn = P && P->GetMovementBase() == Mesh;
		Sink = FMath::Clamp(Sink + (bOn ? Dt * 0.7f : -Dt * 0.5f), 0.f, 1.f);
		SetActorLocation(Start - FVector(0, 0, Sink * Offset.Z) + FVector(0, 0, FMath::Sin(T) * 6.f));
		break;
	}
	case EMove::Float:
		// rise as the gravity fails (chapter 4 sets the amount)
		Sink = FMath::FInterpTo(Sink, AMover::FloatAmount, Dt, 0.8f);
		SetActorLocation(Start + Offset * Sink + FVector(0, 0, FMath::Sin(T) * 15.f * Sink));
		SetActorRotation(StartRot + FRotator(FMath::Sin(T * 0.7f) * 4.f * Sink, 0, FMath::Cos(T * 0.6f) * 4.f * Sink));
		break;
	}
}

float AMover::FloatAmount = 0.f;

// ---------------------------------------------------------------- velcro

AVelcroWall::AVelcroWall()
{
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->CastShadow = false;
	RootComponent = Mesh;
}

void AVelcroWall::Init(const FVector& Half)
{
	FRetroMesh M;
	ALevelGeo* Geo = ALevelGeo::Get(this);
	if (Geo)
	{
		const FVector Base = GetActorLocation();
		M.Light = [Geo, Base](const FVector& P, const FVector& N) { return Geo->LightAt(P + Base, N); };
	}
	const FLinearColor C(1.f, 1.f, 1.f);
	const FVector X(Half.X, 0, 0), Y(0, Half.Y, 0), Z(0, 0, Half.Z);
	auto Face = [&](const FVector& A, const FVector& U, const FVector& V, const FVector& N, float LU, float LV)
	{
		M.Grid(A, U, V, FMath::Clamp(FMath::CeilToInt(LU / 200.f), 1, 12), FMath::Clamp(FMath::CeilToInt(LV / 200.f), 1, 12), N, C,
		       FVector2D(0, 0), FVector2D(LU / 128.f, LV / 128.f));
	};
	Face(X - Y + Z, Y * 2, -Z * 2, FVector(1, 0, 0), Half.Y * 2, Half.Z * 2);
	Face(-X + Y + Z, -Y * 2, -Z * 2, FVector(-1, 0, 0), Half.Y * 2, Half.Z * 2);
	Face(X + Y + Z, -X * 2, -Z * 2, FVector(0, 1, 0), Half.X * 2, Half.Z * 2);
	Face(-X - Y + Z, X * 2, -Z * 2, FVector(0, -1, 0), Half.X * 2, Half.Z * 2);
	Face(-X - Y + Z, X * 2, Y * 2, FVector(0, 0, 1), Half.X * 2, Half.Y * 2);
	PropMesh::Commit(this, Mesh, M, TEXT("T_Velcro"), true);
}

// ---------------------------------------------------------------- hazards

AHazard::AHazard()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
	Mesh = PropMesh::Make(this, TEXT("Mesh"), RootComponent);
}

void AHazard::Init(EHazard InType, const FVector& InOffset, float InPeriod, float InPhase)
{
	Type = InType;
	Offset = InOffset;
	Period = InPeriod;
	Time = InPhase * Period;
	FRetroMesh G = GlowBuilder();
	FRetroMesh M;
	switch (Type)
	{
	case EHazard::Laser:
		// a target ring on the hull
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = 2 * PI * i / 16;
			G.Box(FVector(FMath::Cos(A) * 160, FMath::Sin(A) * 160, 4), FVector(4, 30, 3), FRotator(0, FMath::RadiansToDegrees(A), 0), FLinearColor(1.f, 0.15f, 0.1f));
		}
		G.Box(FVector(0, 0, 4), FVector(60, 5, 3), FLinearColor(1.f, 0.15f, 0.1f));
		G.Box(FVector(0, 0, 4), FVector(5, 60, 3), FLinearColor(1.f, 0.15f, 0.1f));
		GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
		ASparkyGameMode::Sfx(this, ESfx::LaserWarn, GetActorLocation(), 0.8f);
		break;
	case EHazard::Sparks:
		M.Box(FVector::ZeroVector, FVector(18, 18, 18), DarkSteel);
		M.Box(Offset, FVector(18, 18, 18), DarkSteel);
		M.Box(FVector(0, 0, 0), FVector(8, 8, 22), FLinearColor(0.85f, 0.7f, 0.1f));
		M.Box(Offset, FVector(8, 8, 22), FLinearColor(0.85f, 0.7f, 0.1f));
		PropMesh::Commit(this, Mesh, M);
		RebuildSparks();
		break;
	case EHazard::Steam:
		M.Box(FVector(0, 0, 4), FVector(80, 80, 4), DarkSteel);
		for (int32 i = -3; i <= 3; ++i)
		{
			M.Box(FVector(i * 20.f, 0, 9), FVector(3, 70, 2), Steel);
		}
		PropMesh::Commit(this, Mesh, M);
		for (int32 i = 0; i < 10; ++i)
		{
			const float Z = Offset.Z * (i + 0.5f) / 10.f;
			const float R = 50.f + i * 6.f;
			G.Sphere(FVector(FMath::Sin(i * 2.1f) * 20.f, FMath::Cos(i * 1.7f) * 20.f, Z), FVector(R, R, R * 0.8f), 3, 6, FLinearColor(0.35f, 0.38f, 0.42f) * (1.f - i * 0.07f));
		}
		GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
		break;
	case EHazard::Rift:
		G.Sphere(FVector::ZeroVector, FVector(40.f), 4, 8, FLinearColor(0.6f, 0.3f, 1.f));
		G.Sphere(FVector::ZeroVector, FVector(22.f), 3, 6, FLinearColor(1.f, 0.9f, 1.f));
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector D = FRotator(i * 50.f, i * 60.f, 0).Vector();
			G.Box(D * 45.f, FVector(22, 2, 2), D.Rotation(), FLinearColor(0.5f, 0.8f, 1.f));
		}
		GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
		break;
	case EHazard::Gravity:
		G.Sphere(FVector::ZeroVector, FVector(Offset.X), 8, 12, FLinearColor(0.12f, 0.06f, 0.2f));
		for (int32 i = 0; i < 12; ++i)
		{
			G.Diamond(FRotator(i * 30.f, i * 97.f, 0).Vector() * Offset.X * 0.7f, 10.f, FLinearColor(0.6f, 0.4f, 1.f));
		}
		GlowMid = PropMesh::CommitGlow(this, GlowMesh, G, FLinearColor::White, 0.6f);
		break;
	case EHazard::Lift:
		M.Prism(FVector(0, 0, 0), 120.f, 20.f, 10, Steel, 100.f);
		PropMesh::Commit(this, Mesh, M);
		G.Prism(FVector(0, 0, 20), 95.f, Offset.Z, 10, FLinearColor(0.15f, 0.2f, 0.3f), 80.f);
		for (int32 i = 0; i < 8; ++i)
		{
			const float Z = Offset.Z * (i + 0.5f) / 8.f;
			for (int32 k = 0; k < 10; ++k)
			{
				const float A = 2 * PI * k / 10;
				G.Box(FVector(FMath::Cos(A) * 90.f, FMath::Sin(A) * 90.f, Z), FVector(3, 22, 3), FRotator(0, FMath::RadiansToDegrees(A) + 90.f, 0), FLinearColor(0.6f, 0.9f, 1.f));
			}
		}
		GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
		break;
	case EHazard::Shockwave:
		for (int32 i = 0; i < 32; ++i)
		{
			const float A = 2 * PI * i / 32;
			G.Box(FVector(FMath::Cos(A), FMath::Sin(A), 0) * 100.f + FVector(0, 0, 20), FVector(4, 11, 20), FRotator(0, FMath::RadiansToDegrees(A), 0), FLinearColor(1.f, 0.6f, 0.2f));
		}
		GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
		break;
	}
}

void AHazard::RebuildSparks()
{
	FRetroMesh G = GlowBuilder();
	if (bOn)
	{
		FVector Prev = FVector::ZeroVector;
		const int32 N = 8;
		for (int32 i = 1; i <= N; ++i)
		{
			FVector P = Offset * (float(i) / N);
			if (i < N)
			{
				P += FMath::VRand() * 22.f;
			}
			const FVector D = P - Prev;
			G.Box((P + Prev) * 0.5f, FVector(D.Size() * 0.5f, 3, 3), D.Rotation(), FLinearColor(0.6f, 0.85f, 1.f));
			Prev = P;
		}
		G.Diamond(FVector::ZeroVector, 26.f, FLinearColor(0.4f, 0.6f, 1.f));
		G.Diamond(Offset, 26.f, FLinearColor(0.4f, 0.6f, 1.f));
	}
	else
	{
		G.Diamond(FVector::ZeroVector, 8.f, FLinearColor(0.2f, 0.25f, 0.5f));
		G.Diamond(Offset, 8.f, FLinearColor(0.2f, 0.25f, 0.5f));
	}
	if (!GlowMid)
	{
		GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
	}
	else
	{
		for (FLinearColor& Col : G.Colors)
		{
			Col = Col * 2.f;
			Col.A = 1.f;
		}
		GlowMesh->ClearAllMeshSections();
		G.Commit(GlowMesh, 0, false);
	}
}

void AHazard::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!Playing(this))
	{
		return;
	}
	Time += Dt;
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	const FVector Me = GetActorLocation();
	switch (Type)
	{
	case EHazard::Laser:
	{
		const float Warn = 1.4f;
		if (Time < Warn)
		{
			const float K = Time / Warn;
			GlowMesh->SetRelativeScale3D(FVector(1.3f - 0.3f * K, 1.3f - 0.3f * K, 1.f));
			GlowMesh->SetRelativeRotation(FRotator(0, Time * 120.f, 0));
			GlowMid->SetScalarParameterValue("Intensity", 0.5f + 0.5f * FMath::Abs(FMath::Sin(Time * (8.f + 10.f * K))));
		}
		else if (!bOn && !bDone)
		{
			bOn = true;
			// the beam: a tall column, then gone
			FRetroMesh G = GlowBuilder();
			G.Prism(FVector(0, 0, 0), 120.f, 6000.f, 8, FLinearColor(1.f, 0.25f, 0.2f), 60.f);
			G.Prism(FVector(0, 0, 0), 50.f, 6000.f, 6, FLinearColor(1.f, 0.9f, 0.8f));
			GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
			GlowMesh->SetRelativeScale3D(FVector(1.f));
			GlowMesh->SetRelativeRotation(FRotator(-25.f, 0, 0));  // slanting in from the pirate ship
			AFx::Spawn(GetWorld(), EFx::Burst, Me + FVector(0, 0, 30), FLinearColor(1.f, 0.4f, 0.2f), 3.f);
			ASparkyGameMode::Sfx(this, ESfx::LaserBlast, Me);
			if (P && FVector::Dist2D(Feet(P), Me) < 190.f && Feet(P).Z < Me.Z + 250.f)
			{
				P->Hurt(1, Me);
			}
			if (P)
			{
				P->Shake(FMath::Clamp(1.f - FVector::Dist(P->GetActorLocation(), Me) / 2000.f, 0.f, 1.f) * 12.f);
			}
		}
		else if (bOn)
		{
			GlowMid->SetScalarParameterValue("Intensity", FMath::Max(0.f, 1.f - (Time - Warn) / 0.4f));
			if (Time > Warn + 0.4f)
			{
				bOn = false;
				bDone = true;
				Destroy();
			}
		}
		break;
	}
	case EHazard::Sparks:
	{
		const bool bWas = bOn;
		bOn = FMath::Fmod(Time, Period) < Period * 0.5f;
		if (bOn != bWas && bOn)
		{
			ASparkyGameMode::Sfx(this, ESfx::Zap, Me + Offset * 0.5f, 0.7f);
		}
		static float Flicker = 0.f;
		Flicker += Dt;
		if (bOn != bWas || (bOn && Flicker > 0.06f))
		{
			Flicker = 0.f;
			RebuildSparks();
		}
		if (bOn && P && SegDist(P->GetActorLocation(), Me, Me + Offset) < 85.f)
		{
			P->Hurt(1, Me + Offset * 0.5f);
		}
		break;
	}
	case EHazard::Steam:
	{
		const bool bWas = bOn;
		bOn = FMath::Fmod(Time, Period) < Period * 0.6f;
		if (bOn && !bWas)
		{
			ASparkyGameMode::Sfx(this, ESfx::Steam, Me, 0.6f);
		}
		const float Target = bOn ? 1.f : 0.f;
		const float Cur = GlowMesh->GetRelativeScale3D().Z;
		const float S = FMath::FInterpTo(Cur, Target, Dt, bOn ? 10.f : 4.f);
		GlowMesh->SetRelativeScale3D(FVector(FMath::Max(S, 0.01f)));
		GlowMesh->SetRelativeRotation(FRotator(0, Time * 40.f, 0));
		GlowMesh->SetVisibility(S > 0.02f);
		if (bOn && P)
		{
			const FVector F = Feet(P);
			if (FVector::Dist2D(F, Me) < 110.f && F.Z > Me.Z - 20.f && F.Z < Me.Z + Offset.Z)
			{
				P->Bounce(1300.f, false);
			}
		}
		break;
	}
	case EHazard::Rift:
	{
		const float K = 0.5f - 0.5f * FMath::Cos(2 * PI * Time / Period);
		GlowMesh->SetRelativeLocation(Offset * K);
		GlowMesh->SetRelativeRotation(FRotator(Time * 90.f, Time * 140.f, 0));
		GlowMid->SetScalarParameterValue("Intensity", 0.7f + 0.3f * FMath::Sin(Time * 17.f));
		if (P && FVector::Dist(P->GetActorLocation(), GlowMesh->GetComponentLocation()) < 85.f)
		{
			P->Hurt(1, GlowMesh->GetComponentLocation());
		}
		break;
	}
	case EHazard::Gravity:
		GlowMesh->SetRelativeRotation(FRotator(0, Time * 12.f, 0));
		if (P && FVector::Dist(P->GetActorLocation(), Me) < Offset.X)
		{
			P->SetZoneGravity(0.3f);
		}
		break;
	case EHazard::Lift:
	{
		GlowMid->SetScalarParameterValue("Intensity", 0.8f + 0.2f * FMath::Sin(Time * 4.f));
		GlowMesh->SetRelativeRotation(FRotator(0, Time * 30.f, 0));
		if (P)
		{
			const FVector F = Feet(P);
			if (FVector::Dist2D(F, Me) < 110.f && F.Z > Me.Z - 20.f && F.Z < Me.Z + Offset.Z)
			{
				P->Bounce(750.f, false);
			}
		}
		break;
	}
	case EHazard::Shockwave:
	{
		const float R = 100.f + 800.f * Time;
		GlowMesh->SetRelativeScale3D(FVector(R / 100.f, R / 100.f, 1.f));
		GlowMid->SetScalarParameterValue("Intensity", FMath::Clamp(1.6f - Time, 0.f, 1.f));
		if (P && !bDone)
		{
			const FVector F = Feet(P);
			if (FMath::Abs(FVector::Dist2D(F, Me) - R) < 55.f && F.Z < Me.Z + 45.f)
			{
				P->Hurt(1, Me);
				bDone = true;
			}
		}
		if (Time > 1.6f)
		{
			Destroy();
		}
		break;
	}
	}
}

// ---------------------------------------------------------------- checkpoints

ACheckpoint::ACheckpoint()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = PropMesh::Make(this, TEXT("Mesh"), RootComponent);
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

void ACheckpoint::Init(const TCHAR* InTex)
{
	FRetroMesh M;
	M.Prism(FVector(0, 0, 0), 30.f, 10.f, 8, DarkSteel, 24.f);
	M.Prism(FVector(0, 0, 10), 5.f, 150.f, 6, Steel);
	M.Prism(FVector(0, 0, 160), 16.f, 6.f, 8, Brass);
	// a little paw-print flag
	M.Box(FVector(30, 0, 140), FVector(28, 1.5f, 18), FLinearColor(0.2f, 0.5f, 0.9f));
	M.Box(FVector(32, -2, 136), FVector(5, 1, 5), White);
	for (int32 i = 0; i < 3; ++i)
	{
		M.Box(FVector(24 + i * 7.f, -2, 146), FVector(2, 1, 2.5f), White);
	}
	PropMesh::Commit(this, Mesh, M, InTex);
	FRetroMesh G = GlowBuilder();
	G.Diamond(FVector(0, 0, 182), 18.f, FLinearColor(0.3f, 0.9f, 1.f));
	GlowMid = PropMesh::CommitGlow(this, GlowMesh, G, FLinearColor::White, 0.25f);
}

void ACheckpoint::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	GlowMesh->SetRelativeRotation(FRotator(0, Time * (bLit ? 200.f : 40.f), 0));
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	if (!bLit && P && Playing(this) && FVector::Dist(P->GetActorLocation(), GetActorLocation() + FVector(0, 0, 50)) < 170.f)
	{
		bLit = true;
		GlowMid->SetScalarParameterValue("Intensity", 1.2f);
		P->SetCheckpoint(GetActorLocation() + GetActorForwardVector() * 120.f + FVector(0, 0, 60), GetActorRotation().Yaw);
		AFx::Spawn(GetWorld(), EFx::Burst, GetActorLocation() + FVector(0, 0, 180), FLinearColor(0.3f, 0.9f, 1.f), 1.5f);
		if (ASparkyGameMode* M = Mode(this))
		{
			M->OnCheckpoint();
		}
	}
}

// ---------------------------------------------------------------- goals

AGoal::AGoal()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

void AGoal::Init(FName InId, float InRadius, bool bInActive)
{
	Id = InId;
	Radius = InRadius;
	FRetroMesh G = GlowBuilder();
	for (int32 i = 0; i < 24; ++i)
	{
		const float A = 2 * PI * i / 24;
		G.Box(FVector(FMath::Cos(A), FMath::Sin(A), 0) * Radius + FVector(0, 0, 6), FVector(6, Radius * 0.13f, 4), FRotator(0, FMath::RadiansToDegrees(A), 0), FLinearColor(1.f, 0.85f, 0.3f));
		if (i % 3 == 0)
		{
			G.Diamond(FVector(FMath::Cos(A), FMath::Sin(A), 0) * Radius + FVector(0, 0, 60), 10.f, FLinearColor(1.f, 0.9f, 0.5f));
		}
	}
	G.Prism(FVector(0, 0, 0), Radius * 0.9f, 400.f, 12, FLinearColor(0.25f, 0.2f, 0.05f), Radius * 0.6f);
	GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
	SetActive(bInActive);
}

void AGoal::SetActive(bool bOn)
{
	bActive = bOn;
	SetActorHiddenInGame(!bOn);
}

void AGoal::Tick(float Dt)
{
	Super::Tick(Dt);
	if (!bActive)
	{
		return;
	}
	Time += Dt;
	GlowMesh->SetRelativeRotation(FRotator(0, Time * 45.f, 0));
	GlowMid->SetScalarParameterValue("Intensity", 0.75f + 0.25f * FMath::Sin(Time * 4.f));
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	if (P && Playing(this) && !P->IsDown() && FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) < Radius
	    && FMath::Abs(Feet(P).Z - GetActorLocation().Z) < 200.f)
	{
		SetActive(false);
		if (ASparkyGameMode* M = Mode(this))
		{
			M->OnGoal(this);
		}
	}
}

// ---------------------------------------------------------------- repairs

ARepairPoint::ARepairPoint()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = PropMesh::Make(this, TEXT("Mesh"), RootComponent);
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

void ARepairPoint::Init(FName InId, bool bInBig)
{
	Id = InId;
	bBig = bInBig;
	FRetroMesh M;
	if (bBig)
	{
		// the climate control module: a big humming box with a fan and gauges
		M.Box(FVector(0, 0, 150), FVector(150, 110, 150), FLinearColor(0.55f, 0.6f, 0.65f));
		M.Box(FVector(151, 0, 190), FVector(2, 80, 80), DarkSteel);
		for (int32 i = 0; i < 6; ++i)
		{
			M.Box(FVector(154, 0, 190), FVector(2, 75, 8), FRotator(0, 0, i * 30.f), Steel);
		}
		M.Box(FVector(152, -70, 60), FVector(3, 25, 25), FLinearColor(0.9f, 0.85f, 0.7f));
		M.Box(FVector(152, 70, 60), FVector(3, 25, 25), FLinearColor(0.9f, 0.85f, 0.7f));
		M.Box(FVector(0, 0, 305), FVector(120, 90, 6), FLinearColor(0.85f, 0.7f, 0.1f));
	}
	else
	{
		// a conduit with a burnt-out break in it
		M.Box(FVector(0, 0, 25), FVector(40, 40, 25), DarkSteel);
		M.PrismX(FVector(-120, 0, 70), 14.f, 100.f, 8, Steel);
		M.PrismX(FVector(20, 0, 70), 14.f, 100.f, 8, Steel);
		M.Box(FVector(0, 0, 70), FVector(14, 22, 22), FLinearColor(0.85f, 0.7f, 0.1f));
	}
	PropMesh::Commit(this, Mesh, M);
	FRetroMesh G = GlowBuilder();
	const FVector Hot = bBig ? FVector(155, 0, 190) : FVector(0, 0, 70);
	G.Diamond(Hot, bBig ? 50.f : 26.f, FLinearColor(1.f, 0.5f, 0.15f));
	GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
}

ARepairPoint* ARepairPoint::FindNear(const ASparkyCharacter* P)
{
	if (!P)
	{
		return nullptr;
	}
	for (TActorIterator<ARepairPoint> It(P->GetWorld()); It; ++It)
	{
		if (!It->bFixed && It->bEnabled && FVector::Dist2D(It->GetActorLocation(), P->GetActorLocation()) < (It->bBig ? 300.f : 170.f)
		    && FMath::Abs(It->GetActorLocation().Z + (It->bBig ? 100.f : 40.f) - P->GetActorLocation().Z) < 160.f)
		{
			return *It;
		}
	}
	return nullptr;
}

void ARepairPoint::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	if (bFixed)
	{
		GlowMid->SetScalarParameterValue("Intensity", 0.6f);
		return;
	}
	// broken: flicker and spit sparks
	const float Fl = FMath::FRand() < 0.2f ? 0.1f : 1.f;
	GlowMid->SetScalarParameterValue("Intensity", (bEnabled ? 0.8f : 0.3f) * Fl * (0.6f + 0.4f * FMath::Sin(Time * 20.f)));
	const FVector Hot = GetActorLocation() + (bBig ? FVector(155, 0, 190) : FVector(0, 0, 70));
	SparkTimer -= Dt;
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	const bool bWelding = P && P->IsWelding() && FindNear(P) == this;
	if (SparkTimer <= 0.f)
	{
		SparkTimer = bWelding ? 0.12f : FMath::FRandRange(0.6f, 1.6f);
		AFx::Spawn(GetWorld(), EFx::Sparks, Hot, bWelding ? FLinearColor(0.6f, 0.85f, 1.f) : FLinearColor(1.f, 0.6f, 0.2f), bWelding ? 1.2f : 0.7f);
		if (bWelding)
		{
			ASparkyGameMode::Sfx(this, ESfx::Weld, Hot, 0.5f);
		}
	}
	if (bWelding && Playing(this))
	{
		Progress += Dt / (bBig ? 4.f : 2.2f);
		if (Progress >= 1.f)
		{
			Progress = 1.f;
			bFixed = true;
			FRetroMesh G = GlowBuilder();
			G.Diamond(Hot - GetActorLocation(), bBig ? 40.f : 18.f, FLinearColor(0.3f, 1.f, 0.4f));
			GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
			AFx::Spawn(GetWorld(), EFx::Burst, Hot, FLinearColor(0.3f, 1.f, 0.4f), 2.f);
			if (ASparkyGameMode* M = Mode(this))
			{
				M->OnRepaired(this);
			}
		}
	}
}

// ---------------------------------------------------------------- boarding pods

APod::APod()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->CastShadow = false;
	RootComponent = Mesh;
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

void APod::Init(const FVector& OutDir)
{
	Out = OutDir.GetSafeNormal();
	FRetroMesh M;
	const FLinearColor Hull(0.25f, 0.22f, 0.24f), Red(0.7f, 0.1f, 0.1f);
	M.Prism(FVector(0, 0, 0), 110.f, 30.f, 10, DarkSteel, 90.f);       // clamp ring on the hull
	M.Prism(FVector(0, 0, 30), 90.f, 160.f, 10, Hull, 80.f);
	M.Prism(FVector(0, 0, 80), 91.f, 20.f, 10, Red);
	M.Prism(FVector(0, 0, 190), 80.f, 30.f, 10, Hull, 60.f);
	M.Prism(FVector(0, 0, 220), 60.f, 12.f, 10, Brass);                 // the clamp: stomp here
	for (int32 i = 0; i < 4; ++i)
	{
		const float A = PI / 2 * i;
		M.Box(FVector(FMath::Cos(A) * 105.f, FMath::Sin(A) * 105.f, 15.f), FVector(30, 12, 22), FRotator(0, FMath::RadiansToDegrees(A), 0), Brass);
	}
	// dog-skull emblem
	M.Box(FVector(88, 0, 130), FVector(4, 26, 22), White);
	M.Box(FVector(91, -9, 134), FVector(2, 5, 5), Hull);
	M.Box(FVector(91, 9, 134), FVector(2, 5, 5), Hull);
	PropMesh::Commit(this, Mesh, M, TEXT("T_White"), true);
	FRetroMesh G = GlowBuilder();
	for (int32 i = 0; i < 6; ++i)
	{
		const float A = 2 * PI * i / 6;
		G.Diamond(FVector(FMath::Cos(A) * 50.f, FMath::Sin(A) * 50.f, 234.f), 8.f, FLinearColor(1.f, 0.2f, 0.1f));
	}
	PropMesh::CommitGlow(this, GlowMesh, G);
}

bool APod::TrySlam(const FVector& Where)
{
	const FVector Top = GetActorLocation() + FVector(0, 0, 232.f);
	if (bLoose || FVector::Dist2D(Where, Top) > 140.f || FMath::Abs(Where.Z - Top.Z) > 90.f)
	{
		return false;
	}
	bLoose = true;
	Vel = Out * 500.f + FVector(0, 0, 700.f);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AFx::Spawn(GetWorld(), EFx::Burst, Top, FLinearColor(1.f, 0.6f, 0.2f), 3.f);
	ASparkyGameMode::Sfx(this, ESfx::Explosion, Top);
	if (ASparkyCharacter* P = ASparkyCharacter::Get(this))
	{
		P->Bounce(1000.f);
		P->Shake(10.f);
	}
	if (ASparkyGameMode* M = Mode(this))
	{
		M->OnPodDetached(this);
	}
	return true;
}

void APod::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	GlowMesh->SetVisibility(FMath::Fmod(Time, 0.8f) < 0.5f);
	if (bLoose)
	{
		AddActorWorldOffset(Vel * Dt);
		AddActorWorldRotation(FRotator(90.f * Dt, 40.f * Dt, 70.f * Dt));
		if (Time > 30.f || GetActorLocation().Z > 6000.f)
		{
			Destroy();
		}
	}
}

// ---------------------------------------------------------------- pirate ship

APirateShip::APirateShip()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Mesh = PropMesh::Make(this, TEXT("Mesh"), RootComponent);
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

void APirateShip::BeginPlay()
{
	Super::BeginPlay();
	Home = GetActorLocation();
	FRetroMesh M;
	const FLinearColor Hull(0.22f, 0.16f, 0.14f), Trim(0.65f, 0.1f, 0.1f), Fur(0.72f, 0.55f, 0.35f);
	// a long galleon-shaped hull, nose (+X) a giant bulldog head
	M.Box(FVector(0, 0, 0), FVector(1400, 350, 220), Hull);
	M.Box(FVector(0, 0, 230), FVector(1300, 300, 20), FLinearColor(0.4f, 0.28f, 0.18f));
	M.Box(FVector(0, 0, -260), FVector(1100, 200, 60), Hull * 0.7f);
	M.Box(FVector(0, 352, 60), FVector(1380, 4, 30), Trim);
	M.Box(FVector(0, -352, 60), FVector(1380, 4, 30), Trim);
	M.Box(FVector(-1200, 0, 420), FVector(220, 320, 180), Hull);  // stern castle
	for (int32 i = 0; i < 8; ++i)
	{
		M.PrismX(FVector(-1000 + i * 260.f, -360, 0), 26.f, 0.f, 8, DarkSteel);
		M.Box(FVector(-1000 + i * 260.f, -380, 0), FVector(30, 40, 22), DarkSteel);  // cannon ports facing Sparky
	}
	// masts with sails (it's a space pirate ship; the sails are solar)
	for (int32 i = 0; i < 3; ++i)
	{
		const float X = -600.f + i * 600.f;
		M.Prism(FVector(X, 0, 250), 22.f, 1200.f - i * 150.f, 6, FLinearColor(0.35f, 0.25f, 0.15f));
		M.Box(FVector(X, 0, 800 - i * 70.f), FVector(10, 420, 280), FLinearColor(0.15f, 0.12f, 0.15f));
		M.Box(FVector(X - 12, 0, 820 - i * 70.f), FVector(2, 120, 100), White);  // dog skull and crossbones
		M.Box(FVector(X - 13, -40, 840 - i * 70.f), FVector(2, 20, 20), FLinearColor(0.15f, 0.12f, 0.15f));
		M.Box(FVector(X - 13, 40, 840 - i * 70.f), FVector(2, 20, 20), FLinearColor(0.15f, 0.12f, 0.15f));
	}
	// the bulldog figurehead
	M.Box(FVector(1550, 0, 80), FVector(220, 320, 260), Fur);
	M.Box(FVector(1800, 0, -20), FVector(80, 260, 140), Fur * 1.1f);
	M.Box(FVector(1885, 0, 60), FVector(10, 90, 60), Black);
	M.Box(FVector(1850, 0, -130), FVector(30, 220, 20), White);
	M.Box(FVector(1770, -150, 200), FVector(10, 50, 50), White);
	M.Box(FVector(1775, -150, 200), FVector(8, 25, 30), Black);
	M.Box(FVector(1770, 150, 200), FVector(10, 50, 50), Black);  // eye patch
	PropMesh::Commit(this, Mesh, M);
	FRetroMesh G = GlowBuilder();
	G.Box(FVector(-1640, 0, 0), FVector(40, 260, 160), FLinearColor(1.f, 0.4f, 0.15f));  // engines
	G.Box(FVector(-1720, 0, 0), FVector(40, 200, 120), FLinearColor(0.6f, 0.2f, 0.05f));
	for (int32 i = 0; i < 12; ++i)
	{
		G.Box(FVector(-1100 + i * 200.f, -356, 150), FVector(30, 3, 18), FLinearColor(1.f, 0.85f, 0.4f));  // portholes
	}
	PropMesh::CommitGlow(this, GlowMesh, G);
}

FVector APirateShip::TurretPos = FVector::ZeroVector;

void APirateShip::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	if (bGone)
	{
		AddActorWorldOffset(FVector(-12000.f * Dt, 0, 0));
		return;
	}
	SetActorLocation(Home + FVector(FMath::Sin(Time * 0.21f) * 600.f, FMath::Sin(Time * 0.13f) * 300.f, FMath::Sin(Time * 0.5f) * 120.f));
	SetActorRotation(FRotator(FMath::Sin(Time * 0.4f) * 2.f, 0, FMath::Sin(Time * 0.5f) * 4.f));
	if (!Playing(this) || TurretPos.IsZero())
	{
		return;
	}
	// Leo on the guns: tracers from the turret to the pirate hull
	ShotTimer -= Dt;
	if (ShotTimer <= 0.f)
	{
		ShotTimer = FMath::FRandRange(0.35f, 1.2f);
		const FVector Target = GetActorLocation() + FVector(FMath::FRandRange(-1200.f, 1200.f), -360.f, FMath::FRandRange(-150.f, 300.f));
		AFx::Spawn(GetWorld(), EFx::Tracer, TurretPos, FLinearColor(0.4f, 1.f, 0.5f), 1.f, Target);
		AFx::Spawn(GetWorld(), EFx::Burst, Target, FLinearColor(1.f, 0.6f, 0.2f), 3.f);
		ASparkyGameMode::Sfx(this, ESfx::Gun, TurretPos, 0.35f);
	}
}

// ---------------------------------------------------------------- wormholes

ATeleporter::ATeleporter()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

float ATeleporter::Cooldown = 0.f;

void ATeleporter::Init(FName InId, const FLinearColor& InColor)
{
	Id = InId;
	FRetroMesh G = GlowBuilder();
	for (int32 r = 0; r < 3; ++r)
	{
		const float Rad = 110.f - r * 30.f;
		for (int32 i = 0; i < 18; ++i)
		{
			const float A = 2 * PI * i / 18 + r * 0.4f;
			G.Box(FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, 8 + r * 4.f), FVector(4, Rad * 0.18f, 3), FRotator(0, FMath::RadiansToDegrees(A) + 90.f, 0), InColor * (1.f - r * 0.25f));
		}
	}
	G.Prism(FVector(0, 0, 0), 100.f, 260.f, 12, InColor * 0.25f, 40.f);
	PropMesh::CommitGlow(this, GlowMesh, G);
}

void ATeleporter::Tick(float Dt)
{
	Super::Tick(Dt);
	Time += Dt;
	GlowMesh->SetRelativeRotation(FRotator(0, Time * 120.f, 0));
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	const float Now = GetWorld()->GetTimeSeconds();
	if (!P || !Playing(this) || P->IsDown() || Now < Cooldown)
	{
		return;
	}
	if (FVector::Dist2D(P->GetActorLocation(), GetActorLocation()) < 100.f && FMath::Abs(Feet(P).Z - GetActorLocation().Z) < 120.f)
	{
		for (TActorIterator<ATeleporter> It(GetWorld()); It; ++It)
		{
			if (*It != this && It->Id == Id)
			{
				Cooldown = Now + 1.5f;
				AFx::Spawn(GetWorld(), EFx::Poof, P->GetActorLocation(), FLinearColor(0.7f, 0.5f, 1.f), 1.5f);
				P->PlaceAt(It->GetActorLocation() + It->GetActorForwardVector() * 170.f + FVector(0, 0, 80.f), It->GetActorRotation().Yaw);
				AFx::Spawn(GetWorld(), EFx::Poof, P->GetActorLocation(), FLinearColor(0.7f, 0.5f, 1.f), 1.5f);
				ASparkyGameMode::Sfx(this, ESfx::Portal);
				break;
			}
		}
	}
}

// ---------------------------------------------------------------- puzzle tiles

AFloorPad::AFloorPad()
{
	PrimaryActorTick.bCanEverTick = true;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	Mesh->CastShadow = false;
	RootComponent = Mesh;
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

void AFloorPad::Init(int32 InIndex, bool bInLit)
{
	Index = InIndex;
	FRetroMesh M;
	M.Box(FVector(0, 0, -10), FVector(85, 85, 10), FLinearColor(0.35f, 0.35f, 0.45f));
	PropMesh::Commit(this, Mesh, M, TEXT("T_Rune"), true);
	FRetroMesh G = GlowBuilder();
	G.Box(FVector(0, 0, 2), FVector(70, 70, 2), FLinearColor(0.3f, 0.9f, 1.f));
	G.Diamond(FVector(0, 0, 30), 14.f, FLinearColor(0.6f, 1.f, 1.f));
	GlowMid = PropMesh::CommitGlow(this, GlowMesh, G);
	SetLit(bInLit);
}

void AFloorPad::SetLit(bool bInLit)
{
	bLit = bInLit;
	GlowMesh->SetVisibility(bLit);
}

void AFloorPad::Tick(float Dt)
{
	Super::Tick(Dt);
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	if (!P || !Playing(this))
	{
		return;
	}
	const FVector D = Feet(P) - GetActorLocation();
	const bool bNow = FMath::Abs(D.X) < 85.f && FMath::Abs(D.Y) < 85.f && D.Z > -5.f && D.Z < 30.f;
	if (bNow && !bOn && !bLocked)
	{
		if (ASparkyGameMode* M = Mode(this))
		{
			M->OnPadStep(this);
		}
	}
	bOn = bNow;
}

// ---------------------------------------------------------------- effects

AFx::AFx()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	GlowMesh = PropMesh::Make(this, TEXT("Glow"), RootComponent);
}

AFx* AFx::Spawn(UWorld* W, EFx Type, const FVector& At, const FLinearColor& Color, float Size, const FVector& To)
{
	if (!W)
	{
		return nullptr;
	}
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AFx* F = W->SpawnActor<AFx>(AFx::StaticClass(), At, FRotator::ZeroRotator, P);
	if (!F)
	{
		return nullptr;
	}
	F->Type = Type;
	F->Size = Size;
	FRetroMesh G = GlowBuilder();
	FRandomStream R(FMath::Rand());
	switch (Type)
	{
	case EFx::Poof:
		F->Life = 0.5f;
		for (int32 i = 0; i < 7; ++i)
		{
			FVector D = R.GetUnitVector();
			D.Z = FMath::Abs(D.Z) * 0.5f;
			G.Diamond(D * 22.f * Size, 7.f * Size, Color * 0.3f);
		}
		break;
	case EFx::Burst:
		F->Life = 0.35f;
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector D = R.GetUnitVector();
			G.Box(D * 16.f * Size, FVector(12.f, 1.5f, 1.5f) * Size, D.Rotation(), Color);
		}
		G.Diamond(FVector::ZeroVector, 8.f * Size, Color * 0.6f);
		break;
	case EFx::Sparks:
		F->Life = 0.3f;
		for (int32 i = 0; i < 6; ++i)
		{
			G.Diamond(R.GetUnitVector() * R.FRandRange(5.f, 30.f) * Size, 4.f * Size, Color);
		}
		break;
	case EFx::Tracer:
	{
		F->Life = 0.18f;
		const FVector D = To - At;
		G.Box(D * 0.5f, FVector(D.Size() * 0.5f, 6.f, 6.f), D.Rotation(), Color);
		break;
	}
	case EFx::Splash:
		F->Life = 0.7f;
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2 * PI * i / 12;
			G.Diamond(FVector(FMath::Cos(A) * 40.f, FMath::Sin(A) * 40.f, 10.f) * Size, 14.f * Size, Color, 1.6f);
		}
		break;
	case EFx::Stars:
		F->Life = 1.f;
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = 2 * PI * i / 5;
			G.Diamond(FVector(FMath::Cos(A) * 30.f, FMath::Sin(A) * 30.f, 0) * Size, 7.f * Size, Color);
		}
		break;
	}
	F->Mid = PropMesh::CommitGlow(F, F->GlowMesh, G);
	return F;
}

void AFx::Tick(float Dt)
{
	Super::Tick(Dt);
	Age += Dt;
	const float K = FMath::Clamp(Age / Life, 0.f, 1.f);
	switch (Type)
	{
	case EFx::Poof:
		GlowMesh->SetRelativeScale3D(FVector(1.f + 1.2f * K));
		AddActorWorldOffset(FVector(0, 0, 40.f * Dt));
		break;
	case EFx::Burst:
		GlowMesh->SetRelativeScale3D(FVector(1.f + 2.5f * K));
		break;
	case EFx::Sparks:
		GlowMesh->SetRelativeScale3D(FVector(1.f + 3.f * K));
		AddActorWorldOffset(FVector(0, 0, -200.f * Dt));
		break;
	case EFx::Splash:
		GlowMesh->SetRelativeScale3D(FVector(1.f + K, 1.f + K, 1.f + 3.f * FMath::Sin(K * PI)));
		break;
	case EFx::Stars:
		GlowMesh->SetRelativeRotation(FRotator(0, Age * 300.f, 0));
		break;
	default:
		break;
	}
	if (Mid)
	{
		Mid->SetScalarParameterValue("Intensity", 1.f - K);
	}
	if (Age >= Life)
	{
		Destroy();
	}
}
