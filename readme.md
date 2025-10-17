# NVIDIA Deep Learning Dataset Synthesizer (NDDS) — **UE5 Modern Port**
### ⚡ Fully updated for Unreal Engine 5.5 (Linux / Windows / macOS)

![NDDS Demo](./NDDSIntro.png)

---

## 🚀 Overview

**NDDS (Deep Learning Dataset Synthesizer)** is a powerful **Unreal Engine plugin** that enables researchers and developers to generate high-quality **synthetic datasets** for computer-vision and robotics applications.  

This updated fork ports the original **NVIDIA NDDS (UE 4.22)** project to **Unreal Engine 5.5**, with full support for:

- ✅ Modern RHI (Vulkan / DX12)  
- ✅ UE5 Shader Pipeline and Modular Build System  
- ✅ Improved C++ 20 compatibility  
- ✅ Rewritten `NVTextureReader` and `SceneCapturer` modules  
- ✅ Native Linux build support (tested on Debian 13 & Ubuntu 22.04)  

NDDS can capture **RGB**, **depth**, **segmentation**, **normals**, **bounding boxes**, and **object poses**, along with **JSON annotations** — ideal for training and evaluating deep-learning models in tasks such as detection, pose estimation, and domain adaptation.

---

## 🧠 Key Features

- 🎥 **High-fidelity synthetic data** generation from Unreal Engine 5 scenes  
- 🧩 Randomized lighting, materials, object poses, textures & camera paths  
- 📦 Support for RGB, depth, normal, instance segmentation, and class masks  
- 🪄 Camera trajectory recording and motion blur simulation  
- 🔁 Async pixel readback pipeline (`NVTextureReader`, UE5 compatible)  
- 💡 Multi-view & multi-sensor capture via Scene Capturer Viewpoints  
- 🧰 JSON & image exporters for Python / PyTorch integration  

---

## 🧩 Prerequisites

| Requirement | Version / Notes |
|--------------|----------------|
| **Unreal Engine** | 5.5 ( built from source ) |
| **.NET SDK** | 8.0 or newer (for Unreal Build Tool) |
| **Vulkan SDK** | 1.3+ (recommended for Linux) |
| **Git LFS** | Required for cloning large assets |
| **C++ Toolchain** | GCC 11+ (Linux) or MSVC 2022 (Windows) |

---

## ⚙️ Building on Linux

### 1️⃣ Clone with Git LFS
```bash
git lfs install
git clone https://github.com/<your-username>/Dataset_Synthesizer.git NDDS
cd NDDS
```

### 2️⃣ Compile Unreal Editor with NDDS plugin
```bash
dotnet /opt/UnrealEngine/Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll \
  Development Linux \
  "-Project=$(pwd)/Source 5.5/NDDS.uproject" \
  -TargetType=Editor -Progress -NoEngineChanges -NoHotReloadFromIDE
```

### 3️⃣ Launch the Editor
```bash
/opt/UnrealEngine/Engine/Binaries/Linux/UnrealEditor \
  "$(pwd)/Source 5.5/NDDS.uproject" -vulkan
```

> 💡 If Vulkan fails, try `-opengl4` or check your GPU drivers with `vulkaninfo`.

---

## 🧱 Plugin Structure

```
Plugins/NVSceneCapturer/
├── Source/NVSceneCapturer/
│   ├── Public/
│   │   ├── NVSceneCapturerUtils.h
│   │   ├── NVTextureReader.h
│   │   ├── NVSceneCapturerViewpointComponent.h
│   └── Private/
│       ├── NVTextureReader.cpp
│       ├── NVSceneCapturerViewpointComponent.cpp
│       ├── NVSceneFeatureExtractor_*.cpp
```

---

## 🧪 Verification (Test Capture)

1. Open the project in Unreal Editor.  
2. Place an `NVSceneCapturerActor` in the scene.  
3. Press **Play → Capture Dataset**.  
4. Output images and JSON annotations will be saved under:

```
Saved/DatasetCaptures/
```

---

## 🧰 Troubleshooting

| Issue | Fix |
|--------|------|
| ❌ `PlatformCreateDynamicRHI()` crash | Missing Vulkan driver → install `vulkan-tools` and `mesa-vulkan-drivers` |
| ⚠️ `AddReferencedObject` deprecated | Fixed in UE5 branch (`AddObjectRef` removed in favor of TObjectPtr) |
| ⚠️ `ShowFlagSettings` deprecated | Replaced with `SetShowFlagSettings()` API |
| 🧩 Build fails with `GetRenderTargetItem()` | Updated RHI API in `NVTextureReader` and `CopyTexture2D()` methods |

---

## 🧠 Citation

If you use this plugin for research or publications, please cite the original authors:

> **Thang To**, **Jonathan Tremblay**, **Duncan McKay**, **Yukie Yamaguchi**, **Kirby Leung**, **Adrian Balanon**, **Jia Cheng**, **William Hodge**, **Stan Birchfield**  
>  
> *NDDS: NVIDIA Deep Learning Dataset Synthesizer.*  
> [https://github.com/NVIDIA/Dataset_Synthesizer](https://github.com/NVIDIA/Dataset_Synthesizer)

---

## 🧩 Acknowledgements

This UE5 modernization was developed by **Fehim Kuş** (@fehimkus) as part of a deep-learning research pipeline for synthetic dataset generation and robotic vision.  
Original NDDS © NVIDIA Corporation (2018).
