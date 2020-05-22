/* Write your header comments here */
#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position [[position]];
        float4 Color;
    };

    float4 main(VSOutput input)
    {
        return input.Color;
    };

    Fragment_Shader() {}
};


fragment float4 stageMain(Fragment_Shader::VSOutput input [[stage_in]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.Color = input.Color;
    Fragment_Shader main;
    return main.main(input0);
}
