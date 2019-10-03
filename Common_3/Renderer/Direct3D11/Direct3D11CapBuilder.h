#pragma once

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_base.h"
#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

inline void utils_caps_builder(Renderer* pRenderer) {
	memset(pRenderer->capBits.canShaderReadFrom, 0, sizeof(pRenderer->capBits.canShaderReadFrom));
	memset(pRenderer->capBits.canShaderWriteTo, 0, sizeof(pRenderer->capBits.canShaderWriteTo));
	memset(pRenderer->capBits.canRenderTargetWriteTo, 0, sizeof(pRenderer->capBits.canRenderTargetWriteTo));

	for (uint32_t i = 0; i < TinyImageFormat_Count;++i) {
		DXGI_FORMAT fmt = (DXGI_FORMAT) TinyImageFormat_ToDXGI_FORMAT((TinyImageFormat)i);
		if(fmt == DXGI_FORMAT_UNKNOWN) continue;

		UINT formatSupport = 0;

		pRenderer->pDxDevice->CheckFormatSupport(fmt, &formatSupport);
		pRenderer->capBits.canShaderReadFrom[i] = (formatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE) != 0;
		pRenderer->capBits.canShaderWriteTo[i] = (formatSupport & D3D11_FORMAT_SUPPORT_TYPED_UNORDERED_ACCESS_VIEW) != 0;
		pRenderer->capBits.canRenderTargetWriteTo[i] = (formatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET)  != 0;
	}

}
