/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
    struct VSInput
    {
        float3 Position;
        float3 Normal;
        float2 TexCoords;
    };
    struct VSOutput
    {
        float4 Position;
        float2 texcoords;
    };
    struct Uniforms_UniformBlock
    {
        float4x4 mProjectionViewMat;
        float4x4 mModelMatrixCapsule;
        float4x4 mModelMatrixCube;
    };
    constant Uniforms_UniformBlock & UniformBlockCube;
    VSOutput main(VSInput input)
    {
        VSOutput result;
        float4x4 mvp = ((UniformBlockCube.mProjectionViewMat)*(UniformBlockCube.mModelMatrixCube));
        ((result).Position = ((mvp)*(float4(((input).Position).xyz, 1.0))));
        ((result).texcoords = (input).TexCoords);
        return result;
    };

    Vertex_Shader(
constant Uniforms_UniformBlock & UniformBlockCube) :
UniformBlockCube(UniformBlockCube) {}
};


struct main_input
{
    float3 POSITION [[attribute(0)]];
    float3 NORMAL [[attribute(1)]];
    float2 TEXCOORD [[attribute(2)]];
};

struct main_output
{
    float4 SV_POSITION [[position]];
    float2 TEXCOORD;
};

vertex main_output stageMain(
	main_input inputData [[stage_in]],
    constant Vertex_Shader::Uniforms_UniformBlock& uniformBlock [[buffer(0)]]
)
{
    Vertex_Shader::VSInput input0;
    input0.Position = inputData.POSITION;
    input0.Normal = inputData.NORMAL;
    input0.TexCoords = inputData.TEXCOORD;
    Vertex_Shader main(
    uniformBlock);
    Vertex_Shader::VSOutput result = main.main(input0);
    main_output output;
    output.SV_POSITION = result.Position;
    output.TEXCOORD = result.texcoords;
    return output;
}
