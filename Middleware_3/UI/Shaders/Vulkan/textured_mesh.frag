#version 450 core

layout(location = 0) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

struct PsIn
{
    vec4 position;
    vec2 texcoord;
};

layout(push_constant) uniform uRootConstants_Block
{
    vec4 color;
    vec2 scaleBias;
} uRootConstants;

layout(set = 0, binding = 1) uniform texture2D uTex;
layout(set = 0, binding = 2) uniform sampler uSampler;

vec4 HLSLmain(PsIn input0)
{
    return (texture(sampler2D( uTex, uSampler), vec2((input0).texcoord)) * uRootConstants.color);
}

void main()
{
    PsIn input0;
    input0.position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input0.texcoord = fragInput_TEXCOORD0;
    vec4 result = HLSLmain(input0);
    rast_FragData0 = result;
}
