/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "RandomMaterialParameterComponentBase.h" // Own header first
#include "DomainRandomizationDNNPCH.h"            // If your module uses a PCH

#include "Components/MeshComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h" // FProperty (UE5)
#include "Engine/World.h"
#include "DRUtils.h" // DRUtils::GetValidChildMeshComponents

// Sets default values
URandomMaterialParameterComponentBase::URandomMaterialParameterComponentBase()
{
    AffectedComponentType = EAffectedMaterialOwnerComponentType::OnlyAffectMeshComponents;
    OwnerMeshComponents.Reset();
    OwnerDecalComponents.Reset();
}

void URandomMaterialParameterComponentBase::BeginPlay()
{
    AActor *OwnerActor = GetOwner();
    if (OwnerActor)
    {
        OwnerMeshComponents = DRUtils::GetValidChildMeshComponents(OwnerActor);

        // C++ API: use GetComponentsByClass (K2_* is BP-only)
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

    Super::BeginPlay();
}

void URandomMaterialParameterComponentBase::OnRandomization_Implementation()
{
    const bool bAffectMeshComponents =
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::OnlyAffectMeshComponents) ||
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::AffectBothMeshAndDecalComponents);

    const bool bAffectDecalComponents =
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::OnlyAffectDecalComponents) ||
        (AffectedComponentType == EAffectedMaterialOwnerComponentType::AffectBothMeshAndDecalComponents);

    if (bAffectMeshComponents && OwnerMeshComponents.Num() > 0)
    {
        for (UMeshComponent *CheckMeshComp : OwnerMeshComponents)
        {
            if (CheckMeshComp)
            {
                UpdateMeshMaterial(CheckMeshComp);
            }
        }
    }

    if (bAffectDecalComponents && OwnerDecalComponents.Num() > 0)
    {
        for (UDecalComponent *CheckDecalComp : OwnerDecalComponents)
        {
            if (CheckDecalComp)
            {
                UpdateDecalMaterial(CheckDecalComp);
            }
        }
    }
}

void URandomMaterialParameterComponentBase::UpdateMeshMaterial(UMeshComponent *AffectedMeshComp)
{
    // Keep original behavior (iterate owner’s cached mesh comps),
    // but you could narrow to AffectedMeshComp if desired.
    for (UMeshComponent *CheckMeshComp : OwnerMeshComponents)
    {
        if (!CheckMeshComp)
        {
            continue;
        }

        const TArray<int32> AffectedMaterialIndexes = MaterialSelectionConfigData.GetAffectMaterialIndexes(CheckMeshComp);
        for (const int32 MaterialIndex : AffectedMaterialIndexes)
        {
            if (UMaterialInstanceDynamic *MID = CheckMeshComp->CreateDynamicMaterialInstance(MaterialIndex))
            {
                UpdateMaterial(MID);
            }
        }
    }
}

void URandomMaterialParameterComponentBase::UpdateDecalMaterial(UDecalComponent *AffectedDecalComp)
{
    // Keep original behavior (iterate owner’s cached decal comps),
    // but you could narrow to AffectedDecalComp if desired.
    for (UDecalComponent *CheckDecalComp : OwnerDecalComponents)
    {
        if (!CheckDecalComp)
        {
            continue;
        }

        UMaterialInterface *CurrentDecalMaterial = CheckDecalComp->GetDecalMaterial();
        UMaterialInstanceDynamic *DecalMaterialInstance = Cast<UMaterialInstanceDynamic>(CurrentDecalMaterial);
        if (!DecalMaterialInstance)
        {
            DecalMaterialInstance = CheckDecalComp->CreateDynamicMaterialInstance();
        }

        if (DecalMaterialInstance)
        {
            UpdateMaterial(DecalMaterialInstance);
        }
    }
}

#if WITH_EDITOR
void URandomMaterialParameterComponentBase::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    FProperty *PropertyThatChanged = PropertyChangedEvent.MemberProperty;
    if (PropertyThatChanged)
    {
        const FName ChangedPropName = PropertyThatChanged->GetFName();

        if (ChangedPropName == GET_MEMBER_NAME_CHECKED(URandomMaterialParameterComponentBase, MaterialSelectionConfigData))
        {
            MaterialSelectionConfigData.PostEditChangeProperty(PropertyChangedEvent);
        }
    }

    Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR
