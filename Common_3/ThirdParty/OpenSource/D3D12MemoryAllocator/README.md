# D3D12 Memory Allocator

Easy to integrate memory allocation library for Direct3D 12.

**License:** MIT. See [LICENSE.txt](LICENSE.txt)

**Changelog:** See [CHANGELOG.md](CHANGELOG.md)

**Product page:** [D3D12 Memory Allocator on GPUOpen](https://gpuopen.com/gaming-product/d3d12-memory-allocator/)

**Build status:**

Windows: [![Build status](https://ci.appveyor.com/api/projects/status/860i07bxv55ydgvg?svg=true)](https://ci.appveyor.com/project/adam-sawicki-amd/d3d12memoryallocator)

# Problem

Memory allocation and resource (buffer and texture) creation in new, explicit graphics APIs (Vulkan® and Direct3D 12) is difficult comparing to older graphics APIs like Direct3D 11 or OpenGL® because it is recommended to allocate bigger blocks of memory and assign parts of them to resources. [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/) is a library that implements this functionality for Vulkan. It is available online since 2017 and it is successfully used in many software projects, including some AAA game studios. This is an equivalent library for D3D12.

# Features

This library can help developers to manage memory allocations and resource creation by offering function `Allocator::CreateResource` similar to the standard `ID3D12Device::CreateCommittedResource`. It internally:

- Allocates and keeps track of bigger memory heaps, used and unused ranges inside them, finds best matching unused ranges to create new resources there as placed resources.
- Automatically respects size and alignment requirements for created resources.
- Automatically handles resource heap tier - whether it's `D3D12_RESOURCE_HEAP_TIER_1` that requires to keep certain classes of resources separate or `D3D12_RESOURCE_HEAP_TIER_2` that allows to keep them all together.

Additional features:

- Well-documented - description of all classes and functions provided, along with chapters that contain general description and example code.
- Thread-safety: Library is designed to be used in multithreaded code.
- Configuration: Fill optional members of `ALLOCATOR_DESC` structure to provide custom CPU memory allocator and other parameters.
- Customization: Predefine appropriate macros to provide your own implementation of external facilities used by the library, like assert, mutex, and atomic.
- Statistics: Obtain detailed statistics about the amount of memory used, unused, number of allocated blocks, number of allocations etc. - globally and per memory heap type.
- Debug annotations: Associate string name with every allocation.
- JSON dump: Obtain a string in JSON format with detailed map of internal state, including list of allocations and gaps between them.

# Prerequisites

- Self-contained C++ library in single pair of H + CPP files. No external dependencies other than standard C, C++ library and Windows SDK. STL containers, C++ exceptions, and RTTI are not used.
- Object-oriented interface in a convention similar to D3D12.
- Error handling implemented by returning `HRESULT` error codes - same way as in D3D12.
- Interface documented using Doxygen-style comments.

# Example

Basic usage of this library is very simple. Advanced features are optional. After you created global `Allocator` object, a complete code needed to create a texture may look like this:

```cpp
D3D12_RESOURCE_DESC resourceDesc = {};
resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
resourceDesc.Alignment = 0;
resourceDesc.Width = 1024;
resourceDesc.Height = 1024;
resourceDesc.DepthOrArraySize = 1;
resourceDesc.MipLevels = 1;
resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
resourceDesc.SampleDesc.Count = 1;
resourceDesc.SampleDesc.Quality = 0;
resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

D3D12MA::ALLOCATION_DESC allocationDesc = {};
allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

D3D12Resource* resource;
D3D12MA::Allocation* allocation;
HRESULT hr = allocator->CreateResource(
    &allocationDesc,
    &resourceDesc,
    D3D12_RESOURCE_STATE_COPY_DEST,
    NULL,
    &allocation,
    IID_PPV_ARGS(&resource));
```

With this one function call:

1. `ID3D12Heap` memory block is allocated if needed.
2. An unused region of the memory block assigned.
3. `ID3D12Resource` is created as placed resource, bound to this region.

`Allocation` is an object that represents memory assigned to this texture. It can be queried for parameters like offset and size.

# Binariers

The release comes with precompiled binary executable for "D3D12Sample" application which contains test suite. It is compiled using Visual Studio 2017, so it requires appropriate libraries to work, including "vcruntime140.dll" and "msvcp140.dll". If its launch fails with error message telling about those files missing, please download and install [Microsoft Visual C++ Redistributable for Visual Studio 2015, 2017 and 2019](https://support.microsoft.com/en-us/help/2977003/the-latest-supported-visual-c-downloads), "x64" version.

# Documentation

Documentation is provided together with the library in form of Doxygen-style comments inside the code, in file "src/D3D12MemAlloc.h". They can be read directly or turned into HTML and other convenient to read formats. Unfortunately we can't provide pregenerated version browseable online. You can generate it on your own by following these steps:

1. Install [Doxygen](http://www.doxygen.nl/).
2. Open command line and enter "src" directory.
3. Type command: `doxygen`
4. Open following file in a web browser: "docs/html/index.html".

# Copyright notice

This software package uses third party software:

- Parts of the code of [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/) by AMD, license: MIT
- Parts of the code of [DirectX-Graphics-Samples](https://github.com/microsoft/DirectX-Graphics-Samples) by Microsoft, license: MIT
- [Premake 5](https://premake.github.io/) binary, license: BSD

For more information see [NOTICES.txt](NOTICES.txt).

# Software using this library

Place for the link to your project :)

# See also

- **[Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/)** - equivalent library for Vulkan. License: MIT.
