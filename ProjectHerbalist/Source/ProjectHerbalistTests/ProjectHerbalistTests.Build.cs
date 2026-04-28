using UnrealBuildTool;
using System.IO;   // нужно для Path

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
            "InputCore",
            "ProjectHerbalist"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "AutomationTest"
        });

        // Путь к приватной папке основного модуля относительно Source
        string SimPrivatePath = Path.Combine(ModuleDirectory, "..", "ProjectHerbalist", "Core", "Simulation", "Private");
        PrivateIncludePaths.Add(SimPrivatePath);
    }
}