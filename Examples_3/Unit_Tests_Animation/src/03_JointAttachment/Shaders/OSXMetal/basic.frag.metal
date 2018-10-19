/* Write your header comments here */
#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

inline float3x3 matrix_ctor(float4x4 m) {
        return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);
}
struct Fragment_Shader
{
#define MAX_JOINTS 815

	struct Uniforms_uniformBlock {
		
		float4x4 mvp;
		float4x4 toWorld[MAX_JOINTS];
		float4 color[MAX_JOINTS];
		
		float3 lightPosition;
		float3 lightColor;
	};
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


fragment float4 stageMain(Fragment_Shader::VSOutput input [[stage_in]],
						  constant     Fragment_Shader::Uniforms_uniformBlock & uniformBlock [[buffer(1)]]) {
    Fragment_Shader::VSOutput input0;
    input0.Position = float4(input.Position.xyz, 1.0 / input.Position.w);
    input0.Color = input.Color;
    Fragment_Shader main;
        return main.main(input0);
}
