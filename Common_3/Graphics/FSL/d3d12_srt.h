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

#define BEGIN_RS_STATIC_SAMPLERS(...)
#define END_RS_STATIC_SAMPLERS(...)
#define INIT_RS_DESC(desc, graphicsFileName, computeFileName) \
    desc.pGraphicsFileName = graphicsFileName;                \
    desc.pComputeFileName = computeFileName;

#if defined(__cplusplus)

/************************************************************************/
// Root signature macros
/************************************************************************/
#define RS_STATIC_SAMPLER(...)
#define ROOT_SIGNATURE(...)
#define Buffer(x)                       DESCRIPTOR_TYPE_BUFFER
#define ByteBuffer                      DESCRIPTOR_TYPE_BUFFER_RAW
#define RWByteBuffer                    DESCRIPTOR_TYPE_RW_BUFFER_RAW
#define RaytracingAccelerationStructure DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE
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

// supress PVS-Studio warning regarding "layout"
//-V:END_SRT_SET:1096
#define BEGIN_SRT(name) \
    struct SRT_##name   \
    {
#define BEGIN_SRT_NO_AB(name) BEGIN_SRT(name)
#define END_SRT(name) \
    }                 \
    ;
#define BEGIN_SRT_SET(freq)   \
    struct freq               \
    {                         \
        uint32_t mOffset = 0; \
        uint32_t mPad = 0;
#define END_SRT_SET(freq)                                           \
    }                                                               \
    *p##freq;                                                       \
    static const Descriptor* freq##Ptr()                            \
    {                                                               \
        if (!sizeof(freq))                                          \
        {                                                           \
            return NULL;                                            \
        }                                                           \
        static freq layout = {};                                    \
        Descriptor* desc = (Descriptor*)(((uint64_t*)&layout) + 1); \
        return &desc[0];                                            \
    }

#define SRT_RES_IDX(srt, freq, name) ((offsetof(SRT_##srt::freq, name) - sizeof(uint64_t)) / sizeof(Descriptor))
#define SRT_SET_DESC(srt, freq, maxSets, nodeIndex)                                                                       \
    {                                                                                                                     \
        ROOT_PARAM_##freq, (maxSets), (nodeIndex), (sizeof(SRT_##srt::freq) / sizeof(Descriptor)), SRT_##srt::freq##Ptr() \
    }

#define SRT_SET_DESC_LARGE_RW(srt, freq, maxSets, nodeIndex)                                                              \
    {                                                                                                                     \
        ROOT_PARAM_##freq, (maxSets), (nodeIndex), (sizeof(SRT_##srt::freq) / sizeof(Descriptor)), SRT_##srt::freq##Ptr() \
    }
#define DECL_SAMPLER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq##_SAMPLER, ) DESCRIPTOR_TYPE_SAMPLER, 1, SRT_OFFSET(1) };
#define DECL_CBUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, SRT_OFFSET(1) };
#define DECL_TEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) DESCRIPTOR_TYPE_TEXTURE, 1, SRT_OFFSET(1) };

#define DECL_ARRAY_TEXTURES(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) DESCRIPTOR_TYPE_TEXTURE, count, SRT_OFFSET(count) };

#define DECL_ARRAY_RWTEXTURES(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, count, SRT_OFFSET(count) };

#define DECL_BUFFER(freq, type, name) const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) type, 1, SRT_OFFSET(1) };
#define DECL_RAYTRACING(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) type, 1, SRT_OFFSET(1) };
#define DECL_RWTEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, 1, SRT_OFFSET(1) };
#define DECL_WTEXTURE(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) DESCRIPTOR_TYPE_RW_TEXTURE, 1, SRT_OFFSET(1) };
#define DECL_RWBUFFER(freq, type, name) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) DESCRIPTOR_TYPE_RW_BUFFER, 1, SRT_OFFSET(1) };

#define DECL_ARRAY_RWBUFFERS(freq, type, name, count) \
    const Descriptor name = { IF_VALIDATE_DESCRIPTOR(#name, ROOT_PARAM_##freq, ) type, count, SRT_OFFSET(count) };

#else

#define MAKE_REGISTER(prefix, register_index) prefix##register_index
#define MAKE_SPACE(space_index)               space##space_index
#define MAKE_REGISTER_SPACE(prefix, register_index, space_index) : register(MAKE_REGISTER(prefix, register_index), MAKE_SPACE(space_index))

/************************************************************************/
// Root signature macros
/************************************************************************/
#define RS_STATIC_SAMPLER(type, name, reg, space, filter, mip, aniso, addressU, addressV, addressW, cmp) \
    type name MAKE_REGISTER_SPACE(s, reg, space);
#define ROOT_SIGNATURE(rs)                             [RootSignature(rs)]
/************************************************************************/
// SRT macros
/************************************************************************/
#define DECL_SAMPLER(freq, type, name)                 type name MAKE_REGISTER_SPACE(s, 0, SET_##freq);
#define DECL_CBUFFER(freq, type, name)                 type name MAKE_REGISTER_SPACE(b, 0, SET_##freq);
#define DECL_TEXTURE(freq, type, name)                 type name MAKE_REGISTER_SPACE(t, 0, SET_##freq);

#define DECL_ARRAY_TEXTURES(freq, type, name, count)   type name[count] MAKE_REGISTER_SPACE(t, 0, SET_##freq);

#define DECL_BUFFER(freq, type, name)                  type name MAKE_REGISTER_SPACE(t, 0, SET_##freq);
#define DECL_RAYTRACING(freq, type, name)              type name MAKE_REGISTER_SPACE(t, 0, SET_##freq);
#define DECL_RWTEXTURE(freq, type, name)               type name MAKE_REGISTER_SPACE(u, 0, SET_##freq);
#define DECL_ARRAY_RWTEXTURES(freq, type, name, count) type name[count] MAKE_REGISTER_SPACE(u, 0, SET_##freq);
#define DECL_WTEXTURE(freq, type, name)                type name MAKE_REGISTER_SPACE(u, 0, SET_##freq);
#define DECL_RWBUFFER(freq, type, name)                type name MAKE_REGISTER_SPACE(u, 0, SET_##freq);
#define DECL_ARRAY_RWBUFFERS(freq, type, name, count)  type name[count] MAKE_REGISTER_SPACE(u, 0, SET_##freq);

#endif
