#ifndef renderSceneBRDF_h
#define renderSceneBRDF_h

struct PointLight
{
	float4 positionAndRadius;
	float4 colorAndIntensity;
};

struct DirectionalLight
{
	float4 directionAndShadowMap;
	float4 colorAndIntensity;
	float shadowRange;
	float _pad0;
	float _pad1;
	int shadowMapDimensions;
	float4x4 viewProj;
};

struct CameraData
{
    float4x4 projView;
    float4x4 invProjView;
    float3 camPos;

	float fAmbientLightIntensity;
	int bUseEnvironmentLight;
	float fEnvironmentLightIntensity;
	float fAOIntensity;

	int renderMode;
	float fNormalMapIntensity;
};

struct ObjectData
{
	float4x4 worldMat;
	float4 albedoAndRoughness;
	float2 tiling;
	float metalness;
	int textureConfig;
};

struct PointLightData
{
	PointLight PointLights[MAX_NUM_POINT_LIGHTS];
	int NumPointLights;
};

struct DirectionalLightData
{
	DirectionalLight DirectionalLights[MAX_NUM_DIRECTIONAL_LIGHTS];
	int NumDirectionalLights;
};

struct VSData
{
    constant PointLightData& cbPointLights              [[id(0)]];
    constant DirectionalLightData& cbDirectionalLights  [[id(1)]];
    
    texture2d<float, access::sample> brdfIntegrationMap [[id(2)]];
    texturecube<float, access::sample> irradianceMap    [[id(3)]];
    texturecube<float, access::sample> specularMap      [[id(4)]];
    texture2d<float, access::sample> shadowMap          [[id(5)]];

    sampler bilinearSampler                             [[id(6)]];
    sampler bilinearClampedSampler                      [[id(7)]];
};

struct VSDataPerFrame
{
    constant CameraData& cbCamera                       [[id(0)]];
};

struct VSDataPerDraw
{
    constant ObjectData& cbObject                       [[id(0)]];
    
    texture2d<float> albedoMap                          [[id(1)]];
    texture2d<float, access::sample> normalMap          [[id(2)]];
    texture2d<float, access::sample> metallicMap        [[id(3)]];
    texture2d<float, access::sample> roughnessMap       [[id(4)]];
    texture2d<float, access::sample> aoMap              [[id(5)]];
};

#endif /* renderSceneBRDF_h */
