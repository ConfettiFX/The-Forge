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
    float4 main(    VSOutput input)
    {
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
        float3 finalColor = saturate((diffuse + (specular * (float3)(0.5))));
        return float4(finalColor.xyz, input.Color.a);
    };

    Fragment_Shader(
constant Uniforms_LightUniformBlock & LightUniformBlock,constant Uniforms_CameraUniform & CameraUniform) :
LightUniformBlock(LightUniformBlock),CameraUniform(CameraUniform) {}
};


fragment float4 stageMain(
Fragment_Shader::VSOutput input [[stage_in]],
    constant Fragment_Shader::Uniforms_LightUniformBlock & LightUniformBlock [[buffer(0)]],
    constant Fragment_Shader::Uniforms_CameraUniform & CameraUniform [[buffer(1)]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.WorldPosition = input.WorldPosition;
    input0.Color = input.Color;
    input0.Normal = input.Normal;
    Fragment_Shader main(LightUniformBlock, CameraUniform);
    return main.main(input0);
}
