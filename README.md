# ![The Forge Logo](Screenshots/TheForge-on-white.jpg)

The Forge is a cross-platform rendering framework supporting
- PC 
  * Windows 10 
     * with DirectX 12 / Vulkan 1.1
     * with DirectX Ray Tracing API
     * DirectX 11 Fallback Layer for Windows 7 support (not extensively tested)
  * Linux Ubuntu 18.04 LTS with Vulkan 1.1
- Android Pie with Vulkan 1.1
- macOS with Metal 2
- iOS with Metal 2
- XBOX One / XBOX One X (only available for accredited developers on request)
- PS4 (in development) (only available for accredited developers on request)

Particularly, the graphics layer of The Forge supports cross-platform
- Descriptor management
- Multi-threaded resource loading
- Shader reflection
- Multi-threaded command buffer generation

The Forge can be used to provide the rendering layer for custom next-gen game engines. It is also meant to provide building blocks to write your own game engine. It is like a "lego" set that allows you to use pieces to build a game engine quickly. The "lego" High-Level Features supported on all platforms are at the moment:
- Lua Scripting System - currently used in 06_Playground to load models and textures and animate the camera
- Animation System based on [Ozz Animation System](https://github.com/guillaumeblanc/ozz-animation)
- Consistent Math Library  based on an extended version of [Vectormath](https://github.com/glampert/vectormath)
- Extended version of [TinySTL](https://github.com/mendsley/tinystl)
- For loading art assets we have a modified and integrated version of [Assimp](https://github.com/assimp/assimp)
- Consistent Memory Managament
- Input system with Gestures for Touch devices based on an extended version of [gainput](https://github.com/jkuhlmann/gainput)
- Very fast Entity Component System based on [ENTT](https://github.com/skypjack/entt)
- UI system based on [imGui](https://github.com/ocornut/imgui) with a dedicated unit test extended for touch input devices
- Various implementations of high-end Graphics Effects as shown in the unit tests below
Please find a link and credits for all open-source packages used at the end of this readme.


Future plans are
- Unified shader generation -> check out an alpha version of the [Confetti Shader Translator](http://confettishadertranslator.azurewebsites.net). This shader translator is build with the purpose of supporting a higher-level shading language, which we call super HLSL or short sHLSL


<a href="https://twitter.com/TheForge_FX?lang=en" target="_blank"><img src="Screenshots/twitter.png" 
alt="Twitter" width="20" height="20" border="0" /> Join the channel at https://twitter.com/TheForge_FX?lang=en</a>
 
# Build Status 

* Windows [![Build status](https://ci.appveyor.com/api/projects/status/leqbpaqtqj549yhh/branch/master?svg=true)](https://ci.appveyor.com/project/wolfgangfengel/the-forge/branch/master)
* macOS [![Build Status](https://travis-ci.org/ConfettiFX/The-Forge.svg?branch=master)](https://travis-ci.org/ConfettiFX/The-Forge)

# News

## Release 1.22 - January 22nd, 2019 - TressFX Hair | Entity Component System | Lua Scripting System
This is the first release in 2019 and The Forge GitHub repository is today exactly one year old, with the first release on the same day last year :-) In 2018 we made 22 releases and we improved The Forge in many areas.
If you followed us so far ... why not just join us? We are looking for more graphics programmer joining our teams. We have offices in Encinitas, CA USA (Main) / Mumbai, India / Shanghai, China / Breda, Netherlands / St. Petersburg, Russia and in Lviv, Ukraine.

Now back to release notes: many years ago in 2012 / 2013, we helped AMD and Crystal Dynamics with the development of TressFX for [Tomb Raider](https://gfxspeak.com/2013/05/21/amd-and-confetti-collaborate-on-hair/). We also wrote an article about the implementation in [GPU Pro 5](http://gpupro.blogspot.com/2013/10/gpu-pro-5-table-of-contents.html ) and gave a few joint presentations on conferences like [FMX](https://www.slideshare.net/WolfgangEngel/hair-intombraider-final). At the end of last year we revisited TressFX. We took the current code in the [GitHub repository](https://github.com/GPUOpen-Effects/TressFX), changed it a bit and ported it to The Forge. It now runs on PC with DirectX 12 / Vulkan, Linux with Vulkan, macOS and iOS with Metal 2 and on the XBOX One. We also created a few new hair assets so that we can showcase it. Here are screenshots of our programmers art:

PC Windows DirectX 12 GTX 950 Driver 416.81:

![Hair on PC](Screenshots/MaterialPlayground/06_MaterialPlayground_Hair_closup.gif)

iPad (Model A1803):

![Hair on iOS](Screenshots/MaterialPlayground/06_MaterialPlayground_hair_iOS_ponytail.png)


The current implementation is overall too slow to be used on some of our platforms but we are going to improve performance over time. We are also going to improve on the hair art assets.
Apart from Hair, the Material Playground now also has improved versions of Metal and a new material category Wood:

Metal:

![Material Playground Metal on PC](Screenshots/MaterialPlayground/06_MaterialPlayground_Metal.png)

Wood:

![Material Playground Wood on PC](Screenshots/MaterialPlayground/06_MaterialPlayground_Wood.png)

- Entity component system (ECS) for all platforms: we've chosen to integrate ENTT (https://github.com/skypjack/entt) into The Forge. The initial implementation was contributed by Amer Koleci @AmerKoleci:

![Image of the Entity Component System unit test in The Forge](Screenshots/17_EntityComponentSystem.png)

- Lua Scripting System for all platforms: the Lua scripting integration allows to register functions to lua contexts so these functions will be available via scripts. Functions can be static or lambda. In latter case you can store some state information within lambda. Scripts are executed using LuaManager::RunScript() or LuaManager::AddAsyncScript() methods. In second case scripts are still executed in synchronous way. Async execution implementation is planned. Also there is "updateable" script. It is loaded (at this point script main body is executed), then it can be "updated" - update() function from script is invoked. Script can invoke any registered function and can store state information which will be available from update to update. The script can be reloaded - can be useful for fast iterations when you just modify script and don't even need to restart application. If that script contains exit() function then it will be executed when script is reloaded or closed. Checkout unit test 06_MaterialPlayground for an example on how to use it. In this unit tests it executes three scripts to load models and textures and animate the camera.



## Release 1.21 - December 1st, 2018 - Season Greetings with new Skinning Unit Test | Unified Vulkan Shaders
The team will soon go into winter hybernation mode ... which means many Confetti people will fly home over the holiday season to spend time with their loved ones. We will be back with more releases next year, probably in February. 

To send you season greetings, we extended our Ozz implementation by adding a new Skinning unit test:

PC Windows 10 DirectX 12 GeForce 950 Driver 411.63 with a resolution of 1080p
![PC Windows 10 skinning unit test](Screenshots/Skinning_PC.gif)

Linux Ubuntu 18.04.1 LTS Vulkan 1.1.92 RADEON 480 Driver 18.30 with a resolution of 1920x1080
![Ubuntu skinning unit test](Screenshots/Skinning_Ubuntu.png)

iMac with AMD RADEON 580 (Part No. MNED2xx/A) with resolution of 1920x1080
![macOS skinning unit test](Screenshots/Skinning_macOS.png)

iPhone 7 iOS 12.0.1 (16A404) with a resolution of 1334x750
![iOS skinning unit test](Screenshots/Skinning_iOS.png)

XBOX One
![XBOX One skinning unit test](Screenshots/Skinning_XBOX.png)

- Vulkan: 
  - all three Vulkan platforms (Windows, Linux, Android) use now the same Vulkan shaders
  - Upgraded Linux and Windows SDK to 1.1.92.1
- The math library now supports more integer data types
- Updated assimp to use latest master + added projects instead of shipping binaries
- macOS / iOS
  - Added support for iOS Gestures (Not currently in use in the unit-tests)
  - Improved pixel projected reflections on Metal Platforms
  - Upgraded all the XCode projects to target Xcode 10.1 (10B61) and  iOS Version 12.0.1 (16A404) 
  - Started Testing additionally on A12 Devices Phone Xs Max (Model MT5D2LL/A)
- Numerous shader translator updates. Head over to [Confetti Shader Translator](http://confettishadertranslator.azurewebsites.net) check them out :-) It is getting more and more stable.

See the release notes from previous releases in the [Release section](https://github.com/ConfettiFX/The-Forge/releases).

  
# PC Windows Requirements:

1. Windows 10 with latest update

2. Drivers
* AMD / NVIDIA - latest drivers should work. On Vulkan, at least NVIDIA Beta Driver 389.20 are required to support Linked Multi-GPU. 
* Intel - need to install the latest driver (currently Version: 25.20.100.6326, October 9th, 2018) [Intel® Graphics Driver for Windows® 10](https://downloadcenter.intel.com/download/28240/Intel-Graphics-Driver-for-Windows-10?product=80939). As mentioned before this driver still doesn't have full DirectX 12 and Vulkan support.


3. Visual Studio 2017 with Windows SDK / DirectX version 16299.91 (Fall Creators Update)
https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

4. Vulkan [1.1.92.1](https://vulkan.lunarg.com/sdk/home)


5. Ray Tracing 
 * DirectX Raytracing Experimental SDK v0.09.01
 * Windows 10 RS4 builds more info at [DXR](http://aka.ms/DXR)

6. The Forge is currently tested on 
* AMD 5x, VEGA GPUs (various)
* NVIDIA GeForce 9x, 10x GPUs (various)
* Intel Skull Canyon


# macOS Requirements:

1. macOS: 10.14 (18A389)

2. XCode: 10.1 (10B61)

3. The Forge is currently tested on the following macOS devices:
* iMac with AMD RADEON 560 (Part No. MNDY2xx/A)
* iMac with AMD RADEON 580 (Part No. MNED2xx/A)
* MacBook Pro 13 inch (MacBookPro13,2) 
* Macbook Pro 13 inch (MacbookPro14,2)

In the moment we do not have access to an iMac Pro or Mac Pro. We can test those either with Team Viewer access or by getting them into the office and integrating them into our build system.
We will not test any Hackintosh configuration. 


# iOS Requirements:

1. iOS: 12.0.1 (16A404)

2. XCode: see macOS

To run the unit tests, The Forge requires an iOS device with an A9 or higher CPU (see [GPU Processors](https://developer.apple.com/library/content/documentation/DeviceInformation/Reference/iOSDeviceCompatibility/HardwareGPUInformation/HardwareGPUInformation.html) or see iOS_Family in this table [iOS_GPUFamily3_v3](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)). This is required to support the hardware tessellation unit test and the ExecuteIndirect unit test (requires indirect buffer support). The Visibility Buffer doesn't run on current iOS devices because the [texture argument buffer](https://developer.apple.com/documentation/metal/fundamental_components/gpu_resources/understanding_argument_buffers) on those devices is limited to 31 (see [Metal Feature Set Table](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf) and look for the entry "Maximum number of entries in the texture argument table, per graphics or compute function") , while on macOS it is 128, which we need for the bindless texture array. 

We are currently testing on 
* iPhone 7 (Model A1778)
* iPad (Model A1803)
* iPhone Xs Max (Model MT5D2LL/A)


# PC Linux Requirements:

1. [Ubuntu 18.04 LTS](https://www.ubuntu.com/download/desktop) Kernel Version: 4.15.0-20-generic

2. GPU Drivers:
* [AMDGpu-Pro 18.30-641594](https://www.amd.com/en/support/graphics/radeon-500-series/radeon-rx-500-series/radeon-rx-580)
* [NVIDIA Linux x86_64/AMD64/EM64T 390.87](http://www.nvidia.com/object/unix.html) You can update using the command line too https://tecadmin.net/install-latest-nvidia-drivers-ubuntu/

3. Workspace file is provided for [codelite 12.0.6](https://codelite.org/)

4. Vulkan SDK Version: download the native Ubuntu Linux package for all the elements of the Vulkan SDK [LunarG Vulkan SDK Packages for Ubuntu 16.04 and 18.04](https://packages.lunarg.com/)

5. The Forge is currently tested on Ubuntu with the following GPUs:
 * AMD RADEON RX 480
 * AMD RADEON VEGA 56
 * NVIDIA GeForce GTX 950

Make sure VulkanSDK environment variables are configured correctly.
Please read the "Set up the Runtime Environment" and "Environment Variable Persistence" [https://vulkan.lunarg.com/doc/sdk/1.1.70.1/linux/getting_started.html](https://vulkan.lunarg.com/doc/sdk/1.1.70.1/linux/getting_started.html)


# Android Requirements:

1. Android Phone with Android Pie (9.x) for Vulkan 1.1 support

2. Android Studio with API level 28 and follow the instructions

3. We are currently testing on 
* [Essential Phone](https://en.wikipedia.org/wiki/Essential_Phone) with Android 9.0 - Build PPR1.181005.034

In the moment we only support the first two unit tests. We are waiting for devkits with more stable drivers before we bring over the other unit tests. The Essential phone uses an Adreno 540 GPU. Please check out [Vulkan Gpuinfo.org](http://vulkan.gpuinfo.org/) for the supported feature list of this GPU with Android 9.0.


# Install 
 * For PC Windows run PRE_BUILD.bat. It will download and unzip the art assets and install the shader builder extension for Visual Studio 2017.
 * For Linux and Mac run PRE_BUILD.command. If its the first time checking out the forge make sure the PRE_BUILD.command has the correct executable flag by running the following command
  chmod +x PRE_BUILD.command
  
    It will only download and unzip required Art Assets (No plugins/extensions install). 

# Unit Tests
There are the following unit tests in The Forge:

## 1. Transformation

This unit test just shows a simple solar system. It is our "3D game Hello World" setup for cross-platform rendering.

![Image of the Transformations Unit test](Screenshots/01_Transformations.PNG)

## 2. Compute

This unit test shows a Julia 4D fractal running in a compute shader. In the future this test will use several compute queues at once.

![Image of the Compute Shader Unit test](Screenshots/02_Compute.PNG)

## 3. Multi-Threaded Rendering

This unit test shows how to generate a large number of command buffers on all platforms supported by The Forge. This unit test is based on [a demo by Intel called Stardust](https://software.intel.com/en-us/articles/using-vulkan-graphics-api-to-render-a-cloud-of-animated-particles-in-stardust-application).

![Image of the Multi-Threaded command buffer generation example](Screenshots/03_MultiThreading.PNG)

## 4. ExecuteIndirect

This unit test shows the difference in speed between Instanced Rendering, using ExecuteIndirect with CPU update of the indirect argument buffers and using ExecuteIndirect with GPU update of the indirect argument buffers.
This unit test is based on [the Asteroids example by Intel](https://software.intel.com/en-us/articles/asteroids-and-directx-12-performance-and-power-savings).

![Image of the ExecuteIndirect Unit test](Screenshots/04_ExecuteIndirect.PNG)
Using ExecuteIndirect with GPU updates for the indirect argument buffers

![Image of the ExecuteIndirect Unit test](Screenshots/04_ExecuteIndirect_2.PNG)
Using ExecuteIndirect with CPU updates for the indirect argument buffers

![Image of the ExecuteIndirect Unit test](Screenshots/04_ExecuteIndirect_3.PNG)
Using Instanced Rendering

## 5. Font Rendering

This unit test shows the current state of our font rendering library that is based on several open-source libraries.

![Image of the Font Rendering Unit test](Screenshots/05_FontRendering.PNG)

## 6. Material Playground

This unit test shows a range of game related materials:

Hair:

![Hair on PC](Screenshots/MaterialPlayground/06_MaterialPlayground_Hair_closup.gif)

Metal:

![Material Playground Metal on PC](Screenshots/MaterialPlayground/06_MaterialPlayground_Metal.png)

Wood:

![Material Playground Wood on PC](Screenshots/MaterialPlayground/06_MaterialPlayground_Wood.png)

## 7. Hardware Tessellation

This unit test showcases the rendering of grass with the help of hardware tessellation.

![Image of the Hardware Tessellation Unit test](Screenshots/07_Hardware_Tessellation.PNG)

## 8. Procedural 
In the spirit of the shadertoy examples this unit test shows a procedurally generated planet.

![Image of the Procedural Unit test](Screenshots/08_Procedural.PNG)

## 9. Light and Shadow Playground
This unit test shows various shadow and lighting techniques that can be chosen from a drop down menu. There will be more in the future.

![Image of the Light and Shadow Unit test](Screenshots/09_LightShadowPlayground.png)

## 9a. Hybrid Ray-Traced Shadows
This unit test was build by Kostas Anagnostou @KostasAAA to show how to ray trace shadows without using a ray tracing API like DXR / RTX. It should run on all GPUs (not just NVIDIA RTX GPUs) and the expectation is that it should run comparable with a DXR / RTX based version even on a NVIDIA RTX GPU. That means the users of your game do not have to buy a NVIDIA RTX GPU to enjoy HRT shadows :-)
![Hybrid Ray Traced Shadows](Screenshots/09a_HRT_Shadows.png)


## 10. Pixel-Projected Reflections
This unit test shows reflections that are ray traced. It is an implementation of the papers [Optimized pixel-projected reflections for planar reflectors](http://advances.realtimerendering.com/s2017/PixelProjectedReflectionsAC_v_1.92.pdf) and [IMPLEMENTATION OF OPTIMIZED PIXEL-PROJECTED REFLECTIONS FOR PLANAR REFLECTORS](https://github.com/byumjin/Jin-Engine-2.1/blob/master/%5BByumjin%20Kim%5D%20Master%20Thesis_Final.pdf)

![Image of the Pixel-Projected Reflections Unit test](Screenshots/10_Pixel-ProjectedReflections.png)

## 11. Multi-GPU (Driver support only on PC Windows)
This unit test shows a typical VR Multi-GPU configuration. One eye is rendered by one GPU and the other eye by the other one.

![Image of the Multi-GPU Unit test](Screenshots/11_MultiGPU.png)

## 12. The Forge switching between Vulkan and DirectX 12 during Run-time (Windows PC-only)
This unit test shows how to switch between the Vulkan and DirectX 12 graphics API during run-time. 

![Image of the The Forge Switching Unit test](Screenshots/12_TheForgeInDLL.png)

## 13. imGUI integration unit test
This unit test shows how the integration of imGui with a wide range of functionality.

![Image of the imGui Integration in The Forge](Screenshots/13_imGui.gif)


## 14. Order-Independent Transparency unit test
This unit test compares various Order-Indpendent Transparency Methods.

![Image of the Order-Indpendent Transparency unit test in The Forge](Screenshots/14_OIT.png)


## 15. Wave Intrinsics unit test
This unit test shows how to use the new wave intrinsics. In the moment it only supports Windows (DirectX 12 / Vulkan 1.1) and Linux with Vulkan 1.1. More platforms will be added.

![Image of the Wave Intrinsics unit test in The Forge](Screenshots/15_WaveIntrinsics.png)

## 16. Ray Tracing Unit Test for DXR
Ray Tracing API unit test, showing how to use DXR on Windows only.

![Image of the DXR Ray Tracing unit test in The Forge](Screenshots/16_RayTracing.png)

## 16a. Sphere Tracing
This unit test was originally posted on ShaderToy by Inigo Quilez (https://www.shadertoy.com/view/Xds3zN and https://sopyer.github.io/b/post/vulkan-shader-sample/). It shows how a scene is ray marched with shadows, reflections and AO

![Image of the Sphere Tracing  unit test in The Forge](Screenshots/16_RayMarching_Linux.png)

## 17. ENTT - Entity Component System Test
This unit test shows how to use a high-performance entity component system in The Forge.

![Image of the Entity Component System unit test in The Forge](Screenshots/17_EntityComponentSystem.png)


## 18. Ozz Playback Animation
This unit test shows how to playback a clip on a rig.

![Image of Playback Animation in The Forge](Screenshots/01_Playback.gif)

## 19. Ozz Playback Blending
This unit test shows how to blend multiple clips and play them back on a rig.

![Image of Playback Blending in The Forge](Screenshots/02_Blending.gif)

## 20. Ozz Joint Attachment
This unit test shows how to attach an object to a rig which is being posed by an animation.

![Image of Ozz Joint Attachment in The Forge](Screenshots/03_JointAttachment.gif)

## 21. Ozz Partial Blending
This unit test shows how to blend clips having each only effect a certain portion of joints.

![Image of Ozz Partial Blending in The Forge](Screenshots/04_PartialBlending.gif)

## 22. Ozz Additive Blending
This unit test shows how to introduce an additive clip onto another clip and play the result on a rig.

![Image of Ozz Additive Blending in The Forge](Screenshots/05_Additive.gif)

## 23. Ozz Baked Physics
This unit test shows how to use a scene of a physics interaction that has been baked into an animation and play it back on a rig.

![Image of Ozz Baked Physics in The Forge](Screenshots/07_BakedPhysics.gif)

## 24. Ozz Multi Threading
This unit test shows how to animate multiple rigs simultaneously while using multi-threading for the animation updates.

![Image of Ozz Multi Threading in The Forge](Screenshots/09_MultiThread.gif)

## 25. Ozz Skinning
This unit test shows how to use skinning with Ozz

![Image of the Ozz Skinning unit test](Screenshots/Skinning_PC.gif)




# Examples
There is an example implementation of the Triangle Visibility Buffer as covered in various conference talks. [Here](https://diaryofagraphicsprogrammer.blogspot.com/2018/03/triangle-visibility-buffer.html) is a blog entry that details the implementation in The Forge.

![Image of the Visibility Buffer](Screenshots/Visibility_Buffer.png)


# Releases / Maintenance
Confetti will prepare releases when all the platforms are stable and running and push them to this GitHub repository. Up until a release, development will happen on internal servers. This is to sync up the console, mobile, macOS and PC versions of the source code.

# Products
We would appreciate it if you could send us a link in case your product uses The Forge. Here are the ones we received so far:

## StarVR One SDK
The Forge is used to build the StarVR One SDK:

<a href="https://www.starvr.com" target="_blank"><img src="Screenshots/StarVR.PNG" 
alt="StarVR" width="300" height="159" border="0" /></a>

## Torque 3D
The Forge is used as the rendering framework in Torque 3D:

<a href="http://www.garagegames.com/products/torque-3d" target="_blank"><img src="Screenshots/Torque-Logo_H.png" 
alt="Torque 3D" width="417" height="106" border="0" /></a>


# Open-Source Libraries
The Forge utilizes the following Open-Source libraries:
* [Assimp](https://github.com/assimp/assimp)
* [Bullet Physics](https://github.com/bulletphysics)
* [Fontstash](https://github.com/memononen/fontstash)
* [Vectormath](https://github.com/glampert/vectormath)
* [Nothings](https://github.com/nothings/stb) single file libs 
  * [stb.h](https://github.com/nothings/stb/blob/master/stb.h)
  * [stb_image.h](https://github.com/nothings/stb/blob/master/stb_image.h)
  * [stb_image_resize.h](https://github.com/nothings/stb/blob/master/stb_image_resize.h)
  * [stb_image_write.h](https://github.com/nothings/stb/blob/master/stb_image_write.h)
* [shaderc](https://github.com/google/shaderc)
* [SPIRV_Cross](https://github.com/KhronosGroup/SPIRV-Cross)
* [TinyEXR](https://github.com/syoyo/tinyexr)
* [TinySTL](https://github.com/mendsley/tinystl)
* [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* [GeometryFX](https://gpuopen.com/gaming-product/geometryfx/)
* [WinPixEventRuntime](https://blogs.msdn.microsoft.com/pix/winpixeventruntime/)
* [Fluid Studios Memory Manager](http://www.paulnettle.com/)
* [volk Metaloader for Vulkan](https://github.com/zeux/volk)
* [gainput](https://github.com/jkuhlmann/gainput)
* [Shader Playground](https://github.com/tgjones/shader-playground)
* [hlslparser](https://github.com/Thekla/hlslparser)
* [imGui](https://github.com/ocornut/imgui)
* [DirectX Shader Compiler](https://github.com/Microsoft/DirectXShaderCompiler)
* [Ozz Animation System](https://github.com/guillaumeblanc/ozz-animation)
* [ENTT](https://github.com/skypjack/entt)
* [Lua Scripting System](https://www.lua.org/)
* [TressFX](https://github.com/GPUOpen-Effects/TressFX)
