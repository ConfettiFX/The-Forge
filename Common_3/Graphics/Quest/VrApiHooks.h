#pragma once

#if defined(QUEST_VR)

#include "../Interfaces/IGraphics.h"

bool hook_add_vk_instance_extensions(const char** instanceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer, uint bufferSize);
bool hook_add_vk_device_extensions(const char** deviceExtensionCache, uint* extensionCount, uint maxExtensionCount, char* pBuffer, uint bufferSize);

bool hook_post_init_renderer(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
void hook_pre_remove_renderer();

void hook_add_swap_chain(Renderer* pRenderer, const SwapChainDesc* pDesc, SwapChain** ppSwapChain);
void hook_remove_swap_chain(Renderer* pRenderer, SwapChain* pSwapChain);

void hook_acquire_next_image(SwapChain* pSwapChain, uint32_t* pImageIndex);
void hook_queue_present(const QueuePresentDesc* pQueuePresentDesc);

#endif
