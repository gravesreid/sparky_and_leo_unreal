#include "SparkyHUD.h"
#include "Enemy.h"
#include "PixelFont.h"
#include "Props.h"
#include "SparkyCharacter.h"
#include "SparkyGameMode.h"
#include "Story.h"

#include "Engine/Canvas.h"

namespace
{
	const FLinearColor White(1.f, 1.f, 1.f);
	const FLinearColor Gold(1.f, 0.82f, 0.3f);
	const FLinearColor Grey(0.65f, 0.62f, 0.7f);
	const FLinearColor Red(0.95f, 0.2f, 0.15f);
	const FLinearColor Green(0.45f, 1.f, 0.45f);
	const FLinearColor Cyan(0.35f, 0.9f, 1.f);
	const FLinearColor Pink(1.f, 0.45f, 0.65f);
	const FLinearColor Cranberry(0.85f, 0.2f, 0.4f);
	const FLinearColor Shadow(0.f, 0.f, 0.f, 0.85f);

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}

	const TCHAR* WhoName(EWho W)
	{
		switch (W)
		{
		case EWho::Sparky: return TEXT("SPARKY");
		case EWho::Leo: return TEXT("LEO");
		case EWho::Elder: return TEXT("HAT ELDER");
		case EWho::Local: return TEXT("LOCAL");
		case EWho::Captain: return TEXT("CAPT. CROTCH SNIFFER");
		case EWho::Meatball: return TEXT("MEATBALL");
		case EWho::Crowd: return TEXT("ANNOUNCER");
		case EWho::Fluff: return TEXT("FLUFFERNUTTER");
		case EWho::Chief: return TEXT("VILLAGER");
		case EWho::Scientist: return TEXT("LOST SCIENTIST");
		case EWho::Princess: return TEXT("WARRIOR PRINCESS");
		case EWho::Guardian: return TEXT("THE GUARDIAN");
		case EWho::Entity: return TEXT("ANCIENT ENTITY");
		case EWho::Echo: return TEXT("AN ECHO");
		case EWho::Being: return TEXT("LUMINOUS BEING");
		default: return TEXT("");
		}
	}

	FLinearColor WhoColor(EWho W)
	{
		switch (W)
		{
		case EWho::Sparky: return Cyan;
		case EWho::Leo: return Gold;
		case EWho::Elder: return Pink;
		case EWho::Local: return Pink;
		case EWho::Captain: return Red;
		case EWho::Meatball: return FLinearColor(0.9f, 0.6f, 0.4f);
		case EWho::Crowd: return Green;
		case EWho::Fluff: return FLinearColor(1.f, 0.75f, 0.45f);
		case EWho::Chief: return FLinearColor(0.5f, 0.95f, 0.45f);
		case EWho::Scientist:
		case EWho::Echo: return FLinearColor(0.6f, 1.f, 1.f);
		case EWho::Princess: return FLinearColor(1.f, 0.6f, 0.9f);
		case EWho::Guardian: return Gold;
		case EWho::Entity:
		case EWho::Being: return FLinearColor(0.9f, 0.85f, 1.f);
		default: return Grey;
		}
	}
}

// ---------------------------------------------------------------- primitives

void ASparkyHUD::Rect(float X, float Y, float W, float H, const FLinearColor& C)
{
	const float X0 = FMath::RoundToFloat(OX + X * U), Y0 = FMath::RoundToFloat(OY + Y * U);
	const float X1 = FMath::RoundToFloat(OX + (X + W) * U), Y1 = FMath::RoundToFloat(OY + (Y + H) * U);
	if (X1 > X0 && Y1 > Y0)
	{
		DrawRect(C, X0, Y0, X1 - X0, Y1 - Y0);
	}
}

float ASparkyHUD::TextWidth(const FString& S, float Scale) const
{
	return (S.Len() * PixelFont::Advance - 1) * Scale;
}

float ASparkyHUD::Text(const FString& InS, float X, float Y, const FLinearColor& C, float Scale, int32 Align, bool bShadow)
{
	const FString S = InS.ToUpper();
	const float W = TextWidth(S, Scale);
	if (Align == 1)
	{
		X -= W * 0.5f;
	}
	else if (Align == 2)
	{
		X -= W;
	}
	for (int32 Pass = bShadow ? 0 : 1; Pass < 2; ++Pass)
	{
		const float Off = Pass == 0 ? Scale : 0.f;
		const FLinearColor Col = Pass == 0 ? WithAlpha(Shadow, Shadow.A * C.A) : C;
		float CX = X;
		for (TCHAR Ch : S)
		{
			if (const PixelFont::FGlyph* G = PixelFont::Find(Ch))
			{
				for (int32 R = 0; R < 7; ++R)
				{
					const char* Row = G->Rows[R];
					for (int32 C0 = 0; C0 < 5;)
					{
						if (Row[C0] != '#')
						{
							++C0;
							continue;
						}
						int32 C1 = C0;
						while (C1 < 5 && Row[C1] == '#')
						{
							++C1;
						}
						Rect(CX + C0 * Scale + Off, Y + R * Scale + Off, (C1 - C0) * Scale, Scale, Col);
						C0 = C1;
					}
				}
			}
			CX += PixelFont::Advance * Scale;
		}
	}
	return W;
}

void ASparkyHUD::Sprite(const TArray<const char*>& Rows, const TMap<char, FLinearColor>& Palette, float X, float Y, float Px)
{
	for (int32 R = 0; R < Rows.Num(); ++R)
	{
		const char* Row = Rows[R];
		for (int32 Col = 0; Row[Col]; ++Col)
		{
			if (const FLinearColor* C = Palette.Find(Row[Col]))
			{
				Rect(X + Col * Px, Y + R * Px, Px, Px, *C);
			}
		}
	}
}

void ASparkyHUD::Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& C, int32 Segments)
{
	Rect(X - 1, Y - 1, W + 2, H + 2, FLinearColor(0, 0, 0, 0.8f));
	Rect(X, Y, W, H, WithAlpha(C * 0.25f, 0.9f));
	Frac = FMath::Clamp(Frac, 0.f, 1.f);
	if (Segments > 0)
	{
		const float SW = W / Segments;
		const int32 Lit = FMath::CeilToInt(Frac * Segments - 0.001f);
		for (int32 i = 0; i < Lit; ++i)
		{
			Rect(X + i * SW, Y, SW - 1, H, C);
		}
	}
	else
	{
		Rect(X, Y, W * Frac, H, C);
	}
}

void ASparkyHUD::Panel(float X, float Y, float W, float H, float Alpha)
{
	Rect(X, Y, W, H, FLinearColor(0.05f, 0.02f, 0.08f, Alpha));
	const FLinearColor Edge = Cranberry;
	Rect(X, Y, W, 1, Edge);
	Rect(X, Y + H - 1, W, 1, Edge);
	Rect(X, Y, 1, H, Edge);
	Rect(X + W - 1, Y, 1, H, Edge);
	Rect(X + 1, Y + 1, W - 2, 1, WithAlpha(Gold, 0.35f));
}

void ASparkyHUD::Dim(float Alpha)
{
	DrawRect(FLinearColor(0.01f, 0.f, 0.03f, Alpha), 0, 0, Canvas->ClipX, Canvas->ClipY);
}

TArray<FString> ASparkyHUD::Wrap(const FString& S, int32 MaxChars)
{
	TArray<FString> Out;
	TArray<FString> Paras;
	S.ParseIntoArray(Paras, TEXT("\n"), false);
	for (const FString& Pa : Paras)
	{
		TArray<FString> Words;
		Pa.ParseIntoArrayWS(Words);
		FString Line;
		for (const FString& W : Words)
		{
			if (!Line.IsEmpty() && Line.Len() + 1 + W.Len() > MaxChars)
			{
				Out.Add(Line);
				Line.Reset();
			}
			Line += Line.IsEmpty() ? W : TEXT(" ") + W;
		}
		Out.Add(Line);
	}
	return Out;
}

// ---------------------------------------------------------------- art

void ASparkyHUD::Portrait(EWho Who, float X, float Y, float Px)
{
	static const TArray<const char*> SparkyRows = {
		".G..........G.",
		"GPG........GPG",
		"GPGG......GGPG",
		"GGGGGGGGGGGGGG",
		"BBCCBBBBBBCCBB",
		"GBCCBGgGGBCCBG",
		"GGGGGGgGGGGGGG",
		"GWEKWGGGGWEKWG",
		"GWEKWGGGGWEKWG",
		"GGGGGWWWWGGGGG",
		"GGGGWWPPWWGGGG",
		".GGGWKWWKWGGG.",
		"..GGGWWWWGGG..",
		"...YYYCYYYY...",
	};
	static const TArray<const char*> LeoRows = {
		"....HHHHHH....",
		"...HHHHHHHH...",
		"..HHHHHHHHHH..",
		"..HSSSSSSSSH..",
		"..HSHHSSHHSH..",
		"..SSKSSSSKSS..",
		"..SSSSSSSSSS..",
		"..SSSSSNSSSS..",
		"..SSSSSSSSSS..",
		"...SSMMMMSS...",
		"....SSSSSS....",
		".....SSSS.....",
		"..TTTTTTTTTT..",
		".TTTTTTTTTTTT.",
	};
	static const TArray<const char*> ElderRows = {
		"..W..WWWW..W..",
		"..W.WWWWWW.W..",
		"..WWWWWWWWWW..",
		"..OOOOOOOOOO..",
		"...RRRRRRRR...",
		"..RRRRRRRRRR..",
		"..RKKKRRKKKR..",
		"..RKKKRRKKKR..",
		"..RRRRRRRRRR..",
		"..RRRRRRRRRR..",
		"...RRDDDDRR...",
		"....RRRRRR....",
		"..OOOOOOOOOO..",
		".OOOOOOOOOOOO.",
	};
	static const TArray<const char*> LocalRows = {
		"..............",
		"......WW......",
		"....WWWWWW....",
		"...WWWWWWWW...",
		"...RRRRRRRR...",
		"..RRRRRRRRRR..",
		"..RKKKRRKKKR..",
		"..RKKKRRKKKR..",
		"..RRRRRRRRRR..",
		"..RRRRRRRRRR..",
		"...RRDDDDRR...",
		"....RRRRRR....",
		"..VVVVVVVVVV..",
		".VVVVVVVVVVVV.",
	};
	static const TArray<const char*> CaptainRows = {
		"..DDDDDDDDDD..",
		".DDDDDWWDDDDD.",
		"DDDDDDDDDDDDDD",
		"..TTTTTTTTTT..",
		".LTTTTTTTTTTL.",
		".LTKKKTTWWTTL.",
		".LTKKKTTWKTTL.",
		"..TTTTTTTTTT..",
		"..TTTNNNNTTT..",
		".TTTTTNNTTTTT.",
		".TTWTWTTWTWTT.",
		".TTTTTTTTTTTT.",
		"..RRRRRRRRRR..",
		".RRRRRYRRRRRR.",
	};
	static const TArray<const char*> MeatballRows = {
		"GG..........G.",
		"GPG........GPG",
		"..GG......GGPG",
		"GGGGGGGGGGGGGG",
		"GGgGGGgGGgGGGG",
		"GGPGGGgGGGGGGG",
		"GGPGGGGGGGGGGG",
		"GWYKWGGGGWYKWG",
		"GWYKWGGGGWYKWG",
		"GGGGGWWWWGGGGG",
		"GGGGWWPPWWGGGG",
		"GGGGWKKKKWGGGG",
		".GGGGWWWWGGGG.",
		"..GGGGGGGGGG..",
	};
	static const TArray<const char*> FluffRows = {
		"..............",
		".FF........FF.",
		"FPPF......FPPF",
		"FPPFFFFFFFFPPF",
		".FFFFFFFFFFFF.",
		"FFKKKFFFFKKKFF",
		"FKWKKFFFFKWKKF",
		"FKKKKFFFFKKKKF",
		"FFKKFFFFFFKKFF",
		"FFFFFCCCCFFFFF",
		"FFFFCCPPCCFFFF",
		".FFFCCCCCCFFF.",
		"..FFFCCCCFFF..",
		"....FFFFFF....",
	};
	static const TArray<const char*> GuardianRows = {
		"....gGGGGg....",
		"...GGGGGGGG...",
		"..GGgGGGGgGG..",
		".GGGGGGGGGGGG.",
		".GKKKGGGGKKKG.",
		".GKEEKGGKEEKG.",
		".GKKKGGGGKKKG.",
		".GGGGGggGGGGG.",
		"..GGGGggGGGG..",
		"..GgGGGGGGgG..",
		"...GKKKKKKG...",
		"...GGGGGGGG...",
		"....GgGGgG....",
		".....GGGG.....",
	};
	static const TArray<const char*> OrbRows = {
		".....oooo.....",
		"...oo....oo...",
		"..o..OOOO..o..",
		".o..OOOOOO..o.",
		".o.OOWWWWOO.o.",
		"o.OOWWWWWWOO.o",
		"o.OOWWKKWWOO.o",
		"o.OOWWKKWWOO.o",
		"o.OOWWWWWWOO.o",
		".o.OOWWWWOO.o.",
		".o..OOOOOO..o.",
		"..o..OOOO..o..",
		"...oo....oo...",
		".....oooo.....",
	};
	static const TArray<const char*> CrowdRows = {
		"..............",
		"........KK....",
		".......KSSK...",
		"......KSSSSK..",
		".....KSSSSSK..",
		"....KSSSSSK...",
		"...KKSSSSK....",
		"..KYYKKKK.....",
		".KYYYK........",
		".KYYK.........",
		"..KK..........",
		"..............",
		"..............",
		"..............",
	};
	const TArray<const char*>* Rows = nullptr;
	TMap<char, FLinearColor> Pal;
	switch (Who)
	{
	case EWho::Sparky:
		Rows = &SparkyRows;
		Pal = {{'G', FLinearColor(0.55f, 0.6f, 0.7f)}, {'g', FLinearColor(0.32f, 0.36f, 0.45f)}, {'P', FLinearColor(0.95f, 0.6f, 0.65f)},
		       {'W', FLinearColor(0.95f, 0.95f, 0.92f)}, {'E', FLinearColor(0.25f, 0.8f, 0.35f)}, {'K', FLinearColor(0.05f, 0.05f, 0.08f)},
		       {'B', FLinearColor(0.85f, 0.62f, 0.2f)}, {'C', Cyan}, {'Y', FLinearColor(0.95f, 0.8f, 0.15f)}};
		break;
	case EWho::Leo:
		Rows = &LeoRows;
		Pal = {{'H', FLinearColor(0.42f, 0.27f, 0.15f)}, {'S', FLinearColor(0.95f, 0.76f, 0.62f)}, {'K', FLinearColor(0.1f, 0.08f, 0.08f)},
		       {'N', FLinearColor(0.85f, 0.65f, 0.52f)}, {'M', FLinearColor(0.7f, 0.35f, 0.35f)}, {'T', FLinearColor(0.5f, 0.56f, 0.62f)}};
		break;
	case EWho::Elder:
	case EWho::Local:
		Rows = Who == EWho::Elder ? &ElderRows : &LocalRows;
		Pal = {{'W', FLinearColor(0.97f, 0.97f, 0.93f)}, {'O', FLinearColor(0.9f, 0.7f, 0.2f)}, {'R', FLinearColor(0.8f, 0.28f, 0.4f)},
		       {'K', FLinearColor(0.05f, 0.03f, 0.06f)}, {'D', FLinearColor(0.5f, 0.12f, 0.2f)}, {'V', FLinearColor(0.45f, 0.25f, 0.6f)}};
		break;
	case EWho::Captain:
		Rows = &CaptainRows;
		Pal = {{'D', FLinearColor(0.08f, 0.07f, 0.08f)}, {'W', FLinearColor(0.95f, 0.95f, 0.9f)}, {'T', FLinearColor(0.75f, 0.58f, 0.38f)},
		       {'L', FLinearColor(0.5f, 0.38f, 0.25f)}, {'K', FLinearColor(0.05f, 0.05f, 0.05f)}, {'N', FLinearColor(0.12f, 0.1f, 0.1f)},
		       {'R', FLinearColor(0.6f, 0.08f, 0.1f)}, {'Y', FLinearColor(0.9f, 0.75f, 0.2f)}};
		break;
	case EWho::Meatball:
		Rows = &MeatballRows;
		Pal = {{'G', FLinearColor(0.5f, 0.42f, 0.36f)}, {'g', FLinearColor(0.3f, 0.24f, 0.2f)}, {'P', FLinearColor(0.95f, 0.55f, 0.6f)},
		       {'W', FLinearColor(0.8f, 0.72f, 0.65f)}, {'Y', FLinearColor(0.9f, 0.8f, 0.1f)}, {'K', FLinearColor(0.05f, 0.05f, 0.05f)}};
		break;
	case EWho::Crowd:
		Rows = &CrowdRows;
		Pal = {{'K', FLinearColor(0.05f, 0.05f, 0.05f)}, {'S', FLinearColor(0.75f, 0.75f, 0.8f)}, {'Y', FLinearColor(0.9f, 0.2f, 0.2f)}};
		break;
	case EWho::Fluff:
		Rows = &FluffRows;
		Pal = {{'F', FLinearColor(0.9f, 0.68f, 0.42f)}, {'C', FLinearColor(1.f, 0.92f, 0.78f)}, {'K', FLinearColor(0.06f, 0.04f, 0.04f)},
		       {'W', FLinearColor(1.f, 1.f, 1.f)}, {'P', FLinearColor(0.95f, 0.6f, 0.62f)}};
		break;
	case EWho::Chief:
		Rows = &LocalRows;
		Pal = {{'W', FLinearColor(0.2f, 0.6f, 0.9f)}, {'O', FLinearColor(0.75f, 0.28f, 0.15f)}, {'R', FLinearColor(0.48f, 0.72f, 0.42f)},
		       {'K', FLinearColor(0.05f, 0.03f, 0.06f)}, {'D', FLinearColor(0.25f, 0.45f, 0.2f)}, {'V', FLinearColor(0.35f, 0.55f, 0.2f)}};
		break;
	case EWho::Scientist:
	case EWho::Princess:
		Rows = Who == EWho::Scientist ? &ElderRows : &LocalRows;
		Pal = {{'W', FLinearColor(0.75f, 1.f, 1.f)}, {'O', FLinearColor(0.5f, 0.85f, 0.95f)}, {'R', Who == EWho::Princess ? FLinearColor(0.8f, 0.6f, 0.9f) : FLinearColor(0.55f, 0.85f, 0.9f)},
		       {'K', FLinearColor(0.1f, 0.15f, 0.3f)}, {'D', FLinearColor(0.3f, 0.5f, 0.6f)}, {'V', FLinearColor(0.6f, 0.45f, 0.85f)}};
		break;
	case EWho::Guardian:
		Rows = &GuardianRows;
		Pal = {{'G', FLinearColor(0.95f, 0.78f, 0.3f)}, {'g', FLinearColor(0.6f, 0.45f, 0.15f)}, {'E', FLinearColor(0.6f, 1.f, 1.f)}, {'K', FLinearColor(0.08f, 0.05f, 0.1f)}};
		break;
	case EWho::Echo:
		Rows = &SparkyRows;
		Pal = {{'G', FLinearColor(0.45f, 0.75f, 0.85f)}, {'g', FLinearColor(0.3f, 0.5f, 0.65f)}, {'P', FLinearColor(0.6f, 0.85f, 0.95f)},
		       {'W', FLinearColor(0.8f, 1.f, 1.f)}, {'E', FLinearColor(1.f, 1.f, 1.f)}, {'K', FLinearColor(0.1f, 0.2f, 0.3f)},
		       {'B', FLinearColor(0.5f, 0.8f, 0.9f)}, {'C', FLinearColor(0.9f, 1.f, 1.f)}, {'Y', FLinearColor(0.6f, 0.9f, 1.f)}};
		break;
	case EWho::Entity:
	case EWho::Being:
		Rows = &OrbRows;
		Pal = {{'O', Who == EWho::Entity ? FLinearColor(1.f, 0.85f, 0.55f) : FLinearColor(0.65f, 0.9f, 1.f)}, {'o', FLinearColor(0.35f, 0.45f, 0.8f)},
		       {'W', FLinearColor(1.f, 1.f, 1.f)}, {'K', FLinearColor(0.15f, 0.1f, 0.3f)}};
		break;
	default:
		return;
	}
	Rect(X - 1, Y - 1, 14 * Px + 2, 14 * Px + 2, WhoColor(Who));
	Rect(X, Y, 14 * Px, 14 * Px, FLinearColor(0.08f, 0.05f, 0.12f, 1.f));
	Sprite(*Rows, Pal, X, Y, Px);
}

void ASparkyHUD::Paw(float X, float Y, const FLinearColor& C)
{
	static const TArray<const char*> Rows = {
		".XX..XX.",
		".XX..XX.",
		"X......X",
		"XX.XX.XX",
		"..XXXX..",
		".XXXXXX.",
		".XXXXXX.",
		"..XXXX..",
	};
	Sprite(Rows, {{'X', FLinearColor(0, 0, 0, 0.8f)}}, X + 1, Y + 1, 1.f);
	Sprite(Rows, {{'X', C}}, X, Y, 1.f);
}

void ASparkyHUD::DrawLogo(float Y)
{
	const float T = GetWorld()->GetRealTimeSeconds();
	Text(TEXT("THE ADVENTURES OF"), 160, Y, Gold, 1.f, 1);
	// SPARKY & LEO, each letter bobbing
	const FString S = TEXT("SPARKY & LEO");
	const float Scale = 3.f;
	float X = 160.f - TextWidth(S, Scale) * 0.5f;
	for (int32 i = 0; i < S.Len(); ++i)
	{
		const float Bob = FMath::Sin(T * 3.f + i * 0.6f) * 2.f;
		const FLinearColor C = i < 6 ? Cyan : i > 7 ? Gold : Pink;
		Text(FString::Chr(S[i]), X, Y + 12.f + Bob, C, Scale);
		X += PixelFont::Advance * Scale;
	}
}

// ---------------------------------------------------------------- top level

void ASparkyHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	U = FMath::Max(1.f, FMath::FloorToFloat(FMath::Min(Canvas->ClipX / 320.f, Canvas->ClipY / 240.f)));
	OX = FMath::RoundToFloat((Canvas->ClipX - 320.f * U) * 0.5f);
	OY = FMath::RoundToFloat((Canvas->ClipY - 240.f * U) * 0.5f);
	ASparkyGameMode* M = ASparkyGameMode::Get(this);
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	if (!M || !P)
	{
		return;
	}
	switch (M->State)
	{
	case EGameState::Title:
		DrawTitle(M);
		break;
	case EGameState::Story:
		DrawStory(M);
		break;
	case EGameState::Playing:
		DrawPlaying(P, M);
		break;
	case EGameState::Cutscene:
		if (M->HyperTime >= 0.f)
		{
			DrawHyperspace(M->HyperTime);
		}
		if (M->Convo.IsValidIndex(M->ConvoIndex))
		{
			DrawDialogBox(M->Convo[M->ConvoIndex].Who, M->Convo[M->ConvoIndex].Text, true, M->ConvoTime);
		}
		break;
	case EGameState::Question:
		DrawQuestion(M);
		break;
	case EGameState::Paused:
		DrawPlaying(P, M);
		Dim(0.5f);
		Panel(90, 60, 140, 120);
		Text(TEXT("PAUSED"), 160, 70, Gold, 2.f, 1);
		Text(FString::Printf(TEXT("CHAPTER %d"), M->Chapter), 160, 88, Grey, 1.f, 1);
		DrawMenu(M, 104);
		break;
	case EGameState::Calibrate:
		DrawCalibration(M, P);
		break;
	case EGameState::ChapterClear:
		DrawClear(M);
		break;
	case EGameState::GameOver:
		DrawGameOver(M);
		break;
	case EGameState::End:
		DrawEnd(M);
		break;
	}
}

void ASparkyHUD::DrawMenu(ASparkyGameMode* M, float Y)
{
	const TArray<FString> Items = M->MenuItems();
	const float T = GetWorld()->GetRealTimeSeconds();
	for (int32 i = 0; i < Items.Num(); ++i)
	{
		const bool bSel = i == M->MenuIndex;
		const FLinearColor C = bSel ? WithAlpha(Gold * (0.8f + 0.2f * FMath::Sin(T * 8.f)), 1.f) : White;
		Text(Items[i], 160, Y + i * 13.f, C, 1.f, 1);
		if (bSel)
		{
			Paw(160 - TextWidth(Items[i]) * 0.5f - 14, Y + i * 13.f - 1, Cyan);
		}
	}
}

// ---------------------------------------------------------------- playing

void ASparkyHUD::DrawPlaying(ASparkyCharacter* P, ASparkyGameMode* M)
{
	const float T = GetWorld()->GetRealTimeSeconds();
	// paws (health)
	for (int32 i = 0; i < ASparkyCharacter::MaxHealth; ++i)
	{
		const bool bOn = i < P->GetHealth();
		const bool bLow = P->GetHealth() <= 1 && bOn && FMath::Fmod(T, 0.6f) < 0.3f;
		Paw(8 + i * 11.f, 8, bOn ? (bLow ? Red : Pink) : FLinearColor(0.2f, 0.15f, 0.25f, 0.8f));
	}
	// lives: a tiny Sparky and the count
	Portrait(P->IsFluffernutter() ? EWho::Fluff : EWho::Sparky, 8, 20, 1.f);
	Text(FString::Printf(TEXT("X%d"), M->Lives), 25, 24, White);
	// catnip
	Text(FString::Printf(TEXT("%d/%d"), M->Catnip, M->CatnipTotal), 312, 9, Green, 1.f, 2);
	{
		FLinearColor Leaf(0.35f, 0.9f, 0.3f);
		const float X = 312 - TextWidth(FString::Printf(TEXT("%d/%d"), M->Catnip, M->CatnipTotal)) - 11;
		Rect(X + 2, 8, 3, 3, Leaf);
		Rect(X, 11, 3, 3, Leaf);
		Rect(X + 4, 11, 3, 3, Leaf);
		Rect(X + 3, 14, 1, 3, Leaf * 0.6f);
	}
	// objective
	const FString Obj = M->Objective();
	if (!Obj.IsEmpty())
	{
		Text(Obj, 160, 9, WithAlpha(Gold, 0.9f), 1.f, 1);
	}

	// chapter-specific gauges
	if (M->Boss && !M->Boss->IsDead())
	{
		Text(M->BossName, 160, 26, FLinearColor(0.95f, 0.65f, 0.45f), 1.f, 1);
		Bar(90, 35, 140, 5, float(M->Boss->GetHP()) / M->Boss->GetMaxHP(), Red, M->Boss->GetMaxHP());
	}
	if (M->Chapter == 4 && (M->bCrisisOn || M->Repairs > 0))
	{
		const float O2 = M->Oxygen;
		const bool bLow = O2 < 0.25f && FMath::Fmod(T, 0.5f) < 0.25f;
		Text(TEXT("O2"), 8, 40, bLow ? Red : White);
		Bar(24, 41, 70, 5, O2, O2 > 0.5f ? Green : O2 > 0.25f ? Gold : Red, 20);
		Text(TEXT("GRAV"), 8, 50, Grey);
		Bar(36, 51, 58, 3, FMath::Clamp(1.f - (1.f - O2) * 1.3f, 0.f, 1.f), FLinearColor(0.7f, 0.5f, 1.f));
		Text(TEXT("TEMP"), 8, 58, Grey);
		Bar(36, 59, 58, 3, O2, FLinearColor(1.f, 0.5f, 0.2f) * O2 + Cyan * (1.f - O2));
		if (O2 < 0.5f)
		{
			// frost creeping in from the edges
			const float F = (0.5f - O2) * 2.f;
			const FLinearColor Ice(0.75f, 0.9f, 1.f, 0.25f * F);
			const float SW = Canvas->ClipX / U, SH = Canvas->ClipY / U;
			Rect(-OX / U, -OY / U, SW, 14 * F, Ice);
			Rect(-OX / U, -OY / U + SH - 14 * F, SW, 14 * F, Ice);
		}
	}
	if (ARepairPoint* R = P->HasWelder() ? ARepairPoint::FindNear(P) : nullptr)
	{
		if (R->IsEnabled())
		{
			Text(P->IsWelding() ? TEXT("WELDING...") : (P->HasN64Pad() ? TEXT("HOLD B TO WELD") : M->IsKeyboardOnly() ? TEXT("HOLD J TO WELD") : TEXT("HOLD J / CLICK TO WELD")), 160, 150, Cyan, 1.f, 1);
			Bar(110, 160, 100, 4, R->GetProgress(), Cyan);
		}
		else
		{
			Text(TEXT("SEAL THE FOUR CONDUITS FIRST"), 160, 150, Grey, 1.f, 1);
		}
	}
	if (P->GetInteractable() && M->State == EGameState::Playing)
	{
		const float Bob = FMath::Sin(T * 6.f);
		Text(P->HasN64Pad() ? TEXT("B: TALK") : M->IsKeyboardOnly() ? TEXT("J: TALK") : TEXT("J / CLICK: TALK"), 160, 150 + Bob, White, 1.f, 1);
	}
	if (P->IsClinging())
	{
		Text(P->HasN64Pad() ? TEXT("A: JUMP OFF   Z: LET GO") : TEXT("SPACE: JUMP OFF   SHIFT: LET GO"), 160, 150, Pink, 1.f, 1);
	}

	// controls reminder at the start of chapter 1
	if (M->Chapter == 1 && M->ChapterTime < 14.f && M->State == EGameState::Playing)
	{
		const float A = FMath::Clamp(14.f - M->ChapterTime, 0.f, 1.f);
		const float Y0 = 36.f;
		Panel(34, Y0, 252, 34, 0.6f * A);
		if (P->HasN64Pad())
		{
			Text(TEXT("A JUMP (AGAIN IN AIR: SOMERSAULT)  B SWIPE"), 160, Y0 + 5, WithAlpha(White, A), 1.f, 1);
			Text(TEXT("Z IN AIR: GROUND POUND  Z+A: BACKFLIP"), 160, Y0 + 15, WithAlpha(White, A), 1.f, 1);
			Text(TEXT("C BUTTONS: CAMERA   R: RECENTRE"), 160, Y0 + 25, WithAlpha(Grey, A), 1.f, 1);
		}
		else
		{
			Text(TEXT("SPACE JUMP (AGAIN: SOMERSAULT)  J SWIPE"), 160, Y0 + 5, WithAlpha(White, A), 1.f, 1);
			Text(TEXT("SHIFT IN AIR: GROUND POUND  SHIFT+SPACE: FLIP"), 160, Y0 + 15, WithAlpha(White, A), 1.f, 1);
			Text(M->IsKeyboardOnly() ? TEXT("ARROWS: CAMERA  I O: ZOOM  L: RECENTRE") : TEXT("MOUSE / Q E: CAMERA   TAB: RECENTRE"), 160, Y0 + 25, WithAlpha(Grey, A), 1.f, 1);
		}
	}

	if (M->Chapter == 5 && M->Ch5Phase == 2 && M->Trust > 0.f)
	{
		Text(TEXT("TRUST"), 160, 140, FLinearColor(1.f, 0.75f, 0.45f), 1.f, 1);
		Bar(110, 150, 100, 5, M->Trust, FLinearColor(1.f, 0.75f, 0.45f), 10);
	}
	if (M->MessageTime > 0.f)
	{
		const float A = FMath::Clamp(M->MessageTime * 2.f, 0.f, 1.f);
		const float Scale = TextWidth(M->MessageText, 2.f) < 300.f ? 2.f : 1.f;
		Text(M->MessageText, 160, M->Chapter == 1 && M->ChapterTime < 14.f ? 80.f : 64.f, WithAlpha(M->MessageColor, A), Scale, 1);
	}
	if (M->RadioTime > 0.f && M->State == EGameState::Playing)
	{
		DrawDialogBox(M->RadioLine.Who, M->RadioLine.Text, false, 10.f);
	}
	if (P->IsDown())
	{
		Dim(FMath::Clamp((2.2f - M->DownTimer) * 0.4f, 0.f, 0.6f));
	}
}

void ASparkyHUD::DrawDialogBox(EWho Who, const FString& Str, bool bBlocking, float Time)
{
	const float Y = bBlocking ? 168.f : 186.f;
	const float H = bBlocking ? 64.f : 46.f;
	const float Px = bBlocking ? 3.f : 2.f;
	Panel(6, Y, 308, H, bBlocking ? 0.88f : 0.7f);
	float TX = 14.f;
	if (Who != EWho::None)
	{
		Portrait(Who, 12, Y + 8, Px);
		TX = 12 + 14 * Px + 8;
		Text(WhoName(Who), TX, Y + 6, WhoColor(Who));
	}
	// typewriter reveal
	const int32 Shown = bBlocking ? FMath::Min(Str.Len(), int32(Time * 60.f)) : Str.Len();
	const int32 MaxChars = int32((306.f - TX) / PixelFont::Advance);
	const TArray<FString> Lines = Wrap(Str.Left(Shown), MaxChars);
	const float LY = Y + (Who != EWho::None ? 17.f : 8.f);
	const int32 MaxLines = bBlocking ? 5 : 3;
	for (int32 i = 0; i < Lines.Num() && i < MaxLines; ++i)
	{
		Text(Lines[i], TX, LY + i * 9.f, Who == EWho::None ? FLinearColor(0.95f, 0.9f, 0.8f) : White);
	}
	if (bBlocking && Shown >= Str.Len() && FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 0.8f) < 0.5f)
	{
		Text(TEXT(">"), 304, Y + H - 11, Gold);
	}
}

void ASparkyHUD::DrawQuestion(ASparkyGameMode* M)
{
	// the question in a dialog box, the answers in a list above it
	DrawDialogBox(M->QuestionWho, M->QuestionText, true, 1000.f);
	const int32 N = M->QuestionOptions.Num();
	const float H = 12.f * N + 10.f;
	const float Y0 = 160.f - H;
	Panel(40, Y0, 240, H, 0.9f);
	const float T = GetWorld()->GetRealTimeSeconds();
	for (int32 i = 0; i < N; ++i)
	{
		const bool bSel = i == M->QuestionChoice;
		const FLinearColor C = bSel ? WithAlpha(Gold * (0.8f + 0.2f * FMath::Sin(T * 8.f)), 1.f) : White;
		const float Y = Y0 + 6.f + i * 12.f;
		if (bSel)
		{
			Paw(48, Y - 1, Cyan);
		}
		Text(M->QuestionOptions[i], 62, Y, C);
	}
}

void ASparkyHUD::DrawHyperspace(float T)
{
	// stars stretched into streaks of light
	FRandomStream R(77);
	const float CX = Canvas->ClipX * 0.5f, CY = Canvas->ClipY * 0.5f;
	const float K = FMath::Clamp(T / 2.f, 0.f, 1.f);
	Dim(0.2f + 0.5f * K);
	for (int32 i = 0; i < 90; ++i)
	{
		const float A = R.FRandRange(0.f, 2 * PI);
		const float D0 = R.FRandRange(0.05f, 1.f) * Canvas->ClipX * 0.6f;
		const float Len = (40.f + R.FRandRange(0.f, 300.f)) * K * K * U;
		const float Speed = FMath::Fmod(D0 + T * 900.f * U * (0.5f + K), Canvas->ClipX * 0.7f);
		const FVector2D Dir(FMath::Cos(A), FMath::Sin(A));
		const FVector2D S = FVector2D(CX, CY) + Dir * Speed;
		const FVector2D E = S + Dir * Len;
		DrawLine(S.X, S.Y, E.X, E.Y, FLinearColor(0.7f + 0.3f * R.FRand(), 0.85f, 1.f, 0.9f), FMath::Max(1.f, U * 0.7f));
	}
	if (T > 2.2f)
	{
		Dim(FMath::Clamp((T - 2.2f) * 2.f, 0.f, 1.f));
		DrawRect(FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(1.f - FMath::Abs(T - 2.4f) * 4.f, 0.f, 1.f)), 0, 0, Canvas->ClipX, Canvas->ClipY);
	}
}

// ---------------------------------------------------------------- screens

void ASparkyHUD::DrawTitle(ASparkyGameMode* M)
{
	Dim(0.25f);
	DrawLogo(36);
	Portrait(EWho::Sparky, 104, 80, 3.f);
	Portrait(EWho::Leo, 174, 80, 3.f);
	Text(TEXT("&"), 160, 96, Pink, 2.f, 1);
	Panel(80, 134, 160, 74, 0.7f);
	DrawMenu(M, 142);
	Text(TEXT("STORY: REID GRAVES & CHATGPT"), 160, 216, Grey, 1.f, 1);
	ASparkyCharacter* P = ASparkyCharacter::Get(this);
	Text(P && P->HasN64Pad() ? TEXT("N64 CONTROLLER READY") : TEXT("ARROWS / ENTER   (N64 PAD: PLUG IN ANY TIME)"), 160, 228, WithAlpha(Grey, 0.7f), 1.f, 1);
}

void ASparkyHUD::DrawStory(ASparkyGameMode* M)
{
	Dim(0.72f);
	const Story::FChapter& Ch = Story::Get(M->Chapter);
	const bool bOutro = M->AfterStory != ASparkyGameMode::EAfterStory::Play;
	Text(bOutro ? FString::Printf(TEXT("CHAPTER %d - EPILOGUE"), M->Chapter) : FString::Printf(TEXT("CHAPTER %d"), M->Chapter), 160, 22, Grey, 1.f, 1);
	const FString Title = Ch.Title;
	Text(Title, 160, 34, Gold, TextWidth(Title, 2.f) < 300.f ? 2.f : 1.f, 1);
	Text(Ch.Place, 160, 54, Pink, 1.f, 1);
	if (!M->Pages.IsValidIndex(M->Page))
	{
		return;
	}
	const FLine& L = M->Pages[M->Page];
	Panel(16, 72, 288, 130);
	float TX = 26.f;
	if (L.Who != EWho::None)
	{
		Portrait(L.Who, 26, 84, 4.f);
		TX = 26 + 56 + 10;
		Text(WhoName(L.Who), TX, 82, WhoColor(L.Who));
	}
	const int32 Shown = FMath::Min(L.Text.Len(), int32(M->StateTime * 70.f));
	const TArray<FString> Lines = Wrap(L.Text.Left(Shown), int32((296.f - TX) / PixelFont::Advance));
	for (int32 i = 0; i < Lines.Num() && i < 12; ++i)
	{
		Text(Lines[i], TX, (L.Who != EWho::None ? 94.f : 84.f) + i * 10.f, L.Who == EWho::None ? FLinearColor(0.95f, 0.9f, 0.8f) : White);
	}
	Text(FString::Printf(TEXT("%d / %d"), M->Page + 1, M->Pages.Num()), 296, 192, Grey, 1.f, 2);
	if (Shown >= L.Text.Len() && FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 0.8f) < 0.5f)
	{
		Text(TEXT("PRESS A"), 160, 212, Gold, 1.f, 1);
	}
}

void ASparkyHUD::DrawClear(ASparkyGameMode* M)
{
	Dim(0.55f);
	const float T = M->StateTime;
	Text(TEXT("CHAPTER CLEAR!"), 160, 50 + FMath::Sin(T * 3.f) * 2.f, Gold, 3.f, 1);
	Text(Story::Get(M->Chapter).Title, 160, 78, Pink, 1.f, 1);
	Panel(70, 96, 180, 70);
	const int32 Secs = FMath::FloorToInt(M->ChapterTime);
	Text(FString::Printf(TEXT("CATNIP   %d / %d"), M->Catnip, M->CatnipTotal), 90, 106, Green);
	Text(FString::Printf(TEXT("TIME     %d:%02d"), Secs / 60, Secs % 60), 90, 120, White);
	Text(FString::Printf(TEXT("LIVES    %d"), M->Lives), 90, 134, Cyan);
	if (M->CatnipTotal > 0 && M->Catnip == M->CatnipTotal)
	{
		Text(TEXT("ALL CATNIP! PURRFECT!"), 160, 150, Gold, 1.f, 1);
	}
	if (T > 1.5f && FMath::Fmod(T, 0.8f) < 0.5f)
	{
		Text(TEXT("PRESS A"), 160, 190, Gold, 1.f, 1);
	}
}

void ASparkyHUD::DrawGameOver(ASparkyGameMode* M)
{
	Dim(0.7f);
	const bool bAir = M->Chapter == 4 && M->Oxygen <= 0.f;
	Text(bAir ? TEXT("OUT OF OXYGEN") : TEXT("GAME OVER"), 160, 70, Red, 3.f, 1);
	Text(bAir ? TEXT("SPARKY PASSED OUT IN THE COLD...") : TEXT("ALL NINE LIVES USED UP!"), 160, 100, Grey, 1.f, 1);
	Panel(100, 120, 120, 44);
	DrawMenu(M, 130);
}

void ASparkyHUD::DrawEnd(ASparkyGameMode* M)
{
	Dim(0.85f);
	const float T = M->StateTime;
	Text(TEXT("TO BE CONTINUED..."), 160, 60, Gold, 2.f, 1);
	Portrait(EWho::Sparky, 76, 86, 3.f);
	Portrait(EWho::Leo, 139, 86, 3.f);
	Portrait(EWho::Fluff, 202, 86, 3.f);
	Text(TEXT("NEXT TIME: THE LASER TRAP"), 160, 140, Pink, 1.f, 1);
	Text(TEXT("A PULSATING LIGHT ON THE HORIZON..."), 160, 154, Grey, 1.f, 1);
	Text(TEXT("AND A FAMILIAR FACE FROM SPARKY'S PAST."), 160, 164, Grey, 1.f, 1);
	Text(TEXT("THANKS FOR PLAYING!"), 160, 190, White, 1.f, 1);
	if (T > 2.f && FMath::Fmod(T, 0.8f) < 0.5f)
	{
		Text(TEXT("PRESS A"), 160, 210, Gold, 1.f, 1);
	}
}

void ASparkyHUD::DrawCalibration(ASparkyGameMode* M, ASparkyCharacter* P)
{
	const float SW = Canvas->ClipX / U, SH = Canvas->ClipY / U;
	if (M->IsCalibratingFromPause())
	{
		DrawPlaying(P, M);
	}
	Rect(-OX / U, -OY / U, SW, SH, FLinearColor(0.02f, 0.f, 0.05f, 0.78f));
	const FPadCalibrator& Cal = P->GetCalibrator();
	const float T = GetWorld()->GetRealTimeSeconds();
	Panel(30, 24, 260, 192, 0.7f);
	Text(TEXT("CONTROLLER SETUP"), 160, 32, Cyan, 2.f, 1);
	Text(FString::Printf(TEXT("STEP %d OF %d"), Cal.GetStep() + 1, Cal.GetNumSteps()), 160, 52, Grey, 1.f, 1);
	Bar(60, 62, 200, 3, float(Cal.GetStep()) / (Cal.GetNumSteps() - 1), Cyan);
	const FString Prompt = Cal.Prompt();
	const float Scale = TextWidth(Prompt, 1.5f) <= 240.f ? 1.5f : 1.f;
	const float Blink = 0.75f + 0.25f * FMath::Sin(T * 6.f);
	Text(Prompt, 160, 82, WithAlpha(Gold * Blink, 1.f), Scale, 1);
	Text(Cal.Hint(), 160, 100, White, 1.f, 1);
	if (Cal.GetProgress() > 0.f)
	{
		Bar(110, 112, 100, 3, Cal.GetProgress(), Green);
	}
	const FString Dev = Cal.GetDeviceName();
	if (!Dev.IsEmpty())
	{
		Text(Dev.Left(40), 160, 124, Grey, 1.f, 1);
	}
	TArray<float> Axes;
	TArray<int32> Buttons;
	Cal.GetLive(Axes, Buttons);
	const int32 NA = FMath::Min(Axes.Num(), 8);
	for (int32 i = 0; i < NA; ++i)
	{
		const float X = 160.f - NA * 14.f + i * 28.f;
		Rect(X, 140, 22, 20, FLinearColor(0, 0, 0, 0.6f));
		Rect(X + 10.5f, 140, 1, 20, FLinearColor(1, 1, 1, 0.25f));
		const float V = FMath::Clamp(Axes[i], -1.f, 1.f);
		Rect(X + 11.f + FMath::Min(0.f, V * 11.f), 146, FMath::Abs(V * 11.f), 8, FMath::Abs(V) > 0.6f ? Gold : Cyan);
		Text(FString::Printf(TEXT("A%d"), i), X + 11.f, 162, Grey, 1.f, 1, false);
	}
	FString Held;
	for (int32 B : Buttons)
	{
		Held += FString::Printf(TEXT("%d "), B);
	}
	if (!Dev.IsEmpty())
	{
		Text(Held.IsEmpty() ? FString(TEXT("BUTTONS: -")) : TEXT("BUTTONS: ") + Held, 160, 176, Grey, 1.f, 1);
	}
	Text(TEXT("ESC ON THE KEYBOARD TO CANCEL"), 160, 202, FLinearColor(0.5f, 0.55f, 0.6f), 1.f, 1);
}
