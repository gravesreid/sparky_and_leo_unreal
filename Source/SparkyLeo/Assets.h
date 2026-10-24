#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;
class UTexture;

// Content made by Scripts/build_content.py.
namespace Assets
{
	inline const TCHAR* Retro = TEXT("/Game/Materials/M_Retro.M_Retro");  // unlit: Tex x VertexColor x Tint x 2, fog, Flash
	inline const TCHAR* Glow = TEXT("/Game/Materials/M_Glow.M_Glow");     // additive unlit: Color, Intensity

	UMaterialInterface* RetroMaterial();
	UMaterialInterface* GlowMaterial();
	/** /Game/Textures/<Name>, e.g. "T_Adobe". */
	UTexture* Texture(const TCHAR* Name);
}
