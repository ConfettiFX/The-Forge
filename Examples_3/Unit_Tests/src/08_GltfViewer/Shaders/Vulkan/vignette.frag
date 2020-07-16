#version 450 core

precision highp float;
precision highp int; 
#extension GL_EXT_samplerless_texture_functions : enable

layout(location = 0) in vec2 fragInput_TEXCOORD;
layout(location = 0) out vec4 rast_FragData0; 

layout(set = 0, binding = 6) uniform texture2D sceneTexture;
layout(set = 0, binding = 7) uniform sampler clampMiplessLinearSampler;
struct VSOutput
{
    vec4 Position;
    vec2 TexCoord;
};
vec4 HLSLmain(VSOutput input1)
{
    vec4 src = texture(sampler2D(sceneTexture, clampMiplessLinearSampler), (input1).TexCoord);
    ivec2 dim = textureSize(sceneTexture, 0);
    vec2 uv = (input1).TexCoord;
    vec2 coord = (((vec2(2.0) * (uv - vec2(0.5))) * vec2(float(dim.x))) / vec2(float(dim.y)));
    float rf = (sqrt(dot(coord, coord)) * float(0.2));
    float rf2_1 = ((rf * rf) + float(1.0));
    float e = (float(1.0) / (rf2_1 * rf2_1));
    return vec4(((src).rgb * vec3(e)), 1.0);
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.TexCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}
