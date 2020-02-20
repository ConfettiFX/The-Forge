#version 450 core

precision highp float;
precision highp int; 

layout(location = 0) out vec2 vertOutput_TEXCOORD;

struct VSInput
{
    uint VertexID;
};
struct VSOutput
{
    vec4 Position;
    vec2 TexCoord;
};
VSOutput HLSLmain(VSInput input1)
{
    VSOutput Out;
    vec4 position;
    ((position).x = float(((((input1).VertexID == uint(1)))?(3.0):((-1.0)))));
    ((position).y = float(((((input1).VertexID == uint(0)))?((-3.0)):(1.0))));
    ((position).zw = vec2(1.0));
    ((Out).Position = position);
    ((Out).TexCoord = (((position).xy * vec2(0.5, (-0.5))) + vec2(0.5)));
    return Out;
}
void main()
{
    VSInput input1;
    input1.VertexID = gl_VertexIndex;
    VSOutput result = HLSLmain(input1);
    gl_Position = result.Position;
    vertOutput_TEXCOORD = result.TexCoord;
}
