/* Write your header comments here */
#include <metal_stdlib>
using namespace metal;

#ifndef MAX_NUM_OBJECTS
    #define MAX_NUM_OBJECTS 64
#endif

struct Vertex_Shader
{
    struct ObjectInfo
    {
        float4 color;
        float4x4 toWorld;
    };
    struct Uniforms_ObjectUniformBlock
    {
        float4x4 viewProj;
        ObjectInfo objectInfo[MAX_NUM_OBJECTS];
    };
    constant Uniforms_ObjectUniformBlock & ObjectUniformBlock;
    struct Uniforms_DrawInfoRootConstant
    {
        uint baseInstance = 0;
    };
    struct VSInput
    {
        float4 Position [[attribute(0)]];
        float4 Normal [[attribute(1)]];
    };
    struct VSOutput
    {
        float4 Position [[position]];
        float4 WorldPosition;
        float4 Color;
        float4 Normal;
    };
    VSOutput main(    VSInput input,     uint InstanceID)
    {
        VSOutput output;
        (output.Normal = normalize(((ObjectUniformBlock.objectInfo[InstanceID].toWorld)*(float4(input.Normal.xyz, 0)))));
        float4x4 mvp = ((ObjectUniformBlock.viewProj)*(ObjectUniformBlock.objectInfo[InstanceID].toWorld));
        (output.Position = ((mvp)*(input.Position)));
        (output.WorldPosition = ((ObjectUniformBlock.objectInfo[InstanceID].toWorld)*(input.Position)));
        (output.Color = ObjectUniformBlock.objectInfo[InstanceID].color);
        return output;
    };

    Vertex_Shader(

constant Uniforms_ObjectUniformBlock & ObjectUniformBlock) : ObjectUniformBlock(ObjectUniformBlock) {}
};


vertex Vertex_Shader::VSOutput stageMain(
Vertex_Shader::VSInput input [[stage_in]],
uint InstanceID [[instance_id]],
    constant Vertex_Shader::Uniforms_ObjectUniformBlock & ObjectUniformBlock [[buffer(1)]],
    constant Vertex_Shader::Uniforms_DrawInfoRootConstant & DrawInfoRootConstant [[buffer(2)]])
{
    Vertex_Shader::VSInput input0;
    input0.Position = input.Position;
    input0.Normal = input.Normal;
    uint InstanceID0;
    InstanceID0 = InstanceID + DrawInfoRootConstant.baseInstance;
    Vertex_Shader main(ObjectUniformBlock);
    return main.main(input0, InstanceID0);
}
