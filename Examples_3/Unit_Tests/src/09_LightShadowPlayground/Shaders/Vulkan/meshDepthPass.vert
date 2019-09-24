#version 450 core
#if !defined(WINDOWS) && !defined(ANDROID) && !defined(LINUX)
#define WINDOWS 	// Assume windows if no platform define has been added to the shader
#endif

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

layout(row_major, UPDATE_FREQ_PER_DRAW, binding = 0) uniform objectUniformBlock
{
    mat4 WorldViewProjMat;
    mat4 WorldMat;
};

struct VsIn
{
    vec3 Position;
};
struct PsIn
{
    vec4 Position;
};
PsIn HLSLmain(VsIn input1)
{
    PsIn output1;
    ((output1).Position = MulMat(WorldViewProjMat,vec4(((input1).Position).xyz, 1.0)));
    return output1;
}
void main()
{
    VsIn input1;
    input1.Position = POSITION;
    PsIn result = HLSLmain(input1);
    gl_Position = result.Position;
}