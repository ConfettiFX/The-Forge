/* Write your header comments here */
#version 450 core

#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_control_flow_attributes : require

layout(location = 0) in vec2 TexCoord;
layout(location = 0) out vec4 rast_FragData0; 

layout(UPDATE_FREQ_NONE, binding = 11) uniform texture2D sceneTexture;
layout(UPDATE_FREQ_NONE, binding = 16) uniform sampler clampMiplessLinearSampler;

layout(row_major, push_constant) uniform FXAARootConstant_Block
{
    vec2 ScreenSize;
    uint Use;
    uint padding00;
}FXAARootConstant;

float rgb2luma(vec3 rgb)
{
    return sqrt(dot(rgb, vec3(0.299, 0.587, 0.114)));
}
vec3 FXAA(vec2 UV, ivec2 Pixel)
{
    float QUALITY[12] = {0.0, 0.0, 0.0, 0.0, 0.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0};

    vec3 colorCenter = texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(0, 0)).rgb;
    float lumaCenter = rgb2luma(colorCenter);
    float lumaD = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(0, -1)).rgb );
    float lumaU = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(0, 1)).rgb );
    float lumaL = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(-1, 0)).rgb );
    float lumaR = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(1, 0)).rgb );
    float lumaMin = min(lumaCenter, min(min(lumaD, lumaU), min(lumaL, lumaR)));
    float lumaMax = max(lumaCenter, max(max(lumaD, lumaU), max(lumaL, lumaR)));
    float lumaRange = (lumaMax - lumaMin);
    if((lumaRange < max(float (0.0312), (lumaMax * float (0.125)))))
    {
        return vec3 ((texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2(UV))).rgb);
    }
    float lumaDL = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(-1, -1)).rgb );
    float lumaUR = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(1, 1)).rgb );
    float lumaUL = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(-1, 1)).rgb );
    float lumaDR = rgb2luma( texelFetchOffset(sceneTexture, ivec2(Pixel.x, Pixel.y), 0, ivec2(1, -1)).rgb );
    float lumaDownUp = (lumaD + lumaU);
    float lumaLeftRight = (lumaL + lumaR);
    float lumaLeftCorners = (lumaDL + lumaUL);
    float lumaDownCorners = (lumaDL + lumaDR);
    float lumaRightCorners = (lumaDR + lumaUR);
    float lumaUpCorners = (lumaUR + lumaUL);
    float edgeHorizontal = ((abs(((float ((-2.0)) * lumaL) + lumaLeftCorners)) + (abs(((float ((-2.0)) * lumaCenter) + lumaDownUp)) * float (2.0))) + abs(((float ((-2.0)) * lumaR) + lumaRightCorners)));
    float edgeVertical = ((abs(((float ((-2.0)) * lumaU) + lumaUpCorners)) + (abs(((float ((-2.0)) * lumaCenter) + lumaLeftRight)) * float (2.0))) + abs(((float ((-2.0)) * lumaD) + lumaDownCorners)));
    float isHorizontal = (((edgeHorizontal >= edgeVertical))?(0.0):(1.0));
    float luma1 = mix(lumaD, lumaL, isHorizontal);
    float luma2 = mix(lumaU, lumaR, isHorizontal);
    float gradient1 = (luma1 - lumaCenter);
    float gradient2 = (luma2 - lumaCenter);
    bool is1Steepest = (abs(gradient1) >= abs(gradient2));
    float gradientScaled = (float (0.25) * max(abs(gradient1), abs(gradient2)));
    vec2 inverseScreenSize = vec2((float (1.0) / (FXAARootConstant.ScreenSize).x), (float (1.0) / (FXAARootConstant.ScreenSize).y));
    float stepLength = mix((inverseScreenSize).y, (inverseScreenSize).x, isHorizontal);
    float lumaLocalAverage = float (0.0);
    if(is1Steepest)
    {
        (stepLength = (-stepLength));
        (lumaLocalAverage = (float (0.5) * (luma1 + lumaCenter)));
    }
    else
    {
        (lumaLocalAverage = (float (0.5) * (luma2 + lumaCenter)));
    }
    vec2 currentUv = UV;
    if((isHorizontal < 0.5))
    {
        ((currentUv).y += (stepLength * float (0.5)));
    }
    else
    {
        ((currentUv).x += (stepLength * float (0.5)));
    }
    vec2 offset = mix(vec2((inverseScreenSize).x, 0.0), vec2(0.0, (inverseScreenSize).y), vec2 (isHorizontal));
    vec2 uv1 = (currentUv - offset);
    vec2 uv2 = (currentUv + offset);
    float lumaEnd1 = rgb2luma(vec3 ((texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2(uv1))).rgb));
    float lumaEnd2 = rgb2luma(vec3 ((texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2(uv2))).rgb));
    (lumaEnd1 -= lumaLocalAverage);
    (lumaEnd2 -= lumaLocalAverage);
    bool reached1 = (abs(lumaEnd1) >= gradientScaled);
    bool reached2 = (abs(lumaEnd2) >= gradientScaled);
    bool reachedBoth = (reached1 && reached2);
    if((!reached1))
    {
        (uv1 -= offset);
    }
    if((!reached2))
    {
        (uv2 += offset);
    }
    if((!reachedBoth))
    {
        [[unroll]] 
        for (int i = 2; (i < 12); (++i))
        {
            if((!reached1))
            {
                (lumaEnd1 = rgb2luma(vec3 ((texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2(uv1))).rgb)));
                (lumaEnd1 = (lumaEnd1 - lumaLocalAverage));
            }
            if((!reached2))
            {
                (lumaEnd2 = rgb2luma(vec3 ((texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2(uv2))).rgb)));
                (lumaEnd2 = (lumaEnd2 - lumaLocalAverage));
            }
            (reached1 = (abs(lumaEnd1) >= gradientScaled));
            (reached2 = (abs(lumaEnd2) >= gradientScaled));
            (reachedBoth = (reached1 && reached2));
            if((!reached1))
            {
                (uv1 -= (offset * vec2 (QUALITY[i])));
            }
            if((!reached2))
            {
                (uv2 += (offset * vec2 (QUALITY[i])));
            }
            if(reachedBoth)
            {
                break;
            }
        }
    }
    float distance1 = mix(((UV).x - (uv1).x), ((UV).y - (uv1).y), isHorizontal);
    float distance2 = mix(((uv2).x - (UV).x), ((uv2).y - (UV).y), isHorizontal);
    bool isDirection1 = (distance1 < distance2);
    float distanceFinal = min(distance1, distance2);
    float edgeThickness = (distance1 + distance2);
    float pixelOffset = (((-distanceFinal) / edgeThickness) + float (0.5));
    bool isLumaCenterSmaller = (lumaCenter < lumaLocalAverage);
    bool correctVariation = ((((isDirection1)?(lumaEnd1):(lumaEnd2)) < float (0.0)) != isLumaCenterSmaller);
    float finalOffset = ((correctVariation)?(pixelOffset):(float (0.0)));
    float lumaAverage = (float ((1.0 / 12.0)) * (((float (2.0) * (lumaDownUp + lumaLeftRight)) + lumaLeftCorners) + lumaRightCorners));
    float subPixelOffset1 = clamp((abs((lumaAverage - lumaCenter)) / lumaRange), float (0.0), float (1.0));
    float subPixelOffset2 = ((((float ((-2.0)) * subPixelOffset1) + float (3.0)) * subPixelOffset1) * subPixelOffset1);
    float subPixelOffsetFinal = ((subPixelOffset2 * subPixelOffset2) * float (0.75));
    (finalOffset = max(finalOffset, subPixelOffsetFinal));
    vec2 finalUv = UV;
    if((isHorizontal < 0.5))
    {
        ((finalUv).y += (finalOffset * stepLength));
    }
    else
    {
        ((finalUv).x += (finalOffset * stepLength));
    }
    return vec3 ((texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2(finalUv))).rgb);
}
struct PSIn
{
    vec4 Position;
    vec2 TexCoord;
};
vec4 HLSLmain(PSIn input1)
{
    vec3 result = vec3(0.0, 0.0, 0.0);
    if(bool (FXAARootConstant.Use))
    {
        (result = FXAA((input1).TexCoord, ivec2(((input1).TexCoord * FXAARootConstant.ScreenSize))));
    }
    else
    {
        (result = vec3 ((texture(sampler2D( sceneTexture, clampMiplessLinearSampler), vec2((input1).TexCoord))).rgb));
    }
    return vec4((result).r, (result).g, (result).b, 1.0);
}
void main()
{
    PSIn input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.TexCoord = TexCoord;
    vec4 result = HLSLmain(input1);
    rast_FragData0 = result;
}
