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

#pragma once

#include "../../ThirdParty/OpenSource/TinySTL/string.h"
#include "../../ThirdParty/OpenSource/TinySTL/vector.h"
#include "../../ThirdParty/OpenSource/TinySTL/hash.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_set.h"
#include "../../ThirdParty/OpenSource/TinySTL/unordered_map.h"
#include "../../OS/Math/MathTypes.h"
#include "../../OS/Interfaces/IOperatingSystem.h"

struct BoundingBox
{
	float3 vMin;
	float3 vMax;
};

enum TextureMapType
{
	/** Dummy value.
	*
	*  No texture, but the value to be used as 'texture semantic'
	*  (#aiMaterialProperty::mSemantic) for all material properties
	*  *not* related to textures.
	*/
	TEXTURE_MAP_NONE = 0x0,

	/** The texture is combined with the result of the diffuse
	*  lighting equation.
	*/
	TEXTURE_MAP_DIFFUSE = 0x1,

	/** The texture is combined with the result of the specular
	*  lighting equation.
	*/
	TEXTURE_MAP_SPECULAR = 0x2,

	/** The texture is combined with the result of the ambient
	*  lighting equation.
	*/
	TEXTURE_MAP_AMBIENT = 0x3,

	/** The texture is added to the result of the lighting
	*  calculation. It isn't influenced by incoming light.
	*/
	TEXTURE_MAP_EMISSIVE = 0x4,

	/** The texture is a height map.
	*
	*  By convention, higher gray-scale values stand for
	*  higher elevations from the base height.
	*/
	TEXTURE_MAP_HEIGHT = 0x5,

	/** The texture is a (tangent space) normal-map.
	*
	*  Again, there are several conventions for tangent-space
	*  normal maps. Assimp does (intentionally) not
	*  distinguish here.
	*/
	TEXTURE_MAP_NORMALS = 0x6,

	/** The texture defines the glossiness of the material.
	*
	*  The glossiness is in fact the exponent of the specular
	*  (phong) lighting equation. Usually there is a conversion
	*  function defined to map the linear color values in the
	*  texture to a suitable exponent. Have fun.
	*/
	TEXTURE_MAP_SHININESS = 0x7,

	/** The texture defines per-pixel opacity.
	*
	*  Usually 'white' means opaque and 'black' means
	*  'transparency'. Or quite the opposite. Have fun.
	*/
	TEXTURE_MAP_OPACITY = 0x8,

	/** Displacement texture
	*
	*  The exact purpose and format is application-dependent.
	*  Higher color values stand for higher vertex displacements.
	*/
	TEXTURE_MAP_DISPLACEMENT = 0x9,

	/** Lightmap texture (aka Ambient Occlusion)
	*
	*  Both 'Lightmaps' and dedicated 'ambient occlusion maps' are
	*  covered by this material property. The texture contains a
	*  scaling value for the final color value of a pixel. Its
	*  intensity is not affected by incoming light.
	*/
	TEXTURE_MAP_LIGHTMAP = 0xA,

	/** Reflection texture
	*
	* Contains the color of a perfect mirror reflection.
	* Rarely used, almost never for real-time applications.
	*/
	TEXTURE_MAP_REFLECTION = 0xB,

	/** GLTF metallic roughness texture
	*
	* Contains the metallic (B) and roughness (G) textures
	* for GLTF materials.
	*/
	TEXTURE_MAP_GLTF_METALLIC_ROUGHNESS = 0xC,

	/** Unknown texture
	*
	*  A texture reference that does not match any of the definitions
	*  above is considered to be 'unknown'. It is still imported,
	*  but is excluded from any further postprocessing.
	*/
	TEXTURE_MAP_UNKNOWN = 0xD,
	TEXTURE_MAP_COUNT = TEXTURE_MAP_UNKNOWN,
};

enum TextureFilterMode
{
	TEXTURE_FILTERING_MODE_POINT,
	TEXTURE_FILTERING_MODE_BILINEAR,
	TEXTURE_FILTERING_MODE_TRILINEAR,
	TEXTURE_FILTERING_MODE_ANISOTROPIC,
	TEXTURE_FILTERING_MODE_COUNT
};

enum TextureTilingMode
{
	TEXTURE_TILING_MODE_WRAP,
	TEXTURE_TILING_MODE_CLAMP,
	TEXTURE_TILING_MODE_BORDER,
	TEXTURE_TILING_MODE_MIRROR,
	TEXTURE_TILING_MODE_COUNT
};

#define MAX_ELEMENTS_PER_PROPERTY 4U

class IModelImporter
{
	public:
	enum ModelSourceType
	{
		MODEL_SOURCE_TYPE_UNKNOWN,
		MODEL_SOURCE_TYPE_OBJ,
		MODEL_SOURCE_TYPE_FBX,
		MODEL_SOURCE_TYPE_GLTF,
		MODEL_SOURCE_TYPE_COUNT
	};

	enum MaterialSourceType
	{
		MATERIAL_SOURCE_TYPE_UNKNOWN,
		MATERIAL_SOURCE_TYPE_OBJ,
		MATERIAL_SOURCE_TYPE_FBX,
		MATERIAL_SOURCE_TYPE_GLTF_METAL_ROUGH,
		MATERIAL_SOURCE_TYPE_GLTF_SPEC_GLOSS,
		MATERIAL_SOURCE_TYPE_COUNT
	};

	struct TextureMap
	{
		tinystl::string   mName;
		TextureFilterMode mFilterMode;
		TextureTilingMode mTilingModeU;
		TextureTilingMode mTilingModeV;
		float             mScale;
	};

	struct MaterialProperty
	{
		uint32_t mDataSize;
		union
		{
			int           mIntVal[MAX_ELEMENTS_PER_PROPERTY];
			float         mFloatVal[MAX_ELEMENTS_PER_PROPERTY];
			char          mStringVal[MAX_ELEMENTS_PER_PROPERTY * 4];
			unsigned char mBufferVal[MAX_ELEMENTS_PER_PROPERTY * 4];
		};
	};

	struct MaterialData
	{
		tinystl::string                                           mName;
		tinystl::vector<TextureMap>                               mTextureMaps[TEXTURE_MAP_COUNT];
		tinystl::unordered_map<tinystl::string, MaterialProperty> mProperties;
		MaterialSourceType                                        mSourceType;
	};

	struct EmbeddedTextureData
	{
		tinystl::string                mName;
		tinystl::string                mFormat;
		tinystl::vector<unsigned char> mData;
		uint32_t                       mWidth;
		uint32_t                       mHeight;
	};

	struct BoneNames
	{
		tinystl::string mNames[4];
	};

	struct Bone
	{
		tinystl::string mName;
		mat4            mOffsetMatrix;
	};

	struct Mesh
	{
		tinystl::vector<float3>    mPositions;
		tinystl::vector<float3>    mNormals;
		tinystl::vector<float3>    mTangents;
		tinystl::vector<float3>    mBitangents;
		tinystl::vector<float2>    mUvs;
		tinystl::vector<float4>    mBoneWeights;
		tinystl::vector<BoneNames> mBoneNames;
		tinystl::vector<Bone>      mBones;
		tinystl::vector<uint32_t>  mIndices;
		BoundingBox                mBounds;
		uint32_t                   mMaterialId;
	};

	struct Node
	{
		tinystl::string           mName;
		Node*                     pParent;
		tinystl::vector<Node>     mChildren;
		tinystl::vector<uint32_t> mMeshIndices;
		mat4                      mTransform;
	};

	struct Model
	{
		/// Short name of scene
		tinystl::string       mSceneName;
		tinystl::vector<Mesh> mMeshArray;
		/// This is a look up table to map the assimp mesh ID to a geometry component
		tinystl::vector<tinystl::string> mGeometryNameList;
		/// Load all the mateiral in the scene
		tinystl::vector<MaterialData> mMaterialList;
		/// Load all the embedded textures in the scene
		tinystl::vector<EmbeddedTextureData> mEmbeddedTextureList;
		/// Scene graph
		Node mRootNode;
		/// The type of the file the model was loaded from
		ModelSourceType mSourceType;
	};

	virtual bool ImportModel(const char* filename, Model* outModel) = 0;

	virtual tinystl::string MATKEY_NAME() = 0;
	virtual tinystl::string MATKEY_TWOSIDED() = 0;
	virtual tinystl::string MATKEY_SHADING_MODEL() = 0;
	virtual tinystl::string MATKEY_ENABLE_WIREFRAME() = 0;
	virtual tinystl::string MATKEY_BLEND_FUNC() = 0;
	virtual tinystl::string MATKEY_OPACITY() = 0;
	virtual tinystl::string MATKEY_BUMPSCALING() = 0;
	virtual tinystl::string MATKEY_SHININESS() = 0;
	virtual tinystl::string MATKEY_REFLECTIVITY() = 0;
	virtual tinystl::string MATKEY_SHININESS_STRENGTH() = 0;
	virtual tinystl::string MATKEY_REFRACTI() = 0;
	virtual tinystl::string MATKEY_COLOR_DIFFUSE() = 0;
	virtual tinystl::string MATKEY_COLOR_AMBIENT() = 0;
	virtual tinystl::string MATKEY_COLOR_SPECULAR() = 0;
	virtual tinystl::string MATKEY_COLOR_EMISSIVE() = 0;
	virtual tinystl::string MATKEY_COLOR_TRANSPARENT() = 0;
	virtual tinystl::string MATKEY_COLOR_REFLECTIVE() = 0;
	virtual tinystl::string MATKEY_GLOBAL_BACKGROUND_IMAGE() = 0;

	// GLTF specific keys
	virtual tinystl::string MATKEY_GLTF_BASE_COLOR() = 0;
	virtual tinystl::string MATKEY_GLTF_METALLIC_FACTOR() = 0;
	virtual tinystl::string MATKEY_GLTF_ROUGHNESS_FACTOR() = 0;
};

class AssimpImporter: public IModelImporter
{
	public:
	~AssimpImporter() {}
	bool ImportModel(const char* filename, Model* outModel);

	tinystl::string MATKEY_NAME() { return "?mat.name"; }
	tinystl::string MATKEY_TWOSIDED() { return "$mat.twosided"; }
	tinystl::string MATKEY_SHADING_MODEL() { return "$mat.shadingm"; }
	tinystl::string MATKEY_ENABLE_WIREFRAME() { return "$mat.wireframe"; }
	tinystl::string MATKEY_BLEND_FUNC() { return "$mat.blend"; }
	tinystl::string MATKEY_OPACITY() { return "$mat.opacity"; }
	tinystl::string MATKEY_BUMPSCALING() { return "$mat.bumpscaling"; }
	tinystl::string MATKEY_SHININESS() { return "$mat.shininess"; }
	tinystl::string MATKEY_REFLECTIVITY() { return "$mat.reflectivity"; }
	tinystl::string MATKEY_SHININESS_STRENGTH() { return "$mat.shinpercent"; }
	tinystl::string MATKEY_REFRACTI() { return "$mat.refracti"; }
	tinystl::string MATKEY_COLOR_DIFFUSE() { return "$clr.diffuse"; }
	tinystl::string MATKEY_COLOR_AMBIENT() { return "$clr.ambient"; }
	tinystl::string MATKEY_COLOR_SPECULAR() { return "$clr.specular"; }
	tinystl::string MATKEY_COLOR_EMISSIVE() { return "$clr.emissive"; }
	tinystl::string MATKEY_COLOR_TRANSPARENT() { return "$clr.transparent"; }
	tinystl::string MATKEY_COLOR_REFLECTIVE() { return "$clr.reflective"; }
	tinystl::string MATKEY_GLOBAL_BACKGROUND_IMAGE() { return "?bg.global"; }

	// GLTF specific keys
	tinystl::string MATKEY_GLTF_BASE_COLOR() { return "$mat.gltf.pbrMetallicRoughness.baseColorFactor"; }
	tinystl::string MATKEY_GLTF_METALLIC_FACTOR() { return "$mat.gltf.pbrMetallicRoughness.metallicFactor"; }
	tinystl::string MATKEY_GLTF_ROUGHNESS_FACTOR() { return "$mat.gltf.pbrMetallicRoughness.roughnessFactor"; }
};
