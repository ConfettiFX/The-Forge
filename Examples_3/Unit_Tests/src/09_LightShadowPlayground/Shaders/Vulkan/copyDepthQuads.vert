
/* Write your header comments here */
#version 450 core


layout(location = 0) out vec2 vertOutput_TEXCOORD0;
layout(location = 1) out vec4 vertOutput_TEXCOORD1;

layout(row_major, UPDATE_FREQ_PER_FRAME, binding = 0) uniform AtlasQuads_CB
{
    vec4 mPosData;
    vec4 mMiscData;
    vec4 mTexCoordData;
};

struct VSOutput
{
    vec4 Position;
    vec2 UV;
    vec4 MiscData;
};
vec2 ScaleOffset(vec2 a, vec4 p)
{
    return ((a * (p).xy) + (p).zw);
}
VSOutput HLSLmain(uint vertexID)
{
    VSOutput result;
    vec2 pos = vec2((-1.0), 1.0);
    if((vertexID == uint (1)))
    {
        (pos = vec2((-1.0), (-1.0)));
    }
    if((vertexID == uint (2)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((vertexID == uint (3)))
    {
        (pos = vec2(1.0, (-1.0)));
    }
    if((vertexID == uint (4)))
    {
        (pos = vec2(1.0, 1.0));
    }
    if((vertexID == uint (5)))
    {
        (pos = vec2((-1.0), 1.0));
    }
    ((result).Position = vec4(ScaleOffset(pos, mPosData), 0.0, 1.0));
    ((result).MiscData = mMiscData);
    ((result).UV = ScaleOffset(((vec2(0.5, (-0.5)) * pos) + vec2 (0.5)), mTexCoordData));
    return result;
}
void main()
{
    uint vertexID;
    vertexID = gl_VertexIndex;
    VSOutput result = HLSLmain(vertexID);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.UV;
    vertOutput_TEXCOORD1 = result.MiscData;
}




