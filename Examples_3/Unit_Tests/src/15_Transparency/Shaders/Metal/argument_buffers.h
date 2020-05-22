#ifndef argument_buffers_h
#define argument_buffers_h

#ifndef MAX_NUM_OBJECTS
#define MAX_NUM_OBJECTS 64
#endif

struct ObjectInfo
{
	float4x4 toWorld;
	float4x4 normalMat;
	uint matID;
};

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
};

struct Uniforms_ObjectUniformBlock
{
	ObjectInfo objectInfo[MAX_NUM_OBJECTS];
};

struct Uniforms_DrawInfoRootConstant
{
	uint baseInstance = 0;
};

struct Uniforms_CameraUniform
{
	float4x4 camViewProj;
	float4x4 camViewMat;
	float4 camClipInfo;
	float4 camPosition;
};

struct Uniforms_LightUniformBlock
{
	float4x4 lightViewProj;
	float4 lightDirection;
	float4 lightColor;
};

struct Uniforms_MaterialUniform
{
	Material Materials[MAX_NUM_OBJECTS];
};

#ifdef VOLITION
struct Uniforms_WBOITSettings
{
	float opacitySensitivity = 3.0;
	float weightBias = 5.0;
	float precisionScalar = 10000.0;
	float maximumWeight = 20.0;
	float maximumColorValue = 1000.0;
	float additiveSensitivity = 10.0;
	float emissiveSensitivityValue = 0.5;
};
#else
struct Uniforms_WBOITSettings
{
	float colorResistance;
	float rangeAdjustment;
	float depthRange;
	float orderingStrength;
	float underflowLimit;
	float overflowLimit;
};
#endif

struct ArgData
{
    texture2d<float, access::sample> MaterialTextures[MAX_NUM_TEXTURES];
};

struct ArgDataPerFrame
{
	constant Uniforms_ObjectUniformBlock & ObjectUniformBlock [[id(0)]];
	constant Uniforms_CameraUniform & CameraUniform [[id(1)]];
	
    constant Uniforms_LightUniformBlock & LightUniformBlock [[id(2)]];
    constant Uniforms_MaterialUniform & MaterialUniform [[id(3)]];
	
	constant Uniforms_WBOITSettings & WBOITSettings [[id(4)]];
};

#define DECLARE_ARG_DATA() \
texture2d<float> VSM [[texture(0)]], \
sampler VSMSampler [[sampler(0)]], \
texture2d<float> VSMRed [[texture(1)]], \
texture2d<float> VSMGreen [[texture(2)]], \
texture2d<float> VSMBlue [[texture(3)]], \
texture2d<float> DepthTexture [[texture(4)]], \
sampler PointSampler [[sampler(1)]], \
sampler LinearSampler [[sampler(2)]], \
constant ArgData& fsData [[buffer(UPDATE_FREQ_NONE)]], \
constant ArgDataPerFrame& fsDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]

#endif /* argument_buffers_h */
