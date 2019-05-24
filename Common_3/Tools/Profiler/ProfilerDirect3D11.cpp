#if defined(DIRECT3D11)

#include "ProfilerBase.h"

#if (PROFILE_ENABLED != 0)

#include "../../Renderer/IRenderer.h"

struct ProfileGpuTimerStateInternal
{
	ID3D11DeviceContext* pDeviceContext;
	ID3D11Query* pQueries[PROFILE_GPU_MAX_QUERIES];
	ID3D11Query* pRateQuery;
	ID3D11Query* pSyncQuery;

	uint64_t nFrame;
	std::atomic<uint32_t> nFramePut;
	AtomicUint nFramePutAtomic;

	uint32_t nSubmitted[PROFILE_GPU_FRAMES];
	uint64_t nResults[PROFILE_GPU_MAX_QUERIES];

	uint32_t nRateQueryIssue;
	uint64_t nQueryFrequency;
};

PROFILE_GPU_STATE_DECL(Internal)

void ProfileGpuInitInternal(ID3D11Device* pDevice)
{
	ProfileGpuInitStateInternal();

	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	pDevice->GetImmediateContext(&GPU.pDeviceContext);

	D3D11_QUERY_DESC Desc;
	Desc.MiscFlags = 0;
	Desc.Query = D3D11_QUERY_TIMESTAMP;
	for (uint32_t i = 0; i < PROFILE_GPU_MAX_QUERIES; ++i)
	{
		HRESULT hr = pDevice->CreateQuery(&Desc, &GPU.pQueries[i]);
		ASSERT(hr == S_OK);
	}

	HRESULT hr = pDevice->CreateQuery(&Desc, &GPU.pSyncQuery);
	ASSERT(hr == S_OK);

	Desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
	hr = pDevice->CreateQuery(&Desc, &GPU.pRateQuery);
	ASSERT(hr == S_OK);
}

void ProfileGpuShutdownInternal()
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	for (uint32_t i = 0; i < PROFILE_GPU_MAX_QUERIES; ++i)
	{
		GPU.pQueries[i]->Release();
		GPU.pQueries[i] = 0;
	}

	GPU.pRateQuery->Release();
	GPU.pRateQuery = 0;

	GPU.pSyncQuery->Release();
	GPU.pSyncQuery = 0;

	GPU.pDeviceContext->Release();
	GPU.pDeviceContext = 0;
}

uint32_t ProfileGpuFlipInternal()
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	if (!GPU.pDeviceContext) return (uint32_t)-1;

	uint32_t nFrameQueries = PROFILE_GPU_MAX_QUERIES / PROFILE_GPU_FRAMES;

	// Submit current frame
	uint32_t nFrameIndex = GPU.nFrame % PROFILE_GPU_FRAMES;
	uint32_t nFramePut = ProfileMin(GPU.nFramePutAtomic.mAtomicInt, nFrameQueries);

	GPU.nSubmitted[nFrameIndex] = nFramePut;
	GPU.nFramePutAtomic.AtomicStore(0);
	GPU.nFramePut.store(0);
	GPU.nFrame++;

	// Fetch frame results
	if (GPU.nFrame >= PROFILE_GPU_FRAMES)
	{
		uint64_t nPendingFrame = GPU.nFrame - PROFILE_GPU_FRAMES;
		uint32_t nPendingFrameIndex = nPendingFrame % PROFILE_GPU_FRAMES;

		for (uint32_t i = 0; i < GPU.nSubmitted[nPendingFrameIndex]; ++i)
		{
			uint32_t nQueryIndex = nPendingFrameIndex * nFrameQueries + i;
			ASSERT(nQueryIndex < PROFILE_GPU_MAX_QUERIES);

			uint64_t nResult = 0;

			HRESULT hr;
			do hr = GPU.pDeviceContext->GetData(GPU.pQueries[nQueryIndex], &nResult, sizeof(nResult), 0);
			while (hr == S_FALSE);

			GPU.nResults[nQueryIndex] = (hr == S_OK) ? nResult : PROFILE_INVALID_TICK;
		}
	}

	// Update timestamp frequency
	if (GPU.nRateQueryIssue == 0)
	{
		GPU.pDeviceContext->Begin(GPU.pRateQuery);
		GPU.nRateQueryIssue = 1;
	}
	else if (GPU.nRateQueryIssue == 1)
	{
		GPU.pDeviceContext->End(GPU.pRateQuery);
		GPU.nRateQueryIssue = 2;
	}
	else
	{
		D3D11_QUERY_DATA_TIMESTAMP_DISJOINT Result;
		if (S_OK == GPU.pDeviceContext->GetData(GPU.pRateQuery, &Result, sizeof(Result), D3D11_ASYNC_GETDATA_DONOTFLUSH))
		{
			GPU.nQueryFrequency = Result.Frequency;
			GPU.nRateQueryIssue = 0;
		}
	}

	return ProfileGpuInsertTimer(0);
}

uint32_t ProfileGpuInsertTimerInternal(void* pContext)
{
	ProfileGpuTimerStateInternal& GPU = g_ProfileGPU_Internal;

	uint32_t nFrameQueries = PROFILE_GPU_MAX_QUERIES / PROFILE_GPU_FRAMES;

	uint32_t nIndex = GPU.nFramePutAtomic.AtomicIncrement();
	if (nIndex >= nFrameQueries)
		return (uint32_t)-1;

	uint32_t nQueryIndex = (GPU.nFrame % PROFILE_GPU_FRAMES) * nFrameQueries + nIndex;

	GPU.pDeviceContext->End(GPU.pQueries[nQueryIndex]);

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

	GPU.pDeviceContext->End(GPU.pSyncQuery);

	uint64_t nResult = 0;

	HRESULT hr;
	do hr = GPU.pDeviceContext->GetData(GPU.pSyncQuery, &nResult, sizeof(nResult), 0);
	while (hr == S_FALSE);

	if (hr != S_OK) return false;

	*pOutCpu = P_TICK();
	*pOutGpu = nResult;

	return true;
}

extern Profile g_Profile;
#define S g_Profile
PROFILE_GPU_STATE_IMPL(Internal)
#undef S

#endif // PROFILE_ENABLED

#endif // D3D11
