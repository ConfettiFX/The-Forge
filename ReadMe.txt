PC Requirements:

1. Windows 10 with latest update

2. NVIDIA 9x0 or higher or AMD 5x0 or higher GPU with the latest driver ...

3. Visual Studio 2017 with Windows SDK / DirectX version 16299.91 (Fall Creators Update)
https://developer.microsoft.com/en-us/windows/downloads/sdk-archive

4. Vulkan SDK 1.0.65.1 
https://vulkan.lunarg.com/

We are testing on a wide range of in-house AMD 5x and NVIDIA 9x and higher cards and drivers. We are currently not testing Intel GPU based hardware. We are planning to integrate an Intel GPU based system into our build system in the future.


macOS Requirements:
1. macOS: 10.13.3 Beta (17D39a)

2. XCode: Version 9.2 (9C40b)

3. The Forge is currently tested on the following macOS devices:
* iMac with AMD RADEON 560 (Part No. MNDY2xx/A)
* iMac with AMD RADEON 580 (Part No. MNED2xx/A)

We are occasionally testing on Intel GPU based MacBooks but we are running into -what we believe- driver problems. We are going to address those challenges in the future. In the moment we do not have access to an iMac Pro or Mac Pro. We can test those either with Team Viewer access or by getting them into the office and integrating them into our build system.
We will not test any Hackintosh configuration. 

iOS Requirements:

1. iOS: 11.2.5

2. XCode: see macOS

To run the unit tests, The Forge requires an iOS device with an A9 or higher CPU (see [GPU Processors](https://developer.apple.com/library/content/documentation/DeviceInformation/Reference/iOSDeviceCompatibility/HardwareGPUInformation/HardwareGPUInformation.html) or see iOS_Family in this table [iOS_GPUFamily3_v3](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf)). This is required to support the hardware tessellation unit test and the ExecuteIndirect unit test (requires indirect buffer support). The Visibility Buffer doesn't run on current iOS devices because the [texture argument buffer](https://developer.apple.com/documentation/metal/fundamental_components/gpu_resources/understanding_argument_buffers) on those devices is limited to 31 (see [Metal Feature Set Table](https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf) and look for the entry "Maximum number of entries in the texture argument table, per graphics or compute function") , while on macOS it is 128, which we need for the bindless texture array. 

We are currently testing on 
* iPhone 7 (Model A1778)
