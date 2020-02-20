#include <metal_stdlib>
using namespace metal;

inline float3 unpackNormalOctQuad(float2 f) {
    float3 n = float3(f.x, f.y, 1.0 - abs(f.x) - abs(f.y));
    float t = max(-n.z, 0.0);
    n.x += n.x >= 0.0 ? -t : t;
    n.y += n.y >= 0.0 ? -t : t;
    return normalize(n);
}

struct Vertex_Shader
{
    struct VsIn
    {
        float4 position;
        float2 normal;
        float2 texCoord;
    };
    struct Uniforms_cbPerPass
    {
        float4x4 projView;
        float4x4 shadowLightViewProj;
        float4 camPos;
        array<float4, 4> lightColor;
        array<float4, 3> lightDirection;
    };
    constant Uniforms_cbPerPass& cbPerPass;
    struct Uniforms_cbRootConstants
    {
        uint nodeIndex;
    };
    constant Uniforms_cbRootConstants& cbRootConstants;
    const device float4x4* modelToWorldMatrices;
    struct PsIn
    {
        float3 pos;
        float3 normal;
        float2 texCoord;
        float4 position;
    };
    PsIn main(VsIn In)
    {
        float4x4 modelToWorld = modelToWorldMatrices[cbRootConstants.nodeIndex];
        PsIn Out;
        float4 inPos = float4(((In).position).xyz, 1.0);
        float3 inNormal = unpackNormalOctQuad(In.normal);
        float4 worldPosition = ((modelToWorld)*(inPos));
        ((Out).position = ((cbPerPass.projView)*(worldPosition)));
        ((Out).pos = (worldPosition).xyz);
        ((Out).normal = inNormal);
        ((Out).texCoord = float2(((In).texCoord).xy));
        return Out;
    };

    Vertex_Shader(constant Uniforms_cbPerPass& cbPerPass, constant Uniforms_cbRootConstants& cbRootConstants, const device float4x4* modelToWorldMatrices) :
        cbPerPass(cbPerPass), cbRootConstants(cbRootConstants), modelToWorldMatrices(modelToWorldMatrices)
    {}
};

struct main_input
{
    float4 POSITION [[attribute(0)]];
    float2 NORMAL [[attribute(1)]];
    float2 TEXCOORD0 [[attribute(2)]];
};

struct main_output
{
    float3 POSITION;
    float3 NORMAL;
    float2 TEXCOORD0;
    float4 SV_Position [[position]];
};
struct ArgBuffer0
{
    const device float4x4* modelToWorldMatrices [[id(0)]];
    sampler clampMiplessLinearSampler [[id(1)]];
    texture2d<float> ShadowTexture [[id(2)]];
};

struct ArgBuffer1
{
    constant Vertex_Shader::Uniforms_cbPerPass& cbPerPass [[id(0)]];
};

vertex main_output stageMain(
	main_input inputData [[stage_in]],
    constant ArgBuffer0& argBuffer0 [[buffer(UPDATE_FREQ_NONE)]],
    constant ArgBuffer1& argBuffer1 [[buffer(UPDATE_FREQ_PER_FRAME)]],
    constant Vertex_Shader::Uniforms_cbRootConstants& cbRootConstants [[buffer(UPDATE_FREQ_USER)]])
{
    Vertex_Shader::VsIn In0;
    In0.position = inputData.POSITION;
    In0.normal = inputData.NORMAL;
    In0.texCoord = inputData.TEXCOORD0;
    Vertex_Shader main(argBuffer1.cbPerPass, cbRootConstants, argBuffer0.modelToWorldMatrices);
    Vertex_Shader::PsIn result = main.main(In0);
    main_output output;
    output.POSITION = result.pos;
    output.NORMAL = result.normal;
    output.TEXCOORD0 = result.texCoord;
    output.SV_Position = result.position;
    return output;
}
