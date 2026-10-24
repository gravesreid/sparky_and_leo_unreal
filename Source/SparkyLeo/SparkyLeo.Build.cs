using UnrealBuildTool;

public class SparkyLeo : ModuleRules
{
	public SparkyLeo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// MSVC is stricter about shadowed names than clang: keep them warnings so the Windows build goes through
		ShadowVariableWarningLevel = WarningLevel.Warning;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ProceduralMeshComponent", "AIModule" });
	}
}
