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
            "InputCore"
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        PublicIncludePaths.AddRange(new string[]
        {
            "ProjectHerbalist/Core",
            "ProjectHerbalist/Core/Types",
            "ProjectHerbalist/Core/Biome",
            "ProjectHerbalist/Core/Pipeline",
            "ProjectHerbalist/Core/World",
            "ProjectHerbalist/Core/Harvest",   // добавлено
            "ProjectHerbalist"
        });
    }
}