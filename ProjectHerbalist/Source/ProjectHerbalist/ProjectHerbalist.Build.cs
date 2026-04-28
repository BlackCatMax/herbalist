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

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry",
            "Landscape"      // <--- добавлено для работы с ландшафтом
        });

        PublicIncludePaths.AddRange(new string[]
        {
            "ProjectHerbalist",
            "ProjectHerbalist/Core/Simulation/Public"
        });
    }
}