/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VsIn
    {
        float4 Position [[attribute(0)]];
        float2 TexCoord [[attribute(1)]];
    };
    struct PsIn
    {
        float4 Position [[position]];
        float2 TexCoord;
    };
    PsIn main(VsIn input)
    {
        PsIn output;
        ((output).Position = float4(((input).Position).xy, 0.0, 1.0));
        ((output).TexCoord = (input).TexCoord);
        return output;
    };

    Vertex_Shader(
) {}
};


vertex Vertex_Shader::PsIn stageMain(
    Vertex_Shader::VsIn input [[stage_in]])
{
    Vertex_Shader::VsIn input0;
    input0.Position = input.Position;
    input0.TexCoord = input.TexCoord;
    Vertex_Shader main;
    return main.main(input0);
}
