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
            "UMG",         // для UUserWidget
            "Slate",       // для FSlateDrawElement
            "SlateCore"    // для FSlateDrawElement::MakeLines и др.
        });

        PrivateDependencyModuleNames.AddRange(new string[] { });

        PublicIncludePaths.AddRange(new string[]
        {
            "ProjectHerbalist",
            "ProjectHerbalist/Core",
            "ProjectHerbalist/Core/Types",
            "ProjectHerbalist/Core/Pipeline",
            "ProjectHerbalist/Core/World",
            "ProjectHerbalist/Core/Harvest",
            "ProjectHerbalist/Core/Inventory",
            "ProjectHerbalist/Core/Alchemy",
            "ProjectHerbalist/Core/Storage",
            "ProjectHerbalist/Player",
            "ProjectHerbalist/UI"  // добавить путь к UI
        });
    }
}