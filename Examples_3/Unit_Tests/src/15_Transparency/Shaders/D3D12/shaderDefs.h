#ifndef _SHADER_DEFS_H
#define _SHADER_DEFS_H

#define UNIT_CBV_ID         register(b0, UPDATE_FREQ_NONE)
#define UNIT_CBV_OBJECT     register(b0, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_CAMERA     register(b1, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_MATERIAL   register(b2, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_LIGHT      register(b3, UPDATE_FREQ_PER_FRAME)
#define UNIT_CBV_WBOIT      register(b4, UPDATE_FREQ_PER_FRAME)

#define UNIT_SRV_TEXTURES   register(t100, UPDATE_FREQ_NONE)
#define UNIT_SRV_DEPTH      register(t0, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM        register(t1, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM_R      register(t2, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM_G      register(t3, UPDATE_FREQ_NONE)
#define UNIT_SRV_VSM_B      register(t4, UPDATE_FREQ_NONE)

#define UNIT_SAMPLER_LINEAR register(s0, UPDATE_FREQ_NONE)
#define UNIT_SAMPLER_POINT  register(s1, UPDATE_FREQ_NONE)
#define UNIT_SAMPLER_VSM    register(s2, UPDATE_FREQ_NONE)

struct Material
{
	float4 Color;
	float4 Transmission;
	float RefractionRatio;
	float Collimation;
	float2 Padding;
	uint TextureFlags;
	uint AlbedoTexID;
	uint MetallicTexID;
	uint RoughnessTexID;
	uint EmissiveTexID;
	uint3 Padding2;
};

struct ObjectInfo
{
	float4x4 toWorld;
	float4x4 normalMat;
	uint matID;
};

cbuffer DrawInfoRootConstant : UNIT_CBV_ID
{
	uint baseInstance = 0;
};

cbuffer ObjectUniformBlock : UNIT_CBV_OBJECT
{
	ObjectInfo	objectInfo[MAX_NUM_OBJECTS];
};

cbuffer LightUniformBlock : UNIT_CBV_LIGHT
{
	float4x4 lightViewProj;
	float4 lightDirection;
	float4 lightColor;
};

cbuffer CameraUniform : UNIT_CBV_CAMERA
{
	float4x4 camViewProj;
	float4x4 camViewMat;
	float4 camClipInfo;
	float4 camPosition;
};

cbuffer MaterialUniform : UNIT_CBV_MATERIAL
{
	Material Materials[MAX_NUM_OBJECTS];
};

Texture2D MaterialTextures[MAX_NUM_TEXTURES] : UNIT_SRV_TEXTURES;
SamplerState LinearSampler : UNIT_SAMPLER_LINEAR;

Texture2D VSM : UNIT_SRV_VSM;
SamplerState VSMSampler : UNIT_SAMPLER_VSM;
#if PT_USE_CAUSTICS != 0
Texture2D VSMRed : UNIT_SRV_VSM_R;
Texture2D VSMGreen : UNIT_SRV_VSM_G;
Texture2D VSMBlue : UNIT_SRV_VSM_B;
#endif

#endif
