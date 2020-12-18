#version 100
precision mediump float;
precision mediump int;

varying vec2 vertOutput_TEXCOORD0;
varying vec4 vertOutput_COLOR0;

uniform sampler2D uTex;

void main()
{
    //input0.pos = vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w);
    gl_FragColor = vertOutput_COLOR0 * texture2D(uTex, vec2(vertOutput_TEXCOORD0));
}
