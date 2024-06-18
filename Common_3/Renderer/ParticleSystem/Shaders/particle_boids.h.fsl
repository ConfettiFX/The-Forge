/*
 * Copyright (c) 2017-2024 The Forge Interactive Inc.
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


float3 Seek(float3 currPos, float3 currVel, float3 target, float maxSpeed)
{
	return normalize(target - currPos) * maxSpeed - currVel;
}

float3 Avoid(float3 currPos, float3 currVel, float3 otherPos, float maxForce, float avoidStrength)
{
	float3 dir = normalize(currVel);
	float3 otherToCurr = otherPos - currPos;
	
	float targetDist = length(otherToCurr);
	float proj = dot(dir, otherToCurr);

	float3 target = currPos + dir * proj;
	float3 forceDir = normalize(target - otherPos);

	targetDist /= avoidStrength;
	return forceDir * maxForce * (1.0/(targetDist*targetDist));
}

float3 Separation(float3 currPos, float3 otherPos, float maxForce)
{
	float3 otherToCurr = currPos - otherPos;
	float len = length(otherToCurr);
	
	otherToCurr = normalize(otherToCurr);
	return (maxForce * otherToCurr) / (len*len);
}

float3 Cohesion(float3 currPos, float3 avgPos, float maxForce)
{
	return maxForce * normalize(avgPos - currPos);
}

float3 Alignment(float3 currVel, float3 avgVel, float maxForce)
{
	return maxForce * normalize(avgVel - currVel);
}
