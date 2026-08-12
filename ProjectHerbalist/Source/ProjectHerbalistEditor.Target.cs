using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectHerbalistEditorTarget : TargetRules
{
    public ProjectHerbalistEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;

        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;

        bOverrideBuildEnvironment = true;

        ExtraModuleNames.Add("ProjectHerbalist");
        ExtraModuleNames.Add("ProjectHerbalistTests");
    }
}