/*
 * Copyright (c) 2018 Kostas Anagnostou (https://twitter.com/KostasAAA).
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
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    texture2d<float> sceneTexture;
    sampler clampMiplessLinearSampler;
    struct Uniforms_FXAARootConstant
    {
        float2 ScreenSize;
        uint Use;
        uint padding00;
    };
    constant Uniforms_FXAARootConstant & FXAARootConstant;
    float rgb2luma(float3 rgb)
    {
        return sqrt(dot(rgb, float3((float)0.299, (float)0.587, (float)0.114)));
    };
    float3 FXAA(float2 UV, int2 Pixel)
    {
        float QUALITY[12] = { 0.0, 0.0, 0.0, 0.0, 0.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0 };
        float3 colorCenter = (float3)(sceneTexture.read(uint2((Pixel).x, (Pixel).y) + uint2(0, 0).xy).rgb);
        float lumaCenter = rgb2luma(colorCenter);
        float lumaD = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2(0, (-1)).xy)).rgb);
        float lumaU = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2(0, 1).xy)).rgb);
        float lumaL = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2((-1), 0).xy)).rgb);
        float lumaR = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2(1, 0).xy)).rgb);
        float lumaMin = min((float)lumaCenter,(float)min((float)min((float)lumaD,(float)lumaU),(float)min((float)lumaL,(float)lumaR)));
        float lumaMax = max((float)lumaCenter,(float)max((float)max((float)lumaD,(float)lumaU),(float)max((float)lumaL,(float)lumaR)));
        float lumaRange = (lumaMax - lumaMin);
        if ((lumaRange < max((float)0.0312,(float)lumaMax * (float)(0.125))))
        {
            return sceneTexture.sample(clampMiplessLinearSampler, UV).rgb;
        }
        float lumaDL = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2((-1), (-1)).xy)).rgb);
        float lumaUR = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2(1, 1).xy)).rgb);
        float lumaUL = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2((-1), 1).xy)).rgb);
        float lumaDR = rgb2luma(sceneTexture.read(uint2(int2((Pixel).x, (Pixel).y) + int2(1, (-1)).xy)).rgb);
        float lumaDownUp = (lumaD + lumaU);
        float lumaLeftRight = (lumaL + lumaR);
        float lumaLeftCorners = (lumaDL + lumaUL);
        float lumaDownCorners = (lumaDL + lumaDR);
        float lumaRightCorners = (lumaDR + lumaUR);
        float lumaUpCorners = (lumaUR + lumaUL);
        float edgeHorizontal = ((abs((((float)((-2.0)) * lumaL) + lumaLeftCorners)) + (abs((((float)((-2.0)) * lumaCenter) + lumaDownUp)) * (float)(2.0))) + abs((((float)((-2.0)) * lumaR) + lumaRightCorners)));
        float edgeVertical = ((abs((((float)((-2.0)) * lumaU) + lumaUpCorners)) + (abs((((float)((-2.0)) * lumaCenter) + lumaLeftRight)) * (float)(2.0))) + abs((((float)((-2.0)) * lumaD) + lumaDownCorners)));
        float isHorizontal = (((edgeHorizontal >= edgeVertical))?(0.0):(1.0));
        float luma1 = mix(lumaD, lumaL, isHorizontal);
        float luma2 = mix(lumaU, lumaR, isHorizontal);
        float gradient1 = (luma1 - lumaCenter);
        float gradient2 = (luma2 - lumaCenter);
        bool is1Steepest = (abs(gradient1) >= abs(gradient2));
        float gradientScaled = ((float)(0.25) * max((float)abs(gradient1),(float)abs(gradient2)));
        float2 inverseScreenSize = float2(((float)(1.0) / (FXAARootConstant.ScreenSize).x), ((float)(1.0) / (FXAARootConstant.ScreenSize).y));
        float stepLength = mix((inverseScreenSize).y, (inverseScreenSize).x, isHorizontal);
        float lumaLocalAverage = (float)(0.0);
        if (is1Steepest)
        {
            (stepLength = (-stepLength));
            (lumaLocalAverage = ((float)(0.5) * (luma1 + lumaCenter)));
        }
        else
        {
            (lumaLocalAverage = ((float)(0.5) * (luma2 + lumaCenter)));
        }
        float2 currentUv = UV;
        if ((isHorizontal < 0.5))
        {
            ((currentUv).y += (stepLength * (float)(0.5)));
        }
        else
        {
            ((currentUv).x += (stepLength * (float)(0.5)));
        }
        float2 offset = mix(float2((inverseScreenSize).x, (float)0.0), float2((float)0.0, (inverseScreenSize).y), isHorizontal);
        float2 uv1 = (currentUv - offset);
        float2 uv2 = (currentUv + offset);
        float lumaEnd1 = rgb2luma(sceneTexture.sample(clampMiplessLinearSampler, uv1).rgb);
        float lumaEnd2 = rgb2luma(sceneTexture.sample(clampMiplessLinearSampler, uv2).rgb);
        (lumaEnd1 -= lumaLocalAverage);
        (lumaEnd2 -= lumaLocalAverage);
        bool reached1 = (abs(lumaEnd1) >= gradientScaled);
        bool reached2 = (abs(lumaEnd2) >= gradientScaled);
        bool reachedBoth = (reached1 && reached2);
        if ((!reached1))
        {
            (uv1 -= offset);
        }
        if ((!reached2))
        {
            (uv2 += offset);
        }
        if ((!reachedBoth))
        {
            for (int i = 2; (i < 12); (++i))
            {
                if ((!reached1))
                {
                    (lumaEnd1 = rgb2luma(sceneTexture.sample(clampMiplessLinearSampler, uv1).rgb));
                    (lumaEnd1 = (lumaEnd1 - lumaLocalAverage));
                }
                if ((!reached2))
                {
                    (lumaEnd2 = rgb2luma(sceneTexture.sample(clampMiplessLinearSampler, uv2).rgb));
                    (lumaEnd2 = (lumaEnd2 - lumaLocalAverage));
                }
                (reached1 = (abs(lumaEnd1) >= gradientScaled));
                (reached2 = (abs(lumaEnd2) >= gradientScaled));
                (reachedBoth = (reached1 && reached2));
                if ((!reached1))
                {
                    (uv1 -= (offset * (float2)(QUALITY[i])));
                }
                if ((!reached2))
                {
                    (uv2 += (offset * (float2)(QUALITY[i])));
                }
                if (reachedBoth)
                {
                    break;
                }
            }
        }
        float distance1 = mix(((UV).x - (uv1).x), ((UV).y - (uv1).y), isHorizontal);
        float distance2 = mix(((uv2).x - (UV).x), ((uv2).y - (UV).y), isHorizontal);
        bool isDirection1 = (distance1 < distance2);
        float distanceFinal = min((float)distance1,(float)distance2);
        float edgeThickness = (distance1 + distance2);
        float pixelOffset = (((-distanceFinal) / edgeThickness) + (float)(0.5));
        bool isLumaCenterSmaller = (lumaCenter < lumaLocalAverage);
        bool correctVariation = ((((isDirection1)?(lumaEnd1):(lumaEnd2)) < (float)(0.0)) != isLumaCenterSmaller);
        float finalOffset = ((correctVariation)?(pixelOffset):(0.0));
        float lumaAverage = ((float)((1.0 / 12.0)) * ((((float)(2.0) * (lumaDownUp + lumaLeftRight)) + lumaLeftCorners) + lumaRightCorners));
        float subPixelOffset1 = clamp((abs((lumaAverage - lumaCenter)) / lumaRange), 0.0, 1.0);
        float subPixelOffset2 = (((((float)((-2.0)) * subPixelOffset1) + (float)(3.0)) * subPixelOffset1) * subPixelOffset1);
        float subPixelOffsetFinal = ((subPixelOffset2 * subPixelOffset2) * (float)(0.75));
        (finalOffset = max((float)finalOffset,(float)subPixelOffsetFinal));
        float2 finalUv = UV;
        if ((isHorizontal < 0.5))
        {
            ((finalUv).y += (finalOffset * stepLength));
        }
        else
        {
            ((finalUv).x += (finalOffset * stepLength));
        }
        return sceneTexture.sample(clampMiplessLinearSampler, finalUv).rgb;
    };
    struct PSIn
    {
        float4 Position [[position]];
        float2 TEXCOORD;
    };
    float4 main(PSIn input)
    {
        float3 result = float3((float)0.0, (float)0.0, (float)0.0);
#ifndef TARGET_IOS
        if (FXAARootConstant.Use)
        {
            (result = FXAA((input).TEXCOORD, int2((int2)((input).TEXCOORD * FXAARootConstant.ScreenSize))));
        }
        else
#endif
        {
            (result = (float3)(sceneTexture.sample(clampMiplessLinearSampler, (input).TEXCOORD).rgb));
        }
        return float4((result).r, (result).g, (result).b, (float)1.0);
    };

    Fragment_Shader(
texture2d<float> sceneTexture,sampler clampMiplessLinearSampler,constant Uniforms_FXAARootConstant & FXAARootConstant) :
sceneTexture(sceneTexture),clampMiplessLinearSampler(clampMiplessLinearSampler),FXAARootConstant(FXAARootConstant) {}
};

fragment float4 stageMain(
    Fragment_Shader::PSIn input [[stage_in]],
	texture2d<float> sceneTexture [[texture(0)]],
    sampler clampMiplessLinearSampler [[sampler(0)]],
    constant Fragment_Shader::Uniforms_FXAARootConstant & FXAARootConstant [[buffer(0)]])
{
    Fragment_Shader::PSIn input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.TEXCOORD = input.TEXCOORD;
    Fragment_Shader main(
    sceneTexture,
    clampMiplessLinearSampler,
    FXAARootConstant);
    return main.main(input0);
}
