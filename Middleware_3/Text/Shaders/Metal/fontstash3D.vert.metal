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
    struct Uniforms_uniformBlock
    {
        float4x4 mvp;
    };
    constant Uniforms_uniformBlock & uniformBlock;
    PsIn main(VsIn In)
    {
        PsIn Out;
        ((Out).position = ((uniformBlock.mvp)*(float4(((In).position * (uRootConstants.scaleBias).xy), 1.0, 1.0))));
        ((Out).texCoord = (In).texCoord);
        return Out;
    };

    Vertex_Shader(constant Uniforms_uRootConstants & uRootConstants,constant Uniforms_uniformBlock & uniformBlock)
        : uRootConstants(uRootConstants)
        , uniformBlock(uniformBlock)
    {
    }
};

vertex Vertex_Shader::PsIn stageMain(
                                     Vertex_Shader::VsIn In                                                   [[stage_in]],
                                     constant Vertex_Shader::Uniforms_uRootConstants& uRootConstants       [[buffer(0)]],
									 constant Vertex_Shader::Uniforms_uniformBlock& uniformBlock_rootcbv [[buffer(1)]]
)
{
    Vertex_Shader::VsIn In0;
    In0.position = In.position;
    In0.texCoord = In.texCoord;
    Vertex_Shader main(uRootConstants, uniformBlock_rootcbv);
    return main.main(In0);
}
