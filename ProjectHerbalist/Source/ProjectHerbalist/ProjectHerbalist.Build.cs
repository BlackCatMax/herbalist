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
			"ProjectHerbalist"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");
	}
}