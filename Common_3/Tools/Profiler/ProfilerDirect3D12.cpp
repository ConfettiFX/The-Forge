#if defined(DIRECT3D12)

#include "ProfilerBase.h"

#if (PROFILE_ENABLED != 0)

#include "../../Renderer/IRenderer.h"

struct ProfileGpuTimerStateInternal
{
	ID3D12CommandQueue* pCommandQueue;
	ID3D12QueryHeap* pHeap;
	ID3D12Resource* pBuffer;
	ID3D12GraphicsCommandList* pCommandLists[PROFILE_GPU_FRAMES];
	ID3D12CommandAllocator* pCommandAllocators[PROFILE_GPU_FRAMES];
	ID3D12Fence* pFence;
	void* pFenceEvent;

	uint64_t nFrame;
	AtomicUint nFramePutAtomic;

	uint32_t nSubmitted[PROFILE_GPU_FRAMES];
	uint64_t nResults[PROFILE_GPU_MAX_QUERIES];
	uint64_t nQueryFrequency;
};

PROFILE_GPU_STATE_DECL(Internal)

void ProfileGpuInitInternal(ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue)
{
	ProfileGpuInitStateInternal();

	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	GPU.pCommandQueue = pCommandQueue;

	HRESULT hr;

	D3D12_QUERY_HEAP_DESC HeapDesc;
	HeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	HeapDesc.Count = PROFILE_GPU_MAX_QUERIES;
	HeapDesc.NodeMask = 0;

	D3D12_HEAP_PROPERTIES HeapProperties;
	HeapProperties.Type = D3D12_HEAP_TYPE_READBACK;
	HeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProperties.CreationNodeMask = 1;
	HeapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC ResourceDesc;
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Alignment = 0;
	ResourceDesc.Width = PROFILE_GPU_MAX_QUERIES * sizeof(uint64_t);
	ResourceDesc.Height = 1;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	hr = pDevice->CreateQueryHeap(&HeapDesc, IID_PPV_ARGS(&GPU.pHeap));
	ASSERT(hr == S_OK);
	hr = pDevice->CreateCommittedResource(&HeapProperties, D3D12_HEAP_FLAG_NONE, &ResourceDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&GPU.pBuffer));
	ASSERT(hr == S_OK);
	hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&GPU.pFence));
	ASSERT(hr == S_OK);
	GPU.pFenceEvent = CreateEvent(NULL, false, false, NULL);
	ASSERT(GPU.pFenceEvent != INVALID_HANDLE_VALUE);

	for (uint32_t i = 0; i < PROFILE_GPU_FRAMES; ++i)
	{
		hr = pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&GPU.pCommandAllocators[i]));
		ASSERT(hr == S_OK);
		hr = pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, GPU.pCommandAllocators[i], NULL, IID_PPV_ARGS(&GPU.pCommandLists[i]));
		ASSERT(hr == S_OK);
		hr = GPU.pCommandLists[i]->Close();
		ASSERT(hr == S_OK);
	}

	hr = pCommandQueue->GetTimestampFrequency(&GPU.nQueryFrequency);
	ASSERT(hr == S_OK);
}

void ProfileGpuShutdownInternal()
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	if (!GPU.pCommandQueue)
		return;

	if (GPU.nFrame > 0)
	{
		GPU.pFence->SetEventOnCompletion(GPU.nFrame, GPU.pFenceEvent);
		WaitForSingleObject(GPU.pFenceEvent, INFINITE);
	}

	for (uint32_t i = 0; i < PROFILE_GPU_FRAMES; ++i)
	{
		GPU.pCommandLists[i]->Release();
		GPU.pCommandLists[i] = 0;

		GPU.pCommandAllocators[i]->Release();
		GPU.pCommandAllocators[i] = 0;
	}

	GPU.pHeap->Release();
	GPU.pHeap = 0;

	GPU.pBuffer->Release();
	GPU.pBuffer = 0;

	GPU.pFence->Release();
	GPU.pFence = 0;

	CloseHandle(GPU.pFenceEvent);
	GPU.pFenceEvent = 0;

	GPU.pCommandQueue = 0;
}

uint32_t ProfileGpuFlipInternal()
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	uint32_t nFrameQueries = PROFILE_GPU_MAX_QUERIES / PROFILE_GPU_FRAMES;

	// Submit current frame
	uint32_t nFrameIndex = GPU.nFrame % PROFILE_GPU_FRAMES;
	uint32_t nFrameStart = nFrameIndex * nFrameQueries;

	ID3D12CommandAllocator* pCommandAllocator = GPU.pCommandAllocators[nFrameIndex];
	ID3D12GraphicsCommandList* pCommandList = GPU.pCommandLists[nFrameIndex];

	pCommandAllocator->Reset();
	pCommandList->Reset(pCommandAllocator, NULL);

	uint32_t nFrameTimeStamp = ProfileGpuInsertTimer(pCommandList);

	uint32_t nFramePut = ProfileMin(GPU.nFramePutAtomic.mAtomicInt, nFrameQueries);

	if (nFramePut)
		pCommandList->ResolveQueryData(GPU.pHeap, D3D12_QUERY_TYPE_TIMESTAMP, nFrameStart, nFramePut, GPU.pBuffer, nFrameStart * sizeof(int64_t));

	pCommandList->Close();

	ID3D12CommandList* pList = pCommandList;
	GPU.pCommandQueue->ExecuteCommandLists(1, &pList);
	GPU.pCommandQueue->Signal(GPU.pFence, GPU.nFrame + 1);

	GPU.nSubmitted[nFrameIndex] = nFramePut;
	GPU.nFramePutAtomic.AtomicStore(0);
	GPU.nFrame++;

	// Fetch frame results
	if (GPU.nFrame >= PROFILE_GPU_FRAMES)
	{
		uint64_t nPendingFrame = GPU.nFrame - PROFILE_GPU_FRAMES;
		uint32_t nPendingFrameIndex = nPendingFrame % PROFILE_GPU_FRAMES;

		GPU.pFence->SetEventOnCompletion(nPendingFrame + 1, GPU.pFenceEvent);
		WaitForSingleObject(GPU.pFenceEvent, INFINITE);

		uint32_t nPendingFrameStart = nPendingFrameIndex * nFrameQueries;
		uint32_t nPendingFrameCount = GPU.nSubmitted[nPendingFrameIndex];

		if (nPendingFrameCount)
		{
			void* pData = 0;
			D3D12_RANGE Range = { nPendingFrameStart * sizeof(uint64_t), (nPendingFrameStart + nPendingFrameCount) * sizeof(uint64_t) };

			HRESULT hr = GPU.pBuffer->Map(0, &Range, &pData);
			ASSERT(hr == S_OK);

			memcpy(&GPU.nResults[nPendingFrameStart], (uint64_t*)pData + nPendingFrameStart, nPendingFrameCount * sizeof(uint64_t));

			GPU.pBuffer->Unmap(0, 0);
		}
	}

	return nFrameTimeStamp;
}

uint32_t ProfileGpuInsertTimerInternal(void* pContext)
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	if (!pContext) return (uint32_t)-1;

	uint32_t nFrameQueries = PROFILE_GPU_MAX_QUERIES / PROFILE_GPU_FRAMES;

	uint32_t nIndex = GPU.nFramePutAtomic.AtomicIncrement();
	if (nIndex >= nFrameQueries) return (uint32_t)-1;

	uint32_t nQueryIndex = (GPU.nFrame % PROFILE_GPU_FRAMES) * nFrameQueries + nIndex;

	((ID3D12GraphicsCommandList*)pContext)->EndQuery(GPU.pHeap, D3D12_QUERY_TYPE_TIMESTAMP, nQueryIndex);

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

	return SUCCEEDED(GPU.pCommandQueue->GetClockCalibration((uint64_t*)pOutGpu, (uint64_t*)pOutCpu));
}

extern Profile g_Profile;
#define S g_Profile
PROFILE_GPU_STATE_IMPL(Internal)
#undef S

#endif // PROFILE_ENABLED

#endif // D3D12

