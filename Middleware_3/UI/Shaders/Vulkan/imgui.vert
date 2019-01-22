#version 450 core

layout(location = 0) in vec2 POSITION;
layout(location = 1) in vec2 TEXCOORD0;
layout(location = 2) in vec4 COLOR0;
layout(location = 0) out vec4 vertOutput_COLOR0;
layout(location = 1) out vec2 vertOutput_TEXCOORD0;

layout(set = 0, binding = 0) uniform uniformBlockVS
{
    mat4 ProjectionMatrix;
};

struct VS_INPUT
{
    vec2 pos;
    vec2 uv;
    vec4 col;
};

struct PS_INPUT
{
    vec4 pos;
    vec4 col;
    vec2 uv;
};

PS_INPUT HLSLmain(VS_INPUT input0)
{
    PS_INPUT output0;
    ((output0).pos = ((ProjectionMatrix)*(vec4(((input0).pos).xy, 0.0, 1.0))));
    ((output0).col = (input0).col);
    ((output0).uv = (input0).uv);
    return output0;
}

void main()
{
    VS_INPUT input0;
    input0.pos = POSITION;
    input0.uv = TEXCOORD0;
    input0.col = COLOR0;
    PS_INPUT result = HLSLmain(input0);
    gl_Position = result.pos;
    vertOutput_COLOR0 = result.col;
    vertOutput_TEXCOORD0 = result.uv;
}
