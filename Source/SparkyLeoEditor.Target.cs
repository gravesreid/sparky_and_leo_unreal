using UnrealBuildTool;

public class SparkyLeoEditorTarget : TargetRules
{
	public SparkyLeoEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V2;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_2;
		ExtraModuleNames.Add("SparkyLeo");
	}
}
