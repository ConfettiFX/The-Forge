/* Write your header comments here */
#include <metal_stdlib>
#include <metal_atomic>
using namespace metal;

struct Vertex_Shader
{
    struct Uniforms_uniformBlock
	{
        float4x4 vpMatrix;
        float4x4 modelMatrix;
    };
    constant Uniforms_uniformBlock & uniformBlock;
	
	struct Uniforms_boneMatrices
	{
		float4x4 boneMatrix[MAX_NUM_BONES];
	};
	constant Uniforms_boneMatrices & boneMatrices;
	
    struct VSInput
    {
        float3 Position [[attribute(0)]];
        float3 Normal [[attribute(1)]];
        float2 UV [[attribute(2)]];
        float4 BoneWeights [[attribute(3)]];
        uint4 BoneIndices [[attribute(4)]];
    };

    struct VSOutput
    {
        float4 Position [[position]];
        float3 Normal;
        float2 UV;
    };

    VSOutput main(VSInput input)
    {
		VSOutput result;
		
		float4x4 boneTransform = boneMatrices.boneMatrix[input.BoneIndices[0]] * input.BoneWeights[0];
		boneTransform += boneMatrices.boneMatrix[input.BoneIndices[1]] * input.BoneWeights[1];
		boneTransform += boneMatrices.boneMatrix[input.BoneIndices[2]] * input.BoneWeights[2];
		boneTransform += boneMatrices.boneMatrix[input.BoneIndices[3]] * input.BoneWeights[3];
		
		result.Position = boneTransform * float4(input.Position, 1.0f);
		result.Position = uniformBlock.modelMatrix * result.Position;
		result.Position = uniformBlock.vpMatrix * result.Position;
		result.Normal = normalize((uniformBlock.modelMatrix * float4(input.Normal, 0.0f)).xyz);
		result.UV = input.UV;
		
		return result;
    };

    Vertex_Shader(constant Uniforms_uniformBlock & uniformBlock,
				  constant Uniforms_boneMatrices & boneMatrices) :
	uniformBlock(uniformBlock),
	boneMatrices(boneMatrices){}
};

struct VSDataPerDraw {
    constant     Vertex_Shader::Uniforms_uniformBlock & uniformBlock;
    constant     Vertex_Shader::Uniforms_boneMatrices & boneMatrices;
};

vertex Vertex_Shader::VSOutput stageMain(Vertex_Shader::VSInput input [[stage_in]],
constant VSDataPerDraw& vsDataPerDraw [[buffer(UPDATE_FREQ_PER_DRAW)]]
) {
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.Normal = input.Normal;
	input0.UV = input.UV;
	input0.BoneWeights = input.BoneWeights;
	input0.BoneIndices = input.BoneIndices;
    Vertex_Shader main(vsDataPerDraw.uniformBlock, vsDataPerDraw.boneMatrices);
        return main.main(input0);
}
