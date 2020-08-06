#pragma once

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

inline void utils_caps_builder(Renderer* pRenderer)
{
	pRenderer->pCapBits = (GPUCapBits*)tf_calloc(1, sizeof(GPUCapBits));

	for (uint32_t i = 0; i < TinyImageFormat_Count;++i)
	{
		DXGI_FORMAT fmt = (DXGI_FORMAT) TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)i);
		if(fmt == DXGI_FORMAT_UNKNOWN) continue;

		D3D12_FEATURE_DATA_FORMAT_SUPPORT formatSupport = { fmt };

		pRenderer->pDxDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &formatSupport, sizeof(formatSupport));
		pRenderer->pCapBits->canShaderReadFrom[i] = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE) != 0;
		pRenderer->pCapBits->canShaderWriteTo[i] = (formatSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE) != 0;
		pRenderer->pCapBits->canRenderTargetWriteTo[i] = (formatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET)  != 0;
	}
}
