#ifndef _SHADER_DEFS_H
#define _SHADER_DEFS_H

#define UNIT_CBV_ID         UPDATE_FREQ_NONE, binding = 0
#define UNIT_CBV_OBJECT     UPDATE_FREQ_PER_FRAME, binding = 0
#define UNIT_CBV_CAMERA     UPDATE_FREQ_PER_FRAME, binding = 1
#define UNIT_CBV_MATERIAL   UPDATE_FREQ_PER_FRAME, binding = 2
#define UNIT_CBV_LIGHT      UPDATE_FREQ_PER_FRAME, binding = 3
#define UNIT_CBV_WBOIT      UPDATE_FREQ_PER_FRAME, binding = 4

#define UNIT_SRV_TEXTURES   UPDATE_FREQ_NONE, binding = 0
#define UNIT_SRV_DEPTH      UPDATE_FREQ_NONE, binding = 1
#define UNIT_SRV_VSM        UPDATE_FREQ_NONE, binding = 2
#define UNIT_SRV_VSM_R      UPDATE_FREQ_NONE, binding = 3
#define UNIT_SRV_VSM_G      UPDATE_FREQ_NONE, binding = 4
#define UNIT_SRV_VSM_B      UPDATE_FREQ_NONE, binding = 5

#define UNIT_SAMPLER_LINEAR UPDATE_FREQ_NONE, binding = 6
#define UNIT_SAMPLER_POINT  UPDATE_FREQ_NONE, binding = 7
#define UNIT_SAMPLER_VSM    UPDATE_FREQ_NONE, binding = 8

struct Material
{
	vec4 Color;
	vec4 Transmission;
	float RefractionRatio;
	float Collimation;
	vec2 Padding;
	uint TextureFlags;
	uint AlbedoTexID;
	uint MetallicTexID;
	uint RoughnessTexID;
	uint EmissiveTexID;
};

struct ObjectInfo
{
	mat4 toWorld;
	mat4 normalMat;
	uint matID;
};

layout(push_constant) uniform DrawInfoRootConstant_Block
{
	uint baseInstance;
} DrawInfoRootConstant;

layout(UNIT_CBV_OBJECT) uniform ObjectUniformBlock
{
	ObjectInfo	objectInfo[MAX_NUM_OBJECTS];
};

layout(UNIT_CBV_LIGHT) uniform LightUniformBlock
{
	mat4 lightViewProj;
	vec4 lightDirection;
	vec4 lightColor;
};

layout(UNIT_CBV_CAMERA) uniform CameraUniform
{
	mat4 camViewProj;
	mat4 camViewMat;
	vec4 camClipInfo;
	vec4 camPosition;
};

layout(UNIT_CBV_MATERIAL) uniform MaterialUniform
{
	Material Materials[MAX_NUM_OBJECTS];
};

layout(UNIT_SRV_TEXTURES) uniform texture2D MaterialTextures[MAX_NUM_TEXTURES];
layout(UNIT_SAMPLER_LINEAR) uniform sampler LinearSampler;

#if PT_USE_DIFFUSION != 0
layout(UNIT_SRV_DEPTH) uniform texture2D DepthTexture;
layout(UNIT_SAMPLER_POINT) uniform sampler PointSampler;
#endif

layout (UNIT_SRV_VSM) uniform texture2D VSM;
layout (UNIT_SAMPLER_VSM) uniform sampler VSMSampler;
#if PT_USE_CAUSTICS != 0
layout (UNIT_SRV_VSM_R) uniform texture2D VSMRed;
layout (UNIT_SRV_VSM_G) uniform texture2D VSMGreen;
layout (UNIT_SRV_VSM_B) uniform texture2D VSMBlue;
#endif

#endif
