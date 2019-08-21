/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

struct Vertex_Shader
{
	struct Uniforms_objectUniformBlock
	{
		float4x4 WorldViewProjMat;
		float4x4 WorldMat;
	};
	constant Uniforms_objectUniformBlock & objectUniformBlock;
	struct VsIn
	{
		float3 Position [[attribute(0)]];
	};
	struct PsIn
	{
		float4 Position [[position]];
	};
	PsIn main(VsIn input)
	{
		PsIn output;
		((output).Position = ((objectUniformBlock.WorldViewProjMat)*(float4(((input).Position).xyz, 1.0))));
		return output;
	};
	
	Vertex_Shader(
				  constant Uniforms_objectUniformBlock & objectUniformBlock) :
	objectUniformBlock(objectUniformBlock) {}
};


vertex Vertex_Shader::PsIn stageMain(
									 Vertex_Shader::VsIn input [[stage_in]],
									 constant Vertex_Shader::Uniforms_objectUniformBlock & objectUniformBlock [[buffer(2)]])
{
	Vertex_Shader::VsIn input0;
	input0.Position = input.Position;
	Vertex_Shader main(
					   objectUniformBlock);
	return main.main(input0);
}
