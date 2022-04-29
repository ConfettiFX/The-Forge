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

#ifndef RESOURCES_H
#define RESOURCES_H

CBUFFER(uniformBlock_rootcbv, UPDATE_FREQ_NONE, b1, binding = 1)
{
	DATA(float4x4, mvp[VR_MULTIVIEW_COUNT], None);
};

RES(Tex2D(float4), uTex0, UPDATE_FREQ_NONE, t2, binding = 2);
RES(SamplerState, uSampler0, UPDATE_FREQ_NONE, s3, binding = 3);

PUSH_CONSTANT(uRootConstants, b0)
{
	DATA(float4, color, None);
	DATA(float2, scaleBias, None);
};

#endif