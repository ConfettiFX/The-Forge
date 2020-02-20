#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VSInput
    {
        uint VertexID;
    };
    struct VSOutput
    {
        float4 Position;
        float2 TexCoord;
    };
    VSOutput main(VSInput input)
    {
        VSOutput Out;
        float4 position;
        ((position).x = float(((((input).VertexID == uint(1)))?(3.0):((-1.0)))));
        ((position).y = float(((((input).VertexID == uint(0)))?((-3.0)):(1.0))));
        ((position).zw = float2(1.0));
        ((Out).Position = position);
        ((Out).TexCoord = (((position).xy * float2((float)0.5, (float)(-0.5))) + float2(0.5)));
        return Out;
    };

    Vertex_Shader()
    {}
};

struct main_output
{
    float4 SV_POSITION [[position]];
    float2 TEXCOORD;
};

vertex main_output stageMain(
	uint SV_VertexID [[vertex_id]])
{
    Vertex_Shader::VSInput input0;
    input0.VertexID = SV_VertexID;
    Vertex_Shader main;
    Vertex_Shader::VSOutput result = main.main(input0);
    main_output output;
    output.SV_POSITION = result.Position;
    output.TEXCOORD = result.TexCoord;
    return output;
}
