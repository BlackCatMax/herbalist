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

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AssetRegistry"
        });

        // Оставляем только корневую папку модуля.
        // Все инклюды в коде должны быть с префиксами, например: #include "Core/Data/IngredientRegistry.h"
        PublicIncludePaths.AddRange(new string[]
        {
            "ProjectHerbalist",
			"ProjectHerbalist/Core/Simulation/Public"

        });
    }
}