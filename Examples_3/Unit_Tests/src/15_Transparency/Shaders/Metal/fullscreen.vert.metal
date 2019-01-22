/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float4 UV;
    };
    VSOutput main(    uint VertexID)
    {
        VSOutput output;
        (output.UV = float4(((VertexID << (uint)(1)) & (uint)(2)), (VertexID & (uint)(2)), 0, 0));
        (output.Position = float4(((output.UV.xy * float2(2, (-2))) + float2((-1), 1)), 0, 1));
        return output;
    };

    Vertex_Shader(
) {}
};


vertex Vertex_Shader::VSOutput stageMain(
uint VertexID [[vertex_id]])
{
    uint VertexID0;
    VertexID0 = VertexID;
    Vertex_Shader main;
    return main.main(VertexID0);
}
