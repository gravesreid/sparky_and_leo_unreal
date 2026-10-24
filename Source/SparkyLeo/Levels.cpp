#include "Levels.h"
#include "Enemy.h"
#include "LevelGeo.h"
#include "Props.h"
#include "SparkyGameMode.h"

#include "EngineUtils.h"

namespace
{
	const FLinearColor W(1.f, 1.f, 1.f);

	FLinearColor Rgb(float R, float G, float B) { return FLinearColor(R, G, B); }

	/** Building helpers for one chapter. */
	struct FB
	{
		ASparkyGameMode* GM;
		ALevelGeo* Geo;

		void Block(const FVector& C, const FVector& H, const TCHAR* Tex, const FLinearColor& Col = W, float TexSize = 256.f, float Yaw = 0.f)
		{
			Geo->Block(C, H, Tex, Col, TexSize, Yaw);
		}
		/** A block given by its bottom-centre and top height: easier for floors and walls. */
		void Slab(float X, float Y, float HX, float HY, float Z0, float Z1, const TCHAR* Tex, const FLinearColor& Col = W, float TexSize = 256.f)
		{
			Geo->Block(FVector(X, Y, (Z0 + Z1) * 0.5f), FVector(HX, HY, (Z1 - Z0) * 0.5f), Tex, Col, TexSize);
		}
		void Deco(const FVector& C, const FVector& H, const TCHAR* Tex, const FLinearColor& Col = W, float TexSize = 256.f, float Yaw = 0.f)
		{
			Geo->Block(C, H, Tex, Col, TexSize, Yaw, false);
		}
		/** Invisible wall (collision only). */
		void Wall(const FVector& C, const FVector& H, float Yaw = 0.f)
		{
			Geo->Block(C, H, TEXT("Invisible"), W, 256.f, Yaw);
		}
		void Cyl(const FVector& Base, float R, float H, int32 Sides, const TCHAR* Tex, const FLinearColor& Col = W, bool bSolid = true, float TopR = -1.f)
		{
			Geo->Cylinder(Base, R, H, Sides, Tex, Col, bSolid, TopR);
		}
		/** A wedge rising along its local +X from Z0 to Z0 + Rise. */
		void Ramp(const FVector& BottomCenter, float HX, float HY, float Rise, const TCHAR* Tex, float Yaw = 0.f, const FLinearColor& Col = W)
		{
			FRetroMesh& M = Geo->Solid(Tex);
			const FTransform Saved = M.Xf;
			M.Xf = FTransform(FRotator(0, Yaw, 0), BottomCenter);
			const FVector A(-HX, -HY, 0), B(HX, -HY, Rise), C(HX, HY, Rise), D(-HX, HY, 0);
			const FVector N = FVector(-Rise, 0, HX * 2).GetSafeNormal();
			const float L = FVector::Dist(A, B);
			M.Grid(A, B - A, D - A, FMath::Max(1, int32(L / 300.f)), FMath::Max(1, int32(HY / 150.f)), N, Col, FVector2D(0, 0), FVector2D(L / 256.f, HY * 2 / 256.f));
			M.Quad(FVector(HX, -HY, 0), FVector(HX, HY, 0), C, B, FVector(1, 0, 0), Col);
			M.Tri(A, FVector(HX, -HY, 0), B, FVector(0, -1, 0), Col);
			M.Tri(D, C, FVector(HX, HY, 0), FVector(0, 1, 0), Col);
			M.Xf = Saved;
		}
		void Lamp(const FVector& P, const FLinearColor& C, float R, float Bulb = 0.f) { Geo->Lamp(P, C, R, Bulb); }

		APickup* Item(EPickup T, const FVector& P, bool bActive = true)
		{
			APickup* A = GM->Spawn<APickup>(P);
			A->Init(T, bActive);
			return A;
		}
		void Nip(const FVector& P) { Item(EPickup::Catnip, P); }
		void NipLine(const FVector& A, const FVector& B, int32 N)
		{
			for (int32 i = 0; i < N; ++i)
			{
				Nip(FMath::Lerp(A, B, N == 1 ? 0.5f : float(i) / (N - 1)));
			}
		}
		void NipRing(const FVector& C, float R, int32 N, float Phase = 0.f)
		{
			for (int32 i = 0; i < N; ++i)
			{
				const float A = 2 * PI * i / N + Phase;
				Nip(C + FVector(FMath::Cos(A) * R, FMath::Sin(A) * R, 0));
			}
		}
		ANpc* Npc(ECreature K, int32 Var, const FVector& Feet, float Yaw, FName Id, bool bTalk, bool bDance = false)
		{
			ANpc* N = GM->Spawn<ANpc>(Feet, Yaw);
			N->Init(K, Var, Id, bTalk, bDance);
			return N;
		}
		AEnemy* Foe(EEnemy T, const FVector& Feet, int32 Var = 0, float Leash = -1.f)
		{
			AEnemy* E = GM->Spawn<AEnemy>(Feet + FVector(0, 0, 90.f), FMath::FRandRange(0.f, 360.f));
			E->Init(T, Var);
			if (Leash >= 0.f)
			{
				E->SetLeash(Leash);
			}
			return E;
		}
		AMover* Mover(const FVector& C, const FVector& H, const TCHAR* Tex, const FLinearColor& Col, EMove Mv, const FVector& Off, float Period, float Phase = 0.f, bool bGlow = false)
		{
			AMover* M = GM->Spawn<AMover>(C);
			M->Init(H, Tex, Col, Mv, Off, Period, Phase, bGlow);
			return M;
		}
		void Velcro(const FVector& C, const FVector& H)
		{
			GM->Spawn<AVelcroWall>(C)->Init(H);
		}
		void Check(const FVector& Feet, float Yaw, const TCHAR* Tex = TEXT("T_White"))
		{
			GM->Spawn<ACheckpoint>(Feet, Yaw)->Init(Tex);
		}
		AGoal* Goal(FName Id, const FVector& Feet, float R, bool bActive)
		{
			AGoal* G = GM->Spawn<AGoal>(Feet);
			G->Init(Id, R, bActive);
			return G;
		}
		AHazard* Haz(EHazard T, const FVector& P, const FVector& Off, float Period, float Phase = 0.f)
		{
			AHazard* H = GM->Spawn<AHazard>(P);
			H->Init(T, Off, Period, Phase);
			return H;
		}
		ARepairPoint* Repair(FName Id, const FVector& P, float Yaw, bool bBig)
		{
			ARepairPoint* R = GM->Spawn<ARepairPoint>(P, Yaw);
			R->Init(Id, bBig);
			return R;
		}
	};

	// ================================================================ chapter 1: Cranberry Plasma

	/** Sparky's ship, parked (chapter 1) at Base facing +X. */
	void SparkyShip(FB& B, const FVector& Base)
	{
		const FLinearColor Hull = Rgb(0.85f, 0.88f, 0.92f), Trim = Rgb(0.95f, 0.75f, 0.2f);
		B.Block(Base + FVector(0, 0, 200), FVector(420, 150, 110), TEXT("T_Hull"), Hull);
		B.Block(Base + FVector(500, 0, 190), FVector(90, 115, 85), TEXT("T_Hull"), Hull);
		B.Block(Base + FVector(620, 0, 180), FVector(40, 70, 55), TEXT("T_Hull"), Trim);
		B.Deco(Base + FVector(420, 0, 300), FVector(110, 100, 30), TEXT("T_Glass"));
		B.Block(Base + FVector(-50, 0, 102), FVector(380, 155, 12), TEXT("T_Hull"), Trim);
		B.Block(Base + FVector(-60, 0, 312), FVector(300, 120, 6), TEXT("T_Hull"), Trim);
		for (float S : {-1.f, 1.f})
		{
			B.Block(Base + FVector(-80, S * 330, 170), FVector(220, 180, 14), TEXT("T_Hull"), Hull);           // wings
			B.Block(Base + FVector(-200, S * 505, 230), FVector(110, 10, 80), TEXT("T_Hull"), Trim);           // wingtip fins
			B.Block(Base + FVector(-470, S * 95, 200), FVector(70, 60, 60), TEXT("T_Hull"), Rgb(0.4f, 0.42f, 0.48f)); // engines
			B.Geo->Glow().Box(Base + FVector(-545, S * 95, 200), FVector(8, 45, 45), Rgb(0.4f, 0.8f, 1.f));
			for (float X : {-250.f, 250.f})
			{
				B.Block(Base + FVector(X, S * 100, 45), FVector(14, 14, 50), TEXT("T_Hull"), Rgb(0.4f, 0.42f, 0.48f));  // landing legs
			}
		}
		B.Block(Base + FVector(-500, 0, 380), FVector(70, 14, 120), TEXT("T_Hull"), Trim);  // tail fin
		// the boarding ramp, down to the pad on the -Y side
		B.Ramp(Base + FVector(0, -280, 0), 60.f, 140.f, 90.f, TEXT("T_Hull"), 90.f, Rgb(0.6f, 0.62f, 0.68f));
	}

	/** A round hut with a styrofoam roof. */
	void Hut(FB& B, const FVector& Base, float R, float H)
	{
		B.Cyl(Base, R, H, 10, TEXT("T_Stucco"));
		B.Cyl(Base + FVector(0, 0, H), R + 50.f, R * 0.9f, 10, TEXT("T_Styro"), W, true, 20.f);
		B.Deco(Base + FVector(R - 5.f, 0, 90), FVector(8, 55, 90), TEXT("T_Planks"), Rgb(0.5f, 0.3f, 0.25f));  // door
		B.Lamp(Base + FVector(R + 40.f, 0, 220), Rgb(1.f, 0.4f, 0.7f), 600.f, 14.f);
	}

	/** An alien plant: a curly stalk with a glowing bulb. */
	void Plant(FB& B, const FVector& Base, float H, const FLinearColor& Glow)
	{
		B.Cyl(Base, 14.f, H, 6, TEXT("T_White"), Rgb(0.3f, 0.6f, 0.35f), false, 8.f);
		B.Geo->Glow().Diamond(Base + FVector(0, 0, H + 20.f), 26.f, Glow);
		B.Geo->Glow().Diamond(Base + FVector(0, 0, H + 20.f), 50.f, Glow * 0.25f, 0.8f);
	}

	/** A merchant's stall with its keeper and (after the hat) its wares. */
	void Stall(FB& B, const FVector& Base, float Yaw, EPickup Ware, FName Id, int32 Var)
	{
		const FVector F = FRotator(0, Yaw, 0).Vector();
		const FVector R = FRotator(0, Yaw + 90.f, 0).Vector();
		B.Block(Base + FVector(0, 0, 50), FVector(80, 160, 50), TEXT("T_Planks"), Rgb(0.75f, 0.5f, 0.35f), 128.f, Yaw);
		for (float S : {-1.f, 1.f})
		{
			B.Block(Base + R * 150.f * S - F * 60.f + FVector(0, 0, 160), FVector(8, 8, 160), TEXT("T_Planks"), Rgb(0.6f, 0.4f, 0.3f), 128.f, Yaw);
		}
		B.Deco(Base - F * 40.f + FVector(0, 0, 330), FVector(130, 190, 10), TEXT("T_Awning"), W, 128.f, Yaw);
		B.Npc(ECreature::Local, Var, Base - F * 140.f, Yaw, Id, true);
		B.Item(Ware, Base + FVector(0, 0, 135), false);
	}

	Levels::FSetup Chapter1(FB& B)
	{
		FTheme& T = B.Geo->Theme;
		T.Ambient = Rgb(0.62f, 0.56f, 0.62f);
		T.Sun = Rgb(0.5f, 0.42f, 0.38f);
		T.SunDir = FVector(0.3f, 0.5f, 0.8f).GetSafeNormal();
		T.Fog = Rgb(0.7f, 0.38f, 0.5f);
		T.FogStart = 5000.f;
		T.FogEnd = 28000.f;
		T.SkyTop = Rgb(0.12f, 0.04f, 0.24f);
		T.SkyHorizon = Rgb(0.95f, 0.45f, 0.45f);
		T.SkyBottom = Rgb(0.35f, 0.08f, 0.15f);
		T.Stars = 250;
		T.KillZ = -40.f;
		T.Gravity = 2.4f;

		// the cranberry bog everything stands in
		B.Deco(FVector(0, 0, -60), FVector(40000, 40000, 2), TEXT("T_Bog"), W, 512.f);
		FRandomStream R(11);
		for (int32 i = 0; i < 60; ++i)
		{
			// giant cranberries bobbing in the bog
			const FVector P(R.FRandRange(-9000.f, 14000.f), R.FRandRange(-9000.f, 9000.f), -70.f);
			if (FMath::Abs(P.Y) < 2200.f && P.X > -1500.f && P.X < 8000.f)
			{
				continue;
			}
			const float S = R.FRandRange(60.f, 160.f);
			B.Geo->Deco(TEXT("T_White")).Sphere(P, FVector(S), 4, 8, Rgb(0.75f, 0.08f, 0.2f));
		}
		// distant mesas
		for (int32 i = 0; i < 14; ++i)
		{
			const float A = 2 * PI * i / 14 + 0.2f;
			const float D = R.FRandRange(14000.f, 20000.f);
			const FVector P = FVector(3000, 0, 0) + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, -400);
			B.Cyl(P, R.FRandRange(1200.f, 2500.f), R.FRandRange(1500.f, 4000.f), 7, TEXT("T_Rock"), Rgb(0.8f, 0.5f, 0.55f), false, R.FRandRange(600.f, 1200.f));
		}
		// two moons
		B.Geo->Glow().Sphere(FVector(-30000, 60000, 40000), FVector(5000), 8, 12, Rgb(0.95f, 0.85f, 0.7f) * 0.8f);
		B.Geo->Glow().Sphere(FVector(40000, 45000, 25000), FVector(2500), 6, 10, Rgb(0.6f, 0.9f, 1.f) * 0.7f);

		// ---- landing pad island, with jelly hill to the north
		B.Slab(0, 0, 1200, 1200, -400, 0, TEXT("T_Grass"), W, 384.f);
		B.Slab(0, 1700, 900, 500, -400, 0, TEXT("T_Grass"), W, 384.f);
		B.Cyl(FVector(-300, 0, 0), 520.f, 16.f, 12, TEXT("T_Pad"));
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = 2 * PI * i / 8;
			B.Lamp(FVector(-300 + FMath::Cos(A) * 500.f, FMath::Sin(A) * 500.f, 40.f), Rgb(0.3f, 0.7f, 1.f), 350.f, 10.f);
		}
		SparkyShip(B, FVector(-300, 0, 16));
		B.Goal(TEXT("Ship"), FVector(-300, -470, 0), 170.f, false);
		B.GM->Spawn<ALeoBuddy>(FVector(300, -700, 120), 0.f);
		B.Check(FVector(500, 500, 0), 0.f);
		B.NipLine(FVector(300, -500, 40), FVector(1000, -500, 40), 4);
		for (float X : {-900.f, 900.f})
		{
			Plant(B, FVector(X, -900, 0), 240.f, Rgb(1.f, 0.4f, 0.8f));
			Plant(B, FVector(X * 0.7f, 950, 0), 300.f, Rgb(0.4f, 1.f, 0.8f));
		}

		// jelly hill: terraces up to styrofoam #5, with bog blobs
		B.Slab(-100, 1650, 650, 420, 0, 140, TEXT("T_Jelly"), W, 256.f);
		B.Slab(-50, 1750, 450, 300, 140, 280, TEXT("T_Jelly"), W, 256.f);
		B.Slab(0, 1850, 280, 200, 280, 420, TEXT("T_Jelly"), W, 256.f);
		B.Slab(50, 1900, 130, 110, 420, 560, TEXT("T_Jelly"), W, 256.f);
		B.Ramp(FVector(-300, 1090, 0), 140.f, 120.f, 140.f, TEXT("T_Jelly"), 90.f);
		B.Item(EPickup::Styrofoam, FVector(50, 1900, 620));
		B.Foe(EEnemy::BogBlob, FVector(-300, 1500, 140), 0, 700.f);
		B.Foe(EEnemy::BogBlob, FVector(200, 1650, 280), 0, 600.f);
		B.Foe(EEnemy::BogBlob, FVector(-200, 1850, 420), 0, 500.f);
		B.NipLine(FVector(-500, 1450, 190), FVector(-100, 1650, 330), 3);
		B.NipLine(FVector(150, 1750, 470), FVector(-80, 1900, 470), 2);

		// ---- the bridge to the village
		B.Slab(1700, 0, 520, 150, -30, 0, TEXT("T_Planks"), W, 128.f);
		for (float X = 1200.f; X <= 2200.f; X += 250.f)
		{
			for (float S : {-1.f, 1.f})
			{
				B.Cyl(FVector(X, S * 150.f, -60), 12.f, 170.f, 6, TEXT("T_Planks"), Rgb(0.6f, 0.4f, 0.3f));
			}
		}
		B.Deco(FVector(1700, 150, 90), FVector(520, 3, 3), TEXT("T_White"), Rgb(0.8f, 0.7f, 0.5f));
		B.Deco(FVector(1700, -150, 90), FVector(520, 3, 3), TEXT("T_White"), Rgb(0.8f, 0.7f, 0.5f));
		B.NipLine(FVector(1300, 0, 40), FVector(2100, 0, 40), 5);

		// ---- the village plaza
		B.Slab(4000, 0, 1800, 1800, -400, 0, TEXT("T_Grass"), W, 384.f);
		B.Slab(4000, 0, 900, 900, 0, 8, TEXT("T_Tile"), W, 256.f);
		B.Check(FVector(2900, -350, 0), 0.f);
		B.NipRing(FVector(4000, 0, 50), 450.f, 8);
		// the elder's dais and the great hat hall behind it
		B.Cyl(FVector(4750, 0, 8), 360.f, 60.f, 12, TEXT("T_Tile"));
		B.Cyl(FVector(4750, 0, 68), 260.f, 50.f, 12, TEXT("T_Tile"));
		B.Ramp(FVector(4300, 0, 8), 100.f, 140.f, 60.f, TEXT("T_Tile"), 0.f);
		B.Npc(ECreature::Elder, 0, FVector(4800, 0, 118), 180.f, TEXT("Elder"), true);
		B.Cyl(FVector(5450, 0, 0), 420.f, 700.f, 12, TEXT("T_Stucco"));
		B.Cyl(FVector(5450, 0, 700), 500.f, 300.f, 12, TEXT("T_Styro"), W, true, 320.f);
		B.Cyl(FVector(5450, 0, 1000), 320.f, 250.f, 12, TEXT("T_Styro"), W, true, 170.f);
		B.Cyl(FVector(5450, 0, 1250), 170.f, 300.f, 10, TEXT("T_Styro"), W, true, 0.f);
		B.Lamp(FVector(4750, 0, 500), Rgb(1.f, 0.85f, 0.6f), 900.f, 0.f);
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = PI * 0.6f + PI * 0.8f * i / 5.f;
			B.Lamp(FVector(4750, 0, 0) + FVector(FMath::Cos(A) * 420.f, FMath::Sin(A) * 420.f, 220.f), i % 2 ? Rgb(1.f, 0.3f, 0.6f) : Rgb(0.3f, 0.8f, 1.f), 500.f, 14.f);
			B.Cyl(FVector(4750, 0, 0) + FVector(FMath::Cos(A) * 420.f, FMath::Sin(A) * 420.f, 0), 10.f, 210.f, 6, TEXT("T_Styro"));
		}
		// huts round the edge
		Hut(B, FVector(4300, 1450, 0), 260.f, 360.f);
		Hut(B, FVector(5000, 1300, 0), 220.f, 300.f);
		Hut(B, FVector(4000, -1450, 0), 260.f, 340.f);
		Hut(B, FVector(5100, -1350, 0), 230.f, 320.f);
		// merchants (wares appear once Sparky has his hat)
		Stall(B, FVector(3650, 600, 8), -90.f, EPickup::PeanutButter, TEXT("Merchant0"), 1);
		Stall(B, FVector(4350, 600, 8), -90.f, EPickup::CatFood, TEXT("Merchant1"), 2);
		Stall(B, FVector(3650, -600, 8), 90.f, EPickup::Tang, TEXT("Merchant2"), 3);
		Stall(B, FVector(4350, -600, 8), 90.f, EPickup::Soup, TEXT("Merchant3"), 4);
		B.Item(EPickup::DrivePart, FVector(4600, 0, 150), false);
		// townsfolk
		B.Npc(ECreature::Local, 0, FVector(3700, 250, 8), 30.f, TEXT("Local0"), true);
		B.Npc(ECreature::Local, 2, FVector(3800, -200, 8), -60.f, TEXT("Local1"), true);
		B.Npc(ECreature::Local, 3, FVector(4400, 300, 8), 200.f, TEXT("Local2"), true);
		B.Npc(ECreature::Local, 1, FVector(3200, 0, 8), 0.f, TEXT("Local3"), true);

		// ---- styrofoam #1: the old hat tower (a spiral of hat-brim platforms)
		const FVector Tower(2900, -1250, 0);
		B.Cyl(Tower, 130.f, 1050.f, 10, TEXT("T_Styro"));
		for (int32 i = 0; i < 7; ++i)
		{
			const float A = -PI * 0.5f + i * PI / 3.f;
			const FVector P = Tower + FVector(FMath::Cos(A) * 380.f, FMath::Sin(A) * 380.f, 0);
			const float Z = 150.f * (i + 1);
			B.Cyl(P + FVector(0, 0, Z - 40.f), 150.f, 40.f, 10, TEXT("T_Styro"), i % 2 ? Rgb(0.92f, 0.92f, 0.88f) : W);
			B.Cyl(P + FVector(0, 0, Z - 150.f), 30.f, 110.f, 6, TEXT("T_Styro"), Rgb(0.85f, 0.85f, 0.8f), false);
			B.Nip(P + FVector(0, 0, Z + 40.f));
		}
		B.Cyl(Tower + FVector(0, 0, 1160), 230.f, 40.f, 12, TEXT("T_Styro"));
		B.Item(EPickup::Styrofoam, Tower + FVector(0, 0, 1260));
		B.Npc(ECreature::Local, 4, Tower + FVector(-400, 300, 0), 0.f, TEXT("Tower"), true);

		// ---- styrofoam #2: the velcro party hall
		const FVector Hall(3200, 1350, 0);
		B.Slab(Hall.X, Hall.Y + 380, 520, 30, 0, 1500, TEXT("T_Stucco"));      // back wall
		B.Slab(Hall.X - 520, Hall.Y, 30, 400, 0, 1500, TEXT("T_Stucco"));      // side walls
		B.Slab(Hall.X + 520, Hall.Y, 30, 400, 0, 1500, TEXT("T_Stucco"));
		B.Slab(Hall.X, Hall.Y, 520, 400, 0, 10, TEXT("T_Tile"), Rgb(0.8f, 0.6f, 0.9f));
		B.Velcro(FVector(Hall.X - 230, Hall.Y + 100, 760), FVector(40, 220, 600));
		B.Velcro(FVector(Hall.X + 190, Hall.Y + 100, 660), FVector(40, 220, 600));
		B.Item(EPickup::Styrofoam, FVector(Hall.X - 230, Hall.Y + 100, 1430));
		B.NipLine(FVector(Hall.X - 20, Hall.Y + 100, 450), FVector(Hall.X - 20, Hall.Y + 100, 1050), 4);
		for (int32 i = 0; i < 5; ++i)
		{
			B.Npc(ECreature::Local, i + 1, FVector(Hall.X - 350 + i * 170.f, Hall.Y - 250 + (i % 2) * 80.f, 10), FMath::FRandRange(0.f, 360.f), NAME_None, false, true);
			B.Lamp(FVector(Hall.X - 400 + i * 200.f, Hall.Y + 300, 900), i % 2 ? Rgb(1.f, 0.2f, 0.8f) : Rgb(0.2f, 1.f, 0.6f), 700.f, 20.f);
		}
		B.Npc(ECreature::Local, 3, FVector(Hall.X + 100, Hall.Y - 520, 0), -90.f, TEXT("Velcro"), true);
		B.Deco(FVector(Hall.X, Hall.Y - 395, 1300), FVector(500, 6, 60), TEXT("T_Velcro"), Rgb(1.f, 0.6f, 0.8f));

		// ---- styrofoam #3: across the bog on sinking lily pads
		B.Npc(ECreature::Local, 2, FVector(5650, -900, 0), 0.f, TEXT("Bog"), true);
		const FVector Pads[] = {FVector(6100, -1000, -25), FVector(6450, -1180, -25), FVector(6800, -950, -25), FVector(7150, -1150, -25)};
		for (int32 i = 0; i < 4; ++i)
		{
			B.Mover(Pads[i], FVector(110, 110, 15), TEXT("T_Lily"), W, EMove::Sinker, FVector(0, 0, 90), 3.f + i * 0.3f);
			B.Nip(Pads[i] + FVector(0, 0, 80));
		}
		B.Cyl(FVector(7750, -1200, -400), 520.f, 400.f, 12, TEXT("T_Grass"));
		B.Cyl(FVector(7850, -1250, 0), 90.f, 140.f, 8, TEXT("T_Planks"), Rgb(0.55f, 0.35f, 0.25f));  // stump
		B.Item(EPickup::Styrofoam, FVector(7850, -1250, 220));
		B.Check(FVector(7550, -1000, 0), 180.f);
		B.Foe(EEnemy::BogBlob, FVector(7700, -1400, 0), 0, 500.f);
		B.Foe(EEnemy::BogBlob, FVector(8000, -1000, 0), 0, 500.f);
		Plant(B, FVector(8050, -1500, 0), 260.f, Rgb(1.f, 0.8f, 0.3f));

		// ---- styrofoam #4: up the floating rocks, on plasma platforms
		B.Slab(6050, 1300, 180, 180, 0, 150, TEXT("T_Rock"));
		B.Mover(FVector(6450, 1350, 250), FVector(120, 120, 15), TEXT("T_Plasma"), W, EMove::Linear, FVector(0, 0, 250), 4.f, 0.f, true);
		B.Slab(6850, 1550, 170, 170, 400, 520, TEXT("T_Rock"));
		B.Mover(FVector(6900, 1150, 600), FVector(110, 110, 15), TEXT("T_Plasma"), W, EMove::Linear, FVector(450, 0, 0), 5.f, 0.f, true);
		B.Slab(7550, 1150, 200, 200, 640, 760, TEXT("T_Rock"));
		B.Mover(FVector(7550, 1600, 850), FVector(110, 110, 15), TEXT("T_Plasma"), W, EMove::Bob, FVector(0, 0, 120), 3.f, 0.f, true);
		B.Slab(7550, 2050, 260, 220, 900, 1020, TEXT("T_Rock"));
		B.Item(EPickup::Styrofoam, FVector(7550, 2050, 1100));
		B.Nip(FVector(6050, 1300, 200));
		B.Nip(FVector(6850, 1550, 570));
		B.Nip(FVector(7550, 1150, 810));
		B.NipRing(FVector(7550, 2050, 1060), 150.f, 4);
		for (const FVector& P : {FVector(6850, 1550, 400), FVector(7550, 1150, 640), FVector(7550, 2050, 900)})
		{
			B.Cyl(P - FVector(0, 0, 300), 60.f, 300.f, 6, TEXT("T_Rock"), W, false, 150.f);  // rock tails underneath
		}
		return {FVector(450, -550, 60), 0.f, EMusic::Cranberry};
	}

	// ================================================================ chapter 2: Chinatown, years ago

	void Building(FB& B, float X0, float X1, float Y, float Depth, float H, const FLinearColor& Tint, int32 Seed)
	{
		const float Cx = (X0 + X1) * 0.5f, Hx = (X1 - X0) * 0.5f;
		B.Slab(Cx, Y, Hx, Depth, 0, H, TEXT("T_Brick"), Tint, 256.f);
		// lit windows on the alley side
		FRandomStream R(Seed);
		const float Face = Y > 0 ? Y - Depth - 2.f : Y + Depth + 2.f;
		for (float Z = 300.f; Z < H - 100.f; Z += 260.f)
		{
			for (float X = X0 + 120.f; X < X1 - 80.f; X += 220.f)
			{
				if (R.FRand() < 0.55f)
				{
					B.Geo->Glow().Box(FVector(X, Face, Z), FVector(45, 2, 55), R.FRand() < 0.7f ? Rgb(0.9f, 0.7f, 0.35f) : Rgb(0.5f, 0.8f, 0.9f));
				}
				else
				{
					B.Deco(FVector(X, Face, Z), FVector(45, 2, 55), TEXT("T_White"), Rgb(0.08f, 0.08f, 0.12f));
				}
			}
		}
		// a neon sign
		const FLinearColor Neon = R.FRand() < 0.5f ? Rgb(1.f, 0.25f, 0.4f) : Rgb(0.3f, 1.f, 0.6f);
		B.Geo->Glow().Box(FVector(Cx, Face + (Y > 0 ? -30.f : 30.f), 420.f), FVector(14, 6, 120), Neon);
		B.Lamp(FVector(Cx, Face + (Y > 0 ? -80.f : 80.f), 420.f), Neon * 0.8f, 450.f);
	}

	Levels::FSetup Chapter2(FB& B)
	{
		FTheme& T = B.Geo->Theme;
		T.Ambient = Rgb(0.36f, 0.33f, 0.44f);
		T.Sun = Rgb(0.15f, 0.15f, 0.25f);
		T.SunDir = FVector(-0.3f, 0.4f, 0.85f).GetSafeNormal();
		T.Fog = Rgb(0.07f, 0.04f, 0.1f);
		T.FogStart = 2500.f;
		T.FogEnd = 12000.f;
		T.SkyTop = Rgb(0.02f, 0.02f, 0.06f);
		T.SkyHorizon = Rgb(0.2f, 0.1f, 0.2f);
		T.SkyBottom = Rgb(0.05f, 0.02f, 0.05f);
		T.Stars = 60;
		T.KillZ = -2000.f;
		T.Gravity = 2.4f;

		// ---- the alley
		B.Slab(-3200, 0, 2000, 350, -100, 0, TEXT("T_Asphalt"), W, 256.f);
		Building(B, -5200, -4000, 650, 300, 1400, Rgb(0.9f, 0.6f, 0.55f), 1);
		Building(B, -4000, -2600, 700, 350, 1100, Rgb(0.7f, 0.7f, 0.75f), 2);
		Building(B, -2600, -1300, 650, 300, 1600, Rgb(1.f, 0.75f, 0.6f), 3);
		Building(B, -5200, -3600, -650, 300, 1200, Rgb(0.8f, 0.8f, 0.7f), 4);
		Building(B, -3600, -2200, -700, 350, 1500, Rgb(0.95f, 0.6f, 0.5f), 5);
		Building(B, -2200, -1300, -650, 300, 1000, Rgb(0.75f, 0.65f, 0.8f), 6);
		B.Slab(-5300, 0, 100, 400, 0, 1500, TEXT("T_Brick"), Rgb(0.6f, 0.55f, 0.6f));  // dead end behind the start
		// lanterns strung across
		for (float X = -4800.f; X < -1400.f; X += 500.f)
		{
			B.Deco(FVector(X, 0, 560), FVector(2, 350, 2), TEXT("T_White"), Rgb(0.2f, 0.15f, 0.1f));
			for (float Y : {-200.f, 0.f, 200.f})
			{
				B.Geo->Glow().Sphere(FVector(X, Y, 520), FVector(26, 26, 32), 4, 6, Rgb(1.f, 0.25f, 0.12f));
				B.Geo->Glow().Box(FVector(X, Y, 485), FVector(4, 4, 8), Rgb(0.9f, 0.7f, 0.2f));
			}
			B.Lamp(FVector(X, 0, 480), Rgb(1.f, 0.35f, 0.15f), 650.f);
		}
		// crates, dumpsters and fire escapes to clamber on
		B.Slab(-4300, 230, 110, 80, 0, 160, TEXT("T_Crate"));
		B.Slab(-4300, 230, 70, 60, 160, 290, TEXT("T_Crate"));
		B.Slab(-3600, -240, 160, 90, 0, 180, TEXT("T_Dumpster"));
		B.Slab(-2900, 250, 120, 80, 0, 140, TEXT("T_Crate"));
		B.Slab(-2000, -250, 90, 80, 0, 170, TEXT("T_Crate"));
		B.Slab(-3900, 300, 400, 50, 420, 440, TEXT("T_Grate"));   // fire escape
		B.Slab(-2900, -300, 500, 50, 380, 400, TEXT("T_Grate"));
		B.Slab(-2900, -300, 300, 50, 700, 720, TEXT("T_Grate"));
		B.Nip(FVector(-4300, 230, 340));
		B.NipLine(FVector(-4200, 300, 490), FVector(-3600, 300, 490), 4);
		B.NipLine(FVector(-3300, -300, 450), FVector(-2500, -300, 450), 4);
		B.NipLine(FVector(-3100, -300, 770), FVector(-2700, -300, 770), 3);
		B.NipLine(FVector(-4600, 0, 40), FVector(-1500, 0, 40), 6);
		B.Npc(ECreature::AlleyCat, 3, FVector(-4500, -200, 0), 30.f, TEXT("Bookie"), true);
		B.Foe(EEnemy::AlleyCat, FVector(-3000, 0, 0), 1, 1200.f);
		B.Foe(EEnemy::AlleyCat, FVector(-2200, 100, 0), 2, 1200.f);
		B.Check(FVector(-2400, 100, 0), 0.f);

		// ---- the courtyard and the cage
		B.Slab(0, 0, 1700, 1700, -100, 0, TEXT("T_Asphalt"), W, 256.f);
		Building(B, -1300, 1300, 1950, 250, 1300, Rgb(0.85f, 0.6f, 0.55f), 7);
		Building(B, -1300, 1300, -1950, 250, 1300, Rgb(0.7f, 0.7f, 0.8f), 8);
		B.Slab(1950, 0, 250, 1700, 0, 1400, TEXT("T_Brick"), Rgb(0.8f, 0.65f, 0.6f));
		B.Slab(-1450, 1050, 150, 650, 0, 1300, TEXT("T_Brick"), Rgb(0.9f, 0.6f, 0.55f));
		B.Slab(-1450, -1050, 150, 650, 0, 1300, TEXT("T_Brick"), Rgb(0.7f, 0.7f, 0.8f));
		const float CageR = 1028.f;  // octagon: flat sides 950 from the middle
		B.Cyl(FVector(0, 0, 0), 960.f, 30.f, 16, TEXT("T_Mat"), W, true);
		for (int32 s = 0; s < 8; ++s)
		{
			// side s faces s x 45 degrees; side 4 faces the alley (-X) and has the gate
			const float A0 = 2 * PI * (s - 0.5f) / 8, A1 = 2 * PI * (s + 0.5f) / 8;
			const FVector P0(FMath::Cos(A0) * CageR, FMath::Sin(A0) * CageR, 0), P1(FMath::Cos(A1) * CageR, FMath::Sin(A1) * CageR, 0);
			const FVector Mid = (P0 + P1) * 0.5f;
			const float Yaw = FMath::RadiansToDegrees((A0 + A1) * 0.5f);
			const bool bGate = s == 4;
			// bars every 60 cm and a top rail; the gate gap is the middle of the -X side
			const int32 NBars = 12;
			for (int32 b = 0; b <= NBars; ++b)
			{
				const FVector P = FMath::Lerp(P0, P1, float(b) / NBars);
				if (bGate && FMath::Abs(P.Y) < 180.f)
				{
					continue;
				}
				B.Cyl(P + FVector(0, 0, 30), 4.f, 420.f, 4, TEXT("T_White"), Rgb(0.6f, 0.62f, 0.66f), false);
			}
			B.Deco(Mid + FVector(0, 0, 450), FVector(10, FVector::Dist(P0, P1) * 0.5f, 6), TEXT("T_White"), Rgb(0.5f, 0.52f, 0.55f), 256.f, Yaw);
			B.Cyl(P0 + FVector(0, 0, 30), 12.f, 440.f, 6, TEXT("T_White"), Rgb(0.4f, 0.42f, 0.45f));
		}
		// the cage's invisible walls (the -X side has its gate gap)
		const float Side = 2 * CageR * FMath::Sin(PI / 8);
		for (int32 s = 0; s < 8; ++s)
		{
			const float A = 2 * PI * s / 8;
			const FVector N(FMath::Cos(A), FMath::Sin(A), 0);
			if (s == 4)
			{
				const float Half = (Side * 0.5f - 180.f) * 0.5f;
				for (float S : {-1.f, 1.f})
				{
					B.Wall(FVector(-975.f, S * (180.f + Half), 400), FVector(25, Half, 400));
				}
				continue;
			}
			B.Wall(N * 975.f + FVector(0, 0, 400), FVector(25, Side * 0.5f, 400), FMath::RadiansToDegrees(A));
		}
		B.Goal(TEXT("Cage"), FVector(-700, 0, 30), 160.f, true);
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = 2 * PI * i / 4 + PI / 4;
			B.Lamp(FVector(FMath::Cos(A) * 600.f, FMath::Sin(A) * 600.f, 900.f), Rgb(1.f, 0.95f, 0.85f), 1400.f, 30.f);
		}
		// the crowd: alley cats round the cage, cheering
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2 * PI * (i + 0.5f) / 12;
			if (FMath::Abs(FMath::Cos(A)) > 0.9f && FMath::Cos(A) < 0.f)
			{
				continue;
			}
			const FVector P(FMath::Cos(A) * 1250.f, FMath::Sin(A) * 1250.f, 0);
			B.Npc(ECreature::AlleyCat, i, P, FMath::RadiansToDegrees(A) + 180.f, NAME_None, false, true);
		}
		B.Npc(ECreature::Leo, 0, FVector(-1250, 380, 0), 0.f, TEXT("YoungLeo"), true);
		B.NipRing(FVector(0, 0, 80), 600.f, 8, 0.2f);
		return {FVector(-4800, 0, 60), 0.f, EMusic::Chinatown};
	}

	// ================================================================ chapter 3: pirates on the hull

	Levels::FSetup Chapter3(FB& B)
	{
		FTheme& T = B.Geo->Theme;
		T.Ambient = Rgb(0.45f, 0.45f, 0.56f);
		T.Sun = Rgb(0.65f, 0.62f, 0.7f);
		T.SunDir = FVector(-0.4f, -0.6f, 0.7f).GetSafeNormal();
		T.Fog = Rgb(0.02f, 0.01f, 0.05f);
		T.FogStart = 9000.f;
		T.FogEnd = 70000.f;
		T.SkyTop = Rgb(0.01f, 0.01f, 0.03f);
		T.SkyHorizon = Rgb(0.08f, 0.03f, 0.14f);
		T.SkyBottom = Rgb(0.03f, 0.f, 0.06f);
		T.Stars = 800;
		T.KillZ = -1800.f;
		T.Gravity = 1.15f;
		// a ringed planet and a nebula, far off
		B.Geo->Glow().Sphere(FVector(20000, -70000, -20000), FVector(16000), 10, 16, Rgb(0.22f, 0.16f, 0.32f));
		B.Geo->Glow().Sphere(FVector(-60000, 40000, 30000), FVector(25000, 25000, 9000), 6, 10, Rgb(0.15f, 0.04f, 0.18f));

		const FLinearColor Hull = Rgb(0.85f, 0.88f, 0.92f), Trim = Rgb(0.95f, 0.75f, 0.2f), Dark = Rgb(0.4f, 0.42f, 0.48f);
		// the spine
		B.Slab(0, 0, 2600, 450, -200, 0, TEXT("T_Hull"), Hull, 384.f);
		B.Slab(0, 0, 2600, 460, -260, -200, TEXT("T_Hull"), Trim, 256.f);
		for (float X = -2200.f; X <= 2200.f; X += 800.f)
		{
			B.Slab(X, 0, 60, 300, 0, 40, TEXT("T_Hull"), Dark);  // raised ribs
		}
		B.Slab(-1200, 280, 300, 90, 0, 160, TEXT("T_Crate"));  // cargo lashed to the hull: cover from the lasers
		B.Slab(700, -280, 200, 90, 0, 200, TEXT("T_Crate"));
		B.Slab(1800, 250, 150, 120, 0, 140, TEXT("T_Crate"));
		// the cockpit at the nose
		B.Slab(2900, 0, 320, 380, -200, 20, TEXT("T_Hull"), Hull);
		B.Cyl(FVector(2950, 0, 20), 300.f, 200.f, 12, TEXT("T_Glass"), W, true, 120.f);
		B.Slab(2500, 0, 90, 130, 0, 12, TEXT("T_Hazard"));
		B.Goal(TEXT("Cockpit"), FVector(2500, 0, 12), 140.f, false);
		// Leo's gun turret
		const FVector Turret(300, 0, 0);
		B.Cyl(Turret, 230.f, 110.f, 12, TEXT("T_Hull"), Dark);
		B.Cyl(Turret + FVector(0, 0, 110), 200.f, 40.f, 12, TEXT("T_Hull"), Trim);
		for (float S : {-40.f, 40.f})
		{
			B.Block(Turret + FVector(S, 300, 230), FVector(14, 140, 14), TEXT("T_Hull"), Dark);
		}
		B.Npc(ECreature::Leo, 0, Turret + FVector(0, 0, 150), 90.f, NAME_None, false);
		APirateShip::TurretPos = Turret + FVector(0, 400, 170);
		// sensor mast
		B.Cyl(FVector(1400, -200, 0), 30.f, 500.f, 6, TEXT("T_Hull"), Dark);
		B.Cyl(FVector(1400, -200, 500), 140.f, 40.f, 10, TEXT("T_Hull"), Trim, true, 60.f);
		// the wings, a level below
		for (float S : {-1.f, 1.f})
		{
			B.Slab(-700, S * 1150, 1000, 520, -320, -250, TEXT("T_Hull"), Hull, 384.f);
			B.Slab(-700, S * 1670, 1000, 20, -330, -180, TEXT("T_Hull"), Trim);
			B.Slab(-300, S * 600, 160, 160, -250, -120, TEXT("T_Hull"), Dark);  // step back up
			B.Slab(-1500, S * 1350, 120, 120, -250, -100, TEXT("T_Crate"));
			B.Geo->Glow().Box(FVector(-700, S * 1700, -250), FVector(900, 4, 6), Rgb(1.f, 0.3f, 0.3f) * (S > 0 ? 0.3f : 1.f) + Rgb(0.f, 1.f, 0.3f) * (S > 0 ? 1.f : 0.f));
		}
		// engines at the tail
		for (float S : {-1.f, 1.f})
		{
			B.Slab(-2850, S * 430, 380, 230, -260, 100, TEXT("T_Hull"), Dark);
			B.Geo->Glow().Box(FVector(-3240, S * 430, -80), FVector(10, 180, 150), Rgb(0.4f, 0.75f, 1.f));
			B.Geo->Glow().Box(FVector(-3300, S * 430, -80), FVector(40, 130, 100), Rgb(0.15f, 0.3f, 0.6f));
		}
		// boarding pods, with pirate dogs
		struct FPodAt { FVector P; FVector Out; };
		const FPodAt Pods[] = {
			{FVector(-800, -1250, -250), FVector(0, -1, 0.3f)},
			{FVector(-200, 1250, -250), FVector(0, 1, 0.3f)},
			{FVector(-2200, 0, 0), FVector(-1, 0, 0.5f)},
		};
		for (const FPodAt& Pd : Pods)
		{
			B.GM->Spawn<APod>(Pd.P)->Init(Pd.Out);
			for (int32 k = 0; k < 2; ++k)
			{
				const float A = PI * k + 0.6f;
				B.Foe(EEnemy::PirateDog, Pd.P + FVector(FMath::Cos(A) * 300.f, FMath::Sin(A) * 300.f, 0), k, 1100.f);
			}
			B.NipRing(Pd.P + FVector(0, 0, 290), 90.f, 3);
		}
		B.Check(FVector(1100, 150, 0), 180.f, TEXT("T_Hull"));
		B.Check(FVector(-1700, -150, 0), 0.f, TEXT("T_Hull"));
		B.NipLine(FVector(2000, 0, 60), FVector(800, 0, 60), 5);
		B.NipLine(FVector(-400, -800, -190), FVector(-1400, -1000, -190), 4);
		B.NipLine(FVector(-400, 800, -190), FVector(-1400, 1000, -190), 4);
		B.NipLine(FVector(-1200, 280, 220), FVector(-1200, 280, 360), 2);
		// the pirate ship cruising alongside
		B.GM->Spawn<APirateShip>(FVector(0, 6000, 400));
		return {FVector(1700, 0, 60), 180.f, EMusic::Pirates};
	}

	// ================================================================ chapter 4: the freezing ship

	/** A box-shaped room: floor, ceiling, and walls with door gaps (DoorX/DoorY: |coord| < half-width is open). */
	void Room(FB& B, float X0, float X1, float Y0, float Y1, float H, const TCHAR* Floor, const TCHAR* WallTex,
	          float DoorW_XNeg = 0.f, float DoorW_XPos = 0.f, float DoorW_YNeg = 0.f, float DoorW_YPos = 0.f,
	          float DoorCX_Y = 0.f, float DoorCY_X = 0.f)
	{
		const float Cx = (X0 + X1) * 0.5f, Cy = (Y0 + Y1) * 0.5f, Hx = (X1 - X0) * 0.5f, Hy = (Y1 - Y0) * 0.5f;
		const float T = 30.f;
		B.Slab(Cx, Cy, Hx, Hy, -60, 0, Floor, W, 256.f);
		B.Slab(Cx, Cy, Hx + T, Hy + T, H, H + 40, WallTex, Rgb(0.7f, 0.72f, 0.78f));
		auto WallX = [&](float X, float DoorW)
		{
			if (DoorW <= 0.f)
			{
				B.Slab(X, Cy, T, Hy + T, 0, H, WallTex);
				return;
			}
			const float D0 = DoorCX_Y - DoorW, D1 = DoorCX_Y + DoorW;
			B.Slab(X, (Y0 - T + D0) * 0.5f, T, (D0 - (Y0 - T)) * 0.5f, 0, H, WallTex);
			B.Slab(X, (D1 + Y1 + T) * 0.5f, T, ((Y1 + T) - D1) * 0.5f, 0, H, WallTex);
			B.Slab(X, DoorCX_Y, T, DoorW, 380, H, WallTex);
		};
		auto WallY = [&](float Y, float DoorW)
		{
			if (DoorW <= 0.f)
			{
				B.Slab(Cx, Y, Hx + T, T, 0, H, WallTex);
				return;
			}
			const float D0 = DoorCY_X - DoorW, D1 = DoorCY_X + DoorW;
			B.Slab((X0 - T + D0) * 0.5f, Y, (D0 - (X0 - T)) * 0.5f, T, 0, H, WallTex);
			B.Slab((D1 + X1 + T) * 0.5f, Y, ((X1 + T) - D1) * 0.5f, T, 0, H, WallTex);
			B.Slab(DoorCY_X, Y, DoorW, T, 380, H, WallTex);
		};
		WallX(X0 - T, DoorW_XNeg);
		WallX(X1 + T, DoorW_XPos);
		WallY(Y0 - T, DoorW_YNeg);
		WallY(Y1 + T, DoorW_YPos);
	}

	Levels::FSetup Chapter4(FB& B)
	{
		FTheme& T = B.Geo->Theme;
		T.Ambient = Rgb(0.52f, 0.5f, 0.56f);
		T.Sun = Rgb(0.2f, 0.2f, 0.22f);
		T.SunDir = FVector(0, 0, 1);
		T.Fog = Rgb(0.05f, 0.05f, 0.08f);
		T.FogStart = 2500.f;
		T.FogEnd = 9000.f;
		T.SkyTop = T.SkyHorizon = T.SkyBottom = Rgb(0.01f, 0.01f, 0.02f);
		T.Stars = 0;
		T.KillZ = -1000.f;
		T.Gravity = 2.4f;
		const FLinearColor Alarm = Rgb(1.f, 0.15f, 0.1f);

		// ---- Sparky's quarters
		Room(B, -600, 600, -500, 500, 520, TEXT("T_Carpet"), TEXT("T_Panel"), 0.f, 160.f);
		B.Slab(-350, 330, 220, 140, 0, 80, TEXT("T_Blanket"));                   // bed
		B.Slab(-560, 330, 30, 150, 0, 160, TEXT("T_Planks"), Rgb(0.6f, 0.4f, 0.3f));
		B.Slab(-350, 470, 200, 30, 330, 350, TEXT("T_Planks"), Rgb(0.6f, 0.4f, 0.3f));  // the shelf
		B.Item(EPickup::Welder, FVector(-350, 460, 400));
		B.Slab(300, 400, 200, 80, 0, 90, TEXT("T_Computer"));                      // desk
		B.Slab(300, 470, 150, 10, 120, 260, TEXT("T_Computer"));
		B.Slab(-100, -350, 60, 60, 0, 60, TEXT("T_Crate"));
		B.Cyl(FVector(450, -380, 0), 50.f, 60.f, 8, TEXT("T_White"), Rgb(0.7f, 0.4f, 0.25f));  // catnip plant
		B.Nip(FVector(450, -380, 120));
		B.Nip(FVector(-350, 330, 140));
		B.Lamp(FVector(0, 0, 480), Rgb(0.5f, 0.45f, 0.4f), 900.f, 0.f);
		B.Lamp(FVector(500, 0, 450), Alarm, 600.f, 14.f);

		// ---- corridor
		Room(B, 630, 3000, -200, 200, 400, TEXT("T_Grate"), TEXT("T_Panel"), 160.f, 160.f, 0.f, 260.f, 0.f, 1900.f);
		B.Haz(EHazard::Sparks, FVector(1250, -190, 50), FVector(0, 380, 0), 3.f, 0.f);
		B.Haz(EHazard::Sparks, FVector(2500, -190, 50), FVector(0, 380, 0), 2.6f, 0.5f);
		B.Repair(TEXT("C1"), FVector(1500, -150, 0), 0.f, false);
		B.NipLine(FVector(800, 0, 40), FVector(2900, 0, 40), 6);
		for (float X = 900.f; X < 3000.f; X += 500.f)
		{
			B.Lamp(FVector(X, 0, 370), FMath::IsNearlyZero(FMath::Fmod(X - 900.f, 1000.f)) ? Alarm : Rgb(0.4f, 0.45f, 0.5f), 450.f, 10.f);
		}
		B.Check(FVector(1000, 100, 0), 0.f, TEXT("T_Panel"));

		// ---- cockpit (off the corridor), with Leo at the controls
		Room(B, 1500, 2300, 260, 900, 450, TEXT("T_Grate"), TEXT("T_Panel"), 0.f, 0.f, 260.f, 0.f, 0.f, 1900.f);
		B.Slab(1900, 820, 350, 60, 0, 110, TEXT("T_Computer"));
		B.Geo->Glow().Box(FVector(1900, 925, 280), FVector(330, 4, 120), Rgb(0.08f, 0.1f, 0.25f));  // the big window
		for (int32 i = 0; i < 30; ++i)
		{
			B.Geo->Glow().Diamond(FVector(1600 + (i * 37) % 600, 921, 180 + (i * 53) % 200), 3.f, W);
		}
		B.Npc(ECreature::Leo, 0, FVector(1900, 700, 0), 90.f, TEXT("Leo"), true);
		B.Lamp(FVector(1900, 600, 400), Alarm, 700.f, 14.f);

		// ---- cargo bay: crates, a catwalk, steam vents, crates that float as gravity fails
		Room(B, 3060, 5200, -900, 900, 1300, TEXT("T_Grate"), TEXT("T_Panel"), 160.f, 220.f);
		for (float S : {-1.f, 1.f})
		{
			B.Slab(4130, S * 780, 1070, 120, 680, 720, TEXT("T_Grate"));   // catwalks along both long walls
		}
		B.Slab(5080, 0, 120, 660, 680, 720, TEXT("T_Grate"));
		B.Slab(3400, -600, 140, 140, 0, 220, TEXT("T_Crate"));
		B.Slab(3700, -650, 120, 120, 0, 420, TEXT("T_Crate"));
		B.Slab(3400, 400, 160, 160, 0, 180, TEXT("T_Crate"));
		B.Slab(4400, -200, 200, 120, 0, 160, TEXT("T_Crate"));
		B.Slab(4400, -200, 120, 100, 160, 300, TEXT("T_Crate"));
		B.Haz(EHazard::Steam, FVector(3950, 450, 0), FVector(0, 0, 900), 4.f, 0.f);
		B.Haz(EHazard::Steam, FVector(4800, -450, 0), FVector(0, 0, 900), 4.f, 0.5f);
		B.Mover(FVector(4000, -350, 80), FVector(90, 90, 80), TEXT("T_Crate"), W, EMove::Float, FVector(0, 0, 420), 5.f);
		B.Mover(FVector(4600, 300, 80), FVector(90, 90, 80), TEXT("T_Crate"), W, EMove::Float, FVector(0, 0, 560), 6.f, 0.3f);
		B.Mover(FVector(3600, 100, 80), FVector(80, 80, 80), TEXT("T_Crate"), W, EMove::Float, FVector(0, 0, 300), 4.5f, 0.6f);
		B.Repair(TEXT("C2"), FVector(3500, 760, 0), 0.f, false);
		B.Repair(TEXT("C3"), FVector(4700, -790, 720), 0.f, false);
		B.Haz(EHazard::Sparks, FVector(4200, -880, 760), FVector(0, 200, 0), 3.2f, 0.2f);
		B.NipLine(FVector(3500, -780, 780), FVector(4900, -780, 780), 5);
		B.NipLine(FVector(3500, 780, 780), FVector(4900, 780, 780), 5);
		B.Nip(FVector(3700, -650, 480));
		B.Lamp(FVector(4130, 0, 1250), Rgb(0.5f, 0.5f, 0.55f), 1500.f, 0.f);
		B.Lamp(FVector(3300, 0, 1000), Alarm, 900.f, 18.f);
		B.Lamp(FVector(5000, 0, 1000), Alarm, 900.f, 18.f);
		B.Check(FVector(3300, 0, 0), 0.f, TEXT("T_Panel"));

		// ---- engine room: the climate control module
		Room(B, 5260, 7000, -700, 700, 1000, TEXT("T_Grate"), TEXT("T_Panel"), 220.f, 0.f);
		B.Repair(TEXT("Module"), FVector(6650, 0, 0), 180.f, true);
		B.Slab(5900, 520, 300, 150, 380, 420, TEXT("T_Grate"));
		B.Slab(5450, 300, 120, 120, 0, 200, TEXT("T_Crate"));
		B.Repair(TEXT("C4"), FVector(5950, 600, 420), 0.f, false);
		B.Haz(EHazard::Steam, FVector(6000, -300, 0), FVector(0, 0, 700), 3.5f, 0.2f);
		B.Haz(EHazard::Sparks, FVector(5700, -680, 60), FVector(0, 0, 300), 2.f, 0.f);
		for (float S : {-1.f, 1.f})
		{
			B.Cyl(FVector(6200, S * 520, 0), 110.f, 1000.f, 10, TEXT("T_Hull"), Rgb(0.5f, 0.5f, 0.55f));
			B.Geo->Glow().Box(FVector(6200, S * 520, 500), FVector(112, 112, 30), Rgb(0.3f, 0.7f, 1.f));
		}
		B.NipRing(FVector(6100, 0, 60), 250.f, 6);
		B.Lamp(FVector(6100, 0, 900), Rgb(0.55f, 0.55f, 0.6f), 1300.f, 0.f);
		B.Lamp(FVector(6600, 0, 700), Alarm, 900.f, 16.f);
		return {FVector(-150, 120, 60), 0.f, EMusic::Crisis};
	}

	// ================================================================ chapter 5: Fluffer-Nutter

	/** A jungle tree; with bClimb, shelf fungi spiral up the trunk to the canopy. */
	void JungleTree(FB& B, const FVector& Base, float R, float H, float Canopy, bool bClimb)
	{
		B.Cyl(Base, R, H, 9, TEXT("T_Bark"), W, true, R * 0.8f);
		B.Cyl(Base + FVector(0, 0, H), Canopy, 40.f, 12, TEXT("T_Leaves"));
		FRetroMesh& L = B.Geo->Deco(TEXT("T_Leaves"));
		L.Sphere(Base + FVector(0, 0, H + 160.f), FVector(Canopy * 1.1f, Canopy * 1.1f, 220.f), 4, 8, Rgb(0.85f, 1.f, 0.85f));
		for (int32 i = 0; i < 4; ++i)
		{
			const float A = i * PI / 2 + 0.4f;
			L.Sphere(Base + FVector(FMath::Cos(A) * Canopy * 0.7f, FMath::Sin(A) * Canopy * 0.7f, H + 60.f), FVector(Canopy * 0.5f, Canopy * 0.5f, 140.f), 3, 6, W);
		}
		if (bClimb)
		{
			for (int32 i = 0; i < 7; ++i)
			{
				const float A = i * PI / 3.f;
				const FVector P = Base + FVector(FMath::Cos(A) * (R + 110.f), FMath::Sin(A) * (R + 110.f), 150.f * (i + 1) - 30.f);
				B.Cyl(P, 120.f, 30.f, 8, TEXT("T_Fungus"));
				B.Nip(P + FVector(0, 0, 90));
			}
		}
	}

	void ThatchHut(FB& B, const FVector& Base, float R)
	{
		B.Cyl(Base, R, 260.f, 10, TEXT("T_Bark"), Rgb(0.9f, 0.8f, 0.7f));
		B.Cyl(Base + FVector(0, 0, 260), R + 80.f, R * 1.1f, 10, TEXT("T_Thatch"), W, true, 0.f);
		B.Deco(Base + FVector(R - 4.f, 0, 85), FVector(8, 50, 85), TEXT("T_White"), Rgb(0.15f, 0.1f, 0.06f));
	}

	Levels::FSetup Chapter5(FB& B)
	{
		FTheme& T = B.Geo->Theme;
		T.Ambient = Rgb(0.62f, 0.66f, 0.6f);
		T.Sun = Rgb(0.55f, 0.5f, 0.4f);
		T.SunDir = FVector(0.4f, 0.3f, 0.85f).GetSafeNormal();
		T.Fog = Rgb(0.6f, 0.8f, 0.8f);
		T.FogStart = 6000.f;
		T.FogEnd = 32000.f;
		T.SkyTop = Rgb(0.25f, 0.5f, 0.95f);
		T.SkyHorizon = Rgb(0.8f, 0.92f, 1.f);
		T.SkyBottom = Rgb(0.3f, 0.5f, 0.6f);
		T.Stars = 0;
		T.KillZ = -40.f;
		T.Gravity = 2.4f;

		B.Deco(FVector(0, 0, -60), FVector(40000, 40000, 2), TEXT("T_Water"), W, 512.f);
		FRandomStream R(55);
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2 * PI * i / 12;
			const FVector P = FVector(2500, 0, -400) + FVector(FMath::Cos(A), FMath::Sin(A), 0) * R.FRandRange(12000.f, 18000.f);
			B.Cyl(P, R.FRandRange(1500.f, 3000.f), R.FRandRange(2500.f, 5000.f), 7, TEXT("T_JungleGrass"), Rgb(0.6f, 0.75f, 0.65f), false, R.FRandRange(300.f, 800.f));
		}

		// ---- the island, the ship's landing spot
		B.Slab(2000, 0, 2600, 2200, -400, 0, TEXT("T_JungleGrass"), W, 384.f);
		SparkyShip(B, FVector(-100, 0, 0));
		B.Goal(TEXT("Ship"), FVector(-100, -470, 0), 170.f, false);
		B.GM->Spawn<ALeoBuddy>(FVector(300, -750, 120))->Init(ECreature::Leo, -160.f);
		B.Check(FVector(500, 300, 0), 0.f);
		for (int32 i = 0; i < 40; ++i)
		{
			// bushes and flowers
			const FVector P(R.FRandRange(-500.f, 4500.f), R.FRandRange(-2100.f, 2100.f), 0.f);
			if (FVector::Dist2D(P, FVector(-100, 0, 0)) < 700.f || FVector::Dist2D(P, FVector(1800, -1100, 0)) < 900.f || FVector::Dist2D(P, Levels::Spots::Ch5Forest) < 450.f)
			{
				continue;
			}
			B.Geo->Deco(TEXT("T_Leaves")).Sphere(P, FVector(R.FRandRange(60.f, 140.f), R.FRandRange(60.f, 140.f), R.FRandRange(40.f, 90.f)), 3, 6, W);
			if (i % 3 == 0)
			{
				B.Geo->Glow().Diamond(P + FVector(0, 0, 100), 12.f, R.FRand() < 0.5f ? Rgb(1.f, 0.5f, 0.7f) : Rgb(1.f, 0.9f, 0.3f));
			}
		}

		// ---- the village
		const FVector Village(1800, -1100, 0);
		ThatchHut(B, FVector(1200, -1750, 0), 220.f);
		ThatchHut(B, FVector(1850, -1850, 0), 240.f);
		ThatchHut(B, FVector(2650, -1250, 0), 220.f);
		ThatchHut(B, FVector(1250, -600, 0), 200.f);
		ThatchHut(B, FVector(2700, -500, 0), 210.f);
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = 2 * PI * i / 8;
			B.Block(Village + FVector(FMath::Cos(A) * 90.f, FMath::Sin(A) * 90.f, 15), FVector(22, 22, 15), TEXT("T_Rock"));
		}
		B.Geo->Glow().Diamond(Village + FVector(0, 0, 60), 40.f, Rgb(1.f, 0.55f, 0.15f), 1.5f);
		B.Lamp(Village + FVector(0, 0, 120), Rgb(1.f, 0.55f, 0.2f), 900.f);
		for (int32 r = 0; r < 4; ++r)
		{
			// the gardens where Fluffernutter hides
			B.Deco(FVector(2250, -1050 + r * 110.f, 15), FVector(220, 25, 15), TEXT("T_Leaves"), Rgb(0.8f, 1.f, 0.6f));
		}
		B.Npc(ECreature::Villager, 0, Village + FVector(0, 220, 0), 180.f, TEXT("Chief"), true);
		B.Npc(ECreature::Villager, 1, FVector(1500, -1000, 0), 30.f, TEXT("Villager0"), true);
		B.Npc(ECreature::Villager, 2, FVector(2100, -1500, 0), 120.f, TEXT("Villager1"), true);
		B.Npc(ECreature::Villager, 3, FVector(2400, -800, 0), -60.f, TEXT("Villager2"), true);
		B.Npc(ECreature::Villager, 4, FVector(1100, -1250, 0), 0.f, TEXT("Villager3"), true);
		B.Check(FVector(1100, -900, 0), 0.f);
		B.NipLine(FVector(700, -400, 40), FVector(1500, -900, 40), 5);
		B.NipRing(Village + FVector(0, 0, 40), 260.f, 6);

		// ---- the forest
		JungleTree(B, FVector(2400, 600, 0), 110.f, 900.f, 380.f, false);
		JungleTree(B, FVector(2450, 1650, 0), 130.f, 1000.f, 420.f, false);
		JungleTree(B, FVector(2950, 1950, 0), 120.f, 950.f, 380.f, false);
		JungleTree(B, FVector(3500, 550, 0), 140.f, 1050.f, 420.f, false);
		JungleTree(B, FVector(4200, 1100, 0), 120.f, 900.f, 380.f, false);
		JungleTree(B, FVector(2150, 1150, 0), 100.f, 850.f, 340.f, false);
		JungleTree(B, FVector(4250, 1950, 0), 110.f, 900.f, 360.f, false);
		JungleTree(B, FVector(3600, 1500, 0), 150.f, 1100.f, 450.f, true);  // the great tree
		B.Item(EPickup::Berry, FVector(3600, 1500, 1200));
		B.Goal(TEXT("Forest"), Levels::Spots::Ch5Forest, 260.f, true);
		B.Npc(ECreature::Fluffernutter, 0, Levels::Spots::Ch5Forest + FVector(80, 60, 0), 200.f, TEXT("Fluff"), false);
		B.Foe(EEnemy::BogBlob, FVector(3300, 1750, 0), 1, 600.f);
		B.Foe(EEnemy::BogBlob, FVector(2650, 750, 0), 1, 600.f);
		B.Check(FVector(2600, 1100, 0), 0.f);
		B.NipLine(FVector(1800, 300, 40), FVector(2700, 1000, 40), 5);
		B.Lamp(Levels::Spots::Ch5Forest + FVector(0, 0, 400), Rgb(0.6f, 1.f, 0.6f), 900.f);

		// ---- the cliff and the falls
		B.Slab(4200, -1300, 400, 600, 0, 700, TEXT("T_Rock"), Rgb(0.8f, 0.9f, 0.8f));
		B.Geo->Glow().Box(FVector(3796, -1300, 350), FVector(3, 180, 350), Rgb(0.35f, 0.6f, 0.9f));
		B.Geo->Glow().Box(FVector(3700, -1300, 5), FVector(100, 220, 3), Rgb(0.4f, 0.6f, 0.8f));
		B.Slab(3950, -600, 100, 100, 0, 180, TEXT("T_Rock"));
		B.Slab(4150, -600, 100, 100, 0, 360, TEXT("T_Rock"));
		B.Slab(4350, -600, 100, 100, 0, 540, TEXT("T_Rock"));
		B.Item(EPickup::Berry, FVector(3880, -1500, 790));
		B.Foe(EEnemy::BogBlob, FVector(4300, -1100, 700), 1, 400.f);
		B.Foe(EEnemy::BogBlob, FVector(4100, -1700, 700), 1, 400.f);
		B.Check(FVector(3700, -800, 0), 0.f);
		B.Nip(FVector(3950, -600, 240));
		B.Nip(FVector(4150, -600, 420));
		B.Nip(FVector(4350, -600, 600));

		// ---- the rock in the lagoon, over floating logs
		for (int32 i = 0; i < 3; ++i)
		{
			const FVector P(4850 + i * 300.f, i % 2 ? 700.f : 500.f, -25.f);
			B.Mover(P, FVector(110, 70, 15), TEXT("T_Bark"), W, EMove::Bob, FVector(0, 0, 10), 2.5f + i * 0.4f, i * 0.3f);
			B.Nip(P + FVector(0, 0, 80));
		}
		B.Cyl(FVector(5650, 330, -400), 130.f, 620.f, 8, TEXT("T_Rock"));
		B.Cyl(FVector(5900, 620, -400), 260.f, 820.f, 9, TEXT("T_Rock"));
		B.Item(EPickup::Berry, FVector(5900, 620, 520));
		return {FVector(650, -950, 60), 0.f, EMusic::Jungle};
	}

	// ================================================================ chapter 6: Fluffernutter's revenge

	Levels::FSetup Chapter6(FB& B)
	{
		FTheme& T = B.Geo->Theme;
		T.Ambient = Rgb(0.5f, 0.46f, 0.5f);
		T.Sun = Rgb(0.2f, 0.18f, 0.2f);
		T.SunDir = FVector(0, 0, 1);
		T.Fog = Rgb(0.08f, 0.03f, 0.04f);
		T.FogStart = 3000.f;
		T.FogEnd = 10000.f;
		T.SkyTop = T.SkyHorizon = T.SkyBottom = Rgb(0.01f, 0.01f, 0.02f);
		T.Stars = 0;
		T.KillZ = -1000.f;
		T.Gravity = 2.4f;
		const FLinearColor Alarm = Rgb(1.f, 0.15f, 0.1f);

		// the cargo bay (the pirates came in through the wall), a corridor, the engine room
		Room(B, 0, 2400, -1000, 1000, 1200, TEXT("T_Grate"), TEXT("T_Panel"), 0.f, 220.f);
		Room(B, 2460, 3600, -220, 220, 420, TEXT("T_Grate"), TEXT("T_Panel"), 220.f, 220.f);
		Room(B, 3660, 5400, -800, 800, 1000, TEXT("T_Grate"), TEXT("T_Panel"), 220.f, 0.f);
		B.Slab(900, 600, 140, 140, 0, 200, TEXT("T_Crate"));
		B.Slab(1500, -650, 120, 160, 0, 180, TEXT("T_Crate"));
		B.Slab(500, -550, 100, 100, 0, 150, TEXT("T_Crate"));
		B.Slab(2000, 750, 160, 110, 0, 240, TEXT("T_Crate"));
		// the breach: a boarding tube punched through the hull
		for (int32 i = 0; i < 16; ++i)
		{
			const float A = 2 * PI * i / 16;
			B.Geo->Glow().Box(FVector(1300 + FMath::Cos(A) * 220.f, 1028, 320 + FMath::Sin(A) * 220.f), FVector(30, 4, 8), FRotator(0, 0, 0), Rgb(1.f, 0.4f, 0.1f));
		}
		B.Deco(FVector(1300, 1029, 320), FVector(200, 2, 200), TEXT("T_White"), Rgb(0.05f, 0.02f, 0.02f));
		B.Npc(ECreature::Sparky, 0, FVector(250, 780, 0), -60.f, TEXT("Sparky"), true);
		B.Npc(ECreature::Leo, 0, FVector(420, 820, 0), -80.f, TEXT("Leo"), true);
		B.Npc(ECreature::Fluffernutter, 0, FVector(700, 0, 0), 0.f, TEXT("FluffSmall"), false);
		for (float S : {-1.f, 1.f})
		{
			B.Cyl(FVector(4700, S * 520, 0), 110.f, 1000.f, 10, TEXT("T_Hull"), Rgb(0.5f, 0.5f, 0.55f));
			B.Geo->Glow().Box(FVector(4700, S * 520, 500), FVector(112, 112, 30), Rgb(0.3f, 0.7f, 1.f));
		}
		B.Slab(5200, 0, 150, 300, 0, 280, TEXT("T_Computer"));
		B.NipRing(FVector(1200, 0, 60), 500.f, 8);
		B.NipLine(FVector(2600, 0, 60), FVector(3500, 0, 60), 4);
		B.Lamp(FVector(1200, 0, 1150), Rgb(0.5f, 0.45f, 0.45f), 1500.f);
		B.Lamp(FVector(300, 0, 900), Alarm, 1000.f, 18.f);
		B.Lamp(FVector(2200, 0, 900), Alarm, 1000.f, 18.f);
		B.Lamp(FVector(3000, 0, 380), Alarm, 600.f, 12.f);
		B.Lamp(FVector(4500, 0, 900), Rgb(0.5f, 0.5f, 0.6f), 1300.f);
		B.Lamp(FVector(5000, 0, 700), Alarm, 900.f, 16.f);
		return {FVector(700, 0, 120), 0.f, EMusic::Pirates};
	}

	// ================================================================ chapter 7: the Labyrinth of Time

	void VoidTheme(FTheme& T, const FLinearColor& Horizon)
	{
		T.Ambient = Rgb(0.52f, 0.5f, 0.62f);
		T.Sun = Rgb(0.4f, 0.36f, 0.5f);
		T.SunDir = FVector(0.3f, -0.5f, 0.8f).GetSafeNormal();
		T.Fog = Rgb(0.08f, 0.03f, 0.12f);
		T.FogStart = 5000.f;
		T.FogEnd = 40000.f;
		T.SkyTop = Rgb(0.02f, 0.f, 0.06f);
		T.SkyHorizon = Horizon;
		T.SkyBottom = Rgb(0.05f, 0.02f, 0.15f);
		T.Stars = 600;
		T.KillZ = -1200.f;
	}

	/** Old time-worn ornaments drifting in the void: broken pillars and clock rings. */
	void VoidRuins(FB& B, int32 Seed, const FVector& Center, float Spread)
	{
		FRandomStream R(Seed);
		for (int32 i = 0; i < 18; ++i)
		{
			const FVector P = Center + FVector(R.FRandRange(-Spread, Spread), R.FRandRange(-Spread, Spread), R.FRandRange(-1500.f, 1500.f));
			if (i % 2)
			{
				B.Cyl(P, R.FRandRange(60.f, 140.f), R.FRandRange(300.f, 900.f), 8, TEXT("T_Rune"), Rgb(0.7f, 0.7f, 0.8f), false);
			}
			else
			{
				const float Rad = R.FRandRange(200.f, 600.f);
				for (int32 k = 0; k < 12; ++k)
				{
					const float A = 2 * PI * k / 12;
					B.Geo->Glow().Box(P + FVector(FMath::Cos(A) * Rad, 0, FMath::Sin(A) * Rad), FVector(6, 6, Rad * 0.2f), FRotator(0, 0, -FMath::RadiansToDegrees(A)), Rgb(0.4f, 0.3f, 0.7f));
				}
				B.Geo->Glow().Box(P + FVector(0, 0, Rad * 0.35f), FVector(4, 4, Rad * 0.35f), Rgb(0.6f, 0.5f, 0.9f));
			}
		}
	}

	ANpc* Ghost(FB& B, ECreature K, const FVector& Feet, float Yaw, FName Id, const FLinearColor& Tint)
	{
		ANpc* N = B.Npc(K, 1, Feet, Yaw, Id, true);
		N->GetRig()->SetHat(false);
		N->GetRig()->SetTint(Tint);
		return N;
	}

	void Portal(FB& B, FName Id, const FVector& At, float Yaw, const FLinearColor& Col)
	{
		B.GM->Spawn<ATeleporter>(At, Yaw)->Init(Id, Col);
	}

	Levels::FSetup Chapter7(FB& B)
	{
		VoidTheme(B.Geo->Theme, Rgb(0.28f, 0.1f, 0.4f));
		B.Geo->Theme.Gravity = 2.2f;
		const FLinearColor Ghostly(0.5f, 1.1f, 1.3f);
		VoidRuins(B, 7, FVector(0, 3000, 0), 6000.f);

		// ---- the start, and the bridge north to the heart
		B.Slab(0, 0, 400, 400, -100, 0, TEXT("T_Rune"));
		B.GM->Spawn<ALeoBuddy>(FVector(-200, -200, 120))->Init(ECreature::Leo, -170.f);
		B.GM->Spawn<ALeoBuddy>(FVector(200, -200, 60))->Init(ECreature::Fluffernutter, 170.f);
		B.Check(FVector(0, -250, 0), 90.f, TEXT("T_Rune"));
		B.Slab(0, 900, 150, 500, -60, 0, TEXT("T_Rune"));
		B.Cyl(FVector(0, 2100, -200), 700.f, 200.f, 16, TEXT("T_Rune"));
		B.Goal(TEXT("Core"), FVector(0, 2100, 0), 230.f, false);
		for (int32 i = 0; i < 24; ++i)
		{
			const float A = 2 * PI * i / 24;
			B.Geo->Glow().Box(FVector(FMath::Cos(A) * 500.f, 2100 + FMath::Sin(A) * 500.f, 4), FVector(40, 6, 3), FRotator(0, FMath::RadiansToDegrees(A) + 90.f, 0), Rgb(0.5f, 0.3f, 0.9f));
		}
		Ghost(B, ECreature::Fluffernutter, FVector(400, 2300, 0), 180.f, TEXT("EchoFluff"), Ghostly);
		B.NipLine(FVector(0, 450, 50), FVector(0, 1350, 50), 4);

		// ---- west: narrow walkways, rifts drifting across
		B.Slab(-750, 0, 250, 120, -60, 0, TEXT("T_Rune"));
		B.Slab(-1500, 0, 250, 100, -60, 0, TEXT("T_Rune"));
		B.Slab(-2250, 0, 250, 70, -60, 0, TEXT("T_Rune"));
		B.Slab(-3000, 0, 350, 350, -100, 0, TEXT("T_Rune"));
		B.Haz(EHazard::Rift, FVector(-1500, -450, 60), FVector(0, 900, 0), 3.f);
		B.Haz(EHazard::Rift, FVector(-2250, 450, 60), FVector(0, -900, 0), 2.6f, 0.3f);
		B.Haz(EHazard::Rift, FVector(-2450, -450, 60), FVector(0, 900, 0), 3.4f, 0.7f);
		B.Item(EPickup::Shard, FVector(-3100, 0, 120));
		Ghost(B, ECreature::Sparky, FVector(-2850, 200, 0), 0.f, TEXT("EchoSparky"), Ghostly);
		B.Check(FVector(-750, 0, 0), 180.f, TEXT("T_Rune"));
		B.NipLine(FVector(-600, 0, 50), FVector(-2400, 0, 50), 6);

		// ---- east: across the gap inside a gravity distortion
		B.Slab(800, 0, 400, 120, -60, 0, TEXT("T_Rune"));
		B.Slab(1600, 0, 250, 250, -80, 0, TEXT("T_Rune"));
		B.Check(FVector(1600, -150, 0), 0.f, TEXT("T_Rune"));
		Ghost(B, ECreature::Leo, FVector(1700, 150, 0), 180.f, TEXT("EchoLeo"), Ghostly);
		B.Haz(EHazard::Gravity, FVector(2650, 0, 350), FVector(950, 0, 0), 1.f);
		B.Slab(2250, 0, 100, 100, 100, 160, TEXT("T_Rune"));
		B.Slab(2700, 200, 100, 100, 330, 390, TEXT("T_Rune"));
		B.Slab(3100, -150, 100, 100, 560, 620, TEXT("T_Rune"));
		B.Slab(3650, 0, 300, 300, 700, 800, TEXT("T_Rune"));
		B.Item(EPickup::Shard, FVector(3700, 0, 900));
		B.Nip(FVector(2250, 0, 220));
		B.Nip(FVector(2700, 200, 450));
		B.Nip(FVector(3100, -150, 680));

		// ---- through the wormhole: a far-off island of moving stones
		Portal(B, TEXT("C"), FVector(0, 2650, 0), 90.f, Rgb(0.6f, 0.4f, 1.f));
		Portal(B, TEXT("C"), FVector(0, 7000, 0), 90.f, Rgb(0.6f, 0.4f, 1.f));
		B.Slab(0, 7000, 300, 300, -100, 0, TEXT("T_Rune"));
		B.Slab(0, 7650, 120, 300, -60, 0, TEXT("T_Rune"));
		B.Mover(FVector(-450, 8250, -15), FVector(130, 130, 15), TEXT("T_Rune"), W, EMove::Linear, FVector(900, 0, 0), 4.f, 0.f, true);
		B.Slab(0, 8850, 220, 220, -60, 0, TEXT("T_Rune"));
		B.Check(FVector(0, 8750, 0), 0.f, TEXT("T_Rune"));
		B.Haz(EHazard::Rift, FVector(-400, 8850, 60), FVector(800, 0, 0), 2.8f);
		B.Slab(450, 8850, 100, 100, 0, 150, TEXT("T_Rune"));
		B.Slab(750, 8850, 100, 100, 0, 300, TEXT("T_Rune"));
		B.Slab(1050, 8850, 100, 100, 0, 450, TEXT("T_Rune"));
		B.Slab(1450, 8850, 220, 220, 0, 600, TEXT("T_Rune"));
		B.Item(EPickup::Shard, FVector(1450, 8850, 720));
		Portal(B, TEXT("Home"), FVector(1450, 9000, 600), 90.f, Rgb(0.3f, 1.f, 0.7f));
		Portal(B, TEXT("Home"), FVector(250, -250, 0), 90.f, Rgb(0.3f, 1.f, 0.7f));
		B.NipLine(FVector(0, 7400, 50), FVector(0, 7900, 50), 3);
		VoidRuins(B, 77, FVector(0, 8000, 0), 3000.f);
		return {FVector(0, -100, 60), 90.f, EMusic::Labyrinth};
	}

	// ================================================================ chapter 8: Riddles of the Ancients

	/** A round of invisible wall, to keep fights from spilling into the void. */
	void Ring(FB& B, const FVector& C, float R, int32 Sides = 16, float H = 600.f)
	{
		for (int32 s = 0; s < Sides; ++s)
		{
			const float A = 2 * PI * s / Sides;
			const FVector N(FMath::Cos(A), FMath::Sin(A), 0);
			B.Wall(C + N * (R + 25.f) + FVector(0, 0, H * 0.5f), FVector(25, R * FMath::Tan(PI / Sides) + 10.f, H * 0.5f), FMath::RadiansToDegrees(A));
		}
	}

	Levels::FSetup Chapter8(FB& B)
	{
		VoidTheme(B.Geo->Theme, Rgb(0.08f, 0.2f, 0.3f));
		B.Geo->Theme.Gravity = 2.4f;
		using namespace Levels::Spots;
		VoidRuins(B, 8, FVector::ZeroVector, 8000.f);

		// ---- the hub: a round chamber, its hologram, three portals
		B.Cyl(Ch8Hub - FVector(0, 0, 200), 1100.f, 200.f, 16, TEXT("T_Marble"));
		Ring(B, Ch8Hub, 1100.f);
		for (int32 i = 0; i < 8; ++i)
		{
			const float A = 2 * PI * i / 8 + PI / 8;
			B.Cyl(Ch8Hub + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 1000.f, 60.f, 700.f, 8, TEXT("T_Marble"));
			B.Lamp(Ch8Hub + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 900.f + FVector(0, 0, 500), Rgb(0.3f, 0.8f, 0.9f), 700.f, 14.f);
		}
		for (int32 r = 0; r < 3; ++r)
		{
			// the holographic interface: rings of ancient symbols
			const float Rad = 140.f + r * 60.f;
			for (int32 k = 0; k < 16; ++k)
			{
				const float A = 2 * PI * k / 16 + r;
				B.Geo->Glow().Box(Ch8Hub + FVector(FMath::Cos(A) * Rad, FMath::Sin(A) * Rad, 420 + r * 50.f), FVector(10, 3, 14), FRotator(0, FMath::RadiansToDegrees(A), 0), Rgb(0.3f, 0.9f, 1.f) * (1.f - r * 0.2f));
			}
		}
		B.Lamp(Ch8Hub + FVector(0, 0, 450), Rgb(0.4f, 0.9f, 1.f), 1200.f, 30.f);
		B.GM->Spawn<ALeoBuddy>(FVector(-300, -500, 120))->Init(ECreature::Leo, -170.f);
		B.GM->Spawn<ALeoBuddy>(FVector(300, -500, 60))->Init(ECreature::Fluffernutter, 170.f);
		B.Check(FVector(0, -400, 0), 90.f, TEXT("T_Marble"));
		Portal(B, TEXT("A"), Ch8Hub + FVector(0, -800, 0), 90.f, Rgb(0.3f, 0.9f, 1.f));
		Portal(B, TEXT("B"), Ch8Hub + FVector(800, 0, 0), 180.f, Rgb(1.f, 0.4f, 0.8f));
		Portal(B, TEXT("C"), Ch8Hub + FVector(-800, 0, 0), 0.f, Rgb(1.f, 0.85f, 0.3f));
		B.NipRing(Ch8Hub + FVector(0, 0, 50), 600.f, 8, 0.4f);

		// ---- the lost scientist's laboratory: the quantum tiles
		B.Slab(Ch8Lab.X, Ch8Lab.Y, 800, 800, -100, 0, TEXT("T_Marble"));
		Ring(B, Ch8Lab, 820.f, 12, 400.f);
		Portal(B, TEXT("A"), Ch8Lab + FVector(0, -620, 0), 90.f, Rgb(0.3f, 0.9f, 1.f));
		for (int32 i = 0; i < 9; ++i)
		{
			B.GM->Spawn<AFloorPad>(Ch8Lab + FVector((i % 3 - 1) * 200.f, (i / 3 - 1) * 200.f, 12.f))->Init(i, true);
		}
		Ghost(B, ECreature::Elder, Ch8Lab + FVector(-500, -400, 0), 45.f, TEXT("Scientist"), FLinearColor(0.5f, 1.1f, 1.3f));
		B.Lamp(Ch8Lab + FVector(0, 0, 500), Rgb(0.4f, 0.9f, 1.f), 1100.f);

		// ---- the warrior princess's arena
		B.Cyl(Ch8Arena - FVector(0, 0, 200), 1000.f, 200.f, 16, TEXT("T_Marble"));
		Ring(B, Ch8Arena, 1000.f);
		Portal(B, TEXT("B"), Ch8Arena + FVector(-780, 0, 0), 0.f, Rgb(1.f, 0.4f, 0.8f));
		Ghost(B, ECreature::Local, Ch8Arena + FVector(-500, 450, 0), -45.f, TEXT("Princess"), FLinearColor(1.2f, 0.75f, 1.2f));
		for (int32 i = 0; i < 6; ++i)
		{
			const float A = 2 * PI * i / 6;
			B.Cyl(Ch8Arena + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 900.f, 50.f, 500.f, 8, TEXT("T_Marble"));
			// the hidden arsenal: ancient weapons along the walls
			B.Block(Ch8Arena + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 960.f + FVector(0, 0, 250), FVector(6, 20, 160), TEXT("T_White"), Rgb(0.8f, 0.7f, 0.4f), 256.f, FMath::RadiansToDegrees(A));
		}
		B.Lamp(Ch8Arena + FVector(0, 0, 600), Rgb(1.f, 0.6f, 0.9f), 1300.f);

		// ---- the riddle stones
		B.Slab(Ch8Riddles.X, Ch8Riddles.Y, 700, 500, -100, 0, TEXT("T_Marble"));
		Ring(B, Ch8Riddles, 720.f, 12, 400.f);
		Portal(B, TEXT("C"), Ch8Riddles + FVector(600, 0, 0), 180.f, Rgb(1.f, 0.85f, 0.3f));
		for (int32 i = 0; i < 3; ++i)
		{
			B.Cyl(Ch8Riddles + FVector(-400, (i - 1) * 300.f, 0), 70.f, 60.f, 8, TEXT("T_Marble"));
			B.Npc(ECreature::Being, i, Ch8Riddles + FVector(-400, (i - 1) * 300.f, 60), 0.f, FName(*FString::Printf(TEXT("Riddle%d"), i)), true);
		}
		B.Lamp(Ch8Riddles + FVector(0, 0, 500), Rgb(1.f, 0.85f, 0.5f), 1100.f);
		return {FVector(0, -250, 60), 90.f, EMusic::Labyrinth};
	}

	// ================================================================ chapter 9: another dimension

	/** A floating island: a disc on top, a rocky cone hanging under it, maybe some crystals. */
	void Isle(FB& B, const FVector& Top, float R, int32 Crystals = 0)
	{
		B.Cyl(Top - FVector(0, 0, 60), R, 60.f, 14, TEXT("T_Island"));
		B.Cyl(Top - FVector(0, 0, 60 + R * 0.9f), 40.f, R * 0.9f, 10, TEXT("T_Rock"), Rgb(0.6f, 0.5f, 0.75f), false, R * 0.95f);
		FRandomStream Rnd(int32(Top.X * 7 + Top.Y * 3));
		for (int32 i = 0; i < Crystals; ++i)
		{
			const float A = Rnd.FRandRange(0.f, 2 * PI);
			const float D = Rnd.FRandRange(R * 0.5f, R * 0.85f);
			const FVector P = Top + FVector(FMath::Cos(A) * D, FMath::Sin(A) * D, 0);
			B.Cyl(P, Rnd.FRandRange(25.f, 50.f), Rnd.FRandRange(120.f, 280.f), 6, TEXT("T_Crystal"), W, true, 0.f);
			B.Geo->Glow().Diamond(P + FVector(0, 0, 60), 20.f, Rgb(0.6f, 0.4f, 1.f), 1.6f);
		}
	}

	Levels::FSetup Chapter9(FB& B)
	{
		FTheme& T = B.Geo->Theme;
		T.Ambient = Rgb(0.7f, 0.65f, 0.75f);
		T.Sun = Rgb(0.4f, 0.4f, 0.5f);
		T.SunDir = FVector(-0.3f, 0.4f, 0.85f).GetSafeNormal();
		T.Fog = Rgb(0.45f, 0.3f, 0.58f);
		T.FogStart = 7000.f;
		T.FogEnd = 45000.f;
		T.SkyTop = Rgb(0.12f, 0.04f, 0.3f);
		T.SkyHorizon = Rgb(0.8f, 0.45f, 0.75f);
		T.SkyBottom = Rgb(0.2f, 0.6f, 0.75f);
		T.Stars = 300;
		T.KillZ = -350.f;
		T.Gravity = 1.6f;

		// the sea of glowing energy
		B.Deco(FVector(0, 0, -400), FVector(40000, 40000, 2), TEXT("T_Energy"), W, 1024.f);
		FRandomStream R(99);
		for (int32 i = 0; i < 40; ++i)
		{
			B.Geo->Glow().Diamond(FVector(R.FRandRange(-9000.f, 9000.f), R.FRandRange(-9000.f, 9000.f), R.FRandRange(-200.f, 3000.f)), R.FRandRange(10.f, 30.f), Rgb(0.8f, 0.7f, 1.f));
		}
		for (int32 i = 0; i < 10; ++i)
		{
			// distant islands drifting
			const float A = 2 * PI * i / 10;
			Isle(B, FVector(FMath::Cos(A) * 12000.f, FMath::Sin(A) * 12000.f, R.FRandRange(-200.f, 2500.f)), R.FRandRange(800.f, 1800.f), 3);
		}

		// ---- the start
		Isle(B, FVector(0, 0, 0), 700.f, 4);
		B.GM->Spawn<ALeoBuddy>(FVector(-200, -250, 120))->Init(ECreature::Leo, -170.f);
		B.GM->Spawn<ALeoBuddy>(FVector(200, -250, 60))->Init(ECreature::Fluffernutter, 170.f);
		B.Check(FVector(0, -300, 0), 90.f, TEXT("T_Crystal"));

		// ---- east: the sprites' island
		Isle(B, FVector(1100, 0, 0), 230.f);
		Isle(B, FVector(1700, 150, 80), 230.f);
		Isle(B, FVector(2300, -100, 40), 230.f);
		Isle(B, FVector(3500, 0, 100), 1100.f, 6);
		B.Npc(ECreature::Being, 0, FVector(2550, 250, 100), 180.f, TEXT("Being0"), true);
		for (int32 i = 0; i < 5; ++i)
		{
			const float A = 2 * PI * i / 5 + 0.3f;
			B.Item(EPickup::Sprite, FVector(3500 + FMath::Cos(A) * 550.f, FMath::Sin(A) * 550.f, 180));
		}
		B.Check(FVector(2700, -200, 100), 0.f, TEXT("T_Crystal"));
		B.NipLine(FVector(700, 0, 60), FVector(2300, -100, 110), 5);

		// ---- north: the crystal spire
		Isle(B, FVector(0, 1000, 0), 250.f);
		Isle(B, FVector(200, 1700, 50), 250.f);
		Isle(B, FVector(0, 2400, 100), 250.f);
		const FVector Spire(0, 3500, 0);
		Isle(B, Spire, 800.f, 3);
		B.Cyl(Spire, 160.f, 1600.f, 8, TEXT("T_Crystal"), W, true, 60.f);
		for (int32 i = 0; i < 7; ++i)
		{
			const float A = i * PI / 3.f + PI;
			const FVector P = Spire + FVector(FMath::Cos(A) * 400.f, FMath::Sin(A) * 400.f, 200.f * (i + 1) - 30.f);
			B.Cyl(P, 140.f, 30.f, 6, TEXT("T_Crystal"));
			B.Geo->Glow().Diamond(P - FVector(0, 0, 20), 30.f, Rgb(0.6f, 0.4f, 1.f), 0.6f);
			B.Nip(P + FVector(0, 0, 90));
		}
		B.Cyl(Spire + FVector(0, 0, 1600), 260.f, 30.f, 10, TEXT("T_Crystal"));
		B.Item(EPickup::Mote, Spire + FVector(0, 0, 1720));
		B.Mover(Spire + FVector(600, 300, 200), FVector(110, 110, 15), TEXT("T_Crystal"), W, EMove::Linear, FVector(0, 0, 800), 5.f, 0.f, true);
		B.Npc(ECreature::Being, 1, Spire + FVector(450, -450, 0), 200.f, TEXT("Being1"), true);
		B.Check(FVector(0, 2900, 0), 90.f, TEXT("T_Crystal"));

		// ---- west: beams of light up to the island in the sky
		Isle(B, FVector(-900, 300, 0), 230.f);
		Isle(B, FVector(-1500, 650, 0), 230.f);
		Isle(B, FVector(-2000, 800, 0), 550.f, 3);
		B.Haz(EHazard::Lift, FVector(-2000, 800, 0), FVector(0, 0, 1950), 1.f);
		Isle(B, FVector(-2000, 1450, 1800), 450.f, 4);
		B.Item(EPickup::Mote, FVector(-2000, 1550, 1900));
		B.Npc(ECreature::Being, 2, FVector(-2200, 450, 0), 60.f, TEXT("Being2"), true);
		B.Haz(EHazard::Lift, FVector(-2250, 1550, 1800), FVector(0, 0, 500), 1.f);  // a little one for the view
		B.NipLine(FVector(-2000, 800, 300), FVector(-2000, 800, 1500), 5);

		// ---- south: across a gravity well to the temple in the crystal cavern
		Isle(B, FVector(0, -1000, 0), 260.f);
		Isle(B, FVector(-200, -1700, 50), 260.f);
		B.Haz(EHazard::Gravity, FVector(-100, -2350, 150), FVector(550, 0, 0), 1.f);
		const FVector Temple(0, -3300, 0);
		Isle(B, Temple, 1000.f);
		for (int32 i = 0; i < 12; ++i)
		{
			const float A = 2 * PI * i / 12;
			if (FMath::Sin(A) > 0.6f)
			{
				continue;  // leave the way in open
			}
			const FVector P = Temple + FVector(FMath::Cos(A), FMath::Sin(A), 0) * 900.f;
			B.Cyl(P, 110.f, 700.f + 300.f * (i % 3), 6, TEXT("T_Crystal"), W, true, 0.f);
		}
		B.Slab(Temple.X, Temple.Y - 300, 450, 350, 0, 60, TEXT("T_Marble"));
		B.Slab(Temple.X, Temple.Y - 630, 450, 20, 60, 600, TEXT("T_Marble"));
		B.Slab(Temple.X - 430, Temple.Y - 330, 20, 330, 60, 600, TEXT("T_Marble"));
		B.Slab(Temple.X + 430, Temple.Y - 330, 20, 330, 60, 600, TEXT("T_Marble"));
		B.Slab(Temple.X, Temple.Y - 330, 470, 360, 600, 650, TEXT("T_Crystal"));
		B.Goal(TEXT("Temple"), Temple + FVector(0, -300, 60), 200.f, false);
		ANpc* Ent = B.Npc(ECreature::Being, 3, Temple + FVector(0, -520, 60), 90.f, TEXT("Entity"), false);
		Ent->GetRig()->SetScale(1.8f);
		B.Npc(ECreature::Being, 3, Temple + FVector(350, 400, 0), 200.f, TEXT("Being3"), true);
		B.Lamp(Temple + FVector(0, -330, 400), Rgb(1.f, 0.85f, 0.55f), 1000.f, 24.f);
		B.Check(FVector(-200, -1700, 50), -90.f, TEXT("T_Crystal"));
		return {FVector(0, -150, 60), 90.f, EMusic::Dimension};
	}
}

Levels::FSetup Levels::Build(int32 Chapter, ASparkyGameMode* GM, ALevelGeo* Geo)
{
	FB B{GM, Geo};
	FSetup S;
	switch (Chapter)
	{
	case 2: S = Chapter2(B); break;
	case 3: S = Chapter3(B); break;
	case 4: S = Chapter4(B); break;
	case 5: S = Chapter5(B); break;
	case 6: S = Chapter6(B); break;
	case 7: S = Chapter7(B); break;
	case 8: S = Chapter8(B); break;
	case 9: S = Chapter9(B); break;
	default: S = Chapter1(B); break;
	}
	if (Chapter == 4)
	{
		// the module can't be fixed until the four conduits are
		for (TActorIterator<ARepairPoint> It(GM->GetWorld()); It; ++It)
		{
			It->SetEnabled(It->GetId() != TEXT("Module"));
		}
	}
	return S;
}
