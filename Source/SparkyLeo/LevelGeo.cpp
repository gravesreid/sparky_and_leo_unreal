#include "LevelGeo.h"
#include "Assets.h"
#include "SparkyLeo.h"

#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"

ALevelGeo::ALevelGeo()
{
	PrimaryActorTick.bCanEverTick = false;
	SolidMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Solid"));
	RootComponent = SolidMesh;
	SolidMesh->bUseComplexAsSimpleCollision = true;
	SolidMesh->SetCollisionProfileName(TEXT("BlockAll"));
	SolidMesh->CastShadow = false;
	DecoMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Deco"));
	DecoMesh->SetupAttachment(RootComponent);
	DecoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecoMesh->CastShadow = false;
	GlowComp = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Glow"));
	GlowComp->SetupAttachment(RootComponent);
	GlowComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowComp->CastShadow = false;
	SkyMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Sky"));
	SkyMesh->SetupAttachment(RootComponent);
	SkyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SkyMesh->CastShadow = false;
	SkyMesh->bNeverDistanceCull = true;
	SkyMesh->SetBoundsScale(10.f);
	GlowMesh.Light = [](const FVector&, const FVector&) { return FLinearColor::White; };
}

ALevelGeo* ALevelGeo::Get(const UObject* Ctx)
{
	UWorld* W = Ctx ? Ctx->GetWorld() : nullptr;
	if (!W)
	{
		return nullptr;
	}
	for (TActorIterator<ALevelGeo> It(W); It; ++It)
	{
		if (!It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	return nullptr;
}

FRetroMesh& ALevelGeo::Find(TArray<FString>& Names, TArray<FRetroMesh>& Meshes, const TCHAR* Tex)
{
	int32 I = Names.IndexOfByKey(FString(Tex));
	if (I == INDEX_NONE)
	{
		I = Names.Add(Tex);
		FRetroMesh& M = Meshes.AddDefaulted_GetRef();
		M.Light = [this](const FVector& P, const FVector& N) { return LightAt(P, N); };
	}
	return Meshes[I];
}

FRetroMesh& ALevelGeo::Solid(const TCHAR* Tex)
{
	return Find(SolidTex, SolidMeshes, Tex);
}

FRetroMesh& ALevelGeo::Deco(const TCHAR* Tex)
{
	return Find(DecoTex, DecoMeshes, Tex);
}

FLinearColor ALevelGeo::LightAt(const FVector& P, const FVector& N) const
{
	FLinearColor L = Theme.Ambient + Theme.Sun * FMath::Max(0.f, FVector::DotProduct(N, Theme.SunDir));
	for (const FLamp& Lp : Lamps)
	{
		const FVector D = Lp.Pos - P;
		const float Dist = D.Size();
		if (Dist < Lp.Radius)
		{
			const float F = FMath::Square(1.f - Dist / Lp.Radius);
			const float Facing = FMath::Max(0.25f, FVector::DotProduct(N, D / FMath::Max(Dist, 1.f)));
			L += Lp.Color * (F * Facing);
		}
	}
	return L;
}

void ALevelGeo::Block(const FVector& C, const FVector& H, const TCHAR* Tex, const FLinearColor& Color, float TexSize, float Yaw,
                      bool bSolid, int32 Skip)
{
	FRetroMesh& M = bSolid ? Solid(Tex) : Deco(Tex);
	const FTransform Saved = M.Xf;
	M.Xf = FTransform(FRotator(0, Yaw, 0), C);
	auto Cells = [](float Len) { return FMath::Clamp(FMath::CeilToInt(Len / 300.f), 1, 24); };
	auto Face = [&](const FVector& A, const FVector& U, const FVector& V, const FVector& N, float LU, float LV)
	{
		M.Grid(A, U, V, Cells(LU), Cells(LV), N, Color, FVector2D(0, 0), FVector2D(LU / TexSize, LV / TexSize));
	};
	const FVector X(H.X, 0, 0), Y(0, H.Y, 0), Z(0, 0, H.Z);
	// sides: U runs along the face, V runs down from the top so textures stand upright
	if (!(Skip & 1)) Face(X - Y + Z, Y * 2, -Z * 2, FVector(1, 0, 0), H.Y * 2, H.Z * 2);
	if (!(Skip & 2)) Face(-X + Y + Z, -Y * 2, -Z * 2, FVector(-1, 0, 0), H.Y * 2, H.Z * 2);
	if (!(Skip & 4)) Face(X + Y + Z, -X * 2, -Z * 2, FVector(0, 1, 0), H.X * 2, H.Z * 2);
	if (!(Skip & 8)) Face(-X - Y + Z, X * 2, -Z * 2, FVector(0, -1, 0), H.X * 2, H.Z * 2);
	if (!(Skip & 16)) Face(-X - Y + Z, X * 2, Y * 2, FVector(0, 0, 1), H.X * 2, H.Y * 2);
	if (!(Skip & 32)) Face(-X + Y - Z, X * 2, -Y * 2, FVector(0, 0, -1), H.X * 2, H.Y * 2);
	M.Xf = Saved;
}

void ALevelGeo::Cylinder(const FVector& Base, float Radius, float Height, int32 Sides, const TCHAR* Tex, const FLinearColor& Color,
                         bool bSolid, float TopRadius)
{
	FRetroMesh& M = bSolid ? Solid(Tex) : Deco(Tex);
	if (TopRadius < 0.f)
	{
		TopRadius = Radius;
	}
	const int32 Rows = FMath::Clamp(FMath::CeilToInt(Height / 300.f), 1, 12);
	const float Wrap = FMath::Max(1.f, FMath::RoundToFloat(2 * PI * Radius / 256.f));
	for (int32 r = 0; r < Rows; ++r)
	{
		const float Z0 = Height * r / Rows, Z1 = Height * (r + 1) / Rows;
		const float R0 = FMath::Lerp(Radius, TopRadius, float(r) / Rows), R1 = FMath::Lerp(Radius, TopRadius, float(r + 1) / Rows);
		for (int32 i = 0; i < Sides; ++i)
		{
			const float A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
			const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
			const FVector N = ((D0 + D1) * 0.5f + FVector(0, 0, (Radius - TopRadius) / FMath::Max(Height, 1.f))).GetSafeNormal();
			const float U0 = Wrap * i / Sides, U1 = Wrap * (i + 1) / Sides, V0 = (Height - Z1) / 256.f, V1 = (Height - Z0) / 256.f;
			M.Quad(Base + D0 * R0 + FVector(0, 0, Z0), Base + D1 * R0 + FVector(0, 0, Z0), Base + D1 * R1 + FVector(0, 0, Z1),
			       Base + D0 * R1 + FVector(0, 0, Z1), N, Color, FVector2D(U0, V1), FVector2D(U1, V1), FVector2D(U1, V0), FVector2D(U0, V0));
		}
	}
	if (TopRadius > 0.f)
	{
		const FVector Top = Base + FVector(0, 0, Height);
		for (int32 i = 0; i < Sides; ++i)
		{
			const float A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
			const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
			M.Tri(Top, Top + D0 * TopRadius, Top + D1 * TopRadius, FVector(0, 0, 1), Color);
		}
	}
}

void ALevelGeo::Lamp(const FVector& Pos, const FLinearColor& Color, float Radius, float BulbSize)
{
	Lamps.Add({Pos, Color, Radius});
	if (BulbSize > 0.f)
	{
		GlowMesh.Diamond(Pos, BulbSize, Color * 0.8f + FLinearColor(0.2f, 0.2f, 0.2f));
	}
}

UMaterialInstanceDynamic* ALevelGeo::MakeMaterial(UObject* Outer, const TCHAR* Tex, bool bFog)
{
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Assets::RetroMaterial(), Outer);
	if (UTexture* T = Assets::Texture(Tex))
	{
		M->SetTextureParameterValue("Tex", T);
	}
	M->SetVectorParameterValue("FogColor", Theme.Fog);
	M->SetScalarParameterValue("FogStart", Theme.FogStart);
	M->SetScalarParameterValue("FogEnd", Theme.FogEnd);
	M->SetScalarParameterValue("FogAmount", bFog ? 1.f : 0.f);
	if (bFog)
	{
		Mids.Add(M);  // so SetFog reaches it
	}
	return M;
}

UMaterialInstanceDynamic* ALevelGeo::MakeGlow(UObject* Outer, const FLinearColor& Color, float Intensity) const
{
	UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(Assets::GlowMaterial(), Outer);
	M->SetVectorParameterValue("Color", Color);
	M->SetScalarParameterValue("Intensity", Intensity);
	return M;
}

void ALevelGeo::SetFog(const FLinearColor& Color, float Start, float End)
{
	Theme.Fog = Color;
	Theme.FogStart = Start;
	Theme.FogEnd = End;
	for (UMaterialInstanceDynamic* M : Mids)
	{
		if (M)
		{
			M->SetVectorParameterValue("FogColor", Color);
			M->SetScalarParameterValue("FogStart", Start);
			M->SetScalarParameterValue("FogEnd", End);
		}
	}
}

void ALevelGeo::BuildSky()
{
	FRetroMesh Sky;
	Sky.Light = [this](const FVector& P, const FVector& N)
	{
		// colour by height on the dome: bottom -> horizon -> top (x2 for M_Retro's halving)
		const float H = P.GetSafeNormal().Z;
		const FLinearColor C = H > 0.f ? FMath::Lerp(Theme.SkyHorizon, Theme.SkyTop, FMath::Pow(H, 0.6f))
		                               : FMath::Lerp(Theme.SkyHorizon, Theme.SkyBottom, FMath::Pow(-H, 0.5f));
		return FLinearColor(FMath::Pow(C.R, 2.2f), FMath::Pow(C.G, 2.2f), FMath::Pow(C.B, 2.2f)) * 2.f;  // sRGB picks -> linear
	};
	Sky.Sphere(FVector::ZeroVector, FVector(150000.f), 16, 24, FLinearColor::White, true);
	Sky.Commit(SkyMesh, 0, false);
	UMaterialInstanceDynamic* M = MakeMaterial(this, TEXT("T_White"), false);
	SkyMesh->SetMaterial(0, M);

	// stars: diamonds on a big shell, brighter ones bigger
	FRandomStream R(1234);
	for (int32 i = 0; i < Theme.Stars; ++i)
	{
		const FVector Dir = R.GetUnitVector();
		if (Dir.Z < -0.3f && Theme.KillZ > -5000.f)
		{
			continue;  // planets have ground below
		}
		const float B = R.FRandRange(0.4f, 1.f);
		const FLinearColor Tint = FMath::Lerp(FLinearColor(0.7f, 0.8f, 1.f), FLinearColor(1.f, 0.85f, 0.7f), R.FRand());
		GlowMesh.Diamond(Dir * 120000.f, 150.f + 350.f * B * B, Tint * B);
	}
}

void ALevelGeo::Commit()
{
	BuildSky();
	for (int32 i = 0; i < SolidMeshes.Num(); ++i)
	{
		SolidMeshes[i].Commit(SolidMesh, i, true);
		const bool bInvisible = SolidTex[i] == TEXT("Invisible");  // collision only (the cage walls)
		UMaterialInstanceDynamic* M = MakeMaterial(this, bInvisible ? TEXT("T_White") : *SolidTex[i]);
		SolidMesh->SetMaterial(i, M);
		SolidMesh->SetMeshSectionVisible(i, !bInvisible);
	}
	for (int32 i = 0; i < DecoMeshes.Num(); ++i)
	{
		DecoMeshes[i].Commit(DecoMesh, i, false);
		UMaterialInstanceDynamic* M = MakeMaterial(this, *DecoTex[i]);
		DecoMesh->SetMaterial(i, M);
	}
	if (!GlowMesh.IsEmpty())
	{
		// glow keeps its colour: undo the retro halving
		TArray<FLinearColor>& Cols = GlowMesh.Colors;
		for (FLinearColor& C : Cols)
		{
			C = C * 2.f;
			C.A = 1.f;
		}
		GlowMesh.Commit(GlowComp, 0, false);
		GlowComp->SetMaterial(0, MakeGlow(this));
	}
	UE_LOG(LogSparky, Log, TEXT("Level built: %d textures, %d deco, %d lamps"), SolidMeshes.Num(), DecoMeshes.Num(), Lamps.Num());
}
