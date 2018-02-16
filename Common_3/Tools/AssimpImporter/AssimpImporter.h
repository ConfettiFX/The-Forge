/*
 * Copyright (c) 2018 Confetti Interactive Inc.
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

	/** Unknown texture
	*
	*  A texture reference that does not match any of the definitions
	*  above is considered to be 'unknown'. It is still imported,
	*  but is excluded from any further postprocessing.
	*/
	TEXTURE_MAP_UNKNOWN = 0xC,
	TEXTURE_MAP_COUNT = TEXTURE_MAP_UNKNOWN,
};

#define MATKEY_NAME "?mat.name"
#define MATKEY_TWOSIDED "$mat.twosided"
#define MATKEY_SHADING_MODEL "$mat.shadingm"
#define MATKEY_ENABLE_WIREFRAME "$mat.wireframe"
#define MATKEY_BLEND_FUNC "$mat.blend"
#define MATKEY_OPACITY "$mat.opacity"
#define MATKEY_BUMPSCALING "$mat.bumpscaling"
#define MATKEY_SHININESS "$mat.shininess"
#define MATKEY_REFLECTIVITY "$mat.reflectivity"
#define MATKEY_SHININESS_STRENGTH "$mat.shinpercent"
#define MATKEY_REFRACTI "$mat.refracti"
#define MATKEY_COLOR_DIFFUSE "$clr.diffuse"
#define MATKEY_COLOR_AMBIENT "$clr.ambient"
#define MATKEY_COLOR_SPECULAR "$clr.specular"
#define MATKEY_COLOR_EMISSIVE "$clr.emissive"
#define MATKEY_COLOR_TRANSPARENT "$clr.transparent"
#define MATKEY_COLOR_REFLECTIVE "$clr.reflective"
#define MATKEY_GLOBAL_BACKGROUND_IMAGE "?bg.global"

#define MAX_ELEMENTS_PER_PROPERTY 4U

struct MaterialProperty
{
	uint32_t	mDataSize;
	union
	{
		int		mIntVal;
		float	mFloatVal[MAX_ELEMENTS_PER_PROPERTY];
	};
};

struct MaterialData
{
	String	mName;
	String	mTextureMaps[TEXTURE_MAP_COUNT];
	tinystl::unordered_map<String, MaterialProperty> mProperties;
};

struct Mesh
{
	tinystl::vector <float3>	mPositions;
	tinystl::vector <float3>	mNormals;
	tinystl::vector <float3>	mTangents;
	tinystl::vector <float3>	mBitangents;
	tinystl::vector <float2>	mUvs;
	tinystl::vector <uint32_t>	mIndices;
	BoundingBox					mBounds;
	uint32_t					mMaterialId;
};

struct Model
{
	/// Short name of scene
	String							mSceneName;
	tinystl::vector <Mesh>			mMeshArray;
	/// This is a look up table to map the assimp mesh ID to a geometry component
	tinystl::vector<String>			mGeometryNameList;
	/// Load all the mateiral in the scene
	tinystl::vector<MaterialData>	mMaterialList;
};

class AssimpImporter
{
public:
	static bool ImportModel(const char* filename, Model* outModel);
};