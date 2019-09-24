/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct Uniforms_UniformQuadData
    {
        float4x4 mModelMat;
    };
    constant Uniforms_UniformQuadData & UniformQuadData;
    struct VSInput
    {
        float4 Position [[attribute(0)]];
        float2 Tex_Coord [[attribute(1)]];
    };
    struct VSOutput
    {
        float4 Position [[position]];
        float2 Tex_Coord;
    };
    VSOutput main(VSInput input, uint vertexID)
    {
        float2 pos = float2((-1.0), 1.0);
        if (vertexID == 1)
        {
            (pos = float2((-1.0), (-1.0)));
        }
        if (vertexID == 2)
        {
            (pos = float2(1.0, (-1.0)));
        }
        if (vertexID == 3)
        {
            (pos = float2(1.0, (-1.0)));
        }
        if (vertexID == 4)
        {
            (pos = float2(1.0, 1.0));
        }
        if (vertexID == 5)
        {
            (pos = float2((-1.0), 1.0));
        }
        VSOutput result;
        ((result).Position = ((UniformQuadData.mModelMat)*(float4(pos, 0.5, 1.0))));
        ((result).Tex_Coord = (input).Tex_Coord);
        return result;
    };

    Vertex_Shader(
constant Uniforms_UniformQuadData & UniformQuadData) :
UniformQuadData(UniformQuadData) {}
};

struct VSData {
    constant Vertex_Shader::Uniforms_UniformQuadData & UniformQuadData [[buffer(0)]];
};

vertex Vertex_Shader::VSOutput stageMain(
    Vertex_Shader::VSInput input    [[stage_in]],
    uint vertexID                   [[vertex_id]],
    constant VSData& vsData         [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.Tex_Coord = input.Tex_Coord;
    uint vertexID0;
    vertexID0 = vertexID;
    Vertex_Shader main(vsData.UniformQuadData);
    return main.main(input0, vertexID0);
}
