# 🐺 volk [![Build Status](https://travis-ci.org/zeux/volk.svg?branch=master)](https://travis-ci.org/zeux/volk)

## Purpose

volk is a meta-loader for Vulkan. It allows you to dynamically load entrypoints required to use Vulkan
without linking to vulkan-1.dll or statically linking Vulkan loader. Additionally, volk simplifies the use of Vulkan extensions by automatically loading all associated entrypoints. Finally, volk enables loading
Vulkan entrypoints directly from the driver which can increase performance by skipping loader dispatch overhead.

volk is written in C89 and supports Windows, Linux, Android and macOS (via MoltenVK).

## Building

volk comes with one header and source file; to build it, just add the source file, `volk.c`, to your build system.

To use volk, you have to include `volk.h` instead of `vulkan/vulkan.h`; this is necessary to use function definitions from volk.
If some files in your application include `vulkan/vulkan.h` and don't include `volk.h`, this can result in symbol conflicts; consider defining `VK_NO_PROTOTYPES` when compiling code that uses Vulkan to make sure this doesn't happen.

## Basic usage

To initialize volk, call this function first:

```c++
VkResult volkInitialize();
```

This will attempt to load Vulkan loader from the system; if this function returns `VK_SUCCESS` you can proceed to create Vulkan instance.
If this function fails, this means Vulkan loader isn't installed on your system.

After creating the Vulkan instance using Vulkan API, call this function:

```c++
void volkLoadInstance(VkInstance instance);
```

This function will load all required Vulkan entrypoints, including all extensions; you can use Vulkan from here on as usual.

## Optimizing device calls

If you use volk as described in the previous section, all device-related function calls, such as `vkCmdDraw`, will go through Vulkan loader dispatch code.
This allows you to transparently support multiple VkDevice objects in the same application, but comes at a price of dispatch overhead which can be as high as 7% depending on the driver and application.

To avoid this, you have one of two options:

1. For applications that use just one VkDevice object, load device-related Vulkan entrypoints directly from the driver with this function:

```c++
void volkLoadDevice(VkDevice device);
```

2. For applications that use multiple VkDevice objects, load device-related Vulkan entrypoints into a table:

```c++
void volkLoadDeviceTable(struct VolkDeviceTable* table, VkDevice device);
```

The second option requires you to change the application code to store one `VolkDeviceTable` per `VkDevice` and call functions from this table instead.

Device entrypoints are loaded using `vkGetDeviceProcAddr`; when no layers are present, this commonly results in most function pointers pointing directly at the driver functions, minimizing the call overhead. When layers are loaded, the entrypoints will point at the implementations in the first applicable layer, so this is compatible with any layers including validation layers.

## License

This library is available to anybody free of charge, under the terms of MIT License:

	Copyright (c) 2018-2019 Arseny Kapoulkine

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
