using UnrealBuildTool;
using System.Collections.Generic;

public class ProjectHerbalistTarget : TargetRules
{
    public ProjectHerbalistTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        bOverrideBuildEnvironment = true;
        ExtraModuleNames.AddRange(new string[] { "ProjectHerbalist" });
    }
}