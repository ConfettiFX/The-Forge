/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

#include "vb_argument_buffers.h"

struct Vertex_Shader
{
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
		((output).Position = ((objectUniformBlock.mWorldViewProjMat)*(float4(((input).Position).xyz, 1.0))));
		return output;
	};
	
	Vertex_Shader(
				  constant Uniforms_objectUniformBlock & objectUniformBlock) :
	objectUniformBlock(objectUniformBlock) {}
};

vertex Vertex_Shader::PsIn stageMain(
    Vertex_Shader::VsIn input [[stage_in]],
    constant ArgDataPerDraw& vsData     [[buffer(UPDATE_FREQ_PER_DRAW)]]
)
{
	Vertex_Shader::VsIn input0;
	input0.Position = input.Position;
	Vertex_Shader main(vsData.objectUniformBlock);
	return main.main(input0);
}
