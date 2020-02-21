/* Write your header comments here */
#version 450 core


#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "Shader_Defs.h"
#include "Packing.h"


layout(location = 0) in vec3 POSITION;
layout(location = 1) in uint TEXCOORD;

layout(location = 0) out vec2 vertOutput_TEXCOORD0;
layout(location = 1) out flat uint oDrawId;

vec4 MulMat(mat4 lhs, vec4 rhs)
{
    vec4 dst;
	dst[0] = lhs[0][0]*rhs[0] + lhs[0][1]*rhs[1] + lhs[0][2]*rhs[2] + lhs[0][3]*rhs[3];
	dst[1] = lhs[1][0]*rhs[0] + lhs[1][1]*rhs[1] + lhs[1][2]*rhs[2] + lhs[1][3]*rhs[3];
	dst[2] = lhs[2][0]*rhs[0] + lhs[2][1]*rhs[1] + lhs[2][2]*rhs[2] + lhs[2][3]*rhs[3];
	dst[3] = lhs[3][0]*rhs[0] + lhs[3][1]*rhs[1] + lhs[3][2]*rhs[2] + lhs[3][3]*rhs[3];
    return dst;
}



layout(row_major, UPDATE_FREQ_PER_DRAW, binding = 0) uniform objectUniformBlock
{
    mat4 WorldViewProjMat;
    mat4 WorldMat;
};

struct VsIn
{
    vec3 Position;
    uint TexCoord;
};

struct PsIn
{
    vec4 Position;
    vec2 TexCoord;
};

PsIn HLSLmain(VsIn input1)
{
    PsIn output1;
    ((output1).Position = MulMat(WorldViewProjMat,vec4((input1).Position, 1.0)));
    ((output1).TexCoord = unpack2Floats((input1).TexCoord));
    return output1;
}

void main()
{
    VsIn input1;
    input1.Position = POSITION;
    input1.TexCoord = TEXCOORD;
    PsIn result = HLSLmain(input1);
    gl_Position = result.Position;
    vertOutput_TEXCOORD0 = result.TexCoord;
	oDrawId = gl_DrawIDARB;
}