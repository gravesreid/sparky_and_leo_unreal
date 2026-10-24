#include "RetroMesh.h"
#include "Assets.h"
#include "SparkyLeo.h"

#include "Engine/Texture.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

FLinearColor FRetroMesh::Shade(const FVector& N)
{
	static const FVector Key = FVector(0.45f, -0.3f, 0.85f).GetSafeNormal();
	const float V = 0.55f + 0.5f * FMath::Max(0.f, FVector::DotProduct(N, Key)) - 0.12f * FMath::Max(0.f, -N.Z);
	return FLinearColor(V, V, V);
}

void FRetroMesh::AddVert(const FVector& P, const FVector& N, const FVector2D& UV, const FLinearColor& Color)
{
	const FVector WP = Xf.TransformPosition(P);
	const FVector WN = Xf.TransformVectorNoScale(N).GetSafeNormal();
	const FLinearColor L = Light ? Light(WP, WN) : Shade(WN);
	Verts.Add(WP);
	Normals.Add(WN);
	UVs.Add(UV);
	// Colours are picked by eye (sRGB); vertex colours are linear. M_Retro doubles the vertex
	// colour, so store half: lamps can then overbright up to 2x.
	const FLinearColor Lin(FMath::Pow(Color.R, 2.2f), FMath::Pow(Color.G, 2.2f), FMath::Pow(Color.B, 2.2f));
	FLinearColor C = Lin * L * 0.5f;
	C.A = 1.f;
	Colors.Add(C);
}

void FRetroMesh::Quad(const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& Normal,
                      const FLinearColor& Color, const FVector2D& UA, const FVector2D& UB, const FVector2D& UC, const FVector2D& UD)
{
	const int32 Base = Verts.Num();
	AddVert(A, Normal, UA, Color);
	AddVert(B, Normal, UB, Color);
	AddVert(C, Normal, UC, Color);
	AddVert(D, Normal, UD, Color);
	// Unreal's front face: (P2 - P0) x (P1 - P0) points out.
	const bool bFlip = FVector::DotProduct(FVector::CrossProduct(C - A, B - A), Normal) < 0.f;
	if (!bFlip)
	{
		Tris.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
	}
	else
	{
		Tris.Append({Base, Base + 2, Base + 1, Base, Base + 3, Base + 2});
	}
}

void FRetroMesh::Tri(const FVector& A, const FVector& B, const FVector& C, const FVector& Normal, const FLinearColor& Color)
{
	const int32 Base = Verts.Num();
	AddVert(A, Normal, FVector2D(0, 0), Color);
	AddVert(B, Normal, FVector2D(1, 0), Color);
	AddVert(C, Normal, FVector2D(0, 1), Color);
	if (FVector::DotProduct(FVector::CrossProduct(C - A, B - A), Normal) >= 0.f)
	{
		Tris.Append({Base, Base + 1, Base + 2});
	}
	else
	{
		Tris.Append({Base, Base + 2, Base + 1});
	}
}

void FRetroMesh::Grid(const FVector& A, const FVector& U, const FVector& V, int32 Nu, int32 Nv, const FVector& Normal,
                      const FLinearColor& Color, const FVector2D& UV0, const FVector2D& UV1)
{
	Nu = FMath::Max(1, Nu);
	Nv = FMath::Max(1, Nv);
	for (int32 j = 0; j < Nv; ++j)
	{
		for (int32 i = 0; i < Nu; ++i)
		{
			const float U0 = float(i) / Nu, U1 = float(i + 1) / Nu, V0 = float(j) / Nv, V1 = float(j + 1) / Nv;
			auto P = [&](float a, float b) { return A + U * a + V * b; };
			auto T = [&](float a, float b) { return FVector2D(FMath::Lerp(UV0.X, UV1.X, a), FMath::Lerp(UV0.Y, UV1.Y, b)); };
			Quad(P(U0, V0), P(U1, V0), P(U1, V1), P(U0, V1), Normal, Color, T(U0, V0), T(U1, V0), T(U1, V1), T(U0, V1));
		}
	}
}

void FRetroMesh::Box(const FVector& C, const FVector& H, const FLinearColor& Color, int32 Skip)
{
	const FVector X(H.X, 0, 0), Y(0, H.Y, 0), Z(0, 0, H.Z);
	if (!(Skip & 1)) Quad(C + X - Y - Z, C + X + Y - Z, C + X + Y + Z, C + X - Y + Z, FVector(1, 0, 0), Color, FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0));
	if (!(Skip & 2)) Quad(C - X + Y - Z, C - X - Y - Z, C - X - Y + Z, C - X + Y + Z, FVector(-1, 0, 0), Color, FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0));
	if (!(Skip & 4)) Quad(C + Y + X - Z, C + Y - X - Z, C + Y - X + Z, C + Y + X + Z, FVector(0, 1, 0), Color, FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0));
	if (!(Skip & 8)) Quad(C - Y - X - Z, C - Y + X - Z, C - Y + X + Z, C - Y - X + Z, FVector(0, -1, 0), Color, FVector2D(0, 1), FVector2D(1, 1), FVector2D(1, 0), FVector2D(0, 0));
	if (!(Skip & 16)) Quad(C + Z - X - Y, C + Z + X - Y, C + Z + X + Y, C + Z - X + Y, FVector(0, 0, 1), Color);
	if (!(Skip & 32)) Quad(C - Z - X + Y, C - Z + X + Y, C - Z + X - Y, C - Z - X - Y, FVector(0, 0, -1), Color);
}

void FRetroMesh::Box(const FVector& Center, const FVector& Half, const FRotator& Rot, const FLinearColor& Color)
{
	const FTransform Saved = Xf;
	Xf = FTransform(Rot, Center) * Saved;
	Box(FVector::ZeroVector, Half, Color);
	Xf = Saved;
}

void FRetroMesh::Prism(const FVector& Base, float Radius, float Height, int32 Sides, const FLinearColor& Color, float TopRadius)
{
	if (TopRadius < 0.f)
	{
		TopRadius = Radius;
	}
	const FVector Top = Base + FVector(0, 0, Height);
	for (int32 i = 0; i < Sides; ++i)
	{
		const float A0 = 2 * PI * i / Sides, A1 = 2 * PI * (i + 1) / Sides;
		const FVector D0(FMath::Cos(A0), FMath::Sin(A0), 0), D1(FMath::Cos(A1), FMath::Sin(A1), 0);
		const FVector N = ((D0 + D1) * 0.5f + FVector(0, 0, (Radius - TopRadius) / FMath::Max(Height, 1.f))).GetSafeNormal();
		Quad(Base + D0 * Radius, Base + D1 * Radius, Top + D1 * TopRadius, Top + D0 * TopRadius, N, Color,
		     FVector2D(float(i) / Sides, 1), FVector2D(float(i + 1) / Sides, 1), FVector2D(float(i + 1) / Sides, 0), FVector2D(float(i) / Sides, 0));
		if (TopRadius > 0.f)
		{
			Tri(Top, Top + D0 * TopRadius, Top + D1 * TopRadius, FVector(0, 0, 1), Color);
		}
		Tri(Base, Base + D1 * Radius, Base + D0 * Radius, FVector(0, 0, -1), Color);
	}
}

void FRetroMesh::PrismX(const FVector& Base, float Radius, float Length, int32 Sides, const FLinearColor& Color)
{
	const FTransform Saved = Xf;
	Xf = FTransform(FRotator(-90.f, 0.f, 0.f), Base) * Saved;  // +Z becomes +X
	Prism(FVector::ZeroVector, Radius, Length, Sides, Color);
	Xf = Saved;
}

void FRetroMesh::Sphere(const FVector& C, const FVector& R, int32 Rings, int32 Segs, const FLinearColor& Color, bool bInside)
{
	auto P = [&](int32 i, int32 j)
	{
		const float Th = PI * i / Rings, Ph = 2 * PI * j / Segs;
		return FVector(FMath::Sin(Th) * FMath::Cos(Ph), FMath::Sin(Th) * FMath::Sin(Ph), FMath::Cos(Th));
	};
	for (int32 i = 0; i < Rings; ++i)
	{
		for (int32 j = 0; j < Segs; ++j)
		{
			const FVector A = P(i, j), B = P(i, j + 1), D = P(i + 1, j + 1), E = P(i + 1, j);
			const FVector N = ((A + B + D + E) * 0.25f * (bInside ? -1.f : 1.f)).GetSafeNormal();
			const FVector2D UA(float(j) / Segs, float(i) / Rings), UD(float(j + 1) / Segs, float(i + 1) / Rings);
			if (i == 0)
			{
				Tri(C + A * R, C + D * R, C + E * R, N, Color);
			}
			else if (i == Rings - 1)
			{
				Tri(C + A * R, C + B * R, C + E * R, N, Color);
			}
			else
			{
				Quad(C + A * R, C + B * R, C + D * R, C + E * R, N, Color, UA, FVector2D(UD.X, UA.Y), UD, FVector2D(UA.X, UD.Y));
			}
		}
	}
}

void FRetroMesh::Diamond(const FVector& C, float R, const FLinearColor& Color, float Tall)
{
	const FVector Up(0, 0, R * Tall), Dn(0, 0, -R * Tall);
	const FVector Ring[4] = {FVector(R, 0, 0), FVector(0, R, 0), FVector(-R, 0, 0), FVector(0, -R, 0)};
	for (int32 i = 0; i < 4; ++i)
	{
		const FVector& A = Ring[i];
		const FVector& B = Ring[(i + 1) % 4];
		Tri(C + Up, C + A, C + B, ((A + B) * 0.5f + Up * 0.5f).GetSafeNormal(), Color);
		Tri(C + Dn, C + B, C + A, ((A + B) * 0.5f + Dn * 0.5f).GetSafeNormal(), Color);
	}
}

void FRetroMesh::Reset()
{
	Verts.Reset();
	Tris.Reset();
	Normals.Reset();
	UVs.Reset();
	Colors.Reset();
}

void FRetroMesh::Commit(UProceduralMeshComponent* Mesh, int32 Section, bool bCollision) const
{
	TArray<FProcMeshTangent> NoTangents;
	TArray<FLinearColor> Clamped = Colors;
	for (FLinearColor& C : Clamped)
	{
		C.R = FMath::Clamp(C.R, 0.f, 1.f);
		C.G = FMath::Clamp(C.G, 0.f, 1.f);
		C.B = FMath::Clamp(C.B, 0.f, 1.f);
	}
	Mesh->CreateMeshSection_LinearColor(Section, Verts, Tris, Normals, UVs, Clamped, NoTangents, bCollision);
}

namespace Assets
{
	namespace
	{
		template <typename T>
		T* LoadRooted(const TCHAR* Path)
		{
			T* Obj = LoadObject<T>(nullptr, Path);
			if (Obj)
			{
				Obj->AddToRoot();
			}
			else
			{
				UE_LOG(LogSparky, Error, TEXT("Missing asset %s - run ./build_content.sh"), Path);
			}
			return Obj;
		}
	}

	UMaterialInterface* RetroMaterial()
	{
		static UMaterialInterface* M = LoadRooted<UMaterialInterface>(Retro);
		return M;
	}

	UMaterialInterface* GlowMaterial()
	{
		static UMaterialInterface* M = LoadRooted<UMaterialInterface>(Glow);
		return M;
	}

	UTexture* Texture(const TCHAR* Name)
	{
		static TMap<FString, UTexture*> Cache;
		if (UTexture** Found = Cache.Find(Name))
		{
			return *Found;
		}
		// plain white comes from the engine: a pure grey PNG imports as a one-channel texture,
		// which samples as red through M_Retro
		UTexture* T = FCString::Strcmp(Name, TEXT("T_White")) == 0
			? LoadRooted<UTexture>(TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"))
			: LoadRooted<UTexture>(*FString::Printf(TEXT("/Game/Textures/%s.%s"), Name, Name));
		Cache.Add(Name, T);
		return T;
	}
}
