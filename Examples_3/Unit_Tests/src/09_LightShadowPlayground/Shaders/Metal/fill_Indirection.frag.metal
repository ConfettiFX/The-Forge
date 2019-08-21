/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float4 MiscData;
    };
    float4 main(VSOutput input)
    {
        return (input).MiscData;
    };

    Fragment_Shader(
) {}
};


fragment float4 stageMain(
    Fragment_Shader::VSOutput input [[stage_in]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.MiscData = input.MiscData;
    Fragment_Shader main;
    return main.main(input0);
}
