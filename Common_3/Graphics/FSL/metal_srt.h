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
#define RWTex2D(...)
#define RWByteBuffer int
/************************************************************************/
// Root signature macros
/************************************************************************/
#define BEGIN_RS_STATIC_SAMPLERS(...)
#define END_RS_STATIC_SAMPLERS(...)
#define RS_STATIC_SAMPLER(...)
#define INIT_RS_DESC(...)
/************************************************************************/
// SRT macros
/************************************************************************/
#define BEGIN_SRT(name) \
    struct SRT_##name   \
    {                   \
        static const uint32_t isParamsOnly() { return 0; }

#define BEGIN_SRT_NO_AB(name) \
    struct SRT_##name         \
    {                         \
        static const uint32_t isParamsOnly() { return 1; }

#define END_SRT(name)                                                         \
    }                                                                         \
    ;                                                                         \
    static const MetalDescriptorSet* getSrt##name##Ptr()                      \
    {                                                                         \
        static SRT_##name   srt = {};                                         \
        MetalDescriptorSet* destSet = (MetalDescriptorSet*)((uint64_t*)&srt); \
        return &destSet[0];                                                   \
    }

#define SRT_SET_COUNT(name) (sizeof(name) / sizeof(MetalDescriptorSet))

#define BEGIN_SRT_SET(freq) \
    struct freq             \
    {
#define END_SRT_SET(freq)                                                              \
    }                                                                                  \
    *p##freq;                                                                          \
    static const Descriptor* freq##Ptr()                                               \
    {                                                                                  \
        if (sizeof(freq) <= sizeof(uint64_t))                                          \
        {                                                                              \
            return NULL;                                                               \
        }                                                                              \
        static freq layout = {};                                                       \
        Descriptor* desc = (Descriptor*)((uint64_t*)&layout);                          \
        return &desc[0];                                                               \
    }                                                                                  \
    const uint32_t    mResourceCount##freq = sizeof(struct freq) / sizeof(Descriptor); \
    const Descriptor* pDescriptors##freq = freq##Ptr();

#define SRT_RES_IDX(srt, freq, name) (offsetof(SRT_##srt::freq, name) / sizeof(Descriptor))
#define SRT_SET_DESC(srt, freq, maxSets, nodeIndex)                                                                                 \
    {                                                                                                                               \
        (offsetof(SRT_##srt, p##freq) / ((uint32_t)(sizeof(MetalDescriptorSet)))), (maxSets), (nodeIndex),                          \
            (sizeof(SRT_##srt::freq) / sizeof(Descriptor)), SRT_##srt::freq##Ptr(), 0, getSrt##srt##Ptr(), SRT_SET_COUNT(SRT_##srt) \
    }

#define SRT_SET_DESC_LARGE_RW(srt, freq, maxSets, nodeIndex)                                                                        \
    {                                                                                                                               \
        (offsetof(SRT_##srt, p##freq) / ((uint32_t)(sizeof(MetalDescriptorSet)))), (maxSets), (nodeIndex),                          \
            (sizeof(SRT_##srt::freq) / sizeof(Descriptor)), SRT_##srt::freq##Ptr(), 1, getSrt##srt##Ptr(), SRT_SET_COUNT(SRT_##srt) \
    }

#define IS_AB_RES(is_ab) (is_ab & (isParamsOnly() ? 0u : 1u))

#define DECL_SAMPLER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_SAMPLER, 1, 0, IS_AB_RES(0) };
#define DECL_CBUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, 0, IS_AB_RES(1) };
#define DECL_TEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_TEXTURE, 1, 0, IS_AB_RES(1) };

#define DECL_ARRAY_TEXTURES(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_TEXTURE, count, 0, IS_AB_RES(1) };

#define DECL_BUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_BUFFER, 1, 0, IS_AB_RES(1) };
#define DECL_RAYTRACING(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE, 1, 0, IS_AB_RES(1) };
#define DECL_RWTEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, 1, 0, IS_AB_RES(0) };
#define DECL_WTEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, 1, 0, IS_AB_RES(0) };
#define DECL_RWBUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_BUFFER, 1, 0, IS_AB_RES(0) };

#define DECL_ARRAY_RWTEXTURES(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, count, 0, IS_AB_RES(0) };

#define DECL_ARRAY_RWBUFFERS(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, SET_##freq, ) DESCRIPTOR_TYPE_RW_BUFFER, count, 0, IS_AB_RES(0) };

#else

/************************************************************************/
// Root signature macros
/************************************************************************/
#define BEGIN_RS_STATIC_SAMPLERS(...)
#define END_RS_STATIC_SAMPLERS(...)
#define RS_STATIC_SAMPLER(type, name, reg, space, _filter, mip, aniso, addressU, addressV, addressW, cmp)                   \
    constexpr sampler name(coord::normalized, filter::_filter, mip_filter::mip, max_anisotropy(aniso), s_address::addressU, \
                           t_address::addressV, r_address::addressW, compare_func::cmp);
#define ROOT_SIGNATURE(rs)
/************************************************************************/
// SRT macros
/************************************************************************/

#define BEGIN_SRT_SET(freq)
#define END_SRT_SET(freq)

#define DECL_SAMPLER(freq, type, name)               DECLARE_SRT_RESOURCE(type, name, freq);
#define DECL_CBUFFER(freq, type, name)               DECLARE_SRT_RESOURCE(type, name, freq);
#define DECL_TEXTURE(freq, type, name)               DECLARE_SRT_RESOURCE(type, name, freq);

#define DECL_ARRAY_TEXTURES(freq, type, name, count) DECLARE_SRT_RESOURCE(type, name[count], freq);

#define DECL_BUFFER(freq, type, name)                DECLARE_SRT_RESOURCE(type, name, freq);

#if FT_RAYTRACING
#define DECL_RAYTRACING(freq, type, name) DECLARE_SRT_RESOURCE(type, name, freq);
#else
#define DECL_RAYTRACING(freq, type, name)
#endif
#define DECL_RWTEXTURE(freq, type, name)               DECLARE_SRT_RESOURCE(type, name, freq);
#define DECL_WTEXTURE(freq, type, name)                DECLARE_SRT_RESOURCE(type, name, freq);
#define DECL_RWBUFFER(freq, type, name)                DECLARE_SRT_RESOURCE(type, name, freq);

#define DECL_ARRAY_RWTEXTURES(freq, type, name, count) DECLARE_SRT_RESOURCE(type, name[count], freq);
#define DECL_ARRAY_RWBUFFERS(freq, type, name, count)  DECLARE_SRT_RESOURCE(type, name[count], freq);

#endif
