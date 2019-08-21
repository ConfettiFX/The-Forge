/* Write your header comments here */
#version 450 core


layout(location = 0) in vec3 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 0) out vec2 vertOutput_TEXCOORD0;

struct VsIn
{
    vec3 Position;
    vec2 TexCoord;
};
struct PsIn
{
    vec4 Position;
    vec2 TexCoord;
};

/*-1.0f, 1.0f, 0.f,  0.0f, 0.0f,
	-1.0f, -1.0f, 0.f, 0.0f, 1.0f,
	1.0f, -1.0f, 0.f,  1.0f, 1.0f,

	1.0f, -1.0f, 0.f, 1.0f, 1.0f,
	1.0f, 1.0f, 0.f, 1.0f, 0.0f,
	-1.0f, 1.0f, 0.f, 0.0f, 0.0f,*/

PsIn HLSLmain(VsIn input1)
{
	uint vertexID;
    vertexID = gl_VertexIndex;
	vec2 pos = vec2(-1.0,1.0);
	vec2 texCoord = vec2(0.0, 0.0);

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

    PsIn output1;
    ((output1).Position = vec4(((input1).Position).xy, 0.0, 1.0));
	output1.Position = vec4(pos, 0.0, 1.0);
    //((output1).TexCoord = (input1).TexCoord);
	output1.TexCoord = texCoord;
    return output1;
}
void main()
{
    VsIn input1;
    input1.Position = POSITION;
    input1.TexCoord = TEXCOORD0;
    PsIn result = HLSLmain(input1);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.TexCoord;
}
