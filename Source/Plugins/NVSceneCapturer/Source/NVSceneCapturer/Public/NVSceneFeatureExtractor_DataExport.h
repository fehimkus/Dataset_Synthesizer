/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#pragma once

#include "CoreMinimal.h"
#include "NVSceneFeatureExtractor.h"
#include "NVSceneCapturerUtils.h"
#include "Runtime/Engine/Public/ConvexVolume.h"
#include "NVSceneFeatureExtractor_DataExport.generated.h"

USTRUCT(BlueprintType)
struct NVSCENECAPTURER_API FNVDataExportSettings
{
    GENERATED_BODY()

public:
    FNVDataExportSettings();

    // --- Editor properties ---
    UPROPERTY(EditAnywhere, Category = "Export")
    ENVIncludeObjects IncludeObjectsType = ENVIncludeObjects::AllTaggedObjects;

    /// If true, the exporter will ignore all the hidden actors in game
    UPROPERTY(EditAnywhere, Category = "Export")
    bool bIgnoreHiddenActor = true;

    /// How to generate 3D bounding box for each exported actor mesh
    UPROPERTY(EditAnywhere, Category = "Export")
    ENVBoundsGenerationType BoundsType = ENVBoundsGenerationType::VE_OOBB;

    /// How to generate the 2D bounding box for each exported actor mesh
    UPROPERTY(EditAnywhere, Category = "Export")
    ENVBoundBox2dGenerationType BoundingBox2dType = ENVBoundBox2dGenerationType::FromMeshBodyCollision;

    UPROPERTY(EditAnywhere, Category = "Export")
    bool bOutputEvenIfNoObjectsAreInView = true;

    UPROPERTY(EditAnywhere, Category = "Export")
    FFloatInterval DistanceScaleRange = FFloatInterval(100.f, 1000.f);

    /// If true, export absolute pixel coordinates; otherwise, normalized [0,1] ratios
    UPROPERTY(EditAnywhere, Category = "Export")
    bool bExportImageCoordinateInPixel = true;
};

// ============================================================================
// Base class for all feature extractors that export scene data to JSON
// ============================================================================
UCLASS(Abstract)
class NVSCENECAPTURER_API UNVSceneFeatureExtractor_AnnotationData : public UNVSceneFeatureExtractor
{
    GENERATED_BODY()

public:
    UNVSceneFeatureExtractor_AnnotationData(const FObjectInitializer &ObjectInitializer);

    virtual void StartCapturing() override;
    virtual void UpdateCapturerSettings() override;
    virtual void UpdateSettings() override;

    /// Callback function called after capturing scene annotation data
    using OnFinishedCaptureSceneAnnotationDataCallback =
        TFunction<void(TSharedPtr<FJsonObject>, UNVSceneFeatureExtractor_AnnotationData *)>;

    /// Capture the annotation data of the scene and return it in JSON format
    bool CaptureSceneAnnotationData(OnFinishedCaptureSceneAnnotationDataCallback Callback);

protected:
    TSharedPtr<FJsonObject> CaptureSceneAnnotationData_Internal();

    void UpdateProjectionMatrix();

    /// Collects all actor data into FCapturedObjectData
    bool GatherActorData(const AActor *CheckActor, FCapturedObjectData &ActorData);

    /// Whether to include this actor in the export
    bool ShouldExportActor(const AActor *CheckActor) const;

    /// Checks if actor is within camera frustum
    bool IsActorInViewFrustum(const FConvexVolume &ViewFrustum, const AActor *CheckActor) const;

    /// Projects a world-space position to image-space coordinates
    FVector ProjectWorldPositionToImagePosition(const FVector &WorldPosition) const;

    /// Gets 2D bounding box of the actor (projected on image)
    FBox2D GetBoundingBox2D(const AActor *CheckActor, bool bClampToImage = true) const;

    /// Calculates 2D AABB of a 3D vertex list on the viewport
    FBox2D Calculate2dAABB(TArray<FVector> Vertexes, bool bClampToImage = true) const;

    /// Calculates 2D AABB of a static mesh using complex collision data
    FBox2D Calculate2dAABB_MeshComplexCollision(const class UMeshComponent *CheckMeshComp, bool bClampToImage = true) const;

protected: // Editor properties
    UPROPERTY(EditAnywhere, SimpleDisplay, Category = Config, meta = (ShowOnlyInnerProperties = true))
    FNVDataExportSettings DataExportSettings;

    UPROPERTY(Transient)
    FMatrix ViewProjectionMatrix = FMatrix::Identity;

    UPROPERTY(Transient)
    FMatrix ProjectionMatrix = FMatrix::Identity;

protected: // Runtime copy of the export settings
    FNVDataExportSettings ProtectedDataExportSettings;
};
