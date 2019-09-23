/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    texture2d<float> SDFShadowTexture;
    texture2d<float> DepthTexture;
    sampler clampMiplessLinearSampler;
    sampler clampMiplessNearSampler;
    struct Uniforms_cameraUniformBlock
    {
        float4x4 View;
        float4x4 Project;
        float4x4 ViewProject;
        float4x4 InvView;
        float4x4 InvProj;
        float4x4 InvViewProject;
        float4 mCameraPos;
        float mNear;
        float mFarNearDiff;
        float mFarNear;
        float paddingForAlignment0;
        float2 mTwoOverRes;
        float _pad1;
        float _pad2;
        float2 mWindowSize;
        float _pad3;
        float _pad4;
        float4 mDeviceZToWorldZ;
    };
    constant Uniforms_cameraUniformBlock & cameraUniformBlock;
    struct PsIn
    {
        float4 Position [[position]];
        float2 TexCoord;
    };
    struct PsOut
    {
        float4 FinalColor [[color(0)]];
    };
    float ConvertFromDeviceZ(float deviceZ)
    {
        return (((deviceZ * cameraUniformBlock.mDeviceZToWorldZ[0]) + cameraUniformBlock.mDeviceZToWorldZ[1]) + (1.0 / ((deviceZ * cameraUniformBlock.mDeviceZToWorldZ[2]) - cameraUniformBlock.mDeviceZToWorldZ[3])));
    };
    PsOut main(PsIn input)
    {
        PsOut output;
        float2 UV = ((input).TexCoord).xy;
        float depthVal = (DepthTexture.sample(clampMiplessNearSampler, UV)).r;
        float worldDepth = ConvertFromDeviceZ(depthVal);
        float2 downSampledTextureSize = floor(((cameraUniformBlock.mWindowSize).xy / (float2)(2)));
        float2 downSampledTexelSize = ((float2)(1.0) / (downSampledTextureSize).xy);
        float2 cornerUV = ((floor((((UV).xy * downSampledTextureSize) - (float2)(0.5))) / (downSampledTextureSize).xy) + ((float2)(0.5) * downSampledTexelSize));
        float2 billinearWeights = (((UV).xy - (cornerUV).xy) * downSampledTextureSize);
        float2 textureValues_00 = (float2)(SDFShadowTexture.sample(clampMiplessLinearSampler, cornerUV).xy);
        float2 textureValues_10 = (float2)(SDFShadowTexture.sample(clampMiplessLinearSampler, (cornerUV + float2((downSampledTexelSize).x, 0.0))).xy);
        float2 textureValues_01 = (float2)(SDFShadowTexture.sample(clampMiplessLinearSampler, (cornerUV + float2(0.0, (downSampledTexelSize).y))).xy);
        float2 textureValues_11 = (float2)(SDFShadowTexture.sample(clampMiplessLinearSampler, (cornerUV + downSampledTexelSize)).xy);
        float4 cornerWeights = float4(((1.0 - (billinearWeights).y) * (1.0 - (billinearWeights).x)), ((1.0 - (billinearWeights).y) * (billinearWeights).x), ((1.0 - (billinearWeights).x) * (billinearWeights).y), ((billinearWeights).x * (billinearWeights).y));
        float epilson = pow(10.0, (-4.0));
        float4 cornerDepths = abs(float4((textureValues_00).y, (textureValues_10).y, (textureValues_01).y, (textureValues_11).y));
        float4 depthWeights = ((float4)(1.0) / (abs((cornerDepths - (worldDepth))) + (float4)(epilson)));
        float4 finalWeights = (cornerWeights * depthWeights);
        float interpolatedResult = (((((finalWeights).x * (textureValues_00).x) + ((finalWeights).y * (textureValues_10).x)) + ((finalWeights).z * (textureValues_01).x)) + ((finalWeights).w * (textureValues_11).x));
        (interpolatedResult /= dot(finalWeights, 1));
        float outputVal = interpolatedResult;
        ((output).FinalColor = outputVal);
        return output;
    };

    Fragment_Shader(
texture2d<float> SDFShadowTexture,texture2d<float> DepthTexture,sampler clampMiplessLinearSampler,sampler clampMiplessNearSampler,constant Uniforms_cameraUniformBlock & cameraUniformBlock) :
SDFShadowTexture(SDFShadowTexture),DepthTexture(DepthTexture),clampMiplessLinearSampler(clampMiplessLinearSampler),clampMiplessNearSampler(clampMiplessNearSampler),cameraUniformBlock(cameraUniformBlock) {}
};

struct FSData {
    texture2d<float> SDFShadowTexture   [[id(0)]];
    texture2d<float> DepthTexture       [[id(1)]];
    sampler clampMiplessLinearSampler   [[id(2)]];
    sampler clampMiplessNearSampler     [[id(3)]];
};

struct FSDataPerFrame {
    constant Fragment_Shader::Uniforms_cameraUniformBlock & cameraUniformBlock [[id(0)]];
};

fragment Fragment_Shader::PsOut stageMain(
    Fragment_Shader::PsIn input [[stage_in]],
    constant FSData& fsData [[buffer(UPDATE_FREQ_NONE)]],
    constant FSDataPerFrame& fsDataPerFrame [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    Fragment_Shader::PsIn input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.TexCoord = input.TexCoord;
    Fragment_Shader main(fsData.SDFShadowTexture, fsData.DepthTexture, fsData.clampMiplessLinearSampler, fsData.clampMiplessNearSampler, fsDataPerFrame.cameraUniformBlock);
    return main.main(input0);
}
