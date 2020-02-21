#version 450 core

precision highp float;
precision highp int; 

layout(location = 0) in vec3 POSITION;
layout(location = 1) in vec2 TEXCOORD;
layout(location = 0) out vec2 vertOutput_TEXCOORD;

struct VSInput
{
    vec3 Position;
    vec2 TexCoord;
};
struct VSOutput
{
    vec4 Position;
    vec2 TexCoord;
};
VSOutput HLSLmain(VSInput input1)
{
    VSOutput Out;
    ((Out).Position = vec4((input1).Position, 1.0));
    ((Out).TexCoord = (input1).TexCoord);
    return Out;
}
void main()
{
    VSInput input1;
    input1.Position = POSITION;
    input1.TexCoord = TEXCOORD;
    VSOutput result = HLSLmain(input1);
    gl_Position = result.Position;
    vertOutput_TEXCOORD = result.TexCoord;
}
