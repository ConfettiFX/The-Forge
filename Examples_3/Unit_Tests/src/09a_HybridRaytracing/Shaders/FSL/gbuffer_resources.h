//
//  gbuffer_resources.h
//

#ifndef gbuffer_resources_h
#define gbuffer_resources_h

#ifndef TOTAL_IMGS
	#define TOTAL_IMGS 84
#endif

// UPDATE_FREQ_NONE
RES(Tex2D(float4), inputRT,                 UPDATE_FREQ_NONE, t0, binding = 1);

#if !defined(TARGET_IOS)
RES(Tex2D(float4), textureMaps[TOTAL_IMGS], UPDATE_FREQ_NONE, t3, binding = 6);
#else
RES(Tex2D(float4), albedoMap,    UPDATE_FREQ_PER_DRAW, t3, binding = 6);
RES(Tex2D(float4), normalMap,    UPDATE_FREQ_PER_DRAW, t3, binding = 6);
RES(Tex2D(float4), metallicMap,  UPDATE_FREQ_PER_DRAW, t3, binding = 6);
RES(Tex2D(float4), roughnessMap, UPDATE_FREQ_PER_DRAW, t3, binding = 6);
RES(Tex2D(float4), aoMap,        UPDATE_FREQ_PER_DRAW, t3, binding = 6);
#endif

RES(SamplerState,  samplerLinear,           UPDATE_FREQ_NONE, s2, binding = 7);
CBUFFER(cbPerProp, UPDATE_FREQ_NONE, b1, binding = 0)
{
	DATA(float4x4, world, None);
	DATA(float, roughness, None);
	DATA(float, metallic, None);
	DATA(int, pbrMaterials, None);
	DATA(float, pad, None);
};

// UPDATE_FREQ_PER_FRAME
CBUFFER(cbPerPass, UPDATE_FREQ_PER_FRAME, b0, binding = 0)
{
	DATA(float4x4, projView, None);
};

// PUSH_CONSTANTS
#if !defined(DISPLAY) && !defined(TARGET_IOS)
PUSH_CONSTANT(cbTextureRootConstants, b2)
{
	DATA(uint, albedoMap, None);
	DATA(uint, normalMap, None);
	DATA(uint, metallicMap, None);
	DATA(uint, roughnessMap, None);
	DATA(uint, aoMap, None);
};
#endif

#endif /* gbuffer_resources_h */