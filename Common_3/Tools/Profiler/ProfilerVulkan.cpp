#if defined(VULKAN)

#include "ProfilerBase.h"
#if (PROFILE_ENABLED != 0)

#include "../../Renderer/IRenderer.h"

struct ProfileGpuTimerStateInternal
{
	VkDevice pDevice;
	VkQueue pQueue;
	VkQueryPool pQueryPool;
	VkCommandPool pCommandPool;
	VkCommandBuffer pCommandBuffers[PROFILE_GPU_FRAMES];
	VkFence pFences[PROFILE_GPU_FRAMES];

	VkCommandBuffer pReferenceCommandBuffer;
	uint32_t nReferenceQuery;

	uint64_t nFrame;
	AtomicUint nFramePutAtomic;

	uint32_t nSubmitted[PROFILE_GPU_FRAMES];
	uint64_t nResults[PROFILE_GPU_MAX_QUERIES];
	uint64_t nQueryFrequency;
};

PROFILE_GPU_STATE_DECL(Internal)

void ProfileGpuInitInternal(VkDevice pDevice, VkPhysicalDevice pPhysicalDevice, VkQueue pQueue)
{
	ProfileGpuInitStateInternal();

	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	VkPhysicalDeviceProperties Properties;
	vkGetPhysicalDeviceProperties(pPhysicalDevice, &Properties);

	GPU.pDevice = pDevice;
	GPU.pQueue = pQueue;

	VkQueryPoolCreateInfo queryPoolInfo = {};
	queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
	queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
	queryPoolInfo.queryCount = PROFILE_GPU_MAX_QUERIES + 1; // reference query

	VkResult res = vkCreateQueryPool(pDevice, &queryPoolInfo, NULL, &GPU.pQueryPool);
	ASSERT(res == VK_SUCCESS);

	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	commandPoolInfo.queueFamilyIndex = 0;

	res = vkCreateCommandPool(pDevice, &commandPoolInfo, NULL, &GPU.pCommandPool);
	ASSERT(res == VK_SUCCESS);

	VkCommandBuffer pCommandBuffers[PROFILE_GPU_FRAMES + 1] = {};

	VkCommandBufferAllocateInfo commandBufferInfo = {};
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.commandPool = GPU.pCommandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = sizeof(pCommandBuffers) / sizeof(pCommandBuffers[0]);

	res = vkAllocateCommandBuffers(pDevice, &commandBufferInfo, pCommandBuffers);
	ASSERT(res == VK_SUCCESS);

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	for (uint32_t i = 0; i < PROFILE_GPU_FRAMES; ++i)
	{
		GPU.pCommandBuffers[i] = pCommandBuffers[i];

		res = vkCreateFence(pDevice, &fenceInfo, NULL, &GPU.pFences[i]);
		ASSERT(res == VK_SUCCESS);
	}

	GPU.pReferenceCommandBuffer = pCommandBuffers[PROFILE_GPU_FRAMES];
	GPU.nReferenceQuery = PROFILE_GPU_MAX_QUERIES; // reference query

	GPU.nQueryFrequency = (uint64_t)(1e9 / Properties.limits.timestampPeriod);
}

void ProfileGpuShutdownInternal()
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	if (GPU.nFrame > 0)
	{
		uint32_t nFrameIndex = (GPU.nFrame - 1) % PROFILE_GPU_FRAMES;

		VkResult res = vkWaitForFences(GPU.pDevice, 1, &GPU.pFences[nFrameIndex], VK_TRUE, UINT64_MAX);
		ASSERT(res == VK_SUCCESS);
	}

	for (uint32_t i = 0; i < PROFILE_GPU_FRAMES; ++i)
	{
		vkDestroyFence(GPU.pDevice, GPU.pFences[i], NULL);
		GPU.pFences[i] = 0;
	}

	vkDestroyCommandPool(GPU.pDevice, GPU.pCommandPool, NULL);
	memset(GPU.pCommandBuffers, 0, sizeof(GPU.pCommandBuffers));
	GPU.pCommandPool = 0;

	vkDestroyQueryPool(GPU.pDevice, GPU.pQueryPool, NULL);
	GPU.pQueryPool = 0;

	GPU.pQueue = 0;
	GPU.pDevice = 0;
}

uint32_t ProfileGpuFlipInternal()
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	uint32_t nFrameQueries = PROFILE_GPU_MAX_QUERIES / PROFILE_GPU_FRAMES;

	// Submit current frame
	uint32_t nFrameIndex = GPU.nFrame % PROFILE_GPU_FRAMES;
	uint32_t nFrameStart = nFrameIndex * nFrameQueries;

	VkCommandBuffer pCommandBuffer = GPU.pCommandBuffers[nFrameIndex];

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkResult res = vkBeginCommandBuffer(pCommandBuffer, &commandBufferBeginInfo);
	ASSERT(res == VK_SUCCESS);

	uint32_t nFrameTimeStamp = ProfileGpuInsertTimer(pCommandBuffer);
	uint32_t nFramePut = ProfileMin(GPU.nFramePutAtomic.mAtomicInt, nFrameQueries);

	res = vkEndCommandBuffer(pCommandBuffer);
	ASSERT(res == VK_SUCCESS);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &pCommandBuffer;

	res = vkQueueSubmit(GPU.pQueue, 1, &submitInfo, GPU.pFences[nFrameIndex]);
	ASSERT(res == VK_SUCCESS);

	GPU.nSubmitted[nFrameIndex] = nFramePut;
	GPU.nFramePutAtomic.AtomicStore(0);
	GPU.nFrame++;

	// Fetch frame results
	if (GPU.nFrame >= PROFILE_GPU_FRAMES)
	{
		uint64_t nPendingFrame = GPU.nFrame - PROFILE_GPU_FRAMES;
		uint32_t nPendingFrameIndex = nPendingFrame % PROFILE_GPU_FRAMES;

		res = vkWaitForFences(GPU.pDevice, 1, &GPU.pFences[nPendingFrameIndex], VK_TRUE, UINT64_MAX);
		ASSERT(res == VK_SUCCESS);

		res = vkResetFences(GPU.pDevice, 1, &GPU.pFences[nPendingFrameIndex]);
		ASSERT(res == VK_SUCCESS);

		uint32_t nPendingFrameStart = nPendingFrameIndex * nFrameQueries;
		uint32_t nPendingFrameCount = GPU.nSubmitted[nPendingFrameIndex];

		if (nPendingFrameCount)
		{
			res = vkGetQueryPoolResults(GPU.pDevice, GPU.pQueryPool,
				nPendingFrameStart, nPendingFrameCount,
				nPendingFrameCount * sizeof(uint64_t), &GPU.nResults[nPendingFrameStart],
				sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
			ASSERT(res == VK_SUCCESS);
		}
	}

	return nFrameTimeStamp;
}

uint32_t ProfileGpuInsertTimerInternal(void* pContext)
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	uint32_t nFrameQueries = PROFILE_GPU_MAX_QUERIES / PROFILE_GPU_FRAMES;

	uint32_t nIndex = GPU.nFramePutAtomic.AtomicIncrement();
	if (nIndex >= nFrameQueries) return (uint32_t)-1;

	uint32_t nQueryIndex = (GPU.nFrame % PROFILE_GPU_FRAMES) * nFrameQueries + nIndex;

	vkCmdWriteTimestamp((VkCommandBuffer)pContext, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, GPU.pQueryPool, nQueryIndex);

	return nQueryIndex;
}

uint64_t ProfileGpuGetTimeStampInternal(uint32_t nIndex)
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	return GPU.nResults[nIndex];
}

uint64_t ProfileTicksPerSecondGpuInternal()
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	return GPU.nQueryFrequency ? GPU.nQueryFrequency : 1000000000ll;
}

bool ProfileGetGpuTickReferenceInternal(int64_t* pOutCpu, int64_t* pOutGpu)
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	VkCommandBuffer pCommandBuffer = GPU.pReferenceCommandBuffer;
	uint32_t nQueryIndex = GPU.nReferenceQuery;

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VkResult res = vkBeginCommandBuffer(pCommandBuffer, &commandBufferBeginInfo);
	ASSERT(res == VK_SUCCESS);

	vkCmdWriteTimestamp(pCommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, GPU.pQueryPool, nQueryIndex);

	res = vkEndCommandBuffer(pCommandBuffer);
	ASSERT(res == VK_SUCCESS);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &pCommandBuffer;

	res = vkQueueSubmit(GPU.pQueue, 1, &submitInfo, VK_NULL_HANDLE);
	ASSERT(res == VK_SUCCESS);

	res = vkQueueWaitIdle(GPU.pQueue);
	ASSERT(res == VK_SUCCESS);

	*pOutCpu = P_TICK();

	res = vkGetQueryPoolResults(GPU.pDevice, GPU.pQueryPool, nQueryIndex, 1, sizeof(uint64_t), pOutGpu, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
	ASSERT(res == VK_SUCCESS);

	return true;
}

extern Profile g_Profile;
#define S g_Profile
PROFILE_GPU_STATE_IMPL(Internal)
#undef S

#endif // PROFILE_ENABLED

#endif // VULKAN