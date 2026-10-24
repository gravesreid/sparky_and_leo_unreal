#include "CreatureRig.h"
#include "Assets.h"
#include "LevelGeo.h"
#include "RetroMesh.h"

#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

namespace
{
	const FLinearColor White(0.92f, 0.92f, 0.9f);
	const FLinearColor Black(0.06f, 0.06f, 0.07f);
	const FLinearColor Pink(0.95f, 0.55f, 0.6f);
	const FLinearColor Brass(0.85f, 0.65f, 0.25f);
	const FLinearColor Chrome(0.75f, 0.78f, 0.82f);
	const FLinearColor Styro(0.97f, 0.97f, 0.93f);
	const FLinearColor Cyan(0.3f, 0.95f, 1.f);
}

UCreatureRig::UCreatureRig()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCreatureRig::MakePart(EPart P, const FVector& InPivot, const FRetroMesh& M)
{
	if (Parts.Num() < int32(EPart::Count))
	{
		Parts.SetNumZeroed(int32(EPart::Count));
	}
	UProceduralMeshComponent*& C = Parts[int32(P)];
	if (!C)
	{
		C = NewObject<UProceduralMeshComponent>(GetOwner());
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->CastShadow = false;
		C->SetupAttachment(P == EPart::Hat && Part(EPart::Head) ? Part(EPart::Head) : static_cast<USceneComponent*>(this));
		C->RegisterComponent();
	}
	Pivot[int32(P)] = InPivot;
	C->SetRelativeLocation(InPivot);
	C->ClearAllMeshSections();
	M.Commit(C, 0, false);
	C->SetMaterial(0, Mid);
}

void UCreatureRig::Build(ECreature InKind, int32 InVariant)
{
	Kind = InKind;
	Variant = InVariant;
	if (!Mid)
	{
		ALevelGeo* Geo = ALevelGeo::Get(this);
		Mid = Geo ? Geo->MakeMaterial(GetOwner(), TEXT("T_White")) : UMaterialInstanceDynamic::Create(Assets::RetroMaterial(), GetOwner());
	}
	for (FVector& P : Pivot)
	{
		P = FVector::ZeroVector;
	}
	// rebuilding as something else: clear out parts the new body doesn't have (a tail, a hat, a prop)
	for (UProceduralMeshComponent* C : Parts)
	{
		if (C)
		{
			C->ClearAllMeshSections();
			C->SetRelativeTransform(FTransform::Identity);
		}
	}
	bHatOn = false;
	switch (Kind)
	{
	case ECreature::Sparky:
		BuildCat(1.3f, FLinearColor(0.52f, 0.56f, 0.64f), FLinearColor(0.3f, 0.33f, 0.4f), White, true, false);
		break;
	case ECreature::AlleyCat:
	{
		static const FLinearColor Furs[] = {FLinearColor(0.85f, 0.45f, 0.15f), FLinearColor(0.14f, 0.13f, 0.15f), FLinearColor(0.55f, 0.42f, 0.3f), FLinearColor(0.75f, 0.72f, 0.65f)};
		static const FLinearColor Stripes[] = {FLinearColor(0.6f, 0.28f, 0.08f), FLinearColor(0.05f, 0.05f, 0.06f), FLinearColor(0.3f, 0.2f, 0.12f), FLinearColor(0.35f, 0.3f, 0.25f)};
		const int32 V = Variant % 4;
		BuildCat(1.05f, Furs[V], Stripes[V], FLinearColor(0.8f, 0.75f, 0.7f), false, true);
		break;
	}
	case ECreature::Meatball:
		BuildCat(2.3f, FLinearColor(0.66f, 0.52f, 0.4f), FLinearColor(0.42f, 0.32f, 0.25f), FLinearColor(0.85f, 0.78f, 0.7f), false, true);
		break;
	case ECreature::Leo:
		BuildHuman(true, false);
		break;
	case ECreature::Local:
		BuildHuman(false, false);
		break;
	case ECreature::Elder:
		BuildHuman(false, true);
		break;
	case ECreature::PirateDog:
		BuildDog(false);
		break;
	case ECreature::Captain:
		BuildDog(true);
		break;
	case ECreature::BogBlob:
		BuildBlob();
		break;
	case ECreature::Villager:
		BuildHuman(false, false);
		break;
	case ECreature::Fluffernutter:
		BuildFluff(false);
		break;
	case ECreature::FluffBig:
		BuildFluff(true);
		break;
	case ECreature::Being:
		BuildBeing();
		break;
	}
	if (Kind == ECreature::Local || Kind == ECreature::Elder)
	{
		SetHat(true);
	}
}

// ---------------------------------------------------------------- cats

void UCreatureRig::BuildCat(float S, const FLinearColor& Fur, const FLinearColor& Stripe, const FLinearColor& Light, bool bSparky, bool bScarred)
{
	Height = 62.f * S;
	FRetroMesh M;
	auto V = [S](float X, float Y, float Z) { return FVector(X, Y, Z) * S; };

	// body (pivot at the middle of the back)
	M.Box(V(0, 0, 0), V(24, 12, 11), Fur);
	M.Box(V(0, 0, -5), V(20, 10.5f, 7), Light);
	for (float X : {-12.f, -2.f, 8.f})
	{
		M.Box(V(X, 0, 2), V(2.2f, 12.4f, 9.6f), Stripe);
	}
	if (bSparky)
	{
		M.Box(V(21, 0, 3), V(3, 12.6f, 11.6f), FLinearColor(0.95f, 0.8f, 0.15f));  // collar
		M.Diamond(V(25, 0, -6), 3.f * S, Cyan);                                     // tag
	}
	MakePart(EPart::Body, V(0, 0, 30), M);

	// head
	M.Reset();
	M.Box(V(6, 0, 4), V(10, 11, 9), Fur);
	M.Box(V(15, 0, 0), V(4, 6.5f, 4), Light);
	M.Box(V(19.2f, 0, 2.5f), V(0.8f, 2, 1.5f), Pink);
	for (float Y : {-5.f, 5.f})
	{
		M.Box(V(15.5f, Y, 6), V(1, 3.2f, 3.2f), White);
		M.Box(V(16.6f, Y, 6), V(0.4f, 1.4f, 2.6f), bSparky ? FLinearColor(0.2f, 0.75f, 0.35f) : FLinearColor(0.85f, 0.75f, 0.1f));
		M.Prism(V(3, Y * 1.4f, 12), 5.f * S, 9.f * S, 4, Fur, 0.f);
		M.Prism(V(3.8f, Y * 1.4f, 12.5f), 3.f * S, 6.f * S, 4, Pink, 0.f);
		M.Box(V(17, Y * 1.7f, 0.5f), V(0.3f, 5.5f, 0.3f), White);  // whiskers
	}
	if (bSparky)
	{
		// the genius's goggles, pushed up on his forehead
		M.Box(V(6, 0, 11.5f), V(10.4f, 11.4f, 1.6f), FLinearColor(0.35f, 0.22f, 0.12f));
		for (float Y : {-5.f, 5.f})
		{
			M.PrismX(V(14.5f, Y, 12.5f), 3.6f * S, 2.5f * S, 8, Brass);
			M.PrismX(V(16.8f, Y, 12.5f), 2.6f * S, 0.4f * S, 8, Cyan);
		}
	}
	if (bScarred)
	{
		M.Box(V(16, -4, 9), V(0.5f, 0.6f, 4), Pink);  // scar over one eye
	}
	MakePart(EPart::Head, V(24, 0, 40), M);

	// legs (pivot at the hip / shoulder)
	M.Reset();
	M.Box(V(0, 0, -11), V(4, 4, 12), Fur);
	M.Box(V(2, 0, -22), V(5.5f, 4.6f, 2.5f), Light);
	MakePart(EPart::LegFL, V(16, 8, 26), M);
	MakePart(EPart::LegFR, V(16, -8, 26), M);
	MakePart(EPart::LegBL, V(-16, 8, 26), M);
	MakePart(EPart::LegBR, V(-16, -8, 26), M);

	// tail: a curl of boxes rising behind
	M.Reset();
	const FVector Seg[] = {V(-5, 0, 3), V(-10, 0, 10), V(-12, 0, 19), V(-11, 0, 28)};
	const float Pitch[] = {-30.f, -55.f, -80.f, -100.f};
	for (int32 i = 0; i < 4; ++i)
	{
		M.Box(Seg[i], V(5, 3, 3), FRotator(Pitch[i], 0, 0), i == 3 ? Stripe : Fur);
	}
	MakePart(EPart::Tail, V(-24, 0, 34), M);
}

// ---------------------------------------------------------------- people

void UCreatureRig::BuildHuman(bool bLeo, bool bElder)
{
	FRetroMesh M;
	if (bLeo)
	{
		Height = 178.f;
		const FLinearColor Skin(0.93f, 0.74f, 0.6f), Shirt(0.5f, 0.55f, 0.6f), Jeans(0.25f, 0.33f, 0.52f), Hair(0.38f, 0.25f, 0.15f);
		// legs
		M.Box(FVector(0, 0, -44), FVector(8, 8, 44), Jeans);
		M.Box(FVector(4, 0, -86), FVector(12, 8.5f, 4), FLinearColor(0.85f, 0.85f, 0.85f));  // sneakers
		MakePart(EPart::LegFL, FVector(0, 10, 90), M);
		MakePart(EPart::LegFR, FVector(0, -10, 90), M);
		// body: a plain t-shirt, the most average shirt in the galaxy
		M.Reset();
		M.Box(FVector(0, 0, 4), FVector(11, 19, 5), FLinearColor(0.3f, 0.25f, 0.2f));  // belt
		M.Box(FVector(0, 0, 34), FVector(12, 21, 27), Shirt);
		M.Box(FVector(0, 0, 63), FVector(5, 5, 3), Skin);
		MakePart(EPart::Body, FVector(0, 0, 88), M);
		// arms (BL = left, BR = right)
		M.Reset();
		M.Box(FVector(0, 0, -12), FVector(6.5f, 6.5f, 13), Shirt);
		M.Box(FVector(0, 0, -40), FVector(5, 5, 17), Skin);
		MakePart(EPart::LegBL, FVector(0, 27, 146), M);
		MakePart(EPart::LegBR, FVector(0, -27, 146), M);
		// head
		M.Reset();
		M.Box(FVector(2, 0, 13), FVector(11, 10, 13), Skin);
		M.Box(FVector(-1, 0, 25), FVector(12.5f, 11, 4), Hair);
		M.Box(FVector(-8, 0, 15), FVector(4, 11, 10), Hair);
		for (float Y : {-4.5f, 4.5f})
		{
			M.Box(FVector(13, Y, 16), FVector(0.6f, 2, 1.6f), Black);
			M.Box(FVector(13.2f, Y, 19.5f), FVector(0.5f, 2.5f, 0.6f), Hair);  // eyebrows
		}
		M.Box(FVector(14, 0, 11), FVector(2, 1.6f, 3), Skin * 0.9f);
		M.Box(FVector(13.2f, 0, 5), FVector(0.5f, 3.5f, 0.6f), FLinearColor(0.6f, 0.3f, 0.3f));
		MakePart(EPart::Head, FVector(0, 0, 152), M);
		// the can of soup he's never without
		M.Reset();
		M.Prism(FVector(0, 0, -6), 5.f, 6.f, 8, FLinearColor(0.85f, 0.12f, 0.1f));
		M.Prism(FVector(0, 0, 0), 5.f, 6.f, 8, White);
		MakePart(EPart::Prop, FVector(7, -27, 86), M);
		return;
	}

	// the people of Cranberry Plasma: tall, cranberry-skinned, in long robes; status is all in the hat.
	// The green planet's villagers (chapter 5) share the build: mossy skin, leaf tunics, feathers
	const bool bVillager = Kind == ECreature::Villager;
	Height = bElder ? 200.f : 185.f;
	const FLinearColor Skin = bVillager ? FLinearColor(0.45f, 0.7f, 0.4f) : FLinearColor(0.78f, 0.27f, 0.38f);
	const FLinearColor Robe = bVillager ? (Variant % 2 ? FLinearColor(0.55f, 0.4f, 0.22f) : FLinearColor(0.3f, 0.55f, 0.2f))
		: bElder ? FLinearColor(0.85f, 0.65f, 0.2f) : (Variant % 3 == 0 ? FLinearColor(0.45f, 0.25f, 0.6f) : Variant % 3 == 1 ? FLinearColor(0.2f, 0.5f, 0.55f) : FLinearColor(0.55f, 0.5f, 0.2f));
	M.Box(FVector(0, 0, -40), FVector(6, 6, 40), Skin);
	MakePart(EPart::LegFL, FVector(0, 10, 82), M);
	MakePart(EPart::LegFR, FVector(0, -10, 82), M);
	M.Reset();
	M.Prism(FVector(0, 0, -70), 30.f, 110.f, 8, Robe, 15.f);
	M.Prism(FVector(0, 0, -72), 31.f, 8.f, 8, Robe * 0.6f, 30.f);  // hem
	M.Box(FVector(0, 0, 42), FVector(4, 4, 6), Skin);
	MakePart(EPart::Body, FVector(0, 0, 90), M);
	M.Reset();
	M.Box(FVector(0, 0, -10), FVector(6, 6, 12), Robe);
	M.Box(FVector(0, 0, -34), FVector(4, 4, 14), Skin);
	MakePart(EPart::LegBL, FVector(0, 24, 140), M);
	MakePart(EPart::LegBR, FVector(0, -24, 140), M);
	M.Reset();
	M.Sphere(FVector(2, 0, 16), FVector(13, 12, 17), 5, 8, Skin);
	for (float Y : {-5.5f, 5.5f})
	{
		M.Sphere(FVector(12, Y, 18), FVector(3, 4, 5), 3, 6, Black);  // big dark alien eyes
	}
	M.Box(FVector(14, 0, 7), FVector(0.5f, 3, 0.6f), Skin * 0.6f);
	if (bVillager)
	{
		// a headband with a feather or three (the chief, variant 0, has the most)
		M.Prism(FVector(2, 0, 24), 13.5f, 4.f, 8, FLinearColor(0.7f, 0.25f, 0.15f));
		for (int32 f = 0; f < (Variant == 0 ? 5 : 1 + Variant % 3); ++f)
		{
			M.Box(FVector(-10, (f - 1) * 5.f, 36), FVector(1.5f, 2, 12), FRotator(-15.f, 0, (f - 1) * 12.f),
			      f % 2 ? FLinearColor(0.95f, 0.8f, 0.2f) : FLinearColor(0.2f, 0.6f, 0.9f));
		}
	}
	MakePart(EPart::Head, FVector(0, 0, 140), M);
}

void UCreatureRig::BuildDog(bool bCaptain)
{
	FRetroMesh M;
	Height = bCaptain ? 175.f : 150.f;
	const float S = bCaptain ? 1.15f : 1.f;
	const FLinearColor Fur(0.72f, 0.55f, 0.35f), Dark(0.2f, 0.18f, 0.2f);
	auto V = [S](float X, float Y, float Z) { return FVector(X, Y, Z) * S; };
	M.Box(V(0, 0, -25), V(9, 9, 25), Dark);
	M.Box(V(5, 0, -52), V(13, 10, 4), Black);
	MakePart(EPart::LegFL, V(0, 11, 55), M);
	MakePart(EPart::LegFR, V(0, -11, 55), M);
	M.Reset();
	if (bCaptain)
	{
		M.Box(V(0, 0, 30), V(18, 26, 34), FLinearColor(0.55f, 0.08f, 0.1f));  // captain's coat
		M.Box(V(0, 0, -5), V(18.5f, 26.5f, 4), FLinearColor(0.2f, 0.12f, 0.06f));
		M.Box(V(18.6f, 0, 30), V(0.5f, 4, 30), Brass);
	}
	else
	{
		for (int32 i = 0; i < 6; ++i)
		{
			M.Box(V(0, 0, 4 + i * 10.5f), V(16, 24, 5.25f), i % 2 ? FLinearColor(0.75f, 0.1f, 0.1f) : White);
		}
	}
	MakePart(EPart::Body, V(0, 0, 55), M);
	M.Reset();
	M.Box(V(0, 0, -20), V(7, 7, 20), bCaptain ? FLinearColor(0.55f, 0.08f, 0.1f) : Fur);
	M.Box(V(0, 0, -44), V(7.5f, 7.5f, 5), Fur);
	MakePart(EPart::LegBL, V(0, 30, 120), M);
	MakePart(EPart::LegBR, V(0, -30, 120), M);
	// bulldog head: wide, jowly, underbite
	M.Reset();
	M.Box(V(4, 0, 14), V(14, 16, 13), Fur);
	M.Box(V(16, 0, 7), V(6, 13, 7), Fur * 1.1f);
	M.Box(V(22.5f, 0, 12), V(1.2f, 4, 3), Black);
	M.Box(V(21, 0, 1), V(2, 10, 1.5f), White);  // teeth
	for (float Y : {-1.f, 1.f})
	{
		M.Box(V(0, Y * 17, 18), V(6, 2, 9), FRotator(0, 0, Y * 25.f), Fur * 0.6f);  // floppy ears
		M.Box(V(17.5f, Y * 7, 19), V(0.6f, 2.5f, 2.5f), Y < 0 && bCaptain ? Black : White);
		M.Box(V(18.2f, Y * 7, 19), V(0.4f, 1.2f, 1.5f), Black);
	}
	if (bCaptain)
	{
		M.Box(V(17.8f, -7, 19), V(0.4f, 3.2f, 3.2f), Black);                   // eye patch
		M.Box(V(4, 0, 26), V(14.5f, 16.5f, 1.5f), FRotator(0, 0, -20.f), Black); // patch strap
		M.Prism(V(2, 0, 27), 26.f * S, 9.f * S, 3, Black, 12.f * S);              // tricorn
		M.Box(V(14, 0, 32), V(1, 4, 3), White);                                 // bone emblem
	}
	else
	{
		M.Box(V(3, 0, 26), V(14.6f, 16.6f, 4), FLinearColor(0.85f, 0.15f, 0.1f));  // bandana
	}
	MakePart(EPart::Head, V(0, 0, 125), M);
	// cutlass in the right paw
	M.Reset();
	M.Box(V(4, 0, 0), V(3, 6, 1.5f), Brass);
	M.Box(V(28, 0, 0), V(24, 1.2f, 3), Chrome);
	M.Box(V(-4, 0, 0), V(5, 2, 2), FLinearColor(0.3f, 0.18f, 0.08f));
	MakePart(EPart::Prop, V(6, -30, 76), M);
}

void UCreatureRig::BuildBlob()
{
	Height = 70.f;
	FRetroMesh M;
	const FLinearColor Jelly = Variant == 1 ? FLinearColor(0.35f, 0.85f, 0.25f) : FLinearColor(0.85f, 0.1f, 0.25f);  // 1: jungle spore blob
	M.Sphere(FVector(0, 0, 38), FVector(42, 42, 38), 6, 10, Jelly);
	M.Sphere(FVector(10, 12, 62), FVector(12, 10, 8), 3, 6, Jelly * 1.25f);  // shine
	for (float Y : {-12.f, 12.f})
	{
		M.Sphere(FVector(36, Y, 48), FVector(4, 7, 9), 3, 6, White);
		M.Sphere(FVector(39, Y, 47), FVector(2, 3.5f, 5), 3, 6, Black);
	}
	M.Box(FVector(39, 0, 30), FVector(1, 9, 1.5f), Black);
	MakePart(EPart::Body, FVector::ZeroVector, M);
}

// ---------------------------------------------------------------- Fluffernutter and the luminous beings

void UCreatureRig::BuildFluff(bool bBig)
{
	FRetroMesh M;
	const FLinearColor Fur(0.88f, 0.66f, 0.4f), Cream(0.98f, 0.9f, 0.75f), Pinkish(0.95f, 0.6f, 0.62f);
	if (!bBig)
	{
		// small, round and furry, rodent-like, with big soulful eyes (cat-style parts, so it scampers)
		Height = 42.f;
		M.Sphere(FVector(0, 0, 0), FVector(20, 16, 14), 5, 8, Fur);
		M.Sphere(FVector(4, 0, -5), FVector(15, 12, 9), 4, 8, Cream);
		for (int32 i = 0; i < 6; ++i)
		{
			M.Box(FVector(-14 + i * 5.f, (i % 2 ? 1 : -1) * 13.f, 6), FVector(3, 2, 3), Fur * 1.1f);  // tufts
		}
		MakePart(EPart::Body, FVector(0, 0, 18), M);
		M.Reset();
		M.Sphere(FVector(6, 0, 6), FVector(13, 14, 12), 5, 8, Fur);
		M.Sphere(FVector(15, 0, 2), FVector(5, 7, 5), 3, 6, Cream);
		M.Box(FVector(19.5f, 0, 4), FVector(1, 2, 1.5f), Pinkish);
		for (float Y : {-1.f, 1.f})
		{
			M.Sphere(FVector(15, Y * 6.5f, 9), FVector(3, 5, 6), 3, 6, Black);              // huge eyes
			M.Sphere(FVector(17.5f, Y * 5.f, 11.5f), FVector(1, 1.6f, 1.6f), 2, 4, White);   // shine
			M.Sphere(FVector(2, Y * 11.f, 18), FVector(3, 6, 6), 3, 6, Fur);                  // round ears
			M.Sphere(FVector(2.8f, Y * 11.f, 18), FVector(2, 4, 4), 3, 6, Pinkish);
			M.Box(FVector(18, Y * 9.f, 1), FVector(0.3f, 5, 0.3f), White);                   // whiskers
		}
		MakePart(EPart::Head, FVector(16, 0, 24), M);
		M.Reset();
		M.Box(FVector(0, 0, -4), FVector(3, 3, 5), Fur * 0.85f);
		M.Box(FVector(1.5f, 0, -9), FVector(4, 3.5f, 1.5f), Pinkish);
		MakePart(EPart::LegFL, FVector(9, 8, 10), M);
		MakePart(EPart::LegFR, FVector(9, -8, 10), M);
		MakePart(EPart::LegBL, FVector(-9, 8, 10), M);
		MakePart(EPart::LegBR, FVector(-9, -8, 10), M);
		M.Reset();
		M.Sphere(FVector(-8, 0, 8), FVector(9, 7, 11), 4, 6, Fur);  // a fluffy tail
		M.Sphere(FVector(-11, 0, 15), FVector(6, 5, 6), 3, 6, Cream);
		MakePart(EPart::Tail, FVector(-18, 0, 20), M);
		return;
	}
	// TRANSFORMED: the same fur, stretched over a mountain of muscle, eyes glowing
	Height = 190.f;
	const FLinearColor Dark = Fur * 0.75f;
	M.Box(FVector(0, 0, -30), FVector(16, 16, 32), Dark);
	M.Box(FVector(8, 0, -60), FVector(22, 17, 6), Pinkish * 0.8f);
	MakePart(EPart::LegFL, FVector(0, 26, 66), M);
	MakePart(EPart::LegFR, FVector(0, -26, 66), M);
	M.Reset();
	M.Sphere(FVector(0, 0, 50), FVector(48, 58, 56), 6, 10, Fur);
	M.Sphere(FVector(18, 0, 40), FVector(32, 40, 40), 5, 8, Cream);
	for (int32 i = 0; i < 10; ++i)
	{
		const float A = i * 0.62f;
		M.Box(FVector(-30 + FMath::Cos(A) * 10.f, FMath::Sin(A) * 50.f, 70 + (i % 3) * 12.f), FVector(8, 5, 8), FRotator(0, 0, i * 20.f), Fur * 1.12f);
	}
	MakePart(EPart::Body, FVector(0, 0, 60), M);
	M.Reset();
	M.Box(FVector(0, 0, -30), FVector(17, 17, 32), Fur);
	M.Sphere(FVector(4, 0, -68), FVector(20, 19, 17), 4, 8, Dark);  // fists
	MakePart(EPart::LegBL, FVector(0, 66, 150), M);
	MakePart(EPart::LegBR, FVector(0, -66, 150), M);
	M.Reset();
	M.Sphere(FVector(8, 0, 10), FVector(22, 24, 20), 5, 8, Fur);
	M.Box(FVector(26, 0, 4), FVector(4, 14, 5), White);  // fangs
	for (float Y : {-1.f, 1.f})
	{
		M.Sphere(FVector(25, Y * 9.f, 16), FVector(3, 6, 4), 3, 6, FLinearColor(1.f, 1.f, 0.45f));  // glowing eyes
		M.Sphere(FVector(4, Y * 19.f, 28), FVector(4, 8, 8), 3, 6, Fur);
	}
	MakePart(EPart::Head, FVector(18, 0, 168), M);
}

void UCreatureRig::BuildBeing()
{
	// a being of light: a glowing orb in slowly turning rings (colour by variant)
	static const FLinearColor Cols[] = {FLinearColor(0.6f, 0.9f, 1.f), FLinearColor(1.f, 0.7f, 0.95f), FLinearColor(0.75f, 1.f, 0.6f), FLinearColor(1.f, 0.9f, 0.5f)};
	const FLinearColor C = Cols[Variant % 4];
	Height = 150.f;
	FRetroMesh M;
	M.Sphere(FVector(0, 0, 0), FVector(30.f), 6, 10, C);
	M.Sphere(FVector(0, 0, 0), FVector(18.f), 4, 8, FLinearColor::White);
	MakePart(EPart::Body, FVector(0, 0, 110), M);
	M.Reset();
	for (int32 r = 0; r < 2; ++r)
	{
		const FTransform Saved = M.Xf;
		M.Xf = FTransform(FRotator(r ? 60.f : -30.f, r * 90.f, 0));
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = 2 * PI * i / 16;
			M.Box(FVector(FMath::Cos(A) * 52.f, FMath::Sin(A) * 52.f, 0), FVector(8, 2.5f, 2.5f), FRotator(0, FMath::RadiansToDegrees(A) + 90.f, 0), C * 0.9f);
		}
		M.Xf = Saved;
	}
	MakePart(EPart::Tail, FVector(0, 0, 110), M);
}

// ---------------------------------------------------------------- hats

void UCreatureRig::SetHat(bool bOn)
{
	bHatOn = bOn;
	if (!bOn)
	{
		if (UProceduralMeshComponent* H = Part(EPart::Hat))
		{
			H->SetVisibility(false);
		}
		return;
	}
	BuildHat();
	if (UProceduralMeshComponent* H = Part(EPart::Hat))
	{
		H->SetVisibility(true);
	}
}

void UCreatureRig::BuildHat()
{
	FRetroMesh M;
	if (Kind == ECreature::Sparky)
	{
		// Sparky's design: aesthetically pleasing and mechanically brilliant
		M.Prism(FVector(0, 0, 0), 14.f, 3.f, 10, Styro);
		M.Prism(FVector(0, 0, 3), 9.f, 16.f, 10, Styro, 8.f);
		M.Prism(FVector(0, 0, 19), 8.f, 2.f, 10, Brass);
		M.Prism(FVector(0, 0, 21), 6.f, 8.f, 8, Styro, 4.f);
		for (float Y : {-1.f, 1.f})
		{
			FRetroMesh& G = M;
			const FTransform Saved = G.Xf;
			G.Xf = FTransform(FRotator(0, 0, 90.f), FVector(0, Y * 9.5f, 11.f));
			G.Prism(FVector(0, 0, -1), 6.f, 2.f, 8, Brass);
			for (int32 t = 0; t < 8; ++t)
			{
				const float A = t * PI / 4;
				G.Box(FVector(FMath::Cos(A) * 6.5f, FMath::Sin(A) * 6.5f, 0), FVector(1.5f, 1.5f, 1.2f), Brass);
			}
			G.Xf = Saved;
		}
		M.Prism(FVector(0, 0, 29), 0.8f, 14.f, 4, Chrome);
		M.Diamond(FVector(0, 0, 45), 2.5f, Cyan);
		MakePart(EPart::Hat, FVector(6, 0, 13) * 1.3f, M);
		Part(EPart::Hat)->SetRelativeScale3D(FVector(1.3f));
		return;
	}
	if (Kind != ECreature::Local && Kind != ECreature::Elder)
	{
		return;
	}
	// styrofoam towers: the more tiers and twiddly bits, the higher the status
	const int32 Level = Kind == ECreature::Elder ? 6 : FMath::Clamp(Variant % 5 + 1, 1, 5);
	float Z = 0.f;
	float R = 15.f + Level * 1.5f;
	M.Prism(FVector(0, 0, Z), R + 6.f, 3.f, 10, Styro);
	Z += 3.f;
	for (int32 i = 0; i < Level; ++i)
	{
		const float H = 10.f + 2.f * i;
		M.Prism(FVector(0, 0, Z), R, H, 10, i % 2 ? Styro * 0.92f : Styro, R * 0.85f);
		Z += H;
		R *= 0.85f;
		if (i >= 1)
		{
			// little styrofoam balls stuck round the rim
			for (int32 b = 0; b < 4 + i; ++b)
			{
				const float A = 2 * PI * b / (4 + i) + i;
				M.Sphere(FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, Z - 2), FVector(3.f), 2, 5, Styro);
			}
		}
	}
	M.Prism(FVector(0, 0, Z), R * 0.6f, 8.f + Level * 2.f, 6, Styro, 0.f);
	if (Kind == ECreature::Elder)
	{
		for (int32 s = 0; s < 4; ++s)
		{
			const float A = s * PI / 2 + PI / 4;
			M.Prism(FVector(FMath::Cos(A) * 20.f, FMath::Sin(A) * 20.f, 10.f), 4.f, 40.f, 6, Styro, 0.f);
		}
		M.Prism(FVector(0, 0, 3), 22.f, 4.f, 10, Brass);
	}
	MakePart(EPart::Hat, FVector(2, 0, 30), M);
}

// ---------------------------------------------------------------- animation

void UCreatureRig::SetFlash(float Amount, const FLinearColor& Color)
{
	if (Mid)
	{
		Mid->SetScalarParameterValue("Flash", Amount);
		Mid->SetVectorParameterValue("FlashColor", Color);
	}
}

void UCreatureRig::SetTint(const FLinearColor& Tint)
{
	if (Mid)
	{
		Mid->SetVectorParameterValue("Tint", Tint);
	}
}

void UCreatureRig::SetHidden(bool bHide)
{
	SetVisibility(!bHide, true);
	if (bHide == false && !bHatOn)
	{
		if (UProceduralMeshComponent* H = Part(EPart::Hat))
		{
			H->SetVisibility(false);
		}
	}
}

void UCreatureRig::Animate(float Dt, const FRigPose& P)
{
	if (Parts.Num() < int32(EPart::Count))
	{
		return;
	}
	Time += Dt;
	const float Move = FMath::Clamp(P.Speed / 600.f, 0.f, 1.2f);
	const bool bCat = Kind == ECreature::Sparky || Kind == ECreature::AlleyCat || Kind == ECreature::Meatball || Kind == ECreature::Fluffernutter;
	const float Size = Kind == ECreature::Meatball ? 2.3f : 1.f;
	Phase += Dt * (bCat ? 14.f : 9.f) * FMath::Max(Move, 0.f) / FMath::Sqrt(Size);
	const float Sw = FMath::Sin(Phase);

	auto Rot = [this](EPart Pt, const FRotator& R)
	{
		if (UProceduralMeshComponent* C = Part(Pt))
		{
			C->SetRelativeRotation(R);
		}
	};
	auto Loc = [this](EPart Pt, const FVector& Off)
	{
		if (UProceduralMeshComponent* C = Part(Pt))
		{
			C->SetRelativeLocation(Pivot[int32(Pt)] + Off);
		}
	};

	// whole-body tumbles: somersault, velcro cling, crouch
	FRotator Whole = FRotator::ZeroRotator;
	if (P.Flip > 0.f)
	{
		Whole.Pitch = -360.f * FMath::InterpEaseInOut(0.f, 1.f, P.Flip, 1.6f);
	}
	if (P.bCling)
	{
		Whole.Pitch = 70.f;
	}
	const FVector Center(0, 0, Height * 0.45f * ScaleMul);
	const FQuat Q = Whole.Quaternion();
	SetRelativeRotation(Whole);
	SetRelativeLocation(FVector(0, 0, BaseZ) + (Center - Q.RotateVector(Center)));
	SetRelativeScale3D(FVector(1.f + 0.15f * P.Crouch, 1.f + 0.15f * P.Crouch, 1.f - 0.35f * P.Crouch) * ScaleMul);

	if (Kind == ECreature::Being)
	{
		Loc(EPart::Body, FVector(0, 0, FMath::Sin(Time * 1.8f) * 12.f));
		Loc(EPart::Tail, FVector(0, 0, FMath::Sin(Time * 1.8f) * 12.f));
		Rot(EPart::Tail, FRotator(0, Time * 40.f, FMath::Sin(Time) * 10.f));
		if (UProceduralMeshComponent* B = Part(EPart::Body))
		{
			B->SetRelativeScale3D(FVector(1.f + 0.06f * FMath::Sin(Time * 3.f) + 0.2f * P.Talk / 3.f));
		}
		return;
	}
	if (Kind == ECreature::BogBlob)
	{
		const float Sq = P.bAir ? 1.f + FMath::Clamp(P.VZ / 1500.f, -0.3f, 0.4f) : 1.f + 0.12f * FMath::Sin(Time * 7.f);
		if (UProceduralMeshComponent* B = Part(EPart::Body))
		{
			B->SetRelativeScale3D(FVector(1.f / FMath::Sqrt(Sq), 1.f / FMath::Sqrt(Sq), Sq));
		}
		return;
	}

	const float Bob = FMath::Abs(Sw) * 3.f * Move * Size;
	const float TalkNod = P.Talk > 0.f ? FMath::Sin(Time * 14.f) * 6.f : 0.f;
	const float Dizzy = P.bDizzy ? FMath::Sin(Time * 6.f) * 20.f : 0.f;

	if (bCat)
	{
		const float Leg = 38.f * Sw * FMath::Min(Move, 1.f);
		if (P.bAir && !P.bCling)
		{
			// stretched out mid-leap
			const float Stretch = P.VZ > 0.f ? 1.f : 0.6f;
			Rot(EPart::LegFL, FRotator(50.f * Stretch, 0, 0));
			Rot(EPart::LegFR, FRotator(50.f * Stretch, 0, 0));
			Rot(EPart::LegBL, FRotator(-45.f * Stretch, 0, 0));
			Rot(EPart::LegBR, FRotator(-45.f * Stretch, 0, 0));
		}
		else
		{
			Rot(EPart::LegFL, FRotator(Leg, 0, 0));
			Rot(EPart::LegBR, FRotator(Leg, 0, 0));
			Rot(EPart::LegFR, FRotator(-Leg, 0, 0));
			Rot(EPart::LegBL, FRotator(-Leg, 0, 0));
		}
		if (P.Attack > 0.f)
		{
			// a big right hook
			const float A = FMath::Sin(P.Attack * PI);
			Rot(EPart::LegFR, FRotator(110.f * A, -30.f * A, 0));
			Rot(EPart::Body, FRotator(8.f * A, 0, -10.f * A));
		}
		else
		{
			Rot(EPart::Body, FRotator(P.bAir ? FMath::Clamp(P.VZ / 40.f, -15.f, 15.f) : 0.f, 0, 0));
		}
		if (P.bWeld)
		{
			const float W = FMath::Sin(Time * 30.f) * 6.f;
			Rot(EPart::LegFL, FRotator(70.f + W, 0, 0));
			Rot(EPart::LegFR, FRotator(70.f - W, 0, 0));
		}
		Loc(EPart::Body, FVector(0, 0, Bob));
		Loc(EPart::Head, FVector(0, 0, Bob * 0.7f));
		Rot(EPart::Head, FRotator(TalkNod + (P.bWeld ? -15.f : 0.f), 0, Dizzy));
		Rot(EPart::Tail, FRotator(P.bAir ? 25.f : 0.f, FMath::Sin(Time * 2.5f) * 18.f + Sw * 10.f * Move, 0));
		return;
	}

	// two-legged: people and pirate dogs. LegFL/FR are legs, LegBL/BR are arms
	const float Leg = 32.f * Sw * FMath::Min(Move, 1.f);
	Rot(EPart::LegFL, FRotator(P.bAir ? 25.f : Leg, 0, 0));
	Rot(EPart::LegFR, FRotator(P.bAir ? -20.f : -Leg, 0, 0));
	Rot(EPart::LegBL, FRotator(-Leg * 0.8f, 0, P.bAir ? 40.f : 0.f));
	float ArmR = Leg * 0.8f;
	if (P.Attack > 0.f)
	{
		// raise, then hack down
		ArmR = P.Attack < 0.4f ? FMath::Lerp(0.f, 160.f, P.Attack / 0.4f) : FMath::Lerp(160.f, 50.f, (P.Attack - 0.4f) / 0.6f);
	}
	else if (Kind == ECreature::Leo)
	{
		ArmR = 45.f + Leg * 0.3f;  // soup can held up, always
	}
	Rot(EPart::LegBR, FRotator(ArmR, 0, P.bAir ? -40.f : 0.f));
	if (UProceduralMeshComponent* Prop = Part(EPart::Prop))
	{
		// the prop rides along with the right hand
		const FVector Shoulder = Pivot[int32(EPart::LegBR)];
		const FVector Hand = Pivot[int32(EPart::Prop)] - Shoulder;
		const FQuat AQ = FRotator(ArmR, 0, P.bAir ? -40.f : 0.f).Quaternion();
		Prop->SetRelativeLocation(Shoulder + AQ.RotateVector(Hand));
		Prop->SetRelativeRotation(AQ);
	}
	const float Slouch = Kind == ECreature::Leo ? 6.f : 0.f;
	Rot(EPart::Body, FRotator(-Slouch, 0, Sw * 3.f * Move));
	Loc(EPart::Body, FVector(0, 0, Bob));
	Loc(EPart::Head, FVector(Slouch * 0.8f, 0, Bob));
	Loc(EPart::LegBL, FVector(0, 0, Bob));
	Loc(EPart::LegBR, FVector(0, 0, Bob));
	Rot(EPart::Head, FRotator(TalkNod - Slouch * 0.5f, 0, Dizzy));
}
