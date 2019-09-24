/* Write your header comments here */
#version 450 core


layout(location = 0) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out vec4 rast_FragData0; 

struct VSOutput
{
    vec4 Position;
    vec2 Tex_Coord;
};
layout(UPDATE_FREQ_NONE, binding = 1) uniform sampler clampNearSampler;
layout(UPDATE_FREQ_NONE, binding = 2) uniform texture2D screenTexture;

vec4 HLSLmain(VSOutput input1)
{
    //float rcolor = float ((texture(sampler2D( screenTexture, clampNearSampler), vec2((input1).Tex_Coord))).x);
	vec3 rcolor = vec3 (
		texture(sampler2D( screenTexture, clampNearSampler), input1.Tex_Coord)
	);
    //vec3 color = vec3(rcolor, rcolor, rcolor);
	vec3 color = vec3(rcolor);
    return vec4(color, 1.0);
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.Tex_Coord = fragInput_TEXCOORD0;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}
