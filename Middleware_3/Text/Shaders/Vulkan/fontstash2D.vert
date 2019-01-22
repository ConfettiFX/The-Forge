#version 450 core

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 0) out vec2 vertOutput_TEXCOORD0;

struct VsIn
{
    vec2 position;
    vec2 texCoord;
};

struct PsIn
{
    vec4 position;
    vec2 texCoord;
};

layout(push_constant) uniform uRootConstants_Block
{
    vec4 color;
    vec2 scaleBias;
} uRootConstants;

PsIn HLSLmain(VsIn In)
{
    PsIn Out;
    ((Out).position = vec4((In).position, 0.0, 1.0));
    (((Out).position).xy = ((((Out).position).xy * (uRootConstants.scaleBias).xy) + vec2((-1.0), 1.0)));
    ((Out).texCoord = (In).texCoord);
    return Out;
}

void main()
{
    VsIn In;
    In.position = Position;
    In.texCoord = TEXCOORD0;
    PsIn result = HLSLmain(In);
    gl_Position = result.position;
    vertOutput_TEXCOORD0 = result.texCoord;
}
