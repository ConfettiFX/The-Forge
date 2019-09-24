/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;


struct Uniforms_PackedAtlasQuads_CB
{
	float4 mQuadsData[192];
	//array<float4, 192> mQuadsData;
};

struct Vertex_Shader
{
	
   // constant Uniforms_PackedAtlasQuads_CB & PackedAtlasQuads_CB;
    struct AtlasQuads
    {
        float4 mPosData;
        float4 mMiscData;
    };
    struct VSOutput
    {
        float4 Position [[position]];
        float4 MiscData;
    };
    float2 ScaleOffset(float2 a, float4 p)
    {
        return ((a * (p).xy) + (p).zw);
    };
    VSOutput main(uint vertexID, constant Uniforms_PackedAtlasQuads_CB & PackedAtlasQuads_CB)
    {
        VSOutput result;
        const uint verticesPerQuad = (const uint)(6);
        uint quadID = (vertexID / verticesPerQuad);
        uint quadVertexID = (vertexID - (quadID * verticesPerQuad));
        float2 pos = float2((-1.0), 1.0);
        if (quadVertexID == 1)
        {
            (pos = float2((-1.0), (-1.0)));
        }
        if (quadVertexID == 2)
        {
            (pos = float2(1.0, (-1.0)));
        }
        if (quadVertexID == 3)
        {
            (pos = float2(1.0, (-1.0)));
        }
        if (quadVertexID == 4)
        {
            (pos = float2(1.0, 1.0));
        }
        if (quadVertexID == 5)
        {
            (pos = float2((-1.0), 1.0));
        }
        const uint registersPerQuad = (const uint)(2);
        float4 quadData[registersPerQuad];
        for (int i = 0; ((uint)(i) < registersPerQuad); (++i))
        {
            (quadData[i] = PackedAtlasQuads_CB.mQuadsData[((quadID * registersPerQuad) + (uint)(i))]);
        }
        AtlasQuads atlasQuad;
		atlasQuad.mPosData = quadData[0];
		atlasQuad.mMiscData = quadData[1];
		
        ((result).Position = float4(ScaleOffset(pos, (atlasQuad).mPosData), 0.0, 1.0));
        ((result).MiscData = (atlasQuad).mMiscData);
        return result;
    };

    Vertex_Shader(
constant Uniforms_PackedAtlasQuads_CB & PackedAtlasQuads_CB)
/* : PackedAtlasQuads_CB(PackedAtlasQuads_CB) */{}
};

struct VSData {
    constant Uniforms_PackedAtlasQuads_CB & PackedAtlasQuads_CB [[id(0)]];
};

vertex Vertex_Shader::VSOutput stageMain(
    uint vertexID [[vertex_id]],
    constant VSData& vsData [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    uint vertexID0;
    vertexID0 = vertexID;
    Vertex_Shader main(vsData.PackedAtlasQuads_CB);
    return main.main(vertexID0, vsData.PackedAtlasQuads_CB);
}
