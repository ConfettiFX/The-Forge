#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct Uniforms_cbPerPass
    {
        float4x4 projView;
        float4x4 shadowLightViewProj;
        float4 camPos;
        array<float4, 4> lightColor;
        array<float4, 3> lightDirection;
    };
    constant Uniforms_cbPerPass& cbPerPass;
    struct VSInput
    {
        float3 Position;
        float2 TexCoord;
    };
    struct VSOutput
    {
        float4 Position;
        float3 WorldPos;
        float2 TexCoord;
    };
    VSOutput main(VSInput input)
    {
        VSOutput Out;
        float4 worldPos = float4((input).Position, 1.0);
        ((worldPos).xyz *= float3(3.0));
        ((Out).Position = ((cbPerPass.projView)*(worldPos)));
        ((Out).WorldPos = (worldPos).xyz);
        ((Out).TexCoord = (input).TexCoord);
        return Out;
    };

    Vertex_Shader(constant Uniforms_cbPerPass& cbPerPass) :
        cbPerPass(cbPerPass)
    {}
};

struct main_input
{
    float3 POSITION [[attribute(0)]];
    float2 TEXCOORD0 [[attribute(1)]];
};

struct main_output
{
    float4 SV_POSITION [[position]];
    float3 POSITION;
    float2 TEXCOORD;
};

vertex main_output stageMain(
	main_input inputData [[stage_in]],
	constant float4x4* modelToWorldMatrices          [[buffer(0)]],
	constant Vertex_Shader::Uniforms_cbPerPass& cbPerPass [[buffer(1)]]
)
{
    Vertex_Shader::VSInput input0;
    input0.Position = inputData.POSITION;
    input0.TexCoord = inputData.TEXCOORD0;
    Vertex_Shader main(cbPerPass);
    Vertex_Shader::VSOutput result = main.main(input0);
    main_output output;
    output.SV_POSITION = result.Position;
    output.POSITION = result.WorldPos;
    output.TEXCOORD = result.TexCoord;
    return output;
}
