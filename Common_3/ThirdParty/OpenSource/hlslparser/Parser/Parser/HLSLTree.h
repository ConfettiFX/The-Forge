#ifndef HLSL_TREE_H
#define HLSL_TREE_H

#include "Engine.h"

#include <new>
#include <stdint.h>
#include "../../../EASTL/vector.h"
#include "../../../EASTL/string.h"

#include <string.h>

enum
{
	MAX_DIM = 3,
};

class StringLibrary;

// define this to have strings for debugging

struct CachedString
{
	CachedString()
	{
		Reset();
	}

	void Reset()
	{
		m_string = "";
	}

	bool IsEmpty() const
	{
		return !m_string || !m_string[0];
	}

	bool IsNotEmpty() const
	{
		return !IsEmpty();
	}

	const char* m_string;
};

static inline const char * RawStr(const CachedString & cacheStr)
{
	return cacheStr.m_string ? cacheStr.m_string : "";
}

inline bool operator==(const CachedString & lhs, const CachedString & rhs)
{
	bool fullTest = String_Equal(lhs.m_string, rhs.m_string);

	return fullTest;
}

inline bool String_Equal(const CachedString & a, const char * b)
{
	ASSERT_PARSER(b != NULL);

	int cmp = strcmp(a.m_string,b);
	return cmp == 0;
}

inline bool String_Equal(const char * b, const CachedString & a)
{
	ASSERT_PARSER(b != NULL);

	int cmp = strcmp(a.m_string, b);
	return cmp == 0;
}

inline bool String_Equal(const CachedString & lhs, const CachedString & rhs)
{
	bool fullTest = String_Equal(lhs.m_string, rhs.m_string);

	return fullTest;
}

// helper function to get the c_str() from a cached string by looking up the string library
const char * FetchCstr(const StringLibrary * stringLibrary, const CachedString & cstr);

// Does nothing, just returns the type. But we get a compile tiem check that we have a raw C string.
// Since strings often get used in sprintf() statements with no type checking, we put this around
// parameters so we don't accidentally pass in a uint64_t hash.
inline const char * CHECK_CSTR(const char * cstr)
{
	return cstr;
}

enum HLSLNodeType
{
	HLSLNodeType_Unknown,
    HLSLNodeType_Root,
    HLSLNodeType_Declaration,
    HLSLNodeType_Struct,
    HLSLNodeType_StructField,
    HLSLNodeType_Buffer,
    HLSLNodeType_BufferField,
    HLSLNodeType_Function,
    HLSLNodeType_Argument,
    HLSLNodeType_ExpressionStatement,
    HLSLNodeType_Expression,
	HLSLNodeType_InitListExpression,
    HLSLNodeType_ReturnStatement,
    HLSLNodeType_DiscardStatement,
    HLSLNodeType_BreakStatement,
    HLSLNodeType_ContinueStatement,
    HLSLNodeType_IfStatement,
    HLSLNodeType_ForStatement,
	HLSLNodeType_WhileStatement,
	HLSLNodeType_SwitchStatement,
    HLSLNodeType_BlockStatement,
    HLSLNodeType_UnaryExpression,
    HLSLNodeType_BinaryExpression,
    HLSLNodeType_ConditionalExpression,
    HLSLNodeType_CastingExpression,
    HLSLNodeType_LiteralExpression,
    HLSLNodeType_IdentifierExpression,
    HLSLNodeType_ConstructorExpression,	
    HLSLNodeType_MemberAccess,
    HLSLNodeType_ArrayAccess,
    HLSLNodeType_FunctionCall,
    HLSLNodeType_StateAssignment,
	HLSLNodeType_GroupShared,
    HLSLNodeType_SamplerState,
    HLSLNodeType_Pass,
    HLSLNodeType_Technique,
    HLSLNodeType_Attribute,
    HLSLNodeType_Pipeline,
    HLSLNodeType_Stage,
	HLSLNodeType_TextureState,
	HLSLNodeType_TextureStateExpression,
	HLSLNodeType_PatchControlPoint,

};

enum HLSLBaseType
{
    HLSLBaseType_Unknown,
    HLSLBaseType_Void,

	HLSLBaseType_Float,
	HLSLBaseType_FirstNumeric = HLSLBaseType_Float,
	HLSLBaseType_Float1x2,
	HLSLBaseType_Float1x3,
	HLSLBaseType_Float1x4,
	HLSLBaseType_Float2,
	HLSLBaseType_Float2x2,
	HLSLBaseType_Float2x3,
	HLSLBaseType_Float2x4,
	HLSLBaseType_Float3,
	HLSLBaseType_Float3x2,
	HLSLBaseType_Float3x3,
	HLSLBaseType_Float3x4,
	HLSLBaseType_Float4,
	HLSLBaseType_Float4x2,
	HLSLBaseType_Float4x3,
	HLSLBaseType_Float4x4,	

	HLSLBaseType_Half,
	HLSLBaseType_Half1x2,
	HLSLBaseType_Half1x3,
	HLSLBaseType_Half1x4,
	HLSLBaseType_Half2,
	HLSLBaseType_Half2x2,
	HLSLBaseType_Half2x3,
	HLSLBaseType_Half2x4,
	HLSLBaseType_Half3,
	HLSLBaseType_Half3x2,
	HLSLBaseType_Half3x3,
	HLSLBaseType_Half3x4,
	HLSLBaseType_Half4,
	HLSLBaseType_Half4x2,
	HLSLBaseType_Half4x3,
	HLSLBaseType_Half4x4,

	HLSLBaseType_Min16Float,
	HLSLBaseType_Min16Float1x2,
	HLSLBaseType_Min16Float1x3,
	HLSLBaseType_Min16Float1x4,
	HLSLBaseType_Min16Float2,
	HLSLBaseType_Min16Float2x2,
	HLSLBaseType_Min16Float2x3,
	HLSLBaseType_Min16Float2x4,
	HLSLBaseType_Min16Float3,
	HLSLBaseType_Min16Float3x2,
	HLSLBaseType_Min16Float3x3,
	HLSLBaseType_Min16Float3x4,
	HLSLBaseType_Min16Float4,
	HLSLBaseType_Min16Float4x2,
	HLSLBaseType_Min16Float4x3,
	HLSLBaseType_Min16Float4x4,

	HLSLBaseType_Min10Float,
	HLSLBaseType_Min10Float1x2,
	HLSLBaseType_Min10Float1x3,
	HLSLBaseType_Min10Float1x4,
	HLSLBaseType_Min10Float2,
	HLSLBaseType_Min10Float2x2,
	HLSLBaseType_Min10Float2x3,
	HLSLBaseType_Min10Float2x4,
	HLSLBaseType_Min10Float3,
	HLSLBaseType_Min10Float3x2,
	HLSLBaseType_Min10Float3x3,
	HLSLBaseType_Min10Float3x4,
	HLSLBaseType_Min10Float4,
	HLSLBaseType_Min10Float4x2,
	HLSLBaseType_Min10Float4x3,
	HLSLBaseType_Min10Float4x4,

	HLSLBaseType_Bool,
	HLSLBaseType_FirstInteger = HLSLBaseType_Bool,
	HLSLBaseType_Bool1x2,
	HLSLBaseType_Bool1x3,
	HLSLBaseType_Bool1x4,
	HLSLBaseType_Bool2,
	HLSLBaseType_Bool2x2,
	HLSLBaseType_Bool2x3,
	HLSLBaseType_Bool2x4,
	HLSLBaseType_Bool3,
	HLSLBaseType_Bool3x2,
	HLSLBaseType_Bool3x3,
	HLSLBaseType_Bool3x4,
	HLSLBaseType_Bool4,
	HLSLBaseType_Bool4x2,
	HLSLBaseType_Bool4x3,
	HLSLBaseType_Bool4x4,

	HLSLBaseType_Int,
	HLSLBaseType_Int1x2,
	HLSLBaseType_Int1x3,
	HLSLBaseType_Int1x4,
	HLSLBaseType_Int2,
	HLSLBaseType_Int2x2,
	HLSLBaseType_Int2x3,
	HLSLBaseType_Int2x4,
	HLSLBaseType_Int3,
	HLSLBaseType_Int3x2,
	HLSLBaseType_Int3x3,
	HLSLBaseType_Int3x4,
	HLSLBaseType_Int4,
	HLSLBaseType_Int4x2,
	HLSLBaseType_Int4x3,
	HLSLBaseType_Int4x4,

	HLSLBaseType_Uint,
	HLSLBaseType_Uint1x2,
	HLSLBaseType_Uint1x3,
	HLSLBaseType_Uint1x4,
	HLSLBaseType_Uint2,
	HLSLBaseType_Uint2x2,
	HLSLBaseType_Uint2x3,
	HLSLBaseType_Uint2x4,
	HLSLBaseType_Uint3,
	HLSLBaseType_Uint3x2,
	HLSLBaseType_Uint3x3,
	HLSLBaseType_Uint3x4,
	HLSLBaseType_Uint4,	
	HLSLBaseType_Uint4x2,
	HLSLBaseType_Uint4x3,
	HLSLBaseType_Uint4x4,
	HLSLBaseType_LastInteger = HLSLBaseType_Uint4x4,
	HLSLBaseType_LastNumeric = HLSLBaseType_Uint4x4,
	
	HLSLBaseType_InputPatch,
	HLSLBaseType_OutputPatch,

	HLSLBaseType_PointStream,
	HLSLBaseType_LineStream,
	HLSLBaseType_TriangleStream,

	HLSLBaseType_Point,
	HLSLBaseType_Line,
	HLSLBaseType_Triangle,
	HLSLBaseType_Lineadj,
	HLSLBaseType_Triangleadj,
	
	

    HLSLBaseType_Texture,

	HLSLBaseType_Texture1D,
	HLSLBaseType_Texture1DArray,
	HLSLBaseType_Texture2D,
	HLSLBaseType_Texture2DArray,
	HLSLBaseType_Texture3D,
	HLSLBaseType_Texture2DMS,
	HLSLBaseType_Texture2DMSArray,
	HLSLBaseType_TextureCube,
	HLSLBaseType_TextureCubeArray,

	HLSLBaseType_RasterizerOrderedTexture1D,
	HLSLBaseType_RasterizerOrderedTexture1DArray,
	HLSLBaseType_RasterizerOrderedTexture2D,
	HLSLBaseType_RasterizerOrderedTexture2DArray,
	HLSLBaseType_RasterizerOrderedTexture3D,

	HLSLBaseType_RWTexture1D,
	HLSLBaseType_RWTexture1DArray,
	HLSLBaseType_RWTexture2D,
	HLSLBaseType_RWTexture2DArray,
	HLSLBaseType_RWTexture3D,

    HLSLBaseType_Sampler,           // @@ use type inference to determine sampler type.
    HLSLBaseType_Sampler2D,
    HLSLBaseType_Sampler3D,
    HLSLBaseType_SamplerCube,
    HLSLBaseType_Sampler2DShadow,
    HLSLBaseType_Sampler2DMS,
    HLSLBaseType_Sampler2DArray,

    HLSLBaseType_UserDefined,       // struct
	HLSLBaseType_SamplerState,
	HLSLBaseType_SamplerComparisonState,
	HLSLBaseType_TextureState,
	HLSLBaseType_RWTextureState,

	HLSLBaseType_PatchControlPoint,

	//Now, these are for MSL only
	HLSLBaseType_DepthTexture2D,
	HLSLBaseType_DepthTexture2DArray,
	HLSLBaseType_DepthTexture2DMS,
	HLSLBaseType_DepthTexture2DMSArray,
	HLSLBaseType_DepthTextureCube,
	HLSLBaseType_DepthTextureCubeArray,

	HLSLBaseType_AddressU,
	HLSLBaseType_AddressV,
	HLSLBaseType_AddressW,
	HLSLBaseType_BorderColor,
	HLSLBaseType_Filter,
	HLSLBaseType_MaxAnisotropy,
	HLSLBaseType_MaxLOD,
	HLSLBaseType_MinLOD,
	HLSLBaseType_MipLODBias,
	HLSLBaseType_ComparisonFunc,


	HLSLBaseType_CBuffer,
	HLSLBaseType_TBuffer,
	HLSLBaseType_ConstantBuffer,
	HLSLBaseType_StructuredBuffer,
	HLSLBaseType_PureBuffer,
	HLSLBaseType_RWBuffer,
	HLSLBaseType_RWStructuredBuffer,
	HLSLBaseType_ByteAddressBuffer,
	HLSLBaseType_RWByteAddressBuffer,

	HLSLBaseType_RasterizerOrderedBuffer,
	HLSLBaseType_RasterizerOrderedStructuredBuffer,
	HLSLBaseType_RasterizerOrderedByteAddressBuffer,


	HLSLBaseType_UserMacro,
	HLSLBaseType_Empty,
	
    
    HLSLBaseType_Count,
    HLSLBaseType_NumericCount = HLSLBaseType_LastNumeric - HLSLBaseType_FirstNumeric + 1
};

inline bool IsTextureType(HLSLBaseType baseType)
{
	return baseType >= HLSLBaseType_Texture1D && baseType <= HLSLBaseType_RWTexture3D;
}

inline bool IsSamplerType(HLSLBaseType baseType)
{
    return baseType == HLSLBaseType_Sampler ||
           baseType == HLSLBaseType_Sampler2D ||
           baseType == HLSLBaseType_Sampler3D ||
           baseType == HLSLBaseType_SamplerCube ||
           baseType == HLSLBaseType_Sampler2DShadow ||
           baseType == HLSLBaseType_Sampler2DMS ||
           baseType == HLSLBaseType_Sampler2DArray;
}

inline bool IsMatrixType(HLSLBaseType baseType)
{
	return  baseType == HLSLBaseType_Float2x2 ||
		baseType == HLSLBaseType_Float2x3 ||
		baseType == HLSLBaseType_Float2x4 ||
		baseType == HLSLBaseType_Float3x2 ||
		baseType == HLSLBaseType_Float3x3 ||
		baseType == HLSLBaseType_Float3x4 ||
		baseType == HLSLBaseType_Float4x2 ||
		baseType == HLSLBaseType_Float4x3 ||
		baseType == HLSLBaseType_Float4x4 ||

		baseType == HLSLBaseType_Half2x2 ||
		baseType == HLSLBaseType_Half2x3 ||
		baseType == HLSLBaseType_Half2x4 ||
		baseType == HLSLBaseType_Half3x2 ||
		baseType == HLSLBaseType_Half3x3 ||
		baseType == HLSLBaseType_Half3x4 ||
		baseType == HLSLBaseType_Half4x2 ||
		baseType == HLSLBaseType_Half4x3 ||
		baseType == HLSLBaseType_Half4x4 ||

		baseType == HLSLBaseType_Bool2x2 ||
		baseType == HLSLBaseType_Bool2x3 ||
		baseType == HLSLBaseType_Bool2x4 ||
		baseType == HLSLBaseType_Bool3x2 ||
		baseType == HLSLBaseType_Bool3x3 ||
		baseType == HLSLBaseType_Bool3x4 ||
		baseType == HLSLBaseType_Bool4x2 ||
		baseType == HLSLBaseType_Bool4x3 ||
		baseType == HLSLBaseType_Bool4x4 ||

		baseType == HLSLBaseType_Int2x2 ||
		baseType == HLSLBaseType_Int2x3 ||
		baseType == HLSLBaseType_Int2x4 ||
		baseType == HLSLBaseType_Int3x2 ||
		baseType == HLSLBaseType_Int3x3 ||
		baseType == HLSLBaseType_Int3x4 ||
		baseType == HLSLBaseType_Int4x2 ||
		baseType == HLSLBaseType_Int4x3 ||
		baseType == HLSLBaseType_Int4x4 ||

		baseType == HLSLBaseType_Uint2x2 ||
		baseType == HLSLBaseType_Uint2x3 ||
		baseType == HLSLBaseType_Uint2x4 ||
		baseType == HLSLBaseType_Uint3x2 ||
		baseType == HLSLBaseType_Uint3x3 ||
		baseType == HLSLBaseType_Uint3x4 ||
		baseType == HLSLBaseType_Uint4x2 ||
		baseType == HLSLBaseType_Uint4x3 ||
		baseType == HLSLBaseType_Uint4x4;
}

inline bool isScalarType( HLSLBaseType baseType )
{
	return  baseType == HLSLBaseType_Float ||
			baseType == HLSLBaseType_Half ||
			baseType == HLSLBaseType_Bool ||
			baseType == HLSLBaseType_Int ||
			baseType == HLSLBaseType_Uint;
}

inline bool isVectorType( HLSLBaseType baseType )
{
	return  baseType == HLSLBaseType_Float2 ||
		baseType == HLSLBaseType_Float3 ||
		baseType == HLSLBaseType_Float4 ||
		baseType == HLSLBaseType_Half2 ||
		baseType == HLSLBaseType_Half3 ||
		baseType == HLSLBaseType_Half4 ||
		baseType == HLSLBaseType_Bool2 ||
		baseType == HLSLBaseType_Bool3 ||
		baseType == HLSLBaseType_Bool4 ||
		baseType == HLSLBaseType_Int2  ||
		baseType == HLSLBaseType_Int3  ||
		baseType == HLSLBaseType_Int4  ||
		baseType == HLSLBaseType_Uint2 ||
		baseType == HLSLBaseType_Uint3 ||
		baseType == HLSLBaseType_Uint4;
}

//
inline HLSLBaseType GetScalarBaseType(const HLSLBaseType baseType)
{
	switch (baseType)
	{
	case HLSLBaseType_Void:
		ASSERT_PARSER(0);
		return HLSLBaseType_Void;
	case HLSLBaseType_Float:
	case HLSLBaseType_Float1x2:
	case HLSLBaseType_Float1x3:
	case HLSLBaseType_Float1x4:
	case HLSLBaseType_Float2:
	case HLSLBaseType_Float2x2:
	case HLSLBaseType_Float2x3:
	case HLSLBaseType_Float2x4:
	case HLSLBaseType_Float3:
	case HLSLBaseType_Float3x2:
	case HLSLBaseType_Float3x3:
	case HLSLBaseType_Float3x4:
	case HLSLBaseType_Float4:
	case HLSLBaseType_Float4x2:
	case HLSLBaseType_Float4x3:
	case HLSLBaseType_Float4x4:
		return HLSLBaseType_Float;

	case HLSLBaseType_Half:
	case HLSLBaseType_Half1x2:
	case HLSLBaseType_Half1x3:
	case HLSLBaseType_Half1x4:
	case HLSLBaseType_Half2:
	case HLSLBaseType_Half2x2:
	case HLSLBaseType_Half2x3:
	case HLSLBaseType_Half2x4:
	case HLSLBaseType_Half3:
	case HLSLBaseType_Half3x2:
	case HLSLBaseType_Half3x3:
	case HLSLBaseType_Half3x4:
	case HLSLBaseType_Half4:
	case HLSLBaseType_Half4x2:
	case HLSLBaseType_Half4x3:
	case HLSLBaseType_Half4x4:
		return HLSLBaseType_Half;

	case HLSLBaseType_Min16Float:
	case HLSLBaseType_Min16Float1x2:
	case HLSLBaseType_Min16Float1x3:
	case HLSLBaseType_Min16Float1x4:
	case HLSLBaseType_Min16Float2:
	case HLSLBaseType_Min16Float2x2:
	case HLSLBaseType_Min16Float2x3:
	case HLSLBaseType_Min16Float2x4:
	case HLSLBaseType_Min16Float3:
	case HLSLBaseType_Min16Float3x2:
	case HLSLBaseType_Min16Float3x3:
	case HLSLBaseType_Min16Float3x4:
	case HLSLBaseType_Min16Float4:
	case HLSLBaseType_Min16Float4x2:
	case HLSLBaseType_Min16Float4x3:
	case HLSLBaseType_Min16Float4x4:
		return HLSLBaseType_Min16Float;

	case HLSLBaseType_Min10Float:
	case HLSLBaseType_Min10Float1x2:
	case HLSLBaseType_Min10Float1x3:
	case HLSLBaseType_Min10Float1x4:
	case HLSLBaseType_Min10Float2:
	case HLSLBaseType_Min10Float2x2:
	case HLSLBaseType_Min10Float2x3:
	case HLSLBaseType_Min10Float2x4:
	case HLSLBaseType_Min10Float3:
	case HLSLBaseType_Min10Float3x2:
	case HLSLBaseType_Min10Float3x3:
	case HLSLBaseType_Min10Float3x4:
	case HLSLBaseType_Min10Float4:
	case HLSLBaseType_Min10Float4x2:
	case HLSLBaseType_Min10Float4x3:
	case HLSLBaseType_Min10Float4x4:
		return HLSLBaseType_Min10Float;

	case HLSLBaseType_Bool:
	case HLSLBaseType_Bool1x2:
	case HLSLBaseType_Bool1x3:
	case HLSLBaseType_Bool1x4:
	case HLSLBaseType_Bool2:
	case HLSLBaseType_Bool2x2:
	case HLSLBaseType_Bool2x3:
	case HLSLBaseType_Bool2x4:
	case HLSLBaseType_Bool3:
	case HLSLBaseType_Bool3x2:
	case HLSLBaseType_Bool3x3:
	case HLSLBaseType_Bool3x4:
	case HLSLBaseType_Bool4:
	case HLSLBaseType_Bool4x2:
	case HLSLBaseType_Bool4x3:
	case HLSLBaseType_Bool4x4:
		return HLSLBaseType_Bool;

	case HLSLBaseType_Int:
	case HLSLBaseType_Int1x2:
	case HLSLBaseType_Int1x3:
	case HLSLBaseType_Int1x4:
	case HLSLBaseType_Int2:
	case HLSLBaseType_Int2x2:
	case HLSLBaseType_Int2x3:
	case HLSLBaseType_Int2x4:
	case HLSLBaseType_Int3:
	case HLSLBaseType_Int3x2:
	case HLSLBaseType_Int3x3:
	case HLSLBaseType_Int3x4:
	case HLSLBaseType_Int4:
	case HLSLBaseType_Int4x2:
	case HLSLBaseType_Int4x3:
	case HLSLBaseType_Int4x4:
		return HLSLBaseType_Int;

	case HLSLBaseType_Uint:
	case HLSLBaseType_Uint1x2:
	case HLSLBaseType_Uint1x3:
	case HLSLBaseType_Uint1x4:
	case HLSLBaseType_Uint2:
	case HLSLBaseType_Uint2x2:
	case HLSLBaseType_Uint2x3:
	case HLSLBaseType_Uint2x4:
	case HLSLBaseType_Uint3:
	case HLSLBaseType_Uint3x2:
	case HLSLBaseType_Uint3x3:
	case HLSLBaseType_Uint3x4:
	case HLSLBaseType_Uint4:
	case HLSLBaseType_Uint4x2:
	case HLSLBaseType_Uint4x3:
	case HLSLBaseType_Uint4x4:
		return HLSLBaseType_Uint;

	case HLSLBaseType_InputPatch:
	case HLSLBaseType_OutputPatch:

	case HLSLBaseType_TriangleStream:

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

	case HLSLBaseType_RWTexture1D:
	case HLSLBaseType_RWTexture1DArray:
	case HLSLBaseType_RWTexture2D:
	case HLSLBaseType_RWTexture2DArray:
	case HLSLBaseType_RWTexture3D:

	case HLSLBaseType_Sampler:
	case HLSLBaseType_Sampler2D:
	case HLSLBaseType_Sampler3D:
	case HLSLBaseType_SamplerCube:
	case HLSLBaseType_Sampler2DMS:
	case HLSLBaseType_Sampler2DArray:
	case HLSLBaseType_SamplerState:
	case HLSLBaseType_SamplerComparisonState:
	case HLSLBaseType_UserDefined:
	default:
		return HLSLBaseType_Void;
	}

	return HLSLBaseType_Void;
}


enum HLSLBinaryOp
{
	//The ordering must fit with _binaryOpPriority 
    HLSLBinaryOp_And,
    HLSLBinaryOp_Or,
    HLSLBinaryOp_Add,
    HLSLBinaryOp_Sub,
    HLSLBinaryOp_Mul,
    HLSLBinaryOp_Div,
    HLSLBinaryOp_Less,
    HLSLBinaryOp_Greater,
    HLSLBinaryOp_LessEqual,
    HLSLBinaryOp_GreaterEqual,
    HLSLBinaryOp_Equal,
    HLSLBinaryOp_NotEqual,
    HLSLBinaryOp_BitAnd,
    HLSLBinaryOp_BitOr,
    HLSLBinaryOp_BitXor,
	HLSLBinaryOp_LeftShift,
	HLSLBinaryOp_RightShift,
	HLSLBinaryOp_Modular,

    HLSLBinaryOp_Assign,
    HLSLBinaryOp_AddAssign,
    HLSLBinaryOp_SubAssign,
    HLSLBinaryOp_MulAssign,
    HLSLBinaryOp_DivAssign,

	HLSLBinaryOp_BitAndAssign,
	HLSLBinaryOp_BitOrAssign,
	HLSLBinaryOp_BitXorAssign,

	HLSLBinaryOp_Comma, // lowest priority, even below assign
};

inline bool isCompareOp( HLSLBinaryOp op )
{
	return op == HLSLBinaryOp_Less ||
		op == HLSLBinaryOp_Greater ||
		op == HLSLBinaryOp_LessEqual ||
		op == HLSLBinaryOp_GreaterEqual ||
		op == HLSLBinaryOp_Equal ||
		op == HLSLBinaryOp_NotEqual;
}

enum HLSLUnaryOp
{
    HLSLUnaryOp_Negative,       // -x
    HLSLUnaryOp_Positive,       // +x
    HLSLUnaryOp_Not,            // !x
    HLSLUnaryOp_PreIncrement,   // ++x
    HLSLUnaryOp_PreDecrement,   // --x
    HLSLUnaryOp_PostIncrement,  // x++
    HLSLUnaryOp_PostDecrement,  // x++
    HLSLUnaryOp_BitNot,         // ~x
};

enum HLSLArgumentModifier
{
    HLSLArgumentModifier_None,
    HLSLArgumentModifier_In,
    HLSLArgumentModifier_Out,
    HLSLArgumentModifier_Inout,
    HLSLArgumentModifier_Uniform,
    HLSLArgumentModifier_Const,

	
	HLSLArgumentModifier_Point,
	HLSLArgumentModifier_Line,
	HLSLArgumentModifier_Triangle,
	HLSLArgumentModifier_Lineadj,
	HLSLArgumentModifier_Triangleadj,

};

enum HLSLTypeFlags
{
    HLSLTypeFlag_None = 0,
    HLSLTypeFlag_Const = 0x01,
    HLSLTypeFlag_Static = 0x02,

	//HLSLTypeFlag_Uniform = 0x04,
    //HLSLTypeFlag_Extern = 0x10,
    //HLSLTypeFlag_Volatile = 0x20,
    //HLSLTypeFlag_Shared = 0x40,
    //HLSLTypeFlag_Precise = 0x80,

    HLSLTypeFlag_Input = 0x100,
    HLSLTypeFlag_Output = 0x200,

    // Interpolation modifiers.
    HLSLTypeFlag_Linear = 0x10000,
    HLSLTypeFlag_Centroid = 0x20000,
    HLSLTypeFlag_NoInterpolation = 0x40000,
    HLSLTypeFlag_NoPerspective = 0x80000,
    HLSLTypeFlag_Sample = 0x100000,

    // Misc.
    //HLSLTypeFlag_Swizzle_BGRA = 0x200000,
};

enum HLSLAttributeType
{
    HLSLAttributeType_Unknown,
    HLSLAttributeType_Unroll,
    HLSLAttributeType_Branch,
    HLSLAttributeType_Flatten,
	HLSLAttributeType_NumThreads,
	HLSLAttributeType_MaxVertexCount,

	HLSLAttributeType_Domain,
	HLSLAttributeType_Partitioning,
	HLSLAttributeType_OutputTopology,
	HLSLAttributeType_OutputControlPoints,
	HLSLAttributeType_PatchConstantFunc,
	HLSLAttributeType_MaxtessFactor,

	HLSLAttributeType_EarlyDepthStencil,
	
};

enum NumericType
{
	NumericType_Float,
	NumericType_Half,
	NumericType_Min16Float,
	NumericType_Min10Float,
	NumericType_Bool,
	NumericType_Int,
	NumericType_Uint,
	NumericType_Count,
	NumericType_NaN,
};

struct BaseTypeDescription
{
	const char*     typeName;
	NumericType     numericType;
	int             numComponents;
	int             numRows;
	int             binaryOpRank;
};

extern const BaseTypeDescription BASE_TYPE_DESC[HLSLBaseType_Count];

enum HLSLAddressSpace
{
    HLSLAddressSpace_Undefined,
    HLSLAddressSpace_Constant,
    HLSLAddressSpace_Device,
    HLSLAddressSpace_Thread,
    HLSLAddressSpace_Shared,
};

#define MAX_GLOBAL_EXTENSION 128

enum GlobalExtension
{
	USE_SAMPLESS,
	USE_ATOMIC,
	USE_INCLUDE,
	USE_3X3_CONVERSION,
	USE_NonUniformResourceIndex,
	USE_Subgroup_Basic,
	USE_Subgroup_Arithmetic,
	USE_Subgroup_Ballot,
	USE_Subgroup_Quad,
	USE_WaveGetLaneIndex,
	USE_WaveGetLaneCount,
	USE_ControlFlowAttributes

};

struct HLSLNode;
struct HLSLRoot;
struct HLSLStatement;
struct HLSLAttribute;
struct HLSLDeclaration;
struct HLSLStruct;
struct HLSLStructField;
struct HLSLBuffer;
struct HLSLFunction;
struct HLSLArgument;
struct HLSLExpressionStatement;
struct HLSLExpression;
struct HLSLInitListExpression;
struct HLSLBinaryExpression;
struct HLSLLiteralExpression;
struct HLSLIdentifierExpression;
struct HLSLConstructorExpression;
struct HLSLFunctionCall;
struct HLSLArrayAccess;
struct HLSLAttribute;
struct HLSLStateAssignment;
struct HLSLMemberAccess;

struct HLSLType
{
    explicit HLSLType(HLSLBaseType _baseType = HLSLBaseType_Unknown)
    { 
        baseType    = _baseType;
        array       = false;
        flags       = 0;
        addressSpace = HLSLAddressSpace_Undefined;

		arrayExtent[0] = 0;
		arrayExtent[1] = 0;
		arrayExtent[2] = 0;

		arrayDimension = 0;

		maxPoints = -1;

		InputPatchName.Reset();
		OutputPatchName.Reset();

		sampleCount = 0;

		elementType = HLSLBaseType_Unknown;
    }
    HLSLBaseType        baseType;
	HLSLBaseType        elementType;

    CachedString        typeName;       // For user defined types.

    bool                array;
	int					arrayDimension;
	int					arrayExtent[MAX_DIM];

	int                 flags;
    HLSLAddressSpace    addressSpace;

	CachedString		structuredTypeName;

	CachedString		InputPatchName;
	CachedString		OutputPatchName;
	int					maxPoints; // for hull shader

	CachedString		textureTypeName;
	int					sampleCount;
};

inline bool IsSamplerType(const HLSLType & type)
{
    return !type.array && IsSamplerType(type.baseType);
}

inline bool isScalarType(const HLSLType & type)
{
	return !type.array && isScalarType(type.baseType);
}

inline bool isVectorType(const HLSLType & type)
{
	return !type.array && isVectorType(type.baseType);
}

inline bool IsTexture(const HLSLType& type)
{
	switch (type.baseType)
	{
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
		case HLSLBaseType_RWTexture1D:
		case HLSLBaseType_RWTexture1DArray:
		case HLSLBaseType_RWTexture2D:
		case HLSLBaseType_RWTexture2DArray:
		case HLSLBaseType_RWTexture3D:
		case HLSLBaseType_RasterizerOrderedTexture1D:
		case HLSLBaseType_RasterizerOrderedTexture1DArray:
		case HLSLBaseType_RasterizerOrderedTexture2D:
		case HLSLBaseType_RasterizerOrderedTexture2DArray:
		case HLSLBaseType_RasterizerOrderedTexture3D:
			return !type.array;
		default:
			return false;
	}			
}

inline bool IsRWTexture(HLSLBaseType type)
{
	switch (type)
	{
		case HLSLBaseType_RWTexture1D:
		case HLSLBaseType_RWTexture1DArray:
		case HLSLBaseType_RWTexture2D:
		case HLSLBaseType_RWTexture2DArray:
		case HLSLBaseType_RWTexture3D:
		case HLSLBaseType_RasterizerOrderedTexture1D:
		case HLSLBaseType_RasterizerOrderedTexture1DArray:
		case HLSLBaseType_RasterizerOrderedTexture2D:
		case HLSLBaseType_RasterizerOrderedTexture2DArray:
		case HLSLBaseType_RasterizerOrderedTexture3D:
			return true;
		default:
			return false;
	}			
}

inline bool IsBuffer(HLSLBaseType type)
{
	switch (type)
	{
		case HLSLBaseType_StructuredBuffer:
		case HLSLBaseType_PureBuffer:
		case HLSLBaseType_RWBuffer:
		case HLSLBaseType_RWStructuredBuffer:
		case HLSLBaseType_ByteAddressBuffer:
		case HLSLBaseType_RWByteAddressBuffer:
			return true;
		default:
			return false;
	}			
}

inline bool IsStructuredBuffer(HLSLBaseType type)
{
	switch (type)
	{
		case HLSLBaseType_StructuredBuffer:
		case HLSLBaseType_PureBuffer:
		case HLSLBaseType_RWBuffer:
		case HLSLBaseType_RWStructuredBuffer:
		case HLSLBaseType_RasterizerOrderedBuffer:
		case HLSLBaseType_RasterizerOrderedStructuredBuffer:
			return true;
		default:
			return false;
	}			
}

inline bool IsRWBuffer(HLSLBaseType type)
{
	switch (type)
	{
		case HLSLBaseType_RWBuffer:
		case HLSLBaseType_RWStructuredBuffer:
		case HLSLBaseType_RWByteAddressBuffer:
			return true;
		default:
			return false;
	}			
}

inline bool IsRasterizerOrderedTexture(HLSLBaseType type)
{
	switch (type)
	{
	case HLSLBaseType_RasterizerOrderedTexture1D:
	case HLSLBaseType_RasterizerOrderedTexture1DArray:
	case HLSLBaseType_RasterizerOrderedTexture2D:
	case HLSLBaseType_RasterizerOrderedTexture2DArray:
	case HLSLBaseType_RasterizerOrderedTexture3D:
		return true;
	default:
		return false;
	}
}

inline bool IsTexture(HLSLBaseType type)
{
	switch (type)
	{
	case HLSLBaseType_Texture1D:
	case HLSLBaseType_Texture1DArray:
	case HLSLBaseType_Texture2D:
	case HLSLBaseType_Texture2DArray:
	case HLSLBaseType_Texture3D:
	case HLSLBaseType_Texture2DMS:
	case HLSLBaseType_Texture2DMSArray:
	case HLSLBaseType_TextureCube:
	case HLSLBaseType_TextureCubeArray:
		return true;
	default:
		return false;
	}
}

/** Base class for all nodes in the HLSL AST */
struct HLSLNode
{
    HLSLNodeType nodeType;
    CachedString fileName;
    int          line;

	HLSLNode()
	{
		nodeType = HLSLNodeType_Unknown;
	}

	virtual ~HLSLNode()
	{
		// adding virtual destructor so that we can properly delete inherited types
	}
};

struct HLSLRoot : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Root;
    HLSLRoot()          { statement = NULL; }
    HLSLStatement*      statement;          // First statement.
};

struct HLSLStatement : public HLSLNode
{
    HLSLStatement() 
    { 
        nextStatement   = NULL; 
        attributes      = NULL;
        hidden          = false;
	}

	~HLSLStatement()
	{
	}

    HLSLStatement* nextStatement;      // Next statement in the block.
    HLSLAttribute* attributes;
    mutable bool   hidden;
};

struct HLSLAttribute : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Attribute;
	HLSLAttribute()
	{
		attributeType = HLSLAttributeType_Unknown;
		argument      = NULL;
		nextAttribute = NULL;

		numGroupX = 0;
		numGroupY = 0;
		numGroupZ = 0;

		maxVertexCount = 0;

		unrollCount = 0;

		domain.Reset();
		partitioning.Reset();
		outputtopology.Reset();

		outputcontrolpoints = 0;

		patchconstantfunc.Reset();

		maxTessellationFactor = 0.0;

		earlyDepthStencil = false;

	}
    HLSLAttributeType   attributeType;
    HLSLExpression*     argument;
    HLSLAttribute*      nextAttribute;

	unsigned int		numGroupX;
	unsigned int		numGroupY;
	unsigned int		numGroupZ;	

	CachedString		numGroupXstr;
	CachedString		numGroupYstr;
	CachedString		numGroupZstr;

	unsigned int		maxVertexCount;

	unsigned int		unrollCount;
	CachedString		unrollIdentifier;

	CachedString		domain;
	CachedString		partitioning;
	CachedString		outputtopology;

	unsigned int		outputcontrolpoints;

	CachedString		patchconstantfunc;

	float				maxTessellationFactor;

	CachedString		patchIdentifier;

	bool				earlyDepthStencil;

};

struct HLSLDeclaration : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_Declaration;
	HLSLDeclaration()
	{
		nextDeclaration = NULL;
		buffer          = NULL;

		arrayDimExpression[0] = NULL;
		arrayDimExpression[1] = NULL;
		arrayDimExpression[2] = NULL;

		assignment = NULL;

		registerType = 0;
		registerIndex = -1;
		registerSpace = -1;

		global = false;
	}

	HLSLDeclaration*    nextDeclaration;    // If multiple variables declared on a line.

	HLSLBuffer*         buffer;
	CachedString        name;

	HLSLExpression*     arrayDimExpression[MAX_DIM];
	HLSLExpression*     assignment;

	CachedString        semantic;
	CachedString        sv_semantic;

	//TODO: remove register strings
	CachedString        registerName;
	CachedString        registerSpaceName;

	HLSLType	type;

	int registerIndex;
	int registerSpace;
	char registerType;

	//TODO:????
	bool global;

};

struct HLSLStruct : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Struct;
    HLSLStruct()
    {
        field           = NULL;
    }
	CachedString         name;
    HLSLStructField*    field;              // First field in the structure.
};

struct HLSLStructField : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_StructField;
    HLSLStructField()
    {
        nextField       = NULL;
        hidden          = false;
		atomic			= false;
    }
	CachedString        name;
    HLSLType            type;
	CachedString        semantic;
	CachedString        sv_semantic;
    HLSLStructField*    nextField;      // Next field in the structure.
    bool                hidden;
	HLSLExpression*		arrayDimExpression[MAX_DIM];

	bool				atomic;
};


struct HLSLGroupShared : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_GroupShared;
	HLSLGroupShared()
	{
		declaration = NULL;
	}

	CachedString         name;
	HLSLDeclaration*		declaration;
};

struct HLSLSamplerState : public HLSLDeclaration
{
	static const HLSLNodeType s_type = HLSLNodeType_SamplerState;
	HLSLSamplerState()
	{
		numStateAssignments = 0;
		stateAssignments = NULL;
	}
	
	int								numStateAssignments;
	HLSLStateAssignment*			stateAssignments;
};

struct HLSLTextureState : public HLSLDeclaration
{
	static const HLSLNodeType s_type = HLSLNodeType_TextureState;
	HLSLTextureState()
	{
		sampleCount = 0;
	}
	
	unsigned int		sampleCount;
	CachedString		sampleIdentifier;
};

struct HLSLBuffer : public HLSLDeclaration
{
    static const HLSLNodeType s_type = HLSLNodeType_Buffer;
    HLSLBuffer()
    {       
        field           = NULL;

		bAtomic		  = false;
		bPushConstant = false;
    }

    HLSLDeclaration*    field;

	bool				bAtomic;
	bool				bPushConstant;
};

/** Declaration of an argument to a function. */
struct HLSLArgument : public HLSLDeclaration
{
	static const HLSLNodeType s_type = HLSLNodeType_Argument;
	HLSLArgument()
	{
		modifier = HLSLArgumentModifier_None;
		defaultValue = NULL;
		hidden = false;
		type = HLSLType();
	}
	HLSLArgumentModifier    modifier;
	HLSLExpression*         defaultValue;
	bool                    hidden;
};

/** Function declaration */
struct HLSLFunction : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_Function;
	HLSLFunction()
	{
		statement       = NULL;
		forward         = NULL;
		bPatchconstantfunc = false;
	}

	~HLSLFunction()
	{
	}

	CachedString name;
	HLSLType     returnType;
	CachedString semantic;
	CachedString sv_semantic;

	eastl::vector<HLSLArgument*> args;
	HLSLStatement*              statement;
	HLSLFunction*               forward; // Which HLSLFunction this one forward-declares

	bool         bPatchconstantfunc;
	CachedString macroFunctionBody;
};

/** A expression which forms a complete statement. */
struct HLSLExpressionStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ExpressionStatement;
    HLSLExpressionStatement()
    {
        expression = NULL;
    }
    HLSLExpression*     expression;
};

struct HLSLReturnStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ReturnStatement;
    HLSLReturnStatement()
    {
        expression = NULL;
    }
    HLSLExpression*     expression;
};

struct HLSLDiscardStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_DiscardStatement;
};

struct HLSLBreakStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_BreakStatement;
};

struct HLSLContinueStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ContinueStatement;
};

struct HLSLIfStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_IfStatement;
    HLSLIfStatement()
    {
        condition     = NULL;
        statement     = NULL;
        elseStatement = NULL;

		elseifStatement.clear();
    }
    HLSLExpression*     condition;
    HLSLStatement*      statement;
    HLSLStatement*      elseStatement;
	eastl::vector < HLSLIfStatement* >      elseifStatement;
};

struct HLSLSwitchStatement : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_SwitchStatement;
	HLSLSwitchStatement()
	{		
		condition = NULL;
		
		caseNumber.clear();
		caseStatement.clear();
		caseDefault = NULL;
		caseDefaultIndex = 0;
	}
	
	HLSLExpression*     condition;

	eastl::vector < HLSLExpression* > caseNumber;
	eastl::vector < HLSLStatement* > caseStatement;
	HLSLStatement*		caseDefault;

	// this value is the size of the caseNumber/caseStatement number when the default is added.
	int caseDefaultIndex;
};

struct HLSLForStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_ForStatement;
    HLSLForStatement()
    {
        initialization = NULL;
		initializationWithoutDeclaration = NULL;
        condition = NULL;
        increment = NULL;
        statement = NULL;
    }
    HLSLDeclaration*    initialization;
	HLSLExpression*     initializationWithoutDeclaration;
    HLSLExpression*     condition;
    HLSLExpression*     increment;
    HLSLStatement*      statement;
};

struct HLSLWhileStatement : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_WhileStatement;
	HLSLWhileStatement()
	{
		condition = NULL;
		statement = NULL;
	}
	HLSLExpression*     condition;
	HLSLStatement*      statement;
};

struct HLSLBlockStatement : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_BlockStatement;
    HLSLBlockStatement()
    {
        statement = NULL;
    }
    HLSLStatement*      statement;
};

/** Base type for all types of expressions. */
struct HLSLExpression : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Expression;
    HLSLExpression()
    {
		functionExpression = NULL;
    }

    HLSLType            expressionType;
	HLSLExpression*		functionExpression; // this is for function call from texture / buffer type with '.'
};

struct HLSLInitListExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_InitListExpression;
	HLSLInitListExpression() {}

	eastl::vector<HLSLExpression*> initExpressions;
};

struct HLSLUnaryExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_UnaryExpression;
    HLSLUnaryExpression()
    {
        expression = NULL;
    }
    HLSLUnaryOp         unaryOp;
    HLSLExpression*     expression;
};

struct HLSLBinaryExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_BinaryExpression;
    HLSLBinaryExpression()
    {
        expression1 = NULL;
        expression2 = NULL;
    }
    HLSLBinaryOp        binaryOp;
    HLSLExpression*     expression1;
    HLSLExpression*     expression2;
};

/** ? : construct */
struct HLSLConditionalExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_ConditionalExpression;
    HLSLConditionalExpression()
    {
        condition       = NULL;
        trueExpression  = NULL;
        falseExpression = NULL;
    }
    HLSLExpression*     condition;
    HLSLExpression*     trueExpression;
    HLSLExpression*     falseExpression;
};

struct HLSLCastingExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_CastingExpression;

	HLSLExpression* expression = NULL;
	bool            implicit = false;
};

/** Float, integer, boolean, etc. literal constant. */
struct HLSLLiteralExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_LiteralExpression;
    HLSLBaseType        type;   // Note, not all types can be literals.
    union
    {
        bool            bValue;
        float           fValue;
        int             iValue;
		unsigned int    uiValue;
    };
};

/** An identifier, typically a variable name or structure field name. */
struct HLSLIdentifierExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_IdentifierExpression;
	HLSLIdentifierExpression()
	{
		pDeclaration = NULL;
	}

	HLSLDeclaration* pDeclaration;
};

/** float2(1, 2) */
struct HLSLConstructorExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_ConstructorExpression;
	HLSLConstructorExpression() {}
	eastl::vector<HLSLExpression*> params;
};

/** object.member **/
struct HLSLMemberAccess : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_MemberAccess;
	HLSLMemberAccess()
	{
		object  = NULL;
		swizzle = false;
	}
    HLSLExpression*     object;
	CachedString        field;
    bool                swizzle;
};

/** array[index] **/
struct HLSLArrayAccess : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_ArrayAccess;
	HLSLArrayAccess()
	{
		array = NULL;
		index = NULL;
	}
    HLSLExpression*     array;
    HLSLExpression*     index;

	//TODO: remove
	CachedString         identifier;
};

struct HLSLFunctionCall : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_FunctionCall;
	HLSLFunctionCall()
	{
		function = NULL;
	}

	const HLSLFunction*            function;
	eastl::vector<HLSLExpression*> params;
};

struct HLSLStateAssignment : public HLSLNode
{
	static const HLSLNodeType s_type = HLSLNodeType_StateAssignment;
	HLSLStateAssignment()
	{
		iValue = 0;
		nextStateAssignment = NULL;
	}

	CachedString             stateName;
    int                     d3dRenderState;
    union {
		unsigned int		uiValue;
        int                 iValue;
        float               fValue;
    };

	CachedString        strValue;

	HLSLStateAssignment*    nextStateAssignment;
};

struct HLSLPass : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Pass;
    HLSLPass()
    {
        numStateAssignments = 0;
        stateAssignments = NULL;
        nextPass = NULL;
    }
    
	CachedString             name;
    int                     numStateAssignments;
    HLSLStateAssignment*    stateAssignments;
    HLSLPass*               nextPass;
};

struct HLSLTechnique : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Technique;
    HLSLTechnique()
    {
        numPasses = 0;
        passes = NULL;
    }

	CachedString         name;
    int                 numPasses;
    HLSLPass*           passes;
};

struct HLSLPipeline : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Pipeline;
    HLSLPipeline()
    {
        numStateAssignments = 0;
        stateAssignments = NULL;
    }
    
	CachedString             name;
    int                     numStateAssignments;
    HLSLStateAssignment*    stateAssignments;
};

struct HLSLStage : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Stage;
    HLSLStage()
    {
        statement = NULL;
        inputs = NULL;
        outputs = NULL;
    }

	CachedString             name;
    HLSLStatement*          statement;
    HLSLDeclaration*        inputs;
    HLSLDeclaration*        outputs;
};

/**
 * Abstract syntax tree for parsed HLSL code.
 */
class HLSLTree
{

public:

    explicit HLSLTree(StringLibrary * stringLibrary);
    ~HLSLTree();

    /** Adds a string to the string pool used by the tree. */
	CachedString AddStringCached(const char* string);
	CachedString AddStringFormatCached(const char* string, ...);
	
	/** Returns true if the string is contained within the tree. */
    bool GetContainsString(const char* string) const;

    /** Returns the root block in the tree */
    HLSLRoot* GetRoot() const;

    /** Adds a new node to the tree with the specified type. */
    template <class T>
    T* AddNode(const char* fileName, int line)
    {
        T* node = new T();
        node->nodeType  = T::s_type;
        node->fileName  = AddStringCached(fileName);
        node->line      = line;
		m_allNodes.push_back(node);
        return static_cast<T*>(node);
    }

	template <class T>
	T* AddNode(const CachedString & fileName, int line)
	{
		T* node = new T();
		node->nodeType = T::s_type;
		node->fileName = fileName;
		node->line = line;
		m_allNodes.push_back(node);
		return static_cast<T*>(node);
	}
	
	HLSLFunction * FindFunction(const CachedString & name, int index = 0);
    HLSLDeclaration * FindGlobalDeclaration(const CachedString & name, HLSLBuffer ** buffer_out = NULL);
	HLSLStruct * FindGlobalStruct(const CachedString & name);
	CachedString FindGlobalStructMember(const CachedString & memberName);
    HLSLTechnique * FindTechnique(const CachedString & name);
    HLSLPipeline * FindFirstPipeline();
    HLSLPipeline * FindNextPipeline(HLSLPipeline * current);
    HLSLPipeline * FindPipeline(const CachedString & name);
    HLSLBuffer * FindBuffer(const CachedString & name);

	void FindMatrixMultiplyTypes(
		eastl::vector < HLSLBaseType > & lhsTypeVec,
		eastl::vector < HLSLBaseType > & rhsTypeVec) const;

	void FindTextureLoadOverloads(
		eastl::vector < HLSLBaseType > & textureType,
		eastl::vector < HLSLBaseType > & paramType) const;

	
	bool GetExpressionValue(HLSLExpression * expression, int & value);
    int GetExpressionValue(HLSLExpression * expression, float values[4]);

    bool NeedsFunction(const CachedString & name);

	bool NeedsExtension(GlobalExtension ext)
	{
		return gExtension[ext];
	}

	bool gExtension[MAX_GLOBAL_EXTENSION];

	static bool IsCustomMultiply(HLSLBaseType lhs, HLSLBaseType rhs);

private:
    HLSLRoot*       m_root;

	eastl::vector < HLSLNode * > m_allNodes;

public:
	StringLibrary * m_stringLibrary;
};

class HLSLTreeVisitor
{
public:
    virtual void VisitType(HLSLType & type);

    virtual void VisitRoot(HLSLRoot * node);
    virtual void VisitTopLevelStatement(HLSLStatement * node);
    virtual void VisitStatements(HLSLStatement * statement);
    virtual void VisitStatement(HLSLStatement * node);
    virtual void VisitDeclaration(HLSLDeclaration * node);
    virtual void VisitStruct(HLSLStruct * node);
    virtual void VisitStructField(HLSLStructField * node);
    virtual void VisitBuffer(HLSLBuffer * node);
    virtual void VisitFunction(HLSLFunction * node);
    virtual void VisitArgument(HLSLArgument * node);
    virtual void VisitExpressionStatement(HLSLExpressionStatement * node);
    virtual void VisitExpression(HLSLExpression * node);
	virtual void VisitInitListExpression(HLSLInitListExpression * node);
    virtual void VisitReturnStatement(HLSLReturnStatement * node);
    virtual void VisitDiscardStatement(HLSLDiscardStatement * node);
    virtual void VisitBreakStatement(HLSLBreakStatement * node);
    virtual void VisitContinueStatement(HLSLContinueStatement * node);
    virtual void VisitIfStatement(HLSLIfStatement * node);
    virtual void VisitForStatement(HLSLForStatement * node);
    virtual void VisitBlockStatement(HLSLBlockStatement * node);
    virtual void VisitUnaryExpression(HLSLUnaryExpression * node);
    virtual void VisitBinaryExpression(HLSLBinaryExpression * node);
    virtual void VisitConditionalExpression(HLSLConditionalExpression * node);
    virtual void VisitCastingExpression(HLSLCastingExpression * node);
    virtual void VisitLiteralExpression(HLSLLiteralExpression * node);
    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node);
    virtual void VisitConstructorExpression(HLSLConstructorExpression * node);
    virtual void VisitMemberAccess(HLSLMemberAccess * node);
    virtual void VisitArrayAccess(HLSLArrayAccess * node);
    virtual void VisitFunctionCall(HLSLFunctionCall * node);
    virtual void VisitStateAssignment(HLSLStateAssignment * node);
	virtual void VisitSamplerState(HLSLSamplerState * node);
	virtual void VisitTextureState(HLSLTextureState * node);
	virtual void VisitPass(HLSLPass * node);
    virtual void VisitTechnique(HLSLTechnique * node);

	virtual void VisitSamplerIdentifier(const CachedString & name);
	virtual void VisitTextureIdentifier(const CachedString & name);

	virtual void VisitFunctions(HLSLRoot * root);
    virtual void VisitParameters(HLSLRoot * root);
};

// Tree transformations:
extern void PruneTree(HLSLTree* tree, const char* entryName0, const char* entryName1 = NULL);
extern void SortTree(HLSLTree* tree);
extern void HideUnusedArguments(HLSLFunction * function);
extern bool EmulateAlphaTest(HLSLTree* tree, const char* entryName, float alphaRef = 0.5f);

extern const char* getElementTypeAsStr(StringLibrary * stringLibrary, HLSLType type);
extern eastl::string getElementTypeAsStrGLSL(StringLibrary * stringLibrary, HLSLType type);

#endif
