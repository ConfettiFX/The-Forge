/*
 * Copyright (c) 2019 Confetti Interactive Inc.
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


layout(location = 0) in vec3 POSITION;

layout(set = 0, binding = 0) uniform cbCamera
{
	mat4 projView;
};

layout(set = 2, binding = 1) uniform cbObject
{
	mat4 worldMat;
};

struct VSInput
{
	vec3 Position;
};
struct VSOutput
{
	vec4 Position;
};
VSOutput HLSLmain(VSInput input0)
{
	VSOutput result;
	mat4 mvp = ((projView)*(worldMat));
	((result).Position = ((mvp)*(vec4(((input0).Position).xyz, 1.0))));
	return result;
}
void main()
{
	VSInput input0;
	input0.Position = POSITION;
	VSOutput result = HLSLmain(input0);
	gl_Position = result.Position;
}
