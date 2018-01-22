#version 100
precision mediump float;
precision highp int;

struct Inputs
{
    highp vec4 a;
    highp vec2 b;
};

varying highp vec4 Inputs_a;
varying highp vec2 Inputs_b;

void main()
{
    Inputs v0 = Inputs(Inputs_a, Inputs_b);
    Inputs v1 = Inputs(Inputs_a, Inputs_b);
    highp vec4 a = Inputs_a;
    highp vec4 b = Inputs_b.xxyy;
    gl_FragData[0] = ((((v0.a + v0.b.xxyy) + v1.a) + v1.b.yyxx) + a) + b;
}

