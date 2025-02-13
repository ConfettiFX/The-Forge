/*
 * Copyright (c) 2017-2025 The Forge Interactive Inc.
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

#if defined(__cplusplus)

/************************************************************************/
// Root signature macros
/************************************************************************/
#define BEGIN_RS_STATIC_SAMPLERS(...) static const StaticSamplerDesc gStaticSamplerDescs[] = {
#define END_RS_STATIC_SAMPLERS(...)                                                                 \
    }                                                                                               \
    ;                                                                                               \
    static const uint32_t                gStaticSamplerCount = TF_ARRAY_COUNT(gStaticSamplerDescs); \
    static const DescriptorSetLayoutDesc gStaticSamplerLayout = { NULL, gStaticSamplerDescs, 0, gStaticSamplerCount };

#define RS_STATIC_SAMPLER(type, name, reg, space, filter, mip, aniso, addressU, addressV, addressW, cmp)     \
    { SamplerDesc{ filter, filter, mip, addressU, addressV, addressW, 0.0f, false, 0.0f, 0.0f, aniso, cmp }, \
      VK_STATIC_SAMPLER_SHIFT + reg },

#define INIT_RS_DESC(desc, ...)                 \
    desc.pStaticSamplers = gStaticSamplerDescs; \
    desc.mStaticSamplerCount = gStaticSamplerCount;

/************************************************************************/
// Quest VR specific macros
/************************************************************************/
#if defined(QUEST_VR)
#define FT_MULTIVIEW 1
#if defined(STAGE_VERT)
#define VR_VIEW_ID (gl_ViewID_OVR)
#else
#define VR_VIEW_ID(VID) VID
#endif
#define VR_MULTIVIEW_COUNT 2

#else

#if defined(STAGE_VERT)
#define VR_VIEW_ID 0
#else
#define VR_VIEW_ID(VID) (0)
#endif

#define VR_MULTIVIEW_COUNT 1

#endif

/************************************************************************/
// SRT macros
/************************************************************************/

static inline uint32_t SRTAddOffset(uint32_t* pInOutOffset, uint32_t inc)
{
    uint32_t ret = *pInOutOffset;
    *pInOutOffset += inc;
    return ret;
}

#define SRT_OFFSET(count) SRTAddOffset(&mOffset, count)

#define BEGIN_SRT(name) \
    struct SRT_##name   \
    {
#define END_SRT(name) \
    }                 \
    ;

#define BEGIN_SRT_NO_AB(name) BEGIN_SRT(name)

#define BEGIN_SRT_SET(freq)   \
    struct freq               \
    {                         \
        uint32_t mOffset = 0; \
        uint32_t mPad;

#define END_SRT_SET(freq)                                                                                                           \
    }                                                                                                                               \
    *p##freq;                                                                                                                       \
    static const Descriptor* freq##Ptr()                                                                                            \
    {                                                                                                                               \
        if (!sizeof(freq))                                                                                                          \
        {                                                                                                                           \
            return NULL;                                                                                                            \
        }                                                                                                                           \
        static freq layout = {};                                                                                                    \
        Descriptor* desc = (Descriptor*)(((uint64_t*)&layout) + 1);                                                                 \
        return &desc[0];                                                                                                            \
    }                                                                                                                               \
    static const DescriptorSetLayoutDesc* freq##LayoutPtr()                                                                         \
    {                                                                                                                               \
        if (!sizeof(freq))                                                                                                          \
        {                                                                                                                           \
            return NULL;                                                                                                            \
        }                                                                                                                           \
        static DescriptorSetLayoutDesc layout = { freq##Ptr(), SET_##freq == 0 ? gStaticSamplerDescs : NULL,                        \
                                                  (sizeof(freq) / sizeof(Descriptor)), SET_##freq == 0 ? gStaticSamplerCount : 0 }; \
        return &layout;                                                                                                             \
    }

#define SRT_RES_IDX(srt, freq, name) ((offsetof(SRT_##srt::freq, name) - sizeof(uint64_t)) / sizeof(Descriptor))
#define SRT_SET_DESC(srt, freq, maxSets, nodeIndex)                                                                 \
    {                                                                                                               \
        SET_##freq, (maxSets), (nodeIndex), (sizeof(SRT_##srt::freq) / sizeof(Descriptor)), SRT_##srt::freq##Ptr(), \
            SET_##freq == 0 ? gStaticSamplerDescs : NULL, SET_##freq == 0 ? gStaticSamplerCount : 0                 \
    }

#define SRT_SET_DESC_LARGE_RW(srt, freq, maxSets, nodeIndex)                                                        \
    {                                                                                                               \
        SET_##freq, (maxSets), (nodeIndex), (sizeof(SRT_##srt::freq) / sizeof(Descriptor)), SRT_##srt::freq##Ptr(), \
            SET_##freq == 0 ? gStaticSamplerDescs : NULL, SET_##freq == 0 ? gStaticSamplerCount : 0                 \
    }

#define SRT_LAYOUT_DESC(srt, freq) SRT_##srt::freq##LayoutPtr()

#define DECL_SAMPLER(freq, type, name)                                                                \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_SAMPLER, 1, \
                              SRT_OFFSET(1) + VK_DYNAMIC_SAMPLER_SHIFT };
#define DECL_CBUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, SRT_OFFSET(1) };
#define DECL_TEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_TEXTURE, 1, SRT_OFFSET(1) };

#define DECL_ARRAY_TEXTURES(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_TEXTURE, count, SRT_OFFSET(count) };

#define DECL_BUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_BUFFER, 1, SRT_OFFSET(1) };

#define DECL_RAYTRACING(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE, 1, SRT_OFFSET(1) };

#define DECL_RWTEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, 1, SRT_OFFSET(1) };
#define DECL_WTEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, 1, SRT_OFFSET(1) };
#define DECL_RWBUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_BUFFER, 1, SRT_OFFSET(1) };

#define DECL_ARRAY_RWTEXTURES(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, count, SRT_OFFSET(count) };

#define DECL_ARRAY_RWBUFFERS(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_BUFFER, count, SRT_OFFSET(count) };

#else

/************************************************************************/
// Root signature macros
/************************************************************************/
#define BEGIN_RS_STATIC_SAMPLERS(...)
#define END_RS_STATIC_SAMPLERS(...)
#define RS_STATIC_SAMPLER(type, name, reg, space, filter, mip, aniso, addressU, addressV, addressW, cmp) \
    layout(set = 0, binding = VK_STATIC_SAMPLER_SHIFT + reg) uniform sampler name;
#define ROOT_SIGNATURE(rs)
/************************************************************************/
// SRT macros
/************************************************************************/

#define DECL_SAMPLER(freq, type, name)               layout(set = SET_##freq, binding = 0 + VK_DYNAMIC_SAMPLER_SHIFT) uniform sampler name;
#define DECL_CBUFFER(freq, type, name)               DECLARE_SRT_RESOURCE(type, name, set = SET_##freq, binding = 0);
#define DECL_TEXTURE(freq, type, name)               layout(set = SET_##freq, binding = 0) uniform type name;

#define DECL_ARRAY_TEXTURES(freq, type, name, count) layout(set = SET_##freq, binding = 0) uniform type name[count];
#define DECL_TEXTURE(freq, type, name)               layout(set = SET_##freq, binding = 0) uniform type name;
#define DECL_RWTEXTURE(freq, type, name)             DECLARE_SRT_RESOURCE(type, name, set = SET_##freq, binding = 0);
#define DECL_WTEXTURE(freq, type, name)              DECLARE_SRT_RESOURCE(type, name, set = SET_##freq, binding = 0);
#define DECL_BUFFER(freq, type, name)                DECLARE_SRT_RESOURCE(type, name, set = SET_##freq, binding = 0);

#if FT_RAYTRACING
#define DECL_RAYTRACING(freq, type, name) layout(set = SET_##freq, binding = 0) uniform accelerationStructureEXT name;
#else
#define DECL_RAYTRACING(freq, type, name)
#endif

#define DECL_RWBUFFER(freq, type, name)                DECLARE_SRT_RESOURCE(type, name, set = SET_##freq, binding = 0);

#define DECL_ARRAY_RWTEXTURES(freq, type, name, count) DECLARE_SRT_RESOURCE(type, name[count], set = SET_##freq, binding = 0);
#define DECL_ARRAY_RWBUFFERS(freq, type, name, count)  DECLARE_SRT_RESOURCE(type, name[count], set = SET_##freq, binding = 0);

#endif
