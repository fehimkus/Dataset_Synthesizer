/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#pragma once

// UE5: IWYU — put CoreMinimal first
#include "CoreMinimal.h"

#include "NVSceneCapturerModule.h"
#include "Components/ActorComponent.h"
#include "Components/StaticMeshComponent.h"

#include "IImageWrapper.h"   // EImageFormat
#include "Misc/FileHelper.h" // File IO (UE5 path)
#include "Misc/Paths.h"      // FPaths (UE5 path)

#include "Kismet/BlueprintFunctionLibrary.h"

#include "Serialization/JsonSerializerMacros.h"
#include "Serialization/JsonTypes.h"
#include "Json.h"
#include "JsonObjectConverter.h"     // In JsonUtilities module
#include "Templates/SharedPointer.h" // TSharedPtr (UE5 path)

#include "Engine/TextureRenderTarget2D.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "NVCameraSettings.h"

#include "UObject/UnrealType.h" // FProperty and friends (UE5 reflection)

#include "NVSceneCapturerUtils.generated.h"

// NOTE: Should remove this enum when the EImageFormat in IImageWrapper marked as UENUM
UENUM(BlueprintType)
enum class ENVImageFormat : uint8
{
    PNG UMETA(DisplayName = "PNG (Portable Network Graphics)."),
    JPEG UMETA(DisplayName = "JPEG (Joint Photographic Experts Group)."),
    GrayscaleJPEG UMETA(DisplayName = "GrayscaleJPEG (Single channel jpeg"),
    BMP UMETA(DisplayName = "BMP (Windows Bitmap"),
    NVImageFormat_MAX UMETA(Hidden)
};
EImageFormat ConvertExportFormatToImageFormat(ENVImageFormat ExportFormat);
FString GetExportImageExtension(ENVImageFormat ExportFormat);

UENUM()
enum ENVCapturedPixelFormat
{
    R8,
    RGBA8,
    R8G8,
    R32f,
    NVCapturedPixelFormat_MAX UMETA(Hidden)
};
ETextureRenderTargetFormat ConvertCapturedFormatToRenderTargetFormat(ENVCapturedPixelFormat PixelFormat);

USTRUCT()
struct FNVTexturePixelData
{
    GENERATED_BODY()

public:
    UPROPERTY(Transient)
    TArray<uint8> PixelData;

    EPixelFormat PixelFormat;

    UPROPERTY(Transient)
    uint32 RowStride;

    UPROPERTY(Transient)
    FIntPoint PixelSize;
};

USTRUCT()
struct NVSCENECAPTURER_API FNVSocketData
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FString SocketName;

    UPROPERTY()
    FVector2D SocketLocation;
};

UENUM(BlueprintType)
enum class ENVCuboidVertexType : uint8
{
    FrontTopRight = 0,
    FrontTopLeft,
    FrontBottomLeft,
    FrontBottomRight,
    RearTopRight,
    RearTopLeft,
    RearBottomLeft,
    RearBottomRight,

    CuboidVertexType_MAX UMETA(Hidden)
};

USTRUCT(BlueprintType)
struct NVSCENECAPTURER_API FNVCuboidData
{
    GENERATED_BODY();

public:
    UPROPERTY()
    FVector Vertexes[(uint8)ENVCuboidVertexType::CuboidVertexType_MAX];

    static uint8 TotalVertexesCount;

public:
    FNVCuboidData();
    FNVCuboidData(const FBox &AABB);
    FNVCuboidData(const FBox &LocalBox, const FTransform &LocalTransform);

    void BuildFromAABB(const FBox &AABB);
    void BuildFromOOBB(const FBox &OOBB, const FTransform &LocalTransform);

    FVector GetCenter() const { return Center; }
    FVector GetVertex(ENVCuboidVertexType VertexType) const { return Vertexes[(uint8)VertexType]; }
    FVector GetExtent() const { return LocalBox.GetExtent(); }
    FVector GetDimension() const { return LocalBox.GetExtent() * 2.f; }
    FVector GetDirection() const { return Rotation.Vector(); }
    FQuat GetRotation() const { return Rotation; }
    bool IsValid() const { return (LocalBox.IsValid != 0); }

private:
    UPROPERTY()
    FVector Center;

    UPROPERTY()
    FBox LocalBox;

    UPROPERTY()
    FQuat Rotation;
};

USTRUCT()
struct NVSCENECAPTURER_API FNVBox2D
{
    GENERATED_BODY()

    FNVBox2D(const FBox2D &box = FBox2D())
    {
        top_left = FVector2D(box.Min.Y, box.Min.X);
        bottom_right = FVector2D(box.Max.Y, box.Max.X);
    }

public:
    UPROPERTY()
    FVector2D top_left;

    UPROPERTY()
    FVector2D bottom_right;
};

USTRUCT()
struct NVSCENECAPTURER_API FCapturedObjectData
{
    GENERATED_BODY()

public:
    UPROPERTY(Transient)
    FString Name;

    UPROPERTY()
    FString Class;

    UPROPERTY()
    uint32 instance_id;

    UPROPERTY(Transient)
    float truncated;

    UPROPERTY(Transient)
    uint32 occluded;

    UPROPERTY(Transient)
    float occlusion;

    UPROPERTY()
    float visibility;

    UPROPERTY(Transient)
    FVector dimensions_worldspace;

    UPROPERTY(Transient)
    FVector location_worldspace;

    UPROPERTY()
    FVector location;

    UPROPERTY(Transient)
    FRotator rotation_worldspace;

    UPROPERTY(Transient)
    FQuat quaternion_worldspace;

    UPROPERTY(Transient)
    FRotator rotation;

    UPROPERTY()
    FQuat quaternion_xyzw;

    UPROPERTY(Transient)
    FMatrix actor_to_world_matrix_ue4;

    UPROPERTY(Transient)
    FMatrix actor_to_world_matrix_opencv;

    UPROPERTY(Transient)
    FMatrix actor_to_camera_matrix;

    UPROPERTY()
    FMatrix pose_transform;

    UPROPERTY(Transient)
    FVector bounding_box_center_worldspace;

    UPROPERTY()
    FVector cuboid_centroid;

    UPROPERTY()
    FVector2D projected_cuboid_centroid;

    UPROPERTY(Transient)
    FVector bounding_box_forward_direction;

    UPROPERTY(Transient)
    FVector2D bounding_box_forward_direction_imagespace;

    UPROPERTY(Transient)
    float viewpoint_azimuth_angle;

    UPROPERTY(Transient)
    float viewpoint_altitude_angle;

    UPROPERTY(Transient)
    float distance_scale;

    UPROPERTY()
    FNVBox2D bounding_box;

    UPROPERTY()
    TArray<FVector> cuboid;

    UPROPERTY()
    TArray<FVector2D> projected_cuboid;

    UPROPERTY(Transient)
    TArray<FNVSocketData> socket_data;

    TSharedPtr<FJsonObject> custom_data;
};

USTRUCT()
struct NVSCENECAPTURER_API FCapturedViewpointData
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FVector location_worldframe;

    UPROPERTY()
    FQuat quaternion_xyzw_worldframe;

    UPROPERTY(Transient)
    FMatrix ProjectionMatrix;

    UPROPERTY(Transient)
    FMatrix ViewProjectionMatrix;

    UPROPERTY(Transient)
    FCameraIntrinsicSettings CameraSettings;

    UPROPERTY(Transient)
    float fov;
};

USTRUCT()
struct NVSCENECAPTURER_API FCapturedSceneData
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FCapturedViewpointData camera_data;

    UPROPERTY()
    TArray<FCapturedObjectData> Objects;
};

USTRUCT()
struct NVSCENECAPTURER_API FCapturedFrameData
{
    GENERATED_BODY()

public:
    UPROPERTY()
    FCapturedSceneData Data;

    UPROPERTY()
    TArray<FColor> SceneBitmap;
};

UCLASS(ClassGroup = (NVIDIA), meta = (BlueprintSpawnableComponent))
class NVSCENECAPTURER_API UNVCapturableActorTag : public UActorComponent
{
    GENERATED_BODY()
public:
    UNVCapturableActorTag() : bIncludeMe(true) {}

    bool IsValid() const { return bIncludeMe && !Tag.IsEmpty(); }

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    FString Tag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bIncludeMe;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
    bool bExportAllMeshSocketInfo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config", meta = (editcondition = "!bExportAllMeshSocketInfo"))
    TArray<FName> SocketNameToExportList;
};

UENUM(BlueprintType)
enum class ENVIncludeObjects : uint8
{
    AllTaggedObjects,
    MatchesTag
};

UENUM(BlueprintType)
enum class ENVBoundsGenerationType : uint8
{
    VE_AABB UMETA(DisplayName = "AABB"),
    VE_OOBB UMETA(DisplayName = "OOBB"),
    VE_TightOOBB UMETA(DisplayName = "Tight Arbitrary OOBB")
};

UENUM(BlueprintType)
enum class ENVBoundBox2dGenerationType : uint8
{
    From3dBoundingBox,
    FromMeshBodyCollision,
};

USTRUCT(BlueprintType)
struct NVSCENECAPTURER_API FNVSceneExporterConfig
{
    GENERATED_BODY()

public:
    FNVSceneExporterConfig();

protected:
    UPROPERTY(EditAnywhere, Category = "Export")
    bool bExportObjectData;

    UPROPERTY(EditAnywhere, Category = "Export")
    bool bExportScreenShot;

    UPROPERTY(EditAnywhere, Category = "Export")
    ENVIncludeObjects IncludeObjectsType;

    UPROPERTY(EditAnywhere, Category = "Export")
    bool bIgnoreHiddenActor;

    UPROPERTY(EditAnywhere, Category = "Export")
    ENVBoundsGenerationType BoundsType;

    UPROPERTY(EditAnywhere, Category = "Export")
    ENVBoundBox2dGenerationType BoundingBox2dType;

    UPROPERTY(EditAnywhere, Category = "Export")
    bool bOutputEvenIfNoObjectsAreInView;

    UPROPERTY(EditAnywhere, Category = "Export")
    FFloatInterval DistanceScaleRange;
};

USTRUCT(BlueprintType)
struct NVSCENECAPTURER_API FNVSceneCapturerSettings
{
    GENERATED_BODY();

public:
    FNVSceneCapturerSettings();

    float GetFOVAngle() const;
    void RandomizeSettings();
    FCameraIntrinsicSettings GetCameraIntrinsicSettings() const;

#if WITH_EDITORONLY_DATA
    void PostEditChangeProperty(struct FPropertyChangedEvent &PropertyChangedEvent);
#endif

public:
    UPROPERTY(VisibleAnywhere, Category = CapturerSettings)
    ENVImageFormat ExportImageFormat;

    UPROPERTY(Transient, VisibleAnywhere, Category = CapturerSettings, meta = (EditCondition = "!bUseExplicitCameraIntrinsic"))
    float FOVAngle;

    UPROPERTY(EditAnywhere, Category = CapturerSettings, meta = (DisplayName = "Field of View", UIMin = "5.0", UIMax = "170", ClampMin = "0.001", ClampMax = "360.0", EditCondition = "!bUseExplicitCameraIntrinsic"))
    FFloatInterval FOVAngleRange;

    UPROPERTY(EditAnywhere, Category = CapturerSettings)
    FNVImageSize CapturedImageSize;

    UPROPERTY(EditAnywhere, AdvancedDisplay, Category = CapturerSettings)
    int32 MaxSaveImageAsyncTaskCount;

    UPROPERTY(EditAnywhere, Category = CapturerSettings)
    bool bUseExplicitCameraIntrinsic;

    UPROPERTY(EditAnywhere, Category = CapturerSettings, meta = (EditCondition = bUseExplicitCameraIntrinsic))
    FCameraIntrinsicSettings CameraIntrinsicSettings;

    UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = CapturerSettings)
    FMatrix CameraIntrinsicMatrix;

    UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = CapturerSettings)
    FMatrix CameraProjectionMatrix;
};

UENUM()
enum class ENVSceneCapturerState : uint8
{
    NotActive UMETA(DisplayName = "NotActive. The capturer is not active."),
    Active UMETA(DisplayName = "Active.    The capturer is active. but not started."),
    Running UMETA(DisplayName = "Running.   The capturer is running/exporting."),
    Paused UMETA(DisplayName = "Paused.    The capturer is paused, can be resumed."),
    Completed UMETA(DisplayName = "Completed. The capturer finished exporting a batch."),
    NVSceneCapturerState_MAX UMETA(Hidden)
};

namespace NVSceneCapturerStateString
{
    extern const FString NotStarted_Str;
    extern const FString Running_Str;
    extern const FString Paused_Str;
    extern const FString Completed_Str;

    NVSCENECAPTURER_API FString ConvertExporterStateToString(ENVSceneCapturerState ExporterState);
};

USTRUCT(BlueprintType)
struct NVSCENECAPTURER_API FNVFrameCounter
{
    GENERATED_BODY()

public:
    FNVFrameCounter();

    float GetFPS() const { return CachedFPS; }
    int32 GetTotalFrameCount() const { return TotalFrameCount; }

    void Reset();
    void IncreaseFrameCount(int AdditionalFrameCount = 1);
    void SetFrameCount(int NewFrameCount);
    void AddFrameDuration(float NewDuration, bool bIncreaseFrame = false);

protected:
    void UpdateFPS();

    int32 TotalFrameCount;
    float CachedFPS;
    int FPSAccumulatedFrames;
    float FPSAccumulatedDuration;
};

namespace NVSceneCapturerUtils
{
    extern const FMatrix UE4ToOpenCVMatrix;
    extern const FMatrix OpenCVToUE4Matrix;
    extern const FMatrix ObjToUE4Matrix;
    extern const uint32 MaxVertexColorID;

    NVSCENECAPTURER_API FQuat ConvertQuaternionToOpenCVCoordinateSystem(const FQuat &InQuat);
    NVSCENECAPTURER_API FVector ConvertDimensionToOpenCVCoordinateSystem(const FVector &InDimension);

    NVSCENECAPTURER_API FString GetGameDataOutputFolder();
    NVSCENECAPTURER_API FString GetDefaultDataOutputFolder();
    NVSCENECAPTURER_API FString GetOutputFileFullPath(uint32 Index, FString Extension, const FString &Subfolder, FString Filename = TEXT(""), uint8 ZeroPad = 6);

    // UE5: UProperty -> FProperty
    NVSCENECAPTURER_API TSharedPtr<FJsonValue> CustomPropertyToJsonValueFunc(FProperty *PropertyType, const void *Value);

    template <typename InStructType>
    TSharedPtr<FJsonObject> UStructToJsonObject(const InStructType &InStructData, int64 CheckFlags = 0, int64 SkipFlags = 0)
    {
        FJsonObjectConverter::CustomExportCallback CustomPropertyToJsonValue;
        if (!CustomPropertyToJsonValue.IsBound())
        {
            CustomPropertyToJsonValue.BindStatic(&NVSceneCapturerUtils::CustomPropertyToJsonValueFunc);
        }
        return FJsonObjectConverter::UStructToJsonObject(InStructData, CheckFlags, SkipFlags, &CustomPropertyToJsonValue);
    }

    NVSCENECAPTURER_API bool SaveJsonObjectToFile(const TSharedPtr<FJsonObject> &JsonObjData, const FString &Filename);
    NVSCENECAPTURER_API FString GetExportImageExtension(EImageFormat ImageFormat);

    NVSCENECAPTURER_API UMeshComponent *GetFirstValidMeshComponent(const AActor *CheckActor);
    NVSCENECAPTURER_API TArray<FVector> GetSimpleCollisionVertexes(const class UMeshComponent *MeshComp);

    NVSCENECAPTURER_API uint8 GetBitCountPerChannel(EPixelFormat PixelFormat);
    NVSCENECAPTURER_API uint8 GetColorChannelCount(EPixelFormat PixelFormat);
    NVSCENECAPTURER_API uint8 GetPixelByteSize(EPixelFormat PixelFormat);

    NVSCENECAPTURER_API FColor ConvertByteIndexToColor(uint8 Index);
    NVSCENECAPTURER_API FColor ConvertInt32ToRGB(uint32 Value);
    NVSCENECAPTURER_API FColor ConvertInt32ToRGBA(uint32 Value);
    NVSCENECAPTURER_API FColor ConvertInt32ToVertexColor(uint32 Value);

    NVSCENECAPTURER_API void SetMeshVertexColor(AActor *MeshOwnerActor, const FColor &VertexColor);
    NVSCENECAPTURER_API void ClearMeshVertexColor(AActor *MeshOwnerActor);

    NVSCENECAPTURER_API void CalculateSphericalCoordinate(const FVector &TargetLocation, const FVector &SourceLocation, const FVector &ForwardDirection,
                                                          float &OutTargetAzimuthAngle, float &OutTargetAltitudeAngle);

    NVSCENECAPTURER_API FNVCuboidData GetMeshCuboid_AABB(const class UMeshComponent *MeshComp);
    NVSCENECAPTURER_API FNVCuboidData GetMeshCuboid_OOBB_Simple(const class UMeshComponent *MeshComp, bool bInWorldSpace = true, bool bCheckMeshCollision = true);
    NVSCENECAPTURER_API FNVCuboidData GetMeshCuboid_OOBB_Complex(const class UMeshComponent *MeshComp);

    NVSCENECAPTURER_API FNVCuboidData GetActorCuboid_AABB(const AActor *CheckActor);
    NVSCENECAPTURER_API FNVCuboidData GetActorCuboid_OOBB_Simple(const AActor *CheckActor, bool bCheckMeshCollision = true);
    NVSCENECAPTURER_API FNVCuboidData GetActorCuboid_OOBB_Complex(const AActor *CheckActor);
}
