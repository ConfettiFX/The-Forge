//=============================================================================
//
// Render/GLSLGenerator.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#include "GLSLGenerator.h"
#include "HLSLParser.h"
#include "HLSLTree.h"

#include "Engine.h"
#include "StringLibrary.h"

#include <stdarg.h>
#include <string.h>
#include "../../../EASTL/string_hash_map.h"
#include "../../../EASTL/sort.h"

static const HLSLType kFloatType(HLSLBaseType_Float);
static const HLSLType kUintType(HLSLBaseType_Uint);
static const HLSLType kIntType(HLSLBaseType_Int);
static const HLSLType kBoolType(HLSLBaseType_Bool);

// These are reserved words in GLSL that aren't reserved in HLSL.
const char* GLSLGenerator::s_reservedWord[] =
    {
        "output",
        "input",
        "mod",
        "mix",
        "fract",
        "dFdx",
        "dFdy",
    };

const char* GetBaseTypeName(const HLSLBaseType& baseType)
{
	switch (baseType)
	{
	case HLSLBaseType_Void:				return "void";
	case HLSLBaseType_Float:			return "float";
	case HLSLBaseType_Float1x2:			return "mat1x2";
	case HLSLBaseType_Float1x3:			return "mat1x3";
	case HLSLBaseType_Float1x4:			return "mat1x4";
	case HLSLBaseType_Float2:			return "vec2";
	case HLSLBaseType_Float2x2:			return "mat2";
	case HLSLBaseType_Float2x3:			return "mat2x3";
	case HLSLBaseType_Float2x4:			return "mat2x4";
	case HLSLBaseType_Float3:			return "vec3";
	case HLSLBaseType_Float3x2:			return "mat3x2";
	case HLSLBaseType_Float3x3:			return "mat3";
	case HLSLBaseType_Float3x4:			return "mat3x4";
	case HLSLBaseType_Float4:			return "vec4";
	case HLSLBaseType_Float4x2:			return "mat4x2";
	case HLSLBaseType_Float4x3:			return "mat4x3";
	case HLSLBaseType_Float4x4:			return "mat4";

	case HLSLBaseType_Half:				return "mediump float";
	case HLSLBaseType_Half1x2:			return "mediump mat1x2";
	case HLSLBaseType_Half1x3:			return "mediump mat1x3";
	case HLSLBaseType_Half1x4:			return "mediump mat1x4";
	case HLSLBaseType_Half2:			return "mediump vec2";
	case HLSLBaseType_Half2x2:			return "mediump mat2";
	case HLSLBaseType_Half2x3:			return "mediump mat2x3";
	case HLSLBaseType_Half2x4:        return "mediump mat2x4";
	case HLSLBaseType_Half3:       return "mediump vec3";
	case HLSLBaseType_Half3x2:        return "mediump mat3x2";
	case HLSLBaseType_Half3x3:        return "mediump mat3";
	case HLSLBaseType_Half3x4:        return "mediump mat3x4";
	case HLSLBaseType_Half4:       return "mediump vec4";
	case HLSLBaseType_Half4x2:        return "mediump mat4x2";
	case HLSLBaseType_Half4x3:        return "mediump mat4x3";
	case HLSLBaseType_Half4x4:        return "mediump mat4";

	case HLSLBaseType_Min16Float:         return "mediump float";
	case HLSLBaseType_Min16Float1x2:        return "mediump mat1x2";
	case HLSLBaseType_Min16Float1x3:        return "mediump mat1x3";
	case HLSLBaseType_Min16Float1x4:        return "mediump mat1x4";
	case HLSLBaseType_Min16Float2:       return "mediump vec2";
	case HLSLBaseType_Min16Float2x2:        return "mediump mat2";
	case HLSLBaseType_Min16Float2x3:        return "mediump mat2x3";
	case HLSLBaseType_Min16Float2x4:        return "mediump mat2x4";
	case HLSLBaseType_Min16Float3:       return "mediump vec3";
	case HLSLBaseType_Min16Float3x2:        return "mediump mat3x2";
	case HLSLBaseType_Min16Float3x3:        return "mediump mat3";
	case HLSLBaseType_Min16Float3x4:        return "mediump mat3x4";
	case HLSLBaseType_Min16Float4:       return "mediump vec4";
	case HLSLBaseType_Min16Float4x2:        return "mediump mat4x2";
	case HLSLBaseType_Min16Float4x3:        return "mediump mat4x3";
	case HLSLBaseType_Min16Float4x4:        return "mediump mat4";

	case HLSLBaseType_Min10Float:         return "lowp float";
	case HLSLBaseType_Min10Float1x2:        return "lowp mat1x2";
	case HLSLBaseType_Min10Float1x3:        return "lowp mat1x3";
	case HLSLBaseType_Min10Float1x4:        return "lowp mat1x4";
	case HLSLBaseType_Min10Float2:       return "lowp vec2";
	case HLSLBaseType_Min10Float2x2:        return "lowp mat2";
	case HLSLBaseType_Min10Float2x3:        return "lowp mat2x3";
	case HLSLBaseType_Min10Float2x4:        return "lowp mat2x4";
	case HLSLBaseType_Min10Float3:       return "lowp vec3";
	case HLSLBaseType_Min10Float3x2:        return "lowp mat3x2";
	case HLSLBaseType_Min10Float3x3:        return "lowp mat3";
	case HLSLBaseType_Min10Float3x4:        return "lowp mat3x4";
	case HLSLBaseType_Min10Float4:       return "lowp vec4";
	case HLSLBaseType_Min10Float4x2:        return "lowp mat4x2";
	case HLSLBaseType_Min10Float4x3:        return "lowp mat4x3";
	case HLSLBaseType_Min10Float4x4:        return "lowp mat4";

	case HLSLBaseType_Bool:         return "bool";
	case HLSLBaseType_Bool1x2:        return "bvecx2";
	case HLSLBaseType_Bool1x3:        return "bvec1x3";
	case HLSLBaseType_Bool1x4:        return "bvec1x4";
	case HLSLBaseType_Bool2:        return "bvec2";
	case HLSLBaseType_Bool2x2:        return "bvec2x2";
	case HLSLBaseType_Bool2x3:        return "bvec2x3";
	case HLSLBaseType_Bool2x4:        return "bvec2x4";
	case HLSLBaseType_Bool3:        return "bvec3";
	case HLSLBaseType_Bool3x2:        return "bvec3x2";
	case HLSLBaseType_Bool3x3:        return "bvec3x3";
	case HLSLBaseType_Bool3x4:        return "bvec3x4";
	case HLSLBaseType_Bool4:        return "bvec4";
	case HLSLBaseType_Bool4x2:        return "bvec4x2";
	case HLSLBaseType_Bool4x3:        return "bvec4x3";
	case HLSLBaseType_Bool4x4:        return "bvec4x4";

	case HLSLBaseType_Int:          return "int";
	case HLSLBaseType_Int1x2:        return "ivec1x2";
	case HLSLBaseType_Int1x3:        return "ivec1x3";
	case HLSLBaseType_Int1x4:        return "ivec1x4";
	case HLSLBaseType_Int2:        return "ivec2";
	case HLSLBaseType_Int2x2:        return "ivec2x2";
	case HLSLBaseType_Int2x3:        return "ivec2x3";
	case HLSLBaseType_Int2x4:        return "ivec2x4";
	case HLSLBaseType_Int3:        return "ivec3";
	case HLSLBaseType_Int3x2:        return "ivec3x2";
	case HLSLBaseType_Int3x3:        return "ivec3x3";
	case HLSLBaseType_Int3x4:        return "ivec3x4";
	case HLSLBaseType_Int4:        return "ivec4";
	case HLSLBaseType_Int4x2:        return "ivec4x2";
	case HLSLBaseType_Int4x3:        return "ivec4x3";
	case HLSLBaseType_Int4x4:        return "ivec4x4";

	case HLSLBaseType_Uint:          return "uint";
	case HLSLBaseType_Uint1x2:        return "uvec1x2";
	case HLSLBaseType_Uint1x3:        return "uvec1x3";
	case HLSLBaseType_Uint1x4:        return "uvec1x4";
	case HLSLBaseType_Uint2:        return "uvec2";
	case HLSLBaseType_Uint2x2:        return "uvec2x2";
	case HLSLBaseType_Uint2x3:        return "uvec2x3";
	case HLSLBaseType_Uint2x4:        return "uvec2x4";
	case HLSLBaseType_Uint3:        return "uvec3";
	case HLSLBaseType_Uint3x2:        return "uvec3x2";
	case HLSLBaseType_Uint3x3:        return "uvec3x3";
	case HLSLBaseType_Uint3x4:        return "uvec3x4";
	case HLSLBaseType_Uint4:        return "uvec4";
	case HLSLBaseType_Uint4x2:        return "uvec4x2";
	case HLSLBaseType_Uint4x3:        return "uvec4x3";
	case HLSLBaseType_Uint4x4:        return "uvec4x4";

	case HLSLBaseType_InputPatch:     return "<input patch>";
	case HLSLBaseType_OutputPatch:     return "<output patch>";

	case HLSLBaseType_TriangleStream:	return "<triangle stream>";

	case HLSLBaseType_Texture:      return "texture";

	case HLSLBaseType_Texture1D:      return "texture1D";
	case HLSLBaseType_Texture1DArray:      return "texture1DArray";
	case HLSLBaseType_Texture2D:      return "texture2D";
	case HLSLBaseType_Texture2DArray:      return "texture1DArray";
	case HLSLBaseType_Texture3D:      return "texture3D";
	case HLSLBaseType_Texture2DMS:      return "texture2DMS";
	case HLSLBaseType_Texture2DMSArray:      return "texture2DMSArray";
	case HLSLBaseType_TextureCube:      return "textureCube";
	case HLSLBaseType_TextureCubeArray:      return "textureCubeArray";

	case HLSLBaseType_RWTexture1D:      return "image1D";
	case HLSLBaseType_RWTexture1DArray: return "image1DArray";
	case HLSLBaseType_RWTexture2D:      return "image2D";
	case HLSLBaseType_RWTexture2DArray: return "image1DArray";
	case HLSLBaseType_RWTexture3D:      return "image3D";

	case HLSLBaseType_Sampler:      return "sampler";
	case HLSLBaseType_Sampler2D:    return "sampler2D";
	case HLSLBaseType_Sampler3D:    return "sampler3D";
	case HLSLBaseType_SamplerCube:  return "samplerCube";
	case HLSLBaseType_Sampler2DMS:  return "sampler2DMS";
	case HLSLBaseType_Sampler2DArray:  return "sampler2DArray";
	case HLSLBaseType_SamplerState:  return "sampler";
	case HLSLBaseType_SamplerComparisonState:  return "samplerShadow";
	case HLSLBaseType_UserDefined:  return "<user defined>";
	default: return "?";
	}
}

const char * GetTypeName(const HLSLType& type)
{
	HLSLBaseType baseType = type.baseType;

	if (type.baseType == HLSLBaseType_Unknown)
		baseType = type.elementType;


	if (baseType == HLSLBaseType_UserDefined)
	{
		return RawStr(type.typeName);
	}

	if (baseType == HLSLBaseType_InputPatch)
	{
		return RawStr(type.InputPatchName);
	}

	if (baseType == HLSLBaseType_OutputPatch)
	{
		return RawStr(type.OutputPatchName);
	}
	
	if (baseType == HLSLBaseType_TriangleStream)
	{
		return RawStr(type.structuredTypeName);
	}


	return GetBaseTypeName(baseType);
}


const char* GetBaseTypeConstructor(const HLSLBaseType& baseType)
{
	switch (baseType)
	{
	case HLSLBaseType_Void:				return "void";
	case HLSLBaseType_Float:			return "float";
	case HLSLBaseType_Float1x2:			return "mat1x2";
	case HLSLBaseType_Float1x3:			return "mat1x3";
	case HLSLBaseType_Float1x4:			return "mat1x4";
	case HLSLBaseType_Float2:			return "vec2";
	case HLSLBaseType_Float2x2:			return "mat2";
	case HLSLBaseType_Float2x3:			return "mat2x3";
	case HLSLBaseType_Float2x4:			return "mat2x4";
	case HLSLBaseType_Float3:			return "vec3";
	case HLSLBaseType_Float3x2:			return "mat3x2";
	case HLSLBaseType_Float3x3:			return "mat3";
	case HLSLBaseType_Float3x4:			return "mat3x4";
	case HLSLBaseType_Float4:			return "vec4";
	case HLSLBaseType_Float4x2:			return "mat4x2";
	case HLSLBaseType_Float4x3:			return "mat4x3";
	case HLSLBaseType_Float4x4:			return "mat4";

	case HLSLBaseType_Half:				return "float";
	case HLSLBaseType_Half1x2:			return "mat1x2";
	case HLSLBaseType_Half1x3:			return "mat1x3";
	case HLSLBaseType_Half1x4:			return "mat1x4";
	case HLSLBaseType_Half2:			return "vec2";
	case HLSLBaseType_Half2x2:			return "mat2";
	case HLSLBaseType_Half2x3:			return "mat2x3";
	case HLSLBaseType_Half2x4:        return "mat2x4";
	case HLSLBaseType_Half3:       return "vec3";
	case HLSLBaseType_Half3x2:        return "mat3x2";
	case HLSLBaseType_Half3x3:        return "mat3";
	case HLSLBaseType_Half3x4:        return "mat3x4";
	case HLSLBaseType_Half4:       return "vec4";
	case HLSLBaseType_Half4x2:        return "mat4x2";
	case HLSLBaseType_Half4x3:        return "mat4x3";
	case HLSLBaseType_Half4x4:        return "mat4";

	case HLSLBaseType_Min16Float:         return "float";
	case HLSLBaseType_Min16Float1x2:        return "mat1x2";
	case HLSLBaseType_Min16Float1x3:        return "mat1x3";
	case HLSLBaseType_Min16Float1x4:        return "mat1x4";
	case HLSLBaseType_Min16Float2:       return "vec2";
	case HLSLBaseType_Min16Float2x2:        return "mat2";
	case HLSLBaseType_Min16Float2x3:        return "mat2x3";
	case HLSLBaseType_Min16Float2x4:        return "mat2x4";
	case HLSLBaseType_Min16Float3:       return "vec3";
	case HLSLBaseType_Min16Float3x2:        return "mat3x2";
	case HLSLBaseType_Min16Float3x3:        return "mat3";
	case HLSLBaseType_Min16Float3x4:        return "mat3x4";
	case HLSLBaseType_Min16Float4:       return "vec4";
	case HLSLBaseType_Min16Float4x2:        return "mat4x2";
	case HLSLBaseType_Min16Float4x3:        return "mat4x3";
	case HLSLBaseType_Min16Float4x4:        return "mat4";

	case HLSLBaseType_Min10Float:         return "float";
	case HLSLBaseType_Min10Float1x2:        return "mat1x2";
	case HLSLBaseType_Min10Float1x3:        return "mat1x3";
	case HLSLBaseType_Min10Float1x4:        return "mat1x4";
	case HLSLBaseType_Min10Float2:       return "vec2";
	case HLSLBaseType_Min10Float2x2:        return "mat2";
	case HLSLBaseType_Min10Float2x3:        return "mat2x3";
	case HLSLBaseType_Min10Float2x4:        return "mat2x4";
	case HLSLBaseType_Min10Float3:       return "vec3";
	case HLSLBaseType_Min10Float3x2:        return "mat3x2";
	case HLSLBaseType_Min10Float3x3:        return "mat3";
	case HLSLBaseType_Min10Float3x4:        return "mat3x4";
	case HLSLBaseType_Min10Float4:       return "vec4";
	case HLSLBaseType_Min10Float4x2:        return "mat4x2";
	case HLSLBaseType_Min10Float4x3:        return "mat4x3";
	case HLSLBaseType_Min10Float4x4:        return "mat4";

	case HLSLBaseType_Bool:         return "bool";
	case HLSLBaseType_Bool1x2:        return "bvecx2";
	case HLSLBaseType_Bool1x3:        return "bvec1x3";
	case HLSLBaseType_Bool1x4:        return "bvec1x4";
	case HLSLBaseType_Bool2:        return "bvec2";
	case HLSLBaseType_Bool2x2:        return "bvec2x2";
	case HLSLBaseType_Bool2x3:        return "bvec2x3";
	case HLSLBaseType_Bool2x4:        return "bvec2x4";
	case HLSLBaseType_Bool3:        return "bvec3";
	case HLSLBaseType_Bool3x2:        return "bvec3x2";
	case HLSLBaseType_Bool3x3:        return "bvec3x3";
	case HLSLBaseType_Bool3x4:        return "bvec3x4";
	case HLSLBaseType_Bool4:        return "bvec4";
	case HLSLBaseType_Bool4x2:        return "bvec4x2";
	case HLSLBaseType_Bool4x3:        return "bvec4x3";
	case HLSLBaseType_Bool4x4:        return "bvec4x4";

	case HLSLBaseType_Int:          return "int";
	case HLSLBaseType_Int1x2:        return "ivec1x2";
	case HLSLBaseType_Int1x3:        return "ivec1x3";
	case HLSLBaseType_Int1x4:        return "ivec1x4";
	case HLSLBaseType_Int2:        return "ivec2";
	case HLSLBaseType_Int2x2:        return "ivec2x2";
	case HLSLBaseType_Int2x3:        return "ivec2x3";
	case HLSLBaseType_Int2x4:        return "ivec2x4";
	case HLSLBaseType_Int3:        return "ivec3";
	case HLSLBaseType_Int3x2:        return "ivec3x2";
	case HLSLBaseType_Int3x3:        return "ivec3x3";
	case HLSLBaseType_Int3x4:        return "ivec3x4";
	case HLSLBaseType_Int4:        return "ivec4";
	case HLSLBaseType_Int4x2:        return "ivec4x2";
	case HLSLBaseType_Int4x3:        return "ivec4x3";
	case HLSLBaseType_Int4x4:        return "ivec4x4";

	case HLSLBaseType_Uint:          return "uint";
	case HLSLBaseType_Uint1x2:        return "uvec1x2";
	case HLSLBaseType_Uint1x3:        return "uvec1x3";
	case HLSLBaseType_Uint1x4:        return "uvec1x4";
	case HLSLBaseType_Uint2:        return "uvec2";
	case HLSLBaseType_Uint2x2:        return "uvec2x2";
	case HLSLBaseType_Uint2x3:        return "uvec2x3";
	case HLSLBaseType_Uint2x4:        return "uvec2x4";
	case HLSLBaseType_Uint3:        return "uvec3";
	case HLSLBaseType_Uint3x2:        return "uvec3x2";
	case HLSLBaseType_Uint3x3:        return "uvec3x3";
	case HLSLBaseType_Uint3x4:        return "uvec3x4";
	case HLSLBaseType_Uint4:        return "uvec4";
	case HLSLBaseType_Uint4x2:        return "uvec4x2";
	case HLSLBaseType_Uint4x3:        return "uvec4x3";
	case HLSLBaseType_Uint4x4:        return "uvec4x4";

	case HLSLBaseType_InputPatch:     return "<input patch>";
	case HLSLBaseType_OutputPatch:     return "<output patch>";

	case HLSLBaseType_TriangleStream:	return "<triangle stream>";

	case HLSLBaseType_Texture:      return "texture";

	case HLSLBaseType_Texture1D:      return "texture1D";
	case HLSLBaseType_Texture1DArray:      return "texture1DArray";
	case HLSLBaseType_Texture2D:      return "texture2D";
	case HLSLBaseType_Texture2DArray:      return "texture1DArray";
	case HLSLBaseType_Texture3D:      return "texture3D";
	case HLSLBaseType_Texture2DMS:      return "texture2DMS";
	case HLSLBaseType_Texture2DMSArray:      return "texture2DMSArray";
	case HLSLBaseType_TextureCube:      return "textureCube";
	case HLSLBaseType_TextureCubeArray:      return "textureCubeArray";

	case HLSLBaseType_RWTexture1D:      return "RWTexture1D";
	case HLSLBaseType_RWTexture1DArray:      return "RWTexture1DArray";
	case HLSLBaseType_RWTexture2D:      return "RWTexture2D";
	case HLSLBaseType_RWTexture2DArray:      return "RWTexture1DArray";
	case HLSLBaseType_RWTexture3D:      return "RWTexture3D";

	case HLSLBaseType_Sampler:      return "sampler";
	case HLSLBaseType_Sampler2D:    return "sampler2D";
	case HLSLBaseType_Sampler3D:    return "sampler3D";
	case HLSLBaseType_SamplerCube:  return "samplerCube";
	case HLSLBaseType_Sampler2DMS:  return "sampler2DMS";
	case HLSLBaseType_Sampler2DArray:  return "sampler2DArray";
	case HLSLBaseType_SamplerState:  return "sampler";
	case HLSLBaseType_SamplerComparisonState:  return "samplerShadow";
	case HLSLBaseType_UserDefined:  return "<user defined>";
	default: return "?";
	}
}

int TypeArraySize(HLSLType& type)
{
	int size = 1;
	if (type.array)
	{
		for (int i =0; i<type.arrayDimension; ++i)
			size *= type.arrayExtent[i];
	}

	return size;
}

void AssignRegisters(HLSLRoot* root, const eastl::vector<BindingShift>& shiftVec)
{
	struct Range{int start, end;};
	eastl::hash_map<int, eastl::vector<Range>> spaces;

	// Gather allocated registers ranges
	HLSLStatement* statement = root->statement;
	while (statement != NULL)
	{
		switch(statement->nodeType)
		{
		case HLSLNodeType_Buffer:
			if (static_cast<HLSLBuffer*>(statement)->bPushConstant)
				break;
		case HLSLNodeType_TextureState:
		case HLSLNodeType_SamplerState:
			{
				HLSLDeclaration* pDeclaration = (HLSLDeclaration*)statement;
				Range r = {INT32_MAX, INT32_MAX};
				int regId = pDeclaration->registerIndex;
				int spaceId = eastl::max(pDeclaration->registerSpace, 0);
				char regType= pDeclaration->registerType;
				if (regId >=0)
				{
					auto it = eastl::find(
						shiftVec.begin(),
						shiftVec.end(),
						BindingShift{regType, spaceId},
						[](const BindingShift& shift, const BindingShift& desc){
							return shift.m_reg ==desc.m_reg && shift.m_space==desc.m_space;
					});
					if (it != shiftVec.end())
					{
						regId += it->m_shift;
						pDeclaration->registerIndex += it->m_shift;
					}
					r.start = regId;
					r.end = r.start + (TypeArraySize(pDeclaration->type) - 1);
					spaces[spaceId].push_back(r);
				}
				break;
			}
		}

		statement = statement->nextStatement;
	}

	// Sort and merge ranges
	for (auto& ranges: spaces)
	{
		if (ranges.second.empty())
			continue;

		eastl::sort(
			ranges.second.begin(),
			ranges.second.end(),
			[](const Range& r1, const Range& r2){return r1.start < r2.start;}
		);
		size_t p = 0;
		for (size_t i = 1; i < ranges.second.size(); ++i)
		{
			Range& r1 = ranges.second[p];
			Range& r2 = ranges.second[i];
			if (r2.start <= r1.end)
			{
				r1.end = eastl::max(r1.end, r2.end); 
			}
			else
			{
				++p;
			}
		}
		ranges.second.erase(ranges.second.begin() + p + 1, ranges.second.end());
	}

	// Generate free ranges
	for (auto& ranges: spaces)
	{
		if (ranges.second.empty())
			continue;

		eastl::vector<Range> free_ranges;
		eastl::vector<Range>& used_ranges = ranges.second;

		int s = 0;
		for (size_t i = 0; i < used_ranges.size(); ++i)
		{
			Range& r = used_ranges[i];
			if (r.start != s)
			{
				free_ranges.push_back({s, r.start-1});
			}
			s = r.end + 1;
		}
		if (s != INT32_MAX)
			free_ranges.push_back({s, INT32_MAX});
		ranges.second.assign(free_ranges.begin(), free_ranges.end());
	}

	// Assign registers
	statement = root->statement;
	while (statement != NULL)
	{
		switch(statement->nodeType)
		{
		case HLSLNodeType_Buffer:
			if (static_cast<HLSLBuffer*>(statement)->bPushConstant)
				break;
		case HLSLNodeType_TextureState:
		case HLSLNodeType_SamplerState:
			{
				HLSLDeclaration* pDeclaration = (HLSLDeclaration*)statement;
				int regId = pDeclaration->registerIndex;
				int spaceId = pDeclaration->registerSpace;

				if (spaceId < 0)
				{
					pDeclaration->registerSpace = spaceId = 0;
				}
				if (regId < 0)
				{
					// Find suitable interval
					auto& ranges = spaces[spaceId];
					if (ranges.empty())
						ranges.push_back({0, INT32_MAX});
					int size = TypeArraySize(pDeclaration->type);
					auto it = eastl::find(ranges.begin(), ranges.end(), size, [](const Range& r, int sz){return r.end-r.start>=sz-1;});
					if (it == ranges.end())
						break;
					pDeclaration->registerIndex = it->start;
					if (it->end - it->start + 1 <= size)
						ranges.erase(it);
					else
						it->start += size;
				}
				break;
			}
		}

		statement = statement->nextStatement;
	}
}

static bool GetCanImplicitCast(const HLSLType& srcType, const HLSLType& dstType)
{
	HLSLBaseType tempSrcType = HLSLBaseType_Unknown;
	HLSLBaseType tempDstType = HLSLBaseType_Unknown;

	if (srcType.elementType != HLSLBaseType_Unknown)
	{
		tempSrcType = srcType.elementType;
	}
	else if (srcType.baseType >= HLSLBaseType_Texture1D && srcType.baseType <= HLSLBaseType_RWTexture3D)
	{
		tempSrcType = HLSLBaseType_Float4;
	}
	else
		tempSrcType = srcType.baseType;


	if (dstType.elementType != HLSLBaseType_Unknown)
	{
		tempDstType = dstType.elementType;
	}
	else if (dstType.baseType >= HLSLBaseType_Texture1D && dstType.baseType <= HLSLBaseType_RWTexture3D)
	{
		if (tempSrcType >= HLSLBaseType_Float && tempSrcType <= HLSLBaseType_Float4)
		{
			tempDstType = tempSrcType;
		}
		else
			tempDstType = HLSLBaseType_Float4;
	}
	else
		tempDstType = dstType.baseType;

    return tempSrcType == tempDstType;
}

static void WriteOpenglGetDimensionsVariation(CodeWriter & writer,
	const eastl::string & funcName,
	const eastl::string & texType,
	int numElem,
	bool isArray,
	int dimType, /* 0=int, 1=uint, 2=float */
	bool isMsaa)
{
	// each type/isArray combiation has 6 variations, 2 of each type (int/float/uint)
	eastl::string dimNames[3];
	dimNames[0] = "width";
	dimNames[1] = "height";
	dimNames[2] = "depth";

	eastl::string vecNames[3];
	vecNames[0] = "int";
	vecNames[1] = "ivec2";
	vecNames[2] = "ivec3";

	eastl::string typeNames[3];
	typeNames[0] = "int";
	typeNames[1] = "uint";
	typeNames[2] = "float";

	eastl::string typeName = typeNames[dimType];

	// Variation 0: Texture2DArray, UINT MipLevel, UINT Width, UINT Height, UINT Elements, UINT NumberOfLevels
	// Variation 1: UINT Width, UINT Height, UINT Elements // miplevel is assumed to be 0
	for (int iter = 0; iter < 2; iter++)
	{
		// first iteration has mipLevel and numLevels, but the second iteration does not
		bool isFullProto = (iter == 0);

		// msaa versions don't have the lod or lod variants
		if (isMsaa && isFullProto)
		{
			continue;
		}

		eastl::string fullPrototype;

		char prefix[2048];
		String_Printf(prefix,2048,"void %s(%s texName",funcName.c_str(),texType.c_str());
		fullPrototype = prefix;

		if (isFullProto)
		{
			fullPrototype = fullPrototype + ", uint mipLevel";
		}

		for (int i = 0; i < numElem; i++)
		{
			fullPrototype = fullPrototype + ", out " + typeName + " " + dimNames[i];
		}
		
		if (isArray)
		{
			fullPrototype = fullPrototype + ", out " + typeName + " elements";
		}

		if (isFullProto)
		{
			fullPrototype = fullPrototype + ", out uint numberOfLevels";
		}

		fullPrototype = fullPrototype + ")";

		writer.WriteLine(0, "%s", fullPrototype.c_str());

		writer.WriteLine(0, "{");

		eastl::string mipLevelName = (iter == 0) ? ",int(mipLevel)" : ",0";

		if (isMsaa)
		{
			mipLevelName = "";
		}

		if (numElem == 1 && !isArray)
		{
			writer.WriteLine(1, "width = %s(textureSize(texName%s));", typeName.c_str(), mipLevelName.c_str());
			//writer.WriteLine(1, "width = 0;", typeName.c_str());
		}
		else
		{
			writer.WriteLine(1, "width = %s(textureSize(texName%s).x);", typeName.c_str(), mipLevelName.c_str());
			//writer.WriteLine(1, "width = 0;", typeName.c_str());
		}

		if (numElem >= 2)
		{
			writer.WriteLine(1, "height = %s(textureSize(texName%s).y);", typeName.c_str(), mipLevelName.c_str());
			//writer.WriteLine(1, "height = 0;", typeName.c_str());
		}

		if (numElem >= 3)
		{
			writer.WriteLine(1, "depth = %s(textureSize(texName%s).z);", typeName.c_str(), mipLevelName.c_str());
			//writer.WriteLine(1, "depth = 0;", typeName.c_str());
		}

		if (isArray)
		{
			// the array size is the last component, which is y if there is 1 element and z if there are two.
			eastl::string arrayComp = (numElem == 1) ? "y" : "z";

			writer.WriteLine(1, "elements = %s(textureSize(texName%s).%s);", typeName.c_str(), mipLevelName.c_str(),arrayComp.c_str());
			//writer.WriteLine(1, "elements = 0;", typeName.c_str(),arrayComp.c_str());
		}

		if (isFullProto)
		{
			writer.WriteLine(1, "numberOfLevels = uint(textureQueryLevels(texName));");
			//writer.WriteLine(1, "numberOfLevels = 0;", typeName.c_str());
		}

		writer.WriteLine(0, "}");
		writer.WriteLine(0, "");
	}

	// Variation 1: Texture2DArray UINT Width, UINT Height, UINT Elements


}

// TODO: output only revalent signatures
static void WriteOpenglGetDimensionsOverloads(CodeWriter & writer, const eastl::string & funcName)
{
	// After thinking it through, the best way to handle the GetDimensions() variations is to just implement the overloads.
	// If we want to reduce code size, we could only write out the overloads that we actually use.

	WriteOpenglGetDimensionsVariation(writer, funcName, "texture1D", 1, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture1D", 1, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture1D", 1, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture1DArray", 1, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture1DArray", 1, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture1DArray", 1, true, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2D", 2, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2D", 2, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2D", 2, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DArray", 2, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DArray", 2, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DArray", 2, true, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture1D", 1, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture1D", 1, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture1D", 1, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture1DArray", 1, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture1DArray", 1, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture1DArray", 1, true, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2D", 2, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2D", 2, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2D", 2, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DArray", 2, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DArray", 2, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DArray", 2, true, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture1D", 1, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture1D", 1, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture1D", 1, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture1DArray", 1, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture1DArray", 1, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture1DArray", 1, true, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2D", 2, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2D", 2, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2D", 2, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DArray", 2, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DArray", 2, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DArray", 2, true, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "texture3D", 3, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture3D", 3, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture3D", 3, false, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture3D", 3, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture3D", 3, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture3D", 3, false, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture3D", 3, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture3D", 3, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture3D", 3, false, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "textureCube", 2, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "textureCube", 2, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "textureCube", 2, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "textureCubeArray", 2, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "textureCubeArray", 2, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "textureCubeArray", 2, true, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "utextureCube", 2, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utextureCube", 2, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utextureCube", 2, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utextureCubeArray", 2, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utextureCubeArray", 2, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utextureCubeArray", 2, true, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "itextureCube", 2, false, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itextureCube", 2, false, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itextureCube", 2, false, 2, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itextureCubeArray", 2, true, 0, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itextureCubeArray", 2, true, 1, false);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itextureCubeArray", 2, true, 2, false);

	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DMS", 2, false, 0, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DMS", 2, false, 1, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DMS", 2, false, 2, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DMSArray", 2, true, 0, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DMSArray", 2, true, 1, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "texture2DMSArray", 2, true, 2, true);

	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DMS", 2, false, 0, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DMS", 2, false, 1, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DMS", 2, false, 2, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DMSArray", 2, true, 0, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DMSArray", 2, true, 1, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "utexture2DMSArray", 2, true, 2, true);

	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DMS", 2, false, 0, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DMS", 2, false, 1, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DMS", 2, false, 2, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DMSArray", 2, true, 0, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DMSArray", 2, true, 1, true);
	WriteOpenglGetDimensionsVariation(writer, funcName, "itexture2DMSArray", 2, true, 2, true);
}


// we will think about vectors as comun vectors since the matrices are row major
// so a float3x4 has 3 rows and 4 columns
// whereas a float3 as 3 rows and 1 column
// returns 0 on an error
static int GetTypeNumRows(const HLSLBaseType lhsType)
{
	switch(lhsType)
	{
	case HLSLBaseType_Float:
	case HLSLBaseType_Float1x2:
	case HLSLBaseType_Float1x3:
	case HLSLBaseType_Float1x4:
		return 1;
	case HLSLBaseType_Float2:
	case HLSLBaseType_Float2x2:
	case HLSLBaseType_Float2x3:
	case HLSLBaseType_Float2x4:
		return 2;
	case HLSLBaseType_Float3:
	case HLSLBaseType_Float3x2:
	case HLSLBaseType_Float3x3:
	case HLSLBaseType_Float3x4:
		return 3;
	case HLSLBaseType_Float4:
	case HLSLBaseType_Float4x2:
	case HLSLBaseType_Float4x3:
	case HLSLBaseType_Float4x4:
		return 4;

	case HLSLBaseType_Half:
	case HLSLBaseType_Half1x2:
	case HLSLBaseType_Half1x3:
	case HLSLBaseType_Half1x4:
		return 1;
	case HLSLBaseType_Half2:
	case HLSLBaseType_Half2x2:
	case HLSLBaseType_Half2x3:
	case HLSLBaseType_Half2x4:
		return 2;
	case HLSLBaseType_Half3:
	case HLSLBaseType_Half3x2:
	case HLSLBaseType_Half3x3:
	case HLSLBaseType_Half3x4:
		return 3;
	case HLSLBaseType_Half4:
	case HLSLBaseType_Half4x2:
	case HLSLBaseType_Half4x3:
	case HLSLBaseType_Half4x4:
		return 4;


	case HLSLBaseType_Min16Float:
	case HLSLBaseType_Min16Float1x2:
	case HLSLBaseType_Min16Float1x3:
	case HLSLBaseType_Min16Float1x4:
		return 1;
	case HLSLBaseType_Min16Float2:
	case HLSLBaseType_Min16Float2x2:
	case HLSLBaseType_Min16Float2x3:
	case HLSLBaseType_Min16Float2x4:
		return 2;
	case HLSLBaseType_Min16Float3:
	case HLSLBaseType_Min16Float3x2:
	case HLSLBaseType_Min16Float3x3:
	case HLSLBaseType_Min16Float3x4:
		return 3;
	case HLSLBaseType_Min16Float4:
	case HLSLBaseType_Min16Float4x2:
	case HLSLBaseType_Min16Float4x3:
	case HLSLBaseType_Min16Float4x4:
		return 4;

	case HLSLBaseType_Min10Float:
	case HLSLBaseType_Min10Float1x2:
	case HLSLBaseType_Min10Float1x3:
	case HLSLBaseType_Min10Float1x4:
		return 1;
	case HLSLBaseType_Min10Float2:
	case HLSLBaseType_Min10Float2x2:
	case HLSLBaseType_Min10Float2x3:
	case HLSLBaseType_Min10Float2x4:
		return 2;
	case HLSLBaseType_Min10Float3:
	case HLSLBaseType_Min10Float3x2:
	case HLSLBaseType_Min10Float3x3:
	case HLSLBaseType_Min10Float3x4:
		return 3;
	case HLSLBaseType_Min10Float4:
	case HLSLBaseType_Min10Float4x2:
	case HLSLBaseType_Min10Float4x3:
	case HLSLBaseType_Min10Float4x4:
		return 4;

	case HLSLBaseType_Bool:
	case HLSLBaseType_Bool1x2:
	case HLSLBaseType_Bool1x3:
	case HLSLBaseType_Bool1x4:
		return 1;
	case HLSLBaseType_Bool2:
	case HLSLBaseType_Bool2x2:
	case HLSLBaseType_Bool2x3:
	case HLSLBaseType_Bool2x4:
		return 2;
	case HLSLBaseType_Bool3:
	case HLSLBaseType_Bool3x2:
	case HLSLBaseType_Bool3x3:
	case HLSLBaseType_Bool3x4:
		return 3;
	case HLSLBaseType_Bool4:
	case HLSLBaseType_Bool4x2:
	case HLSLBaseType_Bool4x3:
	case HLSLBaseType_Bool4x4:
		return 4;

	case HLSLBaseType_Int:
	case HLSLBaseType_Int1x2:
	case HLSLBaseType_Int1x3:
	case HLSLBaseType_Int1x4:
		return 1;
	case HLSLBaseType_Int2:
	case HLSLBaseType_Int2x2:
	case HLSLBaseType_Int2x3:
	case HLSLBaseType_Int2x4:
		return 2;
	case HLSLBaseType_Int3:
	case HLSLBaseType_Int3x2:
	case HLSLBaseType_Int3x3:
	case HLSLBaseType_Int3x4:
		return 3;
	case HLSLBaseType_Int4:
	case HLSLBaseType_Int4x2:
	case HLSLBaseType_Int4x3:
	case HLSLBaseType_Int4x4:
		return 4;

	case HLSLBaseType_Uint:
	case HLSLBaseType_Uint1x2:
	case HLSLBaseType_Uint1x3:
	case HLSLBaseType_Uint1x4:
		return 1;
	case HLSLBaseType_Uint2:
	case HLSLBaseType_Uint2x2:
	case HLSLBaseType_Uint2x3:
	case HLSLBaseType_Uint2x4:
		return 2;
	case HLSLBaseType_Uint3:
	case HLSLBaseType_Uint3x2:
	case HLSLBaseType_Uint3x3:
	case HLSLBaseType_Uint3x4:
		return 3;
	case HLSLBaseType_Uint4:
	case HLSLBaseType_Uint4x2:
	case HLSLBaseType_Uint4x3:
	case HLSLBaseType_Uint4x4:
		return 4;

	case HLSLBaseType_InputPatch:
	case HLSLBaseType_OutputPatch:

	case HLSLBaseType_PointStream:
	case HLSLBaseType_LineStream:
	case HLSLBaseType_TriangleStream:

	case HLSLBaseType_Point:
	case HLSLBaseType_Line:
	case HLSLBaseType_Triangle:
	case HLSLBaseType_Lineadj:
	case HLSLBaseType_Triangleadj:



	case HLSLBaseType_Texture:

	case HLSLBaseType_Texture1D:
	case HLSLBaseType_Texture1DArray:
	case HLSLBaseType_Texture2D:
	case HLSLBaseType_Texture2DArray:
	case HLSLBaseType_Texture3D:
	case HLSLBaseType_Texture2DMS:
	case HLSLBaseType_Texture2DMSArray:
	case HLSLBaseType_TextureCube:
	case HLSLBaseType_TextureCubeArray:

	case HLSLBaseType_RasterizerOrderedTexture1D:
	case HLSLBaseType_RasterizerOrderedTexture1DArray:
	case HLSLBaseType_RasterizerOrderedTexture2D:
	case HLSLBaseType_RasterizerOrderedTexture2DArray:
	case HLSLBaseType_RasterizerOrderedTexture3D:

	case HLSLBaseType_RWTexture1D:
	case HLSLBaseType_RWTexture1DArray:
	case HLSLBaseType_RWTexture2D:
	case HLSLBaseType_RWTexture2DArray:
	case HLSLBaseType_RWTexture3D:

	case HLSLBaseType_Sampler:           // @@ use type inference to determine sampler type.
	case HLSLBaseType_Sampler2D:
	case HLSLBaseType_Sampler3D:
	case HLSLBaseType_SamplerCube:
	case HLSLBaseType_Sampler2DShadow:
	case HLSLBaseType_Sampler2DMS:
	case HLSLBaseType_Sampler2DArray:

	case HLSLBaseType_UserDefined:       // struct
	case HLSLBaseType_SamplerState:
	case HLSLBaseType_SamplerComparisonState:
	case HLSLBaseType_TextureState:
	case HLSLBaseType_RWTextureState:

	case HLSLBaseType_PatchControlPoint:

	//Now: these are for MSL only
	case HLSLBaseType_DepthTexture2D:
	case HLSLBaseType_DepthTexture2DArray:
	case HLSLBaseType_DepthTexture2DMS:
	case HLSLBaseType_DepthTexture2DMSArray:
	case HLSLBaseType_DepthTextureCube:
	case HLSLBaseType_DepthTextureCubeArray:

	case HLSLBaseType_AddressU:
	case HLSLBaseType_AddressV:
	case HLSLBaseType_AddressW:
	case HLSLBaseType_BorderColor:
	case HLSLBaseType_Filter:
	case HLSLBaseType_MaxAnisotropy:
	case HLSLBaseType_MaxLOD:
	case HLSLBaseType_MinLOD:
	case HLSLBaseType_MipLODBias:
	case HLSLBaseType_ComparisonFunc:


	case HLSLBaseType_CBuffer:
	case HLSLBaseType_TBuffer:
	case HLSLBaseType_ConstantBuffer:
	case HLSLBaseType_StructuredBuffer:
	case HLSLBaseType_PureBuffer:
	case HLSLBaseType_RWBuffer:
	case HLSLBaseType_RWStructuredBuffer:
	case HLSLBaseType_ByteAddressBuffer:
	case HLSLBaseType_RWByteAddressBuffer:

	case HLSLBaseType_RasterizerOrderedBuffer:
	case HLSLBaseType_RasterizerOrderedStructuredBuffer:
	case HLSLBaseType_RasterizerOrderedByteAddressBuffer:


	case HLSLBaseType_UserMacro:
	case HLSLBaseType_Empty:
	default:
		return 0;
	}

	ASSERT_PARSER(0);
	return 0;
}





static int GetTypeNumCols(const HLSLBaseType lhsType)
{
	switch (lhsType)
	{
	case HLSLBaseType_Float:
	case HLSLBaseType_Float2:
	case HLSLBaseType_Float3:
	case HLSLBaseType_Float4:
		return 1;
	case HLSLBaseType_Float1x2:
	case HLSLBaseType_Float2x2:
	case HLSLBaseType_Float3x2:
	case HLSLBaseType_Float4x2:
		return 2;
	case HLSLBaseType_Float1x3:
	case HLSLBaseType_Float2x3:
	case HLSLBaseType_Float3x3:
	case HLSLBaseType_Float4x3:
		return 3;
	case HLSLBaseType_Float1x4:
	case HLSLBaseType_Float2x4:
	case HLSLBaseType_Float3x4:
	case HLSLBaseType_Float4x4:
		return 4;

	case HLSLBaseType_Half:
	case HLSLBaseType_Half2:
	case HLSLBaseType_Half3:
	case HLSLBaseType_Half4:
		return 1;
	case HLSLBaseType_Half1x2:
	case HLSLBaseType_Half2x2:
	case HLSLBaseType_Half3x2:
	case HLSLBaseType_Half4x2:
		return 2;
	case HLSLBaseType_Half1x3:
	case HLSLBaseType_Half2x3:
	case HLSLBaseType_Half3x3:
	case HLSLBaseType_Half4x3:
		return 3;
	case HLSLBaseType_Half1x4:
	case HLSLBaseType_Half2x4:
	case HLSLBaseType_Half3x4:
	case HLSLBaseType_Half4x4:
		return 4;

	case HLSLBaseType_Min16Float:
	case HLSLBaseType_Min16Float2:
	case HLSLBaseType_Min16Float3:
	case HLSLBaseType_Min16Float4:
		return 1;
	case HLSLBaseType_Min16Float1x2:
	case HLSLBaseType_Min16Float2x2:
	case HLSLBaseType_Min16Float3x2:
	case HLSLBaseType_Min16Float4x2:
		return 2;
	case HLSLBaseType_Min16Float1x3:
	case HLSLBaseType_Min16Float2x3:
	case HLSLBaseType_Min16Float3x3:
	case HLSLBaseType_Min16Float4x3:
		return 3;
	case HLSLBaseType_Min16Float1x4:
	case HLSLBaseType_Min16Float2x4:
	case HLSLBaseType_Min16Float3x4:
	case HLSLBaseType_Min16Float4x4:
		return 4;

	case HLSLBaseType_Min10Float:
	case HLSLBaseType_Min10Float2:
	case HLSLBaseType_Min10Float3:
	case HLSLBaseType_Min10Float4:
		return 1;
	case HLSLBaseType_Min10Float1x2:
	case HLSLBaseType_Min10Float2x2:
	case HLSLBaseType_Min10Float3x2:
	case HLSLBaseType_Min10Float4x2:
		return 2;
	case HLSLBaseType_Min10Float1x3:
	case HLSLBaseType_Min10Float2x3:
	case HLSLBaseType_Min10Float3x3:
	case HLSLBaseType_Min10Float4x3:
		return 3;
	case HLSLBaseType_Min10Float1x4:
	case HLSLBaseType_Min10Float2x4:
	case HLSLBaseType_Min10Float3x4:
	case HLSLBaseType_Min10Float4x4:
		return 4;

	case HLSLBaseType_Bool:
	case HLSLBaseType_Bool2:
	case HLSLBaseType_Bool3:
	case HLSLBaseType_Bool4:
		return 1;
	case HLSLBaseType_Bool1x2:
	case HLSLBaseType_Bool2x2:
	case HLSLBaseType_Bool3x2:
	case HLSLBaseType_Bool4x2:
		return 2;
	case HLSLBaseType_Bool1x3:
	case HLSLBaseType_Bool2x3:
	case HLSLBaseType_Bool3x3:
	case HLSLBaseType_Bool4x3:
		return 3;
	case HLSLBaseType_Bool1x4:
	case HLSLBaseType_Bool2x4:
	case HLSLBaseType_Bool3x4:
	case HLSLBaseType_Bool4x4:
		return 4;

	case HLSLBaseType_Int:
	case HLSLBaseType_Int2:
	case HLSLBaseType_Int3:
	case HLSLBaseType_Int4:
		return 1;
	case HLSLBaseType_Int1x2:
	case HLSLBaseType_Int2x2:
	case HLSLBaseType_Int3x2:
	case HLSLBaseType_Int4x2:
		return 2;
	case HLSLBaseType_Int1x3:
	case HLSLBaseType_Int2x3:
	case HLSLBaseType_Int3x3:
	case HLSLBaseType_Int4x3:
		return 3;
	case HLSLBaseType_Int1x4:
	case HLSLBaseType_Int2x4:
	case HLSLBaseType_Int3x4:
	case HLSLBaseType_Int4x4:
		return 4;

	case HLSLBaseType_Uint:
	case HLSLBaseType_Uint2:
	case HLSLBaseType_Uint3:
	case HLSLBaseType_Uint4:
		return 1;
	case HLSLBaseType_Uint1x2:
	case HLSLBaseType_Uint2x2:
	case HLSLBaseType_Uint3x2:
	case HLSLBaseType_Uint4x2:
		return 2;
	case HLSLBaseType_Uint1x3:
	case HLSLBaseType_Uint2x3:
	case HLSLBaseType_Uint3x3:
	case HLSLBaseType_Uint4x3:
		return 3;
	case HLSLBaseType_Uint1x4:
	case HLSLBaseType_Uint2x4:
	case HLSLBaseType_Uint3x4:
	case HLSLBaseType_Uint4x4:
		return 4;
	case HLSLBaseType_InputPatch:
	case HLSLBaseType_OutputPatch:

	case HLSLBaseType_PointStream:
	case HLSLBaseType_LineStream:
	case HLSLBaseType_TriangleStream:

	case HLSLBaseType_Point:
	case HLSLBaseType_Line:
	case HLSLBaseType_Triangle:
	case HLSLBaseType_Lineadj:
	case HLSLBaseType_Triangleadj:



	case HLSLBaseType_Texture:

	case HLSLBaseType_Texture1D:
	case HLSLBaseType_Texture1DArray:
	case HLSLBaseType_Texture2D:
	case HLSLBaseType_Texture2DArray:
	case HLSLBaseType_Texture3D:
	case HLSLBaseType_Texture2DMS:
	case HLSLBaseType_Texture2DMSArray:
	case HLSLBaseType_TextureCube:
	case HLSLBaseType_TextureCubeArray:

	case HLSLBaseType_RasterizerOrderedTexture1D:
	case HLSLBaseType_RasterizerOrderedTexture1DArray:
	case HLSLBaseType_RasterizerOrderedTexture2D:
	case HLSLBaseType_RasterizerOrderedTexture2DArray:
	case HLSLBaseType_RasterizerOrderedTexture3D:

	case HLSLBaseType_RWTexture1D:
	case HLSLBaseType_RWTexture1DArray:
	case HLSLBaseType_RWTexture2D:
	case HLSLBaseType_RWTexture2DArray:
	case HLSLBaseType_RWTexture3D:

	case HLSLBaseType_Sampler:           // @@ use type inference to determine sampler type.
	case HLSLBaseType_Sampler2D:
	case HLSLBaseType_Sampler3D:
	case HLSLBaseType_SamplerCube:
	case HLSLBaseType_Sampler2DShadow:
	case HLSLBaseType_Sampler2DMS:
	case HLSLBaseType_Sampler2DArray:

	case HLSLBaseType_UserDefined:       // struct
	case HLSLBaseType_SamplerState:
	case HLSLBaseType_TextureState:
	case HLSLBaseType_RWTextureState:

	case HLSLBaseType_PatchControlPoint:

		//Now: these are for MSL only
	case HLSLBaseType_DepthTexture2D:
	case HLSLBaseType_DepthTexture2DArray:
	case HLSLBaseType_DepthTexture2DMS:
	case HLSLBaseType_DepthTexture2DMSArray:
	case HLSLBaseType_DepthTextureCube:
	case HLSLBaseType_DepthTextureCubeArray:

	case HLSLBaseType_AddressU:
	case HLSLBaseType_AddressV:
	case HLSLBaseType_AddressW:
	case HLSLBaseType_BorderColor:
	case HLSLBaseType_Filter:
	case HLSLBaseType_MaxAnisotropy:
	case HLSLBaseType_MaxLOD:
	case HLSLBaseType_MinLOD:
	case HLSLBaseType_MipLODBias:
	case HLSLBaseType_ComparisonFunc:


	case HLSLBaseType_CBuffer:
	case HLSLBaseType_TBuffer:
	case HLSLBaseType_ConstantBuffer:
	case HLSLBaseType_StructuredBuffer:
	case HLSLBaseType_PureBuffer:
	case HLSLBaseType_RWBuffer:
	case HLSLBaseType_RWStructuredBuffer:
	case HLSLBaseType_ByteAddressBuffer:
	case HLSLBaseType_RWByteAddressBuffer:

	case HLSLBaseType_RasterizerOrderedBuffer:
	case HLSLBaseType_RasterizerOrderedStructuredBuffer:
	case HLSLBaseType_RasterizerOrderedByteAddressBuffer:


	case HLSLBaseType_UserMacro:
	case HLSLBaseType_Empty:
	default:
		return 0;
	}

	ASSERT_PARSER(0);
	return 0;
}



static void WriteOpenglMatrixMultiplyVariation(CodeWriter & writer,
	const eastl::string & funcName,
	const HLSLBaseType lhsType,
	const HLSLBaseType rhsType,
	const eastl::string & lhsName,
	const eastl::string & rhsName)
{
	bool lhsMatrix = IsMatrixType(lhsType);
	bool rhsMatrix = IsMatrixType(rhsType);

	int lhsRows = GetTypeNumRows(lhsType);
	int lhsCols = GetTypeNumCols(lhsType);

	int rhsRows = GetTypeNumRows(rhsType);
	int rhsCols = GetTypeNumCols(rhsType);


	HLSLBaseType lhsScalarType = GetScalarBaseType(lhsType);
	HLSLBaseType rhsScalarType = GetScalarBaseType(rhsType);
	HLSLBaseType dstScalarType = HLSLBaseType_Unknown;;

	{
		HLSLType tempLhs;
		tempLhs.baseType = lhsScalarType;
		HLSLType tempRhs;
		tempRhs.baseType = rhsScalarType;
		HLSLType tempDst, argType;
		bool convertOk = HLSLParser::GetBinaryOpResultType(HLSLBinaryOp_Mul, tempLhs, tempRhs, argType, tempDst);
		ASSERT_PARSER(convertOk);

		dstScalarType = tempDst.baseType;
	}

	// note: the logic below for the destination type is based on the assumption that the types are sequential

	if (lhsMatrix && rhsMatrix)
	{
		ASSERT_PARSER(lhsCols == rhsRows);
		
		int dstCols = rhsCols;
		int dstRows = lhsRows;

		int vecLen = lhsCols;
		HLSLBaseType dstType = (HLSLBaseType)(dstScalarType + ((dstRows-1) * 4) + (dstCols-1));

		eastl::string dstName = GetBaseTypeName(dstType);

		writer.WriteLine(0, "%s %s(%s lhs, %s rhs)", dstName.c_str(), funcName.c_str(), lhsName.c_str(), rhsName.c_str());
		writer.WriteLine(0, "{");
		writer.WriteLine(1, "%s dst;", dstName.c_str());

		for (int r = 0; r < dstRows; r++)
		{
			for (int c = 0; c < dstCols; c++)
			{
				writer.Write("\tdst[%d][%d] = ", r, c);

				for (int i = 0; i < vecLen; i++)
				{
					if (i >= 1)
					{
						writer.Write(" + ");
					}
					writer.Write("lhs[%d][%d]*rhs[%d][%d]", r, i, i, c);
				}
				writer.Write(";\n");
			}
		}
		writer.WriteLine(1, "return dst;");
		writer.WriteLine(0, "}");
	}
	else if (lhsMatrix)
	{
		// if the matrix is on the left side, it's a matrix times a column vector
		// [ m00 m01 ][ x0 ] = [ b0 ]
		// [ m10 m11 ][ x1 ]   [ b1 ]
		// [ m20 m21 ]         [ b2 ]

		ASSERT_PARSER(!rhsMatrix);
		ASSERT_PARSER(rhsRows >= 2);
		ASSERT_PARSER(lhsCols == rhsRows);

		int dstRows = lhsRows;
		int vecLen = lhsCols;
		HLSLBaseType dstType = (HLSLBaseType)(dstScalarType + ((dstRows - 1) * 4) + (0));

		eastl::string dstName = GetBaseTypeName(dstType);

		writer.WriteLine(0, "%s %s(%s lhs, %s rhs)", dstName.c_str(), funcName.c_str(), lhsName.c_str(), rhsName.c_str());
		writer.WriteLine(0, "{");
		writer.WriteLine(1, "%s dst;", dstName.c_str());

		for (int r = 0; r < dstRows; r++)
		{
			writer.Write("\tdst[%d] = ", r);

			for (int i = 0; i < vecLen; i++)
			{
				if (i >= 1)
				{
					writer.Write(" + ");
				}
				writer.Write("lhs[%d][%d]*rhs[%d]", r, i, i);
			}
			writer.Write(";\n");
		}

		writer.WriteLine(1, "return dst;");
		writer.WriteLine(0, "}");

	}
	else if (rhsMatrix)
	{
		// if the matrix is on the right side, we assume everything is backwards, so
		// the vector is a row vector, not a column vector
		// [ x0 x1 ][ m00 m01 m02] = [ b0 b1 b2]
		//          [ m10 m11 m12] 
		// 

		ASSERT_PARSER(!lhsMatrix);
		ASSERT_PARSER(lhsRows >= 2);
		ASSERT_PARSER(lhsRows == rhsRows);

		int dstRows = rhsCols;
		int vecLen = rhsRows;
		HLSLBaseType dstType = (HLSLBaseType)(dstScalarType + ((dstRows - 1) * 4) + (0));

		eastl::string dstName = GetBaseTypeName(dstType);

		writer.WriteLine(0, "%s %s(%s lhs, %s rhs)", dstName.c_str(), funcName.c_str(), lhsName.c_str(), rhsName.c_str());
		writer.WriteLine(0, "{");

		writer.WriteLine(1, "%s dst;", dstName.c_str());

		for (int r = 0; r < dstRows; r++)
		{
			writer.Write("\tdst[%d] = ", r);

			for (int i = 0; i < vecLen; i++)
			{
				if (i >= 1)
				{
					writer.Write(" + ");
				}
				writer.Write("lhs[%d]*rhs[%d][%d]", i, i, r);
			}
			writer.Write(";\n");
		}

		writer.WriteLine(1, "return dst;");

		writer.WriteLine(0, "}");
	}
	else
	{
		// neither type is a matrix?
		ASSERT_PARSER(0);
	}

	writer.WriteLine(0, "");
}



GLSLGenerator::GLSLGenerator() :
    m_writer(/* writeFileNames= */ false)
{
    m_tree                      = NULL;
	m_entryName.Reset();
    m_target                    = Target_VertexShader;
    m_version                   = Version_140;
    m_versionLegacy             = false;
    m_inAttribPrefix.Reset();
    m_outAttribPrefix.Reset();
    m_error                     = false;
	m_matrixRowFunction.Reset();
	m_matrixCtorFunction.Reset();
    m_matrixMulFunction.Reset();
    m_clipFunction.Reset();
    m_tex2DlodFunction.Reset();
    m_tex2DbiasFunction.Reset();
    m_tex3DlodFunction.Reset();
    m_texCUBEbiasFunction.Reset();
	m_texCUBElodFunction.Reset();
	m_textureLodOffsetFunction.Reset();
	m_scalarSwizzle2Function.Reset();
    m_scalarSwizzle3Function.Reset();
    m_scalarSwizzle4Function.Reset();
    m_sinCosFunction.Reset();
	m_bvecTernary.Reset();
    m_outputPosition            = false;
    m_outputTypes.clear();
	m_outputGeometryType.Reset();

	m_f16tof32Function.Reset();
	m_f32tof16Function.Reset();
	m_stringLibrary = NULL;
}

bool GLSLGenerator::Generate(StringLibrary * stringLibrary, HLSLTree* tree, Target target, Version version, const char* entryName, const Options& options)
{
	m_stringLibrary = stringLibrary;

    m_tree      = tree;
    m_entryName = MakeCached(entryName);
    m_target    = target;
    m_version   = version;
    m_versionLegacy = (version == Version_110 || version == Version_100_ES);
    m_options   = options;


	m_domain.Reset();
	m_partitioning.Reset();
	m_outputtopology.Reset();
	m_patchconstantfunc.Reset();

	m_geoInputdataType.Reset();
	m_geoOutputdataType.Reset();

	m_StructuredBufferNames.clear();
	m_PushConstantBuffers.clear();

	//m_options.flags |= Flag_PackMatrixRowMajor;

	bool needs_f16tof32 = m_tree->NeedsFunction(MakeCached("f16tof32"));
	bool needs_f32tof16 = m_tree->NeedsFunction(MakeCached("f32tof16"));
	bool needs_clip = m_tree->NeedsFunction(MakeCached("clip"));
	bool needs_tex2Dlod = m_tree->NeedsFunction(MakeCached("tex2Dlod"));
	bool needs_tex2Dgrad = m_tree->NeedsFunction(MakeCached("tex2Dgrad"));
	bool needs_tex2Dbias = m_tree->NeedsFunction(MakeCached("tex2Dbias"));
	bool needs_tex2DMSfetch = m_tree->NeedsFunction(MakeCached("tex2DMSfetch"));

	bool needs_USE_SAMPLESS = m_tree->NeedsExtension(USE_SAMPLESS);
	// do we ever actually need this now that includes are removed in favor of mcpp?
	bool needs_USE_INCLUDE = m_tree->NeedsExtension(USE_INCLUDE);
	bool needs_USE_NonUniformResourceIndex = m_tree->NeedsExtension(USE_NonUniformResourceIndex);
	bool needs_USE_Subgroup_Basic = m_tree->NeedsExtension(USE_Subgroup_Basic);
	bool needs_USE_Subgroup_Quad = m_tree->NeedsExtension(USE_Subgroup_Quad);
	bool needs_USE_Subgroup_Ballot = m_tree->NeedsExtension(USE_Subgroup_Ballot);
	bool needs_USE_Subgroup_Arithmetic = m_tree->NeedsExtension(USE_Subgroup_Arithmetic);
	bool needs_USE_ControlFlowAttributes = m_tree->NeedsExtension(USE_ControlFlowAttributes);
	bool needs_tex3Dlod = m_tree->NeedsFunction(MakeCached("tex3Dlod"));
	bool needs_texCUBEbias = m_tree->NeedsFunction(MakeCached("texCUBEbias"));
	bool needs_texCUBElod = m_tree->NeedsFunction(MakeCached("texCUBElod"));

	bool needs_sincos = m_tree->NeedsFunction(MakeCached("sincos"));
	bool needs_asfloat = m_tree->NeedsFunction(MakeCached("asfloat"));
	bool needs_asuint = m_tree->NeedsFunction(MakeCached("asuint"));
	bool needs_asint = m_tree->NeedsFunction(MakeCached("asint"));

	AssignRegisters(m_tree->GetRoot(), m_options.shiftVec);

    ChooseUniqueName("matrix_row", m_matrixRowFunction);
    ChooseUniqueName("matrix_ctor", m_matrixCtorFunction);
    ChooseUniqueName("matrix_mul", m_matrixMulFunction);
    ChooseUniqueName("clip", m_clipFunction);
	ChooseUniqueName("f16tof32", m_f16tof32Function);
	ChooseUniqueName("f32tof16", m_f32tof16Function);

    ChooseUniqueName("tex2Dlod", m_tex2DlodFunction);
    ChooseUniqueName("tex2Dbias", m_tex2DbiasFunction);
    ChooseUniqueName("tex2Dgrad", m_tex2DgradFunction);
    ChooseUniqueName("tex3Dlod", m_tex3DlodFunction);
    ChooseUniqueName("texCUBEbias", m_texCUBEbiasFunction);
	ChooseUniqueName("texCUBElod", m_texCUBElodFunction);
	ChooseUniqueName("textureLodOffset", m_textureLodOffsetFunction);

	ChooseUniqueName("GetDimensions", m_getDimensions);
	ChooseUniqueName("MulMat", m_mulMatFunction);

    for (int i = 0; i < s_numReservedWords; ++i)
    {
        ChooseUniqueName( s_reservedWord[i], m_reservedWord[i]);
    }

    ChooseUniqueName("m_scalar_swizzle2", m_scalarSwizzle2Function);
    ChooseUniqueName("m_scalar_swizzle3", m_scalarSwizzle3Function);
    ChooseUniqueName("m_scalar_swizzle4", m_scalarSwizzle4Function);

    ChooseUniqueName("sincos", m_sinCosFunction);

	ChooseUniqueName( "bvecTernary", m_bvecTernary);

    if (target == Target_VertexShader)
    {
        m_inAttribPrefix  = MakeCached("");
        m_outAttribPrefix = MakeCached("vertOutput_");
    }
    else if (target == Target_FragmentShader)
    {
        m_inAttribPrefix  = MakeCached("fragInput_");
        m_outAttribPrefix = MakeCached("fragOutput_");
    }
	else if (target == Target_HullShader)
	{
		m_inAttribPrefix = MakeCached("tescInput_");
		m_outAttribPrefix = MakeCached("tescOutput_");
	}
	else if (target == Target_DomainShader)
	{
		m_inAttribPrefix = MakeCached("teseInput_");
		m_outAttribPrefix = MakeCached("teseOutput_");
	}
	else if (target == Target_GeometryShader)
	{
		m_inAttribPrefix = MakeCached("geomInput_");
		m_outAttribPrefix = MakeCached("geomOutput_");
	}
	else
	{
		m_inAttribPrefix = MakeCached("");
		m_outAttribPrefix = MakeCached("");
	}

    HLSLRoot* root = m_tree->GetRoot();
    HLSLStatement* statement = root->statement;

    // Find the entry point function.
    HLSLFunction* entryFunction = FindFunction(root, m_entryName);
    if (entryFunction == NULL)
    {
        Error("Entry point '%s' doesn't exist", FetchCstr(m_stringLibrary,m_entryName));
        return false;
    }

    if (m_version == Version_110)
    {
        m_writer.WriteLine(0, "#version 110");
    }
    else if (m_version == Version_140)
    {
        m_writer.WriteLine(0, "#version 140");

        // Pragmas for NVIDIA.
        m_writer.WriteLine(0, "#pragma optionNV(fastmath on)");
        //m_writer.WriteLine(0, "#pragma optionNV(fastprecision on)");
        m_writer.WriteLine(0, "#pragma optionNV(ifcvt none)");
        m_writer.WriteLine(0, "#pragma optionNV(inline all)");
        m_writer.WriteLine(0, "#pragma optionNV(strict on)");
        m_writer.WriteLine(0, "#pragma optionNV(unroll all)");
    }
    else if (m_version == Version_150)
    {
        m_writer.WriteLine(0, "#version 150");
    }
	else if (m_version == Version_450)
	{
		m_writer.WriteLine(0, "#version 450 core");
	}
    else if (m_version == Version_100_ES)
    {
        m_writer.WriteLine(0, "#version 100");
        m_writer.WriteLine(0, "precision highp float;");
    }
    else if (m_version == Version_300_ES)
    {
        m_writer.WriteLine(0, "#version 300 es");
        m_writer.WriteLine(0, "precision highp float;");
    }
    else
    {
        Error("Unrecognized target version");
        return false;
    }

	m_writer.WriteLine(0, "");

	// Output precision to supress warnings on glslangValidator
	m_writer.WriteLine(0, "precision highp float;\nprecision highp int; ");

    // Output the special function used to access rows in a matrix.
	/*
    m_writer.WriteLine(0, "vec3 %s(mat3 m, int i) { return vec3( m[0][i], m[1][i], m[2][i] ); }", m_matrixRowFunction);
    m_writer.WriteLine(0, "vec4 %s(mat4 m, int i) { return vec4( m[0][i], m[1][i], m[2][i], m[3][i] ); }", m_matrixRowFunction);
	*/

    // Output the special function used to do matrix cast for OpenGL 2.0
    if (m_version == Version_110)
    {
        m_writer.WriteLine(0, "mat3 %s(mat4 m) { return mat3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]); }", FetchCstr(m_stringLibrary, m_matrixCtorFunction));
    }

    // Output the special functions used for matrix multiplication lowering
    // They make sure glsl-optimizer can fold expressions better
	/*
    if (m_tree->NeedsFunction("mul") && (m_options.flags & Flag_LowerMatrixMultiplication))
    {
        m_writer.WriteLine(0, "vec2 %s(mat2 m, vec2 v) { return m[0] * v.x + m[1] * v.y; }", m_matrixMulFunction);
        m_writer.WriteLine(0, "vec2 %s(vec2 v, mat2 m) { return vec2(dot(m[0], v), dot(m[1], v)); }", m_matrixMulFunction);
        m_writer.WriteLine(0, "vec3 %s(mat3 m, vec3 v) { return m[0] * v.x + m[1] * v.y + m[2] * v.z; }", m_matrixMulFunction);
        m_writer.WriteLine(0, "vec3 %s(vec3 v, mat3 m) { return vec3(dot(m[0], v), dot(m[1], v), dot(m[2], v)); }", m_matrixMulFunction);
        m_writer.WriteLine(0, "vec4 %s(mat4 m, vec4 v) { return m[0] * v.x + m[1] * v.y + m[2] * v.z + m[3] * v.w; }", m_matrixMulFunction);
        m_writer.WriteLine(0, "vec4 %s(vec4 v, mat4 m) { return vec4(dot(m[0], v), dot(m[1], v), dot(m[2], v), dot(m[3], v)); }", m_matrixMulFunction);
    }
	*/

	///////////////////////////////////////////////////////////////////
	// Output the special function used to emulate HLSL clip.
	if (needs_f16tof32)
	{
		m_writer.WriteLine(0, "float %s(uint x) { return ((x & 0x8000) << 16) | (((x & 0x7c00) + 0x1C000) << 13) | ((x & 0x03FF) << 13); }", FetchCstr(m_stringLibrary, m_f16tof32Function));
	}

	if (needs_f32tof16)
	{		
		m_writer.WriteLine(0, "uint %s(float x) { uint f32 = floatBitsToUint(val); return ((f32 >> 16) & 0x8000) | ((((f32 & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) | ((f32 >> 13) & 0x03ff); }", FetchCstr(m_stringLibrary, m_f32tof16Function));
	}

	// Output the special function used to emulate HLSL clip.
	if (needs_clip)
	{
		const char* discard = m_target == Target_FragmentShader ? "discard" : "";
		m_writer.WriteLine(0, "void %s(float x) { if (x < 0.0) %s;  }", FetchCstr(m_stringLibrary, m_clipFunction), CHECK_CSTR(discard));
		m_writer.WriteLine(0, "void %s(vec2  x) { if (any(lessThan(x, vec2(0.0, 0.0)))) %s;  }", FetchCstr(m_stringLibrary, m_clipFunction), CHECK_CSTR(discard));
		m_writer.WriteLine(0, "void %s(vec3  x) { if (any(lessThan(x, vec3(0.0, 0.0, 0.0)))) %s;  }", FetchCstr(m_stringLibrary, m_clipFunction), CHECK_CSTR(discard));
		m_writer.WriteLine(0, "void %s(vec4  x) { if (any(lessThan(x, vec4(0.0, 0.0, 0.0, 0.0)))) %s;  }", FetchCstr(m_stringLibrary, m_clipFunction), CHECK_CSTR(discard));
	}

    // Output the special function used to emulate tex2Dlod.
    if (needs_tex2Dlod)
    {
        const char* function = "textureLod";

        if (m_version == Version_110)
        {
            m_writer.WriteLine(0, "#extension GL_ARB_shader_texture_lod : require");
            function = "texture2DLod";
        }
        else if (m_version == Version_100_ES)
        {
            m_writer.WriteLine(0, "#extension GL_EXT_shader_texture_lod : require");
            function = "texture2DLodEXT";
        }

        m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec4 texCoord) { return %s(samp, texCoord.xy, texCoord.w);  }", FetchCstr(m_stringLibrary, m_tex2DlodFunction), CHECK_CSTR(function));
    }

    // Output the special function used to emulate tex2Dgrad.
    if (needs_tex2Dgrad)
    {
        const char* function = "textureGrad";

        if (m_version == Version_110)
        {
            m_writer.WriteLine(0, "#extension GL_ARB_shader_texture_lod : require");
            function = "texture2DGradARB";
        }
        else if (m_version == Version_100_ES)
        {
            m_writer.WriteLine(0, "#extension GL_EXT_shader_texture_lod : require");
            function = "texture2DGradEXT";
        }

        m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec2 texCoord, vec2 dx, vec2 dy) { return %s(samp, texCoord, dx, dy);  }", FetchCstr(m_stringLibrary, m_tex2DgradFunction), CHECK_CSTR(function));
    }

    // Output the special function used to emulate tex2Dbias.
    if (needs_tex2Dbias)
    {
        if (target == Target_FragmentShader)
        {
            m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec4 texCoord) { return %s(samp, texCoord.xy, texCoord.w);  }", FetchCstr(m_stringLibrary, m_tex2DbiasFunction), CHECK_CSTR(m_versionLegacy ? "texture2D" : "texture") );
        }
        else
        {
            // Bias value is not supported in vertex shader.
            m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec4 texCoord) { return texture(samp, texCoord.xy);  }", FetchCstr(m_stringLibrary, m_tex2DbiasFunction) );
        }
    }

    // Output the special function used to emulate tex2DMSfetch.
    if (needs_tex2DMSfetch)
    {
        m_writer.WriteLine(0, "vec4 tex2DMSfetch(sampler2DMS samp, ivec2 texCoord, int sample) {");
        m_writer.WriteLine(1, "return texelFetch(samp, texCoord, sample);");
        m_writer.WriteLine(0, "}");
    }

	if (needs_USE_SAMPLESS)
	{
		m_writer.WriteLine(0, "#extension GL_EXT_samplerless_texture_functions : enable");

		m_writer.WriteLine(0, "");

		WriteOpenglGetDimensionsOverloads(m_writer, RawStr(m_getDimensions));
	}

	if (needs_USE_INCLUDE)
	{
		m_writer.WriteLine(0, "#extension GL_GOOGLE_include_directive : enable");
	}


	if (needs_USE_NonUniformResourceIndex)
	{
		m_writer.WriteLine(0, "#extension GL_ARB_shader_ballot : enable");

		m_writer.WriteLine(0, "uint NonUniformResourceIndex(uint textureIdx)");
		m_writer.WriteLine(0, "{");
		m_writer.WriteLine(1, "while (true)");
		m_writer.WriteLine(1, "{");
		m_writer.WriteLine(2, "uint currentIdx = readFirstInvocationARB(textureIdx);");
		m_writer.WriteLine(2, "if (currentIdx == textureIdx)");
		m_writer.WriteLine(3, "return currentIdx;");
		m_writer.WriteLine(1, "}");
		m_writer.WriteLine(1, "return 0;");
		m_writer.WriteLine(0, "}");
	}

	if (needs_USE_Subgroup_Basic)
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_basic : require");
	}

	if (needs_USE_Subgroup_Quad)
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_arithmetic : require");
	}

	if (needs_USE_Subgroup_Ballot)
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_ballot : require");
	}

	if (needs_USE_Subgroup_Arithmetic)
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_quad : require");
	}

	if (needs_USE_ControlFlowAttributes)
	{
		m_writer.WriteLine(0, "#extension GL_EXT_control_flow_attributes : require");
	}

    // Output the special function used to emulate tex3Dlod.
    if (needs_tex3Dlod)
    {
        m_writer.WriteLine(0, "vec4 %s(sampler3D samp, vec4 texCoord) { return %s(samp, texCoord.xyz, texCoord.w);  }", FetchCstr(m_stringLibrary, m_tex3DlodFunction), CHECK_CSTR(m_versionLegacy ? "texture3D" : "texture") );
    }

    // Output the special function used to emulate texCUBEbias.
    if (needs_texCUBEbias)
    {
        if (target == Target_FragmentShader)
        {
            m_writer.WriteLine(0, "vec4 %s(samplerCube samp, vec4 texCoord) { return %s(samp, texCoord.xyz, texCoord.w);  }", FetchCstr(m_stringLibrary, m_texCUBEbiasFunction), CHECK_CSTR(m_versionLegacy ? "textureCube" : "texture") );
        }
        else
        {
            // Bias value is not supported in vertex shader.
            m_writer.WriteLine(0, "vec4 %s(samplerCube samp, vec4 texCoord) { return texture(samp, texCoord.xyz);  }", FetchCstr(m_stringLibrary, m_texCUBEbiasFunction) );
        }
    }

	// Output the special function used to emulate texCUBElod
	if (needs_texCUBElod)
	{
        const char* function = "textureLod";

        if (m_version == Version_110)
        {
            m_writer.WriteLine(0, "#extension GL_ARB_shader_texture_lod : require");
            function = "textureCubeLod";
        }
        else if (m_version == Version_100_ES)
        {
            m_writer.WriteLine(0, "#extension GL_EXT_shader_texture_lod : require");
            function = "textureCubeLodEXT";
        }

		m_writer.WriteLine( 0, "vec4 %s(samplerCube samp, vec4 texCoord) { return %s(samp, texCoord.xyz, texCoord.w);  }", FetchCstr(m_stringLibrary, m_texCUBElodFunction), CHECK_CSTR(function));
	}

	/*
    m_writer.WriteLine(0, "vec2  %s(float x) { return  vec2(x, x); }", m_scalarSwizzle2Function);
    m_writer.WriteLine(0, "ivec2 %s(int   x) { return ivec2(x, x); }", m_scalarSwizzle2Function);

    m_writer.WriteLine(0, "vec3  %s(float x) { return  vec3(x, x, x); }", m_scalarSwizzle3Function);
    m_writer.WriteLine(0, "ivec3 %s(int   x) { return ivec3(x, x, x); }", m_scalarSwizzle3Function);

    m_writer.WriteLine(0, "vec4  %s(float x) { return  vec4(x, x, x, x); }", m_scalarSwizzle4Function);
    m_writer.WriteLine(0, "ivec4 %s(int   x) { return ivec4(x, x, x, x); }", m_scalarSwizzle4Function);
	*/
	
	/*
    if (!m_versionLegacy)
    {
        m_writer.WriteLine(0, "uvec2 %s(uint  x) { return uvec2(x, x); }", m_scalarSwizzle2Function);
        m_writer.WriteLine(0, "uvec3 %s(uint  x) { return uvec3(x, x, x); }", m_scalarSwizzle3Function);
        m_writer.WriteLine(0, "uvec4 %s(uint  x) { return uvec4(x, x, x, x); }", m_scalarSwizzle4Function);
    }
	*/

	
	
    if (needs_sincos)
    {
        const char* floatTypes[] = { "float", "vec2", "vec3", "vec4" };
        for (int i = 0; i < 4; ++i)
        {
            m_writer.WriteLine(0, "void %s(%s x, out %s s, out %s c) { s = sin(x); c = cos(x); }", FetchCstr(m_stringLibrary, m_sinCosFunction),
				CHECK_CSTR(floatTypes[i]), CHECK_CSTR(floatTypes[i]), CHECK_CSTR(floatTypes[i]));
        }
    }

	if (needs_asfloat)
	{
		m_writer.WriteLine(0, "%s %s(%s x) { return uintBitsToFloat(x); }", CHECK_CSTR("float"), CHECK_CSTR("asfloat"), CHECK_CSTR("uint"));
		m_writer.WriteLine(0, "%s %s(%s x) { return intBitsToFloat(x); }", CHECK_CSTR("float"), CHECK_CSTR("asfloat"), CHECK_CSTR("int"));
	}

	if (needs_asuint)
	{
		m_writer.WriteLine(0, "%s %s(%s x) { return floatBitsToUint(x); }", CHECK_CSTR("uint"), CHECK_CSTR("asuint"), CHECK_CSTR("float"));
		m_writer.WriteLine(0, "%s %s(%s x) { return (x); }", CHECK_CSTR("uint"), CHECK_CSTR("asuint"), CHECK_CSTR("int"));
		m_writer.WriteLine(0, "%s %s(%s x) { return (x); }", CHECK_CSTR("uint"), CHECK_CSTR("asuint"), CHECK_CSTR("uint"));
	}

	if (needs_asint)
	{
		m_writer.WriteLine(0, "%s %s(%s x) { return floatBitsToint(x); }", CHECK_CSTR("int"), CHECK_CSTR("asint"), CHECK_CSTR("float"));
		m_writer.WriteLine(0, "%s %s(%s x) { return (x); }", CHECK_CSTR("int"), CHECK_CSTR("asint"), CHECK_CSTR("int"));
		m_writer.WriteLine(0, "%s %s(%s x) { return (x); }", CHECK_CSTR("int"), CHECK_CSTR("asint"), CHECK_CSTR("uint"));
	}
	

	// special function to emulate ?: with bool{2,3,4} condition type
	/*
	m_writer.WriteLine( 0, "vec2 %s(bvec2 cond, vec2 trueExpr, vec2 falseExpr) { vec2 ret; ret.x = cond.x ? trueExpr.x : falseExpr.x; ret.y = cond.y ? trueExpr.y : falseExpr.y; return ret; }", m_bvecTernary );
	m_writer.WriteLine( 0, "vec3 %s(bvec3 cond, vec3 trueExpr, vec3 falseExpr) { vec3 ret; ret.x = cond.x ? trueExpr.x : falseExpr.x; ret.y = cond.y ? trueExpr.y : falseExpr.y; ret.z = cond.z ? trueExpr.z : falseExpr.z; return ret; }", m_bvecTernary );
	m_writer.WriteLine( 0, "vec4 %s(bvec4 cond, vec4 trueExpr, vec4 falseExpr) { vec4 ret; ret.x = cond.x ? trueExpr.x : falseExpr.x; ret.y = cond.y ? trueExpr.y : falseExpr.y; ret.z = cond.z ? trueExpr.z : falseExpr.z; ret.w = cond.w ? trueExpr.w : falseExpr.w; return ret; }", m_bvecTernary );
	*/

    // Output the extension used for dFdx/dFdy in GLES2
    if (m_version == Version_100_ES && (m_tree->NeedsFunction(MakeCached("ddx")) || m_tree->NeedsFunction(MakeCached("ddy"))))
    {
        m_writer.WriteLine(0, "#extension GL_OES_standard_derivatives : require");
    }

	// output the custom row-major matrix multiplies.
	{
		eastl::vector < HLSLBaseType > lhsTypeVec;
		eastl::vector < HLSLBaseType > rhsTypeVec;

		m_tree->FindMatrixMultiplyTypes(lhsTypeVec, rhsTypeVec);

		int num = (int)lhsTypeVec.size();
		ASSERT_PARSER(rhsTypeVec.size() == num);

		eastl::string funcName = RawStr(m_mulMatFunction);

		for (int iter = 0; iter < num; iter++)
		{
			HLSLBaseType lhsType = lhsTypeVec[iter];
			HLSLBaseType rhsType = rhsTypeVec[iter];

			eastl::string lhsName = GetBaseTypeName(lhsType);
			eastl::string rhsName = GetBaseTypeName(rhsType);

			WriteOpenglMatrixMultiplyVariation(m_writer, funcName, lhsType, rhsType, lhsName, rhsName);
		}
	}



	//m_writer.WriteLine(0, "#define VULKAN 1");

	m_writer.WriteLine(0, "");


	OutputAttributes(entryFunction);

	int attributeCounter = 0;

    if (m_target == Target_FragmentShader)
    {
        //if (!m_outputTargets)
        //    Error("Fragment shader must output a color\n");

		if (!m_versionLegacy)
		{
			for (int i = 0; i < m_outputTypes.size(); i++)
			{
				m_writer.Write("layout(location = %d) out ", attributeCounter++);

				switch (m_outputTypes[i])
				{
				case HLSLBaseType_Float:
					m_writer.Write("float");
					break;

				case HLSLBaseType_Float2:
					m_writer.Write("vec2");
					break;

				case HLSLBaseType_Float3:
					m_writer.Write("vec3");
					break;

				case HLSLBaseType_Float4:
					m_writer.Write("vec4");
					break;

				case HLSLBaseType_Half:
					m_writer.Write("mediump float");
					break;

				case HLSLBaseType_Half2:
					m_writer.Write("mediump vec2");
					break;

				case HLSLBaseType_Half3:
					m_writer.Write("mediump vec3");
					break;

				case HLSLBaseType_Half4:
					m_writer.Write("mediump vec4");
					break;

				case HLSLBaseType_Min16Float:
					m_writer.Write("mediump float");
					break;

				case HLSLBaseType_Min16Float2:
					m_writer.Write("mediump vec2");
					break;

				case HLSLBaseType_Min16Float3:
					m_writer.Write("mediump vec3");
					break;

				case HLSLBaseType_Min16Float4:
					m_writer.Write("mediump vec4");
					break;

				case HLSLBaseType_Min10Float:
					m_writer.Write("lowp float");
					break;

				case HLSLBaseType_Min10Float2:
					m_writer.Write("lowp vec2");
					break;

				case HLSLBaseType_Min10Float3:
					m_writer.Write("lowp vec3");
					break;

				case HLSLBaseType_Min10Float4:
					m_writer.Write("lowp vec4");
					break;

				case HLSLBaseType_Int:
					m_writer.Write("int");
					break;

				case HLSLBaseType_Int2:
					m_writer.Write("ivec2");
					break;

				case HLSLBaseType_Int3:
					m_writer.Write("ivec3");
					break;

				case HLSLBaseType_Int4:
					m_writer.Write("ivec4");
					break;


				case HLSLBaseType_Uint:
					m_writer.Write("uint");
					break;

				case HLSLBaseType_Uint2:
					m_writer.Write("uivec2");
					break;

				case HLSLBaseType_Uint3:
					m_writer.Write("uivec3");
					break;

				case HLSLBaseType_Uint4:
					m_writer.Write("uivec4");
					break;

				default:
					break;
				}

				m_writer.Write(" rast_FragData%d; ", i);
				m_writer.WriteLine(0, "");
			}
		}           
    }

	m_writer.WriteLine(0, "");	

	int counter = 0;
    OutputStatements(0, statement);
    OutputEntryCaller(entryFunction);

    m_tree = NULL;

    // The GLSL compilers don't check for this, so generate our own error message.	
    if (target == Target_VertexShader && !m_outputPosition)
    {
		//This can be a vertex shader for a Tesseleation or Geometry shader

        //Error("Vertex shader must output a position");
    }

    return !m_error;

}

const char* GLSLGenerator::GetResult() const
{
    return m_writer.GetResult();
}

void GLSLGenerator::OutputExpressionList(const eastl::vector<HLSLExpression*>& expressionVec, size_t start)
{
	for (size_t i = start; i < expressionVec.size(); i++)
	{
		if (i > start)
		{
			m_writer.Write(", ");
		}

		OutputExpression(expressionVec[i]);
	}
}

const HLSLType* commonScalarType(const HLSLType& lhs, const HLSLType& rhs)
{
    if (!isScalarType(lhs) || !isScalarType(rhs))
        return NULL;

    if (lhs.baseType == HLSLBaseType_Float || lhs.baseType == HLSLBaseType_Half || lhs.baseType == HLSLBaseType_Min16Float || lhs.baseType == HLSLBaseType_Min10Float ||
        rhs.baseType == HLSLBaseType_Float || rhs.baseType == HLSLBaseType_Half || rhs.baseType == HLSLBaseType_Min16Float || rhs.baseType == HLSLBaseType_Min10Float)
        return &kFloatType;

    if (lhs.baseType == HLSLBaseType_Uint || rhs.baseType == HLSLBaseType_Uint)
        return &kUintType;

    if (lhs.baseType == HLSLBaseType_Int || rhs.baseType == HLSLBaseType_Int)
        return &kIntType;

    if (lhs.baseType == HLSLBaseType_Bool || rhs.baseType == HLSLBaseType_Bool)
        return &kBoolType;

    return NULL;
}

void GLSLGenerator::OutputExpressionForBufferArray(HLSLExpression* expression, const HLSLType* dstType)
{
	if (expression->nodeType == HLSLNodeType_ArrayAccess)
	{
		if (expression->expressionType.baseType == HLSLBaseType_UserDefined && expression->expressionType.typeName.IsNotEmpty())
		{
			m_writer.Write(".%s_Data", FetchCstr(m_stringLibrary, expression->expressionType.typeName));
		}
	}
}

void GLSLGenerator::OutputExpression(HLSLExpression* expression, const HLSLType* dstType, bool allowCast)
{
	HLSLType expType = expression->expressionType;

    bool cast = dstType != NULL && !GetCanImplicitCast(expType, *dstType);
    if (expression->nodeType == HLSLNodeType_CastingExpression)
    {
        // No need to include a cast if the expression is already doing it.
        cast = false;
    }

	if (!allowCast)
	{
		cast = false;
	}

    if (cast)
    {
        OutputCast(*dstType);
        m_writer.Write("(");
    }

    HLSLBuffer* bufferAccess = (m_options.flags & Flag_EmulateConstantBuffer) ? GetBufferAccessExpression(expression) : 0;

    if (bufferAccess)
    {
        OutputBufferAccessExpression(bufferAccess, expression, expression->expressionType, 0);
    }
	else if (expression->nodeType == HLSLNodeType_InitListExpression)
	{
		HLSLInitListExpression* initList = static_cast<HLSLInitListExpression*>(expression);
		m_writer.Write("{");
		OutputExpressionList(initList->initExpressions);
		m_writer.Write("}");
	}
    else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
		HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
		OutputIdentifierExpression(identifierExpression);

		if (expression->functionExpression)
		{
			OutputExpression(expression->functionExpression);
		}
    }
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression* constructorExpression = static_cast<HLSLConstructorExpression*>(expression);
        m_writer.Write("%s(", CHECK_CSTR(GetTypeName(constructorExpression->expressionType)));
        OutputExpressionList(constructorExpression->params);
        m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_CastingExpression)
    {
        HLSLCastingExpression* castingExpression = static_cast<HLSLCastingExpression*>(expression);
        OutputCast(castingExpression->expressionType);
        m_writer.Write("(");
        OutputExpression(castingExpression->expression);
        m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_LiteralExpression)
    {
        HLSLLiteralExpression* literalExpression = static_cast<HLSLLiteralExpression*>(expression);
        switch (literalExpression->type)
        {
		case HLSLBaseType_Half:
		case HLSLBaseType_Float:
		case HLSLBaseType_Min16Float:
		case HLSLBaseType_Min10Float:
		{
			// Don't use printf directly so that we don't use the system locale.
			char buffer[64];
			String_FormatFloat(buffer, sizeof(buffer), literalExpression->fValue);
			m_writer.Write("%s", CHECK_CSTR(buffer));
		}
		break;
        case HLSLBaseType_Int:
            m_writer.Write("%d", literalExpression->iValue);
            break;
        case HLSLBaseType_Uint:
            m_writer.Write("%uu", literalExpression->uiValue);
	    break;
	case HLSLBaseType_Bool:
            m_writer.Write("%s", CHECK_CSTR(literalExpression->bValue ? "true" : "false"));
            break;
        default:
			ASSERT_PARSER(0);
        }
    }
    else if (expression->nodeType == HLSLNodeType_UnaryExpression)
    {
        HLSLUnaryExpression* unaryExpression = static_cast<HLSLUnaryExpression*>(expression);
        const char* op = "?";
        bool pre = true;
        const HLSLType* dstType = NULL;
        switch (unaryExpression->unaryOp)
        {
        case HLSLUnaryOp_Negative:      op = "-";  break;
        case HLSLUnaryOp_Positive:      op = "+";  break;
        case HLSLUnaryOp_Not:           op = "!";  dstType = &unaryExpression->expressionType; break;
        case HLSLUnaryOp_PreIncrement:  op = "++"; break;
        case HLSLUnaryOp_PreDecrement:  op = "--"; break;
        case HLSLUnaryOp_PostIncrement: op = "++"; pre = false; break;
        case HLSLUnaryOp_PostDecrement: op = "--"; pre = false; break;
        case HLSLUnaryOp_BitNot:        op = "~";  break;
        }
        m_writer.Write("(");
        if (pre)
        {
            m_writer.Write("%s", CHECK_CSTR(op));
            OutputExpression(unaryExpression->expression, dstType);
        }
        else
        {
            OutputExpression(unaryExpression->expression, dstType);
            m_writer.Write("%s", CHECK_CSTR(op));
        }
        m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_BinaryExpression)
    {
        HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(expression);
        const char* op = "?";
        const HLSLType* dstType1 = NULL;
        const HLSLType* dstType2 = NULL;

		bool vectorExpression = isVectorType( binaryExpression->expression1->expressionType ) || isVectorType( binaryExpression->expression2->expressionType );
		if( vectorExpression && isCompareOp( binaryExpression->binaryOp ))
		{
			switch (binaryExpression->binaryOp)
			{
			case HLSLBinaryOp_Less:         m_writer.Write("lessThan(");			break;
			case HLSLBinaryOp_Greater:      m_writer.Write("greaterThan(");			break;
			case HLSLBinaryOp_LessEqual:    m_writer.Write("lessThanEqual(");		break;
			case HLSLBinaryOp_GreaterEqual: m_writer.Write("greaterThanEqual(");	break;
			case HLSLBinaryOp_Equal:        m_writer.Write("equal(");				break;
			case HLSLBinaryOp_NotEqual:     m_writer.Write("notEqual(");			break;
			default:
				ASSERT_PARSER(0); // is so, check isCompareOp
			}

			if( isVectorType( binaryExpression->expression1->expressionType ) && isScalarType( binaryExpression->expression2->expressionType ) )
				dstType2 = &binaryExpression->expression1->expressionType;
			else if( isScalarType( binaryExpression->expression1->expressionType ) && isVectorType( binaryExpression->expression2->expressionType ) )
				dstType1 = &binaryExpression->expression2->expressionType;
			// TODO if both expressions are vector but with different dimension handle it here or in parser?

			OutputExpression(binaryExpression->expression1, dstType1);
			m_writer.Write(", ");
			OutputExpression(binaryExpression->expression2, dstType2);
			m_writer.Write(")");
		}
		else
		{
			switch (binaryExpression->binaryOp)
			{
			case HLSLBinaryOp_Add:          op = " + "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_Sub:          op = " - "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_Mul:          op = " * "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_Div:          op = " / "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_Less:         op = " < "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_Greater:      op = " > "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_LessEqual:    op = " <= "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_GreaterEqual: op = " >= "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_Equal:        op = " == "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_NotEqual:     op = " != "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_Assign:       op = " = ";  dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_AddAssign:    op = " += "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_SubAssign:    op = " -= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_MulAssign:    op = " *= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_DivAssign:    op = " /= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_BitAndAssign: op = " &= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_BitOrAssign:  op = " |= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_BitXorAssign: op = " ^= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_And:          op = " && "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_Or:           op = " || "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_BitAnd:       op = " & "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_BitOr:		op = " | "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_BitXor:		op = " ^ "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_LeftShift:    op = " << "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_RightShift:   op = " >> "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_Modular:      op = " % "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_Comma:        op = " , "; dstType1 = &binaryExpression->expression1->expressionType; dstType2 = &binaryExpression->expression2->expressionType; break;

			default:
				ASSERT_PARSER(0);
			}

			// Exception Handling for imageStore, this might make errors
			// Need to change better form
			HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(binaryExpression->expression1);
			if ((binaryExpression->binaryOp == HLSLBinaryOp_Assign) && (binaryExpression->expression1->nodeType == HLSLArrayAccess::s_type) && IsRWTexture(arrayAccess->array->expressionType.baseType))
			{
				m_writer.Write("imageStore(");

				OutputExpression(arrayAccess->array);

				switch (arrayAccess->array->expressionType.baseType)
				{
				case HLSLBaseType_RWTexture1D:
					m_writer.Write(", int(");
					break;
				case HLSLBaseType_RWTexture1DArray:
					m_writer.Write(", ivec2(");
					break;
				case HLSLBaseType_RWTexture2D:
					m_writer.Write(", ivec2(");
					break;
				case HLSLBaseType_RWTexture2DArray:
					m_writer.Write(", ivec3(");
					break;
				case HLSLBaseType_RWTexture3D:
					m_writer.Write(", ivec3(");
					break;
				default:
					break;
				}

				OutputExpression(arrayAccess->index);

				m_writer.Write("), ");

				HLSLBaseType elementType = GetScalarBaseType(arrayAccess->expressionType.baseType);
				HLSLBaseType vecType = HLSLBaseType(elementType + 3 * 4 + 0);
				eastl::string elementName = GetBaseTypeName(vecType);

				switch (binaryExpression->expression2->expressionType.baseType)
				{
				case HLSLBaseType_Float:
				case HLSLBaseType_Half:
				case HLSLBaseType_Min16Float:
				case HLSLBaseType_Min10Float:
					m_writer.Write("%s(", elementName.c_str());
					OutputExpression(binaryExpression->expression2);
					m_writer.Write(", 0.0, 0.0, 0.0)");
					break;
				case HLSLBaseType_Float2:
				case HLSLBaseType_Half2:
				case HLSLBaseType_Min16Float2:
				case HLSLBaseType_Min10Float2:
					m_writer.Write("%s(", elementName.c_str());
					OutputExpression(binaryExpression->expression2);
					m_writer.Write(", 0.0, 0.0)");
					break;
				case HLSLBaseType_Float3:
				case HLSLBaseType_Half3:
				case HLSLBaseType_Min16Float3:
				case HLSLBaseType_Min10Float3:
					m_writer.Write("%s(",elementName.c_str());
					OutputExpression(binaryExpression->expression2);
					m_writer.Write(", 0.0)");
					break;
				case HLSLBaseType_Float4:
				case HLSLBaseType_Half4:
				case HLSLBaseType_Min16Float4:
				case HLSLBaseType_Min10Float4:
					m_writer.Write("%s(", elementName.c_str());
					OutputExpression(binaryExpression->expression2);
					m_writer.Write(")");
					break;
				default:
					m_writer.Write("%s(", elementName.c_str());
					OutputExpression(binaryExpression->expression2);
					m_writer.Write(")");
					break;
				}
				m_writer.Write(")");
			}
			else
			{
				m_writer.Write("(");
				OutputExpression(binaryExpression->expression1, dstType1);
				m_writer.Write("%s", CHECK_CSTR(op));
				OutputExpression(binaryExpression->expression2, dstType2);
				m_writer.Write(")");
			}
		}
	}
	else if (expression->nodeType == HLSLNodeType_ConditionalExpression)
	{
		HLSLConditionalExpression* conditionalExpression = static_cast<HLSLConditionalExpression*>(expression);
		if( isVectorType( conditionalExpression->condition->expressionType ) )
		{
			m_writer.Write( "%s", FetchCstr(m_stringLibrary, m_bvecTernary) );
			m_writer.Write( "( " );
			OutputExpression( conditionalExpression->condition );
			m_writer.Write( ", " );
			OutputExpression( conditionalExpression->trueExpression, &conditionalExpression->expressionType );
			m_writer.Write( ", " );
			OutputExpression( conditionalExpression->falseExpression, &conditionalExpression->expressionType  );
			m_writer.Write( " )" );
		}
		else
		{
			m_writer.Write( "((" );
			OutputExpression( conditionalExpression->condition, &kBoolType );
			m_writer.Write( ")?(" );
			OutputExpression( conditionalExpression->trueExpression, dstType );
			m_writer.Write( "):(" );
			OutputExpression( conditionalExpression->falseExpression, dstType );
			m_writer.Write( "))" );
		}
	}
	else if (expression->nodeType == HLSLNodeType_MemberAccess)
	{
		HLSLMemberAccess* memberAccess = static_cast<HLSLMemberAccess*>(expression);

		HLSLStruct* structure = FindStruct(m_tree->GetRoot(), memberAccess->object->expressionType.typeName);

		bool bTessFactor = false;
		bool bGeometryFactor = false;

		if (structure)
		{
			if (String_Equal(structure->field->semantic, "SV_TessFactor") || String_Equal(structure->field->semantic, "SV_InsideTessFactor"))
				bTessFactor = true;
			//now just compare with first field, need to change later
			else if ( m_target == Target_GeometryShader && (String_Equal(structure->field->semantic, "SV_POSITION") || String_Equal(structure->field->semantic, "SV_Position")))
				bGeometryFactor = true;
			
		}

		if (memberAccess->object->expressionType.baseType == HLSLBaseType_InputPatch || memberAccess->object->expressionType.baseType == HLSLBaseType_OutputPatch)
		{
			ASSERT_PARSER(structure != NULL);
			
			
			HLSLStructField* field = structure->field;
			while (field != NULL)
			{
				if (String_Equal(field->name, memberAccess->field))
				{
					//find it from input
					char attribName[64];
					String_Printf(attribName, 64, "%s%s", FetchCstr(m_stringLibrary, m_inAttribPrefix), FetchCstr(m_stringLibrary, field->semantic));
					m_writer.Write("%s", CHECK_CSTR(attribName));

					if (m_target == Target_HullShader)
					{
						//Every input for hull shader should be array
						m_writer.Write("[0]");
					}

					break;
				}				
				field = field->nextField;
			}
		}
		else if (bTessFactor)
		{
			HLSLStructField* field = structure->field;
			while (field != NULL)
			{
				if (String_Equal(field->name, memberAccess->field))
				{
					if (String_Equal(field->semantic, "SV_TessFactor"))
					{
						m_writer.Write("gl_TessLevelOuter");
					}
					else if (String_Equal(field->semantic, "SV_InsideTessFactor"))
					{
						m_writer.Write("gl_TessLevelInner");
					}

					break;
				}
				
				field = field->nextField;
			}
		}
		else if (bGeometryFactor)
		{
			HLSLStructField* field = structure->field;
			while (field != NULL)
			{
				if (String_Equal(field->name, memberAccess->field))
				{
					if ( (String_Equal(field->semantic, "SV_POSITION") || String_Equal(field->semantic, "SV_Position")) && String_Equal(m_geoOutputdataType, structure->name))
					{						
						m_writer.Write("gl_Position");
					}
					else if (String_Equal(structure->name, m_geoOutputdataType))
					{
						m_writer.Write("%s%s", FetchCstr(m_stringLibrary, m_outAttribPrefix), FetchCstr(m_stringLibrary, field->semantic));
					}
					else
					{
						m_writer.Write("(");
						OutputExpression(memberAccess->object);
						m_writer.Write(")");
						m_writer.Write(".%s", FetchCstr(m_stringLibrary, memberAccess->field));
					}

					break;
				}

				field = field->nextField;
			}
		}
		else if (memberAccess->object->expressionType.baseType == HLSLBaseType_PointStream ||
				 memberAccess->object->expressionType.baseType == HLSLBaseType_LineStream ||
				 memberAccess->object->expressionType.baseType == HLSLBaseType_TriangleStream )
		{
			if (String_Equal(memberAccess->field, "Append"))
			{
				m_writer.Write("EmitVertex()");
			}
		}
        else if (memberAccess->object->expressionType.baseType == HLSLBaseType_Half  ||
            memberAccess->object->expressionType.baseType == HLSLBaseType_Float ||
			memberAccess->object->expressionType.baseType == HLSLBaseType_Min16Float ||
			memberAccess->object->expressionType.baseType == HLSLBaseType_Min10Float ||
            memberAccess->object->expressionType.baseType == HLSLBaseType_Int   ||
            memberAccess->object->expressionType.baseType == HLSLBaseType_Uint)
        {
            m_writer.Write("(");
            OutputExpression(memberAccess->object);
            m_writer.Write(")");

			m_writer.Write(".%s", FetchCstr(m_stringLibrary, memberAccess->field));
        }
        else
        {
            m_writer.Write("(");
            OutputExpression(memberAccess->object);
            m_writer.Write(")");

			if( memberAccess->object->expressionType.baseType == HLSLBaseType_Float2x2 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Float3x3 ||
                memberAccess->object->expressionType.baseType == HLSLBaseType_Float4x4 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Half2x2 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Half3x3 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Half4x4 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Min16Float2x2 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Min16Float3x3 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Min16Float4x4 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Min10Float2x2 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Min10Float3x3 ||
				memberAccess->object->expressionType.baseType == HLSLBaseType_Min10Float4x4 )
            {
                // Handle HLSL matrix "swizzling".
                // TODO: Properly handle multiple element selection such as _m00_m12
                const char* n = RawStr(memberAccess->field);
                while (n[0] != 0)
                {
                    if ( n[0] != '_' )
                    {
						ASSERT_PARSER(0);
                        break;
                    }
                    ++n;
                    char base = '1';
                    if (n[0] == 'm')
                    {
                        base = '0';
                        ++n;
                    }
                    if (isdigit(n[0]) && isdigit(n[1]) )
                    {
                        m_writer.Write("[%d][%d]", n[1] - base, n[0] - base);
                        n += 2;
                    }
                    else
                    {
						ASSERT_PARSER(0);
                        break;
                    }
                }
            }
            else
            {
                m_writer.Write(".%s", FetchCstr(m_stringLibrary, memberAccess->field));
            }

        }

    }
    else if (expression->nodeType == HLSLNodeType_ArrayAccess)
    {
        HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);

		HLSLArrayAccess* arrayAccessInMatrix;

		if (arrayAccess->array->nodeType == HLSLNodeType_ArrayAccess)
			arrayAccessInMatrix = static_cast<HLSLArrayAccess*>(arrayAccess->array);
		else
			arrayAccessInMatrix = NULL;

		if (arrayAccessInMatrix != NULL && !arrayAccessInMatrix->array->expressionType.array &&
			(	arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Float2x2 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Float3x3 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Float4x4 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Half2x2 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Half3x3 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Half4x4 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Min16Float2x2 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Min16Float3x3 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Min16Float4x4 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Min10Float2x2 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Min10Float3x3 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Min10Float4x4)
			)
		{
			// GLSL access a matrix as m[c][r] while HLSL is m[r][c], so use our
			// special row access function to convert.
			//OutputExpression(arrayAccessInMatrix->array);
				
			//Disable this for coherence
			/*
			HLSLLiteralExpression* literalExpressionCol = static_cast<HLSLLiteralExpression*>(arrayAccessInMatrix->index);
			HLSLLiteralExpression* literalExpressionRow = static_cast<HLSLLiteralExpression*>(arrayAccess->index);

			if (literalExpressionRow != NULL)
			{
				//Transpose
				int c = literalExpressionCol->iValue;
				int r = literalExpressionRow->iValue;

				//Print Col
				m_writer.Write("[");
				m_writer.Write("%d", r);
				m_writer.Write("]");

				//Print Row
				m_writer.Write("[");
				m_writer.Write("%d", c);
				m_writer.Write("]");
			}
			else
			{
				m_writer.Write("%s(", m_matrixRowFunction);
				OutputExpression(arrayAccess->array);
				m_writer.Write(",");
				OutputExpression(arrayAccess->index);
				m_writer.Write(")");
			}
			*/
					

			OutputExpression(arrayAccess->array);

			m_writer.Write("[");
			OutputExpression(arrayAccess->index);
			m_writer.Write("]");
						
		}
		else if (IsTexture(arrayAccess->array->expressionType))
		{
			bool isImageLoad = false;
			if (HLSLBaseType_RWTexture1D <= arrayAccess->array->expressionType.baseType  &&
				HLSLBaseType_RWTexture3D >= arrayAccess->array->expressionType.baseType)
			{
				isImageLoad = true;
			}

			m_writer.Write(isImageLoad ? "imageLoad(" : "texelFetch(");

			OutputExpression(arrayAccess->array);

			m_writer.Write(", ");

			switch (arrayAccess->array->expressionType.baseType)
			{

			case HLSLBaseType_Texture1D:
			case HLSLBaseType_RWTexture1D:

				m_writer.Write("(");
				OutputExpression(arrayAccess->index);

				////offset
				//if (arrayAccess->index->nextExpression && arrayAccess->index->nextExpression->nextExpression)
				//{
				//	m_writer.Write("+");
				//	OutputExpression(arrayAccess->index->nextExpression->nextExpression);
				//}

				m_writer.Write(").x, ");


				if (!isImageLoad)
				{
					//if (arrayAccess->index->nextExpression)
					//{
					//	m_writer.Write("(");
					//	OutputExpression(arrayAccess->index->nextExpression);
					//	m_writer.Write(").y");
					//}
					//else
						m_writer.Write("0");
				}						
				break;

			case HLSLBaseType_Texture1DArray:
			case HLSLBaseType_RWTexture1DArray:

				m_writer.Write("ivec2(");
				OutputExpression(arrayAccess->index);
				m_writer.Write(").xy, ");


				if (!isImageLoad)
				{
					//if (arrayAccess->index->nextExpression)
					//{
					//	m_writer.Write("(");
					//	OutputExpression(arrayAccess->index->nextExpression);
					//	m_writer.Write(").z");
					//}
					//else
						m_writer.Write("0");
				}
						
				break;

			case HLSLBaseType_Texture2D:
			case HLSLBaseType_RWTexture2D:

				m_writer.Write("ivec2(");
				OutputExpression(arrayAccess->index);
				m_writer.Write(").xy");

				//offset
				//if (arrayAccess->index->nextExpression && arrayAccess->index->nextExpression->nextExpression)
				//{
				//	m_writer.Write("+");
				//	OutputExpression(arrayAccess->index->nextExpression->nextExpression);
				//}

				if (!isImageLoad)
				{
					m_writer.Write(", ");

					//if (arrayAccess->index->nextExpression)
					//{
					//	m_writer.Write("(");
					//	OutputExpression(arrayAccess->index->nextExpression);
					//	m_writer.Write(").z");
					//}
					//else
						m_writer.Write("0");
				}
				break;

			case HLSLBaseType_Texture2DMS:
						
				m_writer.Write("ivec2(");
				OutputExpression(arrayAccess->index);
				m_writer.Write(").xy");

				//offset

				//if (arrayAccess->index->nextExpression == NULL)
				//{
					m_writer.Write("+ ivec2(0, 0), 1)");
				//}
				//else
				//{
				//	if (arrayAccess->index->nextExpression->nextExpression)
				//	{
				//		m_writer.Write("+");
				//		OutputExpression(arrayAccess->index->nextExpression->nextExpression);
				//	}

				//	if (!isImageLoad)
				//	{
				//		m_writer.Write(", ");

				//		m_writer.Write("int(");
				//		OutputExpression(arrayAccess->index->nextExpression);
				//		m_writer.Write(")");
				//	}
				//}

						
				break;

			case HLSLBaseType_Texture2DArray:
			case HLSLBaseType_RWTexture2DArray:

				m_writer.Write("ivec3(");
				OutputExpression(arrayAccess->index);
				m_writer.Write(").xyz, ");


				if (!isImageLoad)
				{
					//if (arrayAccess->index->nextExpression)
					//{
					//	m_writer.Write("(");
					//	OutputExpression(arrayAccess->index->nextExpression);
					//	m_writer.Write(").w");
					//}
					//else
						m_writer.Write("0");
				}						
				break;

			//case HLSLBaseType_Texture2DMSArray:


			//	m_writer.Write("ivec3(");
			//	OutputExpression(arrayAccess->index);
			//	m_writer.Write(").xyz, ");

			//	m_writer.Write("int(");
			//	OutputExpression(arrayAccess->index->nextExpression);
			//	m_writer.Write(")");
			//	break;

			case HLSLBaseType_Texture3D:
			case HLSLBaseType_RWTexture3D:

				m_writer.Write("ivec3(");
				OutputExpression(arrayAccess->index);
				m_writer.Write(").xyz");

				//offset
				//if (arrayAccess->index->nextExpression && arrayAccess->index->nextExpression->nextExpression)
				//{
				//	m_writer.Write("+");
				//	OutputExpression(arrayAccess->index->nextExpression->nextExpression);
				//}

				m_writer.Write(", ");


				if (!isImageLoad)
				{
					//if (arrayAccess->index->nextExpression)
					//{
					//	m_writer.Write("(");
					//	OutputExpression(arrayAccess->index->nextExpression);
					//	m_writer.Write(").w");
					//}
					//else
						m_writer.Write("0");
				}						
				break;
			default:
				break;
			}

			m_writer.Write(")");
		}
        else
        {
			OutputExpression(arrayAccess->array);

			m_writer.Write("[");
			OutputExpression(arrayAccess->index);
			m_writer.Write("]");

			if (arrayAccess->identifier.IsNotEmpty())
			{
				m_writer.Write(".%s_Data", FetchCstr(m_stringLibrary, arrayAccess->identifier));
			}
        }
    }
    else if (expression->nodeType == HLSLNodeType_FunctionCall)
    {
		HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(expression);
		const eastl::vector<HLSLExpression*>& params = functionCall->params;
		const eastl::vector <HLSLArgument*>& args = functionCall->function->args;

        // Handle intrinsic funtions that are different between HLSL and GLSL.
        CachedString functionName = functionCall->function->name;
        if (String_Equal(functionName, "mul"))
        {
			ASSERT_PARSER(params.size() == 2);
			ASSERT_PARSER(args.size() == 2);

			const HLSLType& type0 = args[0]->type;
			const HLSLType& type1 = args[1]->type;

			bool isCustom = HLSLTree::IsCustomMultiply(type0.baseType, type1.baseType);

			if (isCustom)
			{
				eastl::string matrixFunc = FetchCstr(m_stringLibrary, m_mulMatFunction);

				m_writer.Write("%s(", CHECK_CSTR(matrixFunc.c_str()));
				OutputExpression(params[0]);
				m_writer.Write(",");
				OutputExpression(params[1]);
				m_writer.Write(")");
			}
			else
			{
				// standard multiply
				m_writer.Write("(");
				OutputExpression(params[0]);
				m_writer.Write(")*(");
				OutputExpression(params[1]);
				m_writer.Write(")");
			}
			// These functions don't work any more because they rely on other code that has been removed.
			// The original code also wasn't robust. It only handled floats (not halfs or other types), and it
			// didn't work with layout(row_major). The original code was based on taking column major code and
			// applying transposes. But it's much cleaner to just switch everything to row major so it matches
			// HLSL.
#if 0
			eastl::string matrixFunc = FetchCstr(m_stringLibrary, m_matrixMulFunction);
            const char* prefix = (m_options.flags & Flag_LowerMatrixMultiplication) ? matrixFunc.c_str() : "";
            const char* infix = (m_options.flags & Flag_LowerMatrixMultiplication) ? "," : "*";

            if (m_options.flags & Flag_PackMatrixRowMajor)
            {
                m_writer.Write("%s((", CHECK_CSTR(prefix));
                OutputExpression(argument[1], &type1);
                m_writer.Write(")%s(", CHECK_CSTR(infix));
                OutputExpression(argument[0], &type0);
                m_writer.Write("))");
            }
            else
            {
                m_writer.Write("%s((", CHECK_CSTR(prefix));
                OutputExpression(argument[0], &type0);
                m_writer.Write(")%s(", CHECK_CSTR(infix));
                OutputExpression(argument[1], &type1);
                m_writer.Write("))");
            }
#endif
        }
        else if (String_Equal(functionName, "saturate"))
        {
			ASSERT_PARSER(params.size() == 1);

			m_writer.Write("clamp(");
            OutputExpression(params[0]);
            m_writer.Write(", 0.0, 1.0)");
        }
		else if (String_Equal(functionName, "rcp"))
		{
			ASSERT_PARSER(params.size() == 1);

			m_writer.Write("1 / ");
			OutputExpression(params[0]);
		}
		else if (String_Equal(functionName, "rsqrt"))
		{
			ASSERT_PARSER(params.size() == 1);

			m_writer.Write("inversesqrt(");
			OutputExpression(params[0]);
			m_writer.Write(")");
		}
		else if (String_Equal(functionName, "any") || String_Equal(functionName, "all"))
		{
			ASSERT_PARSER(params.size() == 1);

			HLSLBaseType currType = params[0]->expressionType.baseType;

			// if it is a bool type, just do regular any()
			if (HLSLBaseType_Bool <= currType && currType <= HLSLBaseType_Bool4x4)
			{
				m_writer.Write("%s", RawStr(functionName));
				m_writer.Write("(");
				OutputExpression(params[0]);
				m_writer.Write(")");
			}
			else
			{
				m_writer.Write("%s", RawStr(functionName));
				m_writer.Write("(");
				m_writer.Write("notEqual");
				m_writer.Write("(");
				OutputExpression(params[0]);
				m_writer.Write(",");

				const char * strType = GetTypeName(params[0]->expressionType);

				m_writer.Write("%s(0)",strType);
				m_writer.Write(")");
				m_writer.Write(")");
			}
		}
		else if (String_Equal(functionName, "pow"))
		{
			ASSERT_PARSER(params.size() == 2);

			m_writer.Write("pow(");
			OutputExpression(params[0]);
			m_writer.Write(",");

			eastl::string type = getElementTypeAsStrGLSL(m_stringLibrary, params[0]->expressionType);
			if (!String_Equal(type, "UnknownElementType"))
			{
				m_writer.Write("%s", type.c_str());
				m_writer.Write("(");
			}
			OutputExpression(params[1]);
			if (!String_Equal(type, "UnknownElementType"))
			{
				m_writer.Write(")");
			}

			m_writer.Write(")");
		}
		else if (String_Equal(functionName, "WaveGetLaneIndex"))
		{
			m_writer.Write("gl_SubgroupInvocationID");
		}
		else if (String_Equal(functionName, "WaveGetLaneCount"))
		{
			m_writer.Write("gl_SubgroupSize");
		}
		else if (String_Equal(functionName, "Sample") || String_Equal(functionName, "SampleLevel") || String_Equal(functionName, "SampleCmp") ||
			String_Equal(functionName, "SampleCmpLevelZero") || String_Equal(functionName, "SampleBias") || String_Equal(functionName, "GatherRed") || String_Equal(functionName, "SampleGrad"))
		{
			ASSERT_PARSER(params.size() >= 3);

			bool compareFunc = String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero");
			if (String_Equal(functionName, "Sample") || compareFunc)
			{
				m_writer.Write("texture(");
			}
			else if (String_Equal(functionName, "SampleLevel"))
			{
				// If we have 3 arguments, it's textureLod. If we have 4, it's textureLodOffset.
				
				if (args.size() == 4)
				{
					m_writer.Write("textureLod(");
				}
				else if (args.size() == 5)
				{
					m_writer.Write("textureLodOffset(");
				}
				else
				{
					ASSERT_PARSER(0);
				}
			}
			else if (String_Equal(functionName, "SampleBias"))
			{
				m_writer.Write("textureOffset(");
			}
			else if (String_Equal(functionName, "GatherRed"))
			{
				m_writer.Write("textureGather(");
			}
			else if (String_Equal(functionName, "SampleGrad"))
			{
				m_writer.Write("textureGrad(");
			}

			HLSLBaseType textureType =  params[0]->expressionType.baseType;

			switch (textureType)
			{
			case HLSLBaseType_Texture1D:
				m_writer.Write("sampler1D");
				break;
			case HLSLBaseType_Texture1DArray:
				m_writer.Write("sampler1DArray");
				break;
			case HLSLBaseType_Texture2D:
				if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
				{
					m_writer.Write("sampler2DShadow");
				}
				else
				{
					m_writer.Write("sampler2D");
				}
				break;
			case HLSLBaseType_Texture2DArray:
				if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
				{
					m_writer.Write("sampler2DArrayShadow");
				}
				else
				{
					m_writer.Write("sampler2DArray");
				}
				break;
			case HLSLBaseType_Texture3D: 
				m_writer.Write("sampler3D");
				break;
			case HLSLBaseType_Texture2DMS:
				m_writer.Write("sampler2DMS");
				break;
			case HLSLBaseType_Texture2DMSArray:
				m_writer.Write("sampler2DMSArray");
				break;
			case HLSLBaseType_TextureCube:
				m_writer.Write("samplerCube");
				break;
			case HLSLBaseType_TextureCubeArray:
				if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
				{
					m_writer.Write("samplerCubeArrayShadow");
				}
				else
				{
					m_writer.Write("samplerCubeArray");
				}
				break;
			default:
				break;
			}

			m_writer.Write("(");
			OutputExpression(params[0]);
			m_writer.Write(", ");
			OutputExpression(params[1]);
			m_writer.Write("), ");

			size_t start = 2;
			if (compareFunc && textureType != HLSLBaseType_TextureCubeArray)
			{
				start = 4;
				switch (params[0]->expressionType.baseType)
				{
				case HLSLBaseType_Texture1D:
					m_writer.Write("vec3(");
					break;
				case HLSLBaseType_Texture1DArray:
					m_writer.Write("vec3(");
					break;
				case HLSLBaseType_Texture2D:
					m_writer.Write("vec3(");
					break;
				case HLSLBaseType_Texture2DArray:
					m_writer.Write("vec4(");
					break;
				case HLSLBaseType_TextureCube:
					m_writer.Write("vec4(");
					break;
				default:
					ASSERT_PARSER(false);
				}
				OutputExpression(params[2]);
				m_writer.Write(textureType==HLSLBaseType_Texture1D ? "0, " :", ");
				OutputExpression(params[3]);
				m_writer.Write(")");
			}

			OutputExpressionList(params, start);

			m_writer.Write(")");
		}
		else if (String_Equal(functionName, "GetDimensions"))
		{
			ASSERT_PARSER(params.size()>=2);
			HLSLType* expectedType = &args[1]->type;

			//if it is from texture
			if (IsTextureType(params[0]->expressionType.baseType))
			{
				m_writer.Write("%s", FetchCstr(m_stringLibrary, m_getDimensions));
				m_writer.Write("(");

				OutputExpression(params[0]);

				for (int i = 1; i < params.size(); i++)
				{
					m_writer.Write(",");
					OutputExpression(params[i], expectedType, false);
				}
				m_writer.Write(")");
			}
			else
			{
				// TODO: error or unhandled
				ASSERT_PARSER(0);
			}
		}
		/// !!! need to check it later
		else if (String_Equal(functionName, "Load"))
		{
			ASSERT_PARSER(params.size()>=2);

			if (IsTexture(params[0]->expressionType.baseType))
			{
				m_writer.Write("texelFetch(");

				OutputExpression(params[0]);

				m_writer.Write(", ");

				switch (params[0]->expressionType.baseType)
				{
					
				case HLSLBaseType_Texture1D:
					
					m_writer.Write("(");
					OutputExpression(params[1]);
					m_writer.Write(").x, ");

					m_writer.Write("(");
					OutputExpression(params[1]);
					m_writer.Write(").y");
					break;

				case HLSLBaseType_Texture1DArray:
						
					m_writer.Write("ivec2(");
					OutputExpression(params[1]);
					m_writer.Write(").xy, ");

					m_writer.Write("(");
					OutputExpression(params[1]);
					m_writer.Write(").z");
					break;

				case HLSLBaseType_Texture2D:
						
					m_writer.Write("ivec2(");
					OutputExpression(params[1]);
					m_writer.Write(").xy");	

					//offset
					if (params.size() > 3)
					{
						m_writer.Write("+");
						OutputExpression(params[3]);
					}

					m_writer.Write(", ");


					m_writer.Write("(");
					OutputExpression(params[1]);
					m_writer.Write(").z");

					break;

				case HLSLBaseType_Texture2DMS:
						
					m_writer.Write("ivec2(");
					OutputExpression(functionCall->params[1]);
					m_writer.Write(").xy");

					//offset
					if (params.size() > 3)
					{
						m_writer.Write("+");
						OutputExpression(params[3]);
					}

					m_writer.Write(", ");

					if (params.size() > 2)
					{
						m_writer.Write("int(");
						OutputExpression(params[2]);
						m_writer.Write(")");
					}
					else
					{						
						OutputExpression(params[1]);
						m_writer.Write(".z");
					}					

						
					break;

				case HLSLBaseType_Texture2DArray:
						
					m_writer.Write("ivec3(");
					OutputExpression(params[1]);
					m_writer.Write(").xyz, ");

					m_writer.Write("(");
					OutputExpression(params[1]);
					m_writer.Write(").w");
					break;

				case HLSLBaseType_Texture2DMSArray:
						
					m_writer.Write("ivec3(");
					OutputExpression(params[1]);
					m_writer.Write(").xyz, ");

					if (params.size() > 2)
					{
						m_writer.Write("int(");
						OutputExpression(params[2]);
						m_writer.Write(")");
					}
					else
					{
						OutputExpression(params[1]);
						m_writer.Write(".w");
					}
					break;

				case HLSLBaseType_Texture3D:
						
					m_writer.Write("ivec3(");
					OutputExpression(params[1]);
					m_writer.Write(").xyz");

					//offset
					// TODO: what case is this????
					//if (pTextureStateExpression->indexExpression->nextExpression && pTextureStateExpression->indexExpression->nextExpression->nextExpression)
					//{
					//	m_writer.Write("+");
					//	OutputExpression(pTextureStateExpression->indexExpression->nextExpression->nextExpression);
					//}

					m_writer.Write(", ");

					m_writer.Write("(");
					OutputExpression(params[1]);
					m_writer.Write(").w");
					break;
				default:
					break;
				}

				m_writer.Write(")");
			}
			// Buffer
			else
			{
				OutputExpression(params[0]);
				m_writer.Write("[");
				OutputExpression(params[1]);
				m_writer.Write("]");
			}
		}
		else if (String_Equal(functionName, "Store"))
		{

			OutputExpression(params[0]);
			m_writer.Write("[");
			OutputExpression(params[1]);
			m_writer.Write("]");
			m_writer.Write(" = ");
			OutputExpression(params[2]);
		}
		else if (String_Equal(functionName, "InterlockedAdd") ||
			String_Equal(functionName, "InterlockedAnd") ||
			String_Equal(functionName, "InterlockedCompareExchange") ||
			String_Equal(functionName, "InterlockedExchange") ||
			String_Equal(functionName, "InterlockedMax") ||
			String_Equal(functionName, "InterlockedMin") ||
			String_Equal(functionName, "InterlockedOr") ||
			String_Equal(functionName, "InterlockedXor"))
		{

			const eastl::vector<HLSLExpression*>& params = functionCall->params;
			const eastl::vector<HLSLArgument*>& argumentVec = functionCall->function->args;

			//check the number of arguements
			if (argumentVec.size() >= 3)
			{
				// any cases with over 3 parameters are not handled. TODO??
				ASSERT_PARSER(argumentVec.size() == 3);

				HLSLType* expectedType = NULL;

				{
					expectedType = &argumentVec[2]->type;
				}

				bool isImage = IsTextureType(params[0]->expressionType.baseType);

				OutputExpression(params[2]);

				m_writer.Write(" = ");

				if(String_Equal(functionName, "InterlockedAdd"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicAdd" : "atomicAdd");
				}
				else if (String_Equal(functionName, "InterlockedAnd"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicAnd" : "atomicAnd");
				}
				else if (String_Equal(functionName, "InterlockedCompareExchange"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicCompSwap" : "atomicCompSwap");
				}
				else if (String_Equal(functionName, "InterlockedExchange"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicExchange" : "atomicExchange");
				}
				else if (String_Equal(functionName, "InterlockedMax"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicMax" : "atomicMax");
				}
				else if (String_Equal(functionName, "InterlockedMin"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicMin" : "atomicMin");
				}
				else if (String_Equal(functionName, "InterlockedOr"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicOr" : "atomicOr");
				}
				else if (String_Equal(functionName, "InterlockedXor"))
				{
					m_writer.Write("%s(", isImage ? "imageAtomicXor" : "atomicXor");
				}


				// if it is a regular expression, just output it.
				OutputExpression(params[0], expectedType);

				// hardcoded to 3 params, but we are missing some overloads
				for (int iter = 1; iter < argumentVec.size()-1; iter++)
				{
					m_writer.Write(", ");

					OutputExpression(params[iter]);
				}

				m_writer.Write(")");

#if 0
				int numExpressions2 = 0;

				int argIter = 0;
				while (expression != NULL)
				{
					
					if (numExpressions2 == 2)
					{
						m_writer.Write(")");
						break;
					}					

					if (numExpressions2 > 0)
						m_writer.Write(", ");

					if (numExpressions2 == 1 && isImage)
					{
						// if we are doing the image variation, then write the index after the first param
	
						// cast to signed
						m_writer.Write("%s(", GetBaseTypeName(indexDstType));

						OutputExpression(indexExpression);
						m_writer.Write("), ");
					}

					HLSLType* expectedType = NULL;
					if (argIter < argumentVec.size())
					{
						expectedType = &argumentVec[argIter]->type;
						argIter++;
					}

					OutputExpression(expression, expectedType);
					expression = expression->nextExpression;

					numExpressions2++;
				}
#endif
			}
			
		}
		else
		{
			if (String_Equal(functionName, "asfloat"))
			{
				int temp = 0;
				temp++;
			}
			
			OutputIdentifier(functionName);
			m_writer.Write("(");
			OutputExpressionList(functionCall->params);
			m_writer.Write(")");
		}
	}
	else
	{
		m_writer.Write("<unknown expression>");
	}

    if (cast)
    {
        m_writer.Write(")");
    }
}

void GLSLGenerator::OutputIdentifier(const CachedString & srcName)
{
	CachedString dstName;

    // Remap intrinstic functions.
    if (String_Equal(srcName, "tex2D"))
    {
		dstName = MakeCached(m_versionLegacy ? "texture2D" : "texture");
    }
    else if (String_Equal(srcName, "tex2Dproj"))
    {
		dstName = MakeCached(m_versionLegacy ? "texture2DProj" : "textureProj");
    }
    else if (String_Equal(srcName, "texCUBE"))
    {
		dstName = MakeCached(m_versionLegacy ? "textureCube" : "texture");
    }
    else if (String_Equal(srcName, "tex3D"))
    {
		dstName = MakeCached(m_versionLegacy ? "texture3D" : "texture");
    }
    else if (String_Equal(srcName, "clip"))
    {
		dstName = m_clipFunction;
    }
	else if (String_Equal(srcName, "f16tof32"))
	{
		dstName = m_f16tof32Function;
	}
	else if (String_Equal(srcName, "f32tof16"))
	{
		dstName = m_f32tof16Function;
	}
	else if (String_Equal(srcName, "tex2Dlod"))
	{
		dstName = m_tex2DlodFunction;
	}
	else if (String_Equal(srcName, "textureLodOffset"))
	{
		dstName = m_textureLodOffsetFunction;
	}
	else if (String_Equal(srcName, "tex2Dbias"))
    {
		dstName = m_tex2DbiasFunction;
    }
    else if (String_Equal(srcName, "tex2Dgrad"))
    {
		dstName = m_tex2DgradFunction;
    }
    else if (String_Equal(srcName, "tex2DArray"))
    {
		dstName = MakeCached("texture");
    }
    else if (String_Equal(srcName, "texCUBEbias"))
    {
		dstName = m_texCUBEbiasFunction;
    }
	else if( String_Equal(srcName, "texCUBElod" ) )
	{
		dstName = m_texCUBElodFunction;
	}
    else if (String_Equal(srcName, "atan2"))
    {
		dstName = MakeCached("atan");
    }
    else if (String_Equal(srcName, "sincos"))
    {
		dstName = m_sinCosFunction;

    }
    else if (String_Equal(srcName, "fmod"))
    {
        // mod is not the same as fmod if the parameter is negative!
        // The equivalent of fmod(x, y) is x - y * floor(x/y)
        // We use the mod version for performance.
		dstName = MakeCached("mod");
    }
    else if (String_Equal(srcName, "lerp"))
    {
		dstName = MakeCached("mix");
    }
    else if (String_Equal(srcName, "frac"))
    {
		dstName = MakeCached("fract");
    }
    else if (String_Equal(srcName, "ddx"))
    {
		dstName = MakeCached("dFdx");
    }
    else if (String_Equal(srcName, "ddy"))
    {
		dstName = MakeCached("dFdy");
    }

	else if (String_Equal(srcName, "countbits"))
	{
		dstName = MakeCached("bitCount");
	}

	else if (String_Equal(srcName, "QuadReadAcrossDiagonal"))
	{
		dstName = MakeCached("subgroupQuadSwapDiagonal");
	}

	else if (String_Equal(srcName, "QuadReadLaneAt"))
	{
		dstName = MakeCached("subgroupQuadBroadcast");
	}


	else if (String_Equal(srcName, "QuadReadAcrossX"))
	{
		dstName = MakeCached("subgroupQuadSwapHorizontal");
	}

	else if (String_Equal(srcName, "QuadReadAcrossY"))
	{
		dstName = MakeCached("subgroupQuadSwapVertical");
	}

	else if (String_Equal(srcName, "WaveActiveAllEqual"))
	{
		dstName = MakeCached("subgroupAllEqual");
	}

	else if (String_Equal(srcName, "WaveActiveBitAnd"))
	{
		dstName = MakeCached("subgroupAnd");
	}

	else if (String_Equal(srcName, "WaveActiveBitOr"))
	{
		dstName = MakeCached("subgroupOr");
	}

	else if (String_Equal(srcName, "WaveActiveBitXor"))
	{
		dstName = MakeCached("subgroupXor");
	}

	else if (String_Equal(srcName, "WaveActiveCountBits"))
	{
		dstName = MakeCached("subgroupBallotBitCount");
	}

	else if (String_Equal(srcName, "WaveActiveMax"))
	{
		dstName = MakeCached("subgroupMax");
	}
	else if (String_Equal(srcName, "WaveActiveMin"))
	{
		dstName = MakeCached("subgroupMin");
	}
	else if (String_Equal(srcName, "WaveActiveProduct"))
	{
		dstName = MakeCached("subgroupMul");
	}
	else if (String_Equal(srcName, "WaveActiveSum"))
	{
		dstName = MakeCached("subgroupAdd");
	}
	else if (String_Equal(srcName, "WaveActiveAllTrue"))
	{
		dstName = MakeCached("subgroupAll");
	}
	else if (String_Equal(srcName, "WaveActiveAnyTrue"))
	{
		dstName = MakeCached("subgroupAny");
	}
	else if (String_Equal(srcName, "WaveActiveBallot"))
	{
		dstName = MakeCached("subgroupBallot");
	}

	else if (String_Equal(srcName, "WaveIsFirstLane"))
	{
		dstName = MakeCached("subgroupElect");
	}

	else if (String_Equal(srcName, "WavePrefixCountBits"))
	{
		dstName = MakeCached("subgroupBallotExclusiveBitCount");
	}
	
	else if (String_Equal(srcName, "WavePrefixProduct"))
	{
		dstName = MakeCached("subgroupExclusiveMul");
	}

	else if (String_Equal(srcName, "WavePrefixSum"))
	{
		dstName = MakeCached("subgroupExclusiveAdd");
	}

	else if (String_Equal(srcName, "WaveReadLaneFirst"))
	{
		dstName = MakeCached("subgroupBroadcastFirst");
	}

	else if (String_Equal(srcName, "WaveReadLaneAt"))
	{
		dstName = MakeCached("subgroupBroadcast");
	}	

	else if (String_Equal(srcName, "InterlockedAdd"))
	{
		dstName = MakeCached("atomicAdd");
	}
	else if (String_Equal(srcName, "InterlockedAnd"))
	{
		dstName = MakeCached("atomicAnd");
	}
	else if (String_Equal(srcName, "InterlockedOr"))
	{
		dstName = MakeCached("atomicOr");
	}
	else if (String_Equal(srcName, "InterlockedXor"))
	{
		dstName = MakeCached("atomicXor");
	}
	else if (String_Equal(srcName, "InterlockedMin"))
	{
		dstName = MakeCached("atomicMin");
	}
	else if (String_Equal(srcName, "InterlockedMax"))
	{
		dstName = MakeCached("atomicMax");
	}
	else if (String_Equal(srcName, "InterlockedExchange"))
	{
		dstName = MakeCached("atomicExchange");
	}
	else if (String_Equal(srcName, "InterlockedCompareExchange"))
	{
		dstName = MakeCached("atomicCompSwap");
	}	
	else if (String_Equal(srcName, "InterlockedAnd"))
	{
		dstName = MakeCached("atomicAnd");
	}
	else if (String_Equal(srcName, "InterlockedOr"))
	{
		dstName = MakeCached("atomicOr");
	}
	else if (String_Equal(srcName, "InterlockedXor"))
	{
		dstName = MakeCached("atomicXor");
	}
	else if (String_Equal(srcName, "InterlockedMin"))
	{
		dstName = MakeCached("atomicMin");
	}
	else if (String_Equal(srcName, "InterlockedMax"))
	{
		dstName = MakeCached("atomicMax");
	}
	else if (String_Equal(srcName, "InterlockedExchange"))
	{
		dstName = MakeCached("atomicExchange");
	}
	else if (String_Equal(srcName, "InterlockedCompareExchange"))
	{
		dstName = MakeCached("atomicCompSwap");
	}	
	else if (String_Equal(srcName, "GroupMemoryBarrierWithGroupSync"))
	{
		dstName = MakeCached("barrier(); groupMemoryBarrier");
	}
	else if (String_Equal(srcName, "GroupMemoryBarrier"))
	{
		dstName = MakeCached("groupMemoryBarrier");
	}
	else if (String_Equal(srcName, "DeviceMemoryBarrierWithGroupSync"))
	{
		dstName = MakeCached("barrier(); memoryBarrierImage(); memoryBarrier");
	}
	else if (String_Equal(srcName, "DeviceMemoryBarrier"))
	{
		dstName = MakeCached("memoryBarrierImage(); memoryBarrier");
	}
	else if (String_Equal(srcName, "AllMemoryBarrierWithGroupSync"))
	{
		dstName = MakeCached("barrier(); groupMemoryBarrier();  memoryBarrierImage(); memoryBarrier");
	}
	else if (String_Equal(srcName, "AllMemoryBarrier"))
	{
		dstName = MakeCached("barrier(); groupMemoryBarrier();  memoryBarrierImage(); memoryBarrier");
	}
	else 
	{
		dstName = srcName;

		// The identifier could be a GLSL reserved word (if it's not also a HLSL reserved word).
		CachedString baseName = dstName;
		dstName = GetSafeIdentifierName(baseName);
	}
	m_writer.Write("%s", FetchCstr(m_stringLibrary, dstName));
}

void GLSLGenerator::OutputIdentifierExpression(HLSLIdentifierExpression* pIdentExpr)
{
	CachedString baseName = pIdentExpr->pDeclaration->name;

	if (IsBuffer(pIdentExpr->expressionType.baseType))
	{
		HLSLBuffer *buffer =  static_cast<HLSLBuffer*>(pIdentExpr->pDeclaration);

		if (buffer->type.array && buffer->type.arrayDimension > 0)
		{
			// original code for reference, we have an extra name in there, but I think it's harmless. Fixing it anyways.
			// m_writer.Write("%s", name, name);			
			m_writer.Write("%s", FetchCstr(m_stringLibrary, baseName));
		}
		else
			m_writer.Write("%s_Data", FetchCstr(m_stringLibrary, baseName));
		return;
	}

	// if it is one of PushConstaantBuffer's data's Name
	if (pIdentExpr->pDeclaration->buffer)
	{
		HLSLBuffer* buffer = static_cast<HLSLBuffer*>(pIdentExpr->pDeclaration->buffer);
		if (buffer->bPushConstant)
			m_writer.Write("%s.%s", FetchCstr(m_stringLibrary, buffer->name), FetchCstr(m_stringLibrary, baseName));
		else
			m_writer.Write("%s", FetchCstr(m_stringLibrary, baseName));
		return;
	}

	// The identifier could be a GLSL reserved word (if it's not also a HLSL reserved word).
	m_writer.Write("%s", FetchCstr(m_stringLibrary, GetSafeIdentifierName(baseName)));
}

void GLSLGenerator::OutputArguments(const eastl::vector<HLSLArgument*>& arguments)
{
	int firstArgument = 0;

	//DomainShader
	if (m_target == Target_DomainShader)
	{
		firstArgument = 1; // not sure why we do this, but I guess we skip the first argument in domain shaders?
	}

	int numWritten = 0;

	for (int i = firstArgument; i < arguments.size(); i++)
	{
		HLSLArgument * argument = arguments[i];

		if (argument->type.baseType == HLSLBaseType_InputPatch || argument->type.baseType == HLSLBaseType_OutputPatch)
		{
			continue;
		}
		else if (m_target == Target_GeometryShader && argument->modifier == HLSLArgumentModifier_Inout)
		{
			continue;
		}
		else
		{
			// ok, keep going
		}

		// We tend to skip args in a few places, so we keep trak of how many args are actually written,
		// as opposed to just checking if (i==0).
		if (numWritten > 0)
		{
			m_writer.Write(", ");
		}

		switch (argument->modifier)
		{
		case HLSLArgumentModifier_In:
			m_writer.Write("in ");
			break;
		case HLSLArgumentModifier_Out:
			m_writer.Write("out ");
			break;
		case HLSLArgumentModifier_Inout:
			m_writer.Write("inout ");
			break;
		default:
			break;
		}

		OutputDeclaration(argument->type, argument->name);
		numWritten++;
	}
}

static const char * GetAttributeName(HLSLAttributeType attributeType)
{
	if (attributeType == HLSLAttributeType_Unroll) return "unroll";
	if (attributeType == HLSLAttributeType_Branch) return "branch";
	if (attributeType == HLSLAttributeType_Flatten) return "flatten";
	if (attributeType == HLSLAttributeType_NumThreads) return "numthreads";
	if (attributeType == HLSLAttributeType_MaxVertexCount) return "maxvertexcount";

	if (attributeType == HLSLAttributeType_Domain) return "domain";
	if (attributeType == HLSLAttributeType_Partitioning) return "partitioning";
	if (attributeType == HLSLAttributeType_OutputTopology) return "outputtopology";
	if (attributeType == HLSLAttributeType_OutputControlPoints) return "outputcontrolpoints";
	if (attributeType == HLSLAttributeType_PatchConstantFunc) return "patchconstantfunc";

	return NULL;
}

void GLSLGenerator::OutputAttributes(int indent, HLSLAttribute* attribute)
{
	while (attribute != NULL)
	{
		const char * attributeName = GetAttributeName(attribute->attributeType);

		if (attributeName != NULL)
		{
			if (String_Equal(attributeName, "numthreads"))
			{
				m_writer.EndLine();


				m_writer.Write("layout(");

				if (attribute->numGroupX != 0)
					m_writer.Write("local_size_x = %d, ", attribute->numGroupX);
				else
					m_writer.Write("local_size_x = %s, ", FetchCstr(m_stringLibrary, attribute->numGroupXstr));

				if (attribute->numGroupY != 0)
					m_writer.Write("local_size_y = %d, ", attribute->numGroupY);
				else
					m_writer.Write("local_size_y = %s, ", FetchCstr(m_stringLibrary, attribute->numGroupYstr));


				if (attribute->numGroupZ != 0)
					m_writer.Write("local_size_z = %d", attribute->numGroupZ);
				else
					m_writer.Write("local_size_z = %s", FetchCstr(m_stringLibrary, attribute->numGroupZstr));

				m_writer.Write(") in;");

				m_writer.EndLine();
			}
			else if (String_Equal(attributeName, "maxvertexcount"))
			{
				m_writer.EndLine();
				m_writer.Write("layout(%s, max_vertices = %d) out;", FetchCstr(m_stringLibrary, m_outputGeometryType), attribute->maxVertexCount);
				m_writer.EndLine();
			}
			else if (String_Equal(attributeName, "outputcontrolpoints"))
			{
				m_writer.EndLine();
				m_writer.Write("layout(vertices = %d) out;", attribute->outputcontrolpoints);
				m_writer.EndLine();
			}
			else if (String_Equal(attributeName, "domain"))
			{
				m_domain = attribute->domain;
			}
			else if (String_Equal(attributeName, "partitioning"))
			{
				m_partitioning = attribute->partitioning;
			}
			else if (String_Equal(attributeName, "outputtopology"))
			{
				m_outputtopology = attribute->outputtopology;
			}
			else if (String_Equal(attributeName, "patchconstantfunc"))
			{
				m_patchconstantfunc = attribute->patchconstantfunc;
			}
			else if (String_Equal(attributeName, "unroll"))
			{
				m_writer.Write(indent,"[[unroll]] ");
				m_writer.EndLine();
			}

			


			if (m_target == Target_DomainShader && m_domain.IsNotEmpty() && m_partitioning.IsNotEmpty() && m_outputtopology.IsNotEmpty())
			{
				m_writer.EndLine();
				m_writer.Write("layout(");

				if (String_Equal(m_domain, "\"tri\""))
				{
					m_writer.Write("triangles, ");
				}
				else if (String_Equal(m_domain, "\"quad\""))
				{
					m_writer.Write("quads, ");
				}
				else if (String_Equal(m_domain, "\"isoline\""))
				{
					m_writer.Write("isolines, ");
				}


				if (String_Equal(m_partitioning, "\"integer\""))
				{
					m_writer.Write("equal_spacing, ");
				}
				else if (String_Equal(m_partitioning, "\"fractional_even\""))
				{
					m_writer.Write("fractional_even_spacing, ");
				}
				else if (String_Equal(m_partitioning, "\"fractional_odd\""))
				{
					m_writer.Write("fractional_odd_spacing, ");
				}

				if (String_Equal(m_outputtopology, "\"triangle_cw\""))
				{
					m_writer.Write("cw");
				}
				else if (String_Equal(m_outputtopology, "\"triangle_ccw\""))
				{
					m_writer.Write("ccw");
				}

				m_writer.Write(") in;");
				m_writer.EndLine();
			}
			
			
		}

		attribute = attribute->nextAttribute;
	}
}

void GLSLGenerator::OutputStatements(int indent, HLSLStatement* statement, const HLSLType* returnType)
{

    while (statement != NULL)
    {
        if (statement->hidden)
        {
            statement = statement->nextStatement;
            continue;
        }

		OutputAttributes(indent, statement->attributes);

		if (statement->nodeType == HLSLNodeType_TextureState)
		{
			HLSLTextureState* textureState = static_cast<HLSLTextureState*>(statement);

			const char* prefix = "";
			switch (GetScalarBaseType(textureState->type.elementType))
			{
			case HLSLBaseType_Int: prefix = "i";break;
			case HLSLBaseType_Uint: prefix = "u";break;
			}

			const char* baseTypeName = GetBaseTypeName(textureState->type.baseType);
			const char* elementType = "";
			m_writer.BeginLine(indent, RawStr(textureState->fileName), textureState->line);

			if (IsRWTexture(textureState->type.baseType) || IsRasterizerOrderedTexture(textureState->type.baseType))
			{
				// note: we are promoting 3 float types to 4
				switch (textureState->type.elementType)
				{
				case HLSLBaseType_Float:
					elementType = ", r32f";
					break;
				case HLSLBaseType_Float2:
					elementType = ", rg32f";
					break;
				case HLSLBaseType_Float3:
				case HLSLBaseType_Float4:
					elementType = ", rgba32f";
					break;

				case HLSLBaseType_Half:
				case HLSLBaseType_Min16Float:
					elementType = ", r16f";
					break;
				case HLSLBaseType_Half2:
				case HLSLBaseType_Min16Float2:
					elementType = ", rg16f";
					break;
				case HLSLBaseType_Half3:
				case HLSLBaseType_Min16Float3:
				case HLSLBaseType_Half4:
				case HLSLBaseType_Min16Float4:
					elementType = ", rgba16f";
					break;

				case HLSLBaseType_Int:
					elementType = ", r32i";
					break;
				case HLSLBaseType_Int2:
					elementType = ", rg32i";
					break;
				case HLSLBaseType_Int3:
				case HLSLBaseType_Int4:
					elementType = ", rgba32i";
					break;

				case HLSLBaseType_Uint:
					elementType = ", r32ui";
					break;
				case HLSLBaseType_Uint2:
					elementType = ", rg32ui";
					break;
				case HLSLBaseType_Uint3:
				case HLSLBaseType_Uint4:
					elementType = ", rgba32ui";
					break;
				default:
					Error("Unknown RWTexture type %d", (int)textureState->type.elementType);
					break;
				}
			}

			m_writer.Write("layout(set = %d, binding = %d%s) uniform %s%s %s",
				textureState->registerSpace, textureState->registerIndex, elementType,
				prefix, baseTypeName, RawStr(textureState->name));

			if (textureState->type.array)
			{
				OutputArrayExpression(textureState->type.arrayDimension, textureState->arrayDimExpression);
			}

			m_writer.EndLine(";");
		}
		else if (statement->nodeType == HLSLNodeType_GroupShared)
		{
			HLSLGroupShared* pGroupShared = static_cast<HLSLGroupShared*>(statement);

			m_writer.Write(0, "shared ");
			OutputDeclaration(pGroupShared->declaration);
			m_writer.EndLine(";");

		}
		else if (statement->nodeType == HLSLNodeType_SamplerState)
		{
			HLSLSamplerState* samplerState = static_cast<HLSLSamplerState*>(statement);

			m_writer.BeginLine(indent, RawStr(samplerState->fileName), samplerState->line);

			m_writer.Write("layout(set = %d, binding = %d) uniform ", samplerState->registerSpace, samplerState->registerIndex);

			if (samplerState->type.baseType == HLSLBaseType_SamplerComparisonState)
			{
				m_writer.Write("samplerShadow %s", RawStr(samplerState->name));
			}
			else
			{
				m_writer.Write("sampler %s", RawStr(samplerState->name));
			}

			if (samplerState->type.array)
			{
				OutputArrayExpression(samplerState->type.arrayDimension, samplerState->arrayDimExpression);
			}

			m_writer.EndLine(";");
		}
		else if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);

            // GLSL doesn't seem have texture uniforms, so just ignore them.
            if (declaration->type.baseType != HLSLBaseType_Texture)
            {
                m_writer.BeginLine(indent, RawStr(declaration->fileName), declaration->line);
                if (indent == 0)
                {
                    // At the top level, we need the "uniform" keyword.
                    //m_writer.Write("uniform ");

					if (declaration->type.flags & HLSLTypeFlag_Static)
					{
						// ignore?
						//m_writer.Write("static ");
					}
					//m_writer.Write("const ");
                }

				if (declaration->type.flags & HLSLTypeFlag_Const)
				{
					m_writer.Write("const ");
				}

				OutputDeclaration(declaration);
                m_writer.EndLine(";");
            }
        }
        else if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
			
            m_writer.WriteLine(indent, "struct %s", RawStr(structure->name));
			m_writer.WriteLine(indent, "{");
            HLSLStructField* field = structure->field;
            while (field != NULL)
            {
                m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);

				if (m_target == Target_HullShader)
				{
					if (field->type.array && field->arrayDimExpression[0] == NULL)
						field->type.array = false;
				}

				{
					OutputDeclaration(field->type, field->name);
					m_writer.Write(";");
					m_writer.EndLine();
				}
              
                field = field->nextField;
            }
            m_writer.WriteLine(indent, "};");
        }				
		else if (statement->nodeType == HLSLNodeType_Buffer)
		{
			HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
			if (buffer->type.baseType == HLSLBaseType_RWBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				m_writer.BeginLine(indent, RawStr(buffer->fileName), buffer->line);
				eastl::string bufferType = getElementTypeAsStrGLSL(m_stringLibrary, buffer->type);
				m_writer.Write("RWBuffer<%s> %s", bufferType.c_str(), RawStr(buffer->name));

				m_writer.EndLine(";");
			}
			else if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				m_StructuredBufferNames.push_back(buffer->name);

				m_writer.BeginLine(indent, RawStr(buffer->fileName), buffer->line);

				m_writer.Write("layout(row_major, set=%d, binding=%d) ", buffer->registerSpace, buffer->registerIndex);

				if (buffer->type.array && buffer->type.arrayDimension > 0)
					m_writer.Write("buffer %s_Block", RawStr(buffer->name));
				else
					m_writer.Write("buffer %s", RawStr(buffer->name));
								
				m_writer.WriteLine(0, "\n{");

				if (buffer->type.elementType != HLSLBaseType_Unknown)
				{
					//avoid for duplicating buffer name and its element name
					eastl::string bufferType = getElementTypeAsStrGLSL(m_stringLibrary, buffer->type);
					m_writer.WriteLine(indent + 1, "%s %s_Data[];", bufferType.c_str(), RawStr(buffer->name));
				}
				else
				{
					switch (buffer->type.elementType)
					{
					case HLSLBaseType_Float:
						m_writer.Write(indent + 1, "float");
						break;
					case HLSLBaseType_Float2:
						m_writer.Write(indent + 1, "vec2");
						break;
					case HLSLBaseType_Float3:
						m_writer.Write(indent + 1, "vec3");
						break;
					case HLSLBaseType_Float4:
						m_writer.Write(indent + 1, "vec4");
						break;

					case HLSLBaseType_Half:
						m_writer.Write(indent + 1, "mediump float");
						break;
					case HLSLBaseType_Half2:
						m_writer.Write(indent + 1, "mediump vec2");
						break;
					case HLSLBaseType_Half3:
						m_writer.Write(indent + 1, "mediump vec3");
						break;
					case HLSLBaseType_Half4:
						m_writer.Write(indent + 1, "mediump vec4");
						break;

					case HLSLBaseType_Min16Float:
						m_writer.Write(indent + 1, "mediump float");
						break;
					case HLSLBaseType_Min16Float2:
						m_writer.Write(indent + 1, "mediump vec2");
						break;
					case HLSLBaseType_Min16Float3:
						m_writer.Write(indent + 1, "mediump vec3");
						break;
					case HLSLBaseType_Min16Float4:
						m_writer.Write(indent + 1, "mediump vec4");
						break;

					case HLSLBaseType_Min10Float:
						m_writer.Write(indent + 1, "lowp float");
						break;
					case HLSLBaseType_Min10Float2:
						m_writer.Write(indent + 1, "lowp vec2");
						break;
					case HLSLBaseType_Min10Float3:
						m_writer.Write(indent + 1, "lowp vec3");
						break;
					case HLSLBaseType_Min10Float4:
						m_writer.Write(indent + 1, "lowp vec4");
						break;

					case HLSLBaseType_Bool:
						m_writer.Write(indent + 1, "bool");
						break;
					case HLSLBaseType_Bool2:
						m_writer.Write(indent + 1, "bool2");
						break;
					case HLSLBaseType_Bool3:
						m_writer.Write(indent + 1, "bool3");
						break;
					case HLSLBaseType_Bool4:
						m_writer.Write(indent + 1, "bool4");
						break;

					case HLSLBaseType_Int:
						m_writer.Write(indent + 1, "int");
						break;
					case HLSLBaseType_Int2:
						m_writer.Write(indent + 1, "ivec2");
						break;
					case HLSLBaseType_Int3:
						m_writer.Write(indent + 1, "ivec3");
						break;
					case HLSLBaseType_Int4:
						m_writer.Write(indent + 1, "ivec4");
						break;

					case HLSLBaseType_Uint:
						m_writer.Write(indent + 1, "uint");
						break;
					case HLSLBaseType_Uint2:
						m_writer.Write(indent + 1, "uvec2");
						break;
					case HLSLBaseType_Uint3:
						m_writer.Write(indent + 1, "uvec3");
						break;
					case HLSLBaseType_Uint4:
						m_writer.Write(indent + 1, "uvec4");
						break;
					default:
						break;
					}

					m_writer.Write(" %s_Data[];", RawStr(buffer->name));
					m_writer.EndLine();
				}

				if (buffer->type.array && buffer->type.arrayDimension > 0)
				{
					m_writer.Write("}%s", RawStr(buffer->name));

					OutputArrayExpression(buffer->type.arrayDimension, buffer->arrayDimExpression);

					m_writer.EndLine(";");
				}
				else
				{
					m_writer.WriteLine(0, "};");
				}
			}
			else if (buffer->type.baseType == HLSLBaseType_CBuffer || buffer->type.baseType == HLSLBaseType_TBuffer)
			{
				m_writer.BeginLine(indent, RawStr(buffer->fileName), buffer->line);

				if (buffer->bPushConstant)
				{
					m_PushConstantBuffers.push_back(buffer);
					m_writer.Write("layout(row_major, push_constant) ");
				}
				else
					m_writer.Write("layout(row_major, set = %d, binding = %d) ", buffer->registerSpace, buffer->registerIndex);


				if (buffer->bPushConstant)
					m_writer.Write("uniform %s_Block", RawStr(buffer->name));
				else
				{
					m_writer.Write("uniform %s", RawStr(buffer->name));
				}

				m_writer.EndLine();
				m_writer.EndLine("{");

				HLSLDeclaration* field = buffer->field;

				while (field != NULL)
				{
					if (!field->hidden)
					{

						m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);
						OutputDeclaration(field->type, field->name);
						m_writer.Write(";");
						m_writer.EndLine();
					}
					field = (HLSLDeclaration*)field->nextStatement;
				}

				if (buffer->bPushConstant)
					m_writer.WriteLine(indent, "}%s;", RawStr(buffer->name));
				else
					m_writer.WriteLine(indent, "};");
			}
			else if (buffer->type.baseType ==  HLSLBaseType_ByteAddressBuffer || buffer->type.baseType ==  HLSLBaseType_RWByteAddressBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				m_StructuredBufferNames.push_back(buffer->name);

				m_writer.BeginLine(indent, RawStr(buffer->fileName), buffer->line);

				m_writer.Write("layout(set = %d, binding = %d) ", buffer->registerSpace, buffer->registerSpace);

				if (buffer->type.array && buffer->type.arrayDimension > 0)
					m_writer.Write("buffer %s_Block", RawStr(buffer->name));
				else
					m_writer.Write("buffer %s", RawStr(buffer->name));

				m_writer.WriteLine(0, "\n{");
				m_writer.WriteLine(1, "%s %s_Data[];", CHECK_CSTR("uint"), RawStr(buffer->name));

				if (buffer->type.array && buffer->type.arrayDimension > 0)
				{
					m_writer.Write("}%s", RawStr(buffer->name));

					OutputArrayExpression(buffer->type.arrayDimension, buffer->arrayDimExpression);

					m_writer.EndLine(";");
				}
				else
				{
					m_writer.WriteLine(0, "};");
				}

			}
			else if (buffer->type.baseType == HLSLBaseType_ConstantBuffer)
			{
				m_writer.BeginLine(indent, RawStr(buffer->fileName), buffer->line);

				if (buffer->bPushConstant)
				{
					m_PushConstantBuffers.push_back(buffer);
					m_writer.Write("layout(row_major, push_constant) ");
				}
				else
					m_writer.Write("layout(row_major, set= %d, binding = %d) ", buffer->registerSpace, buffer->registerIndex);


				if (buffer->bPushConstant)
					m_writer.Write("uniform %s_Block", RawStr(buffer->name));
				else
				{
					if (buffer->type.elementType != HLSLBaseType_Unknown)
						m_writer.Write("uniform %s_Block", RawStr(buffer->name));
					else
						m_writer.Write("uniform %s", RawStr(buffer->name));
				}

				m_writer.EndLine();
				m_writer.EndLine("{");


				{
					eastl::string glslTypeName = getElementTypeAsStrGLSL(m_stringLibrary, buffer->type);
					HLSLStruct* pStruct = m_tree->FindGlobalStruct(MakeCached(glslTypeName.c_str()));
					HLSLStructField* field = pStruct->field;

					while (field != NULL)
					{
						if (!field->hidden)
						{

							m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);
							OutputDeclaration(field->type, field->name);
							m_writer.Write(";");
							m_writer.EndLine();
						}
						field = (HLSLStructField*)field->nextField;
					}					
				}

				if (buffer->bPushConstant)
					m_writer.WriteLine(indent, "}%s;", RawStr(buffer->name));
				else
				{
					if (buffer->type.elementType != HLSLBaseType_Unknown)
						m_writer.WriteLine(indent, "}%s;", RawStr(buffer->name));
					else
						m_writer.WriteLine(indent, "};");
				}
				
			}
			else if (buffer->type.baseType == HLSLBaseType_StructuredBuffer || buffer->type.baseType == HLSLBaseType_PureBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				m_StructuredBufferNames.push_back(buffer->name);

				m_writer.BeginLine(indent, RawStr(buffer->fileName), buffer->line);

				m_writer.Write("layout(row_major, set = %d, binding = %d) ", buffer->registerSpace, buffer->registerIndex);

				if (buffer->type.array && buffer->type.arrayDimension > 0)
					m_writer.Write("buffer %s_Block", RawStr(buffer->name));
				else
					m_writer.Write("buffer %s", RawStr(buffer->name));

				m_writer.WriteLine(0, "\n{");
				eastl::string bufferType = getElementTypeAsStrGLSL(m_stringLibrary, buffer->type);
				m_writer.WriteLine(1, "%s %s_Data[];", CHECK_CSTR(bufferType.c_str()), RawStr(buffer->name));

				if (buffer->type.array && buffer->type.arrayDimension > 0)
				{
					m_writer.Write("}%s", RawStr(buffer->name));

					OutputArrayExpression(buffer->type.arrayDimension, buffer->arrayDimExpression);

					m_writer.EndLine(";");
				}
				else
				{
					m_writer.WriteLine(0, "};");
				}
			}

			m_writer.EndLine("");
		}
        else if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction* function = static_cast<HLSLFunction*>(statement);

            // Use an alternate name for the function which is supposed to be entry point
            // so that we can supply our own function which will be the actual entry point.
            CachedString functionName   = GetSafeIdentifierName(function->name);
            const char* returnTypeName = GetTypeName(function->returnType);

            m_writer.BeginLine(indent, RawStr(function->fileName), function->line);

			if (String_Equal(functionName, "main"))
				m_writer.Write("%s HLSL%s(", returnTypeName, RawStr(functionName));
			else
				m_writer.Write("%s %s(", returnTypeName, RawStr(functionName));

			OutputArguments(function->args);

            if (function->forward)
            {
                m_writer.WriteLine(indent, ");");
            }
            else
            {
                m_writer.Write(")");
                m_writer.EndLine();
				m_writer.EndLine("{");

                OutputStatements(indent + 1, function->statement, &function->returnType);

				if(m_target == Target_GeometryShader)
					m_writer.WriteLine(indent + 1, "EndPrimitive();");

                m_writer.WriteLine(indent, "}");
            }
        }
        else if (statement->nodeType == HLSLNodeType_ExpressionStatement)
        {
            HLSLExpressionStatement* expressionStatement = static_cast<HLSLExpressionStatement*>(statement);
            m_writer.BeginLine(indent, RawStr(statement->fileName), statement->line);
            OutputExpression(expressionStatement->expression);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_ReturnStatement)
        {
            HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
            if (returnStatement->expression != NULL)
            {
                m_writer.BeginLine(indent, RawStr(returnStatement->fileName), returnStatement->line);
                m_writer.Write("return ");
                OutputExpression(returnStatement->expression, returnType);
                m_writer.EndLine(";");
            }
            else
            {
                m_writer.WriteLineTagged(indent, RawStr(returnStatement->fileName), returnStatement->line, "return;");
            }
        }
        else if (statement->nodeType == HLSLNodeType_DiscardStatement)
        {
            HLSLDiscardStatement* discardStatement = static_cast<HLSLDiscardStatement*>(statement);
            if (m_target == Target_FragmentShader)
            {
                m_writer.WriteLineTagged(indent, RawStr(discardStatement->fileName), discardStatement->line, "discard;");
            }
        }
        else if (statement->nodeType == HLSLNodeType_BreakStatement)
        {
            HLSLBreakStatement* breakStatement = static_cast<HLSLBreakStatement*>(statement);
            m_writer.WriteLineTagged(indent, RawStr(breakStatement->fileName), breakStatement->line, "break;");
        }
        else if (statement->nodeType == HLSLNodeType_ContinueStatement)
        {
            HLSLContinueStatement* continueStatement = static_cast<HLSLContinueStatement*>(statement);
            m_writer.WriteLineTagged(indent, RawStr(continueStatement->fileName), continueStatement->line, "continue;");
        }
        else if (statement->nodeType == HLSLNodeType_IfStatement)
        {
            HLSLIfStatement* ifStatement = static_cast<HLSLIfStatement*>(statement);
            m_writer.Write(indent, "if(");
            OutputExpression(ifStatement->condition, &kBoolType);
			m_writer.EndLine(")");

			if (ifStatement->statement != NULL)
			{
				{
					m_writer.WriteLine(indent, "{");
					OutputStatements(indent + 1, ifStatement->statement, returnType);
					m_writer.WriteLine(indent, "}");
				}
			}
			else
			{
				m_writer.WriteLine(indent, "{}");
			}

			for (int i = 0; i< ifStatement->elseifStatement.size(); i++)
			{
				m_writer.Write(indent, "else if (");
				OutputExpression(ifStatement->elseifStatement[i]->condition);
				m_writer.EndLine(")");
				
				
				
				m_writer.WriteLine(indent, "{");
				OutputStatements(indent + 1, ifStatement->elseifStatement[i]->statement, returnType);
				m_writer.WriteLine(indent, "}");
			}

			if (ifStatement->elseStatement != NULL)
			{
				m_writer.WriteLine(indent, "else");
				m_writer.WriteLine(indent, "{");
				OutputStatements(indent + 1, ifStatement->elseStatement, returnType);
				m_writer.WriteLine(indent, "}");
			}
        }
		else if (statement->nodeType == HLSLNodeType_SwitchStatement)
		{
			HLSLSwitchStatement* switchStatement = static_cast<HLSLSwitchStatement*>(statement);

			m_writer.Write(indent, "switch (");
			OutputExpression(switchStatement->condition, false);
			m_writer.Write(")\n");

			m_writer.WriteLine(indent, "{");

			//print cases
			int numCases = (int)switchStatement->caseNumber.size();
			ASSERT_PARSER(numCases == switchStatement->caseStatement.size());

			for (int i = 0; i< numCases; i++)
			{
				m_writer.Write(indent + 1, "case ");

				OutputExpression(switchStatement->caseNumber[i], false);

				m_writer.Write(":\n");

				m_writer.WriteLine(indent + 1, "{");
				OutputStatements(indent + 2, switchStatement->caseStatement[i]);
				m_writer.WriteLine(indent + 1, "}");
			}

			//print default
			m_writer.Write(indent + 1, "default:\n");
			m_writer.WriteLine(indent + 1, "{");
			OutputStatements(indent + 2, switchStatement->caseDefault);
			m_writer.WriteLine(indent + 1, "}");

			m_writer.WriteLine(indent, "}");
		}
        else if (statement->nodeType == HLSLNodeType_ForStatement)
        {
            HLSLForStatement* forStatement = static_cast<HLSLForStatement*>(statement);
            m_writer.BeginLine(indent, RawStr(forStatement->fileName), forStatement->line);
            m_writer.Write("for (");

			if (forStatement->initialization)
				OutputDeclaration(forStatement->initialization);
			else if (forStatement->initializationWithoutDeclaration)
				OutputExpression(forStatement->initializationWithoutDeclaration);


            m_writer.Write("; ");
            OutputExpression(forStatement->condition, &kBoolType);
            m_writer.Write("; ");
            OutputExpression(forStatement->increment);
			m_writer.Write(")");
			m_writer.EndLine();

			m_writer.WriteLine(indent, "{");
            OutputStatements(indent + 1, forStatement->statement, returnType);
            m_writer.WriteLine(indent, "}");
        }
		else if (statement->nodeType == HLSLNodeType_WhileStatement)
		{
			HLSLWhileStatement* whileStatement = static_cast<HLSLWhileStatement*>(statement);

			m_writer.BeginLine(indent, RawStr(whileStatement->fileName), whileStatement->line);
			m_writer.Write("while (");

			OutputExpression(whileStatement->condition);
			m_writer.Write(") {");
			m_writer.EndLine();
			OutputStatements(indent + 1, whileStatement->statement);
			m_writer.WriteLine(indent, "}");
		}
		else if (statement->nodeType == HLSLNodeType_BlockStatement)
		{
			HLSLBlockStatement* blockStatement = static_cast<HLSLBlockStatement*>(statement);
			m_writer.WriteLineTagged(indent, RawStr(blockStatement->fileName), blockStatement->line, "{");
			OutputStatements(indent + 1, blockStatement->statement);
			m_writer.WriteLine(indent, "}");
		}
        else
        {
            // Unhanded statement type.
			ASSERT_PARSER(0);
        }

        statement = statement->nextStatement;
    }
}

void GLSLGenerator::OutputBuffer(int indent, HLSLBuffer* buffer)
{
    // Empty uniform blocks cause compilation errors on NVIDIA, so don't emit them.
    if (buffer->field == NULL)
        return;

    if (m_options.flags & Flag_EmulateConstantBuffer)
    {
        unsigned int size = 0;
        LayoutBuffer(buffer, size);

        unsigned int uniformSize = (size + 3) / 4;

        m_writer.WriteLineTagged(indent, RawStr(buffer->fileName), buffer->line, "uniform vec4 %s%s[%d];", m_options.constantBufferPrefix, RawStr(buffer->name), uniformSize);
    }
    else
    {
        m_writer.WriteLineTagged(indent, RawStr(buffer->fileName), buffer->line, "layout (std140) uniform %s%s", m_options.constantBufferPrefix, RawStr(buffer->name));
		m_writer.Write(indent, "{");
        HLSLDeclaration* field = buffer->field;
        while (field != NULL)
        {
            m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);
            OutputDeclaration(field->type, field->name);
            m_writer.Write(";");
            m_writer.EndLine();
            field = (HLSLDeclaration*)field->nextStatement;
        }
        m_writer.WriteLine(indent, "};");
    }
}

inline void alignForWrite(unsigned int& offset, unsigned int size)
{
	ASSERT_PARSER(size <= 4);

    if (offset / 4 != (offset + size - 1) / 4)
        offset = (offset + 3) & ~3;
}

void GLSLGenerator::LayoutBuffer(HLSLBuffer* buffer, unsigned int& offset)
{
    for (HLSLDeclaration* field = buffer->field; field; field = (HLSLDeclaration*)field->nextStatement)
    {
        LayoutBuffer(field->type, offset);
    }
}

void GLSLGenerator::LayoutBuffer(const HLSLType& type, unsigned int& offset)
{
    LayoutBufferAlign(type, offset);

    if (type.array)
    {
        int arraySize = type.arrayExtent[0];

        unsigned int elementSize = 0;
        LayoutBufferElement(type, elementSize);

        unsigned int alignedElementSize = (elementSize + 3) & ~3;

        offset += alignedElementSize * arraySize;
    }
    else
    {
        LayoutBufferElement(type, offset);
    }
}

void GLSLGenerator::LayoutBufferElement(const HLSLType& type, unsigned int& offset)
{
    if (type.baseType == HLSLBaseType_Float)
    {
        offset += 1;
    }
    else if (type.baseType == HLSLBaseType_Float2)
    {
        offset += 2;
    }
    else if (type.baseType == HLSLBaseType_Float3)
    {
        offset += 3;
    }
    else if (type.baseType == HLSLBaseType_Float4)
    {
        offset += 4;
    }
    else if (type.baseType == HLSLBaseType_Float4x4)
    {
        offset += 16;
    }
    else if (type.baseType == HLSLBaseType_UserDefined)
    {
        HLSLStruct * st = m_tree->FindGlobalStruct(type.typeName);

        if (st)
        {
            for (HLSLStructField* field = st->field; field; field = field->nextField)
            {
                LayoutBuffer(field->type, offset);
            }
        }
        else
        {
            Error("Unknown type %s", FetchCstr(m_stringLibrary, type.typeName));
        }
    }
    else
    {
        Error("Constant buffer layout is not supported for %s", CHECK_CSTR(GetTypeName(type)));
    }
}

void GLSLGenerator::LayoutBufferAlign(const HLSLType& type, unsigned int& offset)
{
    if (type.array)
    {
        alignForWrite(offset, 4);
    }
    else if (type.baseType == HLSLBaseType_Float)
    {
        alignForWrite(offset, 1);
    }
    else if (type.baseType == HLSLBaseType_Float2)
    {
        alignForWrite(offset, 2);
    }
    else if (type.baseType == HLSLBaseType_Float3)
    {
        alignForWrite(offset, 3);
    }
    else if (type.baseType == HLSLBaseType_Float4)
    {
        alignForWrite(offset, 4);
    }
    else if (type.baseType == HLSLBaseType_Float4x4)
    {
        alignForWrite(offset, 4);
    }
    else if (type.baseType == HLSLBaseType_UserDefined)
    {
        alignForWrite(offset, 4);
    }
    else
    {
        Error("Constant buffer layout is not supported for %s", CHECK_CSTR(GetTypeName(type)));
    }
}

HLSLBuffer* GLSLGenerator::GetBufferAccessExpression(HLSLExpression* expression)
{
	if (expression->nodeType == HLSLNodeType_IdentifierExpression)
	{
		HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
		HLSLDeclaration * declaration = identifierExpression->pDeclaration;
		ASSERT_PARSER(declaration);

		if (declaration->global && declaration->buffer)
			return declaration->buffer;
	}
    else if (expression->nodeType == HLSLNodeType_MemberAccess)
    {
        HLSLMemberAccess* memberAccess = static_cast<HLSLMemberAccess*>(expression);

        if (memberAccess->object->expressionType.baseType == HLSLBaseType_UserDefined)
            return GetBufferAccessExpression(memberAccess->object);
    }
    else if (expression->nodeType == HLSLNodeType_ArrayAccess)
    {
        HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);

        if (arrayAccess->array->expressionType.array)
            return GetBufferAccessExpression(arrayAccess->array);
    }

    return 0;
}

void GLSLGenerator::OutputBufferAccessExpression(HLSLBuffer* buffer, HLSLExpression* expression, const HLSLType& type, unsigned int postOffset)
{
    if (type.array)
    {
        Error("Constant buffer access is not supported for arrays (use indexing instead)");
    }
    else if (type.baseType == HLSLBaseType_Float || type.baseType == HLSLBaseType_Half || type.baseType == HLSLBaseType_Min16Float || type.baseType == HLSLBaseType_Min10Float)
    {
        m_writer.Write("%s%s[", CHECK_CSTR(m_options.constantBufferPrefix), FetchCstr(m_stringLibrary, buffer->name));
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
        m_writer.Write("%d].%c", index / 4, "xyzw"[index % 4]);
    }
    else if (type.baseType == HLSLBaseType_Float2 || type.baseType == HLSLBaseType_Half2 || type.baseType == HLSLBaseType_Min16Float2 || type.baseType == HLSLBaseType_Min10Float2)
    {
        m_writer.Write("%s%s[", CHECK_CSTR(m_options.constantBufferPrefix), FetchCstr(m_stringLibrary, buffer->name));
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
        m_writer.Write("%d].%s", index / 4, index % 4 == 0 ? CHECK_CSTR("xy") : index % 4 == 1 ? CHECK_CSTR("yz") : CHECK_CSTR("zw"));
    }
    else if (type.baseType == HLSLBaseType_Float3 || type.baseType == HLSLBaseType_Half3 || type.baseType == HLSLBaseType_Min16Float3 || type.baseType == HLSLBaseType_Min10Float3)
    {
        m_writer.Write("%s%s[", CHECK_CSTR(m_options.constantBufferPrefix), FetchCstr(m_stringLibrary, buffer->name));
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
        m_writer.Write("%d].%s", index / 4, index % 4 == 0 ? CHECK_CSTR("xyz") : CHECK_CSTR("yzw"));
    }
    else if (type.baseType == HLSLBaseType_Float4 || type.baseType == HLSLBaseType_Half4 || type.baseType == HLSLBaseType_Min16Float4 || type.baseType == HLSLBaseType_Min10Float4)
    {
        m_writer.Write("%s%s[", CHECK_CSTR(m_options.constantBufferPrefix), FetchCstr(m_stringLibrary, buffer->name));
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
		ASSERT_PARSER(index % 4 == 0);
        m_writer.Write("%d]", index / 4);
    }
    else if (type.baseType == HLSLBaseType_Float4x4 || type.baseType == HLSLBaseType_Half4x4 || type.baseType == HLSLBaseType_Min16Float4x4 || type.baseType == HLSLBaseType_Min10Float4x4)
    {
		if(type.baseType == HLSLBaseType_Float4x4)
			m_writer.Write("mat4(");
		if (type.baseType == HLSLBaseType_Half4x4)
			m_writer.Write("mediump mat4(");
		if (type.baseType == HLSLBaseType_Min16Float4x4)
			m_writer.Write("mediump mat4(");
		if (type.baseType == HLSLBaseType_Min10Float4x4)
			m_writer.Write("lowp mat4(");
        for (int i = 0; i < 4; ++i)
        {
            m_writer.Write("%s%s[", CHECK_CSTR(m_options.constantBufferPrefix), FetchCstr(m_stringLibrary, buffer->name));
            unsigned int index = OutputBufferAccessIndex(expression, postOffset + i * 4);
			ASSERT_PARSER(index % 4 == 0);
            m_writer.Write("%d]%c", index / 4, i == 3 ? ')' : ',');
        }
    }
    else if (type.baseType == HLSLBaseType_UserDefined)
    {
        HLSLStruct * st = m_tree->FindGlobalStruct(type.typeName);

        if (st)
        {
            m_writer.Write("%s(", FetchCstr(m_stringLibrary, st->name));

            unsigned int offset = postOffset;

            for (HLSLStructField* field = st->field; field; field = field->nextField)
            {
                OutputBufferAccessExpression(buffer, expression, field->type, offset);

                if (field->nextField)
                    m_writer.Write(",");

                LayoutBuffer(field->type, offset);
            }

            m_writer.Write(")");
        }
        else
        {
            Error("Unknown type %s", FetchCstr(m_stringLibrary, type.typeName));
        }
    }
    else
    {
        Error("Constant buffer layout is not supported for %s", CHECK_CSTR(GetTypeName(type)));
    }
}

unsigned int GLSLGenerator::OutputBufferAccessIndex(HLSLExpression* expression, unsigned int postOffset)
{
    if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
        HLSLDeclaration * declaration = identifierExpression->pDeclaration;
		ASSERT_PARSER(declaration && declaration->global);

        HLSLBuffer * buffer = declaration->buffer;
		ASSERT_PARSER(buffer);

        unsigned int offset = 0;

        for (HLSLDeclaration* field = buffer->field; field; field = (HLSLDeclaration*)field->nextStatement)
        {
            if (field == declaration)
            {
                LayoutBufferAlign(field->type, offset);
                break;
            }

            LayoutBuffer(field->type, offset);
        }

        return offset + postOffset;
    }
    else if (expression->nodeType == HLSLNodeType_MemberAccess)
    {
        HLSLMemberAccess* memberAccess = static_cast<HLSLMemberAccess*>(expression);

        const HLSLType& type = memberAccess->object->expressionType;
		ASSERT_PARSER(type.baseType == HLSLBaseType_UserDefined);

        HLSLStruct * st = m_tree->FindGlobalStruct(type.typeName);

        if (st)
        {
            unsigned int offset = 0;

            for (HLSLStructField* field = st->field; field; field = field->nextField)
            {
                if (field->name == memberAccess->field)
                {
                    LayoutBufferAlign(field->type, offset);
                    break;
                }

                LayoutBuffer(field->type, offset);
            }

            return offset + OutputBufferAccessIndex(memberAccess->object, postOffset);
        }
        else
        {
            Error("Unknown type %s", FetchCstr(m_stringLibrary, type.typeName));
        }
    }
    else if (expression->nodeType == HLSLNodeType_ArrayAccess)
    {
        HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);

        const HLSLType& type = arrayAccess->array->expressionType;
		ASSERT_PARSER(type.array);

        unsigned int elementSize = 0;
        LayoutBufferElement(type, elementSize);

        unsigned int alignedElementSize = (elementSize + 3) & ~3;

        int arrayIndex = 0;
        if (m_tree->GetExpressionValue(arrayAccess->index, arrayIndex))
        {
            unsigned int offset = arrayIndex * alignedElementSize;

            return offset + OutputBufferAccessIndex(arrayAccess->array, postOffset);
        }
        else
        {
            m_writer.Write("%d*(", alignedElementSize / 4);
            OutputExpression(arrayAccess->index);
            m_writer.Write(")+");

            return OutputBufferAccessIndex(arrayAccess->array, postOffset);
        }
    }
    else
    {
		ASSERT_PARSER(!"IsBufferAccessExpression should have returned false");
    }

    return 0;
}

HLSLFunction* GLSLGenerator::FindFunction(HLSLRoot* root, const CachedString & name)
{
    HLSLStatement* statement = root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction* function = static_cast<HLSLFunction*>(statement);
            if (String_Equal(function->name, name))
            {
                return function;
            }
        }
        statement = statement->nextStatement;
    }
    return NULL;
}

HLSLStruct* GLSLGenerator::FindStruct(HLSLRoot* root, const CachedString & name)
{
    HLSLStatement* statement = root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct* structDeclaration = static_cast<HLSLStruct*>(statement);
            if (String_Equal(structDeclaration->name, name))
            {
                return structDeclaration;
            }
        }
        statement = statement->nextStatement;
    }
    return NULL;
}


CachedString GLSLGenerator::GetAttribQualifier(AttributeModifier modifier)
{
    if (m_versionLegacy)
    {
        if (m_target == Target_VertexShader)
            return (modifier == AttributeModifier_In) ? MakeCached("attribute") : MakeCached("varying");
        else
            return (modifier == AttributeModifier_In) ? MakeCached("varying") : MakeCached("out");
    }
    else
    {
        return (modifier == AttributeModifier_In) ? MakeCached("in") : MakeCached("out");
    }
}

void GLSLGenerator::OutputAttribute(const HLSLType& type, const CachedString & semantic, AttributeModifier modifier, int *counter)
{
    CachedString qualifier = GetAttribQualifier(modifier);
    CachedString prefix = (modifier == AttributeModifier_In) ? m_inAttribPrefix : m_outAttribPrefix;

	HLSLRoot* root = m_tree->GetRoot();
    if (type.baseType == HLSLBaseType_UserDefined)
    {
        // If the argument is a struct with semantics specified, we need to
        // grab them.
        HLSLStruct* structDeclaration = FindStruct(root, type.typeName);
		ASSERT_PARSER(structDeclaration != NULL);
        HLSLStructField* field = structDeclaration->field;
        while (field != NULL)
        {
			CachedString builtinSemantic = GetBuiltInSemantic(field->semantic, modifier, field->type);

            if (field->semantic.IsNotEmpty() && builtinSemantic.IsEmpty())
            {
				m_writer.Write("layout(location = %d) ", *counter);

				*counter += TypeArraySize(field->type);

				if( (m_target == Target_HullShader && modifier == AttributeModifier_Out) || (m_target == Target_DomainShader && modifier == AttributeModifier_In))
					m_writer.Write("patch %s ", FetchCstr(m_stringLibrary, qualifier));
				else
					m_writer.Write("%s ", FetchCstr(m_stringLibrary, qualifier) );
				
				char attribName[ 64 ];
				String_Printf( attribName, 64, "%s%s", FetchCstr(m_stringLibrary, prefix), FetchCstr(m_stringLibrary, field->semantic));

				if(field->type.baseType == HLSLBaseType_Int || field->type.baseType == HLSLBaseType_Uint && m_target != Target_VertexShader)
					m_writer.Write("flat ");				
					
				OutputDeclaration(field->type, MakeCached(attribName));

				if (m_target == Target_GeometryShader && modifier == AttributeModifier_In)
					m_writer.Write("[]");
				
				m_writer.EndLine(";");
            }
            field = field->nextField;
        }
    }
    else if (semantic.IsNotEmpty() && GetBuiltInSemantic(semantic, modifier, type).IsEmpty())
    {
		m_writer.Write("layout(location = %d) ", (*counter)++);
		m_writer.Write( "%s ", FetchCstr(m_stringLibrary, qualifier) );
		char attribName[ 64 ];
		String_Printf( attribName, 64, "%s%s", FetchCstr(m_stringLibrary, prefix), FetchCstr(m_stringLibrary, semantic) );

		if (type.baseType == HLSLBaseType_Int || type.baseType == HLSLBaseType_Uint && m_target != Target_VertexShader)
			m_writer.Write("flat ");

		OutputDeclaration( type, MakeCached(attribName) );
		m_writer.EndLine(";");
    }
	else if (semantic.IsEmpty() && (m_target == Target_HullShader || m_target == Target_DomainShader))
	{
		HLSLStruct* structDeclaration = FindStruct(root, type.typeName);
		ASSERT_PARSER(structDeclaration != NULL);

		HLSLStructField* field = structDeclaration->field;
		while (field != NULL)
		{
			if (field->semantic.IsNotEmpty())
			{
				CachedString builtInSemantic = GetBuiltInSemantic(field->semantic, AttributeModifier_In);

				m_writer.Write("layout(location = %d) ", (*counter)++);


				if (m_target == Target_DomainShader && modifier == AttributeModifier_In)
					m_writer.Write("patch ");

				m_writer.Write("%s ", FetchCstr(m_stringLibrary, qualifier));
				
				if (m_target == Target_HullShader && type.array)
				{
					field->type.array = true;
					field->type.arrayDimension = type.arrayDimension;
					field->type.arrayExtent[0] = type.arrayExtent[0];
					field->type.arrayExtent[1] = type.arrayExtent[1];
					field->type.arrayExtent[2] = type.arrayExtent[2];
				}
				else if (m_target == Target_DomainShader)
				{					
					field->type.array = false;
				}

				char attribName[64];
				String_Printf(attribName, 64, "%s%s", RawStr(m_inAttribPrefix), RawStr(field->semantic));
				

				if (builtInSemantic.IsNotEmpty())
				{
					//m_writer.WriteLine(1, "%s.%s = %s;", GetSafeIdentifierName(argument->name), GetSafeIdentifierName(field->name), builtInSemantic);
				}
				else
				{
					OutputDeclaration(field->type, MakeCached(attribName));
					//m_writer.Write("%s%s", , field->semantic);
					//m_writer.WriteLine(0, %s%s;", GetSafeIdentifierName(argument->name), GetSafeIdentifierName(field->name), m_inAttribPrefix, field->semantic);
				}
				m_writer.EndLine(";");
			}
			field = field->nextField;
		}
	}
}

void GLSLGenerator::OutputAttributes(HLSLFunction* entryFunction)
{
    // Write out the input/output attributes to the shader.
	int inputCounter = 0;
	int outputCounter = 0;

	eastl::vector < HLSLArgument * > argumentVec = entryFunction->args;

	for (int i = 0; i < argumentVec.size(); i++)
    {
		HLSLArgument * argument = argumentVec[i];

        if (argument->modifier == HLSLArgumentModifier_None || argument->modifier == HLSLArgumentModifier_In)
            OutputAttribute(argument->type, argument->semantic, AttributeModifier_In, &inputCounter);
        if (argument->modifier == HLSLArgumentModifier_Out)
            OutputAttribute(argument->type, argument->semantic, AttributeModifier_Out, &outputCounter);

		if (m_target == Target_GeometryShader)
		{
			if (argument->modifier == HLSLArgumentModifier_Point)
			{
				m_writer.WriteLine(0, "layout(points) in;");
				OutputAttribute(argument->type, argument->semantic, AttributeModifier_In, &inputCounter);
				String_Equal(m_geoInputdataType, argument->type.typeName);
			}
			else if (argument->modifier == HLSLArgumentModifier_Line)
			{
				m_writer.WriteLine(0, "layout(lines) in;");
				OutputAttribute(argument->type, argument->semantic, AttributeModifier_In, &inputCounter);
				String_Equal(m_geoInputdataType, argument->type.typeName);
			}
			else if (argument->modifier == HLSLArgumentModifier_Lineadj)
			{
				m_writer.WriteLine(0, "layout(lines_adjacency) in;");
				OutputAttribute(argument->type, argument->semantic, AttributeModifier_In, &inputCounter);
				String_Equal(m_geoInputdataType, argument->type.typeName);
			}
			else if (argument->modifier == HLSLArgumentModifier_Triangle)
			{
				m_writer.WriteLine(0, "layout(triangles) in;");
				OutputAttribute(argument->type, argument->semantic, AttributeModifier_In, &inputCounter);
				String_Equal(m_geoInputdataType, argument->type.typeName);
			}
			else if (argument->modifier == HLSLArgumentModifier_Triangleadj)
			{
				m_writer.WriteLine(0, "layout(triangles_adjacency) in;");
				OutputAttribute(argument->type, argument->semantic, AttributeModifier_In, &inputCounter);
				String_Equal(m_geoInputdataType, RawStr(argument->type.typeName));
			}
			else if (argument->modifier == HLSLArgumentModifier_Inout)
			{
				if (argument->type.baseType == HLSLBaseType_PointStream)
				{
					m_outputGeometryType = MakeCached("points");
				}
				else if (argument->type.baseType == HLSLBaseType_LineStream)
				{
					m_outputGeometryType = MakeCached("line_strip");
				}
				else if (argument->type.baseType == HLSLBaseType_TriangleStream)
				{
					m_outputGeometryType = MakeCached("triangle_strip");
				}

				if (argument->type.structuredTypeName.IsNotEmpty())
				{
					argument->type.baseType = HLSLBaseType_UserDefined;
					argument->type.typeName = argument->type.structuredTypeName;

					OutputAttribute(argument->type, argument->semantic, AttributeModifier_Out, &outputCounter);
					m_geoOutputdataType = argument->type.typeName;
				}
				else
				{
					OutputAttribute(argument->type, argument->semantic, AttributeModifier_Out, &outputCounter);
					m_geoOutputdataType = argument->type.typeName;
				}
			}
		}
    }

    // Write out the output attributes from the shader.
    OutputAttribute(entryFunction->returnType, entryFunction->semantic, AttributeModifier_Out, &outputCounter);
}

void GLSLGenerator::OutputSetOutAttribute(const char* semantic, const CachedString & resultName)
{
    int outputIndex = -1;
    CachedString builtInSemantic = GetBuiltInSemantic(MakeCached(semantic), AttributeModifier_Out, &outputIndex);
    if (builtInSemantic.IsNotEmpty())
    {
        if (String_Equal(builtInSemantic, "gl_Position"))
        {
            if (m_options.flags & Flag_FlipPositionOutput)
            {
                // Mirror the y-coordinate when we're outputing from
                // the vertex shader so that we match the D3D texture
                // coordinate origin convention in render-to-texture
                // operations.
                // We also need to convert the normalized device
                // coordinates from the D3D convention of 0 to 1 to the
                // OpenGL convention of -1 to 1.
                m_writer.WriteLine(1, "vec4 temp = %s;", FetchCstr(m_stringLibrary, resultName));
                m_writer.WriteLine(1, "%s = temp * vec4(1,-1,2,1) - vec4(0,0,temp.w,0);", FetchCstr(m_stringLibrary, builtInSemantic));
            }
            else
            {
                m_writer.WriteLine(1, "%s = %s;", FetchCstr(m_stringLibrary, builtInSemantic), FetchCstr(m_stringLibrary, resultName));
            }

			// POSITION is not needed, gets output anyways as a regular semantict gl_POsition
/*
			if (String_Equal(semantic, "POSITION"))
			{
				m_writer.WriteLine(1, "%s%s = %s;", FetchCstr(m_stringLibrary, m_outAttribPrefix), CHECK_CSTR(semantic), FetchCstr(m_stringLibrary, resultName));
			}
*/
            m_outputPosition = true;
        }
        else if (String_Equal(builtInSemantic, "gl_FragDepth"))
        {
            // If the value goes outside of the 0 to 1 range, the
            // fragment will be rejected unlike in D3D, so clamp it.
            m_writer.WriteLine(1, "%s = clamp(float(%s), 0.0, 1.0);", FetchCstr(m_stringLibrary, builtInSemantic), FetchCstr(m_stringLibrary, resultName));
        }
        else if (outputIndex >= 0)
        {
            //m_writer.WriteLine(1, "%s[%d] = %s;", builtInSemantic, outputIndex, resultName);
			m_writer.WriteLine(1, "%s%d = %s;", FetchCstr(m_stringLibrary, builtInSemantic), outputIndex, FetchCstr(m_stringLibrary, resultName));
        }
        else
        {
            m_writer.WriteLine(1, "%s = %s;", FetchCstr(m_stringLibrary, builtInSemantic), FetchCstr(m_stringLibrary, resultName));
        }
    }
    else if (m_target == Target_FragmentShader)
    {
        Error("Output attribute %s does not map to any built-ins", CHECK_CSTR(semantic));
    }
    else
    {
        m_writer.WriteLine(1, "%s%s = %s;", FetchCstr(m_stringLibrary, m_outAttribPrefix), CHECK_CSTR(semantic), FetchCstr(m_stringLibrary, resultName));
    }
}

void GLSLGenerator::OutputEntryCaller(HLSLFunction* entryFunction)
{
    HLSLRoot* root = m_tree->GetRoot();

    m_writer.WriteLine(0, "void main()");
	m_writer.WriteLine(0, "{");

    // Create local variables for each of the parameters we'll need to pass
    // into the entry point function.
	eastl::vector < HLSLArgument * > argumentVec = entryFunction->args;
	for (int i = 0; i < argumentVec.size(); i++)
    {
		HLSLArgument * argument = argumentVec[i];
		//DomainShader
		if (m_target == Target_DomainShader && (i == 0 || argument->type.baseType == HLSLBaseType_OutputPatch ))
		{
			//argument = argument->nextArgument;
			continue;
		}

        m_writer.BeginLine(1);
        OutputDeclaration(argument->type, argument->name);
        m_writer.EndLine(";");

		if ( !( (argument->modifier == HLSLArgumentModifier_Out) || (argument->modifier == HLSLArgumentModifier_Inout)))
        {
            // Set the value for the local variable.
            if (argument->type.baseType == HLSLBaseType_UserDefined)
            {
				HLSLStruct* structDeclaration = FindStruct(root, argument->type.typeName);
				ASSERT_PARSER(structDeclaration != NULL);
                
				
				if (argument->type.array)
				{
					int arraySize = argument->type.arrayExtent[0];

					for (int i = 0; i < arraySize; i++)
					{
						HLSLStructField* field = structDeclaration->field;

						while (field != NULL)
						{
							if (field->semantic.IsNotEmpty())
							{
								CachedString builtInSemantic = GetBuiltInSemantic(field->semantic, AttributeModifier_In);
								if (builtInSemantic.IsNotEmpty())
								{
									if(String_Equal(builtInSemantic, "gl_in"))
										m_writer.WriteLine(1, "%s[%d].%s = %s[%d].gl_Position;", RawStr(GetSafeIdentifierName(argument->name)), i, RawStr(GetSafeIdentifierName(field->name)), RawStr(builtInSemantic), i);
									else
										m_writer.WriteLine(1, "%s[%d].%s = %s;", RawStr(GetSafeIdentifierName(argument->name)), i, RawStr(GetSafeIdentifierName(field->name)), RawStr(builtInSemantic));
								}
								else
								{
									m_writer.WriteLine(1, "%s[%d].%s = %s%s[%d];", RawStr(GetSafeIdentifierName(argument->name)), i, RawStr(GetSafeIdentifierName(field->name)), RawStr(m_inAttribPrefix), RawStr(field->semantic), i);
								}
							}
							field = field->nextField;
						}
					}
				}
				else
				{
					HLSLStructField* field = structDeclaration->field;

					while (field != NULL)
					{
						if (field->semantic.IsNotEmpty())
						{
							CachedString builtInSemantic = GetBuiltInSemantic(field->semantic, AttributeModifier_In);
							if (builtInSemantic.IsNotEmpty())
							{
								m_writer.WriteLine(1, "%s.%s = %s;", RawStr(GetSafeIdentifierName(argument->name)), RawStr(GetSafeIdentifierName(field->name)), RawStr(builtInSemantic));
							}
							else
							{
								m_writer.WriteLine(1, "%s.%s = %s%s;", RawStr(GetSafeIdentifierName(argument->name)), RawStr(GetSafeIdentifierName(field->name)), RawStr(m_inAttribPrefix), RawStr(field->semantic));
							}
						}
						field = field->nextField;
					}
				}
				
				
            }
            else if (argument->semantic.IsNotEmpty())
            {
                CachedString builtInSemantic = GetBuiltInSemantic(argument->semantic, AttributeModifier_In);
                if (builtInSemantic.IsNotEmpty())
                {
					if (m_target == Target_DomainShader && String_Equal("gl_TessCoord", builtInSemantic))
					{
						char additionalBuiltIn[64];

						if (String_Equal(m_domain, "\"quad\"") || String_Equal(m_domain, "\"isoline\""))
							String_Printf(additionalBuiltIn, 64, "%s.xy", RawStr(builtInSemantic));
						else if (String_Equal(m_domain, "\"tri\""))
							String_Printf(additionalBuiltIn, 64, "%s.xyz", RawStr(builtInSemantic));

						m_writer.WriteLine(1, "%s = %s;", RawStr(GetSafeIdentifierName(argument->name)), additionalBuiltIn);
					}
					else
						m_writer.WriteLine(1, "%s = %s;", RawStr(GetSafeIdentifierName(argument->name)), RawStr(builtInSemantic));
                }
                else
                {
                    m_writer.WriteLine(1, "%s = %s%s;", RawStr(GetSafeIdentifierName(argument->name)), RawStr(m_inAttribPrefix), RawStr(argument->semantic));
                }
            }
        }
    }

	m_writer.BeginLine(0);
	//Need to call PatchConstantFunction in Main, if it is a Hull shader
	if (m_target == Target_HullShader)
	{
		//remove double quotes
		if (String_Equal(m_patchconstantfunc, "") || m_patchconstantfunc.IsEmpty())
		{
			// need error handling
			return;
		}

		char newFuncName[64];
		strcpy(newFuncName, RawStr(m_patchconstantfunc) + 1);
		newFuncName[strlen(newFuncName)-1] = 0;

		m_writer.Write("%s(", CHECK_CSTR(newFuncName));

		int firstArgument = 0;

		//DomainShader
		if (m_target == Target_DomainShader) // && entryFunction->argument == argument)
		{
			firstArgument = 1;
		}

		int numWritten = 0;
		for (int i = firstArgument; i < argumentVec.size(); i++)
		{
			HLSLArgument * argument = argumentVec[i];
			if (argument->type.baseType == HLSLBaseType_InputPatch || argument->type.baseType == HLSLBaseType_OutputPatch)
			{
				continue;
			}
			else
			{
			}

			if (numWritten > 0)
			{
				m_writer.Write(", ");
			}

			m_writer.Write("%s", FetchCstr(m_stringLibrary, GetSafeIdentifierName(argument->name)));

			numWritten++;
		}

		m_writer.EndLine(");");
	}

    const char* resultName = "result";

    // Call the original entry function.
    m_writer.BeginLine(1);
    if (entryFunction->returnType.baseType != HLSLBaseType_Void)
        m_writer.Write("%s %s = ", CHECK_CSTR(GetTypeName(entryFunction->returnType)), CHECK_CSTR(resultName));
    
	if(String_Equal(m_entryName, "main"))
		m_writer.Write("HLSL%s(", FetchCstr(m_stringLibrary, m_entryName));
	else
		m_writer.Write("%s(", FetchCstr(m_stringLibrary, m_entryName));
	
	int firstArgument = 0;

	//DomainShader
	if (m_target == Target_DomainShader)
	{
		firstArgument = 1;
	}

	int numWritten = 0;
	for (int i = firstArgument; i < argumentVec.size(); i++)
    {		
		HLSLArgument * argument = argumentVec[i];
		if (argument->type.baseType == HLSLBaseType_InputPatch || argument->type.baseType == HLSLBaseType_OutputPatch)
		{
			continue;
		}
		else if (m_target == Target_GeometryShader && argument->modifier == HLSLArgumentModifier_Inout)
		{
			continue;
		}
		else
		{
		}

        if (numWritten > 0)
        {
            m_writer.Write(", ");
        }

		m_writer.Write("%s", FetchCstr(m_stringLibrary, GetSafeIdentifierName(argument->name)));
        
		numWritten++;
    }
    m_writer.EndLine(");");

    // Copy values from the result into the out attributes as necessary.
	for (int i = 0; i < argumentVec.size(); i++)
    {
		HLSLArgument * argument = argumentVec[i];
        if (argument->modifier == HLSLArgumentModifier_Out && argument->semantic.IsNotEmpty())
            OutputSetOutAttribute(RawStr(argument->semantic), GetSafeIdentifierName(argument->name));
    }

    if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
    {
		HLSLStruct* structDeclaration = FindStruct(root, entryFunction->returnType.typeName);
		ASSERT_PARSER(structDeclaration != NULL);
		HLSLStructField* field = structDeclaration->field;
		while (field != NULL)
		{
			char fieldResultName[1024];

			if (field->name.IsEmpty())
			{
				field = field->nextField;
				continue;
			}


			String_Printf(fieldResultName, sizeof(fieldResultName), "%s.%s", resultName, RawStr(field->name));
			OutputSetOutAttribute(RawStr(field->semantic), MakeCached(fieldResultName));

			if (m_target == Target_HullShader)
			{
				if (field->semantic.IsEmpty())
				{
					if(String_Equal(field->semantic, "POSITION"))
					{
						m_writer.WriteLine(1, "gl_out[gl_InvocationID].gl_Position = %s%s;", RawStr(m_outAttribPrefix), RawStr(field->semantic));
					}
				}
			}

			field = field->nextField;
		}
    }
    else if (entryFunction->semantic.IsNotEmpty())
    {
		// translate the name
        OutputSetOutAttribute(RawStr(entryFunction->semantic), MakeCached(resultName));
    }

    m_writer.WriteLine(0, "}");
}

void GLSLGenerator::OutputDeclaration(HLSLDeclaration* declaration)
{
	OutputDeclarationType( declaration->type );

	HLSLDeclaration* lastDecl = NULL;
	while( declaration )
	{
		if( lastDecl )
			m_writer.Write( ", " );

		OutputDeclarationBody( declaration->type, GetSafeIdentifierName( declaration->name) );

		if( declaration->assignment != NULL )
		{
			m_writer.Write( " = " );
			OutputExpression( declaration->assignment, &declaration->type );
		}

		lastDecl = declaration;
		declaration = declaration->nextDeclaration;
	}
}

void GLSLGenerator::OutputDeclaration(const HLSLType& type, const CachedString & name)
{
	if (!(type.baseType == HLSLBaseType_InputPatch ||
		type.baseType == HLSLBaseType_OutputPatch /*||
		
		type.baseType == HLSLBaseType_PointStream ||
		type.baseType == HLSLBaseType_LineStream ||
		type.baseType == HLSLBaseType_TriangleStream
		*/
		))
	{
		OutputDeclarationType(type);
		OutputDeclarationBody(type, name);
	}

	
}

void GLSLGenerator::OutputDeclarationType( const HLSLType& type )
{
	
	m_writer.Write( "%s ", CHECK_CSTR(GetTypeName( type )) );
}

void GLSLGenerator::OutputDeclarationBody( const HLSLType& type, const CachedString & name)
{
	if( !type.array )
	{
		m_writer.Write( "%s", FetchCstr(m_stringLibrary, GetSafeIdentifierName( name)) );
	}
	else
	{
		
		m_writer.Write("%s[", FetchCstr(m_stringLibrary, GetSafeIdentifierName(name)));

		if (type.arrayExtent[0])		
			m_writer.Write("%d", type.arrayExtent[0]);
		
		m_writer.Write("]");
	
	}

}

void GLSLGenerator::OutputCast(const HLSLType& type)
{
    if (m_version == Version_110 && (type.baseType == HLSLBaseType_Float3x3 || type.baseType == HLSLBaseType_Half3x3 || type.baseType == HLSLBaseType_Min16Float3x3 || type.baseType == HLSLBaseType_Min10Float3x3))
        m_writer.Write("%s", FetchCstr(m_stringLibrary, m_matrixCtorFunction));
    else
		m_writer.Write("%s", GetBaseTypeConstructor(type.baseType));
}

void GLSLGenerator::Error(const char* format, ...)
{
    // It's not always convenient to stop executing when an error occurs,
    // so just track once we've hit an error and stop reporting them until
    // we successfully bail out of execution.
    if (m_error)
    {
        return;
    }
    m_error = true;

    va_list arg;
    va_start(arg, format);
    Log_ErrorArgList(format, arg);
    va_end(arg);
} 

CachedString GLSLGenerator::GetSafeIdentifierName(const CachedString & name) const
{
	for (int i = 0; i < s_numReservedWords; ++i)
	{
		if (String_Equal(s_reservedWord[i], name))
		{
			return m_reservedWord[i];
		}
	}
	return name;
}

bool GLSLGenerator::ChooseUniqueName(const char* base, CachedString & dstName) const
{
	// IC: Try without suffix first.
	char dst[1024];
	int dstLength = 1024;
	String_Printf(dst, dstLength, "%s", CHECK_CSTR(base));
	if (!m_tree->GetContainsString(base))
	{
		dstName = m_tree->AddStringCached(dst);
		return true;
	}

	for (int i = 1; i < 1024; ++i)
	{
		String_Printf(dst, dstLength, "%s%d", CHECK_CSTR(base), i);
		if (!m_tree->GetContainsString(dst))
		{
			dstName = m_tree->AddStringCached(dst);
			return true;
		}
	}
	return false;
}



CachedString GLSLGenerator::GetBuiltInSemantic(const CachedString & semantic, AttributeModifier modifier, int* outputIndex)
{
    if (outputIndex)
        *outputIndex = -1;
	
    if ((m_target == Target_VertexShader || m_target == Target_DomainShader) && modifier == AttributeModifier_Out && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
        return MakeCached("gl_Position");

	//if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "POSITION") && !m_outputPosition)
	//	return MakeCached("gl_Position");

    if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "PSIZE"))
        return MakeCached("gl_PointSize");

    if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_InstanceID"))
        return MakeCached("gl_InstanceIndex");

	if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_VertexID"))
		return MakeCached("gl_VertexIndex");

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out && String_Equal(semantic, "SV_Depth"))
		return MakeCached("gl_FragDepth");

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out && String_Equal(semantic, "SV_DEPTH"))
		return MakeCached("gl_FragDepth");

	//https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return MakeCached("vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w)");

	if (m_target == Target_GeometryShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return MakeCached("gl_in");

	if (m_target == Target_GeometryShader && modifier == AttributeModifier_Out && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
			return MakeCached("gl_Position");
	
	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_DispatchThreadID")))
		return MakeCached("gl_GlobalInvocationID");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return MakeCached("gl_WorkGroupID");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return MakeCached("gl_WorkGroupID");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupIndex")))
		return MakeCached("gl_LocalInvocationIndex");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupThreadID")))
		return MakeCached("gl_LocalInvocationID");

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_SampleIndex")))
		return MakeCached("gl_SampleID");

	if ((m_target == Target_HullShader || m_target == Target_DomainShader) && (String_Equal(semantic, "SV_InsideTessFactor")))
		return MakeCached("gl_TessLevelInner");

	if ((m_target == Target_HullShader || m_target == Target_DomainShader)&& (String_Equal(semantic, "SV_TessFactor")))
		return MakeCached("gl_TessLevelOuter");

	if (m_target == Target_HullShader && (String_Equal(semantic, "SV_OutputControlPointID")))
		return MakeCached("gl_InvocationID");

	if (m_target == Target_DomainShader && (String_Equal(semantic, "SV_DomainLocation")))
		return MakeCached("gl_TessCoord");
	
	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_IsFrontFace")))
		return MakeCached("gl_FrontFacing");

    if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out)
    {
        int index = -1;

        if (strncmp(FetchCstr(m_stringLibrary, semantic), "COLOR", 5) == 0)
            index = atoi(FetchCstr(m_stringLibrary, semantic) + 5);
        else if (strncmp(FetchCstr(m_stringLibrary, semantic), "SV_Target", 9) == 0 || strncmp(FetchCstr(m_stringLibrary, semantic), "SV_TARGET", 9) == 0)
            index = atoi(FetchCstr(m_stringLibrary, semantic) + 9);

        if (index >= 0)
        {
			if (m_outputTypes.size() <= index)
			{
				m_outputTypes.resize(index + 1);
			}

            if (outputIndex)
                *outputIndex = index;

            return m_versionLegacy ? MakeCached("gl_FragData") : MakeCached("rast_FragData");
        }
    }

	return CachedString();
}

CachedString GLSLGenerator::GetBuiltInSemantic(const CachedString & semantic, AttributeModifier modifier, const HLSLType& type, int* outputIndex)
{
	if (outputIndex)
		*outputIndex = -1;

	if ((m_target == Target_VertexShader || m_target == Target_DomainShader) && modifier == AttributeModifier_Out && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return MakeCached("gl_Position");

	//if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "POSITION") && !m_outputPosition)
	//	return "gl_Position";

	if (m_target == Target_GeometryShader && (modifier == AttributeModifier_In || modifier == AttributeModifier_Out) && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return MakeCached("gl_Position");


	if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "PSIZE"))
		return MakeCached("gl_PointSize");

	if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_InstanceID"))
		return MakeCached("gl_InstanceIndex");

	if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_VertexID"))
		return MakeCached("gl_VertexIndex");

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out && String_Equal(semantic, "SV_Depth"))
		return MakeCached("gl_FragDepth");

	//https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return MakeCached("vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w)");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_DispatchThreadID")))
		return MakeCached("gl_GlobalInvocationID");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return MakeCached("gl_WorkGroupID");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return MakeCached("gl_WorkGroupID");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupIndex")))
		return MakeCached("gl_LocalInvocationIndex");

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupThreadID")))
		return MakeCached("gl_LocalInvocationID");

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_SampleIndex")))
		return MakeCached("gl_SampleID");

	if ((m_target == Target_HullShader || m_target == Target_DomainShader) && (String_Equal(semantic, "SV_InsideTessFactor")))
		return MakeCached("gl_TessLevelInner");

	if ((m_target == Target_HullShader || m_target == Target_DomainShader) && (String_Equal(semantic, "SV_TessFactor")))
		return MakeCached("gl_TessLevelOuter");

	if (m_target == Target_HullShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_OutputControlPointID")))
		return MakeCached("gl_InvocationID");

	if (m_target == Target_DomainShader && (String_Equal(semantic, "SV_DomainLocation")))
		return MakeCached("gl_TessCoord");

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_IsFrontFace")))
		return MakeCached("gl_FrontFacing");

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out)
	{
		int index = -1;

		if (strncmp(FetchCstr(m_stringLibrary, semantic), "COLOR", 5) == 0)
			index = atoi(FetchCstr(m_stringLibrary, semantic) + 5);
		else if (strncmp(FetchCstr(m_stringLibrary, semantic), "SV_Target", 9) == 0 || strncmp(FetchCstr(m_stringLibrary, semantic), "SV_TARGET", 9) == 0)
			index = atoi(FetchCstr(m_stringLibrary, semantic) + 9);

		if (index >= 0)
		{
			if (m_outputTypes.size() <= index)
			{
				m_outputTypes.push_back(type.baseType);
			}

			if (outputIndex)
				*outputIndex = index;

			return m_versionLegacy ? MakeCached("gl_FragData") : MakeCached("rast_FragData");
		}
	}



	return CachedString();
}

CachedString GLSLGenerator::MakeCached(const char * str)
{
	CachedString ret = m_tree->AddStringCached(str);
	return ret;
}

void GLSLGenerator::OutputArrayExpression(int arrayDimension, HLSLExpression* (&arrayDimExpression)[MAX_DIM])
{
	for (int i = 0; i < arrayDimension; i++)
	{
		if (arrayDimExpression[i])
		{
			m_writer.Write("[");
			OutputExpression(arrayDimExpression[i]);
			m_writer.Write("]");
		}
		else
		{
			m_writer.Write("[]");
		}
	}
}
