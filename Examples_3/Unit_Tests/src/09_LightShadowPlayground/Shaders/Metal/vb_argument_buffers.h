#ifndef vb_argument_buffers_h
#define vb_argument_buffers_h

struct Uniforms_objectUniformBlock
{
	float4x4 mWorldViewProjMat;
	float4x4 mWorldMat;
};

struct ArgData
{
	sampler textureFilter                        [[id(0)]];
    sampler nearClampSampler                     [[id(1)]];
    array<texture2d<float>,256> diffuseMaps;
};

struct ArgDataPerFrame
{
    constant uint* indirectMaterialBuffer         [[id(0)]];
};

struct ArgDataPerDraw
{
    constant Uniforms_objectUniformBlock & objectUniformBlock   [[id(0)]];
};

#endif /* vb_argument_buffers_h */
