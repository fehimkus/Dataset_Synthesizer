/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "DomainRandomizationDNNPCH.h"
#include "DRSceneManager.h"
#include "GroupActorManager.h"

#include "NVSceneMarker.h"
#include "NVSceneCapturerActor.h"
#include "OrbitalMovementComponent.h"
#include "Components/RandomMovementComponent.h"

// Modern UE includes
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/Package.h"

ADRSceneManager::ADRSceneManager(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    bIsActive = true;
}

void ADRSceneManager::PostLoad()
{
    Super::PostLoad();

    if (GroupActorManager)
    {
        GroupActorManager->bAutoActive = false;
    }
}

void ADRSceneManager::BeginPlay()
{
    Super::BeginPlay();
}

void ADRSceneManager::UpdateSettingsFromCommandLine()
{
    Super::UpdateSettingsFromCommandLine();

    if (!GroupActorManager)
    {
        return;
    }

    const TCHAR *CommandLine = FCommandLine::Get();

    int32 CountPerActorOverride = 0;
    if (FParse::Value(CommandLine, TEXT("-CountPerActor="), CountPerActorOverride))
    {
        GroupActorManager->CountPerActor.Min = GroupActorManager->CountPerActor.Max = CountPerActorOverride;
    }

    int32 TotalNumberOfActorsToSpawn_MinOverride = 0;
    if (FParse::Value(CommandLine, TEXT("-TotalNumberOfActorsToSpawn_Min="), TotalNumberOfActorsToSpawn_MinOverride))
    {
        GroupActorManager->TotalNumberOfActorsToSpawn.Min = TotalNumberOfActorsToSpawn_MinOverride;
    }

    int32 TotalNumberOfActorsToSpawn_MaxOverride = 0;
    if (FParse::Value(CommandLine, TEXT("-TotalNumberOfActorsToSpawn_Max="), TotalNumberOfActorsToSpawn_MaxOverride))
    {
        GroupActorManager->TotalNumberOfActorsToSpawn.Max = TotalNumberOfActorsToSpawn_MaxOverride;
    }

    FString TrainingActorClassesOverride;
    if (FParse::Value(CommandLine, TEXT("-TrainingActorClasses="), TrainingActorClassesOverride))
    {
        GroupActorManager->ActorClassesToSpawn.Reset();

        TArray<FString> ActorClassNames;
        TrainingActorClassesOverride.ParseIntoArray(ActorClassNames, TEXT(","));

        for (const FString &ActorClassName : ActorClassNames)
        {
            FString CleanName = ActorClassName;
            CleanName.TrimStartAndEndInline();

            UClass *ActorClass = FindObject<UClass>(nullptr, *CleanName);
            if (!ActorClass)
            {
                ActorClass = LoadObject<UClass>(nullptr, *CleanName);
            }

            if (ActorClass)
            {
                GroupActorManager->ActorClassesToSpawn.Add(ActorClass);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Could not load actor class: %s"), *CleanName);
            }
        }
    }

    FString TrainingActorMeshesOverride;
    if (FParse::Value(CommandLine, TEXT("-TrainingActorMeshes="), TrainingActorMeshesOverride))
    {
        GroupActorManager->OverrideActorMeshes.Reset();

        TArray<FString> MeshNames;
        TrainingActorMeshesOverride.ParseIntoArray(MeshNames, TEXT(","));

        for (const FString &MeshPath : MeshNames)
        {
            FString CleanPath = MeshPath;
            CleanPath.TrimStartAndEndInline();

            UStaticMesh *MeshRef = FindObject<UStaticMesh>(nullptr, *CleanPath);
            if (!MeshRef)
            {
                MeshRef = LoadObject<UStaticMesh>(nullptr, *CleanPath);
            }

            if (MeshRef)
            {
                GroupActorManager->OverrideActorMeshes.AddUnique(MeshRef);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("Could not load mesh: %s"), *CleanPath);
            }
        }

        GroupActorManager->SpawnTemplateActors();
    }

    if (NoiseActorManager)
    {
        int32 NoiseObjectCountOverride = 0;
        if (FParse::Value(CommandLine, TEXT("-NoiseObjectCount="), NoiseObjectCountOverride))
        {
            NoiseActorManager->TotalNumberOfActorsToSpawn.Min =
                NoiseActorManager->TotalNumberOfActorsToSpawn.Max = NoiseObjectCountOverride;
        }
    }
}

void ADRSceneManager::SetupSceneInternal()
{
    Super::SetupSceneInternal();

    if (CurrentSceneMarker)
    {
        const FTransform &MarkerTransform = CurrentSceneMarker->GetActorTransform();
        if (GroupActorManager)
        {
            GroupActorManager->SetActorTransform(MarkerTransform);
        }
    }

    if (GroupActorManager)
    {
        GroupActorManager->SpawnActors();
    }
}

#if WITH_EDITORONLY_DATA
void ADRSceneManager::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    FProperty *PropertyThatChanged = PropertyChangedEvent.MemberProperty;
    if (PropertyThatChanged)
    {
        const FName ChangedPropName = PropertyThatChanged->GetFName();
        UE_LOG(LogTemp, Verbose, TEXT("Property changed: %s"), *ChangedPropName.ToString());
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITORONLY_DATA
