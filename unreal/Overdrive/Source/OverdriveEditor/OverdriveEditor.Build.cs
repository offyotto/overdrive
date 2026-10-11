using UnrealBuildTool;

public class OverdriveEditor : ModuleRules
{
    public OverdriveEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "Overdrive", "UnrealEd", "AssetRegistry",
            "MeshDescription", "StaticMeshDescription", "EngineSettings", "RenderCore"
        });
    }
}
