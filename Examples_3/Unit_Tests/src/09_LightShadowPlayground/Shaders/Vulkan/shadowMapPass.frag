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
layout(location = 0) in float WDepth;

layout(location = 0) out float DepthExp;

layout (set=0, binding=2) uniform ESMInputConstants {
    vec2 ScreenDimension;
    vec2 NearFarDist;
    float Exponent;
    uint BlurWidth;
    int IfHorizontalBlur;
    int padding;
};

float map_01(float x, float v0, float v1)
{
    return (x - v0) / (v1 - v0);
}

void main()
{
    float mappedDepth = map_01(WDepth, NearFarDist.x, NearFarDist.y);
    DepthExp = exp(Exponent * mappedDepth);
}