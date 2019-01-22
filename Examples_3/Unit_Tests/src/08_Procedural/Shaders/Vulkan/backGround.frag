
#version 450 core

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

/*
layout (std140, set=0, binding=0) uniform cbCamera {
	uniform mat4 projView;
	uniform vec3 camPos;
	uniform float pad_0;
};
*/

layout (std140, set=0, binding=0) uniform cbObject {
	uniform mat4 worldMat;
	uniform mat4 invWorldMat;

	uniform vec4 u_OceanColor;
	uniform vec4 u_ShorelineColor;
	uniform vec4 u_FoliageColor;
	uniform vec4 u_MountainsColor;

	uniform vec4 u_SnowColor;
	uniform vec4 u_PolarCapsColor;
	uniform vec4 u_AtmosphereColor;
	uniform vec4 u_HeightsInfo; // x : Ocean, y : Shore, z : Snow, w : Polar

	uniform vec4 u_TimeInfo; //time, controls.Noise4D, controls.TerrainExp, controls.TerrainSeed * 39.0
};

layout (std140, set=1, binding=0) uniform cbScreen {

	uniform vec4 u_screenSize;
};

float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }
float noise(float x) { float i = floor(x); float f = fract(x); float u = f * f * (3.0 - 2.0 * f); return mix(hash(i), hash(i + 1.0), u); }
float noise(vec2 x) { vec2 i = floor(x); vec2 f = fract(x); float a = hash(i); float b = hash(i + vec2(1.0, 0.0)); float c = hash(i + vec2(0.0, 1.0)); float d = hash(i + vec2(1.0, 1.0)); vec2 u = f * f * (3.0 - 2.0 * f); return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y; }

float rand(vec2 co){
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

layout(location = 0) in vec2 fs_UV;

//refer to Morgan McGuire's Earth-like Tiny Planet
vec3 addStars(vec2 screenSize)
{
    float time = u_TimeInfo.x;

    // Background starfield
    float galaxyClump = (pow(noise(fs_UV.xy * (30.0 * screenSize.x)), 3.0) * 0.5 + pow(noise(100.0 + fs_UV.xy * (15.0 * screenSize.x)), 5.0)) / 3.5;
    
    vec3 starColor = vec3(galaxyClump * pow(hash(fs_UV.xy), 1500.0) * 80.0);

    starColor.x *= sqrt(noise(fs_UV.xy) * 1.2);
    starColor.y *= sqrt(noise(fs_UV.xy * 4.0));

    vec2 delta = (fs_UV.xy - screenSize.xy * 0.5) * screenSize.y * 1.2;  
    float radialNoise = mix(1.0, noise(normalize(delta) * 20.0 + time * 0.5), 0.12);

    float att = 0.057 * pow(max(0.0, 1.0 - (length(delta) - 0.9) / 0.9), 8.0);

    starColor += radialNoise * u_AtmosphereColor.xyz * min(1.0, att);

    float randSeed = rand(fs_UV);

    return starColor *  (( sin(randSeed + randSeed * time* 0.05) + 1.0)* 0.4 + 0.2);
}



layout(location = 0) out vec4 out_Col;

vec2 getScreenSpaceCoords( vec2 NDC ) 
{
    return vec2((NDC.x + 1.0) * 0.5, (1.0 - NDC.y) * 0.5);
}

vec2 getNDC( vec2 ssc ) 
{
    return vec2((ssc.x * 2.0) - 1.0, 1.0 - (ssc.y * 2.0));
}

void main()
{
    out_Col = vec4(0.0, 0.0, 0.0, 1.0);

    vec2 screenSize = u_screenSize.xy;

    // Background stars
    out_Col.xyz += addStars(screenSize);
    
	/*

	vec4 planetPos_ss = projView * vec4(0.0, 0.0, 0.0, 1.0);
    planetPos_ss /= planetPos_ss.w;

    float radius = 3.0;

	vec3 upVector = vec3(u_ShorelineColor.x, u_OceanColor.z, u_OceanColor.w);
    vec3 pinPoint = upVector * radius;

    vec4 planetPolarPos_ss = projView * vec4(pinPoint, 1.0);
    planetPolarPos_ss /= planetPolarPos_ss.w;

    float radius_ss = abs(planetPolarPos_ss.y - planetPos_ss.y);

    vec2 NDC = getNDC(fs_UV);

    float screenRatio = screenSize.x / screenSize.y;
    NDC.x *= screenRatio;
    NDC /= radius_ss;

    vec2 gap = NDC - planetPos_ss.xy;  

    float dist = length(camPos.xyz);
    dist = planetPos_ss.z;

    planetPos_ss.x *= screenRatio; 
    planetPos_ss /= radius_ss;
    
	
    float halo = clamp(1.0 - sqrt(gap.x*gap.x + gap.y*gap.y), 0.0, 1.0);
    halo = pow(halo, 0.5);        
    halo = pow(halo, 3.5);  
	

    out_Col += clamp(vec4(u_AtmosphereColor.xyz * halo * 4.0, 1.0), 0.0, 1.0);
	*/
}
