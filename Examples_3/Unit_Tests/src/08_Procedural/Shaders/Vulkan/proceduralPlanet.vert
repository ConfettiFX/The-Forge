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


layout(location = 0) in vec3 vs_Pos;
layout(location = 1) in vec3 vs_Nor;


layout (std140, set=0, binding=0) uniform cbCamera {
	uniform mat4 projView;
	uniform vec3 camPos;
	uniform float pad_0;
};

layout (std140, set=1, binding=0) uniform cbObject {

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

struct Light
{
	vec4 pos;
	vec4 col;
	float radius;
	float intensity;
};

layout (std140, set=2, binding=0) uniform cbLights {
	int currAmountOfLights;
	int pad0;
	int pad1;
	int pad2;
	Light lights[16];
};

//Noise Generator, refer to "Implicit�Procedural�Planet�Generation Report" 
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float hash(float n)
{    
    //4D
    if(u_TimeInfo.y > 0.0 )
    {
        return fract( sin(n) *cos( u_TimeInfo.x * 0.00001) * 1e4);
    }
    else
    {
        return fract(sin(n) * cos( u_TimeInfo.w * 0.00001) * 1e4);
    }
   
}

float noise(vec3 x)
{   
    vec3 step = vec3(110, 241, 171);
    vec3 i = floor(x); 
    vec3 f = fract(x);
    float n = dot(i, step);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
     mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
     mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
     mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

#define Epsilon 0.0001

#define OCTAVES 6
float fbm(vec3 x)
{
  float v = 0.0;
  float a = 0.5;
  vec3 shift = vec3(100.0);

  for (int i = 0; i < OCTAVES; ++i)
  {
   v += a * noise(x);
   x = x * 2.0 + shift;
   a *= 0.5;
  }  
  return v;
}


vec3 getTerrainPos(vec3 worldPos, float resolution)
{
    vec3 localNormal = normalize(worldPos);
    return worldPos + localNormal * fbm(worldPos*resolution);
}

float OceanNoise(vec3 vertexPos, float oceneHeight, float noiseResult, float blendFactor)
{
    float relativeWaterDepth = min(1.0, (oceneHeight - noiseResult) * 15.0);

    float oceanTime = u_TimeInfo.x * 0.03;

    float shallowWaveRefraction = 4.0;
    float waveMagnitude = 0.0002;
    float waveLength = mix(0.007, 0.0064, blendFactor);

    float shallowWavePhase = (vertexPos.y - noiseResult * shallowWaveRefraction) * (1.0 / waveLength);
    float deepWavePhase    = (atan(vertexPos.z, vertexPos.x) + noise(vertexPos.xyz * 15.0) * 0.075) * (1.5 / waveLength);
    return (cos(shallowWavePhase + oceanTime  * 1.5) * sqrt(1.0 - relativeWaterDepth) + cos(deepWavePhase + oceanTime  * 2.0) * 2.5 * (1.0 - abs(vertexPos.y)) * (relativeWaterDepth * relativeWaterDepth)) * waveMagnitude;
}


layout(location = 0) out vec4 fs_Pos;
layout(location = 1) out vec4 fs_Nor;            
layout(location = 2) out vec4 fs_Col;           
layout(location = 3) out vec4 fs_TerrainInfo;   
layout(location = 4) out vec4 fs_transedPos;

void main ()
{
    fs_TerrainInfo = vec4(0.0);

    vec4 vertexPos = vec4(vs_Pos, 1.0);
    fs_Pos = vertexPos;

    float oceneHeight = length(vertexPos.xyz) + u_HeightsInfo.x;
    vec3 localNormal = normalize(vertexPos.xyz);

    float u_resolution = 4.0;

    float noiseResult = fbm(vertexPos.xyz*u_resolution) * 2.0;
  
    noiseResult = pow(noiseResult, u_TimeInfo.z);

    vertexPos.xyz += localNormal * noiseResult;

    float height = length(vertexPos.xyz);

    float gap = clamp((1.0 - (oceneHeight - height)), 0.0, 1.0);
    float gap5 = pow(gap, 3.0);

    

    vec4 ocenColor = u_OceanColor  * gap5;

    float oceneRougness = 0.15;
    float iceRougness = 0.15;
    float foliageRougness = 0.8;
    float snowRougness = 0.8;
    float shoreRougness = 0.9;

    //ocean
    if(height < oceneHeight)
    {
        //float gap10 = pow(pow(gap, 100.0), 0.8);
        //float wave = OceanNoise(vertexPos.xyz, oceneHeight, noiseResult, gap10);  
        //vertexPos.xyz = (oceneHeight + wave) * localNormal;
		vertexPos.xyz = oceneHeight * localNormal;

		fs_Pos = vec4(vertexPos.xyz, 1.0);        
        fs_TerrainInfo.w = oceneRougness;
        fs_Col = ocenColor;
    }
    //shore
    else
    {
        fs_TerrainInfo.x = 0.05;

        float appliedAttitude;
        
        if(abs(vertexPos.y) > u_HeightsInfo.w)
            appliedAttitude = clamp((abs(vertexPos.y) - u_HeightsInfo.w) * 3.0, 0.0, 1.0);
        else        
            appliedAttitude = 0.0;

        vec4 terrainColor = mix(u_FoliageColor, u_PolarCapsColor, appliedAttitude);
        float terrainRoughness = mix(foliageRougness, iceRougness, appliedAttitude);

        vertexPos.xyz = height * localNormal;

        float oceneLine = oceneHeight + u_HeightsInfo.y;
        float snowLine = 1.0 + u_HeightsInfo.z;

        if(height < oceneLine)
        {
            fs_Col = u_ShorelineColor;
            fs_TerrainInfo.w = shoreRougness;
        }
        else if(height >= snowLine)
        {
            fs_TerrainInfo.x = 0.15;

            float alpha = clamp( (height - snowLine ) / 0.03, 0.0, 1.0);
            fs_Col = mix(terrainColor, u_SnowColor, alpha);

            fs_TerrainInfo.w = mix(terrainRoughness, snowRougness, alpha);
        }        
        else
        {
            float alpha = clamp( (height - oceneLine ) / u_HeightsInfo.y, 0.0, 1.0);
            fs_Col = mix(u_ShorelineColor, terrainColor, alpha);

            fs_TerrainInfo.w = mix(shoreRougness, terrainRoughness, alpha);
        }
    }
   
    vec4 modelposition = worldMat * vertexPos;

	fs_transedPos = modelposition;	

    gl_Position = projView * modelposition;
}

