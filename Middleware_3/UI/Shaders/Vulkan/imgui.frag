#version 450 core

layout(location = 0) in vec4 fragInput_COLOR0;
layout(location = 1) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

struct PS_INPUT
{
    vec4 pos;
    vec4 col;
    vec2 uv;
};

layout(set = 2, binding = 1) uniform texture2D uTex;
layout(set = 0, binding = 2) uniform sampler uSampler;

vec4 HLSLmain(PS_INPUT input0)
{
    return (input0).col * texture(sampler2D( uTex, uSampler), vec2((input0).uv));    
}

void main()
{
    PS_INPUT input0;
    input0.pos = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input0.col = fragInput_COLOR0;
    input0.uv = fragInput_TEXCOORD0;
    vec4 result = HLSLmain(input0);
    rast_FragData0 = result;
}
