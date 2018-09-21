#version 450

layout(set = 0, binding = 0) uniform SceneConstantBuffer
{
	mat4 orthProjMatrix;
	vec2 mousePosition;
	vec2 resolution;
	float time;
	uint renderMode;
	uint laneSize;
	uint padding;
};

layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec4 iColor;

layout(location = 0) out vec4 oColor;

void main()
{
	gl_Position = (orthProjMatrix * vec4(iPosition, 1.0f));
	oColor = iColor;
}
