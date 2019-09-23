/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct Uniforms_AtlasQuads_CB
    {
        float4 mPosData;
        float4 mMiscData;
        float4 mTexCoordData;
    };
    constant Uniforms_AtlasQuads_CB & AtlasQuads_CB;
    struct VSOutput
    {
        float4 Position [[position]];
        float2 UV;
        float4 MiscData;
    };
    float2 ScaleOffset(float2 a, float4 p)
    {
        return ((a * (p).xy) + (p).zw);
    };
    VSOutput main(uint vertexID)
    {
        VSOutput result;
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
        ((result).Position = float4(ScaleOffset(pos, AtlasQuads_CB.mPosData), 0.0, 1.0));
        ((result).MiscData = AtlasQuads_CB.mMiscData);
        ((result).UV = ScaleOffset(((float2(0.5, (-0.5)) * pos) + (float2)(0.5)), AtlasQuads_CB.mTexCoordData));
        return result;
    };

    Vertex_Shader(
constant Uniforms_AtlasQuads_CB & AtlasQuads_CB) :
AtlasQuads_CB(AtlasQuads_CB) {}
};

struct VSData {
    constant Vertex_Shader::Uniforms_AtlasQuads_CB & AtlasQuads_CB [[id(0)]];
};

vertex Vertex_Shader::VSOutput stageMain(
    uint vertexID               [[vertex_id]],
    constant VSData& vsData     [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    uint vertexID0;
    vertexID0 = vertexID;
    Vertex_Shader main(vsData.AtlasQuads_CB);
    return main.main(vertexID0);
}
