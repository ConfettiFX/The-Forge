/*
 * Copyright (c) 2018 Confetti Interactive Inc.
 *
 * This file is part of TheForge
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

//
//  shading.mh.metal
//  Visibility_Buffer
//

#ifndef SHADING_METAL
#define SHADING_METAL

#include <metal_stdlib>
using namespace metal;

#define SHADOW_PCF 0
#define SHADOW_ESM 1
#define SHADOW SHADOW_ESM

#if SHADOW == SHADOW_PCF
constant int NUM_SHADOW_SAMPLES = 32;
constant float NUM_SHADOW_SAMPLES_INV = 0.03125;
constant float shadowSamples[NUM_SHADOW_SAMPLES*2] = {
    -0.1746646, -0.7913184,
    -0.129792, -0.4477116,
    0.08863912, -0.898169,
    -0.5891499, -0.6781639,
    0.1748409, -0.5252063,
    0.6483325, -0.752117,
    0.4529319, -0.384986,
    0.09757467, -0.1166954,
    0.3857658, -0.9096935,
    0.5613058, -0.1283066,
    0.768011, -0.4906538,
    0.8499438, -0.220937,
    0.6946555, 0.1605866,
    0.9614297, 0.05975229,
    0.7986544, 0.5325912,
    0.4513965, 0.5592551,
    0.2847693, 0.2293397,
    -0.2118996, -0.1609127,
    -0.4357893, -0.3808875,
    -0.4662672, -0.05288446,
    -0.139129, 0.2394065,
    0.1781853, 0.5254948,
    0.4287854, 0.899425,
    0.1289349, 0.8724155,
    -0.6924323, -0.2203967,
    -0.48997, 0.2795907,
    -0.2611724, 0.7359962,
    -0.7704172, 0.4233134,
    -0.850104, 0.1263935,
    -0.8345267, -0.4991361,
    -0.5380967, 0.6264234,
    -0.9769312, -0.1550569
};
#endif

float random(float3 seed, float3 freq)
{
    // project seed on random constant vector
    float dt = dot(floor(seed*freq), float3(53.1215, 21.1352, 9.1322));
    // return only the fractional part
    return fract(sin(dt)*2105.2354);
}

float3 calculateSpecular(float3 specularColor, float3 camPos, float3 pixelPos, float3 normalizedDirToLight, float3 normal)
{
    float3 viewVec = normalize(camPos - pixelPos);
    float3 halfVec = normalize(viewVec + normalizedDirToLight);
    float specIntensity = 128;
    float specular = pow(saturate(dot(halfVec, normal)), specIntensity);
    return specularColor * specular;
}

float3 calculateIllumination(float3 normal, float3 camPos, float esmControl, float3 normalizedDirToLight, bool isTwoSided, float4 posLS, float3 position, depth2d<float,access::sample> shadowMap, float3 albedo, float3 specularColor, float ao, sampler sh)
{
    float nDotL = dot(normal, -normalizedDirToLight);
    
    // Deal with two faced materials
    nDotL = (isTwoSided ? abs(nDotL) : saturate(nDotL));
    
    // Project pixel position post-perspective division coordinates and map to [0..1] range to access the shadow map
    posLS /= posLS.w;
    posLS.y *= -1;
    posLS.xy = posLS.xy * 0.5 + float2(0.5, 0.5);
    float shadowFactor = 0.0f;
    
    if (all(posLS.xy > 0) && all(posLS.xy < 1))
    {
#if SHADOW == SHADOW_PCF
        // waste of shader cycles
        // Perform percentage-closer shadows with randomly rotated poisson kernel
        float shadowFilterSize = 0.0016;
        float angle = random(position, 20.0);
        float s = sin(angle);
        float c = cos(angle);
        
        for (int i = 0; i < NUM_SHADOW_SAMPLES; i++)
        {
            float2 offset = float2(shadowSamples[i * 2], shadowSamples[i * 2 + 1]);
            offset = float2(offset.x * c + offset.y * s, offset.x * -s + offset.y * c);
            offset *= shadowFilterSize;
            float shadowMapValue = shadowMap.sample(sh, posLS.xy + offset, level(0.0f));
            shadowFactor += (shadowMapValue < posLS.z - 0.002 ? 0 : 1);
        }
        shadowFactor *= NUM_SHADOW_SAMPLES_INV;
#elif SHADOW == SHADOW_ESM
        // ESM
        float4 shadowDepthSample = float4(0, 0, 0, 0);
        shadowDepthSample.x = shadowMap.sample(sh, posLS.xy, level(0.0f));
        shadowDepthSample.y = shadowMap.sample(sh, posLS.xy, level(0.0f), int2(1, 0));
        shadowDepthSample.z = shadowMap.sample(sh, posLS.xy, level(0.0f), int2(0, 1));
        shadowDepthSample.w = shadowMap.sample(sh, posLS.xy, level(0.0f), int2(1, 1));
        float avgShadowDepthSample = (shadowDepthSample.x + shadowDepthSample.y + shadowDepthSample.z + shadowDepthSample.w) * 0.25f;
        shadowFactor = saturate(2.0 - exp((posLS.z - avgShadowDepthSample) * esmControl));
#endif
    }
    float3 lightColor = (3.0f * float3(255.f / 255.f, 220.f / 255.f, 150 / 255.f));
    // Ambient + Diffuse + Specular
    // Ambient = 0.15 * albedo
    // Diffuse = DiffuseColor * albedo * N.L * SUN_COLOR
    // Specular = SpecColor * N.N^n
    
    
    float3 Ambient = 0.15 * albedo * ao;
    float3 Diffuse = nDotL * albedo * lightColor;
    float3 Specular = calculateSpecular(specularColor, camPos, position, -normalizedDirToLight, normal);
    
    float3 finalColor = ((Diffuse + Specular) * shadowFactor) + Ambient;
    
    return finalColor;
}

float3 pointLightShade(float3 lightPos, float3 lightCol, float3 camPos, float3 pixelPos, float3 normal, float3 specularColor, bool isTwoSided)
{
    float3 lVec = (lightPos - pixelPos) * (1.0 / LIGHT_SIZE);
    float3 lightVec = normalize(lVec);
    float atten = saturate(1.0f - dot(lVec, lVec));
    
    float nDotL = dot(lightVec, normal);
    
    // Deal with two faced materials
    nDotL = (isTwoSided ? abs(nDotL) : saturate(nDotL));
    
    // Compute specular
    float3 specularFactor = calculateSpecular(specularColor, camPos, pixelPos, lightVec, normal);
    return lightCol * atten * (nDotL + specularFactor);
}

#endif

