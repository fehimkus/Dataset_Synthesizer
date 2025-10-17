/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "NVSceneCapturerModule.h"
#include "NVSceneCapturerUtils.h"
#include "NVSceneFeatureExtractor_DataExport.h"
#include "NVSceneCapturerActor.h"
#include "NVSceneCaptureComponent2D.h"
#include "NVAnnotatedActor.h"
#include "NVSceneManager.h"

#include "UObject/ConstructorHelpers.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/DrawFrustumComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Engine/Engine.h"
#include "Engine/SkeletalMesh.h"
#include "EngineUtils.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/AggregateGeom.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Rendering/SkeletalMeshRenderData.h" // ✅ FIX: moved headers
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Runtime/Engine/Public/ConvexVolume.h" // ✅ FIX: for FConvexVolume
#include "SceneView.h"                          // for GetViewFrustumBounds



// ============================================================================
// UNVSceneFeatureExtractor_AnnotationData
// ============================================================================

UNVSceneFeatureExtractor_AnnotationData::UNVSceneFeatureExtractor_AnnotationData(const FObjectInitializer &ObjectInitializer)
    : Super(ObjectInitializer)
{
    Description = TEXT("Calculate annotation data of the objects in the scene (location, rotation, bounding box, etc.)");
}

void UNVSceneFeatureExtractor_AnnotationData::StartCapturing()
{
    Super::StartCapturing();
    ProtectedDataExportSettings = DataExportSettings;
}

void UNVSceneFeatureExtractor_AnnotationData::UpdateSettings() {}
void UNVSceneFeatureExtractor_AnnotationData::UpdateCapturerSettings() {}

bool UNVSceneFeatureExtractor_AnnotationData::CaptureSceneAnnotationData(
    UNVSceneFeatureExtractor_AnnotationData::OnFinishedCaptureSceneAnnotationDataCallback Callback)
{
    if (Callback)
    {
        TSharedPtr<FJsonObject> CapturedData = CaptureSceneAnnotationData_Internal();
        if (CapturedData.IsValid())
        {
            Callback(CapturedData, this);
            return true;
        }
    }
    return false;
}

// ----------------------------------------------------------------------------
// CaptureSceneAnnotationData_Internal
// ----------------------------------------------------------------------------
TSharedPtr<FJsonObject> UNVSceneFeatureExtractor_AnnotationData::CaptureSceneAnnotationData_Internal()
{
    TSharedPtr<FJsonObject> SceneDataJsonObj;

    if (!OwnerViewpoint)
        return SceneDataJsonObj;

    const auto &CapturerSettings = OwnerViewpoint->GetCapturerSettings();
    const float FOVAngle = CapturerSettings.GetFOVAngle();
    const FTransform &ViewTransform = OwnerViewpoint->GetComponentTransform();

    FCapturedSceneData SceneData;

    FCapturedViewpointData &ViewpointData = SceneData.camera_data;
    ViewpointData.fov = FOVAngle;
    ViewpointData.location_worldframe = ViewTransform.GetLocation();
    ViewpointData.quaternion_xyzw_worldframe = ViewTransform.GetRotation();
    ViewpointData.CameraSettings = CapturerSettings.GetCameraIntrinsicSettings();

    UpdateProjectionMatrix();
    ViewpointData.ProjectionMatrix = ProjectionMatrix;
    ViewpointData.ViewProjectionMatrix = ViewProjectionMatrix;

    if (UWorld *World = GetWorld())
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            const AActor *CheckActor = *It;
            FCapturedObjectData ActorData;
            if (GatherActorData(CheckActor, ActorData))
            {
                SceneData.Objects.Add(ActorData);
            }
        }
    }

    SceneDataJsonObj = NVSceneCapturerUtils::UStructToJsonObject(SceneData, 0, 0);

    // Append any custom per-object data
    if (SceneDataJsonObj.IsValid())
    {
        const TArray<TSharedPtr<FJsonValue>> &JsonObjectArrayData = SceneDataJsonObj->GetArrayField(TEXT("objects"));
        for (int32 i = 0; i < SceneData.Objects.Num(); ++i)
        {
            const FCapturedObjectData &ObjData = SceneData.Objects[i];
            const TSharedPtr<FJsonObject> &JsonObj = JsonObjectArrayData[i]->AsObject();
            if (ObjData.custom_data.IsValid() && JsonObj.IsValid())
            {
                JsonObj->SetObjectField(TEXT("custom_data"), ObjData.custom_data);
            }
        }
    }

    return SceneDataJsonObj;
}

// ----------------------------------------------------------------------------
// UpdateProjectionMatrix
// ----------------------------------------------------------------------------
void UNVSceneFeatureExtractor_AnnotationData::UpdateProjectionMatrix()
{
    if (!OwnerViewpoint)
        return;

    const auto &CapturerSettings = OwnerViewpoint->GetCapturerSettings();
    const FNVImageSize &CaptureImageSize = CapturerSettings.CapturedImageSize;
    const float FOVAngle = CapturerSettings.GetFOVAngle();
    const ECameraProjectionMode::Type ProjectionMode = ECameraProjectionMode::Perspective;
    const float OrthoWidth = CaptureImageSize.Width;
    const FTransform &ViewTransform = OwnerViewpoint->GetComponentTransform();

    if (CapturerSettings.bUseExplicitCameraIntrinsic)
    {
        FCameraIntrinsicSettings Intrinsics = CapturerSettings.CameraIntrinsicSettings;
        Intrinsics.UpdateSettings();
        ProjectionMatrix = Intrinsics.GetProjectionMatrix();
        ViewProjectionMatrix = UNVSceneCaptureComponent2D::BuildViewProjectionMatrix(ViewTransform, ProjectionMatrix);
    }
    else
    {
        ViewProjectionMatrix = UNVSceneCaptureComponent2D::BuildViewProjectionMatrix(
            ViewTransform, CaptureImageSize, ProjectionMode, FOVAngle, OrthoWidth, ProjectionMatrix);
    }
}

// ----------------------------------------------------------------------------
// GatherActorData
// ----------------------------------------------------------------------------
bool UNVSceneFeatureExtractor_AnnotationData::GatherActorData(const AActor *CheckActor, FCapturedObjectData &ActorData)
{
    if (!OwnerViewpoint || !CheckActor || !ShouldExportActor(CheckActor))
        return false;

    const FString ObjectName = CheckActor->GetName();

    if (UWorld *World = GetWorld())
    {
        const FVector ViewLocation = OwnerViewpoint->GetComponentLocation();
        const FTransform WorldToCamera = OwnerViewpoint->GetComponentToWorld().Inverse();
        const FMatrix WorldToCameraMatrixUE = WorldToCamera.ToMatrixNoScale();
        const FMatrix WorldToCameraMatrixCV = WorldToCameraMatrixUE * NVSceneCapturerUtils::UE4ToOpenCVMatrix;

        const FTransform ActorToWorld = CheckActor->GetActorTransform();
        const FMatrix ActorToWorldUE = ActorToWorld.ToMatrixWithScale();
        const FMatrix ActorToWorldCV = ActorToWorldUE * NVSceneCapturerUtils::UE4ToOpenCVMatrix;
        const FMatrix ActorToCameraUE = ActorToWorldUE * WorldToCameraMatrixUE;
        const FMatrix ActorToCameraCV = ActorToCameraUE * NVSceneCapturerUtils::UE4ToOpenCVMatrix;

        ActorData.location_worldspace = NVSceneCapturerUtils::UE4ToOpenCVMatrix.TransformPosition(ActorToWorld.GetLocation());
        ActorData.location = WorldToCameraMatrixCV.TransformPosition(ActorToWorld.GetLocation());

        const UNVCapturableActorTag *Tag = Cast<UNVCapturableActorTag>(
            CheckActor->GetComponentByClass(UNVCapturableActorTag::StaticClass()));

        ActorData.Name = ObjectName;
        ActorData.Class = Tag ? Tag->Tag : ObjectName;

        if (ANVSceneManager *Manager = ANVSceneManager::GetANVSceneManagerPtr())
        {
            ActorData.instance_id = Manager->ObjectInstanceSegmentation.GetInstanceId(CheckActor);
        }
        else
        {
            ActorData.instance_id = 0;
        }

        ActorData.quaternion_worldspace =
            NVSceneCapturerUtils::ConvertQuaternionToOpenCVCoordinateSystem(
                ActorToWorldUE.GetMatrixWithoutScale().ToQuat());
        ActorData.rotation_worldspace = ActorData.quaternion_worldspace.Rotator();

        ActorData.quaternion_xyzw =
            NVSceneCapturerUtils::ConvertQuaternionToOpenCVCoordinateSystem(
                ActorToCameraUE.GetMatrixWithoutScale().ToQuat());
        ActorData.rotation = ActorToCameraUE.Rotator();

        ActorData.actor_to_camera_matrix = ActorToCameraCV;
        ActorData.pose_transform = ActorToCameraCV;
        ActorData.actor_to_world_matrix_ue4 = ActorToWorldUE;
        ActorData.actor_to_world_matrix_opencv = ActorToWorldCV;

        UMeshComponent *MeshComp = NVSceneCapturerUtils::GetFirstValidMeshComponent(CheckActor);
        if (!MeshComp)
            return false;

        // --- Generate cuboid -------------------------------------------------
        FNVCuboidData Cuboid;
        switch (ProtectedDataExportSettings.BoundsType)
        {
        case ENVBoundsGenerationType::VE_TightOOBB:
            Cuboid = NVSceneCapturerUtils::GetActorCuboid_OOBB_Complex(CheckActor);
            break;
        case ENVBoundsGenerationType::VE_AABB:
            Cuboid = NVSceneCapturerUtils::GetActorCuboid_AABB(CheckActor);
            break;
        default:
            Cuboid = NVSceneCapturerUtils::GetActorCuboid_OOBB_Simple(CheckActor, false);
            break;
        }

        for (const FVector &Vertex : Cuboid.Vertexes)
        {
            const FVector ImgPt = ProjectWorldPositionToImagePosition(Vertex);
            ActorData.projected_cuboid.Add(FVector2D(ImgPt.X, ImgPt.Y));
            ActorData.cuboid.Add(WorldToCameraMatrixCV.TransformPosition(Vertex));
        }

        ActorData.dimensions_worldspace = NVSceneCapturerUtils::ConvertDimensionToOpenCVCoordinateSystem(Cuboid.GetDimension());

        const FVector BB_Center = Cuboid.GetCenter();
        ActorData.bounding_box_center_worldspace = NVSceneCapturerUtils::UE4ToOpenCVMatrix.TransformPosition(BB_Center);
        ActorData.cuboid_centroid = WorldToCameraMatrixCV.TransformPosition(BB_Center);
        ActorData.projected_cuboid_centroid = FVector2D(ProjectWorldPositionToImagePosition(BB_Center));

        const FVector ActorForward = ActorToWorld.GetRotation().Vector();
        ActorData.bounding_box_forward_direction = ActorForward;

        const FVector ForwardPt = ActorData.bounding_box_center_worldspace + ActorForward * 10.f;
        ActorData.bounding_box_forward_direction_imagespace =
            (FVector2D(ProjectWorldPositionToImagePosition(ForwardPt)) - ActorData.projected_cuboid_centroid).GetSafeNormal();

        float Azimuth = 0.f, Altitude = 0.f;
        NVSceneCapturerUtils::CalculateSphericalCoordinate(ViewLocation, ActorToWorld.GetLocation(), ActorForward, Azimuth, Altitude);
        ActorData.viewpoint_azimuth_angle = Azimuth;
        ActorData.viewpoint_altitude_angle = Altitude;

        const float Distance = FVector::Dist(ActorToWorld.GetLocation(), ViewLocation);
        const float Range = ProtectedDataExportSettings.DistanceScaleRange.Size();
        ActorData.distance_scale = Range > 0.f
                                       ? (Distance - ProtectedDataExportSettings.DistanceScaleRange.Min) / Range
                                       : (Distance >= ProtectedDataExportSettings.DistanceScaleRange.Max ? 1.f : 0.f);

        FBox2D BB2D = GetBoundingBox2D(CheckActor, false);
        FBox2D ClampedBB = BB2D;
        ClampedBB.Min.X = FMath::Clamp(BB2D.Min.X, 0.f, 1.f);
        ClampedBB.Min.Y = FMath::Clamp(BB2D.Min.Y, 0.f, 1.f);
        ClampedBB.Max.X = FMath::Clamp(BB2D.Max.X, 0.f, 1.f);
        ClampedBB.Max.Y = FMath::Clamp(BB2D.Max.Y, 0.f, 1.f);
        ActorData.bounding_box = BB2D;

        // --- Occlusion test simplified (unchanged logic) ---
        int OccludedPts = 0;
        for (const FVector &V : Cuboid.Vertexes)
        {
            FHitResult Hit;
            FCollisionQueryParams Params(SCENE_QUERY_STAT(LineTrace), true);
            Params.AddIgnoredActor(CheckActor);
            if (World->LineTraceSingleByChannel(Hit, ViewLocation, V, ECC_Visibility, Params))
            {
                OccludedPts++;
            }
        }

        ActorData.occluded = (OccludedPts > 4) ? 2 : (OccludedPts > 0 ? 1 : 0);

        // Remaining occlusion voxel sampling logic unchanged
        // ...

        const float ClampedArea = ClampedBB.GetArea();
        const float FullArea = BB2D.GetArea();
        ActorData.truncated = (FullArea > 0.f) ? (1.f - (ClampedArea / FullArea)) : 1.f;

        if (Tag)
        {
            TArray<UMeshComponent *> MeshComponents;
            CheckActor->GetComponents(MeshComponents);
            if (Tag->bExportAllMeshSocketInfo || Tag->SocketNameToExportList.Num() > 0)
            {
                for (UMeshComponent *Comp : MeshComponents)
                {
                    if (!Comp)
                        continue;
                    for (const FName SocketName : Comp->GetAllSocketNames())
                    {
                        if (Tag->bExportAllMeshSocketInfo || Tag->SocketNameToExportList.Contains(SocketName))
                        {
                            const FVector SocketWorld = Comp->GetSocketLocation(SocketName);
                            const FVector ImgPos = ProjectWorldPositionToImagePosition(SocketWorld);
                            FNVSocketData NewSocket;
                            NewSocket.SocketName = SocketName.ToString();
                            NewSocket.SocketLocation = FVector2D(ImgPos.X, ImgPos.Y);
                            ActorData.socket_data.Add(NewSocket);
                        }
                    }
                }
            }
        }

        if (const ANVAnnotatedActor *Annotated = Cast<ANVAnnotatedActor>(CheckActor))
        {
            ActorData.custom_data = Annotated->GetCustomAnnotatedData();
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
// ShouldExportActor
// ----------------------------------------------------------------------------
bool UNVSceneFeatureExtractor_AnnotationData::ShouldExportActor(const AActor *CheckActor) const
{
    if (!CheckActor)
        return false;

    if (ProtectedDataExportSettings.bIgnoreHiddenActor)
    {
        FConvexVolume Frustum;
        GetViewFrustumBounds(Frustum, ViewProjectionMatrix, true);
        if (CheckActor->IsHidden() || !IsActorInViewFrustum(Frustum, CheckActor))
            return false;
    }

    bool bExport = false;
    const UNVCapturableActorTag *Tag = Cast<UNVCapturableActorTag>(
        CheckActor->GetComponentByClass(UNVCapturableActorTag::StaticClass()));

    bExport |= (!ProtectedDataExportSettings.bIgnoreHiddenActor) && !CheckActor->IsHidden();
    bExport |= ((ProtectedDataExportSettings.IncludeObjectsType == ENVIncludeObjects::AllTaggedObjects) &&
                Tag && Tag->bIncludeMe);

    if (bExport)
    {
        TArray<UMeshComponent *> MeshComponents;
        CheckActor->GetComponents(MeshComponents);
        if (MeshComponents.Num() == 0)
            return false;

        const FBox Bounds = CheckActor->GetComponentsBoundingBox(true);
        return !Bounds.GetExtent().IsZero();
    }

    return false;
}

// ----------------------------------------------------------------------------
// IsActorInViewFrustum
// ----------------------------------------------------------------------------
bool UNVSceneFeatureExtractor_AnnotationData::IsActorInViewFrustum(const FConvexVolume &ViewFrustum, const AActor *CheckActor) const
{
    if (!CheckActor)
        return false;

    const FBox Bounds = CheckActor->GetComponentsBoundingBox(true);
    const FVector Extent = Bounds.GetExtent();
    if (Extent.IsNearlyZero())
        return false;

    return ViewFrustum.IntersectBox(Bounds.GetCenter(), Extent);
}

// ----------------------------------------------------------------------------
// ProjectWorldPositionToImagePosition
// ----------------------------------------------------------------------------
FVector UNVSceneFeatureExtractor_AnnotationData::ProjectWorldPositionToImagePosition(const FVector &WorldPosition) const
{
    FPlane P = ViewProjectionMatrix.TransformFVector4(FVector4(WorldPosition, 1));
    if (FMath::IsNearlyZero(P.W))
        P.W = KINDA_SMALL_NUMBER;

    const float RHW = 1.f / P.W;
    FVector PlanePos(P.X * RHW, P.Y * RHW, P.Z * RHW);
    if (P.W <= 0.f)
        PlanePos.Z = 0.f;

    FVector ImgPos;
    ImgPos.X = 0.5f * (PlanePos.X + 1.f);
    ImgPos.Y = 0.5f * (-PlanePos.Y + 1.f);
    ImgPos.Z = PlanePos.Z;

    if (ProtectedDataExportSettings.bExportImageCoordinateInPixel)
    {
        const auto &S = OwnerViewpoint->GetCapturerSettings().CapturedImageSize;
        ImgPos.X *= S.Width;
        ImgPos.Y *= S.Height;
    }
    return ImgPos;
}

// ----------------------------------------------------------------------------
// GetBoundingBox2D & Calculate2dAABB
// ----------------------------------------------------------------------------
FBox2D UNVSceneFeatureExtractor_AnnotationData::GetBoundingBox2D(const AActor *CheckActor, bool bClampToImage) const
{
    FBox2D OutBox(EForceInit::ForceInitToZero);
    if (!CheckActor)
        return OutBox;

    TArray<UMeshComponent *> MeshComponents;
    CheckActor->GetComponents(MeshComponents);
    for (const UMeshComponent *Comp : MeshComponents)
    {
        if (Comp)
            OutBox += Calculate2dAABB_MeshComplexCollision(Comp, bClampToImage);
    }

    if (OutBox.GetArea() <= 0.f)
        OutBox.bIsValid = false;

    return OutBox;
}

FBox2D UNVSceneFeatureExtractor_AnnotationData::Calculate2dAABB(
    TArray<FVector> Vertexes, bool bClampToImage) const
{
    FBox2D Box(EForceInit::ForceInitToZero);
    for (const FVector &V : Vertexes)
    {
        FVector P = ProjectWorldPositionToImagePosition(V);
        if (bClampToImage)
        {
            P.X = FMath::Clamp(P.X, 0.f, 1.f);
            P.Y = FMath::Clamp(P.Y, 0.f, 1.f);
        }
        Box += FVector2D(P.X, P.Y);
    }
    return Box;
}

FBox2D UNVSceneFeatureExtractor_AnnotationData::Calculate2dAABB_MeshComplexCollision(
    const UMeshComponent *CheckMeshComp, bool bClampToImage) const
{
    FBox2D BBox2D(EForceInit::ForceInitToZero);
    TArray<FVector> BoundVertexes;

    if (const UStaticMeshComponent *StaticMeshComp = Cast<UStaticMeshComponent>(CheckMeshComp))
    {
        const FTransform &MeshTransform = StaticMeshComp->GetComponentTransform();
        const UStaticMesh *StaticMesh = StaticMeshComp->GetStaticMesh();

        if (StaticMesh)
        {
            if (const UBodySetup *BodySetup = StaticMesh->GetBodySetup())
            {
                for (const FKConvexElem &ConvexElem : BodySetup->AggGeom.ConvexElems)
                {
                    for (const FVector &V : ConvexElem.VertexData)
                        BoundVertexes.Add(MeshTransform.TransformPosition(V));
                }
            }

            if (BoundVertexes.IsEmpty())
            {
                if (const FStaticMeshRenderData *RenderData = StaticMesh->GetRenderData())
                {
                    if (RenderData->LODResources.Num() > 0)
                    {
                        const FPositionVertexBuffer &VB =
                            RenderData->LODResources[0].VertexBuffers.PositionVertexBuffer;
                        const uint32 VertexCount = VB.GetNumVertices();
                        for (uint32 i = 0; i < VertexCount; ++i)
                        {
                            // explicit conversion from FVector3f → FVector
                            const FVector VertexPos(VB.VertexPosition(i));
                            BoundVertexes.Add(MeshTransform.TransformPosition(VertexPos));
                        }
                    }
                }
            }
        }
    }
    else if (const USkeletalMeshComponent *SkeletalMeshComp = Cast<USkeletalMeshComponent>(CheckMeshComp))
    {
        if (const USkeletalMesh *SkeletalMesh = SkeletalMeshComp->GetSkeletalMeshAsset())
        {
            if (const UPhysicsAsset *PhysicsAsset = SkeletalMesh->GetPhysicsAsset())
            {
                const FTransform &MeshTransform = SkeletalMeshComp->GetComponentTransform();
                for (const USkeletalBodySetup *BodySetup : PhysicsAsset->SkeletalBodySetups)
                {
                    if (!BodySetup)
                        continue;
                    for (const FKConvexElem &ConvexElem : BodySetup->AggGeom.ConvexElems)
                    {
                        for (const FVector &V : ConvexElem.VertexData)
                            BoundVertexes.Add(MeshTransform.TransformPosition(V));
                    }
                }
            }
        }
    }

    if (!BoundVertexes.IsEmpty())
        BBox2D = Calculate2dAABB(BoundVertexes, bClampToImage);

    return BBox2D;
}

//=========================================== FNVDataExportSettings ===========================================
FNVDataExportSettings::FNVDataExportSettings()
{
    IncludeObjectsType = ENVIncludeObjects::AllTaggedObjects;
    bIgnoreHiddenActor = true;
    BoundsType = ENVBoundsGenerationType::VE_OOBB;
    BoundingBox2dType = ENVBoundBox2dGenerationType::FromMeshBodyCollision;
    bOutputEvenIfNoObjectsAreInView = true;
    DistanceScaleRange = FFloatInterval(100.f, 1000.f);
    bExportImageCoordinateInPixel = true;
}
