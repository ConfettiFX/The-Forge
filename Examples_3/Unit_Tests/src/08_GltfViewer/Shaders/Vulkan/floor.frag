#version 450 core

precision highp float;
precision highp int; 
vec4 MulMat(mat4 lhs, vec4 rhs)
{
    vec4 dst;
	dst[0] = lhs[0][0]*rhs[0] + lhs[0][1]*rhs[1] + lhs[0][2]*rhs[2] + lhs[0][3]*rhs[3];
	dst[1] = lhs[1][0]*rhs[0] + lhs[1][1]*rhs[1] + lhs[1][2]*rhs[2] + lhs[1][3]*rhs[3];
	dst[2] = lhs[2][0]*rhs[0] + lhs[2][1]*rhs[1] + lhs[2][2]*rhs[2] + lhs[2][3]*rhs[3];
	dst[3] = lhs[3][0]*rhs[0] + lhs[3][1]*rhs[1] + lhs[3][2]*rhs[2] + lhs[3][3]*rhs[3];
    return dst;
}


layout(location = 0) in vec3 fragInput_POSITION;
layout(location = 1) in vec2 fragInput_TEXCOORD;
layout(location = 0) out vec4 rast_FragData0; 

const float NUM_SHADOW_SAMPLES_INV = float(0.03125);
const float shadowSamples[64] = {(-0.17466460), (-0.79131840), (-0.129792), (-0.44771160), 0.08863912, (-0.8981690), (-0.58914988), (-0.678163), 0.17484090, (-0.5252063), 0.6483325, (-0.752117), 0.45293192, (-0.384986), 0.09757467, (-0.1166954), 0.3857658, (-0.9096935), 0.56130584, (-0.1283066), 0.768011, (-0.4906538), 0.8499438, (-0.220937), 0.6946555, 0.16058660, 0.9614297, 0.0597522, 0.7986544, 0.53259124, 0.45139648, 0.5592551, 0.2847693, 0.2293397, (-0.2118996), (-0.1609127), (-0.4357893), (-0.3808875), (-0.4662672), (-0.05288446), (-0.139129), 0.23940650, 0.1781853, 0.5254948, 0.4287854, 0.899425, 0.12893490, 0.8724155, (-0.6924323), (-0.2203967), (-0.48997), 0.2795907, (-0.26117242), 0.7359962, (-0.7704172), 0.42331340, (-0.8501040), 0.12639350, (-0.83452672), (-0.499136), (-0.5380967), 0.6264234, (-0.9769312), (-0.15505689)};
layout(row_major, set = 1, binding = 0) uniform cbPerPass
{
    mat4 projView;
    mat4 shadowLightViewProj;
    vec4 camPos;
    vec4 lightColor[4];
    vec4 lightDirection[3];
};

layout(set = 0, binding = 14) uniform texture2D ShadowTexture;
layout(set = 0, binding = 7) uniform sampler clampMiplessLinearSampler;
struct VSOutput
{
    vec4 Position;
    vec3 WorldPos;
    vec2 TexCoord;
};
float CalcESMShadowFactor(vec3 worldPos)
{
    vec4 posLS = MulMat(shadowLightViewProj,vec4((worldPos).xyz, 1.0));
    (posLS /= vec4((posLS).w));
    ((posLS).y *= float((-1)));
    ((posLS).xy = (((posLS).xy * vec2(0.5)) + vec2(0.5, 0.5)));
    vec2 HalfGaps = vec2(0.00048828124, 0.00048828124);
    vec2 Gaps = vec2(0.0009765625, 0.0009765625);
    ((posLS).xy += HalfGaps);
    float shadowFactor = float(1.0);
    vec4 shadowDepthSample = vec4(0, 0, 0, 0);
    ((shadowDepthSample).x = (textureLod(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0))).r);
    ((shadowDepthSample).y = (textureLodOffset(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0), ivec2(1, 0))).r);
    ((shadowDepthSample).z = (textureLodOffset(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0), ivec2(0, 1))).r);
    ((shadowDepthSample).w = (textureLodOffset(sampler2D(ShadowTexture, clampMiplessLinearSampler), (posLS).xy, float(0), ivec2(1, 1))).r);
    float avgShadowDepthSample = (((((shadowDepthSample).x + (shadowDepthSample).y) + (shadowDepthSample).z) + (shadowDepthSample).w) * 0.25);
    (shadowFactor = clamp((float(2.0) - exp((((posLS).z - avgShadowDepthSample) * 1.0))), 0.0, 1.0));
    return shadowFactor;
}
float random(vec3 seed, vec3 freq)
{
    float dt = dot(floor((seed * freq)), vec3(53.12149811, 21.1352005, 9.13220024));
    return fract((sin(dt) * float(2105.23535156)));
}
float CalcPCFShadowFactor(vec3 worldPos)
{
    vec4 posLS = MulMat(shadowLightViewProj,vec4((worldPos).xyz, 1.0));
    (posLS /= vec4((posLS).w));
    ((posLS).y *= float((-1)));
    ((posLS).xy = (((posLS).xy * vec2(0.5)) + vec2(0.5, 0.5)));
    vec2 HalfGaps = vec2(0.00048828124, 0.00048828124);
    vec2 Gaps = vec2(0.0009765625, 0.0009765625);
    ((posLS).xy += HalfGaps);
    float shadowFactor = float(1.0);
    float shadowFilterSize = float(0.0016000000);
    float angle = random(worldPos, vec3(20.0));
    float s = sin(angle);
    float c = cos(angle);
    for (int i = 0; (i < 32); (i++))
    {
        vec2 offset = vec2(shadowSamples[(i * 2)], shadowSamples[((i * 2) + 1)]);
        (offset = vec2((((offset).x * c) + ((offset).y * s)), (((offset).x * (-s)) + ((offset).y * c))));
        (offset *= vec2(shadowFilterSize));
        float shadowMapValue = float(textureLod(sampler2D(ShadowTexture, clampMiplessLinearSampler), ((posLS).xy + offset), float(0)));
        (shadowFactor += ((((shadowMapValue - 0.0020000000) > (posLS).z))?(0.0):(1.0)));
    }
    (shadowFactor *= NUM_SHADOW_SAMPLES_INV);
    return shadowFactor;
}
float ClaculateShadow(vec3 worldPos)
{
    vec4 NDC = MulMat(shadowLightViewProj,vec4(worldPos, 1.0));
    (NDC /= vec4((NDC).w));
    float Depth = (NDC).z;
    vec2 ShadowCoord = vec2((((NDC).x + float(1.0)) * float(0.5)), ((float(1.0) - (NDC).y) * float(0.5)));
    float ShadowDepth = (texture(sampler2D(ShadowTexture, clampMiplessLinearSampler), ShadowCoord)).r;
    if(((ShadowDepth - 0.0020000000) > Depth))
    {
        return 0.1;
    }
    else
    {
        return 1.0;
    }
}
vec4 HLSLmain(VSOutput input1)
{
    vec3 color = vec3(1.0, 1.0, 1.0);
    (color *= vec3(CalcPCFShadowFactor((input1).WorldPos)));
    float i = (float(1.0) - length(abs(((input1).TexCoord).xy)));
    (i = pow(i,float(1.20000004)));
    return vec4((color).rgb, i);
}
void main()
{
    VSOutput input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.WorldPos = fragInput_POSITION;
    input1.TexCoord = fragInput_TEXCOORD;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}
