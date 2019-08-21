/* Write your header comments here */
#version 450 core


layout(location = 0) out vec4 vertOutput_TEXCOORD0;

layout(column_major, set = 3, binding = 0) uniform PackedAtlasQuads_CB
{
    vec4 mQuadsData[192];
};

struct AtlasQuads
{
    vec4 mPosData;
    vec4 mMiscData;
};
struct VSOutput
{
    vec4 Position;
    vec4 MiscData;
};
vec2 ScaleOffset(vec2 a, vec4 p)
{
    return ((a * (p).xy) + (p).zw);
}
VSOutput HLSLmain(uint vertexID)
{
    VSOutput result;
    const uint verticesPerQuad = uint (6);
    uint quadID = (vertexID / verticesPerQuad);
    uint quadVertexID = (vertexID - (quadID * verticesPerQuad));
    vec2 pos = vec2((-1.0), 1.0);
    if((quadVertexID == uint (1)))
    {
        (pos = vec2((-1.0), (-1.0)));
    }
    if((quadVertexID == uint (2)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((quadVertexID == uint (3)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((quadVertexID == uint (4)))
    {
        (pos = vec2(1.0, 1.0));
    }
    if((quadVertexID == uint (5)))
    {
        (pos = vec2((-1.0), 1.0));
    }
    const uint registersPerQuad = uint (2);
    vec4 quadData[registersPerQuad];
    for (int i = 0; (uint (i) < registersPerQuad); (++i))
    {
        (quadData[i] = mQuadsData[((quadID * registersPerQuad) + uint (i))]);
    }
    //AtlasQuads atlasQuad = AtlasQuads (quadData);
	AtlasQuads atlasQuad;
	atlasQuad.mPosData = quadData[0];
	atlasQuad.mMiscData = quadData[1];

    ((result).Position = vec4(ScaleOffset(pos, (atlasQuad).mPosData), 0.0, 1.0));
    ((result).MiscData = (atlasQuad).mMiscData);
    return result;
}
void main()
{
    uint vertexID;
    vertexID = gl_VertexIndex;
    VSOutput result = HLSLmain(vertexID);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.MiscData;
}


