# The Forge
The Forge was developed as a cross-platform rendering framework supporting
- PC with DirectX 12 / Vulkan
- macOS with Metal 2
- iOS with Metal 2 (in development)
- Android with Vulkan (in development)
- XBOX One / XBOX One X (only available for accredited developers on request)
- PS4 (in development) (only available for accredited developers on request)

The Forge supports cross-platform
- Descriptor management
- Multi-threaded resource loading
- Shader reflection
- Multi-threaded command buffer generation

Future plans are:
- unified shader generation


# News
"" First Release January xx, 2018



# PC Requirements:

1. NVIDIA 9x0 or higher or AMD 5x0 or higher GPU with latest driver ...

2. Visual Studio 2015 with Windows SDK / DirectX version 15063
https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

3. Vulkan SDK 1.0.65 
https://vulkan.lunarg.com/


# macOS Requirements:

1. macOS 10.13.3 Beta (17D34a)

2. XCode Version 9.2 (9C40b)


# Unit Tests
In the moment there are the following unit tests in The Forge:

1. Transformation

This unit test just shows a simple solar system. It just shows a "Hello World" like setup for cross-platform rendering.

![Image of the Transformations Unit test](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/01_Transformations.PNG)

2. Compute

This unit test shows a Julia 4D fractal running in a compute shader. In the future this test will use several compute queues at once.

![Image of the Compute Shader Unit test](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/02_Compute.PNG)

3. Multi-Threaded Rendering

This unit test shows the usage of the Task Scheduler (https://github.com/SergeyMakeev/TaskScheduler) to generate a large number of command buffers on all platforms supported by The Forge. This unit test is based on a demo by Intel called Stardust (https://software.intel.com/en-us/articles/using-vulkan-graphics-api-to-render-a-cloud-of-animated-particles-in-stardust-application

![Image of the Multi-Threaded command buffer generation example](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/03_MultiThreading.PNG)

4. ExecuteIndirect

This unit test shows the substantial difference in speed between Instanced Rendering, using ExecuteIndirect with CPU update of the indirect argument buffers and using ExecuteIndirect with GPU update of the indirect argument buffers.
This unit test is based on the Asteroids example by Intel (https://software.intel.com/en-us/articles/asteroids-and-directx-12-performance-and-power-savings).

![Image of the ExecuteIndirect Unit test](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/04_ExecuteIndirect.PNG)
Using ExecuteIndirect with GPU updates for the indirect argument buffers

![Image of the ExecuteIndirect Unit test](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/04_ExecuteIndirect_2.PNG)
Using ExecuteIndirect with CPU updates for the indirect argument buffers

![Image of the ExecuteIndirect Unit test](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/04_ExecuteIndirect_3.PNG)
Using Instanced Rendering

5. Font Rendering

This unit test shows the current state of our font rendering library that is based on several open-source libraries.

![Image of the Font Rendering Unit test](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/05_FontRendering.PNG)

6. BRDF

The BRDF example shows a simple BRDF model. In the future we might replace this with a PBR model.



7. Hardware Tessellation

This unit test show cases the rendering of grass with the help of hardware tessellation.

![Image of the Hardware Tessellation Unit test](https://github.com/ConfettiFX/The-Forge/blob/master/Screenshots/07_Hardware_Tessellation.PNG)

# Examples
There is also an example implementation of the Triangle Visibility Buffer as covered in various conference talks.



# Open-Source Libraries
The Forge utilizes the following Open-Source libraries:
- Assimp (https://github.com/assimp/assimp)
- Bullet Physics (https://github.com/bulletphysics)
- Fontstash (https://github.com/memononen/fontstash)
- Vectormath (https://github.com/glampert/vectormath)
- Nothings single file libs (https://github.com/nothings/stb)
 - stb_hash.h
 - stb_image.h
 - stb_image_resize.h
 - stb_image_write.h
- Nuklear UI (https://github.com/vurtun/nuklear)
- shaderc (https://github.com/google/shaderc)
- SPIRV_Cross (https://github.com/KhronosGroup/SPIRV-Cross)
- Task Scheduler (https://github.com/SergeyMakeev/TaskScheduler)
- TinyEXR (https://github.com/syoyo/tinyexr)
- TinySTL (https://github.com/mendsley/tinystl)
- Vulkan Memory Allocator (https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- WinPixEventRuntime (https://blogs.msdn.microsoft.com/pix/winpixeventruntime/)