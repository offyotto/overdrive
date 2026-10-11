using UnrealBuildTool;

public class Overdrive : ModuleRules
{
    public Overdrive(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        PublicIncludePaths.Add(ModuleDirectory);
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "ApplicationCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore", "RenderCore", "RHI" });
    }
}
