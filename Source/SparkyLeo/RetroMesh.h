#pragma once

#include "CoreMinimal.h"

class UProceduralMeshComponent;

/**
 * Builds low-poly meshes the N64 way: flat-coloured boxes and quads with the lighting baked
 * into the vertex colours (Gouraud shading). Everything is drawn with M_Retro, which is unlit
 * and multiplies texture x vertex colour x 2, so a vertex colour of 0.5 is "neutral".
 */
struct FRetroMesh
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;

	/** Light at a vertex, (position, normal) -> 1 = neutral. Unset: Shade(normal), a fixed key light. */
	TFunction<FLinearColor(const FVector&, const FVector&)> Light;
	/** Applied to everything added from now on. */
	FTransform Xf = FTransform::Identity;

	/** Fixed key light from above and in front, for models. */
	static FLinearColor Shade(const FVector& N);

	/** Corners in order around the face; the winding is fixed up from Normal. */
	void Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal,
	          const FLinearColor& Color, const FVector2D& UA = FVector2D(0, 0), const FVector2D& UB = FVector2D(1, 0),
	          const FVector2D& UC = FVector2D(1, 1), const FVector2D& UD = FVector2D(0, 1));
	void Tri(const FVector& A, const FVector& B, const FVector& C, const FVector& Normal, const FLinearColor& Color);

	/**
	 * The quad A, A+U, A+U+V, A+V cut into Nu x Nv tiles, so baked lighting has vertices to live
	 * on. UV0 / UV1 are the texture coordinates at A and at A+U+V.
	 */
	void Grid(const FVector& A, const FVector& U, const FVector& V, int32 Nu, int32 Nv, const FVector& Normal,
	          const FLinearColor& Color, const FVector2D& UV0, const FVector2D& UV1);

	/** Box. Skip is a mask of faces to leave out: 1 +X, 2 -X, 4 +Y, 8 -Y, 16 +Z, 32 -Z. */
	void Box(const FVector& Center, const FVector& Half, const FLinearColor& Color, int32 Skip = 0);
	void Box(const FVector& Center, const FVector& Half, const FRotator& Rot, const FLinearColor& Color);

	/** N-sided prism standing on Base along +Z (barrels, bottles, gun barrels). */
	void Prism(const FVector& Base, float Radius, float Height, int32 Sides, const FLinearColor& Color, float TopRadius = -1.f);
	/** Same, lying along +X. */
	void PrismX(const FVector& Base, float Radius, float Length, int32 Sides, const FLinearColor& Color);
	/** Flat-shaded ellipsoid. bInside turns the faces inward (skies). */
	void Sphere(const FVector& Center, const FVector& Radii, int32 Rings, int32 Segs, const FLinearColor& Color, bool bInside = false);
	/** Eight-faced diamond (stars, gems, sparks). */
	void Diamond(const FVector& Center, float Radius, const FLinearColor& Color, float Tall = 1.f);

	bool IsEmpty() const { return Verts.Num() == 0; }
	void Reset();
	void Commit(UProceduralMeshComponent* Mesh, int32 Section, bool bCollision) const;

private:
	void AddVert(const FVector& P, const FVector& N, const FVector2D& UV, const FLinearColor& Color);
};
