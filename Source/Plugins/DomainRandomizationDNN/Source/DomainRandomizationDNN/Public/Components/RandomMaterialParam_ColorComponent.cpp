/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "RandomMaterialParam_ColorComponent.h" // own header first
#include "DomainRandomizationDNNPCH.h"          // (optional PCH include)
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UnrealType.h" // FProperty (UE5 editor reflection)

// Sets default values
URandomMaterialParam_ColorComponent::URandomMaterialParam_ColorComponent()
{
    ColorParameterName = TEXT("Color");
}

void URandomMaterialParam_ColorComponent::PostLoad()
{
    Super::PostLoad();

    // Maintain old data which still use the deprecated property ColorParameterName
    if (MaterialParameterNames.Num() == 0 && !ColorParameterName.IsNone())
    {
        MaterialParameterNames.Add(ColorParameterName);
    }
}

void URandomMaterialParam_ColorComponent::UpdateMaterial(UMaterialInstanceDynamic *MaterialToModify)
{
    if (!MaterialToModify)
    {
        return;
    }

    for (const FName &ParamName : MaterialParameterNames)
    {
        // TODO: Add option to use the same color for all parameters or not
        const FLinearColor RandomColor = ColorData.GetRandomColor();
        MaterialToModify->SetVectorParameterValue(ParamName, RandomColor);
    }
}

#if WITH_EDITOR
void URandomMaterialParam_ColorComponent::PostEditChangeProperty(FPropertyChangedEvent &PropertyChangedEvent)
{
    FProperty *PropertyThatChanged = PropertyChangedEvent.MemberProperty;
    if (PropertyThatChanged)
    {
        const FName ChangedPropName = PropertyThatChanged->GetFName();

        if (ChangedPropName == GET_MEMBER_NAME_CHECKED(URandomMaterialParam_ColorComponent, ColorData))
        {
            ColorData.PostEditChangeProperty(PropertyChangedEvent);
        }

        Super::PostEditChangeProperty(PropertyChangedEvent);
    }
}
#endif // WITH_EDITOR
