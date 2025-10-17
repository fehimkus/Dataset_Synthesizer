/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "RandomLightComponent.h"      // IWYU: own header first
#include "DomainRandomizationDNNPCH.h" // (optional) precompiled header if used
#include "Components/LightComponent.h" // for ULightComponent
#include "Engine/World.h"              // for GetWorld()
#include "Math/Color.h"
#include "Math/UnrealMathUtility.h" // for FMath
#include "UObject/UnrealType.h"     // for FProperty (if needed for editor)
#include "Logging/LogMacros.h"

// Sets default values
URandomLightComponent::URandomLightComponent()
{
    IntensityRange = FFloatInterval(1000.f, 5000.f);
}

void URandomLightComponent::OnRandomization_Implementation()
{
    if (AActor *OwnerActor = GetOwner())
    {
        TArray<ULightComponent *> RandLightCompList;
        OwnerActor->GetComponents(RandLightCompList);

        for (ULightComponent *LightComp : RandLightCompList)
        {
            if (!LightComp)
            {
                continue;
            }

            if (bShouldModifyIntensity)
            {
                const float RandIntensity = FMath::RandRange(IntensityRange.Min, IntensityRange.Max);
                LightComp->SetIntensity(RandIntensity);
            }

            if (bShouldModifyColor)
            {
                const FLinearColor RandomColor = ColorData.GetRandomColor();
                LightComp->SetLightColor(RandomColor);
            }
        }
    }
}

#if WITH_EDITOR
void URandomLightComponent::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    FProperty *PropertyThatChanged = PropertyChangedEvent.MemberProperty;
    if (PropertyThatChanged)
    {
        const FName ChangedPropName = PropertyThatChanged->GetFName();

        if (ChangedPropName == GET_MEMBER_NAME_CHECKED(URandomLightComponent, ColorData))
        {
            ColorData.PostEditChangeProperty(PropertyChangedEvent);
        }

        Super::PostEditChangeProperty(PropertyChangedEvent);
    }
}
#endif // WITH_EDITOR
