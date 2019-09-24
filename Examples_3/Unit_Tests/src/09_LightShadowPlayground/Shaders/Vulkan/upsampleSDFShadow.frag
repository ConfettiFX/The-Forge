/* Write your header comments here */
#version 450 core

#extension GL_GOOGLE_include_directive : enable


#include "SDF_Constant.h"

layout(location = 0) in vec2 fragInput_TEXCOORD0;
layout(location = 0) out float rast_FragData0; 

layout(UPDATE_FREQ_NONE, binding = 0) uniform texture2D SDFShadowTexture;
layout(UPDATE_FREQ_NONE, binding = 1) uniform texture2D DepthTexture;
layout(UPDATE_FREQ_NONE, binding = 2) uniform sampler clampMiplessLinearSampler;
layout(UPDATE_FREQ_NONE, binding = 3) uniform sampler clampMiplessNearSampler;
layout(column_major, UPDATE_FREQ_PER_FRAME, binding = 4) uniform cameraUniformBlock
{
    mat4 View;
    mat4 Project;
    mat4 ViewProject;
    layout(row_major) mat4 InvView;
    mat4 InvProj;
    mat4 InvViewProject;
    vec4 mCameraPos;
    float mNear;
    float mFarNearDiff;
    float mFarNear;
    float paddingForAlignment0;
    vec2 mTwoOverRes;
    float _pad1;
    float _pad2;
    vec2 mWindowSize;
    float _pad3;
    float _pad4;
    vec4 mDeviceZToWorldZ;
};

struct PsIn
{
    vec4 Position;
    vec2 TexCoord;
};
struct PsOut
{
    float FinalColor;
};
float ConvertFromDeviceZ(float deviceZ)
{
    return (((deviceZ * mDeviceZToWorldZ[0]) + mDeviceZToWorldZ[1]) + (1.0 / ((deviceZ * mDeviceZToWorldZ[2]) - mDeviceZToWorldZ[3])));
}
PsOut HLSLmain(PsIn input1)
{
    PsOut output1;
    vec2 UV = ((input1).TexCoord).xy;
    float depthVal = texture(sampler2D( DepthTexture, clampMiplessNearSampler), vec2(UV)).x;
    float worldDepth = ConvertFromDeviceZ(depthVal);
    vec2 downSampledTextureSize = floor(((mWindowSize).xy / vec2 (SDF_SHADOW_DOWNSAMPLE_VALUE)));
    vec2 downSampledTexelSize = (vec2 (1.0) / (downSampledTextureSize).xy);
    vec2 cornerUV = ((floor((((UV).xy * downSampledTextureSize) - vec2 (0.5))) / (downSampledTextureSize).xy) + (vec2 (0.5) * downSampledTexelSize));
    vec2 billinearWeights = (((UV).xy - (cornerUV).xy) * downSampledTextureSize);
    vec2 textureValues_00 = (texture(sampler2D( SDFShadowTexture, clampMiplessLinearSampler), vec2(cornerUV))).xy;
    vec2 textureValues_10 = (texture(sampler2D( SDFShadowTexture, clampMiplessLinearSampler), vec2((cornerUV + vec2((downSampledTexelSize).x, 0.0))))).xy;
    vec2 textureValues_01 = (texture(sampler2D( SDFShadowTexture, clampMiplessLinearSampler), vec2((cornerUV + vec2(0.0, (downSampledTexelSize).y))))).xy;
    vec2 textureValues_11 = (texture(sampler2D( SDFShadowTexture, clampMiplessLinearSampler), vec2((cornerUV + downSampledTexelSize)))).xy;
    vec4 cornerWeights = vec4(((1.0 - (billinearWeights).y) * (1.0 - (billinearWeights).x)), ((1.0 - (billinearWeights).y) * (billinearWeights).x), ((1.0 - (billinearWeights).x) * (billinearWeights).y), ((billinearWeights).x * (billinearWeights).y));
    float epilson = pow(10.0,float((-4.0)));
    vec4 cornerDepths = abs(vec4((textureValues_00).y, (textureValues_10).y, (textureValues_01).y, (textureValues_11).y));
    vec4 depthWeights = (vec4 (1.0) / (abs((cornerDepths - vec4(worldDepth))) + vec4 (epilson)));
    vec4 finalWeights = (cornerWeights * depthWeights);
    float interpolatedResult = (((((finalWeights).x * (textureValues_00).x) + ((finalWeights).y * (textureValues_10).x)) + ((finalWeights).z * (textureValues_01).x)) + ((finalWeights).w * (textureValues_11).x));
    (interpolatedResult /= dot(finalWeights, vec4 (1)));
    float outputVal = interpolatedResult;
    ((output1).FinalColor = outputVal);
	//output1.FinalColor = input1.TexCoord.y;
    return output1;
}
void main()
{
    PsIn input1;
    input1.Position = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    input1.TexCoord = fragInput_TEXCOORD0;
    PsOut result = HLSLmain(input1);
    rast_FragData0 = result.FinalColor;
}
