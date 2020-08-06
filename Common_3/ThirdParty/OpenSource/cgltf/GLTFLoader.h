#pragma once

#include "cgltf.h"

#include "../../../OS/Interfaces/ILog.h"
#include "../../../Renderer/IRenderer.h"

#define IMEMORY_FROM_HEADER
#include "../../../OS/Interfaces/IMemory.h"

// MARK: - Materials

// NOTE: These are more-or-less copies of the GLTF structures, adjusted to use The Forge's types.

#define GLTF_NAME_MAX_LENGTH 64

typedef struct GLTFTextureTransform
{
	float2 mOffset;
	float2 mScale;
	float mRotation;
} GLTFTextureTransform;

typedef struct GLTFTextureView
{
	char pName[GLTF_NAME_MAX_LENGTH];
	ssize_t mTextureIndex;
	ssize_t mSamplerIndex;
	int32_t mUVStreamIndex;
	float mScale;
	GLTFTextureTransform mTransform;
} GLTFTextureView;

typedef struct GLTFMetallicRoughnessMaterial
{
	GLTFTextureView mBaseColorTexture;
	GLTFTextureView mMetallicRoughnessTexture;

	float4 mBaseColorFactor;
	float mMetallicFactor;
	float mRoughnessFactor;
} GLTFMetallicRoughnessMaterial;

typedef struct GLTFSpecularGlossinessMaterial
{
	GLTFTextureView mDiffuseTexture;
	GLTFTextureView mSpecularGlossinessTexture;

	float4 mDiffuseFactor;
	float3 mSpecularFactor;
	float mGlossinessFactor;
} PBRSpecularGlossiness;

typedef enum GLTFMaterialType
{
	GLTF_MATERIAL_TYPE_METALLIC_ROUGHNESS,
	GLTF_MATERIAL_TYPE_SPECULAR_GLOSSINESS
} GLTFMaterialType;

typedef enum GLTFMaterialAlphaMode
{
	GLTF_MATERIAL_ALPHA_MODE_OPAQUE,
	GLTF_MATERIAL_ALPHA_MODE_MASK,
	GLTF_MATERIAL_ALPHA_MODE_BLEND,
} GLTFMaterialAlphaMode;

typedef struct GLTFMaterial
{
	char pName[GLTF_NAME_MAX_LENGTH];
	union
	{
		GLTFMetallicRoughnessMaterial mMetallicRoughness;
		GLTFSpecularGlossinessMaterial mSpecularGlossiness;
	};
	GLTFMaterialType mMaterialType;

	GLTFTextureView mNormalTexture;
	GLTFTextureView mOcclusionTexture;
	GLTFTextureView mEmissiveTexture;

	float3 mEmissiveFactor;
	GLTFMaterialAlphaMode mAlphaMode;
	float mAlphaCutoff;
	bool mDoubleSided;
	bool mUnlit;
} GLTFMaterial;

typedef struct GLTFNode
{
	char pName[GLTF_NAME_MAX_LENGTH];
	uint32_t mParentIndex;
	uint32_t* pChildIndices;
	uint32_t mChildCount;
	uint32_t mMeshIndex;
	uint32_t mMeshCount;
	float* pWeights;
	uint32_t mWeightsCount;
	uint32_t mSkinIndex;

	float3 mTranslation;
	Quat mRotation;
	float3 mScale;
	mat4 mMatrix;
} GLTFNode;

typedef struct GLTFSkin
{
	char pName[GLTF_NAME_MAX_LENGTH];
	uint32_t* pJointNodeIndices;
	uint32_t mJointCount;
	uint32_t mSkeletonNodeIndex;
	mat4* pInverseBindMatrices;
} GLTFSkin;

typedef enum GLTFAttributeType
{
	GLTF_ATTRIBUTE_TYPE_INVALID,
	GLTF_ATTRIBUTE_TYPE_POSITION,
	GLTF_ATTRIBUTE_TYPE_NORMAL,
	GLTF_ATTRIBUTE_TYPE_TANGENT,
	GLTF_ATTRIBUTE_TYPE_TEXCOORD,
	GLTF_ATTRIBUTE_TYPE_COLOR,
	GLTF_ATTRIBUTE_TYPE_JOINTS,
	GLTF_ATTRIBUTE_TYPE_WEIGHTS,
	GLTF_ATTRIBUTE_TYPE_COUNT,
} GLTFAttributeType;

typedef struct GLTFMesh
{
	Point3   mMin;
	Point3   mMax;
	uint32_t mIndexCount;
	uint32_t mVertexCount;
	uint32_t mStartIndex;
	void*    pIndices;
	void*    pAttributes[GLTF_ATTRIBUTE_TYPE_COUNT];
} GLTFMesh;

typedef struct GLTFContainer
{
	cgltf_data*                 pHandle;
	GLTFMaterial*               pMaterials;
	uint32_t*                   pMaterialIndices;
	SamplerDesc*                pSamplers;
	GLTFMesh*                   pMeshes;
	GLTFNode*                   pNodes;
	GLTFSkin*                   pSkins;
	uint32_t                    mMaterialCount;
	uint32_t                    mSamplerCount;
	uint32_t                    mMeshCount;
	uint32_t                    mNodeCount;
	uint32_t                    mSkinCount;
	uint32_t                    mIndexCount;
	uint32_t                    mVertexCount;
	uint32_t                    mPadA;
	uint32_t                    mPadB;
} GLTFContainer;
static_assert(sizeof(GLTFContainer) % 16 == 0, "GLTFContainer size must be a multiple of 16");

static void gltfGetTextureView(const cgltf_data* scene, GLTFTextureView* textureView, cgltf_texture_view* sourceView)
{
	if (sourceView->texture)
	{
		if (sourceView->texture->name)
			strncpy(textureView->pName, sourceView->texture->name, GLTF_NAME_MAX_LENGTH);

		if (sourceView->texture->image)
			textureView->mTextureIndex = sourceView->texture->image - scene->images;
		else
			textureView->mTextureIndex = -1;

		if (sourceView->texture->sampler)
			textureView->mSamplerIndex = sourceView->texture->sampler - scene->samplers;
		else
			textureView->mSamplerIndex = -1;
	}
	else
	{
		textureView->mTextureIndex = -1;
		textureView->mSamplerIndex = -1;
	}

	textureView->mUVStreamIndex = sourceView->texcoord;
	textureView->mScale = sourceView->scale;

	if (sourceView->has_transform)
	{
		textureView->mTransform.mOffset[0] = sourceView->transform.offset[0];
		textureView->mTransform.mOffset[1] = sourceView->transform.offset[1];
		textureView->mTransform.mScale[0] = sourceView->transform.scale[0];
		textureView->mTransform.mScale[1] = sourceView->transform.scale[1];
		textureView->mTransform.mRotation = sourceView->transform.rotation;
	}
	else
	{
		textureView->mTransform.mOffset[0] = 0.0f;
		textureView->mTransform.mOffset[1] = 0.0f;
		textureView->mTransform.mScale[0] = 1.0f;
		textureView->mTransform.mScale[1] = 1.0f;
		textureView->mTransform.mRotation = 0.0;
	}
}

#ifndef GL_NEAREST
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_NEAREST_MIPMAP_NEAREST         0x2700
#define GL_LINEAR_MIPMAP_NEAREST          0x2701
#define GL_NEAREST_MIPMAP_LINEAR          0x2702
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_REPEAT                         0x2901
#define GL_CLAMP_TO_EDGE                  0x812F
#define GL_MIRRORED_REPEAT                0x8370
#define GL_CLAMP_TO_BORDER                0x812D
#endif

static inline AddressMode gltfConvertWrapMode(cgltf_int wrapMode)
{
	switch (wrapMode)
	{
	case GL_REPEAT:
		return ADDRESS_MODE_REPEAT;
	case GL_CLAMP_TO_EDGE:
		return ADDRESS_MODE_CLAMP_TO_EDGE;
	case GL_MIRRORED_REPEAT:
		return ADDRESS_MODE_MIRROR;
	case GL_CLAMP_TO_BORDER:
		return ADDRESS_MODE_CLAMP_TO_BORDER;
	default:
		LOGF(LogLevel::eERROR, "Invalid GLTF wrap mode %i", wrapMode);
		return ADDRESS_MODE_REPEAT;
	}
}

static inline FilterType gltfConvertFilter(cgltf_int filter)
{
	switch (filter)
	{
	case GL_NEAREST:
	case GL_NEAREST_MIPMAP_NEAREST:
	case GL_NEAREST_MIPMAP_LINEAR:
		return FILTER_NEAREST;

	case GL_LINEAR:
	case GL_LINEAR_MIPMAP_NEAREST:
	case GL_LINEAR_MIPMAP_LINEAR:
		return FILTER_LINEAR;

	default:
		LOGF(LogLevel::eERROR, "Invalid GLTF filter %i", filter);
		return FILTER_NEAREST;

	}
}

static SamplerDesc convertCGLTFSamplerToSamplerDesc(cgltf_sampler sampler)
{
	SamplerDesc desc = {};
	desc.mAddressU = gltfConvertWrapMode(sampler.wrap_s);
	desc.mAddressV = gltfConvertWrapMode(sampler.wrap_t);
	desc.mMinFilter = gltfConvertFilter(sampler.min_filter);
	desc.mMagFilter = gltfConvertFilter(sampler.mag_filter);

	desc.mMipMapMode = MIPMAP_MODE_NEAREST;
	switch (sampler.min_filter)
	{
	case GL_NEAREST_MIPMAP_LINEAR:
	case GL_LINEAR_MIPMAP_LINEAR:
		desc.mMipMapMode = MIPMAP_MODE_LINEAR;
	default:
		break;
	}
	switch (sampler.mag_filter)
	{
	case GL_NEAREST_MIPMAP_LINEAR:
	case GL_LINEAR_MIPMAP_LINEAR:
		desc.mMipMapMode = MIPMAP_MODE_LINEAR;
	default:
		break;
	}

	return desc;
}

typedef enum GLTFFlags
{
	GLTF_FLAG_LOAD_VERTICES = 0x1,
	GLTF_FLAG_CALCULATE_BOUNDS = 0x2,
} GLTFFlags;

static uint32_t gltfLoadContainer(const char* pFileName, GLTFFlags flags, GLTFContainer** ppGLTF)
{
	FileStream file = {};

	if (!fsOpenStreamFromPath(RD_MESHES, pFileName, FM_READ_BINARY, &file))
	{
		LOGF(eERROR, "Failed to open gltf file %s", pFileName);
		ASSERT(false);
		return -1;
	}

	ssize_t fileSize = fsGetStreamFileSize(&file);
	void* fileData = tf_malloc(fileSize);
	fsReadFromStream(&file, fileData, fileSize);
	fsCloseStream(&file);

	cgltf_result result = cgltf_result_invalid_gltf;
	cgltf_options options = {};
	cgltf_data* data = NULL;
	options.memory_alloc = [](void* user, cgltf_size size) { return tf_malloc(size); };
	options.memory_free = [](void* user, void* ptr) { tf_free(ptr); };
	result = cgltf_parse(&options, fileData, fileSize, &data);

	if (cgltf_result_success != result)
	{
		LOGF(eERROR, "Failed to parse gltf data with error %u", (uint32_t)result);
		ASSERT(false);
		return result;
	}

#ifdef _DEBUG
	result = cgltf_validate(data);
	if (cgltf_result_success != result)
	{
		LOGF(eERROR, "Failed to validate gltf file %s with error %u", (uint32_t)result);
		ASSERT(false);
		return result;
	}
#endif

	// Load buffers located in separate files (.bin) using our file system
	for (uint32_t i = 0; i < data->buffers_count; ++i)
	{
		const char* uri = data->buffers[i].uri;

		if (!uri || data->buffers[i].data)
		{
			continue;
		}

		if (strncmp(uri, "data:", 5) != 0 && !strstr(uri, "://"))
		{
			char binFile[FS_MAX_PATH] = {};
			char parentPath[FS_MAX_PATH] = {};
			fsGetParentPath(pFileName, parentPath);
			fsAppendPathComponent(parentPath, uri, binFile);
			FileStream fs = {};	
			if (fsOpenStreamFromPath(RD_MESHES, binFile, FM_READ_BINARY, &fs))
			{
				ASSERT(fsGetStreamFileSize(&fs) >= (ssize_t)data->buffers[i].size);
				data->buffers[i].data = tf_malloc(data->buffers[i].size);
				fsReadFromStream(&fs, data->buffers[i].data, data->buffers[i].size);
				fsCloseStream(&fs);
			}
		}
	}

	result = cgltf_load_buffers(&options, data, pFileName);
	if (cgltf_result_success != result)
	{
		LOGF(eERROR, "Failed to load buffers from gltf file %s with error %u", pFileName, (uint32_t)result);
		ASSERT(false);
		tf_free(fileData);
		return result;
	}

	uint32_t drawCount = 0;
	uint32_t nodeCount = (uint32_t)data->nodes_count;
	uint32_t skinCount = (uint32_t)data->skins_count;
	uint32_t materialCount = (uint32_t)data->materials_count;
	uint32_t samplerCount = (uint32_t)data->samplers_count;
	uint32_t strides[cgltf_attribute_type_weights + 1] = {};
	// Collect geometry
	uint32_t indexCount = 0;
	uint32_t vertexCount = 0;

	// Find number of traditional draw calls required to draw this piece of geometry
	for (uint32_t i = 0; i < data->meshes_count; ++i)
	{
		for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
		{
			const cgltf_primitive* prim = &data->meshes[i].primitives[p];
			indexCount += (uint32_t)prim->indices->count;
			vertexCount += (uint32_t)prim->attributes->data->count;

			for (uint32_t a = 0; a < prim->attributes_count; ++a)
			{
				const cgltf_attribute* attr = &prim->attributes[a];
				strides[attr->type] = (uint32_t)attr->data->stride;
			}
			++drawCount;
		}
	}

	uint32_t totalSize = 0;
	uint32_t prevTotalSize = 0;

#define ALIGN_16(s)(round_up((s), 16))

	totalSize += ALIGN_16(sizeof(GLTFContainer)); // GLTFContainer
	totalSize += ALIGN_16(materialCount * sizeof(GLTFMaterial)); // Materials
	totalSize += ALIGN_16(drawCount * sizeof(uint32_t));         // Material indices
	totalSize += ALIGN_16(samplerCount * sizeof(SamplerDesc));
	totalSize += ALIGN_16(drawCount * sizeof(GLTFMesh));

	totalSize += ALIGN_16(nodeCount * sizeof(GLTFNode));
	prevTotalSize = totalSize;
	for (uint32_t i = 0; i < data->nodes_count; ++i)
	{
		cgltf_node* sourceNode = &data->nodes[i];
		totalSize += ALIGN_16((uint32_t)sourceNode->weights_count * sizeof(float));
		totalSize += ALIGN_16((uint32_t)sourceNode->children_count * sizeof(uint32_t));
	}
	uint32_t nodeMemSize = totalSize - prevTotalSize;

	totalSize += ALIGN_16(skinCount * sizeof(GLTFSkin));
	prevTotalSize = totalSize;
	for (uint32_t i = 0; i < data->skins_count; ++i)
	{
		cgltf_skin* sourceNode = &data->skins[i];
		totalSize += ALIGN_16((uint32_t)sourceNode->joints_count * sizeof(mat4));
		totalSize += ALIGN_16((uint32_t)sourceNode->joints_count * sizeof(uint32_t));
	}
	uint32_t skinMemSize = totalSize - prevTotalSize;

	if (flags & GLTF_FLAG_LOAD_VERTICES)
	{
		for (uint32_t i = 0; i < sizeof(strides) / sizeof(strides[0]); ++i)
			totalSize += vertexCount * strides[i];
		totalSize += indexCount * sizeof(uint32_t);
	}
	GLTFContainer* pGLTF = (GLTFContainer*)tf_calloc(1, totalSize);
	ASSERT(pGLTF);

	pGLTF->pHandle = data;
	pGLTF->pMaterials = (GLTFMaterial*)(pGLTF + 1);
	pGLTF->pMaterialIndices = (uint32_t*)((uint8_t*)pGLTF->pMaterials + ALIGN_16(materialCount * sizeof(GLTFMaterial)));
	pGLTF->pSamplers = (SamplerDesc*)((uint8_t*)pGLTF->pMaterialIndices + ALIGN_16(drawCount * sizeof(uint32_t)));
	pGLTF->pMeshes = (GLTFMesh*)((uint8_t*)pGLTF->pSamplers + ALIGN_16(samplerCount * sizeof(SamplerDesc)));
	pGLTF->pNodes = (GLTFNode*)((uint8_t*)pGLTF->pMeshes + ALIGN_16(drawCount * sizeof(GLTFMesh)));
	uint8_t* nodeMem = (uint8_t*)pGLTF->pNodes + ALIGN_16(nodeCount * sizeof(GLTFNode));
	pGLTF->pSkins = (GLTFSkin*)(nodeMem + ALIGN_16(nodeMemSize));
	uint8_t* skinMem = (uint8_t*)pGLTF->pSkins + ALIGN_16(skinCount * sizeof(GLTFSkin));

	uint8_t* vertices[cgltf_attribute_type_weights + 1] = {};
	uint8_t* indices = NULL;
	if (flags & GLTF_FLAG_LOAD_VERTICES)
	{
		vertices[0] = skinMem + skinMemSize;
		for (uint32_t i = 1; i < sizeof(strides) / sizeof(strides[0]); ++i)
			vertices[i] = vertices[i - 1] + (strides[i - 1] * vertexCount);
		indices = vertices[GLTF_ATTRIBUTE_TYPE_COUNT - 1] + strides[GLTF_ATTRIBUTE_TYPE_COUNT - 1] * vertexCount;
	}

	pGLTF->mMaterialCount = materialCount;
	pGLTF->mMeshCount = drawCount;
	pGLTF->mSamplerCount = samplerCount;
	pGLTF->mNodeCount = nodeCount;
	pGLTF->mSkinCount = skinCount;

	indexCount = 0;
	vertexCount = 0;
	drawCount = 0;

	uint32_t* meshIndices = (uint32_t*)alloca(data->meshes_count * sizeof(uint32_t));

	for (uint32_t i = 0; i < data->meshes_count; ++i)
	{
		meshIndices[i] = drawCount;

		for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
		{
			const cgltf_primitive* prim = &data->meshes[i].primitives[p];
			/************************************************************************/
			// Fill draw arguments for this primitive
			/************************************************************************/
			pGLTF->pMeshes[drawCount].mIndexCount = (uint32_t)prim->indices->count;
			pGLTF->pMeshes[drawCount].mVertexCount = (uint32_t)prim->attributes->data->count;
			pGLTF->pMeshes[drawCount].mStartIndex = indexCount;

			if (flags & GLTF_FLAG_LOAD_VERTICES)
			{
				for (uint32_t i = 0; i < sizeof(strides) / sizeof(strides[0]); ++i)
					pGLTF->pMeshes[drawCount].pAttributes[i] = vertices[i] + (strides[i] * vertexCount);
				pGLTF->pMeshes[drawCount].pIndices = indices + sizeof(uint32_t) * indexCount;
				/************************************************************************/
				// Fill index buffer for this primitive
				/************************************************************************/
				uint32_t* dst = (uint32_t*)pGLTF->pMeshes[drawCount].pIndices;
				for (uint32_t idx = 0; idx < prim->indices->count; ++idx)
					dst[idx] = (uint32_t)cgltf_accessor_read_index(prim->indices, idx);

				for (uint32_t a = 0; a < prim->attributes_count; ++a)
				{
					cgltf_attribute* attr = &prim->attributes[a];
					const uint8_t* src = (uint8_t*)attr->data->buffer_view->buffer->data + attr->data->buffer_view->offset;
					uint8_t* dst = (uint8_t*)pGLTF->pMeshes[drawCount].pAttributes[attr->type];
					memcpy(dst, src, attr->data->buffer_view->size);
				}
			}

			if (flags & GLTF_FLAG_CALCULATE_BOUNDS)
			{
				for (uint32_t a = 0; a < prim->attributes_count; ++a)
				{
					cgltf_attribute* attribute = &prim->attributes[a];
					float3* src = (float3*)((uint8_t*)attribute->data->buffer_view->buffer->data + attribute->data->buffer_view->offset);

					if (cgltf_attribute_type_position == attribute->type)
					{
						if (attribute->data->has_min)
						{
							pGLTF->pMeshes[drawCount].mMin = Point3(attribute->data->min[0], attribute->data->min[1], attribute->data->min[2]);
						}
						else
						{
							pGLTF->pMeshes[drawCount].mMin = Point3(INFINITY);
							for (uint32_t i = 0; i < attribute->data->count; ++i)
								pGLTF->pMeshes[drawCount].mMin = minPerElem(Point3(f3Tov3(src[i])), pGLTF->pMeshes[drawCount].mMin);
						}
						if (attribute->data->has_max)
						{
							pGLTF->pMeshes[drawCount].mMax = Point3(attribute->data->max[0], attribute->data->max[1], attribute->data->max[2]);
						}
						else
						{
							pGLTF->pMeshes[drawCount].mMax = Point3(-INFINITY);
							for (uint32_t i = 0; i < attribute->data->count; ++i)
								pGLTF->pMeshes[drawCount].mMin = maxPerElem(Point3(f3Tov3(src[i])), pGLTF->pMeshes[drawCount].mMin);
						}
					}
				}
			}

			indexCount += (uint32_t)prim->indices->count;
			vertexCount += (uint32_t)prim->attributes->data->count;
			++drawCount;
		}
	}

	// Collect nodes
	for (uint32_t i = 0; i < nodeCount; ++i)
	{
		cgltf_node* sourceNode = &data->nodes[i];
		GLTFNode* outNode = &pGLTF->pNodes[i];

		if (sourceNode->name)
			strncpy(outNode->pName, sourceNode->name, GLTF_NAME_MAX_LENGTH);

		if (sourceNode->parent)
			outNode->mParentIndex = (uint32_t)(sourceNode->parent - data->nodes);
		else
			outNode->mParentIndex = -1;

		outNode->mChildCount = (uint32_t)sourceNode->children_count;
		if (sourceNode->children)
		{
			outNode->pChildIndices = (uint32_t*)(nodeMem);
			for (size_t i = 0; i < outNode->mChildCount; i += 1)
				outNode->pChildIndices[i] = (uint32_t)(sourceNode->children[i] - data->nodes);

			nodeMem += outNode->mChildCount * sizeof(uint32_t);
		}
		else
		{
			outNode->pChildIndices = NULL;
		}

		if (sourceNode->mesh)
		{
			outNode->mMeshIndex = meshIndices[(uint32_t)(sourceNode->mesh - data->meshes)];
			outNode->mMeshCount = (uint32_t)sourceNode->mesh->primitives_count;
		}
		else
		{
			outNode->mMeshIndex = UINT_MAX;
		}

		outNode->mWeightsCount = (uint32_t)sourceNode->weights_count;
		if (sourceNode->weights)
		{
			outNode->pWeights = (float*)(nodeMem);
			for (size_t i = 0; i < outNode->mWeightsCount; i += 1)
				outNode->pWeights[i] = sourceNode->weights[i];

			nodeMem += ALIGN_16(outNode->mWeightsCount * sizeof(float));
		}
		else
		{
			outNode->pWeights = NULL;
		}

		if (sourceNode->has_translation)
			outNode->mTranslation = float3(sourceNode->translation);
		else
			outNode->mTranslation = float3(0);

		if (sourceNode->has_rotation)
			outNode->mRotation = Quat(sourceNode->rotation[0], sourceNode->rotation[1], sourceNode->rotation[2], sourceNode->rotation[3]);
		else
			outNode->mRotation = Quat::identity();

		if (sourceNode->has_scale)
			outNode->mScale = float3(sourceNode->scale);
		else
			outNode->mScale = float3(1);

		if (sourceNode->has_matrix)
		{
			for (int i = 0; i < 4; i += 1)
			{
				for (int j = 0; j < 4; j += 1)
				{
					outNode->mMatrix[i][j] = sourceNode->matrix[4 * i + j];
				}
			}
		}
		else
		{
			outNode->mMatrix = mat4::translation(f3Tov3(outNode->mTranslation)) * mat4::rotation(outNode->mRotation) * mat4::scale(f3Tov3(outNode->mScale));
		}
	}

	// Collect skins
	for (uint32_t i = 0; i < data->skins_count; ++i)
	{
		cgltf_skin* sourceSkin = &data->skins[i];
		GLTFSkin* pOutSkin = &pGLTF->pSkins[i];

		if (sourceSkin->name)
			strncpy(pOutSkin->pName, sourceSkin->name, GLTF_NAME_MAX_LENGTH);

		if (sourceSkin->skeleton)
			pOutSkin->mSkeletonNodeIndex = (uint32_t)(sourceSkin->skeleton - data->nodes);
		else
			pOutSkin->mSkeletonNodeIndex = UINT_MAX;

		pOutSkin->mJointCount = (uint32_t)sourceSkin->joints_count;

		pOutSkin->pJointNodeIndices = (uint32_t*)skinMem;
		skinMem += pOutSkin->mJointCount * sizeof(uint32_t);
		for (uint32_t i = 0; i < pOutSkin->mJointCount; ++i)
			pOutSkin->pJointNodeIndices[i] = (uint32_t)(sourceSkin->joints[i] - data->nodes);

		pOutSkin->pInverseBindMatrices = (mat4*)skinMem;
		skinMem += ALIGN_16(pOutSkin->mJointCount * sizeof(mat4));
		cgltf_accessor_unpack_floats(sourceSkin->inverse_bind_matrices, (float*)pOutSkin->pInverseBindMatrices, sizeof(mat4) / sizeof(float) * pOutSkin->mJointCount);
	}

	// Collect samplers
	for (uint32_t i = 0; i < data->samplers_count; ++i)
	{
		cgltf_sampler* sourceSampler = &data->samplers[i];
		pGLTF->pSamplers[i] = convertCGLTFSamplerToSamplerDesc(*sourceSampler);
	}

	// Collect materials
	for (uint32_t i = 0; i < data->materials_count; ++i)
	{
		cgltf_material* sourceMaterial = &data->materials[i];
		GLTFMaterial* pOutMaterial = &pGLTF->pMaterials[i];

		if (sourceMaterial->name)
			strncpy(pOutMaterial->pName, sourceMaterial->name, GLTF_NAME_MAX_LENGTH);

		if (sourceMaterial->has_pbr_specular_glossiness)
		{
			pOutMaterial->mMaterialType = GLTF_MATERIAL_TYPE_SPECULAR_GLOSSINESS;

			gltfGetTextureView(data, &pOutMaterial->mSpecularGlossiness.mDiffuseTexture, &sourceMaterial->pbr_specular_glossiness.diffuse_texture);
			gltfGetTextureView(data, &pOutMaterial->mSpecularGlossiness.mSpecularGlossinessTexture, &sourceMaterial->pbr_specular_glossiness.specular_glossiness_texture);

			pOutMaterial->mSpecularGlossiness.mDiffuseFactor = float4(sourceMaterial->pbr_specular_glossiness.diffuse_factor);
			pOutMaterial->mSpecularGlossiness.mSpecularFactor = float3(sourceMaterial->pbr_specular_glossiness.specular_factor);
			pOutMaterial->mSpecularGlossiness.mGlossinessFactor = sourceMaterial->pbr_specular_glossiness.glossiness_factor;
		}
		else
		{
			pOutMaterial->mMaterialType = GLTF_MATERIAL_TYPE_METALLIC_ROUGHNESS;

			gltfGetTextureView(data, &pOutMaterial->mMetallicRoughness.mBaseColorTexture, &sourceMaterial->pbr_metallic_roughness.base_color_texture);
			gltfGetTextureView(data, &pOutMaterial->mMetallicRoughness.mMetallicRoughnessTexture, &sourceMaterial->pbr_metallic_roughness.metallic_roughness_texture);

			pOutMaterial->mMetallicRoughness.mBaseColorFactor = float4(sourceMaterial->pbr_metallic_roughness.base_color_factor);
			pOutMaterial->mMetallicRoughness.mMetallicFactor = sourceMaterial->pbr_metallic_roughness.metallic_factor;
			pOutMaterial->mMetallicRoughness.mRoughnessFactor = sourceMaterial->pbr_metallic_roughness.roughness_factor;
		}

		gltfGetTextureView(data, &pOutMaterial->mNormalTexture, &sourceMaterial->normal_texture);
		gltfGetTextureView(data, &pOutMaterial->mOcclusionTexture, &sourceMaterial->occlusion_texture);
		gltfGetTextureView(data, &pOutMaterial->mEmissiveTexture, &sourceMaterial->emissive_texture);

		pOutMaterial->mEmissiveFactor = float3(sourceMaterial->emissive_factor);

		switch (sourceMaterial->alpha_mode)
		{
		case cgltf_alpha_mode_opaque:
			pOutMaterial->mAlphaMode = GLTF_MATERIAL_ALPHA_MODE_OPAQUE;
			break;
		case cgltf_alpha_mode_mask:
			pOutMaterial->mAlphaMode = GLTF_MATERIAL_ALPHA_MODE_MASK;
			break;
		case cgltf_alpha_mode_blend:
			pOutMaterial->mAlphaMode = GLTF_MATERIAL_ALPHA_MODE_BLEND;
			break;
		}

		pOutMaterial->mAlphaCutoff = sourceMaterial->alpha_cutoff;
		pOutMaterial->mDoubleSided = sourceMaterial->double_sided;
		pOutMaterial->mUnlit = sourceMaterial->unlit;
	}

	drawCount = 0;

	// Collect material indices
	for (uint32_t i = 0; i < data->meshes_count; ++i)
	{
		for (uint32_t p = 0; p < data->meshes[i].primitives_count; ++p)
		{
			const cgltf_primitive* prim = &data->meshes[i].primitives[p];
			pGLTF->pMaterialIndices[drawCount] = (uint32_t)(prim->material - data->materials);
			++drawCount;
		}
	}

	*ppGLTF = pGLTF;

	data->file_data = fileData;

	return result;
}

static void gltfUnloadContainer(GLTFContainer* pGLTF)
{
	ASSERT(pGLTF);
	cgltf_free(pGLTF->pHandle);
	tf_free(pGLTF);
}

void gltfLoadTextureAtIndex(GLTFContainer* pGLTF, size_t index, bool isSRGB, SyncToken* token, TextureContainerType container, Texture** ppOutTexture)
{
	if (!pGLTF)
	{
		LOGF(LogLevel::eWARNING, "Scene passed to gltfLoadTextureAtIndex is NULL.");
		return;
	}

	ASSERT(index < pGLTF->pHandle->images_count);
	cgltf_image* image = pGLTF->pHandle->images + index;

	TextureLoadDesc loadDesc = {};
	loadDesc.ppTexture = ppOutTexture;
	loadDesc.mCreationFlag = isSRGB ? TEXTURE_CREATION_FLAG_SRGB : TEXTURE_CREATION_FLAG_NONE;
	loadDesc.mContainer = container;

	if (!image->buffer_view)
	{
		loadDesc.pFileName = image->uri;
		addResource(&loadDesc, token);
	}
}
