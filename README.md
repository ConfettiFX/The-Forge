# The Forge
The Forge is a cross-platform rendering framework supporting
- PC with DirectX 12 / Vulkan
- macOS with Metal 2
- iOS with Metal 2 (in development)
- Android with Vulkan (in development)
- XBOX One / XBOX One X (only available for accredited developers on request)
- PS4 (in development) (only available for accredited developers on request)

Particularly, The Forge supports cross-platform
- Descriptor management
- Multi-threaded resource loading
- Shader reflection
- Multi-threaded command buffer generation

Future plans are:
- Unified shader generation

The intended usage of The Forge is to enable developers to quickly build their own game engines. The Forge can provide the rendering layer for custom 3D engines.

# Build Status

* Windows [![Build status](https://ci.appveyor.com/api/projects/status/0w2qj3fs8u4utojr/branch/master?svg=true)](https://ci.appveyor.com/project/manaskulkarni786/the-forge/branch/master)
* macOS [![Build Status](https://travis-ci.org/ConfettiFX/The-Forge.svg?branch=master)](https://travis-ci.org/ConfettiFX/The-Forge)

# News
## Release 1.02 - January 31st, 2018
* Fixed all the issues menioned in the issue tracker.
* Removed the NVX commands, we don't use them and they seem to confuse people.
* Dealing with multiple resolutions on PC is now a bit easier. We need to expose this in the GUI, so that you can switch in full-screen between -let's say- 1080p and 4k back and forth
* For macOS the procedural planet unit test works now too. That should bring macOS on par with PC on the macOS platforms we are currently testing. All unit tests and the Visibility Buffer work.
* We improved performance of the Visibility Buffer on macOS a bit more. Now if you have a comparable GPU on the PC, the performance should be on a similar level on macOS and PC.

## Release 1.01 - January 25th, 2018

* Mainly improved the performance of the macOS build. macOS is now using the same art assets as the PC and the performance of the triangle filtering compute shader is improved. 
* Reduced the size of the art assets because we only need one version of San Miguel for all platforms now. 
* macOS now runs the Hardware Tessellation unit test. 
* There is also now a new unit test that shows a procedurally generated planet.

## Release 1.0 - January 22nd, 2018
Very first release.

  
# PC Requirements:

1. NVIDIA 9x0 or higher or AMD 5x0 or higher GPU with the latest driver ...

2. Visual Studio 2015 with Windows SDK / DirectX version 15063
https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

3. Vulkan SDK 1.0.65 
https://vulkan.lunarg.com/

We are testing on a wide range of in-house AMD 5x and NVIDIA 9x and higher cards and drivers. We are currently not testing Intel GPU based hardware. We are planning to integrate an Intel GPU based system into our build system in the future.

# macOS Requirements:

1. macOS: 10.13.3 Beta (17D39a)

2. XCode: Version 9.2 (9C40b)

3. The Forge is currently tested on 
* iMac with AMD RADEON 560 (Part No. MNDY2xx/A)
* iMac with AMD RADEON 580 (Part No. MNED2xx/A)

We are occasionally testing on Intel GPU based MacBooks but we are running into what we believe driver problems. We are going to address those challenges in the future. In the moment we do not have access to an iMac Pro or Mac Pro. We can test those either with Team Viewer access or by getting them into the office and integrating them into our build system.
We will not test any Hackintosh configuration. 
We will get better with testing :-)

# Install
Run PRE_BUILD.bat to download and unzip the art assets.



# Unit Tests
In the moment there are the following unit tests in The Forge:

## 1. Transformation

This unit test just shows a simple solar system. It is our "3D game Hello World" setup for cross-platform rendering.

![Image of the Transformations Unit test](Screenshots/01_Transformations.PNG)

## 2. Compute

This unit test shows a Julia 4D fractal running in a compute shader. In the future this test will use several compute queues at once.

![Image of the Compute Shader Unit test](Screenshots/02_Compute.PNG)

## 3. Multi-Threaded Rendering

This unit test shows the usage of [the open source fiber-based Task Scheduler](https://github.com/SergeyMakeev/TaskScheduler) to generate a large number of command buffers on all platforms supported by The Forge. This unit test is based on [a demo by Intel called Stardust](https://software.intel.com/en-us/articles/using-vulkan-graphics-api-to-render-a-cloud-of-animated-particles-in-stardust-application).

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

## 6. BRDF

The BRDF example shows a simple BRDF model. In the future we might replace this with a better PBR model.

![Image of the BRDF Unit test](Screenshots/06_BRDF.PNG)

## 7. Hardware Tessellation

This unit test showcases the rendering of grass with the help of hardware tessellation.

![Image of the Hardware Tessellation Unit test](Screenshots/07_Hardware_Tessellation.PNG)

## 8. Procedural 
In the spirit of the shadertoy examples this unit test shows a procedurally generated planet.

![Image of the Procedural Unit test](Screenshots/08_Procedural.PNG)


# Examples
There is an example implementation of the Triangle Visibility Buffer as covered in various conference talks (e.g. <a href="http://www.conffx.com/Visibility_Buffer_GDCE.pdf" target="_blank">Triangle Visibility Buffer</a>).

![Image of the Visibility Buffer](Screenshots/Visibility_Buffer.png)


# Releases / Maintenance
Confetti will prepare releases when all the platforms are stable and running and push them to this GitHub repository. Up until a release, development will happen on internal servers. This is to sync up the console, mobile, macOS and PC versions of the source code.
We are looking for people that want to become platform maintainers for certain platforms.


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
* [Nuklear UI](https://github.com/vurtun/nuklear)
* [shaderc](https://github.com/google/shaderc)
* [SPIRV_Cross](https://github.com/KhronosGroup/SPIRV-Cross)
* [Task Scheduler](https://github.com/SergeyMakeev/TaskScheduler)
* [TinyEXR](https://github.com/syoyo/tinyexr)
* [TinySTL](https://github.com/mendsley/tinystl)
* [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
* [GeometryFX](https://gpuopen.com/gaming-product/geometryfx/)
* [WinPixEventRuntime](https://blogs.msdn.microsoft.com/pix/winpixeventruntime/)
