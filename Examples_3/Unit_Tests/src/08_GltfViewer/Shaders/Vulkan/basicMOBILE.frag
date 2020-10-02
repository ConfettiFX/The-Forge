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

#version 450 core

void sincos1(float x, out float s, out float c) { s = sin(x); c = cos(x); }
void sincos1(vec2 x, out vec2 s, out vec2 c) { s = sin(x); c = cos(x); }
void sincos1(vec3 x, out vec3 s, out vec3 c) { s = sin(x); c = cos(x); }
void sincos1(vec4 x, out vec4 s, out vec4 c) { s = sin(x); c = cos(x); }
vec3 MulMat(vec3 lhs, mat3 rhs)
{
    vec3 dst;
	dst[0] = lhs[0]*rhs[0][0] + lhs[1]*rhs[1][0] + lhs[2]*rhs[2][0];
	dst[1] = lhs[0]*rhs[0][1] + lhs[1]*rhs[1][1] + lhs[2]*rhs[2][1];
	dst[2] = lhs[0]*rhs[0][2] + lhs[1]*rhs[1][2] + lhs[2]*rhs[2][2];
    return dst;
}

vec4 MulMat(mat4 lhs, vec4 rhs)
{
    vec4 dst;
	dst[0] = lhs[0][0]*rhs[0] + lhs[0][1]*rhs[1] + lhs[0][2]*rhs[2] + lhs[0][3]*rhs[3];
	dst[1] = lhs[1][0]*rhs[0] + lhs[1][1]*rhs[1] + lhs[1][2]*rhs[2] + lhs[1][3]*rhs[3];
	dst[2] = lhs[2][0]*rhs[0] + lhs[2][1]*rhs[1] + lhs[2][2]*rhs[2] + lhs[2][3]*rhs[3];
	dst[3] = lhs[3][0]*rhs[0] + lhs[3][1]*rhs[1] + lhs[3][2]*rhs[2] + lhs[3][3]*rhs[3];
    return dst;
}

#define PI 3.141592654f

#define NUM_SHADOW_SAMPLES 4

const float NUM_SHADOW_SAMPLES_INV = .03125;
const float shadowSamples[NUM_SHADOW_SAMPLES * 8] =
{
	-0.1746646, -0.7913184,
	0.08863912, -0.898169,
	0.1748409, -0.5252063,
	0.4529319, -0.384986,
	0.3857658, -0.9096935,
	0.768011, -0.4906538,
	0.6946555, 0.1605866,
	0.7986544, 0.5325912,
	0.2847693, 0.2293397,
	-0.4357893, -0.3808875,
	-0.139129, 0.2394065,
	0.4287854, 0.899425,
	-0.6924323, -0.2203967,
	-0.2611724, 0.7359962,
	-0.850104, 0.1263935,
	-0.5380967, 0.6264234
};

layout(row_major, set = 1, binding = 0) uniform cbPerPass
{
    mat4 projView;
    mat4 shadowLightViewProj;
    vec4 camPos;
    vec4 lightColor[4];
    vec4 lightDirection[3];
};

struct GLTFTextureProperties
{
    uint mTextureSamplerIndex;
    int mUVStreamIndex;
    float mRotation;
    float mValueScale;
    vec2 mOffset;
    vec2 mScale;
};
struct GLTFMaterialData
{
    uint mMaterialType;
    float mAlphaCutoff;
    vec2 mEmissiveGBScale;
    vec4 mBaseColorFactor;
    vec4 mMetallicRoughnessFactors;
    GLTFTextureProperties mBaseColorProperties;
    GLTFTextureProperties mMetallicRoughnessProperties;
    GLTFTextureProperties mNormalTextureProperties;
    GLTFTextureProperties mOcclusionTextureProperties;
    GLTFTextureProperties mEmissiveTextureProperties;
};
layout(row_major, set = 3, binding = 0) uniform cbMaterialData
{
    GLTFMaterialData materialData;
};

layout(set = 3, binding = 1) uniform texture2D baseColorMap;
layout(set = 3, binding = 2) uniform texture2D normalMap;
layout(set = 3, binding = 3) uniform texture2D metallicRoughnessMap;
layout(set = 3, binding = 4) uniform texture2D occlusionMap;
layout(set = 3, binding = 5) uniform texture2D emissiveMap;
layout(set = 0, binding = 14) uniform texture2D ShadowTexture;
layout(set = 3, binding = 6) uniform sampler baseColorSampler;
layout(set = 3, binding = 7) uniform sampler normalMapSampler;
layout(set = 3, binding = 8) uniform sampler metallicRoughnessSampler;
layout(set = 3, binding = 9) uniform sampler occlusionMapSampler;
layout(set = 3, binding = 10) uniform sampler emissiveMapSampler;
layout(set = 0, binding = 7) uniform sampler clampMiplessLinearSampler;

layout(location = 0) in vec3 fragInput_POSITION;
layout(location = 1) in vec3 fragInput_NORMAL;
layout(location = 2) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

struct PsIn
{
    vec3 pos;
    vec3 normal;
    vec2 texCoord;
};
struct PSOut
{
    vec4 outColor;
};
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return (F0 + ((max(vec3((float(1.0) - roughness), (float(1.0) - roughness), (float(1.0) - roughness)), F0) - F0) * vec3(pow((float(1.0) - cosTheta),float(float(5.0))))));
}
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    float Fc = pow((1.0 - cosTheta),float(5.0));
    return (F0 + ((vec3(1.0) - F0) * vec3(Fc)));
}
vec4 sampleTexture(GLTFTextureProperties textureProperties, texture2D tex, sampler s, vec4 scaleFactor, vec2 uv)
{
    uint textureIndex = (textureProperties).mTextureSamplerIndex & 0xFFFF;
    uint samplerIndex = ((textureProperties).mTextureSamplerIndex >> 16) & 0xFFFF;
    if(textureIndex == 0xFFFF)
    {
        return scaleFactor;
    }
    vec2 texCoord = ((uv * (textureProperties).mScale) + (textureProperties).mOffset);
    if(bool((textureProperties).mRotation))
    {
        float s, c;
        sincos1((textureProperties).mRotation, s, c);
        (texCoord = vec2(((c * (texCoord).x) - (s * (texCoord).y)), ((s * (texCoord).x) + (c * (texCoord).y))));
    }
    return ((texture(sampler2D(tex, s), texCoord) * vec4((textureProperties).mValueScale)) * scaleFactor);
}
float distributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = (roughness * roughness);
    float a2 = (a * a);
    float NdotH = max(dot(N, H), float(0.0));
    float NdotH2 = (NdotH * NdotH);
    float nom = a2;
    float denom = ((NdotH2 * (a2 - float(1.0))) + float(1.0));
    (denom = ((3.14159274 * denom) * denom));
    return (nom / denom);
}
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = ((r * r) / 8.0);
    float nom = NdotV;
    float denom = ((NdotV * (float(1.0) - k)) + k);
    return (nom / denom);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), float(0.0));
    float NdotL = max(dot(N, L), float(0.0));
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return (ggx1 * ggx2);
}
float Vis_SmithJointApprox(float a, float NoV, float NoL)
{
    float Vis_SmithV = (NoL * ((NoV * (1.0 - a)) + a));
    float Vis_SmithL = (NoV * ((NoL * (1.0 - a)) + a));
    return (float(0.5) * 1 / max((Vis_SmithV + Vis_SmithL), float(0.0010000000)));
}
vec3 reconstructNormal(in vec4 sampleNormal)
{
    vec3 tangentNormal;
    ((tangentNormal).xy = (((sampleNormal).rg * vec2(2)) - vec2(1)));
    ((tangentNormal).z = sqrt((float(1.0) - clamp(dot((tangentNormal).xy, (tangentNormal).xy), 0.0, 1.0))));
    return normalize(tangentNormal);
}
vec3 getNormalFromMap(vec3 normal, vec3 pos, vec2 uv)
{
    uint textureIndex = materialData.mNormalTextureProperties.mTextureSamplerIndex & 0xFFFF;
    if(textureIndex == 0xFFFF)
    {
        return normalize(normal);
    }
    vec3 tangentNormal = reconstructNormal(texture(sampler2D(normalMap, normalMapSampler), uv));
    vec3 Q1 = dFdx(pos);
    vec3 Q2 = dFdy(pos);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);
    vec3 N = normalize(normal);
    vec3 T = ((Q1 * vec3((st2).g)) - (Q2 * vec3((st1).g)));
    (T = normalize(T));
    if(((isnan((T).x) || isnan((T).y)) || isnan((T).z)))
    {
        vec3 UpVec = (((abs((N).y) < float(0.9990000)))?(vec3(0.0, 1.0, 0.0)):(vec3(0.0, 0.0, 1.0)));
        (T = normalize(cross(N, UpVec)));
    }
    vec3 B = normalize(cross(T, N));
    mat3 TBN = mat3(T, B, N);
    vec3 res = MulMat(tangentNormal,TBN);
    return normalize(res);
}
vec3 ComputeLight(vec3 albedo, vec3 lightColor, vec3 metalness, float roughness, vec3 N, vec3 L, vec3 V, vec3 H, float NoL, float NoV)
{
    float a = (roughness * roughness);
    vec3 F0 = vec3(0.040000000, 0.040000000, 0.040000000);
    vec3 diffuse = ((vec3(1.0) - metalness) * albedo);
    float NDF = distributionGGX(N, H, roughness);
    float G = Vis_SmithJointApprox(a, NoV, NoL);
    vec3 F = fresnelSchlick(max(dot(N, H), 0.0), mix(F0, albedo, metalness));
    vec3 specular = (vec3((NDF * G)) * F);
    vec3 F2 = fresnelSchlick(max(dot(N, V), 0.0), F0);
    (specular += F2);
    vec3 irradiance = (vec3((lightColor).r, (lightColor).g, (lightColor).b) * vec3(1.0, 1.0, 1.0));
    vec3 result = (((diffuse + specular) * vec3(NoL)) * irradiance);
    return result;
}
float CalcESMShadowFactor(vec3 worldPos)
{
    vec4 posLS = MulMat(shadowLightViewProj,vec4((worldPos).xyz, 1.0));
    (posLS /= vec4((posLS).w));
    ((posLS).y *= float((-1)));
    ((posLS).xy = (((posLS).xy * vec2(0.5)) + vec2(0.5, 0.5)));
    vec2 HalfGaps = vec2(0.00048828124, 0.00048828124);
    vec2 Gaps = vec2(0.0009765625, 0.0009765625);
    ((posLS).xy += HalfGaps);
    float shadowFactor = float(1.0);
    vec4 shadowDepthSample = vec4(0, 0, 0, 0);
    ((shadowDepthSample).x = (textureLod(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0))).r);
    ((shadowDepthSample).y = (textureLodOffset(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0), ivec2(1, 0))).r);
    ((shadowDepthSample).z = (textureLodOffset(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0), ivec2(0, 1))).r);
    ((shadowDepthSample).w = (textureLodOffset(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0), ivec2(1, 1))).r);
    float avgShadowDepthSample = (((((shadowDepthSample).x + (shadowDepthSample).y) + (shadowDepthSample).z) + (shadowDepthSample).w) * 0.25);
    (shadowFactor = clamp((float(2.0) - exp((((posLS).z - avgShadowDepthSample) * 100.0))), 0.0, 1.0));
    return shadowFactor;
}
float random(vec3 seed, vec3 freq)
{
    float dt = dot(floor((seed * freq)), vec3(53.12149811, 21.1352005, 9.13220024));
    return fract((sin(dt) * float(2105.23535156)));
}
float CalcPCFShadowFactor(vec3 worldPos)
{
    vec4 posLS = MulMat(shadowLightViewProj,vec4((worldPos).xyz, 1.0));
    (posLS /= vec4((posLS).w));
    ((posLS).y *= float((-1)));
    ((posLS).xy = (((posLS).xy * vec2(0.5)) + vec2(0.5, 0.5)));
    vec2 HalfGaps = vec2(0.00048828124, 0.00048828124);
    vec2 Gaps = vec2(0.0009765625, 0.0009765625);
    ((posLS).xy += HalfGaps);
    float shadowFactor = float(1.0);
    float shadowFilterSize = float(0.0016000000);
    float angle = random(worldPos, vec3(20.0));
    float s = sin(angle);
    float c = cos(angle);
    for (int i = 0; (i < 32); (i++))
    {
        vec2 offset = vec2(shadowSamples[(i * 2)], shadowSamples[((i * 2) + 1)]);
        (offset = vec2((((offset).x * c) + ((offset).y * s)), (((offset).x * (-s)) + ((offset).y * c))));
        (offset *= vec2(shadowFilterSize));
        float shadowMapValue = float(textureLod(sampler2D(ShadowTexture, clampMiplessLinearSampler), ((posLS).xy + offset), float(0)));
        (shadowFactor += ((((shadowMapValue - 0.0020000000) > (posLS).z))?(0.0):(1.0)));
    }
    (shadowFactor *= NUM_SHADOW_SAMPLES_INV);
    return shadowFactor;
}
float CalculateShadow(vec3 worldPos)
{
    vec4 NDC = MulMat(shadowLightViewProj,vec4(worldPos, 1.0));
    (NDC /= vec4((NDC).w));
    float Depth = (NDC).z;
    vec2 ShadowCoord = vec2((((NDC).x + float(1.0)) * float(0.5)), ((float(1.0) - (NDC).y) * float(0.5)));
    float ShadowDepth = (texture(sampler2D(ShadowTexture, clampMiplessLinearSampler), ShadowCoord)).r;
    if(((ShadowDepth - 0.0020000000) > Depth))
    {
        return 0.0;
    }
    else
    {
        return 1.0;
    }
}
PSOut HLSLmain(PsIn input1)
{
    PSOut Out;
    vec4 baseColor = sampleTexture((materialData).mBaseColorProperties, baseColorMap, baseColorSampler, (materialData).mBaseColorFactor, (input1).texCoord);
    vec4 metallicRoughness = sampleTexture((materialData).mMetallicRoughnessProperties, metallicRoughnessMap, metallicRoughnessSampler, (materialData).mMetallicRoughnessFactors, (input1).texCoord);
    float ao = (sampleTexture((materialData).mOcclusionTextureProperties, occlusionMap, occlusionMapSampler, vec4(1.0), (input1).texCoord)).x;
    vec3 emissive = (sampleTexture((materialData).mEmissiveTextureProperties, emissiveMap, emissiveMapSampler, vec4(1.0), (input1).texCoord)).rgb;
	emissive *= materialData.mEmissiveTextureProperties.mValueScale;
    ((emissive).gb *= (materialData).mEmissiveGBScale);
    vec3 normal = getNormalFromMap((input1).normal, (input1).pos, (input1).texCoord);
    vec3 metalness = vec3((metallicRoughness).r, (metallicRoughness).r, (metallicRoughness).r);
    float roughness = (metallicRoughness).g;
    if((((materialData).mAlphaCutoff < 1.0) && ((baseColor).a < (materialData).mAlphaCutoff)))
    {
        discard;
    }
    (roughness = clamp(0.020000000, 1.0, roughness));
    vec3 N = normal;
    vec3 V = normalize(((camPos).xyz - (input1).pos));
    float NoV = max(dot(N, V), float(0.0));
    vec3 result = vec3(0.0, 0.0, 0.0);
    for (uint i = uint(0); (i < uint(1)); (++i))
    {
        vec3 L = normalize((lightDirection[i]).xyz);
        vec3 H = normalize((V + L));
        float NoL = max(dot(N, L), float(0.0));
        (result += (ComputeLight((baseColor).rgb, (lightColor[i]).rgb, metalness, roughness, N, L, V, H, NoL, NoV) * vec3((lightColor[i]).a)));
    }
    (result *= vec3(ao));
    (result *= vec3(CalcPCFShadowFactor((input1).pos)));
    (result += (((baseColor).rgb * (lightColor[3]).rgb) * vec3((lightColor[3]).a)));
    (result += emissive);
    ((Out).outColor = vec4((result).r, (result).g, (result).b, (baseColor).a));
    return Out;
}
void main()
{
    PsIn input1;
    input1.pos = fragInput_POSITION;
    input1.normal = fragInput_NORMAL;
    input1.texCoord = fragInput_TEXCOORD0;
    PSOut result = HLSLmain(input1);
    rast_FragData0 = result.outColor;
}
