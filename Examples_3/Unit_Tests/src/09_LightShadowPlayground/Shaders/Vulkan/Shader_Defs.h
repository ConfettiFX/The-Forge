
#ifndef _SHADER_DEFS_H
#define _SHADER_DEFS_H


#define MAX_TEXTURE_UNITS 256U


// This defines the amount of triangles that will be processed in parallel by the
// compute shader in the triangle filtering process.
// Should be a multiple of the wavefront size
#define CLUSTER_SIZE 256

// BATCH_COUNT limits the amount of triangle batches we can process on the GPU.
// It depends on the amoutnt of data we need to store in the constant buffers, since
// the max constant buffer size is 64KB - sizeof(SmallBatchData) * 2048 = 64KB
#define BATCH_COUNT 2048


// This value defines the amount of threads per group that will be used to clear the
// indirect draw buffers.
#define CLEAR_THREAD_COUNT 256

// The following value defines the maximum amount of indirect draw calls that will be
// drawn at once. This value depends on the number of submeshes or individual objects
// in the scene. Changing a scene will require to change this value accordingly.
#define MAX_DRAWS_INDIRECT 256

// This defines the amount of viewports that are going to be culled in parallel.
#define NUM_CULLING_VIEWPORTS 2
#define VIEW_SHADOW 0
#define VIEW_CAMERA 1

// Size for the material buffer assuming each draw call uses one material index.
// The 4 values here stands for the 4 types of rendering passes used in the demo:
// alpha_tested_view0, opaque_view0, alpha_tested_view1, opaque_view1
#define MATERIAL_BUFFER_SIZE (MAX_DRAWS_INDIRECT * 2 * NUM_CULLING_VIEWPORTS)


// The following values point to the position in the indirect draw buffer that holds the
// number of draw calls to draw after triangle filtering and batch compaction.
// This value number is stored in the last position of the indirect draw buffer.
// So it depends on MAX_DRAWS_INDIRECT
#define INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS 8
#define DRAW_COUNTER_SLOT_POS ((MAX_DRAWS_INDIRECT-1)*INDIRECT_DRAW_ARGUMENTS_STRUCT_NUM_ELEMENTS)
#define DRAW_COUNTER_SLOT_OFFSET_IN_BYTES (DRAW_COUNTER_SLOT_POS*sizeof(uint))



// This function is used to get the offset of the current material base index depending
	// on the type of geometry and on the culling view.
#ifdef NO_GLSL_DEFINITIONS
static uint BaseMaterialBuffer(bool alpha, uint viewID)
#else
uint BaseMaterialBuffer(bool alpha, uint viewID)
#endif
{
	return (viewID * 2 + (alpha ? 0 : 1)) * MAX_DRAWS_INDIRECT;
}

struct RootConstant
{
	uint drawId;
};

struct SmallBatchData
{
	uint mMeshIndex;	  // Index into meshConstants
	uint mIndexOffset;	  // Index relative to the meshConstants[meshIndex].indexOffset
	uint mFaceCount;	  // Number of faces in this small batch
	uint mOutputIndexOffset; // Offset into the output index buffer
	uint mDrawBatchStart;	// First slot for the current draw call
	uint mAccumDrawIndex;
	uint _pad0;
	uint _pad1;
};



struct MeshConstants
{
	uint	faceCount;
	uint indexOffset;
	uint materialID;
	uint twoSided; //0 or 1
};


struct UncompactedDrawArguments
{
	uint mNumIndices;
	uint mStartIndex;
	uint mMaterialID;
	uint pad_;
};

struct CullingViewPort
{
	vec2 mWindowSize;
	uint mSampleCount;
	uint _pad0;
};



#define UNIT_UNCOMPACTED_ARGS UPDATE_FREQ_PER_FRAME, binding = 0
#define UNIT_MATERIAL_PROPS   UPDATE_FREQ_NONE, binding = 1
#define UNIT_VERTEX_DATA UPDATE_FREQ_NONE, binding = 2
#define UNIT_INDEX_DATA UPDATE_FREQ_NONE, binding = 3
#define UNIT_MESH_CONSTANTS UPDATE_FREQ_NONE, binding = 4
#define UNIT_BATCH_DATA_CBV UPDATE_FREQ_NONE, binding = 5
#define UNIT_UNIFORMS_CBV UPDATE_FREQ_PER_FRAME, binding = 6
#define UNIT_INDIRECT_MATERIAL_RW UPDATE_FREQ_PER_FRAME, binding = 7
#define UNIT_INDIRECT_DRAW_ARGS_ALPHA_RW UPDATE_FREQ_PER_FRAME, binding = 8
#define UNIT_INDIRECT_DRAW_ARGS_RW UPDATE_FREQ_PER_FRAME, binding = 9
#define UNIT_UNCOMPACTED_ARGS_RW UPDATE_FREQ_PER_FRAME, binding = 10
#define UNIT_INDEX_DATA_RW UPDATE_FREQ_PER_FRAME, binding = 11

#endif // !SHADER_DEFS_H
