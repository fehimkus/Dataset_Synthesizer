/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "DomainRandomizationDNNPCH.h"
#include "Components/RandomMeshComponent.h"
#include "Components/RandomMaterialComponent.h"
#include "Components/RandomVisibilityComponent.h"
#include "Components/RandomMaterialParam_ColorComponent.h"
#include "Components/RandomMovementComponent.h"
#include "Components/RandomRotationComponent.h"
#include "RandomizedActor.h"
#include "RandomDataObject.h"

// Sets default values
ARandomizedActor::ARandomizedActor(const class FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    this->RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("RootComponnent"));

    StaticMeshComp = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("StaticMeshComp"));
    StaticMeshComp->SetupAttachment(RootComponent);

    RandomMeshComp = CreateDefaultSubobject<URandomMeshComponent>(TEXT("RandomMeshComp"));
    RandomMaterialComp = CreateDefaultSubobject<URandomMaterialComponent>(TEXT("RandomMaterialComp"));
    RandomVisibilityComp = CreateDefaultSubobject<URandomVisibilityComponent>(TEXT("RandomVisibilityComp"));
    RandomMaterialParam_ColorComp = CreateDefaultSubobject<URandomMaterialParam_ColorComponent>(TEXT("RandomMaterialParam_ColorComp"));
    RandomMovementComp = CreateDefaultSubobject<URandomMovementComponent>(TEXT("RandomMovementComp"));
    RandomRotationComp = CreateDefaultSubobject<URandomRotationComponent>(TEXT("RandomRotationComp"));

    RandomDataObjects.Reset();
}

void ARandomizedActor::OnRandomization()
{
    // TODO: Need to sort the RandomDataObjects base on its priority and sort name by alphabet order?
    for (int i = 0; i < RandomDataObjects.Num(); i++)
    {
        URandomDataObject* RandomDataObject = RandomDataObjects[i];
        if (RandomDataObject && RandomDataObject->ShouldRandomize())
        {
            RandomDataObject->OnRandomization();
        }
    }
}
