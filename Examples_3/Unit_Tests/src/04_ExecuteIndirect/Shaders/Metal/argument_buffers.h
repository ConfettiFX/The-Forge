#ifndef argument_buffers_h
#define argument_buffers_h

struct InstanceData
{
    float4x4 mvp;
    float4x4 normalMat;
    float4 surfaceColor;
    float4 deepColor;
    int textureID;
    uint _pad0[3];
};

struct UniformBlockData
{
    float4x4 viewProj;
};

struct AsteroidDynamic
{
	float4x4 transform;
    uint indexStart;
    uint indexEnd;
    uint padding[2];
};

struct AsteroidStatic
{
	float4 rotationAxis;
	float4 surfaceColor;
	float4 deepColor;

	float scale;
	float orbitSpeed;
	float rotationSpeed;

    uint textureID;
    uint vertexStart;
    uint padding[3];
};

struct BasicArgData
{
    texture2d_array<float> uTex0;
    sampler uSampler0;
};

struct BasicArgDataPerBatch
{
    constant InstanceData* instanceBuffer;
};

struct ExecuteIndirectArgData
{
    constant AsteroidStatic* asteroidsStatic;
    constant AsteroidDynamic* asteroidsDynamic;
	
    texture2d_array<float> uTex0;
    sampler uSampler0;
};

struct ExecuteIndirectArgDataPerFrame
{
    constant UniformBlockData& uniformBlock;
};


#endif /* argument_buffers_h */
