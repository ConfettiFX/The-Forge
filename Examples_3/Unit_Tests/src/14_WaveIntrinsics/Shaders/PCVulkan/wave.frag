#version 450
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_ballot : require
#extension GL_KHR_shader_subgroup_quad : require

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

layout(location = 0) in vec4 in_var_COLOR;
layout(location = 0) out vec4 out_var_SV_TARGET;

// use this to generate grid-like texture
float texPattern(vec2 position)
{
	float scale = 0.13;
	float t = sin(position.x * scale) + cos(position.y * scale);
	float c = smoothstep(0.0, 0.2, t*t);

	return c;
}

void main()
{
	vec4 outputColor;

	// Add grid-like texture pattern on top of the color
	float texP = texPattern(gl_FragCoord.xy);
	outputColor = texP * in_var_COLOR;

	switch (renderMode)
	{
	case 1:
	{
		// Just pass through the color we generate before
		break;
	}
	case 2:
	{
		// Example of query intrinsics: WaveGetLaneIndex
		// Gradiently color the wave block by their lane id. Black for the smallest lane id and White for the largest lane id.
		outputColor = vec4(float(gl_SubgroupInvocationID) / float(laneSize));
		break;
	}
	case 3:
	{
		// Example of query intrinsics: WaveIsFirstLane
		// Mark the first lane as white pixel
		if (subgroupElect())
			outputColor = vec4(1., 1., 1., 1.);
		break;
	}
	case 4:
	{
		// Example of query intrinsics: WaveIsFirstLane
		// Mark the first active lane as white pixel. Mark the last active lane as red pixel.
		if (subgroupElect())
			outputColor = vec4(1., 1., 1., 1.);
		if (gl_SubgroupInvocationID == subgroupMax(gl_SubgroupInvocationID))
			outputColor = vec4(1., 0., 0., 1.);
		break;
	}
	case 5:
	{
		// Example of vote intrinsics: WaveActiveBallot
		// Active lanes ratios (# of total activelanes / # of total lanes).
		uvec4 activeLaneMask = subgroupBallot(true);
		uint numActiveLanes = bitCount(activeLaneMask.x) + bitCount(activeLaneMask.y) + bitCount(activeLaneMask.z) + bitCount(activeLaneMask.w);
		float activeRatio = float(numActiveLanes) / float(laneSize);
		outputColor = vec4(activeRatio, activeRatio, activeRatio, 1.0);
		break;
	}
	case 6:
	{
		// Example of wave broadcast intrinsics: WaveReadLaneFirst
		// Broadcast the color in first lan to the wave.
		outputColor = subgroupBroadcastFirst(outputColor);
		break;
	}
	case 7:
	{
		// Example of wave reduction intrinsics: WaveActiveSum
		// Paint the wave with the averaged color inside the wave.
		uvec4 activeLaneMask = subgroupBallot(true);
		uint numActiveLanes = bitCount(activeLaneMask.x) + bitCount(activeLaneMask.y) + bitCount(activeLaneMask.z) + bitCount(activeLaneMask.w);
		vec4 avgColor = subgroupAdd(outputColor) / float(numActiveLanes);
		outputColor = avgColor;
		break;
	}
	case 8:
	{
		// Example of wave scan intrinsics: WavePrefixSum
		// First, compute the prefix sum of distance each lane to first lane.
		// Then, use the prefix sum value to color each pixel.
		vec4 basePos = subgroupBroadcastFirst(gl_FragCoord);
		vec4 prefixSumPos = subgroupExclusiveAdd(gl_FragCoord - basePos);

		// Get the number of total active lanes.
		uvec4 activeLaneMask = subgroupBallot(true);
		uint numActiveLanes = bitCount(activeLaneMask.x) + bitCount(activeLaneMask.y) + bitCount(activeLaneMask.z) + bitCount(activeLaneMask.w);

		outputColor = prefixSumPos / numActiveLanes;

		break;
	}
	case 9:
	{
		// Example of Quad-Wide shuffle intrinsics: QuadReadAcrossX and QuadReadAcrossY
		// Color pixels based on their quad id:
		//  q0 -> red
		//  q1 -> green
		//  q2 -> blue
		//  q3 -> white
		//
		//   -------------> x
		//  |   [0] [1]
		//  |   [2] [3]
		//  V
		//  Y
		//
		float dx = subgroupQuadSwapHorizontal(gl_FragCoord.x) - gl_FragCoord.x;
		float dy = subgroupQuadSwapVertical(gl_FragCoord.y) - gl_FragCoord.y;


		// q0
		if (dx > 0 && dy > 0)
			outputColor = vec4(1, 0, 0, 1);
		// q1
		else if (dx < 0 && dy > 0)
			outputColor = vec4(0, 1, 0, 1);
		// q2
		else if (dx > 0 && dy < 0)
			outputColor = vec4(0, 0, 1, 1);
		// q3
		else if (dx < 0 && dy < 0)
			outputColor = vec4(1, 1, 1, 1);
		else
			outputColor = vec4(0, 0, 0, 1);

		break;
	}

	default:
	{
		break;
	}
	}

	out_var_SV_TARGET = outputColor;
}
