//
//  cull_argument_buffers.h
//  Visibility_Buffer
//
//  Created by agent47 on 06/05/20.
//  Copyright © 2020 Confetti. All rights reserved.
//

#ifndef cull_argument_buffers_h
#define cull_argument_buffers_h

struct SceneVertexPos
{
    packed_float3 position;
};

struct Uniforms_visibilityBufferConstants
{
	float4x4 mWorldViewProjMat[NUM_CULLING_VIEWPORTS];
	CullingViewPort mCullingViewports[NUM_CULLING_VIEWPORTS];
};

struct CSData
{
    constant uint* materialProps;
    constant SceneVertexPos* vertexDataBuffer;
    constant uint* indexDataBuffer;
    constant MeshConstants* meshConstantsBuffer;
};

struct CSDataPerFrame
{
	device uint* filteredIndicesBuffer[NUM_CULLING_VIEWPORTS];
    device uint* indirectMaterialBuffer;
    device atomic_uint* indirectDrawArgsBufferAlpha [NUM_CULLING_VIEWPORTS];
    device atomic_uint* indirectDrawArgsBufferNoAlpha [NUM_CULLING_VIEWPORTS];
    device UncompactedDrawArguments* uncompactedDrawArgs[NUM_CULLING_VIEWPORTS];
	device UncompactedDrawArguments* uncompactedDrawArgsRW[NUM_CULLING_VIEWPORTS];
	
	constant Uniforms_visibilityBufferConstants& visibilityBufferConstants;
};


#endif /* cull_argument_buffers_h */
