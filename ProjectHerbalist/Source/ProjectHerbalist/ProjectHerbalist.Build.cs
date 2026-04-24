// ProjectHerbalist.Build.cs
using UnrealBuildTool;

public class ProjectHerbalist : ModuleRules
{
    public ProjectHerbalist(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "InputCore",
            "EnhancedInput",
            "UMG",
            "Slate",
            "SlateCore",
            "DeveloperSettings"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        PublicIncludePaths.AddRange(new string[]
        {
            "ProjectHerbalist",
            "ProjectHerbalist/Core",
            "ProjectHerbalist/Core/Types",
            "ProjectHerbalist/Core/Data",
            "ProjectHerbalist/Core/Pipeline",
            "ProjectHerbalist/Core/BiomeGraph",
            "ProjectHerbalist/Core/Subsystems",
            "ProjectHerbalist/Core/World",
            "ProjectHerbalist/Core/Harvest",
            "ProjectHerbalist/Core/Inventory",
            "ProjectHerbalist/Core/Storage",
            "ProjectHerbalist/Player",
            "ProjectHerbalist/UI"
        });
    }
}