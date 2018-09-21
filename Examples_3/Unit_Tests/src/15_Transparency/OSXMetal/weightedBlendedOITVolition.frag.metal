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
        float opacitySensitivity = 3.0;
        float weightBias = 5.0;
        float precisionScalar = 10000.0;
        float maximumWeight = 20.0;
        float maximumColorValue = 1000.0;
        float additiveSensitivity = 10.0;
        float emissiveSensitivityValue = 0.5;
    };
    constant Uniforms_WBOITSettings & WBOITSettings;
    void weighted_oit_process(    thread float4(& accum),     thread float(& revealage),     thread float(& emissive_weight),     float4 premultiplied_alpha_color,     float raw_emissive_luminance,     float view_depth,     float current_camera_exposure)
    {
        float relative_emissive_luminance = (raw_emissive_luminance * current_camera_exposure);
        const float emissive_sensitivity = ((float)(1.0) / WBOITSettings.emissiveSensitivityValue);
        float clamped_emissive = saturate(relative_emissive_luminance);
        float clamped_alpha = saturate(premultiplied_alpha_color.a);
        float a = saturate(((clamped_alpha * WBOITSettings.opacitySensitivity) + (clamped_emissive * emissive_sensitivity)));
        const float canonical_near_z = 0.5;
        const float canonical_far_z = 300.0;
        float range = (canonical_far_z - canonical_near_z);
        float canonical_depth = saturate(((canonical_far_z / range) - ((canonical_far_z * canonical_near_z) / (view_depth * range))));
        float b = ((float)(1.0) - canonical_depth);
        float3 clamped_color = min(premultiplied_alpha_color.rgb, WBOITSettings.maximumColorValue);
        float w = (((WBOITSettings.precisionScalar * b) * b) * b);
        (w += WBOITSettings.weightBias);
        (w = min(w, WBOITSettings.maximumWeight));
        (w *= ((a * a) * a));
        (accum = float4((clamped_color * (float3)(w)), w));
        (revealage = clamped_alpha);
        (emissive_weight = (saturate((relative_emissive_luminance * WBOITSettings.additiveSensitivity)) / 8.0));
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
        float emissiveLuminance = dot(finalColor.rgb, float3(0.21260000, 0.71520000, 0.072200000));
        (output.Revealage = (float4)(0.0));
        float revealageX = 0.0;
        float revealageW = 0.0;
        weighted_oit_process(output.Accumulation, revealageX, revealageW, premultipliedColor, emissiveLuminance, d, 1.0);
        output.Revealage = float4(revealageX, 0.0, 0.0, revealageW);
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
