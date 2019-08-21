#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif


/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#if VK_EXT_DESCRIPTOR_INDEXING_ENABLED
	#extension GL_EXT_nonuniform_qualifier : enable
#endif

#extension GL_GOOGLE_include_directive : enable

#include "Shader_Defs.h"
#include "non_uniform_resource_index.h"

uint calculateOutputVBID(bool opaque, uint drawID, uint primitiveID)
{
	uint drawID_primID = ((drawID << 23) & 0x7F800000) | (primitiveID & 0x007FFFFF);
	return (opaque) ? drawID_primID : (1 << 31) | drawID_primID;
}

layout(set = 1, binding = 1) restrict readonly buffer indirectMaterialBuffer
{
	uint indirectMaterialBufferData[];
};
layout(set = 0, binding = 2) uniform texture2D diffuseMaps[MAX_TEXTURE_UNITS];
layout(set = 0, binding = 3) uniform sampler nearClampSampler;


layout(location = 0) in vec2 iTexCoord;
layout(location = 1) in flat uint iDrawId;

layout(location = 0) out vec4 oColor;

void main()
{
	uint matBaseSlot = BaseMaterialBuffer(true, 1); //1 is camera view, 0 is shadow map view
	uint materialID = indirectMaterialBufferData[matBaseSlot + iDrawId];
	vec4 texColor = texture(sampler2D(diffuseMaps[ NonUniformResourceIndex(materialID)], nearClampSampler), iTexCoord);

	if (texColor.a < 0.5f)
	{
		discard;
		return;
	}

	oColor = unpackUnorm4x8(calculateOutputVBID(false, iDrawId, gl_PrimitiveID));
}