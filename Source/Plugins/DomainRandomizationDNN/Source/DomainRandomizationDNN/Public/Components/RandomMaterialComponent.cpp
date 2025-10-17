/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "RandomMaterialComponent.h"   // IWYU: own header first
#include "DomainRandomizationDNNPCH.h" // optional: precompiled header if you use one

#include "Components/MeshComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h" // for FProperty
#include "Engine/World.h"
#include "Math/UnrealMathUtility.h"
#include "RandomMaterialParameterComponentBase.h"
#include "DRUtils.h" // ensure this exists for DRUtils::GetValidChildMeshComponents
#include "Logging/LogMacros.h"

// Sets default values
URandomMaterialComponent::URandomMaterialComponent()
{
    AffectedComponentType = EAffectedMaterialOwnerComponentType::OnlyAffectMeshComponents;
    OwnerMeshComponents.Reset();
    OwnerDecalComponents.Reset();
}

void URandomMaterialComponent::BeginPlay()
{
    AActor *OwnerActor = GetOwner();
    if (OwnerActor)
    {
        OwnerMeshComponents = DRUtils::GetValidChildMeshComponents(OwnerActor);

        TArray<UActorComponent *> ChildDecalComps = OwnerActor->K2_GetComponentsByClass(UDecalComponent::StaticClass());
        OwnerDecalComponents.Reset();

        for (UActorComponent *CheckComp : ChildDecalComps)
        {
            if (UDecalComponent *CheckDecalComp = Cast<UDecalComponent>(CheckComp))
            {
                OwnerDecalComponents.Add(CheckDecalComp);
            }
        }
    }

    if (bUseAllMaterialInDirectories)
    {
        MaterialStreamer.Init(MaterialDirectories, UMaterialInterface::StaticClass());
    }

    // NOTE: Only randomize once if there's only 1 material to choose from
    if (bUseAllMaterialInDirectories)
    {
        if (MaterialStreamer.GetAssetsCount() <= 1)
        {
            bOnlyRandomizeOnce = true;
        }
    }
    else if (MaterialList.Num() <= 1)
    {
        bOnlyRandomizeOnce = true;
    }

    Super::BeginPlay();
}

void URandomMaterialComponent::OnRandomization_Implementation()
{
    if (!HasMaterialToRandomize())
    {
        return;
    }

    const bool bAffectMeshComponents =
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::OnlyAffectMeshComponents) ||
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::AffectBothMeshAndDecalComponents);

    const bool bAffectDecalComponents =
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::OnlyAffectDecalComponents) ||
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::AffectBothMeshAndDecalComponents);

    bool bActorMaterialChanged = false;

    // Update the owner's mesh components list if it's not initialized
    if (OwnerMeshComponents.Num() == 0)
    {
        if (AActor *OwnerActor = GetOwner())
        {
            OwnerMeshComponents = DRUtils::GetValidChildMeshComponents(OwnerActor);
        }
    }

    // ✅ Affect mesh components
    if (bAffectMeshComponents && OwnerMeshComponents.Num() > 0)
    {
        for (UMeshComponent *CheckMeshComp : OwnerMeshComponents)
        {
            if (!CheckMeshComp)
                continue;

            if (UMaterialInterface *NewMaterial = GetNextMaterial())
            {
                const TArray<int32> AffectedMaterialIndexes = MaterialSelectionConfigData.GetAffectMaterialIndexes(CheckMeshComp);
                for (const int32 MaterialIndex : AffectedMaterialIndexes)
                {
                    CheckMeshComp->SetMaterial(MaterialIndex, NewMaterial);
                }
                bActorMaterialChanged = true;
            }
        }
    }

    // ✅ Affect decal components
    if (bAffectDecalComponents && OwnerDecalComponents.Num() > 0)
    {
        for (UDecalComponent *CheckDecalComp : OwnerDecalComponents)
        {
            if (!CheckDecalComp)
                continue;

            if (UMaterialInterface *NewMaterial = GetNextMaterial())
            {
                CheckDecalComp->SetDecalMaterial(NewMaterial);
                bActorMaterialChanged = true;
            }
        }
    }

    // ✅ Notify other randomizers
    if (bActorMaterialChanged)
    {
        if (AActor *OwnerActor = GetOwner())
        {
            TArray<URandomMaterialParameterComponentBase *> RandMatParamCompList;
            OwnerActor->GetComponents(RandMatParamCompList);

            for (URandomMaterialParameterComponentBase *RandMatParamComp : RandMatParamCompList)
            {
                if (RandMatParamComp && RandMatParamComp->ShouldRandomize())
                {
                    RandMatParamComp->Randomize();
                }
            }
        }
    }
}

#if WITH_EDITOR
void URandomMaterialComponent::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    FProperty *PropertyThatChanged = PropertyChangedEvent.MemberProperty;
    if (PropertyThatChanged)
    {
        const FName ChangedPropName = PropertyThatChanged->GetFName();

        if (ChangedPropName == GET_MEMBER_NAME_CHECKED(URandomMaterialComponent, MaterialSelectionConfigData))
        {
            MaterialSelectionConfigData.PostEditChangeProperty(PropertyChangedEvent);
        }

        Super::PostEditChangeProperty(PropertyChangedEvent);
    }
}
#endif // WITH_EDITOR

bool URandomMaterialComponent::HasMaterialToRandomize() const
{
#if WITH_EDITOR
    return bUseAllMaterialInDirectories ? MaterialStreamer.HasAssets() : (MaterialList.Num() > 0);
#else
    return false;
#endif
}

UMaterialInterface *URandomMaterialComponent::GetNextMaterial()
{
#if WITH_EDITOR
    // Choose a random material in the list
    UMaterialInterface *NewMaterial = nullptr;

    if (bUseAllMaterialInDirectories)
    {
        NewMaterial = MaterialStreamer.GetNextAsset<UMaterialInterface>();
    }
    else if (MaterialList.Num() > 0)
    {
        const int32 RandomIndex = FMath::Rand() % MaterialList.Num();
        NewMaterial = MaterialList[RandomIndex];
    }

    return NewMaterial;
#else
    return nullptr;
#endif
}
