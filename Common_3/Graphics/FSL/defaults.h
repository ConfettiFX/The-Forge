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

#include "fsl_srt.h"

#define SET_Persistent 0
#define SET_PerFrame   1
#define SET_PerBatch   2
#define SET_PerDraw    3

#if defined(DIRECT3D12)
#define DESCRIPTOR_TABLE(space)                                            \
    "DescriptorTable("                                                     \
    "SRV(t0, numDescriptors = unbounded, space = " #space ", offset = 0)," \
    "CBV(b0, numDescriptors = unbounded, space = " #space ", offset = 0)," \
    "UAV(u0, numDescriptors = unbounded, space = " #space ", offset = 0)),"

#define SAMPLER_DESCRIPTOR_TABLE(space) \
    "DescriptorTable("                  \
    "SAMPLER(s0, numDescriptors = unbounded, space = " #space ", offset = 0)),"

#define DefaultRootSignature                                                                                                         \
    "RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT)," DESCRIPTOR_TABLE(3) DESCRIPTOR_TABLE(2) DESCRIPTOR_TABLE(1) DESCRIPTOR_TABLE(0) \
        SAMPLER_DESCRIPTOR_TABLE(                                                                                                    \
            0) "StaticSampler(s0, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_POINT,"                                                                                  \
               "addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"              \
               "StaticSampler(s1, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_POINT,"                                                                                  \
               "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP),"                 \
               "StaticSampler(s2, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,"                                                                           \
               "addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"              \
               "StaticSampler(s3, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,"                                                                           \
               "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP),"                 \
               "StaticSampler(s4, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_LINEAR,"                                                                                 \
               "addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"              \
               "StaticSampler(s5, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_LINEAR,"                                                                                 \
               "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP),"                 \
               "StaticSampler(s6, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_POINT,"                                                                                  \
               "addressU = TEXTURE_ADDRESS_MIRROR, addressV = TEXTURE_ADDRESS_MIRROR, addressW = TEXTURE_ADDRESS_MIRROR),"           \
               "StaticSampler(s7, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_POINT, borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK,"                             \
               "addressU = TEXTURE_ADDRESS_BORDER, addressV = TEXTURE_ADDRESS_BORDER, addressW = TEXTURE_ADDRESS_BORDER),"           \
               "StaticSampler(s8, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_LINEAR,"                                                                                 \
               "addressU = TEXTURE_ADDRESS_MIRROR, addressV = TEXTURE_ADDRESS_MIRROR, addressW = TEXTURE_ADDRESS_MIRROR),"           \
               "StaticSampler(s9, space = 100,"                                                                                      \
               "filter = FILTER_MIN_MAG_MIP_LINEAR, borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK,"                            \
               "addressU = TEXTURE_ADDRESS_BORDER, addressV = TEXTURE_ADDRESS_BORDER, addressW = TEXTURE_ADDRESS_BORDER),"           \
               "StaticSampler(s10, space = 100,"                                                                                     \
               "filter = FILTER_ANISOTROPIC, maxAnisotropy = 8,"                                                                     \
               "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP)"

#define ComputeRootSignature                                                                                    \
    DESCRIPTOR_TABLE(3)                                                                                         \
    DESCRIPTOR_TABLE(2)                                                                                         \
    DESCRIPTOR_TABLE(1)                                                                                         \
    DESCRIPTOR_TABLE(0)                                                                                         \
    SAMPLER_DESCRIPTOR_TABLE(0)                                                                                 \
    "StaticSampler(s0, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_POINT,"                                                                        \
    "addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"    \
    "StaticSampler(s1, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_POINT,"                                                                        \
    "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP),"       \
    "StaticSampler(s2, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,"                                                                 \
    "addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"    \
    "StaticSampler(s3, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_LINEAR_MIP_POINT,"                                                                 \
    "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP),"       \
    "StaticSampler(s4, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_LINEAR,"                                                                       \
    "addressU = TEXTURE_ADDRESS_CLAMP, addressV = TEXTURE_ADDRESS_CLAMP, addressW = TEXTURE_ADDRESS_CLAMP),"    \
    "StaticSampler(s5, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_LINEAR,"                                                                       \
    "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP),"       \
    "StaticSampler(s6, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_POINT,"                                                                        \
    "addressU = TEXTURE_ADDRESS_MIRROR, addressV = TEXTURE_ADDRESS_MIRROR, addressW = TEXTURE_ADDRESS_MIRROR)," \
    "StaticSampler(s7, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_POINT, borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK,"                   \
    "addressU = TEXTURE_ADDRESS_BORDER, addressV = TEXTURE_ADDRESS_BORDER, addressW = TEXTURE_ADDRESS_BORDER)," \
    "StaticSampler(s8, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_LINEAR,"                                                                       \
    "addressU = TEXTURE_ADDRESS_MIRROR, addressV = TEXTURE_ADDRESS_MIRROR, addressW = TEXTURE_ADDRESS_MIRROR)," \
    "StaticSampler(s9, space = 100,"                                                                            \
    "filter = FILTER_MIN_MAG_MIP_LINEAR, borderColor = STATIC_BORDER_COLOR_TRANSPARENT_BLACK,"                  \
    "addressU = TEXTURE_ADDRESS_BORDER, addressV = TEXTURE_ADDRESS_BORDER, addressW = TEXTURE_ADDRESS_BORDER)," \
    "StaticSampler(s10, space = 100,"                                                                           \
    "filter = FILTER_ANISOTROPIC, maxAnisotropy = 8,"                                                           \
    "addressU = TEXTURE_ADDRESS_WRAP, addressV = TEXTURE_ADDRESS_WRAP, addressW = TEXTURE_ADDRESS_WRAP)"

#define ROOT_PARAM_Persistent_SAMPLER 4
#define ROOT_PARAM_Persistent         3
#define ROOT_PARAM_PerFrame           2
#define ROOT_PARAM_PerBatch           1
#define ROOT_PARAM_PerDraw            0
#endif

#if defined(VULKAN)
#define VK_STATIC_SAMPLER_SHIFT  100
#define VK_DYNAMIC_SAMPLER_SHIFT 200

#define CAT_(a, b)               a##b
#define CAT(a, b)                CAT_(a, b)
#define VARNAME(Var)             CAT(Var, __LINE__)
#define PIPELINE_LAYOUT_DESC(pipelineDesc, none, perframe, perbatch, perdraw)                                                       \
    const DescriptorSetLayoutDesc* VARNAME(pipelineDesc)[4] = { none ? none : &gStaticSamplerLayout, perframe, perbatch, perdraw }; \
    pipelineDesc.pLayouts = VARNAME(pipelineDesc);                                                                                  \
    pipelineDesc.mLayoutCount = TF_ARRAY_COUNT(VARNAME(pipelineDesc));
#else
#define PIPELINE_LAYOUT_DESC(...)
#endif

#if defined(ORBIS) || defined(PROSPERO)

#define ROOT_SIGNATURE_DEFINITION \
    struct set0* p0;              \
    struct set1* p1;              \
    struct set2* p2;              \
    struct set3* p3;

#endif

// Static samplers
BEGIN_RS_STATIC_SAMPLERS()
RS_STATIC_SAMPLER(SamplerState, gSamplerPointClamp, 0, 100, FILTER_NEAREST, MIPMAP_MODE_NEAREST, 1, ADDRESS_MODE_CLAMP_TO_EDGE,
                  ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerPointWrap, 1, 100, FILTER_NEAREST, MIPMAP_MODE_NEAREST, 1, ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT,
                  ADDRESS_MODE_REPEAT, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerBilinearClamp, 2, 100, FILTER_LINEAR, MIPMAP_MODE_NEAREST, 1, ADDRESS_MODE_CLAMP_TO_EDGE,
                  ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerBilinearWrap, 3, 100, FILTER_LINEAR, MIPMAP_MODE_NEAREST, 1, ADDRESS_MODE_REPEAT,
                  ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerTrilinearClamp, 4, 100, FILTER_LINEAR, MIPMAP_MODE_LINEAR, 1, ADDRESS_MODE_CLAMP_TO_EDGE,
                  ADDRESS_MODE_CLAMP_TO_EDGE, ADDRESS_MODE_CLAMP_TO_EDGE, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerTrilinearWrap, 5, 100, FILTER_LINEAR, MIPMAP_MODE_LINEAR, 1, ADDRESS_MODE_REPEAT,
                  ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerPointMirror, 6, 100, FILTER_NEAREST, MIPMAP_MODE_NEAREST, 1, ADDRESS_MODE_MIRROR,
                  ADDRESS_MODE_MIRROR, ADDRESS_MODE_MIRROR, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerPointBorder, 7, 100, FILTER_NEAREST, MIPMAP_MODE_NEAREST, 1, ADDRESS_MODE_CLAMP_TO_BORDER,
                  ADDRESS_MODE_CLAMP_TO_BORDER, ADDRESS_MODE_CLAMP_TO_BORDER, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerTrilinearMirror, 8, 100, FILTER_LINEAR, MIPMAP_MODE_LINEAR, 1, ADDRESS_MODE_MIRROR,
                  ADDRESS_MODE_MIRROR, ADDRESS_MODE_MIRROR, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerTrilinearBorder, 9, 100, FILTER_LINEAR, MIPMAP_MODE_LINEAR, 1, ADDRESS_MODE_CLAMP_TO_BORDER,
                  ADDRESS_MODE_CLAMP_TO_BORDER, ADDRESS_MODE_CLAMP_TO_BORDER, CMP_NEVER)
RS_STATIC_SAMPLER(SamplerState, gSamplerAnisotropic, 10, 100, FILTER_LINEAR, MIPMAP_MODE_LINEAR, 8, ADDRESS_MODE_REPEAT,
                  ADDRESS_MODE_REPEAT, ADDRESS_MODE_REPEAT, CMP_NEVER)

END_RS_STATIC_SAMPLERS()
//
