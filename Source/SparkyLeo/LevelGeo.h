#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetroMesh.h"
#include "LevelGeo.generated.h"

class UMaterialInstanceDynamic;
class UProceduralMeshComponent;

/** A point light baked into the vertex colours. */
struct FLamp
{
	FVector Pos;
	FLinearColor Color;
	float Radius;
};

/** How a chapter looks: lighting, fog and sky. */
struct FTheme
{
	FLinearColor Ambient = FLinearColor(0.6f, 0.6f, 0.62f);
	FVector SunDir = FVector(0.4f, -0.3f, 0.85f).GetSafeNormal();
	FLinearColor Sun = FLinearColor(0.5f, 0.48f, 0.45f);
	FLinearColor Fog = FLinearColor(0.4f, 0.3f, 0.4f);
	float FogStart = 5000.f;
	float FogEnd = 30000.f;
	FLinearColor SkyTop = FLinearColor(0.1f, 0.05f, 0.2f);
	FLinearColor SkyHorizon = FLinearColor(0.8f, 0.4f, 0.5f);
	FLinearColor SkyBottom = FLinearColor(0.2f, 0.05f, 0.1f);
	int32 Stars = 0;
	/** Gravity scale for everyone (the player, enemies, Leo). */
	float Gravity = 2.4f;
	/** Below this the player has fallen out of the world. */
	float KillZ = -1500.f;
};

/**
 * The static world of a chapter: textured blocks with baked vertex lighting (one mesh section
 * per texture, with collision), decoration without collision, additive glow, and the sky.
 * The chapter code in Levels.cpp fills it in and calls Commit().
 */
UCLASS()
class SPARKYLEO_API ALevelGeo : public AActor
{
	GENERATED_BODY()

public:
	ALevelGeo();

	static ALevelGeo* Get(const UObject* Ctx);

	FTheme Theme;
	TArray<FLamp> Lamps;

	/** Solid geometry drawn with a texture (collision). */
	FRetroMesh& Solid(const TCHAR* Tex);
	/** Decoration (no collision). */
	FRetroMesh& Deco(const TCHAR* Tex);
	/** Additive glow (no collision); vertex colours are the glow colour. */
	FRetroMesh& Glow() { return GlowMesh; }

	/** A box with world-sized UVs (one texture tile per TexSize cm), cut up so the lighting has vertices. */
	void Block(const FVector& Center, const FVector& Half, const TCHAR* Tex, const FLinearColor& Color, float TexSize = 256.f,
	           float Yaw = 0.f, bool bSolid = true, int32 Skip = 0);
	/** A cylinder standing on Base (towers, huts, pillars). */
	void Cylinder(const FVector& Base, float Radius, float Height, int32 Sides, const TCHAR* Tex, const FLinearColor& Color,
	              bool bSolid = true, float TopRadius = -1.f);
	/** A lamp, with a glowing bulb at Pos when BulbSize > 0. */
	void Lamp(const FVector& Pos, const FLinearColor& Color, float Radius, float BulbSize = 0.f);

	/** Builds the meshes. Call once after everything is added. */
	void Commit();

	/** Vertex light at a point, 1 = neutral. */
	FLinearColor LightAt(const FVector& P, const FVector& N) const;

	/** M_Retro instance with this chapter's fog. */
	UMaterialInstanceDynamic* MakeMaterial(UObject* Outer, const TCHAR* Tex, bool bFog = true);
	UMaterialInstanceDynamic* MakeGlow(UObject* Outer, const FLinearColor& Color = FLinearColor::White, float Intensity = 1.f) const;

	/** Changes the fog colour on everything (the freezing ship in chapter 4). */
	void SetFog(const FLinearColor& Color, float Start, float End);

private:
	UPROPERTY()
	UProceduralMeshComponent* SolidMesh;
	UPROPERTY()
	UProceduralMeshComponent* DecoMesh;
	UPROPERTY()
	UProceduralMeshComponent* GlowComp;
	UPROPERTY()
	UProceduralMeshComponent* SkyMesh;
	UPROPERTY()
	TArray<UMaterialInstanceDynamic*> Mids;

	TArray<FString> SolidTex, DecoTex;
	TArray<FRetroMesh> SolidMeshes, DecoMeshes;
	FRetroMesh GlowMesh;

	FRetroMesh& Find(TArray<FString>& Names, TArray<FRetroMesh>& Meshes, const TCHAR* Tex);
	void BuildSky();
};
