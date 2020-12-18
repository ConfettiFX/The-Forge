#version 100
precision mediump float;
precision mediump int;

attribute vec2 Position;
attribute vec2 UV;
attribute vec4 Color;

varying vec2 vertOutput_TEXCOORD0;
varying vec4 vertOutput_COLOR0;

uniform mat4 uniformBlockVS;

void main()
{
    gl_Position = uniformBlockVS*vec4(Position.xy, 0.0, 1.0);
    vertOutput_TEXCOORD0 = UV;
    vertOutput_COLOR0 = Color;
}
