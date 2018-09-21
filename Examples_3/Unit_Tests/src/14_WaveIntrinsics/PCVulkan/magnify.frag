#version 450

layout(set = 0, binding = 0, std140) uniform SceneConstantBuffer
{
	layout(row_major) mat4 orthProjMatrix;
	vec2 mousePosition;
	vec2 resolution;
	float time;
	uint renderMode;
	uint laneSize;
	uint padding;
};

layout(set = 0, binding = 1) uniform texture2D g_texture;
layout(set = 0, binding = 2) uniform sampler g_sampler;

layout(location = 0) in vec2 in_var_TEXCOORD;
layout(location = 0) out vec4 out_var_SV_TARGET;

void main()
{
	float aspectRatio = resolution.x / resolution.y;
	float magnifiedFactor = 6.0f;
	float magnifiedAreaSize = 0.05f;
	float magnifiedAreaBorder = 0.005f;

	// check the distance between this pixel and mouse location in UV space. 
	vec2 normalizedPixelPos = in_var_TEXCOORD;
	vec2 normalizedMousePos = mousePosition / resolution;         // convert mouse position to uv space.
	vec2 diff = abs(normalizedPixelPos - normalizedMousePos);     // distance from this pixel to mouse.

	vec4 color = texture(sampler2D(g_texture, g_sampler), normalizedPixelPos);

	// if the distance from this pixel to mouse is touching the border of the magnified area, color it as cyan.
	if (diff.x < (magnifiedAreaSize + magnifiedAreaBorder) && diff.y < (magnifiedAreaSize + magnifiedAreaBorder)*aspectRatio)
	{
		color = vec4(0.0f, 1.0f, 1.0f, 1.0f);
	}

	// if the distance from this pixel to mouse is inside the magnified area, enable the magnify effect.
	if (diff.x < magnifiedAreaSize && diff.y < magnifiedAreaSize *aspectRatio)
	{
		color = texture(sampler2D(g_texture, g_sampler), normalizedMousePos + (normalizedPixelPos - normalizedMousePos) / magnifiedFactor);
	}

	out_var_SV_TARGET = color;
}
