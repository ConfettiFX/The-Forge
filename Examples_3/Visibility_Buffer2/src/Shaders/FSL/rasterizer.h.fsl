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

#include "../../../../../Common_3/Renderer/visibilitybuffer2/Shaders/FSL/vb_shading_utilities.h.fsl"

RES(SamplerState, textureSampler, UPDATE_FREQ_NONE, s0, binding = 20);
#if defined(METAL) || defined(ORBIS) || defined(PROSPERO)
	RES(Tex2D(float4), diffuseMaps[INSTANCE_BUFFER_SIZE],  UPDATE_FREQ_NONE, t6, binding = 21);
#else
	RES(Tex2D(float4), diffuseMaps[INSTANCE_BUFFER_SIZE],  space4, t6, binding = 21);
#endif
