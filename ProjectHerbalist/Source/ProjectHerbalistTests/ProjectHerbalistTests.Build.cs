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
            "AssetRegistry", // FAssetRegistryModule::AssetCreated -- новые *CreateCommandlet, создающие DataTable-ассет с нуля (2026-09-02)
            "PCG",           // UPCGHerbalistGridSettings -- узел обратной связи «симуляция -> граф» (2026-09-03)
            // Первые тесты на виджеты (AlchemyUIBugfixesTest.cpp, 2026-09-05) --
            // FGeometry/FPointerEvent (SlateCore) и CreateWidget<>/UUserWidget
            // (UMG/Slate) раньше отсюда не вызывались ни разу, транзитивность
            // Public-зависимостей ProjectHerbalist на компоновку не хватило --
            // явные записи нужны так же, как уже явно указан InputCore выше.
            "Slate",
            "SlateCore",
            "UMG"
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