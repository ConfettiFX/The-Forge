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

#pragma once

#include "../GraphicsConfig.h"

#ifdef DIRECT3D12

#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../Resources/ResourceLoader/ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

inline void d3d12_utils_caps_builder(Renderer* pRenderer)
{
	pRenderer->pCapBits = (GPUCapBits*)tf_calloc(1, sizeof(GPUCapBits));

	for (uint32_t i = 0; i < TinyImageFormat_Count;++i)
	{
		DXGI_FORMAT fmt = (DXGI_FORMAT) TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)i);
		if(fmt == DXGI_FORMAT_UNKNOWN) continue;

		D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { fmt };

		pRenderer->mD3D12.pDxDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
		pRenderer->pCapBits->canShaderReadFrom[i] = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
		pRenderer->pCapBits->canShaderWriteTo[i] = (formatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE) != 0;
		pRenderer->pCapBits->canRenderTargetWriteTo[i] = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)  != 0;
	}
}
#endif