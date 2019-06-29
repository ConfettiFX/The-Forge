#version 450 core

layout(location = 0) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

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

layout(set = 0, binding = 2) uniform texture2D uTex0;
layout(set = 0, binding = 3) uniform sampler uSampler0;

vec4 HLSLmain(PsIn In)
{
    return (vec4(1.0, 1.0, 1.0, (texture(sampler2D( uTex0, uSampler0), vec2((In).texCoord))).r) * uRootConstants.color);
}

void main()
{
    PsIn In;
    In.position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    In.texCoord = fragInput_TEXCOORD0;
    vec4 result = HLSLmain(In);
    rast_FragData0 = result;
}