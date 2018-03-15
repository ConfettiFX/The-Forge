/*
 * Copyright (c) 2018 Confetti Interactive Inc.
 * 
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

#include "FloatUtil.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../Interfaces/IMemoryManager.h"

float lerp(const float u, const float v, const float x) {
	return u + x * (v - u);
}

float cerp(const float u0, const float u1, const float u2, const float u3, float x) {
	float p = (u3 - u2) - (u0 - u1);
	float q = (u0 - u1) - p;
	float r = u2 - u0;
	return x * (x * (x * p + q) + r) + u1;
}

float sign(const float v) {
	return (v > 0) ? 1.0f : (v < 0) ? -1.0f : 0.0f;
}

float clamp(const float v, const float c0, const float c1) {
	return min(max(v, c0), c1);
}

float saturate(const float x)
{
	return clamp(x, 0, 1);
}

float sCurve(const float t) {
	return t * t * (3 - 2 * t);
}
/************************************************************************/
// Mesh generation helpers
/************************************************************************/
void generateSpherePoints(float **ppPoints, int *pNumberOfPoints, int numberOfDivisions)
{
	float numStacks = (float)numberOfDivisions;
	float numSlices = (float)numberOfDivisions;
	float radius = 0.5f; // Diameter of 1

	tinystl::vector<vec3> vertices;
	tinystl::vector<vec3> normals;

	for (int i = 0; i < numberOfDivisions; i++)
	{
		for (int j = 0; j < numberOfDivisions; j++)
		{
			// Sectioned into quads, utilizing two triangles
			vec3 topLeftPoint = { (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)),
				(float)(-cos(PI * (j + 1.0f) / numSlices)),
				(float)(sin(2.0f * PI * i / numStacks) * sin(PI * (j + 1.0f) / numSlices)) };
			vec3 topRightPoint = { (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)),
				(float)(-cos(PI * (j + 1.0) / numSlices)),
				(float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * (j + 1.0) / numSlices)) };
			vec3 botLeftPoint = { (float)(-cos(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)),
				(float)(-cos(PI * j / numSlices)),
				(float)(sin(2.0f * PI * i / numStacks) * sin(PI * j / numSlices)) };
			vec3 botRightPoint = { (float)(-cos(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)),
				(float)(-cos(PI * j / numSlices)),
				(float)(sin(2.0f * PI * (i + 1.0) / numStacks) * sin(PI * j / numSlices)) };

			// Top right triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botRightPoint);
			vertices.push_back(radius * topRightPoint);
			normals.push_back(normalize(topLeftPoint));
			normals.push_back(normalize(botRightPoint));
			normals.push_back(normalize(topRightPoint));

			// Bot left triangle
			vertices.push_back(radius * topLeftPoint);
			vertices.push_back(radius * botLeftPoint);
			vertices.push_back(radius * botRightPoint);
			normals.push_back(normalize(topLeftPoint));
			normals.push_back(normalize(botLeftPoint));
			normals.push_back(normalize(botRightPoint));
		}
	}

	*pNumberOfPoints = (uint32_t)vertices.size() * 3 * 2;
	(*ppPoints) = (float*)conf_malloc(sizeof(float) * (*pNumberOfPoints));

	for (uint32_t i = 0; i < (uint32_t)vertices.size(); i++)
	{
		vec3 vertex = vertices[i];
		vec3 normal = normals[i];
		(*ppPoints)[i * 6 + 0] = vertex.getX();
		(*ppPoints)[i * 6 + 1] = vertex.getY();
		(*ppPoints)[i * 6 + 2] = vertex.getZ();
		(*ppPoints)[i * 6 + 3] = normal.getX();
		(*ppPoints)[i * 6 + 4] = normal.getY();
		(*ppPoints)[i * 6 + 5] = normal.getZ();
	}
}