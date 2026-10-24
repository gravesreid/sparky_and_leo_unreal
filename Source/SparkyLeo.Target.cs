using UnrealBuildTool;

public class SparkyLeoTarget : TargetRules
{
	public SparkyLeoTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;
		ExtraModuleNames.Add("SparkyLeo");
	}
}
