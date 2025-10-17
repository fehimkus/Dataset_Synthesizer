/*
* Copyright (c) 2018 NVIDIA Corporation. All rights reserved.
* This work is licensed under a Creative Commons Attribution-NonCommercial-ShareAlike 4.0
* International License.  (https://creativecommons.org/licenses/by-nc-sa/4.0/legalcode)
*/

#include "NVSceneCapturerModule.h"
#include "NVSceneCapturerUtils.h"
#include "NVTextureReader.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ScreenRendering.h"
#include "PipelineStateCache.h"
#include "CommonRenderResources.h"
#include "RenderingThread.h"
#include "RendererInterface.h"
#include "StaticBoundShaderState.h"
#include "Engine/TextureRenderTarget2D.h"

DEFINE_LOG_CATEGORY(LogNVTextureReader);

//======================= FNVTextureReader =======================//
FNVTextureReader::FNVTextureReader()
{
    SourceTexture = nullptr;
}

FNVTextureReader::~FNVTextureReader()
{
    SourceTexture = nullptr;
}

FNVTextureReader &FNVTextureReader::operator=(const FNVTextureReader &OtherReader)
{
    SourceTexture = OtherReader.SourceTexture;
    SourceRect = OtherReader.SourceRect;
    ReadbackPixelFormat = OtherReader.ReadbackPixelFormat;
    ReadbackSize = OtherReader.ReadbackSize;

    return (*this);
}

void FNVTextureReader::SetSourceTexture(FTextureRHIRef NewSourceTexture,
                                        const FIntRect &NewSourceRect /*= FIntRect()*/,
                                        EPixelFormat NewReadbackPixelFormat /*= EPixelFormat::PF_Unknown*/,
                                        const FIntPoint &NewReadbackSize /*= FIntPoint::ZeroValue*/)
{
    // SourceTexture can be null by design.
    // so we don't check NewSourceTexture here.
    SourceTexture = NewSourceTexture;
    SourceRect = NewSourceRect;
    ReadbackPixelFormat = NewReadbackPixelFormat;
    ReadbackSize = NewReadbackSize;

    if (SourceTexture)
    {
        if (ReadbackSize == FIntPoint::ZeroValue)
        {
            ReadbackSize = SourceTexture->GetSizeXY();
        }
        if (ReadbackPixelFormat == EPixelFormat::PF_Unknown)
        {
            ReadbackPixelFormat = SourceTexture->GetFormat();

            if (GDynamicRHI)
            {
                const FString RHIName = GDynamicRHI->GetName();
                // NOTE: UE4's D3D11 implement of the RHI doesn't support all the pixel formats so we must change it to be another format with the same pixel size
                if (RHIName.Contains(TEXT("D3D11")))
                {
                    if ((ReadbackPixelFormat == PF_R16F) || (ReadbackPixelFormat == PF_R16_UINT))
                    {
                        ReadbackPixelFormat = PF_ShadowDepth;
                    }
                }
                // TODO: Should we ignore non-supported pixel format?
                // NOTE: Since we read back the pixel in bytes, we need to change the format to be uint mode instead of float
                if (ReadbackPixelFormat == PF_R32_FLOAT)
                {
                    ReadbackPixelFormat = PF_R32_UINT;
                }
            }
        }
        if (SourceRect.IsEmpty())
        {
            SourceRect = FIntRect(FIntPoint::ZeroValue, SourceTexture->GetSizeXY());
        }
    }
}

// Read back the pixels data from the current source texture
// NOTE: This function is sync, the pixels data is returned right away but it may cause the game to hitches since it flush the rendering commands
bool FNVTextureReader::ReadPixelsData(FNVTexturePixelData &OutPixelsData)
{
    bool bResult = false;
    if (SourceTexture)
    {
        ENQUEUE_RENDER_COMMAND(ReadPixelsFromTexture)(
            [this, &OutPixelsData](FRHICommandListImmediate &RHICmdList)
            {
                // --- Create a CPU-readable staging texture ---
                FRHITextureCreateDesc Desc =
                    FRHITextureCreateDesc::Create2D(TEXT("NVTextureReader_Readback"))
                        .SetExtent(SourceTexture->GetSizeXY().X, SourceTexture->GetSizeXY().Y)
                        .SetFormat(SourceTexture->GetFormat())
                        .SetNumMips(1)
                        .SetNumSamples(1)
                        .SetFlags(ETextureCreateFlags::CPUReadback);

                FTextureRHIRef ReadbackTexture = RHICreateTexture(Desc);

                // --- Copy from GPU texture → staging texture ---
                FRHICopyTextureInfo CopyInfo;
                CopyInfo.Size = FIntVector(
                    SourceTexture->GetSizeXY().X,
                    SourceTexture->GetSizeXY().Y,
                    1);

                RHICmdList.CopyTexture(SourceTexture, ReadbackTexture, CopyInfo);

                // --- Map the staging texture to CPU memory ---
                void *PixelDataBuffer = nullptr;
                FIntPoint PixelSize = FIntPoint::ZeroValue;
                RHICmdList.MapStagingSurface(ReadbackTexture, PixelDataBuffer, PixelSize.X, PixelSize.Y);

                if (PixelDataBuffer)
                {
                    BuildPixelData(OutPixelsData,
                                   static_cast<uint8 *>(PixelDataBuffer),
                                   ReadbackPixelFormat,
                                   PixelSize,
                                   ReadbackSize);
                }

                RHICmdList.UnmapStagingSurface(ReadbackTexture);
            });

        FlushRenderingCommands();
        bResult = true;
    }
    return bResult;
}

// Read back the pixels data from the current source texture
// NOTE: This function is sync, the pixels data is returned right away but it may cause the game to hitches since it flush the rendering commands
bool FNVTextureReader::ReadPixelsData(
    FNVTexturePixelData &OutPixelsData,
    const FTextureRHIRef &NewSourceTexture,
    const FIntRect &NewSourceRect /* = FIntRect()*/,
    EPixelFormat NewReadbackPixelFormat /*= EPixelFormat::PF_Unknown*/,
    const FIntPoint &NewReadbackSize /*= FIntPoint::ZeroValue*/)
{
    SetSourceTexture(NewSourceTexture, NewSourceRect, NewReadbackPixelFormat, NewReadbackSize);
    return ReadPixelsData(OutPixelsData);
}

bool FNVTextureReader::ReadPixelsData(OnFinishedReadingPixelsDataCallback Callback, bool bIgnoreAlpha /*= false*/)
{
    bool bResult = false;

    ensure(Callback);
    if (!Callback)
    {
        UE_LOG(LogNVTextureReader, Error, TEXT("invalid argument."));
    }
    else
    {
        if (SourceTexture)
        {
            bResult = ReadPixelsRaw(SourceTexture,
                                    SourceRect, ReadbackPixelFormat, ReadbackSize, bIgnoreAlpha,
                                    [this, Callback](uint8 *PixelData, EPixelFormat PixelFormat, FIntPoint PixelSize)
                                    {
                                        Callback(BuildPixelData(PixelData, PixelFormat, PixelSize, ReadbackSize));
                                    });
        }
    }
    return bResult;
}

bool FNVTextureReader::ReadPixelsData(OnFinishedReadingPixelsDataCallback Callback,
                                      const FTextureRHIRef &NewSourceTexture,
                                      const FIntRect &NewSourceRect /*= FIntRect()*/,
                                      EPixelFormat NewReadbackPixelFormat /*= EPixelFormat::PF_Unknown*/,
                                      const FIntPoint &NewReadbackSize /*= FIntPoint::ZeroValue*/,
                                      bool bIgnoreAlpha /*= false*/)
{
    ensure(Callback);
    SetSourceTexture(NewSourceTexture, NewSourceRect, NewReadbackPixelFormat, NewReadbackSize);
    return ReadPixelsData(Callback, bIgnoreAlpha);
}

bool FNVTextureReader::ReadPixelsRaw(const FTextureRHIRef &NewSourceTexture, const FIntRect &SourceRect,
                                     EPixelFormat TargetPixelFormat, const FIntPoint &TargetSize, bool bIgnoreAlpha, OnFinishedReadingRawPixelsCallback Callback)
{
    bool bResult = false;

    // Make sure the source render target and area settings are valid
    ensure(NewSourceTexture);
    ensure(SourceRect.Area() != 0);
    ensure(TargetSize != FIntPoint::ZeroValue);
    ensure(Callback);
    if (!NewSourceTexture || (SourceRect.Area() == 0) || (TargetSize == FIntPoint::ZeroValue) || (!Callback))
    {
        UE_LOG(LogNVTextureReader, Error, TEXT("invalid argument."));
    }
    else
    {
        static const FName RendererModuleName("Renderer");
        // Load the renderer module on the main thread, as the module manager is not thread-safe, and copy the ptr into the render command, along with 'this' (which is protected by BlockUntilAvailable in ~FViewportSurfaceReader())
        IRendererModule *RendererModule = &FModuleManager::GetModuleChecked<IRendererModule>(RendererModuleName);
        check(RendererModule);

        // NOTE: This approach almost identical to function FViewportSurfaceReader::ResolveRenderTarget in FrameGrabber.cpp
        // The main different is we reading the pixels back from a render target instead of a viewport
        ENQUEUE_RENDER_COMMAND(ReadPixelsFromTexture)(
            [=](FRHICommandListImmediate &RHICmdList)
            {
                // FRHIResourceCreateInfo CreateInfo(FClearValueBinding::None);
                // TODO: Can cache the ReadbackTexture and reuse it if the format and size doesn't change instead of creating it everytime we read like this
                // Need to be careful with reusing the ReadbackTexture: need to make sure the texture is already finished reading
                FRHITextureCreateDesc Desc =
                    FRHITextureCreateDesc::Create2D(TEXT("NVTextureReadback"))
                        .SetExtent(TargetSize.X, TargetSize.Y)
                        .SetFormat(TargetPixelFormat)
                        .SetNumMips(1)
                        .SetNumSamples(1)
                        .SetFlags(ETextureCreateFlags::CPUReadback);

                FTextureRHIRef ReadbackTexture = RHICreateTexture(Desc);

                bool bOverwriteAlpha = !bIgnoreAlpha;
                // Copy the source texture to the readback texture so we can read it back later even after the source texture is modified
                CopyTexture2d(RendererModule, RHICmdList, NewSourceTexture, SourceRect, ReadbackTexture, FIntRect(FIntPoint::ZeroValue, TargetSize), bOverwriteAlpha);

                // Stage the texture to read back its pixels data
                FIntPoint PixelSize = FIntPoint::ZeroValue;
                void *PixelDataBuffer = nullptr;
                RHICmdList.MapStagingSurface(ReadbackTexture, PixelDataBuffer, PixelSize.X, PixelSize.Y);

                if (PixelDataBuffer)
                {
                    Callback((uint8 *)PixelDataBuffer, TargetPixelFormat, PixelSize);
                }

                RHICmdList.UnmapStagingSurface(ReadbackTexture);

                ReadbackTexture.SafeRelease();
            });
        bResult = true;
    }

    return bResult;
}

void FNVTextureReader::CopyTexture2d(IRendererModule * /*RendererModule*/, FRHICommandListImmediate &RHICmdList,
                                     const FTextureRHIRef &NewSourceTexture, const FIntRect &SourceRect,
                                     FTextureRHIRef &TargetTexture, const FIntRect &TargetRect, bool /*bOverwriteAlpha*/)
{
    if (!NewSourceTexture || !TargetTexture)
    {
        UE_LOG(LogNVTextureReader, Error, TEXT("Invalid texture in CopyTexture2d"));
        return;
    }

    FRHICopyTextureInfo CopyInfo;
    CopyInfo.SourcePosition = FIntVector(SourceRect.Min.X, SourceRect.Min.Y, 0);
    CopyInfo.DestPosition = FIntVector(TargetRect.Min.X, TargetRect.Min.Y, 0);
    CopyInfo.Size = FIntVector(TargetRect.Width(), TargetRect.Height(), 1);

    RHICmdList.CopyTexture(NewSourceTexture, TargetTexture, CopyInfo);
}

FNVTexturePixelData FNVTextureReader::BuildPixelData(uint8 *PixelsData, EPixelFormat PixelFormat, const FIntPoint &ImageSize, const FIntPoint &TargetSize)
{
    FNVTexturePixelData NewPixelData;
    BuildPixelData(NewPixelData, PixelsData, PixelFormat, ImageSize, TargetSize);

    return NewPixelData;
}

void FNVTextureReader::BuildPixelData(FNVTexturePixelData &OutPixelsData, uint8 *RawPixelsData, EPixelFormat PixelFormat, const FIntPoint &ImageSize, const FIntPoint &TargetSize)
{
    ensure(RawPixelsData);
    if (!RawPixelsData)
    {
        UE_LOG(LogNVTextureReader, Error, TEXT("invalid argument."));
    }
    else
    {
        const uint32 PixelCount = TargetSize.X * TargetSize.Y;

        OutPixelsData.PixelSize = TargetSize;
        OutPixelsData.PixelFormat = PixelFormat;

        // NOTE: We don't support pixel format that use less than 1 byte for now, e.g: grayscale 1, 2 or 4 bit
        const uint8 PixelByteSize = NVSceneCapturerUtils::GetPixelByteSize(PixelFormat);
        const uint32 PixelBufferSize = PixelByteSize * PixelCount;
        OutPixelsData.PixelData.InsertUninitialized(0, PixelBufferSize);

        uint8 *SrcPixelBuffer = RawPixelsData;
        uint8 *Dest = &OutPixelsData.PixelData[0];
        // NOTE: The 2d size of the read back pixel buffer (PixelSize) may be different than the target size that we want (ReadbackSize)
        // So we must make sure to only copy the minimum part of it
        const int32 MinWidth = FMath::Min(TargetSize.X, ImageSize.X);
        const int32 MinHeight = FMath::Min(TargetSize.Y, ImageSize.Y);
        const int32 TargetWidthByteSize = MinWidth * PixelByteSize;
        const int32 SourceWidthByteSize = ImageSize.X * PixelByteSize;
        OutPixelsData.RowStride = TargetWidthByteSize;
        for (int32 Row = 0; Row < MinHeight; ++Row)
        {
            FMemory::Memcpy(Dest, SrcPixelBuffer, TargetWidthByteSize);

            SrcPixelBuffer += SourceWidthByteSize;
            Dest += TargetWidthByteSize;
        }
    }
}

//======================= FNVTextureRenderTargetReader =======================//
FNVTextureRenderTargetReader::FNVTextureRenderTargetReader(UTextureRenderTarget2D *InRenderTarget) : FNVTextureReader()
{
    SourceRenderTarget = InRenderTarget;
}

FNVTextureRenderTargetReader::~FNVTextureRenderTargetReader()
{
    SourceRenderTarget = nullptr;
}

void FNVTextureRenderTargetReader::SetTextureRenderTarget(UTextureRenderTarget2D *NewRenderTarget)
{
    if (NewRenderTarget != SourceRenderTarget)
    {
        SourceRenderTarget = NewRenderTarget;
        UpdateTextureFromRenderTarget();
    }
}

bool FNVTextureRenderTargetReader::ReadPixelsData(OnFinishedReadingPixelsDataCallback Callback, bool bIgnoreAlpha /*= false*/)
{
    UpdateTextureFromRenderTarget();
    return FNVTextureReader::ReadPixelsData(Callback, bIgnoreAlpha);
}

bool FNVTextureRenderTargetReader::ReadPixelsData(FNVTexturePixelData &OutPixelsData)
{
    ENQUEUE_RENDER_COMMAND(ReadPixelsFromTexture)(
        [this, &OutPixelsData = OutPixelsData](FRHICommandListImmediate &RHICmdList)
        {
            const FTextureRenderTargetResource *RenderTargetResource = SourceRenderTarget->GetRenderTargetResource();
            ensure(RenderTargetResource);

            if (RenderTargetResource)
            {
                FTextureRHIRef RenderTexture = RenderTargetResource->GetRenderTargetTexture();

                // --- create a CPU-readback texture ---
                FRHITextureCreateDesc Desc =
                    FRHITextureCreateDesc::Create2D(TEXT("RenderTargetReadback"))
                        .SetExtent(RenderTexture->GetSizeXY().X, RenderTexture->GetSizeXY().Y)
                        .SetFormat(RenderTexture->GetFormat())
                        .SetNumMips(1)
                        .SetFlags(ETextureCreateFlags::CPUReadback);

                FTextureRHIRef ReadbackTexture = RHICreateTexture(Desc);

                // --- copy GPU → CPU staging texture ---
                FRHICopyTextureInfo CopyInfo;
                CopyInfo.Size = FIntVector(ReadbackTexture->GetSizeXY().X, ReadbackTexture->GetSizeXY().Y, 1);
                RHICmdList.CopyTexture(RenderTexture, ReadbackTexture, CopyInfo);

                // --- map the staging surface ---
                void *PixelDataBuffer = nullptr;
                FIntPoint PixelSize = FIntPoint::ZeroValue;
                RHICmdList.MapStagingSurface(ReadbackTexture, PixelDataBuffer, PixelSize.X, PixelSize.Y);

                if (PixelDataBuffer)
                {
                    BuildPixelData(OutPixelsData, static_cast<uint8 *>(PixelDataBuffer),
                                   ReadbackPixelFormat, PixelSize, ReadbackSize);
                }

                RHICmdList.UnmapStagingSurface(ReadbackTexture);
            }
        });

    FlushRenderingCommands();

    return true;
}

void FNVTextureRenderTargetReader::UpdateTextureFromRenderTarget()
{
    // Update the texture reference from the render target
    // NOTE: Should check if the texture still belong to the render target before updating its reference
    if (SourceRenderTarget)
    {
        // NOTE: Because function GameThread_GetRenderTargetResource is not marked as const, we can't have SourceRenderTarget as const either
        const FTextureRenderTargetResource *RenderTargetResource = IsInGameThread() ? SourceRenderTarget->GameThread_GetRenderTargetResource()
                                                                                    : SourceRenderTarget->GetRenderTargetResource();
        if (RenderTargetResource)
        {
            SetSourceTexture(RenderTargetResource->GetRenderTargetTexture());
        }
    }
    else
    {
        SetSourceTexture(nullptr);
    }
}