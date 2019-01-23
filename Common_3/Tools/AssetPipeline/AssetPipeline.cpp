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

#include "AssetPipeline.h"
#include "AssetLoader.h"

// Tiny stl
#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"

// Assimp
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/Importer.hpp"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/Exporter.hpp"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/scene.h"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/metadata.h"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/config.h"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/cimport.h"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/postprocess.h"
#include "../../ThirdParty/OpenSource/assimp/4.1.0/include/assimp/DefaultLogger.hpp"

// OZZ
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/stream.h"
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/base/io/archive.h"
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_skeleton.h"
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/raw_animation.h"
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/skeleton_builder.h"
#include "../../ThirdParty/OpenSource/ozz-animation/include/ozz/animation/offline/animation_builder.h"

#include "../../OS/Interfaces/IOperatingSystem.h"
#include "../../OS/Interfaces/IFileSystem.h"
#include "../../OS/Interfaces/ILogManager.h"
#include "../../OS/Interfaces/IMemoryManager.h"    //NOTE: this should be the last include in a .cpp

typedef tinystl::unordered_map<tinystl::string, tinystl::vector<tinystl::string>> AnimationAssetMap;

struct NodeInfo
{
	tinystl::string      mName;
	int                  pParentNodeIndex;
	tinystl::vector<int> mChildNodeIndices;
	bool                 mUsedInSkeleton;
	aiNode*              pNode;
};

struct BoneInfo
{
	int                                          mNodeIndex;
	int                                          mParentNodeIndex;
	ozz::animation::offline::RawSkeleton::Joint* pParentJoint;
};

bool ImportFBX(const char* fbxFile, const aiScene** pScene)
{
	// Set up assimp to be able to parse our fbx files correctly
	aiPropertyStore* propertyStore = aiCreatePropertyStore();

	// Tell Assimp to not import a bunch of useless layers of objects
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_IMPORT_FBX_READ_ALL_GEOMETRY_LAYERS, 0);
	aiSetImportPropertyInteger(propertyStore, AI_CONFIG_IMPORT_FBX_READ_TEXTURES, 1);
	aiSetImportPropertyFloat(propertyStore, AI_CONFIG_PP_GSN_MAX_SMOOTHING_ANGLE, 66.0f);

	unsigned int flags = aiProcessPreset_TargetRealtime_MaxQuality;
	flags |= aiProcess_FixInfacingNormals | aiProcess_ConvertToLeftHanded;
	flags &= ~aiProcess_SortByPType;
	flags &= ~aiProcess_FindInstances;

	// Import an assimp scene
	*pScene = aiImportFileExWithProperties(fbxFile, flags, nullptr, propertyStore);

	// If the import failed, report it
	if (*pScene == NULL)
	{
		LOGERRORF("%s", aiGetErrorString());
		return false;
	}

	return true;
}

bool AssetPipeline::ProcessAnimations(const char* animationDirectory, const char* outputDirectory, ProcessAssetsSettings* settings)
{
	// Check if animationDirectory exists
	if (!FileSystem::DirExists(animationDirectory))
	{
		LOGERRORF("AnimationDirectory: \"%s\" does not exist.", animationDirectory);
		return false;
	}

	// Check if output directory exists
	tinystl::string outputDir = FileSystem::AddTrailingSlash(outputDirectory);
	bool            outputDirExists = FileSystem::DirExists(outputDir);

	// Check for assets containing animations in animationDirectory
	AnimationAssetMap                animationAssets;
	tinystl::vector<tinystl::string> subDirectories = {};
	FileSystem::GetSubDirectories(animationDirectory, subDirectories);
	for (const tinystl::string& subDir : subDirectories)
	{
		// Get all fbx files
		tinystl::vector<tinystl::string> filesInDirectory;
		FileSystem::GetFilesWithExtension(subDir, ".fbx", filesInDirectory);

		for (const tinystl::string& file : filesInDirectory)
		{
			if (FileSystem::GetFileName(file).to_lower() == "riggedmesh")
			{
				// Add new animation asset named after the folder it is in
				tinystl::string assetName = FileSystem::GetFileName(subDir);
				animationAssets[assetName].push_back(file);

				// Find sub directory called animations
				tinystl::vector<tinystl::string> assetSubDirectories;
				FileSystem::GetSubDirectories(subDir, assetSubDirectories);
				for (const tinystl::string& assetSubDir : assetSubDirectories)
				{
					if (FileSystem::GetFileName(assetSubDir).to_lower() == "animations")
					{
						// Add all files in animations to the animation asset
						filesInDirectory.clear();
						FileSystem::GetFilesWithExtension(assetSubDir, ".fbx", filesInDirectory);
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
			LOGWARNINGF("%s does not contain any animation files.", animationDirectory);

		for (AnimationAssetMap::iterator it = animationAssets.begin(); it != animationAssets.end(); ++it)
		{
			if (it->second.size() == 1)
				LOGWARNINGF("No animations found for rigged mesh %s.", it->first.c_str());
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
		const tinystl::string& skinnedMesh = it->second[0];

		// Create skeleton output file name
		tinystl::string skeletonOutputDir = outputDir + it->first;
		tinystl::string skeletonOutput = skeletonOutputDir + "/skeleton.ozz";

		// Check if the skeleton is already up-to-date
		bool processSkeleton = true;
		if (!settings->force && outputDirExists)
		{
			uint lastModified = FileSystem::GetLastModifiedTime(skinnedMesh);
			uint lastProcessed = FileSystem::GetLastModifiedTime(skeletonOutput);

			if (lastModified < lastProcessed && lastProcessed != ~0u && lastProcessed > settings->minLastModifiedTime)
				processSkeleton = false;
		}

		ozz::animation::Skeleton skeleton;
		if (processSkeleton)
		{
			// If output directory doesn't exist, create it.
			if (!outputDirExists)
			{
				if (!FileSystem::CreateDir(outputDir))
				{
					LOGERRORF("Failed to create output directory %s.", outputDir.c_str());
					return false;
				}
				outputDirExists = true;
			}

			if (!FileSystem::DirExists(skeletonOutputDir))
			{
				if (!FileSystem::CreateDir(skeletonOutputDir))
				{
					LOGERRORF("Failed to create output directory %s.", skeletonOutputDir.c_str());
					success = false;
					continue;
				}
			}

			// Process the skeleton
			if (!CreateRuntimeSkeleton(skinnedMesh, it->first.c_str(), skeletonOutput.c_str(), &skeleton, settings))
			{
				success = false;
				continue;
			}

			++assetsProcessed;
		}
		else
		{
			if (!AssetLoader::LoadSkeleton(skeletonOutput.c_str(), FSR_Absolute, &skeleton))
				return false;
		}

		// If output directory doesn't exist, create it.
		tinystl::string animationOutputDir = skeletonOutputDir + "/animations";
		if (!FileSystem::DirExists(animationOutputDir))
		{
			if (!FileSystem::CreateDir(animationOutputDir))
			{
				LOGERRORF("Failed to create output directory %s.", animationOutputDir.c_str());
				success = false;
				skeleton.Deallocate();
				continue;
			}
		}

		// Process animations
		for (size_t i = 1; i < it->second.size(); ++i)
		{
			const tinystl::string& animationFile = it->second[i];
			;
			tinystl::string animationName = FileSystem::GetFileName(animationFile);

			// Create animation output file name
			tinystl::string animationOutput = outputDir + it->first + "/animations/" + animationName + ".ozz";

			// Check if the animation is already up-to-date
			bool processAnimation = true;
			if (!settings->force && outputDirExists && !processSkeleton)
			{
				uint lastModified = FileSystem::GetLastModifiedTime(animationFile);
				uint lastProcessed = FileSystem::GetLastModifiedTime(animationOutput);

				if (lastModified < lastProcessed && lastProcessed != ~0u && lastModified > settings->minLastModifiedTime)
					processAnimation = false;
			}

			if (processAnimation)
			{
				// Process the animation
				if (!CreateRuntimeAnimation(
						animationFile, &skeleton, it->first.c_str(), animationName.c_str(), animationOutput.c_str(), settings))
					continue;

				++assetsProcessed;
			}
		}

		skeleton.Deallocate();
	}

	if (!settings->quiet && assetsProcessed == 0 && success)
		LOGINFO("All assets already up-to-date.");

	return success;
}

bool AssetPipeline::CreateRuntimeSkeleton(
	const char* skeletonAsset, const char* skeletonName, const char* skeletonOutput, ozz::animation::Skeleton* skeleton,
	ProcessAssetsSettings* settings)
{
	// Import the FBX with the animation
	const aiScene* scene = NULL;
	if (!ImportFBX(skeletonAsset, &scene))
		return false;

	// Check if the asset contains any bones
	uint numBones = 0;
	for (size_t i = 0; i < scene->mNumMeshes; ++i)
		numBones += scene->mMeshes[i]->mNumBones;

	if (numBones == 0)
	{
		LOGERRORF("Rigged mesh %s has no bones. Skeleton can not be created.", skeletonName);
		return false;
	}

	// Gather node info
	// Used to mark nodes that should be included in the skeleton
	tinystl::vector<NodeInfo> nodeData(1);
	nodeData[0] = { scene->mRootNode->mName.C_Str(), -1, {}, false, scene->mRootNode };

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
		aiNode* node = nodeData[nodeIndex].pNode;
		nodeQueue[nodeQueueStart] = -1;

		for (uint i = 0; i < node->mNumChildren; ++i)
		{
			NodeInfo childNode = {};
			childNode.mName = node->mChildren[i]->mName.C_Str();
			childNode.pParentNodeIndex = nodeIndex;
			int childNodeIndex = (int)nodeData.size();
			childNode.pNode = node->mChildren[i];
			nodeData.push_back(childNode);

			nodeData[nodeIndex].mChildNodeIndices.push_back(childNodeIndex);

			// Push
			nodeQueue[nodeQueueEnd] = childNodeIndex;
			nodeQueueEnd = (nodeQueueEnd + 1) % queueSize;
			if (nodeQueueStart == nodeQueueEnd)
			{
				LOGERRORF("Too many nodes in scene. Skeleton can not be created.", skeletonName);
				return false;
			}
		}

		nodeQueueStart = (nodeQueueStart + 1) % queueSize;
	}

	// Mark all nodes that are required to be in the skeleton
	for (uint i = 0; i < scene->mNumMeshes; ++i)
	{
		aiMesh* mesh = scene->mMeshes[i];
		for (uint j = 0; j < mesh->mNumBones; ++j)
		{
			aiBone* bone = mesh->mBones[j];
			for (uint k = 0; k < (uint)nodeData.size(); ++k)
			{
				if (nodeData[k].mName == tinystl::string(bone->mName.C_Str()))
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

	tinystl::vector<BoneInfo> boneData(1);
	boneData[0] = { 0, -1, NULL };

	while (!boneData.empty())
	{
		BoneInfo boneInfo = boneData.back();
		boneData.pop_back();
		NodeInfo* nodeInfo = &nodeData[boneInfo.mNodeIndex];
		aiNode*   node = nodeInfo->pNode;

		// Get node transform
		aiVector3D   translation;
		aiQuaternion rotation;
		aiVector3D   scale;
		node->mTransformation.Decompose(scale, rotation, translation);

		if (boneInfo.mParentNodeIndex == -1)
		{
			// Scale the root node from centimenters to meters
			// TODO: Remove this and replace with aiProcess_GlobalScale
			// See: https://github.com/assimp/assimp/issues/775
			scale *= 0.01f;
		}

		// Create joint from node
		ozz::animation::offline::RawSkeleton::Joint joint;
		joint.transform.translation = vec3(translation.x, translation.y, translation.z);
		joint.transform.rotation = Quat(rotation.x, rotation.y, rotation.z, rotation.w);
		joint.transform.scale = vec3(scale.x, scale.y, scale.z);
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

	aiReleaseImport(scene);

	// Validate raw skeleton
	if (!rawSkeleton.Validate())
	{
		LOGERRORF("Skeleton created for %s is invalid. Skeleton can not be created.", skeletonName);
		return false;
	}

	// Build runtime skeleton from raw skeleton
	if (!ozz::animation::offline::SkeletonBuilder::Build(rawSkeleton, skeleton))
	{
		LOGERRORF("Skeleton can not be created for %s.", skeletonName);
		return false;
	}

	// Write skeleton to disk
	ozz::io::File     file(skeletonOutput, "wb");
	ozz::io::OArchive archive(&file);
	archive << *skeleton;
	file.Close();

	return true;
}

bool AssetPipeline::CreateRuntimeAnimation(
	const char* animationAsset, ozz::animation::Skeleton* skeleton, const char* skeletonName, const char* animationName,
	const char* animationOutput, ProcessAssetsSettings* settings)
{
	// Import the FBX with the animation
	const aiScene* scene = NULL;
	if (!ImportFBX(animationAsset, &scene))
		return false;

	// Check if the asset contains any animations
	if (!scene->HasAnimations())
	{
		if (!settings->quiet)
			LOGWARNINGF("Animation asset %s of skeleton %s contains no animations.", animationName, skeletonName);
		return false;
	}

	// Check if the asset contains more than 1 animation
	if (scene->mNumAnimations > 1)
	{
		if (!settings->quiet)
			LOGWARNINGF(
				"Animation asset %s of skeleton %s contains more than one animation. This is not allowed.", animationName, skeletonName);
		return false;
	}

	const aiAnimation* animationData = scene->mAnimations[0];

	// Create raw animation
	ozz::animation::offline::RawAnimation rawAnimation;
	rawAnimation.name = animationName;
	rawAnimation.duration = (float)(animationData->mDuration * (1.0 / animationData->mTicksPerSecond));

	tinystl::unordered_map<tinystl::string, aiNodeAnim*> jointMap;
	for (uint i = 0; i < animationData->mNumChannels; ++i)
	{
		aiNodeAnim* node = animationData->mChannels[i];
		jointMap[node->mNodeName.C_Str()] = node;
	}

	bool rootFound = false;
	rawAnimation.tracks.resize(skeleton->num_joints());
	for (uint i = 0; i < (uint)skeleton->num_joints(); ++i)
	{
		const char* jointName = skeleton->joint_names()[i];

		ozz::animation::offline::RawAnimation::JointTrack* track = &rawAnimation.tracks[i];

		tinystl::unordered_map<tinystl::string, aiNodeAnim*>::iterator it = jointMap.find(jointName);
		if (it != jointMap.end())
		{
			// There is a track for this bone
			aiNodeAnim* node = it->second;

			track->translations.resize(node->mNumPositionKeys);
			track->rotations.resize(node->mNumRotationKeys);
			track->scales.resize(node->mNumScalingKeys);

			for (uint j = 0; j < node->mNumPositionKeys; ++j)
			{
				aiVectorKey key = node->mPositionKeys[j];
				track->translations[j] = { min((float)(key.mTime / animationData->mTicksPerSecond), rawAnimation.duration),
										   vec3(key.mValue.x, key.mValue.y, key.mValue.z) };
			}

			for (uint j = 0; j < node->mNumRotationKeys; ++j)
			{
				aiQuatKey key = node->mRotationKeys[j];
				track->rotations[j] = { min((float)(key.mTime / animationData->mTicksPerSecond), rawAnimation.duration),
										Quat(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w) };
			}

			for (uint j = 0; j < node->mNumScalingKeys; ++j)
			{
				aiVectorKey key = node->mScalingKeys[j];
				track->scales[j] = { min((float)(key.mTime / animationData->mTicksPerSecond), rawAnimation.duration),
									 vec3(key.mValue.x, key.mValue.y, key.mValue.z) };
			}

			if (!rootFound)
			{
				// Scale root of animation from centimeters to meters
				// TODO: Remove this and replace with aiProcess_GlobalScale
				// See: https://github.com/assimp/assimp/issues/775
				for (uint j = 0; j < node->mNumPositionKeys; ++j)
					track->translations[j].value *= 0.01f;

				for (uint j = 0; j < node->mNumScalingKeys; ++j)
					track->scales[j].value *= 0.01f;

				rootFound = true;
			}
		}
	}

	aiReleaseImport(scene);

	// Validate raw animation
	if (!rawAnimation.Validate())
	{
		LOGERRORF("Animation %s created for %s is invalid. Animation can not be created.", animationName, skeletonName);
		return false;
	}

	// Build runtime animation from raw animation
	ozz::animation::Animation animation;
	if (!ozz::animation::offline::AnimationBuilder::Build(rawAnimation, &animation))
	{
		LOGERRORF("Animation %s can not be created for %s.", animationName, skeletonName);
		return false;
	}

	// Write animation to disk
	ozz::io::File     file(animationOutput, "wb");
	ozz::io::OArchive archive(&file);
	archive << animation;
	file.Close();

	return true;
}
