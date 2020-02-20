#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VSInput
    {
        float3 Position;
        float2 TexCoord;
    };
    struct VSOutput
    {
        float4 Position;
        float2 TexCoord;
    };
    VSOutput main(VSInput input)
    {
        VSOutput Out;
        ((Out).Position = float4((input).Position, (float)1.0));
        ((Out).TexCoord = (input).TexCoord);
        return Out;
    };

    Vertex_Shader()
    {}
};

struct main_input
{
    float3 POSITION [[attribute(0)]];
    float2 TEXCOORD [[attribute(1)]];
};

struct main_output
{
    float4 SV_POSITION [[position]];
    float2 TEXCOORD;
};

vertex main_output stageMain(
	main_input inputData [[stage_in]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = inputData.POSITION;
    input0.TexCoord = inputData.TEXCOORD;
    Vertex_Shader main;
    Vertex_Shader::VSOutput result = main.main(input0);
    main_output output;
    output.SV_POSITION = result.Position;
    output.TEXCOORD = result.TexCoord;
    return output;
}
