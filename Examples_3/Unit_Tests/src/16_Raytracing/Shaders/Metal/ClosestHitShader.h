/*
 * Copyright (c) 2017-2022 The Forge Interactive Inc.
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

#ifndef ClosestHitShader_h
#define ClosestHitShader_h

#include "ShaderTypes.h"

struct MetalClosestHitShader {
	
	uint _activeThreadIndex;
	uint _pathIndex;
	PathCallStack _callStack;
	
	device Ray& _ray;
	device Payload& _payload;
	
	short _nextSubshader;
	
	MetalClosestHitShader(uint tid,
					constant RaytracingArguments& arguments,
					const device uint* pathIndices,
					const device uint2& pathBaseOffsetAndCount,
					constant short& shaderIndex,
					short subshaderIndex) :
	_activeThreadIndex(pathBaseOffsetAndCount.x + tid),
	_pathIndex(pathIndices[_activeThreadIndex]),
	_callStack(arguments.pathCallStackHeaders, arguments.pathCallStackFunctions, _pathIndex, arguments.uniforms.maxCallStackDepth),
	_ray(arguments.rays[_activeThreadIndex]),
	_payload(arguments.payloads[_pathIndex]),
	_nextSubshader(subshaderIndex + 1 < MetalClosestHitShader::subshaderCount() ? shaderIndex + 1 : -1)
	{
		_ray.maxDistance = -1.f;
	}
	
	void setupNextShader()
	{
		if (_nextSubshader >= 0)
		{
			_callStack.PushFunction(_nextSubshader);
		}
	}
	
	void SkipSubshaders()
	{
		_nextSubshader = -1;
	}
	
	void CallShader(uint shaderIndex)
	{
		_callStack.PushFunction(shaderIndex);
	}
	
	void TraceRay(Ray ray, short missShaderIndex = -1, uchar rayContributionToHitGroupIndex = 0)
	{
		_ray = ray;
		_callStack.SetRayContributionToHitGroupIndex(rayContributionToHitGroupIndex);
//		_callStack.SetMultiplierForGeometryContributionToHitGroupIndex(multiplierForGeometryContributionToHitGroupIndex);
		if (missShaderIndex >= 0)
		{
			_callStack.SetMissShaderIndex(missShaderIndex);
		}
		else
		{
			_callStack.SetHitShaderOnly();
		}
		
		_payload.intersectionIndex = _activeThreadIndex;
	}
	
	static ushort subshaderCount();
	
	// ---------------------------------------------------------------------------------------
	// User shaders.
	
	void shader0(uint pathIndex,
				 constant Uniforms & uniforms,
				 const device Intersection& intersection,
				 device Payload &payload,
				 constant CSDataPerFrame& csDataPerFrame,
				 constant CSData& csData
				 );
	
	void shader1(uint pathIndex,
				 constant Uniforms & uniforms,
				 const device Intersection& intersection,
				 device Payload &payload,
				 constant CSDataPerFrame& csDataPerFrame,
				 constant CSData& csData
				 );
	
	void shader2(uint pathIndex,
				 constant Uniforms & uniforms,
				 const device Intersection& intersection,
				 device Payload &payload,
				 constant CSDataPerFrame& csDataPerFrame,
				 constant CSData& csData
				 );
	
	void shader3(uint pathIndex,
				 constant Uniforms & uniforms,
				 const device Intersection& intersection,
				 device Payload &payload,
				 constant CSDataPerFrame& csDataPerFrame,
				 constant CSData& csData
				 );
	
	void shader4(uint pathIndex,
				 constant Uniforms & uniforms,
				 const device Intersection& intersection,
				 device Payload &payload,
				 constant CSDataPerFrame& csDataPerFrame,
				 constant CSData& csData
	);
};

#define DEFINE_METAL_CLOSEST_HIT_SHADER(name, index) \
kernel void name(uint tid                                [[thread_position_in_grid]], \
				   constant RaytracingArguments & arguments	[[buffer(UPDATE_FREQ_USER + 0)]], \
				   const device uint* pathIndices			 	[[buffer(UPDATE_FREQ_USER + 1)]], \
				   const device uint2& pathBaseOffsetAndCount	[[buffer(UPDATE_FREQ_USER + 2)]], \
				   constant short& shaderIndex				[[buffer(UPDATE_FREQ_USER + 3)]], \
				   constant CSDataPerFrame& csDataPerFrame	[[buffer(UPDATE_FREQ_PER_FRAME)]], \
				   constant CSData& csData					[[buffer(UPDATE_FREQ_NONE)]] \
				   ) \
{ \
	if (tid >= pathBaseOffsetAndCount.y) { \
		return; \
	} \
	MetalClosestHitShader closestHitShader(tid, arguments, pathIndices, pathBaseOffsetAndCount, shaderIndex, /* subshaderIndex = */ index); \
	closestHitShader.shader##index(closestHitShader._pathIndex, arguments.uniforms, arguments.intersections[closestHitShader._payload.intersectionIndex], closestHitShader._payload, csDataPerFrame, csData); \
	closestHitShader.setupNextShader(); \
}

#endif /* ClosestHitShader_h */
