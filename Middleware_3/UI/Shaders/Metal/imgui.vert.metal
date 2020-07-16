#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct Uniforms_uniformBlockVS
    {
        float4x4 ProjectionMatrix;
    };
    constant Uniforms_uniformBlockVS & uniformBlockVS;
    struct VS_INPUT
    {
        float2 pos [[attribute(0)]];
        float2 uv [[attribute(1)]];
        float4 col [[attribute(2)]];
    };
    struct PS_INPUT
    {
        float4 pos [[position]];
        float4 col;
        float2 uv;
    };
    PS_INPUT main(VS_INPUT input)
    {
        PS_INPUT output;
        ((output).pos = ((uniformBlockVS.ProjectionMatrix)*(float4(((input).pos).xy, 0.0, 1.0))));
        ((output).col = (input).col);
        ((output).uv = (input).uv);
        return output;
    };

    Vertex_Shader(
constant Uniforms_uniformBlockVS & uniformBlockVS) :
uniformBlockVS(uniformBlockVS) {}
};

vertex Vertex_Shader::PS_INPUT stageMain(
    Vertex_Shader::VS_INPUT input  [[stage_in]],
    constant Vertex_Shader::Uniforms_uniformBlockVS& uniformBlockVS [[buffer(0)]]
)
{
    Vertex_Shader::VS_INPUT input0;
    input0.pos = input.pos;
    input0.uv = input.uv;
    input0.col = input.col;
    Vertex_Shader main(uniformBlockVS);
    return main.main(input0);
}
