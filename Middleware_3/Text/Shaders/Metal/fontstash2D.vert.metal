#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VsIn
    {
        float2 position [[attribute(0)]];
        float2 texCoord [[attribute(1)]];
    };
    struct PsIn
    {
        float4 position [[position]];
        float2 texCoord;
    };
    struct Uniforms_uRootConstants
    {
        packed_float4 color;
        float2 scaleBias;
    };
    constant Uniforms_uRootConstants & uRootConstants;
    PsIn main(VsIn In)
    {
        PsIn Out;
        ((Out).position = float4((In).position, 0.0, 1.0));
        (((Out).position).xy = ((((Out).position).xy * (uRootConstants.scaleBias).xy) + float2((-1.0), 1.0)));
        ((Out).texCoord = (In).texCoord);
        return Out;
    };

    Vertex_Shader(constant Uniforms_uRootConstants & uRootConstants)
        : uRootConstants(uRootConstants)
    {
    }
};

vertex Vertex_Shader::PsIn stageMain(
                                     Vertex_Shader::VsIn In                                           [[stage_in]],
                                     constant Vertex_Shader::Uniforms_uRootConstants& uRootConstants [[buffer(0)]]
)
{
    Vertex_Shader::VsIn In0;
    In0.position = In.position;
    In0.texCoord = In.texCoord;
    Vertex_Shader main(uRootConstants);
    return main.main(In0);
}
