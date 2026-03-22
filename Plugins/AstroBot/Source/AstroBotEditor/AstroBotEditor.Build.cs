using UnrealBuildTool;

public class AstroBotEditor : ModuleRules
{
    public AstroBotEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "AstroBot"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
			{
				"CoreUObject",
				"Engine",
				"Kismet",
				"AssetRegistry",
				"AssetTools",
				"UnrealEd",
				"Json",
				"JsonUtilities"
			}
        );
    }
}
