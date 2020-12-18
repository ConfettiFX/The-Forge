#version 100

precision mediump float;
precision mediump int;

struct uRootConstants_Block
{
    vec4 color;
    vec2 scaleBias;
};

uniform uRootConstants_Block uRootConstants;

attribute vec2 Position;
attribute vec2 UV;

varying vec2 vertOutput_TEXCOORD0;
varying vec4 vertOutput_COLOR0;

void main()
{
    vec4 positionResult = vec4(Position, 0.0, 1.0);
    positionResult.xy = positionResult.xy * uRootConstants.scaleBias.xy + vec2(-1.0, 1.0);
    gl_Position = positionResult;
    vertOutput_TEXCOORD0 = UV;
    vertOutput_COLOR0 = uRootConstants.color;
}
