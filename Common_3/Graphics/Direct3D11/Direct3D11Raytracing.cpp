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

#include "../GraphicsConfig.h"

// Renderer
#include "../Interfaces/IRay.h"

bool d3d11_isRaytracingSupported(Renderer* /*pRenderer*/) {
	return false;
}

bool d3d11_initRaytracing(Renderer* /*pRenderer*/, Raytracing** /*ppRaytracing*/) {
	return false;
}

void d3d11_removeRaytracing(Renderer* /*pRenderer*/, Raytracing* /*pRaytracing*/) {}

void d3d11_addAccelerationStructure(Raytracing* /*pRaytracing*/, const AccelerationStructureDescTop* /*pDesc*/, AccelerationStructure** /*ppAccelerationStructure*/) {}
void d3d11_removeAccelerationStructure(Raytracing* /*pRaytracing*/, AccelerationStructure* /*pAccelerationStructure*/) {}
void d3d11_removeAccelerationStructureScratch(Raytracing* /*pRaytracing*/, AccelerationStructure* /*pAccelerationStructure*/) {}

void d3d11_addRaytracingRootSignature(Raytracing* /*pRaytracing*/, const ShaderResource* /*pResources*/, uint32_t /*resourceCount*/, bool /*local*/, RootSignature** /*ppRootSignature*/, const RootSignatureDesc* /*pRootDesc */) {}

void d3d11_addRaytracingShaderTable(Raytracing* /*pRaytracing*/, const RaytracingShaderTableDesc* /*pDesc*/, RaytracingShaderTable** /*ppTable*/) {}
void d3d11_removeRaytracingShaderTable(Raytracing* /*pRaytracing*/, RaytracingShaderTable* /*pTable*/) {}

void d3d11_cmdBuildAccelerationStructure(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, RaytracingBuildASDesc* /*pDesc*/) {}
void d3d11_cmdDispatchRays(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, const RaytracingDispatchDesc* /*pDesc*/) {}

void initD3D11RaytracingFunctions()
{
	isRaytracingSupported = d3d11_isRaytracingSupported;
	initRaytracing = d3d11_initRaytracing;
	removeRaytracing = d3d11_removeRaytracing;
	addAccelerationStructure = d3d11_addAccelerationStructure;
	removeAccelerationStructure = d3d11_removeAccelerationStructure;
	removeAccelerationStructureScratch = d3d11_removeAccelerationStructureScratch;
	addRaytracingShaderTable = d3d11_addRaytracingShaderTable;
	removeRaytracingShaderTable = d3d11_removeRaytracingShaderTable;
	cmdBuildAccelerationStructure = d3d11_cmdBuildAccelerationStructure;
	cmdDispatchRays = d3d11_cmdDispatchRays;
}