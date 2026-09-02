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
            "AutomationTest",
            "Json",          // BiomeGraphExportCommandlet.cpp
            "AssetRegistry"  // FAssetRegistryModule::AssetCreated -- новые *CreateCommandlet, создающие DataTable-ассет с нуля (2026-09-02)
        });

        // GEditor/UEditorEngine (SaveSystemTest.cpp — GetEditorWorldContext) — только
        // в редакторских сборках, этот модуль в других не собирается, но не полагаемся
        // на это молча.
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        // Путь к приватной папке основного модуля относительно Source
        string SimPrivatePath = Path.Combine(ModuleDirectory, "..", "ProjectHerbalist", "Core", "Simulation", "Private");
        PrivateIncludePaths.Add(SimPrivatePath);
    }
}