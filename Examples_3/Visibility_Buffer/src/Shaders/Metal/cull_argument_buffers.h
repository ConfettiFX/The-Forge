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

struct IndirectDrawIndexArguments
{
    uint indexCount;
    uint instanceCount;
    uint startIndex;
    uint vertexOffset;
    uint startInstance;
	uint pad[3];
};

struct CSData
{
    constant uint* materialProps;
    constant SceneVertexPos* vertexDataBuffer;
    constant uint* indexDataBuffer;
    constant MeshConstants* meshConstantsBuffer;
	
	// ICB
    constant void* vertexTexCoord;
    constant void* vertexTangent;
    constant void* vertexNormal;
    constant uint* drawIDs;
    constant void* texturesArgBuffer;
};

struct CSDataPerFrame
{
	device uint* filteredIndicesBuffer[NUM_CULLING_VIEWPORTS];
    device uint* indirectMaterialBuffer;
    device atomic_uint* indirectDrawArgsBufferAlpha [NUM_CULLING_VIEWPORTS];
    device atomic_uint* indirectDrawArgsBufferNoAlpha [NUM_CULLING_VIEWPORTS];
    device UncompactedDrawArguments* uncompactedDrawArgs[NUM_CULLING_VIEWPORTS];
	device UncompactedDrawArguments* uncompactedDrawArgsRW[NUM_CULLING_VIEWPORTS];
	
	constant PerFrameConstants& uniforms;
	
	// ICB
    constant IndirectDrawIndexArguments* indirectDrawArgsBufferAlphaICB [NUM_CULLING_VIEWPORTS];
    constant IndirectDrawIndexArguments* indirectDrawArgsBufferNoAlphaICB  [NUM_CULLING_VIEWPORTS];
    constant uint* filteredIndicesBufferICB                                [NUM_CULLING_VIEWPORTS];
    constant uint* indirectMaterialBufferICB;
    command_buffer icbContainerShadow; // alpha, no alpha
    command_buffer icbContainerCamera; // alpha, no alpha
    render_pipeline_state piplineStatesShadow[2];
	render_pipeline_state piplineStatesCamera[2];
};


#endif /* cull_argument_buffers_h */
