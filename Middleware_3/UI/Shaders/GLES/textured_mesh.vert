#version 100
precision mediump float;
precision mediump int;

struct uRootConstants_Block
{
    vec4 color;
    vec2 scaleBias;
};

attribute vec2 Position;
attribute vec2 UV;

varying vec2 vertOutput_TEXCOORD0;
varying vec4 vertOutput_COLOR0;

uniform uRootConstants_Block uRootConstants;

void main()
{
    gl_Position = vec4((Position.xy * uRootConstants.scaleBias.xy + vec2(-1.0, 1.0)), 0.0, 1.0);
    vertOutput_TEXCOORD0 = UV;
    vertOutput_COLOR0 = uRootConstants.color;
}