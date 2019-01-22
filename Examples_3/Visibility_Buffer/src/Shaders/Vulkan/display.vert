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
#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif



layout(location = 0) out vec2 vertOutput_TEXCOORD;

struct PsIn
{
    vec4 position;
    vec2 texCoord;
};
PsIn HLSLmain(uint VertexID)
{
    PsIn Out;
    vec4 position;
    ((position).x = float ((((VertexID == uint (2)))?(float (3.0)):(float ((-1.0))))));
    ((position).y = float ((((VertexID == uint (0)))?(float ((-3.0))):(float (1.0)))));
    ((position).zw = vec2 (1.0));
    ((Out).position = position);
    ((Out).texCoord = (((position).xy * vec2(0.5, (-0.5))) + vec2 (0.5)));
    return Out;
}
void main()
{
    uint VertexID;
    VertexID = gl_VertexIndex;
    PsIn result = HLSLmain(VertexID);
    gl_Position = result.position;
    vertOutput_TEXCOORD = result.texCoord;
}
