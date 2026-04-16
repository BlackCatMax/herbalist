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
            "EnhancedInput"
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
            "ProjectHerbalist/Core/Inventory",   // новая папка для инвентаря
            "ProjectHerbalist/Core/Alchemy",     // для будущего алхимического сервиса
            "ProjectHerbalist/Core/Storage",     // для будущих акторов-хранилищ
            "ProjectHerbalist/Player"
        });
    }
}