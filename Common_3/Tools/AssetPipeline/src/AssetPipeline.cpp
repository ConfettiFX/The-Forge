/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
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

#include "AssetPipeline.h"

// Tiny stl
#include "../../../ThirdParty/OpenSource/EASTL/string.h"
#include "../../../ThirdParty/OpenSource/EASTL/vector.h"
#include "../../../ThirdParty/OpenSource/EASTL/unordered_map.h"

// OZZ
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/stream.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_skeleton.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_animation.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/skeleton_builder.h"
#include "../../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/animation_builder.h"

// TressFX
#include "../../../ThirdParty/OpenSource/TressFX/TressFXAsset.h"

#define CGLTF_WRITE_IMPLEMENTATION
#define CGLTF_IMPLEMENTATION
#include "../../../ThirdParty/OpenSource/cgltf/cgltf_write.h"

#define IMAGE_CLASS_ALLOWED
#include "../../../OS/Image/Image.h"
#include "../../../OS/Interfaces/IOperatingSystem.h"
#include "../../../OS/Interfaces/IFileSystem.h"
#include "../../../OS/Interfaces/ILog.h"
#include "../../../OS/Interfaces/IMemory.h"    //NOTE: this should be the last include in a .cpp

typedef eastl::unordered_map<eastl::string, eastl::vector<PathHandle>> AnimationAssetMap;

struct NodeInfo
{
	eastl::string		mName;
	int                 pParentNodeIndex;
	eastl::vector<int>	mChildNodeIndices;
	bool                mUsedInSkeleton;
	cgltf_node*			pNode;
};

struct BoneInfo
{
	int                                          mNodeIndex;
	int                                          mParentNodeIndex;
	ozz::animation::offline::RawSkeleton::Joint* pParentJoint;
};

bool AssetPipeline::ProcessAnimations(const Path* animationDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings)
{
	// Check if animationDirectory exists
	if (!fsFileExists(animationDirectory))
	{
		LOGF(LogLevel::eERROR, "AnimationDirectory: \"%s\" does not exist.", animationDirectory);
		return false;
	}

	// Check if output directory exists
	bool outputDirExists = fsFileExists(outputDirectory);

	// Check for assets containing animations in animationDirectory
    AnimationAssetMap                animationAssets;
    
    eastl::vector<PathHandle> subDirectories = fsGetSubDirectories(animationDirectory);
    for (const PathHandle& subDir : subDirectories)
    {
        // Get all glTF files
        eastl::vector<PathHandle> filesInDirectory = fsGetFilesWithExtension(subDir, "gltf");

        for (const PathHandle& file : filesInDirectory)
        {
            eastl::string fileName = fsPathComponentToString(fsGetPathFileName(file));
            fileName.make_lower();
            if (fileName == "riggedmesh")
            {
                // Add new animation asset named after the folder it is in
                eastl::string assetName = fsPathComponentToString(fsGetPathFileName(subDir));
                animationAssets[assetName].push_back(file);

                // Find sub directory called animations
                eastl::vector<PathHandle> assetSubDirectories = fsGetSubDirectories(subDir);
                for (const PathHandle& assetSubDir : assetSubDirectories)
                {
                    eastl::string subDir = fsPathComponentToString(fsGetPathFileName(assetSubDir));
                    subDir.make_lower();
                    if (subDir == "animations")
                    {
                        // Add all files in animations to the animation asset
                        filesInDirectory.clear();
                        filesInDirectory = fsGetFilesWithExtension(assetSubDir, "gltf");
                        animationAssets[assetName].insert(
                            animationAssets[assetName].end(), filesInDirectory.begin(), filesInDirectory.end());
                        break;
                    }
                }

                break;
            }
        }
    }

    // Do some checks
    if (!settings->quiet)
    {
        if (animationAssets.empty())
            LOGF(LogLevel::eWARNING, "%s does not contain any animation files.", fsGetPathAsNativeString(animationDirectory));

        for (AnimationAssetMap::iterator it = animationAssets.begin(); it != animationAssets.end(); ++it)
        {
            if (it->second.size() == 1)
                LOGF(LogLevel::eWARNING, "No animations found for rigged mesh %s.", it->first.c_str());
        }
    }

    // No assets found. Return.
    if (animationAssets.empty())
        return true;

    // Process the found assets
    int  assetsProcessed = 0;
    bool success = true;
    for (AnimationAssetMap::iterator it = animationAssets.begin(); it != animationAssets.end(); ++it)
    {
        const Path* skinnedMesh = it->second[0];

        // Create skeleton output file name
        PathHandle skeletonOutputDir = fsAppendPathComponent(outputDirectory, it->first.c_str());
        PathHandle skeletonOutput = fsAppendPathComponent(skeletonOutputDir, "skeleton.ozz");

        // Check if the skeleton is already up-to-date
        bool processSkeleton = true;
        if (!settings->force && outputDirExists)
        {
            time_t lastModified = fsGetLastModifiedTime(skinnedMesh);
            time_t lastProcessed = fsGetLastModifiedTime(skeletonOutput);

            if (lastModified < lastProcessed && lastProcessed != ~0u && lastProcessed > settings->minLastModifiedTime)
                processSkeleton = false;
        }

        ozz::animation::Skeleton skeleton;
        if (processSkeleton)
        {
            // If output directory doesn't exist, create it.
            if (!outputDirExists)
            {
                if (!fsCreateDirectory(outputDirectory))
                {
                    LOGF(LogLevel::eERROR, "Failed to create output directory %s.", fsGetPathAsNativeString(outputDirectory));
                    return false;
                }
                outputDirExists = true;
            }

            if (!fsFileExists(skeletonOutputDir))
            {
                if (!fsCreateDirectory(skeletonOutputDir))
                {
                    LOGF(LogLevel::eERROR, "Failed to create output directory %s.", fsGetPathAsNativeString(skeletonOutputDir));
                    success = false;
                    continue;
                }
            }

            // Process the skeleton
            if (!CreateRuntimeSkeleton(skinnedMesh, it->first.c_str(), skeletonOutput, &skeleton, settings))
            {
                success = false;
                continue;
            }

            ++assetsProcessed;
        }
        else
        {
			// Load skeleton from disk
			ozz::io::File file(skeletonOutput, FM_READ_BINARY);

			if (!file.opened())
				return false;
			ozz::io::IArchive archive(&file);
			archive >> skeleton;
			if (!file.CloseOzzFile())
				return false;
        }

        // If output directory doesn't exist, create it.
        PathHandle animationOutputDir = fsAppendPathComponent(skeletonOutputDir, "animations");
        if (!fsFileExists(animationOutputDir))
        {
            if (!fsCreateDirectory(animationOutputDir))
            {
                LOGF(LogLevel::eERROR, "Failed to create output directory %s.", fsGetPathAsNativeString(animationOutputDir));
                success = false;
                skeleton.Deallocate();
                continue;
            }
        }

        // Process animations
        for (size_t i = 1; i < it->second.size(); ++i)
        {
            const PathHandle& animationFile = it->second[i];
            
            eastl::string animationName = fsPathComponentToString(fsGetPathFileName(animationFile));

            // Create animation output file name
            const eastl::string outputFileString = it->first + "/animations/" + animationName + ".ozz";
            const PathHandle animationOutput = fsAppendPathComponent(outputDirectory, outputFileString.c_str());

            // Check if the animation is already up-to-date
            bool processAnimation = true;
            if (!settings->force && outputDirExists && !processSkeleton)
            {
                time_t lastModified = fsGetLastModifiedTime(animationFile);
                time_t lastProcessed = fsGetLastModifiedTime(animationOutput);

                if (lastModified < lastProcessed && lastProcessed != ~0u && lastModified > settings->minLastModifiedTime)
                    processAnimation = false;
            }

            if (processAnimation)
            {
                // Process the animation
                if (!CreateRuntimeAnimation(
                    animationFile, &skeleton, it->first.c_str(), animationName.c_str(), animationOutput, settings))
                    continue;

                ++assetsProcessed;
            }
        }

        skeleton.Deallocate();
    }

    if (!settings->quiet && assetsProcessed == 0 && success)
        LOGF(LogLevel::eINFO, "All assets already up-to-date.");

    return success;
}

bool AssetPipeline::ProcessTextures(const Path* textureDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings)
{
	// Check if directory exists
	if (!fsFileExists(textureDirectory))
	{
		LOGF(LogLevel::eERROR, "textureDirectory: \"%s\" does not exist.", textureDirectory);
		return false;
	}

	// If output directory doesn't exist, create it.
	if (!fsFileExists(outputDirectory))
	{
		if (!fsCreateDirectory(outputDirectory))
		{
			LOGF(LogLevel::eERROR, "Failed to create output directory %s.", outputDirectory);
			return false;
		}
	}

    PathHandle currentDir = fsCopyWorkingDirectoryPath();
	currentDir = fsAppendPathComponent(currentDir, "ImageConvertTools/ImageConvertTool.py");
	eastl::string cmd("python ");
	cmd.append("\"");
	cmd.append(fsGetPathAsNativeString(currentDir));
	cmd.append("\"");
	cmd.append(" fixall ");
	eastl::string inputStr(fsGetPathAsNativeString(textureDirectory));
	cmd.append(inputStr);
	cmd.append(" ");

	eastl::string outputStr(fsGetPathAsNativeString(outputDirectory));
	cmd.append(outputStr);

#if !defined(TARGET_IOS) && !defined(__ANDROID__) && !defined(_DURANGO) && !defined(ORBIS)
	int result = system(cmd.c_str());
	if (result)
		return true;
	else
		return false;
#else
    return true;
#endif
}

bool AssetPipeline::ProcessVirtualTextures(const Path* textureDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings)
{
#if !defined(__linux__) && !defined(METAL)
	// Check if directory exists
	if (!fsFileExists(textureDirectory))
	{
		LOGF(LogLevel::eERROR, "textureDirectory: \"%s\" does not exist.", textureDirectory);
		return false;
	}

	// If output directory doesn't exist, create it.
	if (!fsFileExists(outputDirectory))
	{
		if (!fsCreateDirectory(outputDirectory))
		{
			LOGF(LogLevel::eERROR, "Failed to create output directory %s.", outputDirectory);
			return false;
		}
	}

	// Get all image files
	eastl::vector<PathHandle> ddsFilesInDirectory;
	ddsFilesInDirectory = fsGetFilesWithExtension(textureDirectory, ".dds");

	for (size_t i = 0; i < ddsFilesInDirectory.size(); ++i)
	{
		eastl::string outputFile = fsGetPathAsNativeString(ddsFilesInDirectory[i]);
		
		if (outputFile.size() > 0)
		{
			Image* pImage = conf_new(Image);
			pImage->Init();

			if (!pImage->LoadFromFile(ddsFilesInDirectory[i], NULL, NULL))
			{
				pImage->Destroy();
				conf_delete(pImage);
				LOGF(LogLevel::eERROR, "Failed to load image %s.", outputFile.c_str());
				continue;
			}

			outputFile.resize(outputFile.size() - 4);
			outputFile.append(".svt");

			PathHandle pathForSVT = fsCreatePath(fsGetSystemFileSystem(), outputFile.c_str());

			bool result = pImage->iSaveSVT(pathForSVT);

			pImage->Destroy();
			conf_delete(pImage);

			if (result == false)
			{
				LOGF(LogLevel::eERROR, "Failed to save sparse virtual texture %s.", outputFile.c_str());
				return false;
			}			
		}
	}

	ddsFilesInDirectory.set_capacity(0);

	eastl::vector<PathHandle> ktxFilesInDirectory;
	ktxFilesInDirectory = fsGetFilesWithExtension(textureDirectory, ".ktx");

	for (size_t i = 0; i < ktxFilesInDirectory.size(); ++i)
	{
		eastl::string outputFile = fsGetPathAsNativeString(ktxFilesInDirectory[i]);

		if (outputFile.size() > 0)
		{
			Image* pImage = conf_new(Image);
			pImage->Init();

			if (!pImage->LoadFromFile(ktxFilesInDirectory[i], NULL, NULL))
			{
				pImage->Destroy();
				conf_delete(pImage);
				LOGF(LogLevel::eERROR, "Failed to load image %s.", outputFile.c_str());
				continue;
			}

			outputFile.resize(outputFile.size() - 4);
			outputFile.append(".svt");

			PathHandle pathForSVT = fsCreatePath(fsGetSystemFileSystem(), outputFile.c_str());

			bool result = pImage->iSaveSVT(pathForSVT);

			pImage->Destroy();
			conf_delete(pImage);

			if (result == false)
			{
				LOGF(LogLevel::eERROR, "Failed to save sparse virtual texture %s.", outputFile.c_str());
				return false;
			}
		}
	}

	ktxFilesInDirectory.set_capacity(0);
#endif
	return true;
}

bool AssetPipeline::ProcessTFX(const Path* textureDirectory, const Path* outputDirectory, ProcessAssetsSettings* settings)
{
	// Check if directory exists
	if (!fsFileExists(textureDirectory))
	{
		LOGF(LogLevel::eERROR, "textureDirectory: \"%s\" does not exist.", textureDirectory);
		return false;
	}

	// If output directory doesn't exist, create it.
	if (!fsFileExists(outputDirectory))
	{
		if (!fsCreateDirectory(outputDirectory))
		{
			LOGF(LogLevel::eERROR, "Failed to create output directory %s.", outputDirectory);
			return false;
		}
	}

	cgltf_result result = cgltf_result_success;

	// Get all tfx files
	eastl::vector<PathHandle> tfxFilesInDirectory;
	tfxFilesInDirectory = fsGetFilesWithExtension(textureDirectory, ".tfx");

#define RETURN_IF_TFX_ERROR(expression) if (!(expression)) { LOGF(eERROR, "Failed to load tfx"); return false; }

	for (size_t i = 0; i < tfxFilesInDirectory.size(); ++i)
	{
		PathHandle input = tfxFilesInDirectory[i];
		PathHandle output = fsAppendPathComponent(outputDirectory, fsGetPathFileName(input).buffer);
		output = fsReplacePathExtension(output, "gltf");
		PathHandle binFilePath = fsReplacePathExtension(output, "bin");

		FileStream* tfxFile = fsOpenFile(input, FM_READ_BINARY);
		AMD::TressFXAsset tressFXAsset = {};
		RETURN_IF_TFX_ERROR(tressFXAsset.LoadHairData(tfxFile))
		fsCloseStream(tfxFile);

		if (settings->mFollowHairCount)
		{
			RETURN_IF_TFX_ERROR(tressFXAsset.GenerateFollowHairs(settings->mFollowHairCount, settings->mTipSeperationFactor, settings->mMaxRadiusAroundGuideHair))
		}

		RETURN_IF_TFX_ERROR(tressFXAsset.ProcessAsset())

		struct TypePair { cgltf_type type; cgltf_component_type comp; };
		const TypePair vertexTypes[] =
		{
			{ cgltf_type_scalar, cgltf_component_type_r_32u },   // Indices
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Position
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Tangents
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Global rotations
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Local rotations
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Ref vectors
			{ cgltf_type_vec4,   cgltf_component_type_r_32f },   // Follow root offsets
			{ cgltf_type_vec2,   cgltf_component_type_r_32f },   // Strand UVs
			{ cgltf_type_scalar, cgltf_component_type_r_32u },   // Strand types
			{ cgltf_type_scalar, cgltf_component_type_r_32f },   // Thickness coeffs
			{ cgltf_type_scalar, cgltf_component_type_r_32f },   // Rest lengths
		};
		const uint32_t vertexStrides[] =
		{
			sizeof(uint32_t), // Indices
			sizeof(float4),   // Position
			sizeof(float4),   // Tangents
			sizeof(float4),   // Global rotations
			sizeof(float4),   // Local rotations
			sizeof(float4),   // Ref vectors
			sizeof(float4),   // Follow root offsets
			sizeof(float2),   // Strand UVs
			sizeof(uint32_t), // Strand types
			sizeof(float),    // Thickness coeffs
			sizeof(float),    // Rest lengths
		};
		const uint32_t vertexCounts[] =
		{
			(uint32_t)tressFXAsset.GetNumHairTriangleIndices(),   // Indices
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Position
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Tangents
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Global rotations
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Local rotations
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Ref vectors
			(uint32_t)tressFXAsset.m_numTotalStrands,    // Follow root offsets
			(uint32_t)tressFXAsset.m_numTotalStrands,    // Strand UVs
			(uint32_t)tressFXAsset.m_numTotalStrands,    // Strand types
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Thickness coeffs
			(uint32_t)tressFXAsset.m_numTotalVertices,   // Rest lengths
		};
		const void* vertexData[] =
		{
			tressFXAsset.m_triangleIndices,    // Indices
			tressFXAsset.m_positions,          // Position
			tressFXAsset.m_tangents,           // Tangents
			tressFXAsset.m_globalRotations,    // Global rotations
			tressFXAsset.m_localRotations,     // Local rotations
			tressFXAsset.m_refVectors,         // Ref vectors
			tressFXAsset.m_followRootOffsets,  // Follow root offsets
			tressFXAsset.m_strandUV,           // Strand UVs
			tressFXAsset.m_strandTypes,        // Strand types
			tressFXAsset.m_thicknessCoeffs,    // Thickness coeffs
			tressFXAsset.m_restLengths,        // Rest lengths
		};
		const char* vertexNames[] =
		{
			"INDEX",             // Indices
			"POSITION",          // Position
			"TANGENT",           // Tangents
			"TEXCOORD_0",        // Global rotations
			"TEXCOORD_1",        // Local rotations
			"TEXCOORD_2",        // Ref vectors
			"TEXCOORD_3",        // Follow root offsets
			"TEXCOORD_4",        // Strand UVs
			"TEXCOORD_5",        // Strand types
			"TEXCOORD_6",        // Thickness coeffs
			"TEXCOORD_7",        // Rest lengths
		};
		const uint32_t count = sizeof(vertexData) / sizeof(vertexData[0]);

		cgltf_buffer buffer = {};
		cgltf_accessor accessors[count] = {};
		cgltf_buffer_view views[count] = {};
		cgltf_attribute attribs[count] = {};
		cgltf_mesh mesh = {};
		cgltf_primitive prim = {};
		cgltf_size offset = 0;
		FileStream* binFile = fsOpenFile(binFilePath, FM_WRITE_BINARY);
		size_t fileSize = 0;

		for (uint32_t i = 0; i < count; ++i)
		{
			views[i].type = (i ? cgltf_buffer_view_type_vertices : cgltf_buffer_view_type_indices);
			views[i].buffer = &buffer;
			views[i].offset = offset;
			views[i].size = vertexCounts[i] * vertexStrides[i];
			accessors[i].component_type = vertexTypes[i].comp;
			accessors[i].stride = vertexStrides[i];
			accessors[i].count = vertexCounts[i];
			accessors[i].offset = 0;
			accessors[i].type = vertexTypes[i].type;
			accessors[i].buffer_view = &views[i];

			attribs[i].name = (char*)vertexNames[i];
			attribs[i].data = &accessors[i];

			fileSize += fsWriteToStream(binFile, vertexData[i], views[i].size);
			offset += views[i].size;
		}
		fsCloseStream(binFile);

		PathComponent fn = fsGetPathFileName(binFilePath);
		char uri[2048] = {};
		sprintf(uri, "%s", fn.buffer);
		buffer.uri = uri;
		buffer.size = fileSize;

		prim.indices = accessors;
		prim.attributes_count = count - 1;
		prim.attributes = attribs + 1;
		prim.type = cgltf_primitive_type_triangles;

		mesh.primitives_count = 1;
		mesh.primitives = &prim;

		char extras[128] = {};
		sprintf(extras, "{ \"%s\" : %d, \"%s\" : %d }",
			"mVertexCountPerStrand", tressFXAsset.m_numVerticesPerStrand, "mGuideCountPerStrand", tressFXAsset.m_numGuideStrands);

		char generator[] = "TressFX";
		cgltf_data data = {};
		data.asset.generator = generator;
		data.buffers_count = 1;
		data.buffers = &buffer;
		data.buffer_views_count = count;
		data.buffer_views = views;
		data.accessors_count = count;
		data.accessors = accessors;
		data.meshes_count = 1;
		data.meshes = &mesh;
		data.file_data = extras;
		data.asset.extras.start_offset = 0;
		data.asset.extras.end_offset = strlen(extras);
		cgltf_options options = {};
		result = cgltf_write_file(&options, fsGetPathAsNativeString(output), &data);
	}

	return result == cgltf_result_success;
}

static uint32_t FindJoint(ozz::animation::Skeleton* skeleton, const char* name)
{
	for (int i = 0; i < skeleton->num_joints(); i++)
	{
		if (strcmp(skeleton->joint_names()[i], name) == 0)
			return i;
	}
	return UINT_MAX;
}

bool AssetPipeline::CreateRuntimeSkeleton(
	const Path* skeletonAsset, const char* skeletonName, const Path* skeletonOutput, ozz::animation::Skeleton* skeleton,
	ProcessAssetsSettings* settings)
{
	// Import the glTF with the animation
	cgltf_data* data = NULL;
	cgltf_options options = {};
	options.memory_alloc = [](void* user, cgltf_size size) { return conf_malloc(size); };
	options.memory_free = [](void* user, void* ptr) { conf_free(ptr); };
	cgltf_result result = cgltf_parse_file(&options, fsGetPathAsNativeString(skeletonAsset), &data);
	if (result != cgltf_result_success)
		return false;

	cgltf_load_buffers(&options, data, fsGetPathAsNativeString(skeletonAsset));

	if (data->skins_count == 0)
	{
		LOGF(LogLevel::eERROR, "Rigged mesh %s has no bones. Skeleton can not be created.", skeletonName);
		return false;
	}

	// Gather node info
	// Used to mark nodes that should be included in the skeleton
	eastl::vector<NodeInfo> nodeData(1);
	nodeData[0] = { data->nodes[0].name, -1, {}, false, &data->nodes[0] };

	const int queueSize = 128;
	int       nodeQueue[queueSize] = {};    // Simple queue because tinystl doesn't have one
	for (int i = 0; i < queueSize; ++i)
		nodeQueue[i] = -1;
	nodeQueue[0] = 0;
	int nodeQueueStart = 0;
	int nodeQueueEnd = 1;
	while (nodeQueue[nodeQueueStart] != -1)
	{
		// Pop
		int     nodeIndex = nodeQueue[nodeQueueStart];
		cgltf_node *node = nodeData[nodeIndex].pNode;
		nodeQueue[nodeQueueStart] = -1;

		for (uint i = 0; i < node->children_count; ++i)
		{
			NodeInfo childNode = {};
			childNode.mName = node->children[i]->name;
			childNode.pParentNodeIndex = nodeIndex;
			int childNodeIndex = (int)nodeData.size();
			childNode.pNode = node->children[i];
			nodeData.push_back(childNode);

			nodeData[nodeIndex].mChildNodeIndices.push_back(childNodeIndex);

			// Push
			nodeQueue[nodeQueueEnd] = childNodeIndex;
			nodeQueueEnd = (nodeQueueEnd + 1) % queueSize;
			if (nodeQueueStart == nodeQueueEnd)
			{
				LOGF(LogLevel::eERROR, "Too many nodes in scene. Skeleton can not be created.", skeletonName);
				return false;
			}
		}

		nodeQueueStart = (nodeQueueStart + 1) % queueSize;
	}

	// Mark all nodes that are required to be in the skeleton
	for (uint i = 0; i < data->skins_count; ++i)
	{
		cgltf_skin* skin = &data->skins[i];
		for (uint j = 0; j < skin->joints_count; ++j)
		{
			cgltf_node* bone = skin->joints[j];
			for (uint k = 0; k < (uint)nodeData.size(); ++k)
			{
				if (nodeData[k].mName == eastl::string(bone->name))
				{
					int nodeIndex = k;
					while (nodeIndex != -1)
					{
						if (nodeData[nodeIndex].mUsedInSkeleton)
							break;    // Remaining part of the tree is already marked
						nodeData[nodeIndex].mUsedInSkeleton = true;
						nodeIndex = nodeData[nodeIndex].pParentNodeIndex;
					}
				}
			}
		}
	}

	// Create raw skeleton
	ozz::animation::offline::RawSkeleton rawSkeleton;

	eastl::vector<BoneInfo> boneData(1);
	boneData[0] = { 0, -1, NULL };

	while (!boneData.empty())
	{
		BoneInfo boneInfo = boneData.back();
		boneData.pop_back();
		NodeInfo* nodeInfo = &nodeData[boneInfo.mNodeIndex];
		cgltf_node*   node = nodeInfo->pNode;

		// Get node transform

		// Create joint from node
		ozz::animation::offline::RawSkeleton::Joint joint;
		joint.transform.translation = vec3(node->translation[0], node->translation[1], node->translation[2]);
		joint.transform.rotation = Quat(node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]);
		joint.transform.scale = vec3(node->scale[0], node->scale[1], node->scale[2]) * (boneInfo.mParentNodeIndex == -1 ? 0.01f : 1.f);
		joint.name = nodeInfo->mName.c_str();

		// Add node to raw skeleton
		ozz::animation::offline::RawSkeleton::Joint* newParentJoint = NULL;
		if (boneInfo.pParentJoint == NULL)
		{
			rawSkeleton.roots.push_back(joint);
			newParentJoint = &rawSkeleton.roots.back();
		}
		else
		{
			boneInfo.pParentJoint->children.push_back(joint);
			newParentJoint = &boneInfo.pParentJoint->children.back();
		}

		// Count the child nodes that are required to be in the skeleton
		int requiredChildCount = 0;
		for (uint i = 0; i < (uint)nodeInfo->mChildNodeIndices.size(); ++i)
		{
			NodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
			if (childNodeInfo->mUsedInSkeleton)
				++requiredChildCount;
		}

		// Add child nodes to the list of nodes to process
		newParentJoint->children.reserve(requiredChildCount);    // Reserve to make sure memory isn't reallocated later.
		for (uint i = 0; i < (uint)nodeInfo->mChildNodeIndices.size(); ++i)
		{
			NodeInfo* childNodeInfo = &nodeData[nodeInfo->mChildNodeIndices[i]];
			if (childNodeInfo->mUsedInSkeleton)
			{
				boneInfo.mNodeIndex = nodeInfo->mChildNodeIndices[i];
				boneInfo.mParentNodeIndex = boneInfo.mNodeIndex;
				boneInfo.pParentJoint = newParentJoint;
				boneData.push_back(boneInfo);
			}
		}
	}

	// Validate raw skeleton
	if (!rawSkeleton.Validate())
	{
		LOGF(LogLevel::eERROR, "Skeleton created for %s is invalid. Skeleton can not be created.", skeletonName);
		return false;
	}

	// Build runtime skeleton from raw skeleton
	if (!ozz::animation::offline::SkeletonBuilder::Build(rawSkeleton, skeleton))
	{
		LOGF(LogLevel::eERROR, "Skeleton can not be created for %s.", skeletonName);
		return false;
	}

	// Write skeleton to disk
	ozz::io::File     file(skeletonOutput, FM_WRITE_BINARY);
	if (!file.opened())
		return false;

	ozz::io::OArchive archive(&file);
	archive << *skeleton;
	if (!file.CloseOzzFile())
		return false;

	void* fileData = data->file_data;

	// Generate joint remaps in the gltf
	{
		eastl::vector<char> buffer;
		uint32_t size = 0;

		for (uint32_t i = 0; i < data->meshes_count; ++i)
		{
			cgltf_mesh* mesh = &data->meshes[i];
			cgltf_node* node = NULL;
			for (uint32_t n = 0; n < data->nodes_count; ++n)
			{
				if (data->nodes[n].mesh == mesh)
				{
					node = &data->nodes[n];
					break;
				}
			}

			if (node && node->mesh && node->skin)
			{
				cgltf_skin* skin = node->skin;
				cgltf_mesh* mesh = node->mesh;

				char jointRemaps[1024] = {};
				uint32_t offset = 0;
				offset += sprintf(jointRemaps + offset, "[ ");

				for (uint32_t j = 0; j < skin->joints_count; ++j)
				{
					const cgltf_node* jointNode = skin->joints[j];
					uint32_t jointIndex = FindJoint(skeleton, jointNode->name);
					offset += sprintf(jointRemaps + offset, "%u, ", jointIndex);
				}

				offset += sprintf(jointRemaps + offset, "],");
				skin->extras.start_offset = size;
				size += (uint32_t)strlen(jointRemaps) + 1;
				skin->extras.end_offset = size - 1;

				for (uint32_t i = 0; i < strlen(jointRemaps) + 1; ++i)
					buffer.push_back(jointRemaps[i]);
			}
		}

		for (uint32_t i = 0; i < data->buffers_count; ++i)
		{
			if (strstr(data->buffers[i].uri, "://") == NULL)
			{
				PathHandle parent = fsCopyParentPath(skeletonAsset);
				PathHandle bufferOut = fsAppendPathComponent(parent, data->buffers[i].uri);
				FileStream* fs = fsOpenFile(bufferOut, FM_WRITE_BINARY);
				fsWriteToStream(fs, data->buffers[i].data, data->buffers[i].size);
				fsCloseStream(fs);
			}
		}

		data->file_data = buffer.data();
		cgltf_write_file(&options, fsGetPathAsNativeString(skeletonAsset), data);
	}

	data->file_data = fileData;
	cgltf_free(data);

	return true;
}

bool AssetPipeline::CreateRuntimeAnimation(
	const Path* animationAsset, ozz::animation::Skeleton* skeleton, const char* skeletonName, const char* animationName,
	const Path* animationOutput, ProcessAssetsSettings* settings)
{
	// Import the glTF with the animation
	cgltf_data* data = NULL;
	cgltf_options options = {};
	options.memory_alloc = [](void* user, cgltf_size size) { return conf_malloc(size); };
	options.memory_free = [](void* user, void* ptr) { conf_free(ptr); };
	cgltf_result result = cgltf_parse_file(&options, fsGetPathAsNativeString(animationAsset), &data);
	if (result != cgltf_result_success)
		return false;

	cgltf_load_buffers(&options, data, fsGetPathAsNativeString(animationAsset));

	// Check if the asset contains any animations
	if (data->animations_count == 0)
	{
		if (!settings->quiet)
			LOGF(LogLevel::eWARNING, "Animation asset %s of skeleton %s contains no animations.", animationName, skeletonName);
		return false;
	}

	// Create raw animation
	ozz::animation::offline::RawAnimation rawAnimation;
	rawAnimation.name = animationName;
    rawAnimation.duration = 0.f;

	bool rootFound = false;
	rawAnimation.tracks.resize(skeleton->num_joints());
	for (uint i = 0; i < (uint)skeleton->num_joints(); ++i)
	{
		const char* jointName = skeleton->joint_names()[i];

		ozz::animation::offline::RawAnimation::JointTrack* track = &rawAnimation.tracks[i];

        for (cgltf_size animationIndex = 0; animationIndex < data->animations_count; animationIndex += 1)
        {
            cgltf_animation* animationData = &data->animations[animationIndex];
        
            for (cgltf_size channelIndex = 0; channelIndex < animationData->channels_count; channelIndex += 1)
            {
                cgltf_animation_channel* channel = &animationData->channels[channelIndex];

                if (strcmp(channel->target_node->name, jointName) != 0) continue;
                
                if (channel->target_path == cgltf_animation_path_type_translation)
                {
                    track->translations.resize(channel->sampler->output->count);
                    for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
                    {
                        float time = 0.0;
                        vec3 translation = vec3(0.0f);
                        cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
                        cgltf_accessor_read_float(channel->sampler->output, j, (float*)&translation, 3);

                        track->translations[j] = { time, translation };
                    }
                }

                if (channel->target_path == cgltf_animation_path_type_rotation)
                {
                    track->rotations.resize(channel->sampler->output->count);
                    for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
                    {
                        float time = 0.0;
                        Quat rotation = Quat(0.0f);
                        cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
                        cgltf_accessor_read_float(channel->sampler->output, j, (float*)&rotation, 4);

                        track->rotations[j] = { time, rotation };
                    }
                }

                if (channel->target_path == cgltf_animation_path_type_scale)
                {
                    track->scales.resize(channel->sampler->output->count);
                    for (cgltf_size j = 0; j < channel->sampler->output->count; j += 1)
                    {
                        float time = 0.0;
                        vec3 scale = vec3(0.0f);
                        cgltf_accessor_read_float(channel->sampler->input, j, &time, 1);
                        cgltf_accessor_read_float(channel->sampler->output, j, (float*)&scale, 3);

                        track->scales[j] = { time, scale };
                    }
                }
            }
        }

		if (!rootFound)
		{
			// Scale root of animation from centimeters to meters
            if (track->scales.empty())
            {
                track->scales.resize(1, { 0.f, Vector3(1.0f) });
            }
            
			for (uint j = 0; j < (uint)track->translations.size(); ++j)
				track->translations[j].value *= 0.01f;

			for (uint j = 0; j < track->scales.size(); ++j)
				track->scales[j].value *= 0.01f;

			rootFound = true;
		}
        
        if (!track->translations.empty())
            rawAnimation.duration = max(rawAnimation.duration, track->translations.back().time);
        
        if (!track->rotations.empty())
            rawAnimation.duration = max(rawAnimation.duration, track->rotations.back().time);
        
        if (!track->scales.empty())
            rawAnimation.duration = max(rawAnimation.duration, track->scales.back().time);
	}

	cgltf_free(data);

	// Validate raw animation
	if (!rawAnimation.Validate())
	{
		LOGF(LogLevel::eERROR, "Animation %s created for %s is invalid. Animation can not be created.", animationName, skeletonName);
		return false;
	}

	// Build runtime animation from raw animation
	ozz::animation::Animation animation;
	if (!ozz::animation::offline::AnimationBuilder::Build(rawAnimation, &animation))
	{
		LOGF(LogLevel::eERROR, "Animation %s can not be created for %s.", animationName, skeletonName);
		return false;
	}

	// Write animation to disk
	ozz::io::File     file(animationOutput, FM_WRITE_BINARY);
	if (!file.opened())
		return false;

	ozz::io::OArchive archive(&file);
	archive << animation;
	if (!file.CloseOzzFile())
		return false;
	//Deallocate animation
	animation.Deallocate();

	return true;
}
