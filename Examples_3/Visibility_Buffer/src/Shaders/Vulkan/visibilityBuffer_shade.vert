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


layout(location = 0) out vec2 oScreenPos;

// Vertex shader
void main()
{
	// Produce a fullscreen triangle using the current vertexId
	// to automatically calculate the vertex porision. This
	// method avoids using vertex/index buffers to generate a
	// fullscreen quad.
	vec4 position;
	position.x = (gl_VertexIndex == 2) ?  3.0f : -1.0f;
	position.y = (gl_VertexIndex == 0) ? -3.0f :  1.0f;
	position.zw = vec2(0.0f, 1.0f);

	oScreenPos = position.xy;
	gl_Position = position;
}