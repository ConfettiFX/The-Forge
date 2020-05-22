#ifndef hair_h
#define hair_h

struct CameraData
{
	float4x4 CamVPMatrix;
	float4x4 CamInvVPMatrix;
	float3 CamPos;
	float fAmbientLightIntensity;
	int bUseEnvironmentLight;
	float fEnvironmentLightIntensity;
	float fAOIntensity;

	int renderMode;
	float fNormalMapIntensity;
};

struct HairData
{
	float4x4 Transform;
	uint RootColor;
	uint StrandColor;
	float ColorBias;
	float Kd;
	float Ks1;
	float Ex1;
	float Ks2;
	float Ex2;
	float FiberRadius;
	float FiberSpacing;
	uint NumVerticesPerStrand;
};

struct GlobalHairData
{
	float4 Viewport;
	float4 Gravity;
	float4 Wind;
	float TimeStep;
};

struct PointLight
{
    float4 positionAndRadius;
    float4 colorAndIntensity;
};

struct DirectionalLight
{
	packed_float3 direction;
	int shadowMap;
	packed_float3 color;
	float intensity;
	float shadowRange;
	float _pad0;
	float _pad1;
	int shadowMapDimensions;
	float4x4 viewProj;
};

struct PointLightData
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	uint NumPointLights;
};

struct DirectionalLightData
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	uint NumDirectionalLights;
};

struct DirectionalLightCameraData
{
    CameraData Cam[MAX_NUM_DIRECTIONAL_LIGHTS];
};

struct VSData
{
    constant GlobalHairData& cbHairGlobal;
    
    constant PointLightData& cbPointLights;
    constant DirectionalLightData& cbDirectionalLights;
    
    device uint* DepthsTexture;
    
    sampler PointSampler;
	
    texture2d<float, access::read> ColorsTexture;
    texture2d<float, access::read> InvAlphaTexture;
};

struct VSDataPerFrame
{
#if !defined(HAIR_SHADOW)
    constant CameraData& cbCamera;
#endif
};

struct VSDataPerBatch
{
#if defined(HAIR_SHADOW)
        constant CameraData& cbCamera;
#endif
    
    constant DirectionalLightCameraData& cbDirectionalLightShadowCameras;
    
    array<texture2d<float, access::sample>, MAX_NUM_DIRECTIONAL_LIGHTS> DirectionalLightShadowMaps;
};

struct VSDataPerDraw
{
    constant HairData& cbHair;

    constant float4* GuideHairVertexPositions;
    constant float4* GuideHairVertexTangents;
    constant float* HairThicknessCoefficients;
};

#endif /* hair_h */
