// Copyright RadioGuesser. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class RadioGuesser : ModuleRules
{
    public RadioGuesser(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Make all subdirectories under Source/RadioGuesser available as include roots.
        // This lets us write #include "Radio/RGRadioSubsystem.h" from anywhere in the module.
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory));

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
            "HTTP",
            "Json",
            "JsonUtilities",
            "CesiumRuntime",   // Cesium for Unreal
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // OnlineSubsystem and OnlineSubsystemUtils will be added in Phase 7 (Multiplayer)
        });
    }
}
