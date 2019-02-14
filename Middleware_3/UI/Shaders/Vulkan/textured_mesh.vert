#version 450 core

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 Texcoord;
layout(location = 0) out vec2 vertOutput_TEXCOORD0;

struct VsIn
{
    vec2 position;
    vec2 texcoord;
};

struct VsOut
{
    vec4 position;
    vec2 texcoord;
};

layout(push_constant) uniform uRootConstants_Block
{
    vec4 color;
    vec2 scaleBias;
} uRootConstants;

VsOut HLSLmain(VsIn input0)
{
    VsOut output0;
    ((output0).position = vec4(((((input0).position).xy * (uRootConstants.scaleBias).xy) + vec2((-1.0), 1.0)), 0.0, 1.0));
    ((output0).texcoord = (input0).texcoord);
    return output0;
}

void main()
{
    VsIn input0;
    input0.position = Position;
    input0.texcoord = Texcoord;
    VsOut result = HLSLmain(input0);
    gl_Position = result.position;
    vertOutput_TEXCOORD0 = result.texcoord;
}
