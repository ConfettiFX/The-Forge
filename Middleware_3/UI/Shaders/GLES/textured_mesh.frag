#version 100
precision mediump float;
precision mediump int;

varying vec2 vertOutput_TEXCOORD0;
varying vec4 vertOutput_COLOR0;

uniform sampler2D uTex;

void main()
{
    gl_FragColor = texture2D(uTex, vec2(vertOutput_TEXCOORD0)) * vertOutput_COLOR0;
}
