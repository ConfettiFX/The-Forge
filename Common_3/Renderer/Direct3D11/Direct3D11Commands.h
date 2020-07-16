#pragma once

#include "../IRenderer.h"

/* Utility functions to cache commands so that we can execute them when client
 * calls on queueSubmit.
 * TODO: create a script to autogenerate this file
 */

enum CmdType
{
	CMD_TYPE_cmdBindRenderTargets,
	CMD_TYPE_cmdSetViewport,
	CMD_TYPE_cmdSetScissor,
	CMD_TYPE_cmdBindPipeline,
	CMD_TYPE_cmdBindDescriptorSet,
	CMD_TYPE_cmdBindPushConstants,
	CMD_TYPE_cmdBindIndexBuffer,
	CMD_TYPE_cmdBindVertexBuffer,
	CMD_TYPE_cmdDraw,
	CMD_TYPE_cmdDrawInstanced,
	CMD_TYPE_cmdDrawIndexed,
	CMD_TYPE_cmdDrawIndexedInstanced,
	CMD_TYPE_cmdDispatch,
	CMD_TYPE_cmdBeginQuery,
	CMD_TYPE_cmdEndQuery,
	CMD_TYPE_cmdResolveQuery,
	CMD_TYPE_cmdBeginDebugMarker,
	CMD_TYPE_cmdEndDebugMarker,
	CMD_TYPE_cmdAddDebugMarker,
	CMD_TYPE_cmdUpdateBuffer,
	CMD_TYPE_cmdUpdateSubresource
};

struct BindRenderTargetsCmd
{
	ID3D11RenderTargetView* ppRenderTargets[MAX_RENDER_TARGET_ATTACHMENTS];
	ID3D11DepthStencilView* pDepthStencil;
	uint32_t                renderTargetCount;
	LoadActionsDesc         mLoadActions;
};

struct SetViewportCmd
{
	float x;
	float y;
	float width;
	float height;
	float minDepth;
	float maxDepth;
};

struct SetScissorCmd
{
	uint32_t x;
	uint32_t y;
	uint32_t width;
	uint32_t height;
};

struct BindPipelineCmd
{
	Pipeline* pPipeline;
};

struct BindDescriptorsCmd
{
	DescriptorSet* pDescriptorSet;
	void*          pDynamicCBVs;
	uint32_t        mDynamicCBVCount;
	uint32_t        mIndex;
};

struct BindPushConstantsCmd
{
	uint32_t mPushConstant[128];
	uint32_t mBinding;
	uint32_t mSize;
	uint32_t mStage;
};

struct BindIndexBufferCmd
{
	Buffer*  pBuffer;
	uint32_t offset;
	IndexType mIndexType;
};

struct BindVertexBufferCmd
{
	uint32_t       bufferCount;
	ID3D11Buffer** ppBuffers;
	uint32_t*      pStrides;
	uint32_t*      pOffsets;
};

struct DrawCmd
{
	uint32_t vertexCount;
	uint32_t firstVertex;
};

struct DrawInstancedCmd
{
	uint32_t vertexCount;
	uint32_t firstVertex;
	uint32_t instanceCount;
	uint32_t firstInstance;
};

struct DrawIndexedCmd
{
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t firstVertex;
};

struct DrawIndexedInstancedCmd
{
	uint32_t indexCount;
	uint32_t firstIndex;
	uint32_t instanceCount;
	uint32_t firstVertex;
	uint32_t firstInstance;
};

struct DispatchCmd
{
	uint32_t groupCountX;
	uint32_t groupCountY;
	uint32_t groupCountZ;
};

struct ResourceBarrierCmd
{
	uint32_t        numBufferBarriers;
	BufferBarrier*  pBufferBarriers;
	uint32_t        numTextureBarriers;
	TextureBarrier* pTextureBarriers;
	bool            batch;
};

struct SynchronizeResourcesCmd
{
	uint32_t  numBuffers;
	Buffer**  ppBuffers;
	uint32_t  numTextures;
	Texture** ppTextures;
	bool      batch;
};

struct FlushBarriersCmd
{
};

struct ExecuteIndirectCmd
{
	CommandSignature* pCommandSignature;
	uint              maxCommandCount;
	Buffer*           pIndirectBuffer;
	uint64_t          bufferOffset;
	Buffer*           pCounterBuffer;
	uint64_t          counterBufferOffset;
};

struct BeginQueryCmd
{
	QueryPool* pQueryPool;
	QueryDesc  mQuery;
};

struct EndQueryCmd
{
	QueryPool* pQueryPool;
	QueryDesc  mQuery;
};

struct ResolveQueryCmd
{
	QueryPool* pQueryPool;
	Buffer*    pReadbackBuffer;
	uint32_t   startQuery;
	uint32_t   queryCount;
};

struct BeginDebugMarkerCmd
{
	float       r;
	float       g;
	float       b;
	const char* pName;
};

struct EndDebugMarkerCmd
{
};

struct AddDebugMarkerCmd
{
	float       r;
	float       g;
	float       b;
	const char* pName;
};

struct UpdateBufferCmd
{
	uint64_t srcOffset;
	uint64_t dstOffset;
	uint64_t size;
	Buffer*  pSrcBuffer;
	Buffer*  pBuffer;
};

struct SubresourceDataDesc
{
	uint64_t mSrcOffset;
	uint32_t mMipLevel;
	uint32_t mArrayLayer;
	uint32_t mRowPitch;
	uint32_t mSlicePitch;
};

struct UpdateSubresourcesCmd
{
	Texture*            pTexture;
	Buffer*             pSrcBuffer;
	SubresourceDataDesc mSubresourceDesc;
};

struct CachedCmd
{
	Cmd*    pCmd;
	CmdType sType;
	union
	{
		BindRenderTargetsCmd    mBindRenderTargetsCmd;
		SetViewportCmd          mSetViewportCmd;
		SetScissorCmd           mSetScissorCmd;
		BindPipelineCmd         mBindPipelineCmd;
		BindDescriptorsCmd      mBindDescriptorSetCmd;
		BindPushConstantsCmd    mBindPushConstantsCmd;
		BindIndexBufferCmd      mBindIndexBufferCmd;
		BindVertexBufferCmd     mBindVertexBufferCmd;
		DrawCmd                 mDrawCmd;
		DrawInstancedCmd        mDrawInstancedCmd;
		DrawIndexedCmd          mDrawIndexedCmd;
		DrawIndexedInstancedCmd mDrawIndexedInstancedCmd;
		DispatchCmd             mDispatchCmd;
		BeginQueryCmd           mBeginQueryCmd;
		EndQueryCmd             mEndQueryCmd;
		ResolveQueryCmd         mResolveQueryCmd;
		BeginDebugMarkerCmd     mBeginDebugMarkerCmd;
		EndDebugMarkerCmd       mEndDebugMarkerCmd;
		AddDebugMarkerCmd       mAddDebugMarkerCmd;
		UpdateBufferCmd         mUpdateBufferCmd;
		UpdateSubresourcesCmd   mUpdateSubresourcesCmd;
	};
};
