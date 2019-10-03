#include "Engine.h"

#include "HLSLTree.h"
#include "StringLibrary.h"

const BaseTypeDescription BASE_TYPE_DESC[HLSLBaseType_Count] = 
	{
		{ "unknown type",       NumericType_NaN,       0, 0, -1 },      // HLSLBaseType_Unknown
		{ "void",               NumericType_NaN,       0, 0, -1 },      // HLSLBaseType_Void

		{ "float",              NumericType_Float,     1, 1,  0 },      // HLSLBaseType_Float
		{ "float1x2",           NumericType_Float,     1, 2,  0 },      // HLSLBaseType_Float1x2
		{ "float1x3",           NumericType_Float,     1, 3,  0 },      // HLSLBaseType_Float1x3
		{ "float1x4",           NumericType_Float,     1, 4,  0 },      // HLSLBaseType_Float1x4
		{ "float2",             NumericType_Float,     2, 1,  0 },      // HLSLBaseType_Float2
		{ "float2x2",           NumericType_Float,     2, 2,  0 },      // HLSLBaseType_Float2x2
		{ "float2x3",           NumericType_Float,     2, 3,  0 },      // HLSLBaseType_Float2x3
		{ "float2x4",           NumericType_Float,     2, 4,  0 },      // HLSLBaseType_Float2x4
		{ "float3",             NumericType_Float,     3, 1,  0 },      // HLSLBaseType_Float3
		{ "float3x2",           NumericType_Float,     3, 2,  0 },      // HLSLBaseType_Float3x2
		{ "float3x3",           NumericType_Float,     3, 3,  0 },      // HLSLBaseType_Float3x3
		{ "float3x4",           NumericType_Float,     3, 4,  0 },      // HLSLBaseType_Float3x4
		{ "float4",             NumericType_Float,     4, 1,  0 },      // HLSLBaseType_Float4
		{ "float4x2",           NumericType_Float,     4, 2,  0 },      // HLSLBaseType_Float4x2
		{ "float4x3",           NumericType_Float,     4, 3,  0 },      // HLSLBaseType_Float4x3
		{ "float4x4",           NumericType_Float,     4, 4,  0 },      // HLSLBaseType_Float4x4


		{ "half",               NumericType_Half,      1, 1,  1 },      // HLSLBaseType_Half
		{ "half1x2",            NumericType_Half,      1, 2,  1 },      // HLSLBaseType_Half1x2
		{ "half1x3",            NumericType_Half,      1, 3,  1 },      // HLSLBaseType_Half1x3
		{ "half1x4",            NumericType_Half,      1, 4,  1 },      // HLSLBaseType_Half1x4
		{ "half2",              NumericType_Half,      2, 1,  1 },      // HLSLBaseType_Half2
		{ "half2x2",            NumericType_Half,      2, 2,  1 },      // HLSLBaseType_Half2x2
		{ "half2x3",            NumericType_Half,      2, 3,  1 },      // HLSLBaseType_Half2x3
		{ "half2x4",            NumericType_Half,      2, 4,  1 },      // HLSLBaseType_Half2x4
		{ "half3",              NumericType_Half,      3, 1,  1 },      // HLSLBaseType_Half3
		{ "half3x2",            NumericType_Half,      3, 2,  1 },      // HLSLBaseType_Half3x2
		{ "half3x3",            NumericType_Half,      3, 3,  1 },      // HLSLBaseType_Half3x3
		{ "half3x4",            NumericType_Half,      3, 4,  1 },      // HLSLBaseType_Half3x4
		{ "half4",              NumericType_Half,      4, 1,  1 },      // HLSLBaseType_Half4
		{ "half4x2",            NumericType_Half,      4, 2,  1 },      // HLSLBaseType_Half4x2
		{ "half4x3",            NumericType_Half,      4, 3,  1 },      // HLSLBaseType_Half4x3
		{ "half4x4",            NumericType_Half,      4, 4,  1 },      // HLSLBaseType_Half4x4


		{ "min16float",              NumericType_Min16Float,      1, 1,  1 },      // HLSLBaseType_Min16Float
		{ "min16float1x2",           NumericType_Min16Float,      1, 2,  1 },      // HLSLBaseType_Min16Float1x2
		{ "min16float1x3",           NumericType_Min16Float,      1, 3,  1 },      // HLSLBaseType_Min16Float1x3
		{ "min16float1x4",           NumericType_Min16Float,      1, 4,  1 },      // HLSLBaseType_Min16Float1x4
		{ "min16float2",             NumericType_Min16Float,      2, 1,  1 },      // HLSLBaseType_Min16Float2
		{ "min16float2x2",           NumericType_Min16Float,      2, 2,  1 },      // HLSLBaseType_Min16Float2x2
		{ "min16float2x3",           NumericType_Min16Float,      2, 3,  1 },      // HLSLBaseType_Min16Float2x3
		{ "min16float2x4",           NumericType_Min16Float,      2, 4,  1 },      // HLSLBaseType_Min16Float2x4
		{ "min16float3",             NumericType_Min16Float,      3, 1,  1 },      // HLSLBaseType_Min16Float3
		{ "min16float3x2",           NumericType_Min16Float,      3, 2,  1 },      // HLSLBaseType_Min16Float3x2
		{ "min16float3x3",           NumericType_Min16Float,      3, 3,  1 },      // HLSLBaseType_Min16Float3x3
		{ "min16float3x4",           NumericType_Min16Float,      3, 4,  1 },      // HLSLBaseType_Min16Float3x4
		{ "min16float4",             NumericType_Min16Float,      4, 1,  1 },      // HLSLBaseType_Min16Float4
		{ "min16float4x2",           NumericType_Min16Float,      4, 2,  1 },      // HLSLBaseType_Min16Float4x2
		{ "min16float4x3",           NumericType_Min16Float,      4, 3,  1 },      // HLSLBaseType_Min16Float4x3
		{ "min16float4x4",           NumericType_Min16Float,      4, 4,  1 },      // HLSLBaseType_Min16Float4x4


		{ "min10float",              NumericType_Min10Float,      1, 1,  1 },      // HLSLBaseType_Min10Float
		{ "min10float1x2",           NumericType_Min10Float,      1, 2,  1 },      // HLSLBaseType_Min10Float1x2
		{ "min10float1x3",           NumericType_Min10Float,      1, 3,  1 },      // HLSLBaseType_Min10Float1x3
		{ "min10float1x4",           NumericType_Min10Float,      1, 4,  1 },      // HLSLBaseType_Min10Float1x4
		{ "min10float2",             NumericType_Min10Float,      2, 1,  1 },      // HLSLBaseType_Min10Float2
		{ "min10float2x2",           NumericType_Min10Float,      2, 2,  1 },      // HLSLBaseType_Min10Float2x2
		{ "min10float2x3",           NumericType_Min10Float,      2, 3,  1 },      // HLSLBaseType_Min10Float2x3
		{ "min10float2x4",           NumericType_Min10Float,      2, 4,  1 },      // HLSLBaseType_Min10Float2x4
		{ "min10float3",             NumericType_Min10Float,      3, 1,  1 },      // HLSLBaseType_Min10Float3
		{ "min10float3x2",           NumericType_Min10Float,      3, 2,  1 },      // HLSLBaseType_Min10Float3x2
		{ "min10float3x3",           NumericType_Min10Float,      3, 3,  1 },      // HLSLBaseType_Min10Float3x3
		{ "min10float3x4",           NumericType_Min10Float,      3, 4,  1 },      // HLSLBaseType_Min10Float3x4
		{ "min10float4",             NumericType_Min10Float,      4, 1,  1 },      // HLSLBaseType_Min10Float4
		{ "min10float4x2",           NumericType_Min10Float,      4, 2,  1 },      // HLSLBaseType_Min10Float4x2
		{ "min10float4x3",           NumericType_Min10Float,      4, 3,  1 },      // HLSLBaseType_Min10Float4x3
		{ "min10float4x4",           NumericType_Min10Float,      4, 4,  1 },      // HLSLBaseType_Min10Float4x4

		{ "bool",               NumericType_Bool,      1, 1,  4 },      // HLSLBaseType_Bool
		{ "bool1x2",            NumericType_Bool,      1, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool1x3",            NumericType_Bool,      1, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool1x4",            NumericType_Bool,      1, 4,  4 },      // HLSLBaseType_Bool1x4
		{ "bool2",				NumericType_Bool,	   2, 1,  4 },      // HLSLBaseType_Bool2
		{ "bool2x2",            NumericType_Bool,      2, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool2x3",            NumericType_Bool,      2, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool2x4",            NumericType_Bool,      2, 4,  4 },      // HLSLBaseType_Bool1x4
		{ "bool3",				NumericType_Bool,	   3, 1,  4 },      // HLSLBaseType_Bool3
		{ "bool3x2",            NumericType_Bool,      3, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool3x3",            NumericType_Bool,      3, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool3x4",            NumericType_Bool,      3, 4,  4 },      // HLSLBaseType_Bool1x4
		{ "bool4",				NumericType_Bool,	   4, 1,  4 },      // HLSLBaseType_Bool4
		{ "bool4x2",            NumericType_Bool,      4, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool4x3",            NumericType_Bool,      4, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool4x4",            NumericType_Bool,      4, 4,  4 },      // HLSLBaseType_Bool1x4

		{ "int",                NumericType_Int,       1, 1,  3 },      // HLSLBaseType_Int
		{ "int1x2",             NumericType_Int,	   1, 2,  3 },      // HLSLBaseType_Int1x2
		{ "int1x3",             NumericType_Int,	   1, 3,  3 },      // HLSLBaseType_Int1x3
		{ "int1x4",             NumericType_Int,	   1, 4,  3 },      // HLSLBaseType_Int1x4
		{ "int2",               NumericType_Int,       2, 1,  3 },      // HLSLBaseType_Int2
		{ "int2x2",             NumericType_Int,	   2, 2,  3 },      // HLSLBaseType_Int2x2
		{ "int2x3",             NumericType_Int,	   2, 3,  3 },      // HLSLBaseType_Int2x3
		{ "int2x4",             NumericType_Int,	   2, 4,  3 },      // HLSLBaseType_Int2x4
		{ "int3",               NumericType_Int,       3, 1,  3 },      // HLSLBaseType_Int3
		{ "int3x2",             NumericType_Int,	   3, 2,  3 },      // HLSLBaseType_Int3x2
		{ "int3x3",             NumericType_Int,	   3, 3,  3 },      // HLSLBaseType_Int3x3
		{ "int3x4",             NumericType_Int,	   3, 4,  3 },      // HLSLBaseType_Int3x4
		{ "int4",               NumericType_Int,       4, 1,  3 },      // HLSLBaseType_Int4
		{ "int4x2",             NumericType_Int,	   4, 2,  3 },      // HLSLBaseType_Int4x2
		{ "int4x3",             NumericType_Int,	   4, 3,  3 },      // HLSLBaseType_Int4x3
		{ "int4x4",             NumericType_Int,	   4, 4,  3 },      // HLSLBaseType_Int4x4
		
		{ "uint",               NumericType_Uint,      1, 1,  2 },      // HLSLBaseType_Uint
		{ "uint1x2",            NumericType_Uint,	   1, 2,  2 },      // HLSLBaseType_Int1x2
		{ "uint1x3",            NumericType_Uint,	   1, 3,  2 },      // HLSLBaseType_Int1x3
		{ "uint1x4",            NumericType_Uint,	   1, 4,  2 },      // HLSLBaseType_Int1x4
		{ "uint2",              NumericType_Uint,      2, 1,  2 },      // HLSLBaseType_Uint2
		{ "uint2x2",            NumericType_Uint,	   1, 2,  2 },      // HLSLBaseType_Uint1x2
		{ "uint2x3",            NumericType_Uint,	   1, 3,  2 },      // HLSLBaseType_Uint1x3
		{ "uint2x4",            NumericType_Uint,	   1, 4,  2 },      // HLSLBaseType_Uint1x4
		{ "uint3",              NumericType_Uint,      3, 1,  2 },      // HLSLBaseType_Uint3
		{ "uint3x2",            NumericType_Uint,	   1, 2,  2 },      // HLSLBaseType_Uint1x2
		{ "uint3x3",            NumericType_Uint,	   1, 3,  2 },      // HLSLBaseType_Uint1x3
		{ "uint3x4",            NumericType_Uint,	   1, 4,  2 },      // HLSLBaseType_Uint1x4
		{ "uint4",              NumericType_Uint,      4, 1,  2 },      // HLSLBaseType_Uint4
		{ "uint4x2",            NumericType_Uint,	   1, 2,  2 },      // HLSLBaseType_Uint1x2
		{ "uint4x3",            NumericType_Uint,	   1, 3,  2 },      // HLSLBaseType_Uint1x3
		{ "uint4x4",            NumericType_Uint,	   1, 4,  2 },      // HLSLBaseType_Uint1x4


		{ "inputPatch",         NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_InputPatch
		{ "outputPatch",        NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_OutputPatch

		{ "pointStream",		NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_PointStream
		{ "lineStream",         NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_LineStream
		{ "triangleStream",     NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_TriangleStream

		{ "point",				NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Point
		{ "line",				NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Line
		{ "triangle",			NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Triangle
		{ "lineadj",			NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Lineadj
		{ "triangleadj",		NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Triangleadj		

		{ "texture",            NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture
		{ "Texture1D",          NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture1D
		{ "Texture1DArray",     NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture1DArray
		{ "Texture2D",          NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture2D
		{ "Texture2DArray",     NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture2DArray
		{ "Texture3D",          NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture3D
		{ "Texture2DMS",        NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture2DMS
		{ "Texture2DMSArray",   NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_Texture2DMSArray
		{ "TextureCube",        NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_TextureCube
		{ "TextureCubeArray",   NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_TextureCubeArray

		{ "RWTexture1D",          NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_RWTexture1D
		{ "RWTexture1DArray",     NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_RWTexture1DArray
		{ "RWTexture2D",          NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_RWTexture2D
		{ "RWTexture2DArray",     NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_RWTexture2DArray
		{ "RWTexture3D",          NumericType_NaN,        1, 0, -1 },     // HLSLBaseType_RWTexture3D


		{ "sampler",            NumericType_NaN,        1, 0, -1 },      // HLSLBaseType_Sampler
		{ "sampler2D",          NumericType_NaN,        1, 0, -1 },      // HLSLBaseType_Sampler2D
		{ "sampler3D",          NumericType_NaN,        1, 0, -1 },      // HLSLBaseType_Sampler3D
		{ "samplerCUBE",        NumericType_NaN,        1, 0, -1 },      // HLSLBaseType_SamplerCube
		{ "sampler2DShadow",    NumericType_NaN,        1, 0, -1 },      // HLSLBaseType_Sampler2DShadow
		{ "sampler2DMS",        NumericType_NaN,        1, 0, -1 },      // HLSLBaseType_Sampler2DMS
		{ "sampler2DArray",     NumericType_NaN,        1, 0, -1 },      // HLSLBaseType_Sampler2DArray
		{ "user defined",       NumericType_NaN,        1, 0, -1 }       // HLSLBaseType_UserDefined
	};


const char * FetchCstr(const StringLibrary * stringLibrary, const CachedString & cstr)
{
	ASSERT_PARSER(stringLibrary != NULL);
	return RawStr(cstr);
}

const char* getElementTypeAsStr(StringLibrary * stringLibrary, HLSLType type)
{
	HLSLBaseType compareType;

	if (type.elementType != HLSLBaseType_Unknown)
		compareType = type.elementType;
	else
		compareType = type.baseType;

	switch (compareType)
	{
	case HLSLBaseType_Bool:
		return "bool";
	case HLSLBaseType_Bool1x2:
		return "bool1x2";
	case HLSLBaseType_Bool1x3:
		return "bool1x3";
	case HLSLBaseType_Bool1x4:
		return "bool1x4";

	case HLSLBaseType_Bool2:
		return "bool2";
	case HLSLBaseType_Bool2x2:
		return "bool2x2";
	case HLSLBaseType_Bool2x3:
		return "bool2x3";
	case HLSLBaseType_Bool2x4:
		return "bool2x4";

	case HLSLBaseType_Bool3:
		return "bool3";
	case HLSLBaseType_Bool3x2:
		return "bool3x2";
	case HLSLBaseType_Bool3x3:
		return "bool3x3";
	case HLSLBaseType_Bool3x4:
		return "bool3x4";

	case HLSLBaseType_Bool4:
		return "bool4";
	case HLSLBaseType_Bool4x2:
		return "bool4x2";
	case HLSLBaseType_Bool4x3:
		return "bool4x3";
	case HLSLBaseType_Bool4x4:
		return "bool4x4";

	case HLSLBaseType_Float:
		return "float";
	case HLSLBaseType_Float1x2:
		return "float1x2";
	case HLSLBaseType_Float1x3:
		return "float1x3";
	case HLSLBaseType_Float1x4:
		return "float1x4";

	case HLSLBaseType_Float2:
		return "float2";
	case HLSLBaseType_Float2x2:
		return "float2x2";
	case HLSLBaseType_Float2x3:
		return "float2x3";
	case HLSLBaseType_Float2x4:
		return "float2x4";

	case HLSLBaseType_Float3:
		return "float3";
	case HLSLBaseType_Float3x2:
		return "float3x2";
	case HLSLBaseType_Float3x3:
		return "float3x3";
	case HLSLBaseType_Float3x4:
		return "float3x4";

	case HLSLBaseType_Float4:
		return "float4";
	case HLSLBaseType_Float4x2:
		return "float4x2";
	case HLSLBaseType_Float4x3:
		return "float4x3";
	case HLSLBaseType_Float4x4:
		return "float4x4";

	case HLSLBaseType_Half:
		return "half";
	case HLSLBaseType_Half1x2:
		return "half1x2";
	case HLSLBaseType_Half1x3:
		return "half1x3";
	case HLSLBaseType_Half1x4:
		return "half1x4";

	case HLSLBaseType_Half2:
		return "half2";
	case HLSLBaseType_Half2x2:
		return "half2x2";
	case HLSLBaseType_Half2x3:
		return "half2x3";
	case HLSLBaseType_Half2x4:
		return "half2x4";

	case HLSLBaseType_Half3:
		return "half3";
	case HLSLBaseType_Half3x2:
		return "half3x2";
	case HLSLBaseType_Half3x3:
		return "half3x3";
	case HLSLBaseType_Half3x4:
		return "half3x4";

	case HLSLBaseType_Half4:
		return "half4";
	case HLSLBaseType_Half4x2:
		return "half4x2";
	case HLSLBaseType_Half4x3:
		return "half4x3";
	case HLSLBaseType_Half4x4:
		return "half4x4";



	case HLSLBaseType_Min16Float:
		return "min16float";
	case HLSLBaseType_Min16Float1x2:
		return "min16float1x2";
	case HLSLBaseType_Min16Float1x3:
		return "min16float1x3";
	case HLSLBaseType_Min16Float1x4:
		return "min16float1x4";

	case HLSLBaseType_Min16Float2:
		return "min16float2";
	case HLSLBaseType_Min16Float2x2:
		return "min16float2x2";
	case HLSLBaseType_Min16Float2x3:
		return "min16float2x3";
	case HLSLBaseType_Min16Float2x4:
		return "min16float2x4";

	case HLSLBaseType_Min16Float3:
		return "min16float3";
	case HLSLBaseType_Min16Float3x2:
		return "min16float3x2";
	case HLSLBaseType_Min16Float3x3:
		return "min16float3x3";
	case HLSLBaseType_Min16Float3x4:
		return "min16float3x4";

	case HLSLBaseType_Min16Float4:
		return "min16float4";
	case HLSLBaseType_Min16Float4x2:
		return "min16float4x2";
	case HLSLBaseType_Min16Float4x3:
		return "min16float4x3";
	case HLSLBaseType_Min16Float4x4:
		return "min16float4x4";


	case HLSLBaseType_Min10Float:
		return "min10float";
	case HLSLBaseType_Min10Float1x2:
		return "min10float1x2";
	case HLSLBaseType_Min10Float1x3:
		return "min10float1x3";
	case HLSLBaseType_Min10Float1x4:
		return "min10float1x4";

	case HLSLBaseType_Min10Float2:
		return "min10float2";
	case HLSLBaseType_Min10Float2x2:
		return "min10float2x2";
	case HLSLBaseType_Min10Float2x3:
		return "min10float2x3";
	case HLSLBaseType_Min10Float2x4:
		return "min10float2x4";

	case HLSLBaseType_Min10Float3:
		return "min10float3";
	case HLSLBaseType_Min10Float3x2:
		return "min10float3x2";
	case HLSLBaseType_Min10Float3x3:
		return "min10float3x3";
	case HLSLBaseType_Min10Float3x4:
		return "min10float3x4";

	case HLSLBaseType_Min10Float4:
		return "min10float4";
	case HLSLBaseType_Min10Float4x2:
		return "min10float4x2";
	case HLSLBaseType_Min10Float4x3:
		return "min10float4x3";
	case HLSLBaseType_Min10Float4x4:
		return "min10float4x4";


	case HLSLBaseType_Int:
		return "int";
	case HLSLBaseType_Int1x2:
		return "int1x2";
	case HLSLBaseType_Int1x3:
		return "int1x3";
	case HLSLBaseType_Int1x4:
		return "int1x4";

	case HLSLBaseType_Int2:
		return "int2";
	case HLSLBaseType_Int2x2:
		return "int2x2";
	case HLSLBaseType_Int2x3:
		return "int2x3";
	case HLSLBaseType_Int2x4:
		return "int2x4";

	case HLSLBaseType_Int3:
		return "int3";
	case HLSLBaseType_Int3x2:
		return "int3x2";
	case HLSLBaseType_Int3x3:
		return "int3x3";
	case HLSLBaseType_Int3x4:
		return "int3x4";

	case HLSLBaseType_Int4:
		return "int4";
	case HLSLBaseType_Int4x2:
		return "int4x2";
	case HLSLBaseType_Int4x3:
		return "int4x3";
	case HLSLBaseType_Int4x4:
		return "int4x4";

	case HLSLBaseType_Uint:
		return "uint";
	case HLSLBaseType_Uint1x2:
		return "uint1x2";
	case HLSLBaseType_Uint1x3:
		return "uint1x3";
	case HLSLBaseType_Uint1x4:
		return "uint1x4";

	case HLSLBaseType_Uint2:
		return "uint2";
	case HLSLBaseType_Uint2x2:
		return "uint2x2";
	case HLSLBaseType_Uint2x3:
		return "uint2x3";
	case HLSLBaseType_Uint2x4:
		return "uint2x4";

	case HLSLBaseType_Uint3:
		return "uint3";
	case HLSLBaseType_Uint3x2:
		return "uint3x2";
	case HLSLBaseType_Uint3x3:
		return "uint3x3";
	case HLSLBaseType_Uint3x4:
		return "uint3x4";

	case HLSLBaseType_Uint4:
		return "uint4";
	case HLSLBaseType_Uint4x2:
		return "uint4x2";
	case HLSLBaseType_Uint4x3:
		return "uint4x3";
	case HLSLBaseType_Uint4x4:
		return "uint4x4";
	case HLSLBaseType_UserDefined:
		return FetchCstr(stringLibrary,type.typeName);
	default:
		return "UnknownElementType";
	}
}


eastl::string getElementTypeAsStrGLSL(StringLibrary * stringLibrary, HLSLType type)
{
	HLSLBaseType compareType;

	if (type.elementType != HLSLBaseType_Unknown)
		compareType = type.elementType;
	else
		compareType = type.baseType;

	switch (compareType)
	{
	case HLSLBaseType_Bool:
		return "bool";
	case HLSLBaseType_Bool1x2:
		return "bool1x2";
	case HLSLBaseType_Bool1x3:
		return "bool1x3";
	case HLSLBaseType_Bool1x4:
		return "bool1x4";

	case HLSLBaseType_Bool2:
		return "bvec2";
	case HLSLBaseType_Bool2x2:
		return "bool2x2";
	case HLSLBaseType_Bool2x3:
		return "bool2x3";
	case HLSLBaseType_Bool2x4:
		return "bool2x4";

	case HLSLBaseType_Bool3:
		return "bvec3";
	case HLSLBaseType_Bool3x2:
		return "bool3x2";
	case HLSLBaseType_Bool3x3:
		return "bool3x3";
	case HLSLBaseType_Bool3x4:
		return "bool3x4";

	case HLSLBaseType_Bool4:
		return "bvec4";
	case HLSLBaseType_Bool4x2:
		return "bool4x2";
	case HLSLBaseType_Bool4x3:
		return "bool4x3";
	case HLSLBaseType_Bool4x4:
		return "bool4x4";

	case HLSLBaseType_Float:
		return "float";
	case HLSLBaseType_Float1x2:
		return "mat1x2";
	case HLSLBaseType_Float1x3:
		return "mat1x3";
	case HLSLBaseType_Float1x4:
		return "mat1x4";

	case HLSLBaseType_Float2:
		return "vec2";
	case HLSLBaseType_Float2x2:
		return "mat2";
	case HLSLBaseType_Float2x3:
		return "mat2x3";
	case HLSLBaseType_Float2x4:
		return "mat2x4";

	case HLSLBaseType_Float3:
		return "vec3";
	case HLSLBaseType_Float3x2:
		return "mat3x2";
	case HLSLBaseType_Float3x3:
		return "mat3";
	case HLSLBaseType_Float3x4:
		return "mat3x4";

	case HLSLBaseType_Float4:
		return "vec4";
	case HLSLBaseType_Float4x2:
		return "mat4x2";
	case HLSLBaseType_Float4x3:
		return "mat4x3";
	case HLSLBaseType_Float4x4:
		return "mat4";

	case HLSLBaseType_Half:
		return "float";
	case HLSLBaseType_Half1x2:
		return "mat1x2";
	case HLSLBaseType_Half1x3:
		return "mat1x3";
	case HLSLBaseType_Half1x4:
		return "mat1x4";

	case HLSLBaseType_Half2:
		return "vec2";
	case HLSLBaseType_Half2x2:
		return "mat2x2";
	case HLSLBaseType_Half2x3:
		return "mat2x3";
	case HLSLBaseType_Half2x4:
		return "mat2x4";

	case HLSLBaseType_Half3:
		return "vec3";
	case HLSLBaseType_Half3x2:
		return "mat3x2";
	case HLSLBaseType_Half3x3:
		return "mat3x3";
	case HLSLBaseType_Half3x4:
		return "mat3x4";

	case HLSLBaseType_Half4:
		return "vec4";
	case HLSLBaseType_Half4x2:
		return "mat4x2";
	case HLSLBaseType_Half4x3:
		return "mat4x3";
	case HLSLBaseType_Half4x4:
		return "mat4x4";



	case HLSLBaseType_Min16Float:
		return "float";
	case HLSLBaseType_Min16Float1x2:
		return "mat1x2";
	case HLSLBaseType_Min16Float1x3:
		return "mat1x3";
	case HLSLBaseType_Min16Float1x4:
		return "mat1x4";

	case HLSLBaseType_Min16Float2:
		return "vec2";
	case HLSLBaseType_Min16Float2x2:
		return "mat2";
	case HLSLBaseType_Min16Float2x3:
		return "mat2x3";
	case HLSLBaseType_Min16Float2x4:
		return "mat2x4";

	case HLSLBaseType_Min16Float3:
		return "vec3";
	case HLSLBaseType_Min16Float3x2:
		return "mat3x2";
	case HLSLBaseType_Min16Float3x3:
		return "mat3";
	case HLSLBaseType_Min16Float3x4:
		return "mat3x4";

	case HLSLBaseType_Min16Float4:
		return "vec4";
	case HLSLBaseType_Min16Float4x2:
		return "mat4x2";
	case HLSLBaseType_Min16Float4x3:
		return "mat4x3";
	case HLSLBaseType_Min16Float4x4:
		return "mat4";


	case HLSLBaseType_Min10Float:
		return "float";
	case HLSLBaseType_Min10Float1x2:
		return "mat1x2";
	case HLSLBaseType_Min10Float1x3:
		return "mat1x3";
	case HLSLBaseType_Min10Float1x4:
		return "mat1x4";

	case HLSLBaseType_Min10Float2:
		return "vec2";
	case HLSLBaseType_Min10Float2x2:
		return "mat2";
	case HLSLBaseType_Min10Float2x3:
		return "mat2x3";
	case HLSLBaseType_Min10Float2x4:
		return "mat2x4";

	case HLSLBaseType_Min10Float3:
		return "vec3";
	case HLSLBaseType_Min10Float3x2:
		return "mat3x2";
	case HLSLBaseType_Min10Float3x3:
		return "mat3";
	case HLSLBaseType_Min10Float3x4:
		return "mat3x4";

	case HLSLBaseType_Min10Float4:
		return "vec4";
	case HLSLBaseType_Min10Float4x2:
		return "mat4x2";
	case HLSLBaseType_Min10Float4x3:
		return "mat4x3";
	case HLSLBaseType_Min10Float4x4:
		return "mat4";


	case HLSLBaseType_Int:
		return "int";
	case HLSLBaseType_Int1x2:
		return "int1x2";
	case HLSLBaseType_Int1x3:
		return "int1x3";
	case HLSLBaseType_Int1x4:
		return "int1x4";

	case HLSLBaseType_Int2:
		return "ivec2";
	case HLSLBaseType_Int2x2:
		return "int2x2";
	case HLSLBaseType_Int2x3:
		return "int2x3";
	case HLSLBaseType_Int2x4:
		return "int2x4";

	case HLSLBaseType_Int3:
		return "ivec3";
	case HLSLBaseType_Int3x2:
		return "int3x2";
	case HLSLBaseType_Int3x3:
		return "int3x3";
	case HLSLBaseType_Int3x4:
		return "int3x4";

	case HLSLBaseType_Int4:
		return "ivec4";
	case HLSLBaseType_Int4x2:
		return "int4x2";
	case HLSLBaseType_Int4x3:
		return "int4x3";
	case HLSLBaseType_Int4x4:
		return "int4x4";

	case HLSLBaseType_Uint:
		return "uint";
	case HLSLBaseType_Uint1x2:
		return "uint1x2";
	case HLSLBaseType_Uint1x3:
		return "uint1x3";
	case HLSLBaseType_Uint1x4:
		return "uint1x4";

	case HLSLBaseType_Uint2:
		return "uvec2";
	case HLSLBaseType_Uint2x2:
		return "uint2x2";
	case HLSLBaseType_Uint2x3:
		return "uint2x3";
	case HLSLBaseType_Uint2x4:
		return "uint2x4";

	case HLSLBaseType_Uint3:
		return "uvec3";
	case HLSLBaseType_Uint3x2:
		return "uint3x2";
	case HLSLBaseType_Uint3x3:
		return "uint3x3";
	case HLSLBaseType_Uint3x4:
		return "uint3x4";

	case HLSLBaseType_Uint4:
		return "uvec4";
	case HLSLBaseType_Uint4x2:
		return "uint4x2";
	case HLSLBaseType_Uint4x3:
		return "uint4x3";
	case HLSLBaseType_Uint4x4:
		return "uint4x4";
	case HLSLBaseType_UserDefined:
		return RawStr(type.typeName);
	default:
		return "UnknownElementType";
	}
}


HLSLTree::HLSLTree(StringLibrary * stringLibrary)
{
	m_stringLibrary = stringLibrary;
    m_root              = AddNode<HLSLRoot>(NULL, 1);

	for (int i = 0; i < MAX_GLOBAL_EXTENSION; i++)
	{
		gExtension[i] = false;
	}
}

HLSLTree::~HLSLTree()
{
	// removing all the existing node page tables

	for (int i = 0; i < m_allNodes.size(); i++)
	{
		HLSLNode * pNode = m_allNodes[i];
		if (pNode != nullptr)
		{
			delete pNode;
			pNode = nullptr;
			m_allNodes[i] = nullptr;
		}
	}

	m_allNodes.clear();
}

// @@ From mprintf.cpp
static char *mprintf_valist(int size, const char *fmt, va_list args) {
	ASSERT_PARSER(size > 0);
	char *res = NULL;
	va_list tmp;

	while (1) {
		res = new char[size];
		if (!res) return NULL;

		va_copy(tmp, args);
		int len = vsnprintf(res, size, fmt, tmp);
		va_end(tmp);

		if ((len >= 0) && (size >= len + 1)) {
			break;
		}

		delete[] res;

		if (len > -1) {
			size = len + 1;
		}
		else {
			size *= 2;
		}
	}

	return res;
}

CachedString HLSLTree::AddStringCached(const char* string)
{
	CachedString ret;
	// also add it to the string library
	ret.m_string = m_stringLibrary->InsertDirect(eastl::string(string));
	return ret;
}

CachedString HLSLTree::AddStringFormatCached(const char* format, ...)
{

	eastl::string finalStr;
	{
		va_list args;
		va_start(args, format);
		const char * string = mprintf_valist(256, format, args);
		va_end(args);

		finalStr = string;
		delete[] string;

	}

	CachedString ret;
	ret.m_string = m_stringLibrary->InsertDirect(finalStr);
	return ret;
}


bool HLSLTree::GetContainsString(const char* string) const
{
	bool isFound = m_stringLibrary->HasString(string);
	return isFound;
}

HLSLRoot* HLSLTree::GetRoot() const
{
    return m_root;
}

// @@ This doesn't do any parameter matching. Simply returns the first function with that name.
HLSLFunction * HLSLTree::FindFunction(const CachedString & name, int index)
{
    HLSLStatement * statement = m_root->statement;

	int count = 0;

    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction * function = (HLSLFunction *)statement;
            if (String_Equal(name, function->name))
            {
				if(index == count)
					return function;

				count++;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLDeclaration * HLSLTree::FindGlobalDeclaration(const CachedString & name, HLSLBuffer ** buffer_out/*=NULL*/)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration * declaration = (HLSLDeclaration *)statement;
            if (String_Equal(name, declaration->name))
            {
                if (buffer_out) *buffer_out = NULL;
                return declaration;
            }
        }
        else if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer* buffer = (HLSLBuffer*)statement;

            HLSLDeclaration* field = buffer->field;
            while (field != NULL)
            {
				ASSERT_PARSER(field->nodeType == HLSLNodeType_Declaration);
                if (String_Equal(name, field->name))
                {
                    if (buffer_out) *buffer_out = buffer;
                    return field;
                }
                field = (HLSLDeclaration*)field->nextStatement;
            }
        }

        statement = statement->nextStatement;
    }

    if (buffer_out) *buffer_out = NULL;
    return NULL;
}

HLSLStruct * HLSLTree::FindGlobalStruct(const CachedString & name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct * declaration = (HLSLStruct *)statement;
            if (String_Equal(name, declaration->name))
            {
                return declaration;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

CachedString HLSLTree::FindGlobalStructMember(const CachedString & memberName)
{
	HLSLStatement * statement = m_root->statement;
	while (statement != NULL)
	{
		if (statement->nodeType == HLSLNodeType_Struct)
		{
			HLSLStruct * declaration = (HLSLStruct *)statement;

			HLSLStructField* field = declaration->field;

			while (field != NULL)
			{
				if (String_Equal(memberName, field->name))
				{
					return declaration->name;
				}

				field = field->nextField;
			}
		}

		statement = statement->nextStatement;
	}

	return CachedString();
}

HLSLTechnique * HLSLTree::FindTechnique(const CachedString & name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Technique)
        {
            HLSLTechnique * technique = (HLSLTechnique *)statement;
            if (String_Equal(name, technique->name))
            {
                return technique;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLPipeline * HLSLTree::FindFirstPipeline()
{
    return FindNextPipeline(NULL);
}

HLSLPipeline * HLSLTree::FindNextPipeline(HLSLPipeline * current)
{
    HLSLStatement * statement = current ? current : m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Pipeline)
        {
            return (HLSLPipeline *)statement;
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLPipeline * HLSLTree::FindPipeline(const CachedString & name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Pipeline)
        {
            HLSLPipeline * pipeline = (HLSLPipeline *)statement;
            if (String_Equal(name, pipeline->name))
            {
                return pipeline;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLBuffer * HLSLTree::FindBuffer(const CachedString & name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer * buffer = (HLSLBuffer *)statement;
            if (String_Equal(name, buffer->name))
            {
                return buffer;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

bool HLSLTree::GetExpressionValue(HLSLExpression * expression, int & value)
{
	ASSERT_PARSER(expression != NULL);

    // Expression must be constant.
    if ((expression->expressionType.flags & HLSLTypeFlag_Const) == 0) 
    {
        return false;
    }

    // We are expecting an integer scalar. @@ Add support for type conversion from other scalar types.
    if (expression->expressionType.baseType != HLSLBaseType_Int &&
        expression->expressionType.baseType != HLSLBaseType_Bool)
    {
        return false;
    }

    if (expression->expressionType.array) 
    {
        return false;
    }

    if (expression->nodeType == HLSLNodeType_BinaryExpression) 
    {
        HLSLBinaryExpression * binaryExpression = (HLSLBinaryExpression *)expression;

        int value1, value2;
        if (!GetExpressionValue(binaryExpression->expression1, value1) ||
            !GetExpressionValue(binaryExpression->expression2, value2))
        {
            return false;
        }

        switch(binaryExpression->binaryOp)
        {
            case HLSLBinaryOp_And:
                value = value1 && value2;
                return true;
            case HLSLBinaryOp_Or:
                value = value1 || value2;
                return true;
            case HLSLBinaryOp_Add:
                value = value1 + value2;
                return true;
            case HLSLBinaryOp_Sub:
                value = value1 - value2;
                return true;
            case HLSLBinaryOp_Mul:
                value = value1 * value2;
                return true;
            case HLSLBinaryOp_Div:
                value = value1 / value2;
                return true;
            case HLSLBinaryOp_Less:
                value = value1 < value2;
                return true;
            case HLSLBinaryOp_Greater:
                value = value1 > value2;
                return true;
            case HLSLBinaryOp_LessEqual:
                value = value1 <= value2;
                return true;
            case HLSLBinaryOp_GreaterEqual:
                value = value1 >= value2;
                return true;
            case HLSLBinaryOp_Equal:
                value = value1 == value2;
                return true;
            case HLSLBinaryOp_NotEqual:
                value = value1 != value2;
                return true;
            case HLSLBinaryOp_BitAnd:
                value = value1 & value2;
                return true;
            case HLSLBinaryOp_BitOr:
                value = value1 | value2;
                return true;
            case HLSLBinaryOp_BitXor:
                value = value1 ^ value2;
                return true;
            case HLSLBinaryOp_Assign:
            case HLSLBinaryOp_AddAssign:
            case HLSLBinaryOp_SubAssign:
            case HLSLBinaryOp_MulAssign:
            case HLSLBinaryOp_DivAssign:
                // IC: These are not valid on non-constant expressions and should fail earlier when querying expression value.
                return false;
        }
    }
    else if (expression->nodeType == HLSLNodeType_UnaryExpression) 
    {
        HLSLUnaryExpression * unaryExpression = (HLSLUnaryExpression *)expression;

        if (!GetExpressionValue(unaryExpression->expression, value))
        {
            return false;
        }

        switch(unaryExpression->unaryOp)
        {
            case HLSLUnaryOp_Negative:
                value = -value;
                return true;
            case HLSLUnaryOp_Positive:
                // nop.
                return true;
            case HLSLUnaryOp_Not:
                value = !value;
                return true;
            case HLSLUnaryOp_BitNot:
                value = ~value;
                return true;
            case HLSLUnaryOp_PostDecrement:
            case HLSLUnaryOp_PostIncrement:
            case HLSLUnaryOp_PreDecrement:
            case HLSLUnaryOp_PreIncrement:
                // IC: These are not valid on non-constant expressions and should fail earlier when querying expression value.
                return false;
        }
    }
	else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
	{
		HLSLIdentifierExpression * identifier = (HLSLIdentifierExpression *)expression;

		HLSLDeclaration * declaration = identifier->pDeclaration;
		ASSERT_PARSER(declaration) 

		if ((declaration->type.flags & HLSLTypeFlag_Const) == 0)
		{
			return false;
		}

		return GetExpressionValue(declaration->assignment, value);
	}
    else if (expression->nodeType == HLSLNodeType_LiteralExpression) 
    {
        HLSLLiteralExpression * literal = (HLSLLiteralExpression *)expression;
   
        if (literal->expressionType.baseType == HLSLBaseType_Int) value = literal->iValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Bool) value = (int)literal->bValue;
        else return false;
        
        return true;
    }

    return false;
}

bool HLSLTree::NeedsFunction(const CachedString & name)
{
    struct NeedsFunctionVisitor: HLSLTreeVisitor
    {
        CachedString name;
        bool result;

        virtual void VisitTopLevelStatement(HLSLStatement * node)
        {
            if (!node->hidden)
                HLSLTreeVisitor::VisitTopLevelStatement(node);
        }

        virtual void VisitFunctionCall(HLSLFunctionCall * node)
        {
            result = result || String_Equal(name, node->function->name);

            HLSLTreeVisitor::VisitFunctionCall(node);
        }
    };

    NeedsFunctionVisitor visitor;
    visitor.name = name;
    visitor.result = false;

    visitor.VisitRoot(m_root);

    return visitor.result;
}

int GetVectorDimension(HLSLType & type)
{
    if (type.baseType >= HLSLBaseType_FirstNumeric &&
        type.baseType <= HLSLBaseType_LastNumeric)
    {
        if (type.baseType == HLSLBaseType_Float || type.baseType == HLSLBaseType_Half) return 1;
        if (type.baseType == HLSLBaseType_Float2 || type.baseType == HLSLBaseType_Half2) return 2;
        if (type.baseType == HLSLBaseType_Float3 || type.baseType == HLSLBaseType_Half3) return 3;
        if (type.baseType == HLSLBaseType_Float4 || type.baseType == HLSLBaseType_Half4) return 4;

    }
    return 0;
}

// Returns dimension, 0 if invalid.
int HLSLTree::GetExpressionValue(HLSLExpression * expression, float values[4])
{
	ASSERT_PARSER(expression != NULL);

    // Expression must be constant.
    if ((expression->expressionType.flags & HLSLTypeFlag_Const) == 0) 
    {
        return 0;
    }

    if (expression->expressionType.baseType == HLSLBaseType_Int ||
        expression->expressionType.baseType == HLSLBaseType_Bool)
    {
        int int_value;
        if (GetExpressionValue(expression, int_value)) {
            for (int i = 0; i < 4; i++) values[i] = (float)int_value;   // @@ Warn if conversion is not exact.
            return 1;
        }

        return 0;
    }
    if (expression->expressionType.baseType >= HLSLBaseType_FirstInteger && expression->expressionType.baseType <= HLSLBaseType_LastInteger)
    {
        // @@ Add support for uints?
        // @@ Add support for int vectors?
        return 0;
    }
    if (expression->expressionType.baseType > HLSLBaseType_LastNumeric)
    {
        return 0;
    }

    // @@ Not supported yet, but we may need it?
    if (expression->expressionType.array) 
    {
        return false;
    }

    if (expression->nodeType == HLSLNodeType_BinaryExpression) 
    {
        HLSLBinaryExpression * binaryExpression = (HLSLBinaryExpression *)expression;
        int dim = GetVectorDimension(binaryExpression->expressionType);

        float values1[4], values2[4];
        int dim1 = GetExpressionValue(binaryExpression->expression1, values1);
        int dim2 = GetExpressionValue(binaryExpression->expression2, values2);

        if (dim1 == 0 || dim2 == 0)
        {
            return 0;
        }

        if (dim1 != dim2)
        {
            // Brodacast scalar to vector size.
            if (dim1 == 1)
            {
                for (int i = 1; i < dim2; i++) values1[i] = values1[0];
                dim1 = dim2;
            }
            else if (dim2 == 1)
            {
                for (int i = 1; i < dim1; i++) values2[i] = values2[0];
                dim2 = dim1;
            }
            else
            {
                return 0;
            }
        }
		ASSERT_PARSER(dim == dim1);

        switch(binaryExpression->binaryOp)
        {
            case HLSLBinaryOp_Add:
                for (int i = 0; i < dim; i++) values[i] = values1[i] + values2[i];
                return dim;
            case HLSLBinaryOp_Sub:
                for (int i = 0; i < dim; i++) values[i] = values1[i] - values2[i];
                return dim;
            case HLSLBinaryOp_Mul:
                for (int i = 0; i < dim; i++) values[i] = values1[i] * values2[i];
                return dim;
            case HLSLBinaryOp_Div:
                for (int i = 0; i < dim; i++) values[i] = values1[i] / values2[i];
                return dim;
            default:
                return 0;
        }
    }
    else if (expression->nodeType == HLSLNodeType_UnaryExpression) 
    {
        HLSLUnaryExpression * unaryExpression = (HLSLUnaryExpression *)expression;
        int dim = GetVectorDimension(unaryExpression->expressionType);

        int dim1 = GetExpressionValue(unaryExpression->expression, values);
        if (dim1 == 0)
        {
            return 0;
        }
		ASSERT_PARSER(dim == dim1);

        switch(unaryExpression->unaryOp)
        {
            case HLSLUnaryOp_Negative:
                for (int i = 0; i < dim; i++) values[i] = -values[i];
                return dim;
            case HLSLUnaryOp_Positive:
                // nop.
                return dim;
            default:
                return 0;
        }
    }
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression * constructor = (HLSLConstructorExpression *)expression;

        int dim = GetVectorDimension(constructor->expressionType);

        int idx = 0;
        for (HLSLExpression * arg: constructor->params)
        {
            float tmp[4];
            int n = GetExpressionValue(arg, tmp);
            for (int i = 0; i < n; i++) values[idx + i] = tmp[i];
            idx += n;
        }
		ASSERT_PARSER(dim == idx);

        return dim;
    }
	else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
	{
		HLSLIdentifierExpression * identifier = (HLSLIdentifierExpression *)expression;

		HLSLDeclaration * declaration = identifier->pDeclaration;
		ASSERT_PARSER(declaration) 

		if ((declaration->type.flags & HLSLTypeFlag_Const) == 0)
		{
			return 0;
		}

		return GetExpressionValue(declaration->assignment, values);
	}
    else if (expression->nodeType == HLSLNodeType_LiteralExpression)
    {
        HLSLLiteralExpression * literal = (HLSLLiteralExpression *)expression;

        if (literal->expressionType.baseType == HLSLBaseType_Float) values[0] = literal->fValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Half) values[0] = literal->fValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Bool) values[0] = literal->bValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Int) values[0] = (float)literal->iValue;  // @@ Warn if conversion is not exact.
        else return 0;

        return 1;
    }

    return 0;
}




void HLSLTreeVisitor::VisitType(HLSLType & type)
{
}

void HLSLTreeVisitor::VisitRoot(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        VisitTopLevelStatement(statement);
        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitTopLevelStatement(HLSLStatement * node)
{
    if (node->nodeType == HLSLNodeType_Declaration) {
        VisitDeclaration((HLSLDeclaration *)node);
    }
    else if (node->nodeType == HLSLNodeType_Struct) {
        VisitStruct((HLSLStruct *)node);
    }
    else if (node->nodeType == HLSLNodeType_Buffer) {
        VisitBuffer((HLSLBuffer *)node);
    }
    else if (node->nodeType == HLSLNodeType_Function) {
        VisitFunction((HLSLFunction *)node);
    }
    else if (node->nodeType == HLSLNodeType_Technique) {
        VisitTechnique((HLSLTechnique *)node);
    }
    else {
		// TODO
		//ASSERT_PARSER(0);
    }
}

void HLSLTreeVisitor::VisitStatements(HLSLStatement * statement)
{
    while (statement != NULL) {
        VisitStatement(statement);
        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitStatement(HLSLStatement * node)
{
	//if (String_Equal(node->name,"localNextPositionRest"))
	//{
	//	int testMe = 0;
	//	testMe++;
	//}
    // Function statements
    if (node->nodeType == HLSLNodeType_Declaration) {
        VisitDeclaration((HLSLDeclaration *)node);
    }
    else if (node->nodeType == HLSLNodeType_ExpressionStatement) {
        VisitExpressionStatement((HLSLExpressionStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ReturnStatement) {
        VisitReturnStatement((HLSLReturnStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_DiscardStatement) {
        VisitDiscardStatement((HLSLDiscardStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_BreakStatement) {
        VisitBreakStatement((HLSLBreakStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ContinueStatement) {
        VisitContinueStatement((HLSLContinueStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_IfStatement) {
        VisitIfStatement((HLSLIfStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ForStatement) {
        VisitForStatement((HLSLForStatement *)node);
    }
	else if (node->nodeType == HLSLNodeType_BlockStatement) {
		VisitBlockStatement((HLSLBlockStatement *)node);
	}
	else if (node->nodeType == HLSLNodeType_WhileStatement) {
		//VisitWhileStatement((HLSLWhileStatement *)node);
	}
	else if (node->nodeType == HLSLNodeType_SwitchStatement) {
		//VisitWhileStatement((HLSLWhileStatement *)node);
	}
	else {
		ASSERT_PARSER(0);
    }
}

void HLSLTreeVisitor::VisitDeclaration(HLSLDeclaration * node)
{
    VisitType(node->type);

	if (node->assignment)
		VisitExpression(node->assignment);

    if (node->nextDeclaration != NULL) {
        VisitDeclaration(node->nextDeclaration);
    }
}

void HLSLTreeVisitor::VisitStruct(HLSLStruct * node)
{
    HLSLStructField * field = node->field;
    while (field != NULL) {
        VisitStructField(field);
        field = field->nextField;
    }
}

void HLSLTreeVisitor::VisitStructField(HLSLStructField * node)
{
    VisitType(node->type);
}

void HLSLTreeVisitor::VisitBuffer(HLSLBuffer * node)
{
    HLSLDeclaration * field = node->field;
    while (field != NULL) {
		ASSERT_PARSER(field->nodeType == HLSLNodeType_Declaration);
        VisitDeclaration(field);
		ASSERT_PARSER(field->nextDeclaration == NULL);
        field = (HLSLDeclaration *)field->nextStatement;
    }
}

void HLSLTreeVisitor::VisitFunction(HLSLFunction * node)
{
	VisitType(node->returnType);

	for (HLSLArgument* arg: node->args)
	{
		VisitArgument(arg);
	}

	VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitArgument(HLSLArgument * node)
{
    VisitType(node->type);
    if (node->defaultValue != NULL) {
        VisitExpression(node->defaultValue);
    }
}

void HLSLTreeVisitor::VisitExpressionStatement(HLSLExpressionStatement * node)
{
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitInitListExpression(HLSLInitListExpression * node)
{
	for (HLSLExpression* expression: node->initExpressions)
		VisitExpression(expression);
}

void HLSLTreeVisitor::VisitExpression(HLSLExpression * node)
{
	VisitType(node->expressionType);

	if (node->nodeType == HLSLNodeType_InitListExpression) {
		VisitInitListExpression((HLSLInitListExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_UnaryExpression) {
		VisitUnaryExpression((HLSLUnaryExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_BinaryExpression) {
		VisitBinaryExpression((HLSLBinaryExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_ConditionalExpression) {
		VisitConditionalExpression((HLSLConditionalExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_CastingExpression) {
		VisitCastingExpression((HLSLCastingExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_LiteralExpression) {
		VisitLiteralExpression((HLSLLiteralExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_IdentifierExpression) {
		VisitIdentifierExpression((HLSLIdentifierExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_ConstructorExpression) {
		VisitConstructorExpression((HLSLConstructorExpression *)node);
	}
	else if (node->nodeType == HLSLNodeType_MemberAccess) {
		VisitMemberAccess((HLSLMemberAccess *)node);
	}
	else if (node->nodeType == HLSLNodeType_ArrayAccess) {
		VisitArrayAccess((HLSLArrayAccess *)node);
	}
	else if (node->nodeType == HLSLNodeType_FunctionCall) {
		VisitFunctionCall((HLSLFunctionCall *)node);
	}
	// Acoget-TODO: This was missing. Did adding it break anything?
	else if (node->nodeType == HLSLNodeType_SamplerState) {
		VisitSamplerState((HLSLSamplerState *)node);
	}
	else if (node->nodeType == HLSLNodeType_TextureState) {
		VisitTextureState((HLSLTextureState *)node);
	}
	else {
		// TODO
		ASSERT_PARSER(0);
	}
}

void HLSLTreeVisitor::VisitReturnStatement(HLSLReturnStatement * node)
{
	if(node->expression)
		VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitDiscardStatement(HLSLDiscardStatement * node) {}
void HLSLTreeVisitor::VisitBreakStatement(HLSLBreakStatement * node) {}
void HLSLTreeVisitor::VisitContinueStatement(HLSLContinueStatement * node) {}

void HLSLTreeVisitor::VisitIfStatement(HLSLIfStatement * node)
{
    VisitExpression(node->condition);
    VisitStatements(node->statement);

	for (int i = 0; i < node->elseifStatement.size(); i++)
	{
		HLSLIfStatement* currElseIf = node->elseifStatement[i];
		//VisitIfStatement(node->elseifStatement[i]);
		VisitExpression(currElseIf->condition);
		VisitStatements(currElseIf->statement);
	}

    if (node->elseStatement) {
        VisitStatements(node->elseStatement);
    }
}

void HLSLTreeVisitor::VisitForStatement(HLSLForStatement * node)
{
    if (node->initialization) {
        VisitDeclaration(node->initialization);

    }
    if (node->condition) {
        VisitExpression(node->condition);
    }
    if (node->increment) {
        VisitExpression(node->increment);
    }
    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitBlockStatement(HLSLBlockStatement * node)
{
    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitUnaryExpression(HLSLUnaryExpression * node)
{
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitBinaryExpression(HLSLBinaryExpression * node)
{
    VisitExpression(node->expression1);
    VisitExpression(node->expression2);
}

void HLSLTreeVisitor::VisitConditionalExpression(HLSLConditionalExpression * node)
{
    VisitExpression(node->condition);
    VisitExpression(node->falseExpression);
    VisitExpression(node->trueExpression);
}

void HLSLTreeVisitor::VisitCastingExpression(HLSLCastingExpression * node)
{
    VisitType(node->expressionType);
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitLiteralExpression(HLSLLiteralExpression * node) {}
void HLSLTreeVisitor::VisitIdentifierExpression(HLSLIdentifierExpression * node) {}

void HLSLTreeVisitor::VisitConstructorExpression(HLSLConstructorExpression * node)
{
	for (HLSLExpression * argument: node->params)
	{
		VisitExpression(argument);
	}
}

void HLSLTreeVisitor::VisitMemberAccess(HLSLMemberAccess * node)
{
	// if node->object is just an expression, it probably points back to itself, but if it's a function
	// then we should traverse
	if (node->object != NULL)
	{
		VisitExpression(node->object);
	}
}

void HLSLTreeVisitor::VisitArrayAccess(HLSLArrayAccess * node)
{
	// sometimes node->array points to itself
	if (node != node->array)
	{
		VisitExpression(node->array);
	}

	VisitExpression(node->index);
}

void HLSLTreeVisitor::VisitFunctionCall(HLSLFunctionCall * node)
{
	for (HLSLExpression* param: node->params)
	{
		VisitExpression(param);
	}
}

void HLSLTreeVisitor::VisitStateAssignment(HLSLStateAssignment * node) {}

void HLSLTreeVisitor::VisitSamplerState(HLSLSamplerState * node)
{
    HLSLStateAssignment * stateAssignment = node->stateAssignments;
    while (stateAssignment != NULL) {
        VisitStateAssignment(stateAssignment);
        stateAssignment = stateAssignment->nextStateAssignment;
    }
}

void HLSLTreeVisitor::VisitTextureState(HLSLTextureState * node)
{
	// no op? not much to do here.
}

void HLSLTreeVisitor::VisitPass(HLSLPass * node)
{
    HLSLStateAssignment * stateAssignment = node->stateAssignments;
    while (stateAssignment != NULL) {
        VisitStateAssignment(stateAssignment);
        stateAssignment = stateAssignment->nextStateAssignment;
    }
}

void HLSLTreeVisitor::VisitTechnique(HLSLTechnique * node)
{
    HLSLPass * pass = node->passes;
    while (pass != NULL) {
        VisitPass(pass);
        pass = pass->nextPass;
    }
}

void HLSLTreeVisitor::VisitSamplerIdentifier(const CachedString & name)
{
	// no op, just here to be overloaded
}

void HLSLTreeVisitor::VisitTextureIdentifier(const CachedString & name)
{
	// no op, just here to be overloaded
}

void HLSLTreeVisitor::VisitFunctions(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        if (statement->nodeType == HLSLNodeType_Function) {
            VisitFunction((HLSLFunction *)statement);
        }

        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitParameters(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        if (statement->nodeType == HLSLNodeType_Declaration) {
            VisitDeclaration((HLSLDeclaration *)statement);
        }

        statement = statement->nextStatement;
    }
}


class ResetHiddenFlagVisitor : public HLSLTreeVisitor
{
public:
    virtual void VisitTopLevelStatement(HLSLStatement * statement)
    {
        statement->hidden = true;

        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            VisitBuffer((HLSLBuffer*)statement);
        }
    }

    // Hide buffer fields.
    virtual void VisitDeclaration(HLSLDeclaration * node)
    {
        node->hidden = true;
    }

    virtual void VisitArgument(HLSLArgument * node)
    {
        node->hidden = false;   // Arguments are visible by default.
    }
};

class MarkVisibleStatementsVisitor : public HLSLTreeVisitor
{
public:
    HLSLTree * tree;
    MarkVisibleStatementsVisitor(HLSLTree * tree) : tree(tree) {}

    virtual void VisitFunction(HLSLFunction * node)
    {
        node->hidden = false;
        HLSLTreeVisitor::VisitFunction(node);

        if (node->forward)
            VisitFunction(node->forward);
    }

    virtual void VisitFunctionCall(HLSLFunctionCall * node)
    {
        HLSLTreeVisitor::VisitFunctionCall(node);

        if (node->function->hidden)
        {
            VisitFunction(const_cast<HLSLFunction*>(node->function));
        }
    }

    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node)
    {
        HLSLTreeVisitor::VisitIdentifierExpression(node);
        HLSLDeclaration * declaration = node->pDeclaration;
		ASSERT_PARSER(declaration);

        if (declaration->global && declaration->hidden)
        {
            declaration->hidden = false;
            VisitDeclaration(declaration);
        }
    }

    virtual void VisitType(HLSLType & type)
    {
        if (type.baseType == HLSLBaseType_UserDefined)
        {
            HLSLStruct * globalStruct = tree->FindGlobalStruct(type.typeName);
            if (globalStruct != NULL)
            {
                globalStruct->hidden = false;
                VisitStruct(globalStruct);
            }
        }
    }

};


void PruneTree(HLSLTree* tree, const char * srcEntryName0, const char * srcEntryName1/*=NULL*/)
{
    HLSLRoot* root = tree->GetRoot();

	CachedString entryName0 = tree->AddStringCached(srcEntryName0);
	CachedString entryName1 = tree->AddStringCached(srcEntryName1);

    // Reset all flags.
    ResetHiddenFlagVisitor reset;
    reset.VisitRoot(root);

    // Mark all the statements necessary for these entrypoints.
    HLSLFunction* entry = tree->FindFunction(entryName0);
    if (entry != NULL)
    {
        MarkVisibleStatementsVisitor mark(tree);
        mark.VisitFunction(entry);
    }

    if (entryName1.IsNotEmpty())
    {
        entry = tree->FindFunction(entryName1);
        if (entry != NULL)
        {
            MarkVisibleStatementsVisitor mark(tree);
            mark.VisitFunction(entry);
        }
    }

    // Mark buffers visible, if any of their fields is visible.
    HLSLStatement * statement = root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer* buffer = (HLSLBuffer*)statement;

            HLSLDeclaration* field = buffer->field;
            while (field != NULL)
            {
				ASSERT_PARSER(field->nodeType == HLSLNodeType_Declaration);
                if (!field->hidden)
                {
                    buffer->hidden = false;
                    break;
                }
                field = (HLSLDeclaration*)field->nextStatement;
            }
        }

        statement = statement->nextStatement;
    }
}


void SortTree(HLSLTree * tree)
{
    // Stable sort so that statements are in this order:
    // structs, declarations, functions, techniques.
	// but their relative order is preserved.

    HLSLRoot* root = tree->GetRoot();

    HLSLStatement* structs = NULL;
    HLSLStatement* lastStruct = NULL;
    HLSLStatement* constDeclarations = NULL;
    HLSLStatement* lastConstDeclaration = NULL;
    HLSLStatement* declarations = NULL;
    HLSLStatement* lastDeclaration = NULL;
    HLSLStatement* functions = NULL;
    HLSLStatement* lastFunction = NULL;
    HLSLStatement* other = NULL;
    HLSLStatement* lastOther = NULL;

    HLSLStatement* statement = root->statement;
    while (statement != NULL) {
        HLSLStatement* nextStatement = statement->nextStatement;
        statement->nextStatement = NULL;

        if (statement->nodeType == HLSLNodeType_Struct) {
            if (structs == NULL) structs = statement;
            if (lastStruct != NULL) lastStruct->nextStatement = statement;
            lastStruct = statement;
        }
        else if (statement->nodeType == HLSLNodeType_Declaration || statement->nodeType == HLSLNodeType_Buffer) {
            if (statement->nodeType == HLSLNodeType_Declaration && (((HLSLDeclaration *)statement)->type.flags & HLSLTypeFlag_Const)) {
                if (constDeclarations == NULL) constDeclarations = statement;
                if (lastConstDeclaration != NULL) lastConstDeclaration->nextStatement = statement;
                lastConstDeclaration = statement;
            }
            else {
                if (declarations == NULL) declarations = statement;
                if (lastDeclaration != NULL) lastDeclaration->nextStatement = statement;
                lastDeclaration = statement;
            }
        }
        else if (statement->nodeType == HLSLNodeType_Function) {
            if (functions == NULL) functions = statement;
            if (lastFunction != NULL) lastFunction->nextStatement = statement;
            lastFunction = statement;
        }
        else {
            if (other == NULL) other = statement;
            if (lastOther != NULL) lastOther->nextStatement = statement;
            lastOther = statement;
        }

        statement = nextStatement;
    }

    // Chain all the statements in the order that we want.
    HLSLStatement * firstStatement = structs;
    HLSLStatement * lastStatement = lastStruct;

    if (constDeclarations != NULL) {
        if (firstStatement == NULL) firstStatement = constDeclarations;
        else lastStatement->nextStatement = constDeclarations;
        lastStatement = lastConstDeclaration;
    }

    if (declarations != NULL) {
        if (firstStatement == NULL) firstStatement = declarations;
        else lastStatement->nextStatement = declarations;
        lastStatement = lastDeclaration;
    }

    if (functions != NULL) {
        if (firstStatement == NULL) firstStatement = functions;
        else lastStatement->nextStatement = functions;
        lastStatement = lastFunction;
    }

    if (other != NULL) {
        if (firstStatement == NULL) firstStatement = other;
        else lastStatement->nextStatement = other;
        lastStatement = lastOther;
    }

    root->statement = firstStatement;
}

// First and last can be the same.
void AddStatements(HLSLRoot * root, HLSLStatement * before, HLSLStatement * first, HLSLStatement * last)
{
    if (before == NULL) {
        last->nextStatement = root->statement;
        root->statement = first;
    }
    else {
        last->nextStatement = before->nextStatement;
        before->nextStatement = first;
    }
}

void AddSingleStatement(HLSLRoot * root, HLSLStatement * before, HLSLStatement * statement)
{
    AddStatements(root, before, statement, statement);
}

class FindArgumentVisitor : public HLSLTreeVisitor
{
public:
    bool found;
    CachedString name;

	FindArgumentVisitor()
	{
		found = false;
		name.Reset();// = NULL;
	}

    bool FindArgument(const CachedString & name, HLSLFunction * function)
    {
        this->found = false;
        this->name = name;
        VisitStatements(function->statement);
        return found;
    }

    virtual void VisitStatements(HLSLStatement * statement) override
    {
        while (statement != NULL && !found)
        {
            VisitStatement(statement);
            statement = statement->nextStatement;
        }
    }

    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node) override
    {
        if (String_Equal(node->pDeclaration->name, name))
        {
            found = true;
        }
    }
};

bool HLSLTree::IsCustomMultiply(HLSLBaseType lhsType, HLSLBaseType rhsType)
{
	bool lhsMatrixOrVector = (IsMatrixType(lhsType) || isVectorType(lhsType));
	bool rhsMatrixOrVector = (IsMatrixType(rhsType) || isVectorType(rhsType));
	bool isEitherMatrix = (IsMatrixType(lhsType) || IsMatrixType(rhsType));

	// to add this type, we must meet all three conditions
	// 1. LHS is either matrix or vector
	// 2. RHS is either matrix or vector
	// 3. At least one of LHS/RHS must be a matrix
	if (lhsMatrixOrVector && rhsMatrixOrVector && isEitherMatrix)
	{
		return true;
	}

	return false;
}



class FindMatrixMultiplyVisitor : public HLSLTreeVisitor
{
public:

	eastl::vector < HLSLBaseType > lhsTypeVec;
	eastl::vector < HLSLBaseType > rhsTypeVec;

	FindMatrixMultiplyVisitor()
	{
		lhsTypeVec.clear();
		rhsTypeVec.clear();
	}

	void AddTypes(HLSLBaseType lhsType, HLSLBaseType rhsType)
	{
		bool isCustom = HLSLTree::IsCustomMultiply(lhsType,rhsType);

		if (isCustom)
		{
			int num = (int)lhsTypeVec.size();
			ASSERT_PARSER(lhsTypeVec.size() == rhsTypeVec.size());

			// if we already have this one in the list, then don't add it
			bool found = false;
			for (int i = 0; i < num; i++)
			{
				if (lhsType == lhsTypeVec[i] &&
					rhsType == rhsTypeVec[i])
				{
					found = true;
				}
			}

			if (!found)
			{
				lhsTypeVec.push_back(lhsType);
				rhsTypeVec.push_back(rhsType);
			}
		}
	}

	virtual void VisitFunctionCall(HLSLFunctionCall * node)
	{
		bool found = String_Equal("mul", node->function->name);

		if (found)
		{
			const eastl::vector<HLSLArgument*>& argVec = node->function->args;
			ASSERT_PARSER(argVec.size() == 2);

			HLSLBaseType lhsType = argVec[0]->type.baseType;
			HLSLBaseType rhsType = argVec[1]->type.baseType;
			AddTypes(lhsType, rhsType);
		}
		HLSLTreeVisitor::VisitFunctionCall(node);
	}


	virtual void VisitBinaryExpression(HLSLBinaryExpression * node) override
	{
		if (node->binaryOp == HLSLBinaryOp_Mul)
		{
			HLSLBaseType lhsType = node->expression1->expressionType.baseType;
			HLSLBaseType rhsType = node->expression2->expressionType.baseType;

			AddTypes(lhsType, rhsType);
		}

		VisitExpression(node->expression1);
		VisitExpression(node->expression2);
	}
};

void HLSLTree::FindMatrixMultiplyTypes(
	eastl::vector < HLSLBaseType > & lhsTypeVec,
	eastl::vector < HLSLBaseType > & rhsTypeVec) const
{
	FindMatrixMultiplyVisitor visitor;

	visitor.VisitRoot(m_root);

	lhsTypeVec = visitor.lhsTypeVec;
	rhsTypeVec = visitor.rhsTypeVec;
}



class FindTextureLoadVisitor : public HLSLTreeVisitor
{
public:

	eastl::vector < HLSLBaseType > textureTypeVec;
	eastl::vector < HLSLBaseType > paramTypeVec;

	FindTextureLoadVisitor()
	{
		textureTypeVec.clear();
		paramTypeVec.clear();
	}

	void AddTextureCombo(HLSLBaseType texType, HLSLBaseType indexType)
	{
		for (int i = 0; i < textureTypeVec.size(); i++)
		{
			if (textureTypeVec[i] == texType &&
				paramTypeVec[i] == indexType)
			{
				// we already have it
				return;
			}
		}

		textureTypeVec.push_back(texType);
		paramTypeVec.push_back(indexType);
	}

	virtual void VisitFunctionCall(HLSLFunctionCall * node) override
	{
		HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(node);
		CachedString name = functionCall->function->name;

		HLSLTreeVisitor::VisitFunctionCall(node);
	}
};


void HLSLTree::FindTextureLoadOverloads(
	eastl::vector < HLSLBaseType > & textureTypeVec,
	eastl::vector < HLSLBaseType > & paramTypeVec) const
{
	FindTextureLoadVisitor visitor;
	visitor.VisitRoot(m_root);

	textureTypeVec = visitor.textureTypeVec;
	paramTypeVec = visitor.paramTypeVec;
}

void HideUnusedArguments(HLSLFunction * function)
{
	// this function probably doesn't work
    FindArgumentVisitor visitor;

	for (HLSLArgument* arg: function->args)
	{
		if (!visitor.FindArgument(arg->name, function))
		{
			arg->hidden = true;
		}
	}
}

bool EmulateAlphaTest(HLSLTree* tree, const char * srcEntryName, float alphaRef/*=0.5*/)
{
	CachedString entryName = tree->AddStringCached(srcEntryName);

    // Find all return statements of this entry point.
    HLSLFunction* entry = tree->FindFunction(entryName);
    if (entry != NULL)
    {
        HLSLStatement ** ptr = &entry->statement;
        HLSLStatement * statement = entry->statement;
        while (statement != NULL)
        {
            if (statement->nodeType == HLSLNodeType_ReturnStatement)
            {
                HLSLReturnStatement * returnStatement = (HLSLReturnStatement *)statement;
                HLSLBaseType returnType = returnStatement->expression->expressionType.baseType;
                
                // Build statement: "if (%s.a < 0.5) discard;"

                HLSLDiscardStatement * discard = tree->AddNode<HLSLDiscardStatement>(statement->fileName, statement->line);
                
                HLSLExpression * alpha = NULL;
                if (returnType == HLSLBaseType_Float4 || returnType == HLSLBaseType_Half4)
                {
                    // @@ If return expression is a constructor, grab 4th argument.
                    // That's not as easy, since we support 'float4(float3, float)' or 'float4(float, float3)', extracting
                    // the latter is not that easy.
                    /*if (returnStatement->expression->nodeType == HLSLNodeType_ConstructorExpression) {
                        HLSLConstructorExpression * constructor = (HLSLConstructorExpression *)returnStatement->expression;
                        //constructor->
                    }
                    */
                    
                    if (alpha == NULL) {
                        HLSLMemberAccess * access = tree->AddNode<HLSLMemberAccess>(statement->fileName, statement->line);
                        access->expressionType = HLSLType(HLSLBaseType_Float);
                        access->object = returnStatement->expression;     // @@ Is reference OK? Or should we clone expression?
                        access->field = tree->AddStringCached("a");
                        access->swizzle = true;
                        
                        alpha = access;
                    }
                }
                else if (returnType == HLSLBaseType_Float || returnType == HLSLBaseType_Half)
                {
                    alpha = returnStatement->expression;     // @@ Is reference OK? Or should we clone expression?
                }
                else {
                    return false;
                }
                
                HLSLLiteralExpression * threshold = tree->AddNode<HLSLLiteralExpression>(statement->fileName, statement->line);
                threshold->expressionType = HLSLType(HLSLBaseType_Float);
                threshold->fValue = alphaRef;
                threshold->type = HLSLBaseType_Float;
                
                HLSLBinaryExpression * condition = tree->AddNode<HLSLBinaryExpression>(statement->fileName, statement->line);
                condition->expressionType = HLSLType(HLSLBaseType_Bool);
                condition->binaryOp = HLSLBinaryOp_Less;
                condition->expression1 = alpha;
                condition->expression2 = threshold;

                // Insert statement.
                HLSLIfStatement * st = tree->AddNode<HLSLIfStatement>(statement->fileName, statement->line);
                st->condition = condition;
                st->statement = discard;
                st->nextStatement = statement;
                *ptr = st;
            }
        
            ptr = &statement->nextStatement;
            statement = statement->nextStatement;
        }
    }

    return true;
}

