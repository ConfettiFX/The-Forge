#ifndef argument_buffers_h
#define argument_buffers_h

struct IndirectDrawCommand
{
    uint indexCount;
    uint instanceCount;
    uint startIndex;
    uint vertexOffset;
    uint startInstance;
    uint padding[3];
};

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


#define ExecuteIndirectArgData \
    constant AsteroidStatic* asteroidsStatic   [[buffer(0)]],  \
    constant AsteroidDynamic* asteroidsDynamic [[buffer(1)]],  \
    texture2d_array<float> uTex0               [[texture(0)]], \
    sampler uSampler0                          [[sampler(0)]]

#define ExecuteIndirectArgDataPerFrame \
    constant UniformBlockData& uniformBlock    [[buffer(2)]]


#endif /* argument_buffers_h */
