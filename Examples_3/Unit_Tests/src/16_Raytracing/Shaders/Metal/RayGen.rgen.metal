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

/*
 See LICENSE folder for this sample’s licensing information.
 
 Abstract:
 Metal shaders used for ray tracing
 */

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

#include "ShaderTypes.h"

struct MetalRayGenShader {
	
	uint _activeThreadIndex;
	uint _pathIndex;
	PathCallStack _callStack;
	
	device Ray& _ray;
	device Payload& _payload;
	
	short _nextSubshader;
	
	// For the first ray generation shader.
	MetalRayGenShader(uint2 tid,
					  constant RaytracingArguments & arguments,
					  device uint* pathIndices,
					  device uint4& rayCountIndirectBuffer,
					  constant short& shaderIndex,
					  short subshaderCount) :
	_activeThreadIndex(tid.y * arguments.uniforms.width + tid.x),
	_pathIndex(_activeThreadIndex),
	_callStack(arguments.pathCallStackHeaders, arguments.pathCallStackFunctions, _pathIndex, arguments.uniforms.maxCallStackDepth),
	_ray(arguments.rays[_activeThreadIndex]),
	_payload(arguments.payloads[_pathIndex]),
	_nextSubshader(1 < subshaderCount ? 1 : -1)
	{
		pathIndices[_pathIndex] = _pathIndex;
		_callStack.Initialize();
		
		if (tid.x == 0 && tid.y == 0)
		{
			uint rayCount = arguments.uniforms.width * arguments.uniforms.height;
			rayCountIndirectBuffer[0] = rayCount;
			rayCountIndirectBuffer[1] = (rayCount + THREADS_PER_THREADGROUP - 1) / THREADS_PER_THREADGROUP;
			rayCountIndirectBuffer[2] = 1;
			rayCountIndirectBuffer[3] = 1;
		}
		
		const short subshaderIndex = 0;
		this->sharedInit(arguments, subshaderIndex, subshaderCount);
	}
	
	// For subsequent ray generation shaders.
	MetalRayGenShader(uint tid,
					  constant RaytracingArguments & arguments,
					  const device uint* pathIndices,
					  const device uint2& pathBaseOffsetAndCount,
					  constant short& shaderIndex,
					  short subshaderIndex,
					  short subshaderCount) :
	_activeThreadIndex(pathBaseOffsetAndCount.x + tid),
	_pathIndex(pathIndices[_activeThreadIndex]),
	_callStack(arguments.pathCallStackHeaders, arguments.pathCallStackFunctions, _pathIndex, arguments.uniforms.maxCallStackDepth),
	_ray(arguments.rays[_activeThreadIndex]),
	_payload(arguments.payloads[_pathIndex]),
	_nextSubshader(subshaderIndex + 1 < subshaderCount ? shaderIndex + 1 : -1)
	{
		this->sharedInit(arguments, subshaderIndex, subshaderCount);
	}
	
	void sharedInit(constant RaytracingArguments & arguments, short subshaderIndex, short subshaderCount) {
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
	
	// ---------------------------------------------------------------------------------------
	// User shaders.
	
	void shader0(uint2 threadId,
				 constant Uniforms & uniforms,
				 device Payload &payload,
				 constant CSDataPerFrame& csDataPerFrame,
				 constant CSData& csData
#ifdef TARGET_IOS
				 ,texture2d<float, access::read_write> gOutput
#endif
) {
		uint2 launchIndex = threadId.xy;
		uint2 launchDim = uint2(uniforms.width, uniforms.height);
		
		constant RayGenConfigBlock &settings = csDataPerFrame.gSettings;
		
		float2 crd = float2(launchIndex) + settings.mSubpixelJitter;;
		float2 dims = float2(launchDim);
		
		float2 d = ((crd / dims) * 2.f - 1.f);
		d.y *= -1;
		
		float3 direction = normalize(float3(d * settings.mZ1PlaneSize, 1));
		float3 directionWS = float3x3(settings.mCameraToWorld[0].xyz, settings.mCameraToWorld[1].xyz, settings.mCameraToWorld[2].xyz) * direction;
		
		Ray ray;
		ray.origin = settings.mCameraToWorld[3].xyz + directionWS * settings.mProjNear;
		ray.direction = directionWS;
		ray.mask = 0xFF;
		ray.maxDistance = settings.mProjFarMinusNear;
		
		payload.throughput = float3(1.0);
		payload.radiance = float3(0.0);
		payload.lightSample = 0.0;
		payload.randomSeed = csDataPerFrame.gSettings.mRandomSeed;
		payload.recursionDepth = 0;
	#if DENOISER_ENABLED
		payload.surfaceAlbedo = 1.0;
	#endif
		
		TraceRay(ray, /* missShaderIndex = */ 0);
		
		// Debug: uncomment this on to show green when the second part of the rayGen shader isn't reached.
//		if (settings.mFrameIndex == 0) {
//	#ifndef TARGET_IOS
//			csData.gOutput.write(float4(0, 1, 0, 1.0f), threadId);
//	#else
//			gOutput.write(float4(0, 1, 0, 1.0f), threadId);
//	#endif
//		}
	}
	
	void shader1(uint pathIndex,
				 constant Uniforms & uniforms,
				 device Payload &payload,
				 constant CSDataPerFrame& csDataPerFrame,
				 constant CSData& csData
#ifdef TARGET_IOS
				 ,texture2d<float, access::read_write> gOutput
				 ,texture2d<float, access::read_write> gAlbedoTex
#endif
				 )
	{
		uint2 pixelPos = uint2(pathIndex % uniforms.width, pathIndex / uniforms.width);
		
		constant RayGenConfigBlock &settings = csDataPerFrame.gSettings;
		
#ifndef TARGET_IOS
		texture2d<float, access::read_write> outputTexture = csData.gOutput;
#else
		texture2d<float, access::read_write> outputTexture = gOutput;
#endif
		float4 accumulatedRadiance = 0.0;
		
#if DENOISER_ENABLED
		accumulatedRadiance = float4(payload.radiance, 1.0);
#else
		if (settings.mFrameIndex == 0) {
			accumulatedRadiance = float4(payload.radiance, 1.0);
		} else {
			accumulatedRadiance = outputTexture.read(pixelPos);
			accumulatedRadiance.w += 1.0;
			accumulatedRadiance.rgb += (payload.radiance - accumulatedRadiance.rgb) / accumulatedRadiance.w;
		}
#endif
		
		outputTexture.write(accumulatedRadiance, pixelPos);
		
#if DENOISER_ENABLED
#ifndef TARGET_IOS
		texture2d<float, access::read_write> albedoTexture = csData.gAlbedoTex;
#else
		texture2d<float, access::read_write> albedoTexture = gAlbedoTex;
#endif
		
		float3 accumulatedAlbedo = 0.0;
		if (settings.mFramesSinceCameraMove == 0) {
			accumulatedAlbedo = payload.surfaceAlbedo;
		} else {
			float invWeight = (float)(settings.mFramesSinceCameraMove + 1);
			accumulatedAlbedo = albedoTexture.read(pixelPos).rgb;
			accumulatedAlbedo += (payload.surfaceAlbedo - accumulatedAlbedo) / invWeight;
		}
		
		albedoTexture.write(float4(accumulatedAlbedo, 1.0), pixelPos);
#endif
	}
};

// Generates rays starting from the camera origin and traveling towards the image plane aligned
// with the camera's coordinate system.
// [numthreads(8, 8, 1)]
kernel void rayGen(uint2 tid                     [[thread_position_in_grid]],
				   constant RaytracingArguments & arguments  [[buffer(UPDATE_FREQ_USER + 0)]],
				   device uint* pathIndices [[buffer(UPDATE_FREQ_USER + 1)]],
				   device uint4& rayCountIndirectBuffer [[buffer(UPDATE_FREQ_USER + 2)]],
				   constant short& shaderIndex [[buffer(UPDATE_FREQ_USER + 3)]],
				   constant CSDataPerFrame& csDataPerFrame    [[buffer(UPDATE_FREQ_PER_FRAME)]],
				   constant CSData& csData                    [[buffer(UPDATE_FREQ_NONE)]]
#ifdef TARGET_IOS
				   ,texture2d<float, access::read_write> gOutput    [[texture(0)]]
#endif
)
{
    if (tid.x >= arguments.uniforms.width || tid.y >= arguments.uniforms.height) {
		return;
	}

	MetalRayGenShader rayGenShader(tid, arguments, pathIndices, rayCountIndirectBuffer, shaderIndex, /* subshaderCount = */ 2);
#ifdef TARGET_IOS
	rayGenShader.shader0(tid, arguments.uniforms, rayGenShader._payload, csDataPerFrame, csData, gOutput);
#else
	rayGenShader.shader0(tid, arguments.uniforms, rayGenShader._payload, csDataPerFrame, csData);
#endif
	
	rayGenShader.setupNextShader();
}

kernel void rayGen_0(uint tid                                [[thread_position_in_grid]],
					 constant RaytracingArguments & arguments  	[[buffer(UPDATE_FREQ_USER + 0)]],
					 const device uint* pathIndices			 	[[buffer(UPDATE_FREQ_USER + 1)]],
					 const device uint2& pathBaseOffsetAndCount [[buffer(UPDATE_FREQ_USER + 2)]],
					 constant short& shaderIndex             	[[buffer(UPDATE_FREQ_USER + 3)]],
					 constant CSDataPerFrame& csDataPerFrame    	[[buffer(UPDATE_FREQ_PER_FRAME)]],
					 constant CSData& csData                    	[[buffer(UPDATE_FREQ_NONE)]]
#ifdef TARGET_IOS
					 ,texture2d<float, access::read_write> gOutput    [[texture(0)]]
					 ,texture2d<float, access::read_write> gAlbedoTex 	  [[texture(1)]]
#endif
					 )
{
	if (tid >= pathBaseOffsetAndCount.y) {
		return;
	}
	
	MetalRayGenShader rayGenShader(tid, arguments, pathIndices, pathBaseOffsetAndCount, shaderIndex, /* subshaderIndex = */ 1, /* subshaderCount = */ 2);
	
#ifdef TARGET_IOS
	rayGenShader.shader1(rayGenShader._pathIndex, arguments.uniforms, rayGenShader._payload, csDataPerFrame, csData, gOutput, gAlbedoTex);
#else
	rayGenShader.shader1(rayGenShader._pathIndex, arguments.uniforms, rayGenShader._payload, csDataPerFrame, csData);
#endif
	
	rayGenShader.setupNextShader();
}
