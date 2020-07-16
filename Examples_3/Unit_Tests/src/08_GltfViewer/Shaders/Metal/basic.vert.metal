#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VsIn
    {
        float4 position;
        float3 normal;
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
    constant float4x4* modelToWorldMatrices;
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
        float3 inNormal = normalize((modelToWorld * float4(In.normal,0)).xyz);
        float4 worldPosition = ((modelToWorld)*(inPos));
        ((Out).position = ((cbPerPass.projView)*(worldPosition)));
        ((Out).pos = (worldPosition).xyz);
        ((Out).normal = inNormal);
        ((Out).texCoord = float2(((In).texCoord).xy));
        return Out;
    };

    Vertex_Shader(constant Uniforms_cbPerPass& cbPerPass, constant Uniforms_cbRootConstants& cbRootConstants, constant float4x4* modelToWorldMatrices) :
        cbPerPass(cbPerPass), cbRootConstants(cbRootConstants), modelToWorldMatrices(modelToWorldMatrices)
    {}
};

struct main_input
{
    float4 POSITION [[attribute(0)]];
    float3 NORMAL [[attribute(1)]];
    float2 TEXCOORD0 [[attribute(2)]];
};

struct main_output
{
    float3 POSITION;
    float3 NORMAL;
    float2 TEXCOORD0;
    float4 SV_Position [[position]];
};

vertex main_output stageMain(
	main_input inputData [[stage_in]],
	constant float4x4* modelToWorldMatrices          [[buffer(0)]],
	sampler clampMiplessLinearSampler                [[sampler(0)]],
	texture2d<float> ShadowTexture                   [[texture(0)]],

	constant Vertex_Shader::Uniforms_cbPerPass& cbPerPass [[buffer(1)]],

    constant Vertex_Shader::Uniforms_cbRootConstants& cbRootConstants [[buffer(3)]])
{
    Vertex_Shader::VsIn In0;
    In0.position = inputData.POSITION;
    In0.normal = inputData.NORMAL;
    In0.texCoord = inputData.TEXCOORD0;
    Vertex_Shader main(cbPerPass, cbRootConstants, modelToWorldMatrices);
    Vertex_Shader::PsIn result = main.main(In0);
    main_output output;
    output.POSITION = result.pos;
    output.NORMAL = result.normal;
    output.TEXCOORD0 = result.texCoord;
    output.SV_Position = result.position;
    return output;
}
