/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

using UnrealBuildTool;
using System.IO;

public class NVUtilities : ModuleRules
{
    public NVUtilities(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.AddRange(new string[]
        {
                // ✅ Force Unreal 5.5 Engine headers
                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Core/Public"),
                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Core/Public/Modules"),
                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Core/Public/Misc"),
                System.IO.Path.Combine(EngineDirectory, "Source/Editor/MainFrame/Public/Interfaces"),
                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Core/Public/Templates"),
                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/CoreUObject/Public"),
                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/Engine/Public"),
                System.IO.Path.Combine(EngineDirectory, "Source/Runtime/AssetRegistry/Public/AssetRegistry"),
                System.IO.Path.Combine(EngineDirectory, "Source/Editor/MainFrame/Public"),
                System.IO.Path.Combine(EngineDirectory, "Source/Editor/EditorStyle/Public")
        });
        PrivateIncludePaths.AddRange(new string[] { "NVUtilities/Private" });

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Json", "JsonUtilities", "InputCore" });
        PrivateDependencyModuleNames.AddRange(new string[] { });

        PrivatePCHHeaderFile = "Public/NVUtilitiesModule.h";

        // bFasterWithoutUnity = true;
    }
}

