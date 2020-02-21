#version 450 core

precision highp float;
precision highp int; 
vec4 MulMat(mat4 lhs, vec4 rhs)
{
    vec4 dst;
	dst[0] = lhs[0][0]*rhs[0] + lhs[0][1]*rhs[1] + lhs[0][2]*rhs[2] + lhs[0][3]*rhs[3];
	dst[1] = lhs[1][0]*rhs[0] + lhs[1][1]*rhs[1] + lhs[1][2]*rhs[2] + lhs[1][3]*rhs[3];
	dst[2] = lhs[2][0]*rhs[0] + lhs[2][1]*rhs[1] + lhs[2][2]*rhs[2] + lhs[2][3]*rhs[3];
	dst[3] = lhs[3][0]*rhs[0] + lhs[3][1]*rhs[1] + lhs[3][2]*rhs[2] + lhs[3][3]*rhs[3];
    return dst;
}


layout(location = 0) in vec3 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 0) out vec3 vertOutput_POSITION;
layout(location = 1) out vec2 vertOutput_TEXCOORD;

layout(row_major, set = 1, binding = 0) uniform cbPerPass
{
    mat4 projView;
    mat4 shadowLightViewProj;
    vec4 camPos;
    vec4 lightColor[4];
    vec4 lightDirection[3];
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
    vec4 worldPos = vec4((input1).Position, 1.0);
    ((worldPos).xyz *= vec3(3.0));
    ((Out).Position = MulMat(projView,worldPos));
    ((Out).WorldPos = (worldPos).xyz);
    ((Out).TexCoord = (input1).TexCoord);
    return Out;
}
void main()
{
    VSInput input1;
    input1.Position = POSITION;
    input1.TexCoord = TEXCOORD0;
    VSOutput result = HLSLmain(input1);
    gl_Position = result.Position;
    vertOutput_POSITION = result.WorldPos;
    vertOutput_TEXCOORD = result.TexCoord;
}
