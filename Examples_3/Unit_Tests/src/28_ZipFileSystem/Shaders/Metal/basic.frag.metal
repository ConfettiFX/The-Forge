/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Fragment_Shader
{
    struct VSOutput
    {
        float4 Position;
        float4 Color;
    };
    float4 main(VSOutput input)
    {
        return (input).Color;
    };

    Fragment_Shader(
) {}
};


struct main_input
{
    float4 SV_POSITION [[position]];
    float4 COLOR;
};


fragment float4 stageMain(
	main_input inputData [[stage_in]])
{
    Fragment_Shader::VSOutput input0;
    input0.Position = inputData.SV_POSITION;
    input0.Color = inputData.COLOR;
    Fragment_Shader main;
    return main.main(input0);
}
