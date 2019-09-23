/* Write your header comments here */
#version 450 core

vec4 MulMat(mat4 lhs, vec4 rhs)
{
    vec4 dst;
	dst[0] = lhs[0][0]*rhs[0] + lhs[0][1]*rhs[1] + lhs[0][2]*rhs[2] + lhs[0][3]*rhs[3];
	dst[1] = lhs[1][0]*rhs[0] + lhs[1][1]*rhs[1] + lhs[1][2]*rhs[2] + lhs[1][3]*rhs[3];
	dst[2] = lhs[2][0]*rhs[0] + lhs[2][1]*rhs[1] + lhs[2][2]*rhs[2] + lhs[2][3]*rhs[3];
	dst[3] = lhs[3][0]*rhs[0] + lhs[3][1]*rhs[1] + lhs[3][2]*rhs[2] + lhs[3][3]*rhs[3];
    return dst;
}


layout(location = 0) in vec4 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 0) out vec2 vertOutput_TEXCOORD0;

layout(row_major, UPDATE_FREQ_PER_FRAME, binding = 0) uniform UniformQuadData
{
    mat4 mModelMat;
};

struct VSInput
{
    vec4 Position;
    vec2 Tex_Coord;
};
struct VSOutput
{
    vec4 Position;
    vec2 Tex_Coord;
};
VSOutput HLSLmain(VSInput input1, uint vertexID)
{
    vec2 pos = vec2((-1.0), 1.0);
	vec2 texCoord = vec2(0.f, 0.f);
    const uint verticesPerQuad = uint (6);
    uint quadID = (vertexID / verticesPerQuad);
    uint quadVertexID = (vertexID - (quadID * verticesPerQuad));
    /*if((vertexID == uint (1)))
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
    }*/


	if((vertexID == uint (1)))
    {
        (pos = vec2((-1.0), (-1.0)));
		texCoord = vec2(0.f, 1.f);
    }
    if((vertexID == uint (2)))
    {
        (pos = vec2(1.0, (-1.0)));
		texCoord = vec2(1.f, 1.f);
    }
    if((vertexID == uint (3)))
    {
        (pos = vec2(1.0, (-1.0)));
		texCoord = vec2(1.f, 1.f);
    }
    if((vertexID == uint (4)))
    {
        (pos = vec2(1.0, 1.0));
		texCoord = vec2(1.f, 0.f);
    }
    if((vertexID == uint (5)))
    {
        (pos = vec2((-1.0), 1.0));
		texCoord = vec2(0.f, 0.f);
    }


    VSOutput result;
    ((result).Position = MulMat(mModelMat,vec4(pos, 0.5, 1.0)));
    ((result).Tex_Coord = texCoord);
    return result;
}
void main()
{
    VSInput input1;
    input1.Position = POSITION;
    input1.Tex_Coord = TEXCOORD0;
    uint vertexID;
    vertexID = gl_VertexIndex;
    VSOutput result = HLSLmain(input1, vertexID);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.Tex_Coord;
}
