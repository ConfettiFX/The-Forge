/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

inline uint32_t getFamilyTier(id<MTLDevice> device)
{
    uint32_t familyTier = 0;

#if !defined(TARGET_IOS) // macOS
    familyTier = [device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v1] ? 1 : familyTier;
#if defined(ENABLE_GPU_FAMILY_1_V2)
    if (@available(macOS 10.12, *))
    {
        familyTier = [device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v2] ? 2 : familyTier;
    }
#endif
#if defined(ENABLE_GPU_FAMILY_1_V3)
    if (@available(macOS 10.13, *))
    {
        familyTier = [device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v3] ? 3 : familyTier;
    }
#endif
#if defined(ENABLE_GPU_FAMILY_1_V4)
    if (@available(macOS 10.14, *))
    {
        familyTier = [device supportsFeatureSet:MTLFeatureSet_macOS_GPUFamily1_v4] ? 4 : familyTier;
    }
#endif
#else // IOS
    familyTier = [device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily1_v1] ? 1 : familyTier;

    familyTier = [device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily2_v1] ? 2 : familyTier;

#if defined(ENABLE_GPU_FAMILY_3)
    if (@available(iOS 9.0, *))
    {
        familyTier = [device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily3_v1] ? 3 : familyTier;
    }
#endif
#if defined(ENABLE_GPU_FAMILY_4)
    if (@available(iOS 11.0, *))
    {
        familyTier = [device supportsFeatureSet:MTLFeatureSet_iOS_GPUFamily4_v1] ? 4 : familyTier;
    }
#endif

#endif

    return familyTier;
}

inline void mtlCapsBuilder(id<MTLDevice> pDevice, GpuDesc* pGpuDesc)
{
    // for metal this is a case of going through each family and looking up the info off apple documentation
    // we start low and go higher, add things as we go
    // data from https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf

    // all pixel formats that metal support it claims can be sampled from if they exist on the platform
    // this is however a lie when compressed texture formats
    for (uint32_t i = 0; i < TinyImageFormat_Count; ++i)
    {
        TinyImageFormat_MTLPixelFormat mtlFmt = TinyImageFormat_ToMTLPixelFormat((TinyImageFormat)i);

        if (mtlFmt != TIF_MTLPixelFormatInvalid)
        {
            // Metal Feature Set Tables:
            // All graphics and compute kernels can read or sample a texture with any pixel format.
#ifndef TARGET_IOS
            pGpuDesc->mFormatCaps[i] = TinyImageFormat_MTLPixelFormatOnMac(mtlFmt) ? FORMAT_CAP_READ : FORMAT_CAP_NONE;
#else
            pGpuDesc->mFormatCaps[i] = TinyImageFormat_MTLPixelFormatOnIOS(mtlFmt) ? FORMAT_CAP_READ : FORMAT_CAP_NONE;
#endif
        }
        else
        {
            pGpuDesc->mFormatCaps[i] = FORMAT_CAP_NONE;
        }
    }

#define CAN_SHADER_SAMPLE_LINEAR(x) pGpuDesc->mFormatCaps[TinyImageFormat_##x] |= FORMAT_CAP_LINEAR_FILTER;
#define CAN_SHADER_WRITE(x)         pGpuDesc->mFormatCaps[TinyImageFormat_##x] |= FORMAT_CAP_WRITE;
#define CAN_RENDER_TARGET_WRITE(x)  pGpuDesc->mFormatCaps[TinyImageFormat_##x] |= FORMAT_CAP_RENDER_TARGET;
#define CAN_SHADER_READ_WRITE(x)    pGpuDesc->mFormatCaps[TinyImageFormat_##x] |= FORMAT_CAP_READ_WRITE;
// Read-write capability needs special treatment.
#define CAN_SHADER_AND_RENDER_TARGET_WRITE(x) \
    CAN_SHADER_WRITE(x)                       \
    CAN_RENDER_TARGET_WRITE(x)
#define CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(x) \
    CAN_SHADER_SAMPLE_LINEAR(x)                             \
    CAN_RENDER_TARGET_WRITE(x)
#define CAN_ALL(x)              \
    CAN_SHADER_SAMPLE_LINEAR(x) \
    CAN_SHADER_WRITE(x)         \
    CAN_RENDER_TARGET_WRITE(x)

    MTLGPUFamily highestAppleFamily = MTLGPUFamilyApple1;
    int          currentFamily = HIGHEST_GPU_FAMILY;
    for (; currentFamily >= (int)MTLGPUFamilyApple1; currentFamily--)
    {
        if ([pDevice supportsFamily:highestAppleFamily])
        {
            highestAppleFamily = (MTLGPUFamily)currentFamily;
            break;
        }
    }
    if (highestAppleFamily >= MTLGPUFamilyApple2)
    {
        // Ordinary 8-bit pixel formats
        CAN_SHADER_SAMPLE_LINEAR(A8_UNORM)
        CAN_ALL(R8_UNORM)
        CAN_ALL(R8_SRGB)
        CAN_ALL(R8_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8_SINT)
        // Ordinary 16-bit pixel formats
        CAN_ALL(R16_UNORM)
        CAN_ALL(R16_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16_SINT)
        CAN_ALL(R16_SFLOAT)
        CAN_ALL(R8G8_UNORM)
        CAN_ALL(R8G8_SRGB)
        CAN_ALL(R8G8_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8_SINT)
        // Packed 16-bit pixel formats
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(R5G6B5_UNORM)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(R5G5B5A1_UNORM)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(R4G4B4A4_UNORM)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(A1R5G5B5_UNORM)
        // Ordinary 32-bit pixel formats
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32_SINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32_SFLOAT)
        CAN_ALL(R16G16_UNORM)
        CAN_ALL(R16G16_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16_SINT)
        CAN_ALL(R16G16_SFLOAT)
        CAN_ALL(R8G8B8A8_UNORM)
        CAN_ALL(R8G8B8A8_SRGB)
        CAN_ALL(R8G8B8A8_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8B8A8_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8B8A8_SINT)
        CAN_ALL(B8G8R8A8_UNORM)
        CAN_ALL(B8G8R8A8_SRGB)
        // Packed 32-bit pixel formats
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(R10G10B10A2_UNORM)
        CAN_ALL(B10G10R10A2_UNORM)
        CAN_RENDER_TARGET_WRITE(R10G10B10A2_UINT)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(B10G11R11_UFLOAT)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(E5B9G9R9_UFLOAT)
        // Ordinary 64-bit pixel formats
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32_SINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32_SFLOAT)
        CAN_ALL(R16G16B16A16_UNORM)
        CAN_ALL(R16G16B16A16_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16B16A16_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16B16A16_SINT)
        CAN_ALL(R16G16B16A16_SFLOAT)
        // Ordinary 128-bit pixel formats
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32B32A32_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32B32A32_SINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32B32A32_SFLOAT)
        // Compressed pixel formats
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_2BPP_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_2BPP_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_4BPP_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_4BPP_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_2BPP_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_2BPP_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_4BPP_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(PVRTC1_4BPP_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_EAC_R11_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_EAC_R11_SNORM)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_EAC_R11G11_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_EAC_R11G11_SNORM)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_R8G8B8A8_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_R8G8B8A8_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_R8G8B8_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_R8G8B8_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_R8G8B8A1_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ETC2_R8G8B8A1_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_4x4_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_5x4_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_5x4_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_6x5_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_6x6_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_8x5_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_8x6_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_8x8_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x5_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x6_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x8_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x10_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_12x10_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_12x12_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_4x4_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_5x4_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_5x4_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_6x5_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_6x6_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_8x5_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_8x6_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_8x8_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x5_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x6_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x8_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_10x10_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_12x10_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(ASTC_12x12_UNORM)
        // TinyImage doesn't support HDR ASTC formats.
        // YUV pixel formats - not supported by TinyImage.
        // Depth and stencil pixel formats.
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(D16_UNORM)
        // Extended range and wide color pixel formats - not supported by TinyImage.
    }
    if (highestAppleFamily >= MTLGPUFamilyApple3)
    {
        // Ordinary 8-bit pixel formats.
        CAN_ALL(A8_UNORM)
        // Ordinary 16-bit pixel formats.
        // Packed 16-bit pixel formats.
        // Ordinary 32-bit pixel formats.
        // Packed 32-bit pixel formats.
        CAN_ALL(R10G10B10A2_UNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R10G10B10A2_UINT)
        CAN_ALL(B10G11R11_UFLOAT)
        CAN_ALL(E5B9G9R9_UFLOAT)
        // Ordinary 64-bit pixel formats.
        // Ordinary 128-bit pixel formats.
        // Compressed pixel formats.
        // YUV pixel formats.
        // Depth and stencil pixel formats.
        // Extended range and wide color pixel formats.
    }
    if (highestAppleFamily >= MTLGPUFamilyApple4)
    {
        // Ordinary 8-bit pixel formats.
        // Ordinary 16-bit pixel formats.
        // Packed 16-bit pixel formats.
        // Ordinary 32-bit pixel formats.
        // Packed 32-bit pixel formats.
        // Ordinary 64-bit pixel formats.
        // Ordinary 128-bit pixel formats.
        // Compressed pixel formats.
        // YUV pixel formats.
        // Depth and stencil pixel formats.
        // Extended range and wide color pixel formats.
    }
    if (highestAppleFamily >= MTLGPUFamilyApple5)
    {
        // Ordinary 8-bit pixel formats.
        // Ordinary 16-bit pixel formats.
        // Packed 16-bit pixel formats.
        // Ordinary 32-bit pixel formats.
        // Packed 32-bit pixel formats.
        // Ordinary 64-bit pixel formats.
        // Ordinary 128-bit pixel formats.
        // Compressed pixel formats.
        // YUV pixel formats.
        // Depth and stencil pixel formats.
        // Extended range and wide color pixel formats.
    }
#ifdef ENABLE_GPU_FAMILY_6
    if (highestAppleFamily >= MTLGPUFamilyApple6)
    {
        // Ordinary 8-bit pixel formats.
        // Ordinary 16-bit pixel formats.
        // Packed 16-bit pixel formats.
        // Ordinary 32-bit pixel formats.
        // Packed 32-bit pixel formats.
        // Ordinary 64-bit pixel formats.
        // Ordinary 128-bit pixel formats.
        // Compressed pixel formats.
        // YUV pixel formats.
        // Depth and stencil pixel formats.
        // Extended range and wide color pixel formats.
    }
#endif // ENABLE_GPU_FAMILY_6
#ifdef ENABLE_GPU_FAMILY_7
    if (highestAppleFamily >= MTLGPUFamilyApple7)
    {
        // Ordinary 8-bit pixel formats.
        // Ordinary 16-bit pixel formats.
        // Packed 16-bit pixel formats.
        // Ordinary 32-bit pixel formats.
        // Packed 32-bit pixel formats.
        // Ordinary 64-bit pixel formats.
        // Ordinary 128-bit pixel formats.
        // Compressed pixel formats.
#ifdef TARGET_MACOS
        if (@available(macOS 11.0, iOS 16.4, *))
        {
            if ([pDevice supportsBCTextureCompression])
            {
                CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGB_UNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGB_SRGB)
                CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGBA_UNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGBA_SRGB)
                CAN_SHADER_SAMPLE_LINEAR(DXBC2_UNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC2_SRGB)
                CAN_SHADER_SAMPLE_LINEAR(DXBC3_UNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC3_SRGB)
                CAN_SHADER_SAMPLE_LINEAR(DXBC4_UNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC4_SNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC5_UNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC5_SNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC6H_UFLOAT)
                CAN_SHADER_SAMPLE_LINEAR(DXBC6H_SFLOAT)
                CAN_SHADER_SAMPLE_LINEAR(DXBC7_UNORM)
                CAN_SHADER_SAMPLE_LINEAR(DXBC7_SRGB)
            }
        }
#endif // TARGET_IOS
       // YUV pixel formats.
       // Depth and stencil pixel formats.
       // Extended range and wide color pixel formats.
    }
#endif // ENABLE_GPU_FAMILY_7
#ifdef ENABLE_GPU_FAMILY_8
    if (highestAppleFamily >= MTLGPUFamilyApple8)
    {
        // Ordinary 8-bit pixel formats.
        // Ordinary 16-bit pixel formats.
        // Packed 16-bit pixel formats.
        // Ordinary 32-bit pixel formats.
        // Packed 32-bit pixel formats.
        // Ordinary 64-bit pixel formats.
        // Ordinary 128-bit pixel formats.
        // Compressed pixel formats.
        // YUV pixel formats.
        // Depth and stencil pixel formats.
        // Extended range and wide color pixel formats.
    }
#endif // ENABLE_GPU_FAMILY_8
    if ([pDevice supportsFamily:MTLGPUFamilyMac2])
    {
        // Ordinary 8-bit pixel formats.
        CAN_ALL(A8_UNORM)
        CAN_ALL(R8_UNORM)
        CAN_ALL(R8_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8_SINT)
        // Ordinary 16-bit pixel formats.
        CAN_ALL(R16_UNORM)
        CAN_ALL(R16_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16_SINT)
        CAN_ALL(R16_SFLOAT)
        CAN_ALL(R8G8_UNORM)
        CAN_ALL(R8G8_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8_SINT)
        // Packed 16-bit pixel formats.
        // Ordinary 32-bit pixel formats.
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32_SINT)
        CAN_ALL(R32_SFLOAT)
        CAN_ALL(R16G16_UNORM)
        CAN_ALL(R16G16_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16_SINT)
        CAN_ALL(R16G16_SFLOAT)
        CAN_ALL(R8G8B8A8_UNORM)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(R8G8B8A8_SRGB)
        CAN_ALL(R8G8B8A8_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8B8A8_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R8G8B8A8_SINT)
        CAN_ALL(B8G8R8A8_UNORM)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(B8G8R8A8_SRGB)
        // Packed 32-bit pixel formats
        CAN_ALL(R10G10B10A2_UNORM)
        CAN_ALL(B10G10R10A2_UNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R10G10B10A2_UINT)
        CAN_ALL(B10G11R11_UFLOAT)
        CAN_SHADER_SAMPLE_LINEAR(E5B9G9R9_UFLOAT)
        // Ordinary 64-bit pixel formats
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32_SINT)
        CAN_ALL(R32G32_SFLOAT)
        CAN_ALL(R16G16B16A16_UNORM)
        CAN_ALL(R16G16B16A16_SNORM)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16B16A16_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R16G16B16A16_SINT)
        CAN_ALL(R16G16B16A16_SFLOAT)
        // Ordinary 128-bit pixel formats
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32B32A32_UINT)
        CAN_SHADER_AND_RENDER_TARGET_WRITE(R32G32B32A32_SINT)
        CAN_ALL(R32G32B32A32_SFLOAT)
        // Compressed pixel formats
        CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGB_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGB_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGBA_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC1_RGBA_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(DXBC2_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC2_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(DXBC3_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC3_SRGB)
        CAN_SHADER_SAMPLE_LINEAR(DXBC4_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC4_SNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC5_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC5_SNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC6H_UFLOAT)
        CAN_SHADER_SAMPLE_LINEAR(DXBC6H_SFLOAT)
        CAN_SHADER_SAMPLE_LINEAR(DXBC7_UNORM)
        CAN_SHADER_SAMPLE_LINEAR(DXBC7_SRGB)
        // YUV pixel formats - not supported by TinyImage.
        // Depth and stencil pixel formats.
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(D16_UNORM)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(D32_SFLOAT)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(D24_UNORM_S8_UINT)
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(D32_SFLOAT_S8_UINT)
        // Extended range and wide color pixel formats - not supported by TinyImage.
    }
#ifdef TARGET_MACOS
    if ([pDevice isDepth24Stencil8PixelFormatSupported])
    {
        CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE(D24_UNORM_S8_UINT)
    }
#endif // TARGET_MACOS
    // this call is supported on mac and ios
    // technically I think you can write but not read some texture, this is telling
    // you you can do both.
    MTLReadWriteTextureTier rwTextureTier = [pDevice readWriteTextureSupport];
    // intentional fall through on this switch
    switch (rwTextureTier)
    {
    default:
    case MTLReadWriteTextureTier2:
        CAN_SHADER_READ_WRITE(R32G32B32A32_SFLOAT);
        CAN_SHADER_READ_WRITE(R32G32B32A32_UINT);
        CAN_SHADER_READ_WRITE(R32G32B32A32_SINT);
        CAN_SHADER_READ_WRITE(R16G16B16A16_SFLOAT);
        CAN_SHADER_READ_WRITE(R16G16B16A16_UINT);
        CAN_SHADER_READ_WRITE(R16G16B16A16_SINT);
        CAN_SHADER_READ_WRITE(R8G8B8A8_UNORM);
        CAN_SHADER_READ_WRITE(R8G8B8A8_UINT);
        CAN_SHADER_READ_WRITE(R8G8B8A8_SINT);
        CAN_SHADER_READ_WRITE(R16_SFLOAT);
        CAN_SHADER_READ_WRITE(R16_UINT);
        CAN_SHADER_READ_WRITE(R16_SINT);
        CAN_SHADER_READ_WRITE(R8_UNORM);
        CAN_SHADER_READ_WRITE(R8_UINT);
        CAN_SHADER_READ_WRITE(R8_SINT);
    case MTLReadWriteTextureTier1:
        CAN_SHADER_READ_WRITE(R32_SFLOAT);
        CAN_SHADER_READ_WRITE(R32_UINT);
        CAN_SHADER_READ_WRITE(R32_SINT);
    case MTLReadWriteTextureTierNone:
        break;
    }
#if defined(ENABLE_32_BIT_FLOAT_FILTERING)
    if (@available(macOS 11.0, iOS 14.0, *))
    {
        if ([pDevice supports32BitFloatFiltering])
        {
            CAN_SHADER_SAMPLE_LINEAR(R32_SFLOAT);
            CAN_SHADER_SAMPLE_LINEAR(R32G32_SFLOAT);
            CAN_SHADER_SAMPLE_LINEAR(R32G32B32A32_SFLOAT);
        }
    }
#elif defined(ENABLE_GPU_FAMILY_7)
    // We use iOS SDK 14.0 for testing so need a workaround.
    if (highestAppleFamily >= MTLGPUFamilyApple7)
    {
        CAN_SHADER_SAMPLE_LINEAR(R32_SFLOAT);
        CAN_SHADER_SAMPLE_LINEAR(R32G32_SFLOAT);
        CAN_SHADER_SAMPLE_LINEAR(R32G32B32A32_SFLOAT);
    }
#endif

#undef CAN_SHADER_SAMPLE_LINEAR
#undef CAN_SHADER_WRITE
#undef CAN_RENDER_TARGET_WRITE
#undef CAN_SHADER_READ_WRITE
#undef CAN_SHADER_AND_RENDER_TARGET_WRITE
#undef CAN_SHADER_SAMPLE_LINEAR_AND_RENDER_TARGET_WRITE
#undef CAN_ALL
}
