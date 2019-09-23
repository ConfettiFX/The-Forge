/* Write your header comments here */
#version 450 core


layout(location = 0) in vec2 fragInput_TEXCOORD0;
layout(location = 1) in vec4 fragInput_TEXCOORD1;
layout(location = 0) out float rast_FragData0; 

struct VSOutput
{
    vec4 Position;
    vec2 UV;
    vec4 MiscData;
};
layout(UPDATE_FREQ_NONE, binding = 1) uniform sampler clampToEdgeNearSampler;
layout(UPDATE_FREQ_NONE, binding = 0) uniform texture2D DepthPassTexture;
float HLSLmain(VSOutput input1)
{
    float tileDepth = float (textureLod(sampler2D( DepthPassTexture, clampToEdgeNearSampler), vec2((input1).UV), float (0.0)));
    return tileDepth;
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.UV = fragInput_TEXCOORD0;
    input1.MiscData = fragInput_TEXCOORD1;
    float result = HLSLmain(input1);
    rast_FragData0 = result;
}
