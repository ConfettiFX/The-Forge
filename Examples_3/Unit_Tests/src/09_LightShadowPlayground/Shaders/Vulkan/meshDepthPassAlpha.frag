/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif


#extension GL_GOOGLE_include_directive : enable

#include "Shader_Defs.h"
#include "Packing.h"
#include "non_uniform_resource_index.h"

#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
	#extension GL_EXT_nonuniform_qualifier : enable
#endif


layout(location = 0) in vec2 fragInput_TEXCOORD0;
layout(location = 1) in flat uint oDrawId;

struct PsIn
{
    vec4 Position;
    vec2 TexCoord;
};

layout(row_major, push_constant) uniform indirectRootConstant_Block
{
    uint drawId;
} indirectRootConstant;

layout(row_major, UPDATE_FREQ_PER_FRAME, binding = 1) buffer indirectMaterialBuffer
{
    uint indirectMaterialBuffer_Data[];
};

layout(UPDATE_FREQ_NONE, binding = 2) uniform texture2D diffuseMaps[256];
layout(UPDATE_FREQ_NONE, binding = 3) uniform sampler nearClampSampler;

void HLSLmain(PsIn In)
{
    uint matBaseSlot = BaseMaterialBuffer(true, uint (0));
    uint materialID = indirectMaterialBuffer_Data[(matBaseSlot + oDrawId)];
    vec4 texColor = textureLod(sampler2D( diffuseMaps[NonUniformResourceIndex(materialID)], nearClampSampler), vec2((In).TexCoord), 0);
    
	if(texColor.a < 0.5)
	{
		discard;
	}
	
	//code below is from shader translator
	//clip1(float (((((texColor).a < 0.5))?(float ((-1))):(float (1)))));
}
void main()
{
    PsIn In;
    In.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    In.TexCoord = fragInput_TEXCOORD0;
    HLSLmain(In);
}
