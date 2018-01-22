#version 100

attribute vec4 Position;
varying vec4 VertexOut_color;
varying vec3 VertexOut_normal;

void main()
{
    gl_Position = Position;
    VertexOut_color = vec4(1.0);
    VertexOut_normal = vec3(0.5);
}

