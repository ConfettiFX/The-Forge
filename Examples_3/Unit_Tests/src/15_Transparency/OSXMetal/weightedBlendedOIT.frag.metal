/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    #define SPECULAR_EXP 10.0

    struct VSOutput
    {
        float4 Position [[position]];
        float4 WorldPosition;
        float4 Color;
        float4 Normal;
    };
    struct PSOutput
    {
        float4 Accumulation [[color(0)]];
        float4 Revealage [[color(1)]];
    };
    struct Uniforms_LightUniformBlock
    {
        float4x4 lightViewProj;
        float4 lightDirection;
        float4 lightColor;
    };
    constant Uniforms_LightUniformBlock & LightUniformBlock;
    struct Uniforms_CameraUniform
    {
        float4 CameraPosition;
    };
    constant Uniforms_CameraUniform & CameraUniform;
    struct Uniforms_WBOITSettings
    {
        float colorResistance;
        float rangeAdjustment;
        float depthRange;
        float orderingStrength;
        float underflowLimit;
        float overflowLimit;
    };
    constant Uniforms_WBOITSettings & WBOITSettings;
    float WeightFunction(    float alpha,     float depth)
    {
        return (pow(alpha, WBOITSettings.colorResistance) * clamp((0.3 / (0.000010000000 + pow((depth / WBOITSettings.depthRange), WBOITSettings.orderingStrength))), WBOITSettings.underflowLimit, WBOITSettings.overflowLimit));
    };
    PSOutput main(    VSOutput input)
    {
        PSOutput output;
        float3 normal = normalize(input.Normal.xyz);
        float3 lightVec = (-normalize(LightUniformBlock.lightDirection.xyz));
        float3 viewVec = normalize((input.WorldPosition.xyz - CameraUniform.CameraPosition.xyz));
        float dotP = dot(normal, lightVec.xyz);
        if ((dotP < 0.05))
        {
            (dotP = 0.05);
        }
        float3 diffuse = ((LightUniformBlock.lightColor.xyz * input.Color.xyz) * (float3)(dotP));
        float3 specular = (LightUniformBlock.lightColor.xyz * (float3)(pow(saturate(dot(reflect(lightVec, normal), viewVec)), SPECULAR_EXP)));
        float4 finalColor = float4(saturate((diffuse + (specular * (float3)(0.5)))), input.Color.a);
        float d = (input.Position.z / input.Position.w);
        float4 premultipliedColor = float4((finalColor.rgb * (float3)(finalColor.a)), finalColor.a);
        float w = WeightFunction(premultipliedColor.a, d);
        (output.Accumulation = (premultipliedColor * (float4)(w)));
        (output.Revealage = premultipliedColor.aaaa);
        return output;
    };

    Fragment_Shader(
constant Uniforms_LightUniformBlock & LightUniformBlock,constant Uniforms_CameraUniform & CameraUniform,constant Uniforms_WBOITSettings & WBOITSettings) :
LightUniformBlock(LightUniformBlock),CameraUniform(CameraUniform),WBOITSettings(WBOITSettings) {}
};


fragment Fragment_Shader::PSOutput stageMain(
Fragment_Shader::VSOutput input [[stage_in]],
    constant Fragment_Shader::Uniforms_LightUniformBlock & LightUniformBlock [[buffer(0)]],
    constant Fragment_Shader::Uniforms_CameraUniform & CameraUniform [[buffer(1)]],
    constant Fragment_Shader::Uniforms_WBOITSettings & WBOITSettings [[buffer(2)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.WorldPosition = input.WorldPosition;
    input0.Color = input.Color;
    input0.Normal = input.Normal;
    Fragment_Shader main(LightUniformBlock, CameraUniform, WBOITSettings);
    return main.main(input0);
}
