#version 100
precision mediump float;
precision mediump int;

varying vec2 vertOutput_TEXCOORD0;
varying vec4 vertOutput_COLOR0;

uniform sampler2D uTex0;

void main()
{
    gl_FragColor = vec4(1.0, 1.0, 1.0, texture2D(uTex0, vec2(vertOutput_TEXCOORD0)).r) * vertOutput_COLOR0;
}
