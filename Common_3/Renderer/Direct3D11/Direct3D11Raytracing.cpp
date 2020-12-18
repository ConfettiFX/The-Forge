/*
 * Copyright (c) 2018-2021 The Forge Interactive Inc.
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

// Renderer
#include "../IRay.h"

bool isRaytracingSupported(Renderer* /*pRenderer*/) {
	return false;
}

bool initRaytracing(Renderer* /*pRenderer*/, Raytracing** /*ppRaytracing*/) {
	return false;
}

void removeRaytracing(Renderer* /*pRenderer*/, Raytracing* /*pRaytracing*/) {}

void addAccelerationStructure(Raytracing* /*pRaytracing*/, const AccelerationStructureDescTop* /*pDesc*/, AccelerationStructure** /*ppAccelerationStructure*/) {}
void removeAccelerationStructure(Raytracing* /*pRaytracing*/, AccelerationStructure* /*pAccelerationStructure*/) {}

void addRaytracingRootSignature(Raytracing* /*pRaytracing*/, const ShaderResource* /*pResources*/, uint32_t /*resourceCount*/, bool /*local*/, RootSignature** /*ppRootSignature*/, const RootSignatureDesc* /*pRootDesc */) {}

void addRaytracingShaderTable(Raytracing* /*pRaytracing*/, const RaytracingShaderTableDesc* /*pDesc*/, RaytracingShaderTable** /*ppTable*/) {}
void removeRaytracingShaderTable(Raytracing* /*pRaytracing*/, RaytracingShaderTable* /*pTable*/) {}

void cmdBuildAccelerationStructure(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, RaytracingBuildASDesc* /*pDesc*/) {}
void cmdDispatchRays(Cmd* /*pCmd*/, Raytracing* /*pRaytracing*/, const RaytracingDispatchDesc* /*pDesc*/) {}

