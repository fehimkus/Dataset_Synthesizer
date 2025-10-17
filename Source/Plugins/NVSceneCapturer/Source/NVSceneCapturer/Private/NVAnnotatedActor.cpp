/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/
#include "NVSceneCapturerModule.h"
#include "NVSceneCapturerUtils.h"
#include "NVAnnotatedActor.h"
#include "NVCoordinateComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine.h"
#if WITH_EDITOR
#include "Factories/FbxAssetImportData.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#endif

ANVAnnotatedActor::ANVAnnotatedActor(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("Root"));
    BoxComponent->SetMobility(EComponentMobility::Movable);

    RootComponent = BoxComponent;

    MeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComponent"));
    MeshComponent->SetupAttachment(RootComponent);

    CoordComponent = CreateDefaultSubobject<UNVCoordinateComponent>(TEXT("CoordComponent"));
    CoordComponent->SetupAttachment(RootComponent);

    AnnotationTag = CreateDefaultSubobject<UNVCapturableActorTag>(TEXT("AnnotationTag"));

    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

#if WITH_EDITORONLY_DATA
    USelection::SelectObjectEvent.AddUObject(this, &ANVAnnotatedActor::OnActorSelected);
#endif //WITH_EDITORONLY_DATA

    bForceCenterOfBoundingBoxAtRoot = true;
    bSetClassNameFromMesh = false;
}

void ANVAnnotatedActor::PostLoad()
{
    Super::PostLoad();
}

void ANVAnnotatedActor::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    ensure(World);
#if WITH_EDITOR
    bool bIsSimulating = GUnrealEd ? (GUnrealEd->bIsSimulatingInEditor || (GUnrealEd->PlayWorld != nullptr)) : false;
    if (!World || !World->IsGameWorld() || bIsSimulating)
    {
        return;
    }
#endif

    UpdateStaticMesh();
}

void ANVAnnotatedActor::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    UpdateStaticMesh();
}

#if WITH_EDITORONLY_DATA
void ANVAnnotatedActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
    const FProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
    if (PropertyThatChanged)
    {
        const FName ChangedPropName = PropertyThatChanged->GetFName();
        if ((ChangedPropName == GET_MEMBER_NAME_CHECKED(ANVAnnotatedActor, MeshComponent)))
        {
            UpdateStaticMesh();
        }

        Super::PostEditChangeProperty(PropertyChangedEvent);
    }
}

void ANVAnnotatedActor::OnActorSelected(UObject* Object)
{
    // TODO: Show the  3d bounding box and facing direction
}
#endif //WITH_EDITORONLY_DATA


void ANVAnnotatedActor::SetStaticMesh(class UStaticMesh* NewMesh)
{
    ensure(NewMesh);
    if (MeshComponent)
    {
        MeshComponent->SetStaticMesh(NewMesh);
        UpdateStaticMesh();
    }
}

TSharedPtr<FJsonObject> ANVAnnotatedActor::GetCustomAnnotatedData() const
{
    return nullptr;
}

void ANVAnnotatedActor::SetClassSegmentationId(uint32 NewId)
{
    if (MeshComponent)
    {
        MeshComponent->CustomDepthStencilValue = (int32)NewId;
        MeshComponent->SetRenderCustomDepth(true);
    }
}

void ANVAnnotatedActor::UpdateMeshCuboid()
{
    UStaticMesh* ActorStaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
    if (ActorStaticMesh)
    {
        const FMatrix& PCATransformMat = CalculatePCA(ActorStaticMesh);
        PCACenter = PCATransformMat.GetOrigin();
        const FVector& TempPCADirection = PCATransformMat.GetUnitAxis(EAxis::Z);
        PCARotation = (-TempPCADirection).ToOrientationRotator();

        CoordComponent->SetRelativeRotation(PCARotation);

        // NOTE: We need to check all the mesh's vertices here for accuracy
        const bool bCheckMeshCollision = false;
        MeshCuboid = NVSceneCapturerUtils::GetMeshCuboid_OOBB_Simple(MeshComponent, false, bCheckMeshCollision);

        const FVector& MeshCuboidSize = MeshCuboid.GetDimension();
        BoxComponent->SetBoxExtent(MeshCuboidSize * 0.5f);
        CoordComponent->SetSize(MeshCuboidSize);

        CuboidDimension = MeshCuboidSize;
        CuboidCenterLocal = MeshCuboid.GetCenter();
    }
}

FMatrix ANVAnnotatedActor::CalculatePCA(const UStaticMesh *Mesh)
{
    FMatrix TransformMatrix = FMatrix::Identity;

    if (!Mesh)
        return TransformMatrix;

    // ✅ UE5-safe: access render data properly
    const FStaticMeshRenderData *RenderData = Mesh->GetRenderData();
    if (!RenderData || RenderData->LODResources.Num() == 0)
        return TransformMatrix;

    const FPositionVertexBuffer &MeshVertexBuffer = RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
    const uint32 VertexCount = MeshVertexBuffer.GetNumVertices();
    if (VertexCount == 0)
        return TransformMatrix;

    // Collect all vertices
    TArray<FVector> MeshVertices;
    MeshVertices.Reserve(VertexCount);
    for (uint32 i = 0; i < VertexCount; i++)
    {
        // UE5: VertexPosition() returns FVector3f (float) → convert to FVector (double)
        const FVector3f VertexPosF = MeshVertexBuffer.VertexPosition(i);
        MeshVertices.Add(FVector(VertexPosF));
    }

    // Compute mean vertex
    FVector MeanVertex = FVector::ZeroVector;
    for (const FVector &V : MeshVertices)
        MeanVertex += V;
    MeanVertex /= VertexCount;

    // Compute covariance matrix
    FMatrix Covariance = FMatrix::Identity;
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            double Cij = 0.0;
            for (const FVector &V : MeshVertices)
            {
                Cij += (V[i] - MeanVertex[i]) * (V[j] - MeanVertex[j]);
            }
            Cij /= VertexCount;
            Covariance.M[i][j] = static_cast<float>(Cij);
        }
    }

    // Eigen decomposition (dominant eigenvector via power method)
    FVector ZAxis = ComputeEigenVector(Covariance);
    FVector XAxis, YAxis;
    ZAxis.FindBestAxisVectors(YAxis, XAxis);

    TransformMatrix = FMatrix(XAxis, YAxis, ZAxis, MeanVertex);
    return TransformMatrix;
}

FVector ANVAnnotatedActor::ComputeEigenVector(const FMatrix& Mat)
{
    // NOTES: Copied from function ComputeEigenVector in PhysicsASsetUtils.cpp
    //using the power method: this is ok because we only need the dominate eigenvector and speed is not critical:
    // http://en.wikipedia.org/wiki/Power_iteration
    FVector EVector = FVector(0, 0, 1.f);
    for (int32 i = 0; i < 32; ++i)
    {
        float Length = EVector.Size();
        if (Length > 0.f)
        {
            EVector = Mat.TransformVector(EVector) / Length;
        }
    }

    return EVector.GetSafeNormal();
}

void ANVAnnotatedActor::UpdateStaticMesh()
{
    UpdateMeshCuboid();

    if (bForceCenterOfBoundingBoxAtRoot)
    {
        MeshComponent->SetRelativeLocation(-CuboidCenterLocal);
        CuboidCenterLocal = FVector::ZeroVector;
    }

    if (bSetClassNameFromMesh)
    {
        if (AnnotationTag)
        {
            const UStaticMesh* ActorStaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
            if (ActorStaticMesh)
            {
                AnnotationTag->Tag = ActorStaticMesh->GetName();
            }
        }
    }
}

FMatrix ANVAnnotatedActor::GetMeshInitialMatrix() const
{
    FMatrix ResultFMatrix = FMatrix::Identity;

    UStaticMesh* ActorStaticMesh = MeshComponent ? MeshComponent->GetStaticMesh() : nullptr;
    if (ActorStaticMesh)
    {
#if WITH_EDITOR
        UFbxAssetImportData* FbxAssetImportData = Cast<UFbxAssetImportData>(ActorStaticMesh->AssetImportData);
        FMatrix MeshImportMatrix = FMatrix::Identity;

        if (FbxAssetImportData)
        {
            FTransform AssetImportTransform(
                FbxAssetImportData->ImportRotation.Quaternion(),
                FbxAssetImportData->ImportTranslation,
                FVector(FbxAssetImportData->ImportUniformScale)
            );

            MeshImportMatrix = AssetImportTransform.ToMatrixWithScale();
        }

        const FTransform& MeshRelativeTransform = MeshComponent->GetRelativeTransform();
        const FMatrix& MeshRelativeMatrix = MeshRelativeTransform.ToMatrixWithScale();

        const FMatrix& MeshInitialMatrix = MeshImportMatrix * MeshRelativeMatrix;
        FMatrix MeshInitialMatrix_OpenCV = MeshInitialMatrix * NVSceneCapturerUtils::UE4ToOpenCVMatrix;

        // HACK: Invert the Y axis - need to figure out why?
        FVector MatX, MatY, MatZ;
        MeshInitialMatrix_OpenCV.GetScaledAxes(MatX, MatY, MatZ);
        MatY = -MatY;
        MeshInitialMatrix_OpenCV.SetAxes(nullptr, &MatY, nullptr);
        ResultFMatrix = MeshInitialMatrix_OpenCV;
#endif // WITH_EDITOR
    }
    return ResultFMatrix;
}
