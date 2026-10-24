#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameTypes.h"
#include "SparkyHUD.generated.h"

class ASparkyCharacter;
class ASparkyGameMode;

/**
 * Everything 2D, drawn on a virtual 320x240 screen (the N64's) scaled up in whole pixels, with
 * a 5x7 pixel font and pixel-art portraits: paws, lives and catnip, dialog boxes, the chapter
 * 4 gauges, menus and the story pages.
 */
UCLASS()
class SPARKYLEO_API ASparkyHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	float U = 1.f;   // screen pixels per virtual pixel
	float OX = 0.f;  // left edge of the 320-wide area
	float OY = 0.f;

	void Rect(float X, float Y, float W, float H, const FLinearColor& C);
	float TextWidth(const FString& S, float Scale = 1.f) const;
	/** Align: 0 left, 1 centre, 2 right. Returns the width. */
	float Text(const FString& S, float X, float Y, const FLinearColor& C, float Scale = 1.f, int32 Align = 0, bool bShadow = true);
	void Sprite(const TArray<const char*>& Rows, const TMap<char, FLinearColor>& Palette, float X, float Y, float Px = 1.f);
	void Bar(float X, float Y, float W, float H, float Frac, const FLinearColor& C, int32 Segments = 0);
	void Panel(float X, float Y, float W, float H, float Alpha = 0.8f);
	void Dim(float Alpha);
	static TArray<FString> Wrap(const FString& S, int32 MaxChars);

	void Portrait(EWho Who, float X, float Y, float Px);
	void Paw(float X, float Y, const FLinearColor& C);
	void DrawLogo(float Y);

	void DrawPlaying(ASparkyCharacter* P, ASparkyGameMode* M);
	void DrawDialogBox(EWho Who, const FString& Text, bool bBlocking, float Time);
	void DrawMenu(ASparkyGameMode* M, float Y);
	void DrawTitle(ASparkyGameMode* M);
	void DrawStory(ASparkyGameMode* M);
	void DrawClear(ASparkyGameMode* M);
	void DrawGameOver(ASparkyGameMode* M);
	void DrawEnd(ASparkyGameMode* M);
	void DrawCalibration(ASparkyGameMode* M, ASparkyCharacter* P);
	void DrawHyperspace(float T);
	void DrawQuestion(ASparkyGameMode* M);
};
