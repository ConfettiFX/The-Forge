#version 450 core

precision highp float;
precision highp int; 

layout(location = 0) in vec3 POSITION;
layout(location = 1) in vec2 TEXCOORD0;

layout(location = 0) out vec3 vertOutput_WorldPos;
layout(location = 1) out vec2 vertOutput_TEXCOORD;

layout(UPDATE_FREQ_PER_FRAME, binding = 15) uniform cbPerFrame
{
    mat4 worldMat;
    mat4 projViewMat;
    vec4 screenSize;
};

struct VSInput
{
    vec3 Position;
    vec2 TexCoord;
};

struct VSOutput
{
    vec4 Position;
	  vec3 WorldPos;
    vec2 TexCoord;
};

VSOutput HLSLmain(VSInput input1)
{
    VSOutput Out;
    vec4 worldPos	=	vec4(input1.Position, 1.0);
	  worldPos		=	worldMat * worldPos;
    Out.Position	=	projViewMat * worldPos;
	  Out.WorldPos	=	worldPos.xyz;
    Out.TexCoord	=	input1.TexCoord;
    return Out;
}

void main()
{
    VSInput input1;
    input1.Position = POSITION;
    input1.TexCoord = TEXCOORD0;
    VSOutput result = HLSLmain(input1);
    gl_Position = result.Position;
	  vertOutput_WorldPos = result.WorldPos;
    vertOutput_TEXCOORD = result.TexCoord;
}
