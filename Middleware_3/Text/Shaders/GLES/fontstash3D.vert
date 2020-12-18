#version 100
precision mediump float;
precision mediump int;

struct uRootConstants_Block
{
    vec4 color;
    vec2 scaleBias;
};

uniform uRootConstants_Block uRootConstants;

uniform mat4 uniformBlock_rootcbv;

attribute vec2 Position;
attribute vec2 UV;

varying vec2 vertOutput_TEXCOORD0;
varying vec4 vertOutput_COLOR0;

void main()
{
    gl_Position = uniformBlock_rootcbv*vec4((Position * uRootConstants.scaleBias.xy), 1.0, 1.0);
    vertOutput_TEXCOORD0 = UV;
    vertOutput_COLOR0 = uRootConstants.color;
}
