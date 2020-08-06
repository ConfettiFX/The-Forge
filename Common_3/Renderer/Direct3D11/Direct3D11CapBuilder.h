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

		UINT formatSupport = 0;

		pRenderer->pDxDevice->CheckFormatSupport(fmt, &formatSupport);
		pRenderer->pCapBits->canShaderReadFrom[i] = (formatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0;
		pRenderer->pCapBits->canShaderWriteTo[i] = (formatSupport & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0;
		pRenderer->pCapBits->canRenderTargetWriteTo[i] = (formatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET) != 0;
	}
}
