#pragma once

#include "../../ThirdParty/OpenSource/tinyimageformat/tinyimageformat_apis.h"

inline void utils_caps_builder(Renderer* pRenderer) {
	memset(pRenderer->capBits.canShaderReadFrom, 0, sizeof(pRenderer->capBits.canShaderReadFrom));
	memset(pRenderer->capBits.canShaderWriteTo, 0, sizeof(pRenderer->capBits.canShaderWriteTo));
	memset(pRenderer->capBits.canColorWriteTo, 0, sizeof(pRenderer->capBits.canColorWriteTo));

	for (uint32_t i = 0; i < TinyImageFormat_Count;++i) {
		VkFormatProperties formatSupport;
		VkFormat fmt = (VkFormat) TinyImageFormat_ToVkFormat((TinyImageFormat)i);
		if(fmt == VK_FORMAT_UNDEFINED) continue;

		vkGetPhysicalDeviceFormatProperties(pRenderer->pVkActiveGPU, fmt, &formatSupport);
		pRenderer->capBits.canShaderReadFrom[i] =
				(formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
		pRenderer->capBits.canShaderWriteTo[i] =
				(formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
		pRenderer->capBits.canColorWriteTo[i] =
				(formatSupport.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0;
	}

}
