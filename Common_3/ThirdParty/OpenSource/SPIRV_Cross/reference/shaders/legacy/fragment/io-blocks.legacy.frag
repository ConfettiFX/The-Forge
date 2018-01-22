#version 100
precision mediump float;
precision highp int;

varying vec4 VertexOut_color;
varying highp vec3 VertexOut_normal;

void main()
{
    gl_FragData[0] = VertexOut_color + VertexOut_normal.xyzz;
}

