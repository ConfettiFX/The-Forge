/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
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

#include "Shader_Defs.h"
#include "Packing.h"


struct PsIn
{
	float4 Position : SV_Position;
};

ConstantBuffer<RootConstant> indirectRootConstant : register(b2);


uint calculateOutputVBID(bool opaque, uint drawID, uint primitiveID)
{
    uint drawID_primID = ((drawID << 23) & 0x7F800000) | (primitiveID & 0x007FFFFF);
    return (opaque) ? drawID_primID : (1 << 31) | drawID_primID;
}

float4 main(PsIn In, uint primitiveID : SV_PrimitiveID) : SV_Target
{
	return unpackUnorm4x8(calculateOutputVBID(true, 
		indirectRootConstant.drawId, primitiveID));
}
