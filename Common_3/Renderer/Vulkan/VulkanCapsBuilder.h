#pragma once

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

inline void utils_caps_builder(Renderer* pRenderer)
{
	pRenderer->pCapBits = (GPUCapBits*)tf_calloc(1, sizeof(GPUCapBits));

	for (uint32_t i = 0; i < TinyImageFormat_Count;++i) {
		VkFormatProperties formatSupport;
		VkFormat fmt = (VkFormat) TinyImageFormat_ToVkFormat((TinyImageFormat)i);
		if(fmt == VK_FORMAT_UNDEFINED) continue;

		vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, fmt, &formatSupport);
		pRenderer->pCapBits->canShaderReadFrom[i] =
				(formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
		pRenderer->pCapBits->canShaderWriteTo[i] =
				(formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
		pRenderer->pCapBits->canRenderTargetWriteTo[i] =
				(formatSupport.optimalTilingFeatures & 
					(VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT | VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0;
	}
}
