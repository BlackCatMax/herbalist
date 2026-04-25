// ProjectHerbalistTests.Build.cs
using UnrealBuildTool;

public class ProjectHerbalistTests : ModuleRules
{
    public ProjectHerbalistTests(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "ProjectHerbalist"
        });
    }
}