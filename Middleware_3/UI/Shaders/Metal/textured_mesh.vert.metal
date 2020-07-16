#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VsIn
    {
        float2 position [[attribute(0)]];
        float2 texcoord [[attribute(1)]];
    };
    struct VsOut
    {
        float4 position [[position]];
        float2 texcoord;
    };
    struct Uniforms_uRootConstants
    {
        packed_float4 color;
        packed_float2 scaleBias;
    };
    constant Uniforms_uRootConstants & uRootConstants;
    VsOut main(VsIn input)
    {
		VsOut output;
        ((output).position = float4(((((input).position).xy * (uRootConstants.scaleBias).xy) + float2((-1.0), 1.0)), 0.0, 1.0));
        ((output).texcoord = (input).texcoord);
        return output;
    };

    Vertex_Shader(constant Uniforms_uRootConstants & uRootConstants)
    : uRootConstants(uRootConstants)
    {
    }
};


vertex Vertex_Shader::VsOut stageMain(
    Vertex_Shader::VsIn input                                        [[stage_in]],
    constant Vertex_Shader::Uniforms_uRootConstants& uRootConstants  [[buffer(0)]])
{
    Vertex_Shader::VsIn input0;
    input0.position = input.position;
    input0.texcoord = input.texcoord;
    Vertex_Shader main(uRootConstants);
    return main.main(input0);
}
