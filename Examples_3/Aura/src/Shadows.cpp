/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
 *
 * This is a part of Aura.
 * 
 * This file(code) is licensed under a 
 * Creative Commons Attribution-NonCommercial 4.0 International License 
 *
 *   (https://creativecommons.org/licenses/by-nc/4.0/legalcode) 
 *
 * Based on a work at https://github.com/ConfettiFX/The-Forge.
 * You may not use the material for commercial purposes.
 *
*/

#include "Shadows.hpp"
#include "Camera.hpp"
#include "BoundingVolumes.hpp"

const static int frustumVertexCount = 8;
const vec3 normalizedCube[frustumVertexCount] = { vec3(1, 1, 1), vec3(1, -1, 1), vec3(-1, -1, 1), vec3(-1, 1, 1), vec3(1, 1, 0), vec3(1, -1, 0), vec3(-1, -1, 0), vec3(-1, 1, 0) };

float splitPositionLog(int cascadeIndex, int cascadeCount, float zNear, float zFar)
{
    return zNear * pow(zFar/zNear, (float)cascadeIndex/(float)cascadeCount);
}

float splitPositionLinear(int cascadeIndex, int cascadeCount, float zNear, float zFar)
{
    return zNear + (zFar - zNear) * (float)cascadeIndex / (float)cascadeCount;
}

void calculateSubFrustra(const PerspectiveProjection& frustum, int frustumCount, PerspectiveProjection* subFrustra)
{
    float currentNear = frustum.mNear;
    const float lambda = 0.5f;
    for (int i = 0; i < frustumCount; i++)
    {
        float logFarPlane = splitPositionLog(i + 1, frustumCount, frustum.mNear, frustum.mFar);
        float linearFarPlane = splitPositionLinear(i + 1, frustumCount, frustum.mNear, frustum.mFar);

        PerspectiveProjection projection = frustum;
        projection.mNear = currentNear;
        projection.mFar = lerp(linearFarPlane, logFarPlane, lambda);
        currentNear = projection.mFar;

        subFrustra[i] = projection;
    }
} 

void calculateShadowCascades(const PerspectiveProjection& viewFrustum, const mat4& viewMatrix, const mat4& lightView, int cascadeCount, mat4* cascadeProjections, mat4* cascadeTransforms, float* viewSize, uint32_t shadowMapResolution) 
	{
		PerspectiveProjection* subFrustra = (PerspectiveProjection*)tf_malloc(cascadeCount * sizeof(PerspectiveProjection));
		calculateSubFrustra(viewFrustum, cascadeCount, subFrustra);

		for (int i = 0; i < cascadeCount; i++)
		{
			PerspectiveProjection subFrustum = subFrustra[i];
			mat4 subFrustraProjection = mat4::perspective(subFrustum.mFovY, subFrustum.mAspectRatio, subFrustum.mNear, subFrustum.mFar);

			//mat4 worldToCascadeProjection = subFrustraProjection * viewMatrix;
			//mat4 cascadeProjectionToWorld = inverse(worldToCascadeProjection);

			mat4 inverseSubFrustraProjection = inverse(subFrustraProjection);

			vec3 viewspaceVertices[frustumVertexCount] = {};
			for (int j = 0; j < frustumVertexCount; j++)
			{
				vec4 vertex = (inverseSubFrustraProjection * vec4(normalizedCube[j], 1));
				viewspaceVertices[j] = vertex.getXYZ();
				viewspaceVertices[j] /= vertex.getW();
			}	

			vec3 minPos = vec3(FLT_MAX, FLT_MAX, FLT_MAX);
			vec3 maxPos = vec3(FLT_MIN, FLT_MIN, FLT_MIN);

			for (int j = 0; j < frustumVertexCount; j++)
			{
				vec3 current = viewspaceVertices[j];
				if (current.getX() < minPos.getX()) minPos.setX(current.getX());
				if (current.getY() < minPos.getY()) minPos.setY(current.getY());
				if (current.getZ() < minPos.getZ()) minPos.setZ(current.getZ());

				if (current.getX() > maxPos.getX()) maxPos.setX(current.getX());
				if (current.getY() > maxPos.getY()) maxPos.setY(current.getY());
				if (current.getZ() > maxPos.getZ()) maxPos.setZ(current.getZ());
			}
			AxisAlignedBoundingBox bb;
			bb.minPoint = minPos;
			bb.maxPoint = maxPos;

			BoundingSphere bs(bb);
			
			const float shadowRange = 2000.0f;

			float worldUnitsPerTexel = bs.radius / ((float)shadowMapResolution);
			bs.centre.setX(floor(bs.centre.getX() / worldUnitsPerTexel) * worldUnitsPerTexel);
			bs.centre.setY(floor(bs.centre.getY() / worldUnitsPerTexel) * worldUnitsPerTexel);

			vec3 centreWorld = ((inverse(viewMatrix) * vec4(bs.centre, 1))).getXYZ();
			cascadeProjections[i] = mat4::orthographic(bs.radius, -bs.radius, bs.radius, -bs.radius, -shadowRange, shadowRange);

			vec3 lightDirection = normalize(lightView.getRow(2).getXYZ());

			vec3 offset = lightDirection * shadowRange * 0.5f;
			cascadeTransforms[i] = mat4::lookAt(Point3(centreWorld + offset), Point3(centreWorld - offset), vec3(0, 1, 0));
			viewSize[i] = bs.radius;
		}

		tf_free(subFrustra);
	}