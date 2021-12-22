
//  batch_compaction
RES(Buffer(UncompactedDrawArguments), uncompactedDrawArgs[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, t0, binding = 6);
RES(Buffer(uint), materialProps, UPDATE_FREQ_NONE, t11, binding = 1);
RES(RWBuffer(uint), indirectMaterialBuffer, UPDATE_FREQ_PER_FRAME, u19, binding = 7);

// clear_buffers
RES(RWBuffer(atomic_uint), indirectDrawArgsBufferAlpha[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, u20, binding = 8);
RES(RWBuffer(atomic_uint), indirectDrawArgsBufferNoAlpha[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, u30, binding = 9);

// triangle_filtering
RES(ByteBuffer, vertexDataBuffer, UPDATE_FREQ_NONE, t12, binding = 2);
RES(ByteBuffer, indexDataBuffer, UPDATE_FREQ_NONE, t13, binding = 3);
RES(RWByteBuffer, filteredIndicesBuffer[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, u50, binding = 11);

RES(Buffer(MeshConstants), meshConstantsBuffer, UPDATE_FREQ_NONE, t14, binding = 4);
RES(RWBuffer(UncompactedDrawArguments), uncompactedDrawArgsRW[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, u40, binding = 10);

// ICB
#ifdef METAL

STRUCT(IndirectDrawIndexArguments)
{
    DATA(uint, indexCount, None);
    DATA(uint, instanceCount, None);
    DATA(uint, startIndex, None);
    DATA(uint, vertexOffset, None);
    DATA(uint, startInstance, None);
	DATA(uint, pad[3], None);
};

// ICB
RES(Buffer(void), vertexTexCoord, UPDATE_FREQ_NONE, t50, binding=1);
RES(Buffer(void), vertexTangent, UPDATE_FREQ_NONE, t51, binding=1);
RES(Buffer(void), vertexNormal, UPDATE_FREQ_NONE, t52, binding=1);
// force AB generation by declaring drawIDs as an array, this is necessary because
// the UPDATE_FREQ_NONE resources need to reside in ABs for the descriptor extraction to work
RES(Buffer(uint), drawIDs[1], UPDATE_FREQ_NONE, t53, binding=1);
RES(Buffer(void), texturesArgBuffer, UPDATE_FREQ_NONE, t54, binding=1);

// ICB
RES(Buffer(IndirectDrawIndexArguments), indirectDrawArgsBufferAlphaICB[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, t14, binding=0);
RES(Buffer(IndirectDrawIndexArguments), indirectDrawArgsBufferNoAlphaICB[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, t2, binding=0);
RES(Buffer(uint), filteredIndicesBufferICB[NUM_CULLING_VIEWPORTS], UPDATE_FREQ_PER_FRAME, t10, binding=0);
RES(Buffer(uint), indirectMaterialBufferICB, UPDATE_FREQ_PER_FRAME, t12, binding=0);
RES(command_buffer, icbContainerShadow, UPDATE_FREQ_PER_FRAME, t4, binding=1);
RES(command_buffer, icbContainerCamera, UPDATE_FREQ_PER_FRAME, t5, binding=2);
RES(render_pipeline_state, piplineStatesShadow[2], UPDATE_FREQ_PER_FRAME, t6, binding=3);
RES(render_pipeline_state, piplineStatesCamera[2], UPDATE_FREQ_PER_FRAME, t8, binding=4);
#endif

#if !defined(METAL) || defined(TRIANGLE_FILTERING)
CBUFFER(batchData_rootcbv, UPDATE_FREQ_USER, b15, binding = 5)
{
	DATA(SmallBatchData, smallBatchDataBuffer[BATCH_COUNT], None);
};
#endif

DECLARE_RESOURCES()