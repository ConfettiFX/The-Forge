/*
 * Copyright (c) 2018-2019 Confetti Interactive Inc.
 *
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

// This compute shader resets the vertex count property of the IndirectDrawArguments struct.
// This needs to be done before triangle filtering is executed to reset it to 0.

#include <metal_stdlib>
using namespace metal;

#include "shader_defs.h"

struct RootConstantData
{
    uint numBatches;
};

struct IndirectDrawIndexArguments
{
    uint indexCount;
    uint instanceCount;
    uint startIndex;
    uint vertexOffset;
    uint startInstance;
};

struct BindlessDiffuseData
{
    array<texture2d<float>,MATERIAL_BUFFER_SIZE> textures;
};

struct CSData {
    constant void* vertexDataBuffer;
    constant void* vertexTexCoord;
    constant void* vertexTangent;
    constant void* vertexNormal;
    constant uint* drawIDs;
    constant void* texturesArgBuffer;
};

struct CSDataPerFrame {
    constant IndirectDrawIndexArguments* indirectDrawArgsBufferAlpha    [[id(0)]]   [NUM_CULLING_VIEWPORTS];
    constant IndirectDrawIndexArguments* indirectDrawArgsBufferNoAlpha  [[id(2)]]   [NUM_CULLING_VIEWPORTS];
    device UncompactedDrawArguments* uncompactedDrawArgsRW              [[id(4)]]   [NUM_CULLING_VIEWPORTS];
    constant uint* filteredIndicesBuffer                                [[id(6)]]   [NUM_CULLING_VIEWPORTS];
    constant uint* indirectMaterialBuffer                               [[id(8)]];
    constant void* uniforms                                             [[id(9)]];
    command_buffer icbContainerShadow                                   [[id(10)]]; // alpha, no alpha
    command_buffer icbContainerCamera                                   [[id(11)]]; // alpha, no alpha
    render_pipeline_state piplineStatesShadow                           [[id(12)]]  [2];
    render_pipeline_state piplineStatesCamera                           [[id(14)]]  [2];
};

void cmdDrawIndexedPrimitive(
                     uint tid,
                     uint commandId,
                     command_buffer cmdBuffer,
                     render_pipeline_state pipelineState,
                     constant void* vertexDataBuffer,
                     constant void* vertexTexCoord,
                     constant void* vertexTangent,
                     constant void* vertexNormal,
                     constant void* uniforms,
                     constant uint* drawIDs,
                     constant void* texturesArgBuffer,
                     constant uint* indirectMaterialBuffer,
                     constant IndirectDrawIndexArguments* indirectDrawArgs,
                     constant uint* filteredIndicesBuffer)
{
    const constant IndirectDrawIndexArguments& args(indirectDrawArgs[tid]);
    
    render_command cmd(cmdBuffer, commandId);

    if (args.indexCount)
    {
        // pipeline
        cmd.set_render_pipeline_state(pipelineState);
        
        // buffers
        cmd.set_vertex_buffer(vertexDataBuffer,         UNIT_VBPASS_POSITION);
        cmd.set_vertex_buffer(vertexTexCoord,           UNIT_VBPASS_TEXCOORD);
        cmd.set_vertex_buffer(vertexTangent,            UNIT_VBPASS_TANGENT);
        cmd.set_vertex_buffer(vertexNormal,             UNIT_VBPASS_NORMAL);
        cmd.set_vertex_buffer(uniforms,                 UNIT_VBPASS_UNIFORMS);
        cmd.set_fragment_buffer(texturesArgBuffer,      UNIT_VBPASS_TEXTURES);
        cmd.set_fragment_buffer(drawIDs + tid,          UINT_VBPASS_DRAWID);
        cmd.set_fragment_buffer(indirectMaterialBuffer, UNIT_INDIRECT_MATERIAL_RW);

        cmd.draw_indexed_primitives(
                          primitive_type::triangle,                         // primitive type
                          args.indexCount,                                  // index count
                          filteredIndicesBuffer + args.startIndex,          // index buffer
                          args.instanceCount,                               // instance count
                          args.vertexOffset,                                // base vertex
                          args.startInstance                                // base instance
        );
    }
    else
    {
        cmd.reset();
    }
}

//[numthreads(256, 1, 1)]
kernel void stageMain(uint     tid                              [[thread_position_in_grid]],
                      uint     inGroupId                        [[thread_position_in_threadgroup]],
                      constant CSData& csData                   [[buffer(UPDATE_FREQ_NONE)]],
                      constant CSDataPerFrame& csDataPerFrame   [[buffer(UPDATE_FREQ_PER_FRAME)]]
)
{
    if (tid >= MAX_DRAWS_INDIRECT)
        return;
    
    // camera view
    cmdDrawIndexedPrimitive(tid,
                            tid,
                            csDataPerFrame.icbContainerCamera,
                            csDataPerFrame.piplineStatesCamera[0],
                            csData.vertexDataBuffer,
                            csData.vertexTexCoord,
                            csData.vertexTangent,
                            csData.vertexNormal,
                            csDataPerFrame.uniforms,
                            csData.drawIDs,
                            csData.texturesArgBuffer,
                            csDataPerFrame.indirectMaterialBuffer,
                            csDataPerFrame.indirectDrawArgsBufferNoAlpha[VIEW_CAMERA],
                            csDataPerFrame.filteredIndicesBuffer[VIEW_CAMERA]
    );
    cmdDrawIndexedPrimitive(tid,
                            tid + MAX_DRAWS_INDIRECT,
                            csDataPerFrame.icbContainerCamera,
                            csDataPerFrame.piplineStatesCamera[1],
                            csData.vertexDataBuffer,
                            csData.vertexTexCoord,
                            csData.vertexTangent,
                            csData.vertexNormal,
                            csDataPerFrame.uniforms,
                            csData.drawIDs,
                            csData.texturesArgBuffer,
                            csDataPerFrame.indirectMaterialBuffer,
                            csDataPerFrame.indirectDrawArgsBufferAlpha[VIEW_CAMERA],
                            csDataPerFrame.filteredIndicesBuffer[VIEW_CAMERA]
    );

    // shadow view
    cmdDrawIndexedPrimitive(tid,
                            tid,
                            csDataPerFrame.icbContainerShadow,
                            csDataPerFrame.piplineStatesShadow[0],
                            csData.vertexDataBuffer,
                            csData.vertexTexCoord,
                            nullptr,
                            nullptr,
                            csDataPerFrame.uniforms,
                            csData.drawIDs,
                            nullptr,
                            nullptr,
                            csDataPerFrame.indirectDrawArgsBufferNoAlpha[VIEW_SHADOW],
                            csDataPerFrame.filteredIndicesBuffer[VIEW_SHADOW]
    );
    cmdDrawIndexedPrimitive(tid,
                            tid + MAX_DRAWS_INDIRECT,
                            csDataPerFrame.icbContainerShadow,
                            csDataPerFrame.piplineStatesShadow[1],
                            csData.vertexDataBuffer,
                            csData.vertexTexCoord,
                            nullptr,
                            nullptr,
                            csDataPerFrame.uniforms,
                            csData.drawIDs,
                            csData.texturesArgBuffer,
                            csDataPerFrame.indirectMaterialBuffer,
                            csDataPerFrame.indirectDrawArgsBufferAlpha[VIEW_SHADOW],
                            csDataPerFrame.filteredIndicesBuffer[VIEW_SHADOW]
    );
}
