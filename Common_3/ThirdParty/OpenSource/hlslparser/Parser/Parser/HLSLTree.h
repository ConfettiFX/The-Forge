#ifndef HLSL_TREE_H
#define HLSL_TREE_H

//#include "Engine/StringPool.h"
#include "Engine.h"

#include <new>

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
	HLSLNodeType_SamplerStateExpression,
    HLSLNodeType_Pass,
    HLSLNodeType_Technique,
    HLSLNodeType_Attribute,
    HLSLNodeType_Pipeline,
    HLSLNodeType_Stage,
	HLSLNodeType_Preprocessor,
	HLSLNodeType_PreprocessorExpression,
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

	HLSLBaseType_PreProcessorDefine,
	HLSLBaseType_PreProcessorIf,
	HLSLBaseType_PreProcessorElif,
	HLSLBaseType_PreProcessorElse,
	HLSLBaseType_PreProcessorEndif,
	HLSLBaseType_PreProcessorIfDef,
	HLSLBaseType_PreProcessorIfnDef,
	HLSLBaseType_PreProcessorUndef,
	HLSLBaseType_PreProcessorInclude,
	HLSLBaseType_PreProcessorLine,
	HLSLBaseType_PreProcessorPragma,


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
struct HLSLBinaryExpression;
struct HLSLLiteralExpression;
struct HLSLIdentifierExpression;
struct HLSLConstructorExpression;
struct HLSLFunctionCall;
struct HLSLArrayAccess;
struct HLSLAttribute;
struct HLSLStateAssignment;
struct HLSLSamplerStateExpression;
struct HLSLpreprocessor;
struct HLSLPreprocessorExpression;
struct HLSLMemberAccess;

struct HLSLType
{
    explicit HLSLType(HLSLBaseType _baseType = HLSLBaseType_Unknown)
    { 
        baseType    = _baseType;
        typeName    = NULL;
        array       = false;
        arraySize   = NULL;
		arrayCount = 0;
        flags       = 0;
        addressSpace = HLSLAddressSpace_Undefined;

		//bStruct = false;
		structuredTypeName = NULL;
		maxPoints = -1;

		InputPatchName[0] = NULL;
		OutputPatchName[0] = NULL;

		sampleCount = 0;

		//typeIdentifierName = NULL;

		elementType = HLSLBaseType_Unknown;

		textureTypeName = NULL;
    }
    HLSLBaseType        baseType;
	HLSLBaseType        elementType;
	

    const char*         typeName;       // For user defined types.

	//const char*			typeIdentifierName;

    bool                array;
    HLSLExpression*     arraySize;
	int					arrayCount;
    int                 flags;
    HLSLAddressSpace    addressSpace;

	//bool                bStruct;
	const char*			structuredTypeName;


	

	char				InputPatchName[128];
	char				OutputPatchName[128];
	int					maxPoints; // for hull shader

	const char*			textureTypeName;
	int					sampleCount;
};

inline bool IsSamplerType(const HLSLType & type)
{
    return IsSamplerType(type.baseType);
}

inline bool isScalarType(const HLSLType & type)
{
	return isScalarType(type.baseType);
}

inline bool isVectorType(const HLSLType & type)
{
	return isVectorType(type.baseType);
}


/** Base class for all nodes in the HLSL AST */
struct HLSLNode
{
    HLSLNodeType        nodeType;
    const char*         fileName;
    int                 line;

	const char*			preprocessor;

	HLSLNode()
	{
		nodeType = HLSLNodeType_Unknown;
		fileName = NULL;

		preprocessor = NULL;
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

		name = NULL;
		registerName = NULL;
		registerSpaceName = NULL;

		arrayIdentifier[0][0] = 0;
		arrayIdentifier[1][0] = 0;
		arrayIdentifier[2][0] = 0;

		arrayIndex[0] = 0;
		arrayIndex[1] = 0;
		arrayIndex[2] = 0;

		arrayDimension = 0;

		bArray = false;		
    }

    HLSLStatement*      nextStatement;      // Next statement in the block.
    HLSLAttribute*      attributes;
    mutable bool        hidden;

	const char*         name;
	const char*         registerName;
	const char*			registerSpaceName;

	bool				bArray;
	
	// assuming that it can be multidimension up to 3-D
	char				arrayIdentifier[3][64];
	unsigned int		arrayIndex[3];
	unsigned int		arrayDimension;


	
	HLSLType			type;

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

		numGroupXstr = NULL;
		numGroupYstr = NULL;
		numGroupZstr = NULL;

		maxVertexCount = 0;

		unrollCount = 0;

		unrollIdentifier = NULL;

		domain[0] = NULL;
		partitioning[0] = NULL;
		outputtopology[0] = NULL;

		outputcontrolpoints = 0;

		patchconstantfunc[0] = NULL;

		maxTessellationFactor = 0.0;

		earlyDepthStencil = false;

	}
    HLSLAttributeType   attributeType;
    HLSLExpression*     argument;
    HLSLAttribute*      nextAttribute;

	unsigned int		numGroupX;
	unsigned int		numGroupY;
	unsigned int		numGroupZ;	

	const char*			numGroupXstr;
	const char*			numGroupYstr;
	const char*			numGroupZstr;

	unsigned int		maxVertexCount;

	unsigned int		unrollCount;
	const char*			unrollIdentifier;

	char				domain[16];
	char				partitioning[16];
	char				outputtopology[16];

	unsigned int		outputcontrolpoints;

	char				patchconstantfunc[128];

	float				maxTessellationFactor;

	char				patchIdentifier[128];

	bool				earlyDepthStencil;

};

struct HLSLDeclaration : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Declaration;
    HLSLDeclaration()
    {
        //name            = NULL;
        //registerName    = NULL;
        semantic        = NULL;
        nextDeclaration = NULL;
        assignment      = NULL;
        buffer          = NULL;
    }
    //const char*         name;
    HLSLType            type;
    //const char*         registerName;       // @@ Store register index?
    const char*         semantic;
    HLSLDeclaration*    nextDeclaration;    // If multiple variables declared on a line.
    HLSLExpression*     assignment;
    HLSLBuffer*         buffer;
};

struct HLSLStruct : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Struct;
    HLSLStruct()
    {
        name            = NULL;
        field           = NULL;
    }
    const char*         name;
    HLSLStructField*    field;              // First field in the structure.
};

struct HLSLStructField : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_StructField;
    HLSLStructField()
    {
        name            = NULL;
        semantic        = NULL;
        sv_semantic     = NULL;
        nextField       = NULL;
        hidden          = false;
		atomic			= false;
		preProcessor = NULL;

    }
    const char*         name;
    HLSLType            type;
    const char*         semantic;
    const char*         sv_semantic;
    HLSLStructField*    nextField;      // Next field in the structure.
    bool                hidden;

	HLSLpreprocessor*	preProcessor;
	bool				atomic;
};


struct HLSLGroupShared : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_GroupShared;
	HLSLGroupShared()
	{
		name = NULL;
		declaration = NULL;
	}

	const char*         name;
	HLSLDeclaration*		declaration;
};

struct HLSLSamplerState : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_SamplerState;
	HLSLSamplerState()
	{
		
		expression = NULL;
		numStateAssignments = 0;
		stateAssignments = NULL;
	
		bStructured = false;
		IsComparisionState = false;
	}
	
	HLSLSamplerStateExpression*		expression;              // First field in the sampler state
	int								numStateAssignments;
	HLSLStateAssignment*			stateAssignments;
	bool							bStructured;
	bool							IsComparisionState;
};

struct HLSLTextureState : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_TextureState;
	HLSLTextureState()
	{

		sampleCount = 0;
		sampleIdentifier = NULL;
	}
	
	//HLSLBaseType        baseType;
	//HLSLBaseType        dataType;	
	
	unsigned int		sampleCount;
	const char*			sampleIdentifier;

	
};

/*
struct HLSLRWTextureState : public HLSLTextureState
{
	static const HLSLNodeType s_type = HLSLNodeType_RWTextureState;
	HLSLRWTextureState()
	{
		HLSLTextureState();
	}
};
*/

struct HLSLBuffer : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Buffer;
    HLSLBuffer()
    {       
        field           = NULL;	
		//elementType		= NULL;
		//dataType		= HLSLBaseType_Unknown;

		bAtomic			= false;
		bPushConstant = false;

		userDefinedElementTypeStr = NULL;
    }
  
    HLSLDeclaration*    field;
	//HLSLBaseType        dataType;

	bool				bAtomic;
	bool				bPushConstant;

	const char*			userDefinedElementTypeStr;

	//const char* elementType;
};

/*
struct HLSLConstantBuffer : public HLSLBuffer
{
	static const HLSLNodeType s_type = HLSLNodeType_ConstantBuffer;
	HLSLConstantBuffer()
	{
		HLSLBuffer();
		bPush_Constant = false;		
	}

	bool				bPush_Constant;
	
};

struct HLSLStructuredBuffer : public HLSLBuffer
{
	static const HLSLNodeType s_type = HLSLNodeType_StructuredBuffer;
	HLSLStructuredBuffer()
	{
		HLSLBuffer();
	}
};

struct HLSLByteAddressBuffer : public HLSLBuffer
{
	static const HLSLNodeType s_type = HLSLNodeType_ByteAddressBuffer;
	HLSLByteAddressBuffer()
	{
		HLSLBuffer();		
	}
};


struct HLSLRWBuffer : public HLSLBuffer
{
	static const HLSLNodeType s_type = HLSLNodeType_RWBuffer;
	HLSLRWBuffer()
	{
		HLSLBuffer();
		bAtomic = false;
	}	
	
	bool				bAtomic;
};

struct HLSLRWStructuredBuffer : public HLSLBuffer
{
	static const HLSLNodeType s_type = HLSLNodeType_RWStructuredBuffer;
	HLSLRWStructuredBuffer()
	{				
		elementType = NULL;	
		bAtomic = false;
	}

	const char*			elementType;
	
	bool				bAtomic;
};
*/


/** Function declaration */
struct HLSLFunction : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Function;
    HLSLFunction()
    {
        name            = NULL;
        semantic        = NULL;
        sv_semantic     = NULL;
        statement       = NULL;
        argument        = NULL;
        numArguments    = 0;
        forward         = NULL;
		bPatchconstantfunc = false;
		macroFunctionBody = NULL;
    }
    const char*         name;
    HLSLType            returnType;
    const char*         semantic;
    const char*         sv_semantic;
    int                 numArguments;
    HLSLArgument*       argument;
    HLSLStatement*      statement;
    HLSLFunction*       forward; // Which HLSLFunction this one forward-declares

	bool				bPatchconstantfunc;
	const char*			macroFunctionBody;

};

/** Declaration of an argument to a function. */
struct HLSLArgument : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Argument;
    HLSLArgument()
    {
        name            = NULL;
        modifier        = HLSLArgumentModifier_None;
        semantic        = NULL;
        sv_semantic     = NULL;
        defaultValue    = NULL;
        nextArgument    = NULL;
        hidden          = false;

		dataTypeName	= NULL;
		preprocessor = NULL;
    }
    const char*             name;
    HLSLArgumentModifier    modifier;
    HLSLType                type;
    const char*             semantic;
    const char*             sv_semantic;
    HLSLExpression*         defaultValue;
    HLSLArgument*           nextArgument;
	HLSLStatement*			preprocessor;
    bool                    hidden;

	const char*             dataTypeName;

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

		for (int i = 0; i < 128; i++)
		{
			elseifStatement[i] = NULL;
		}

		

		elseifStatementCounter = 0;
    }
    HLSLExpression*     condition;
    HLSLStatement*      statement;
    HLSLStatement*      elseStatement;
	HLSLIfStatement*      elseifStatement[128];
	int					elseifStatementCounter;
};

struct HLSLSwitchStatement : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_SwitchStatement;
	HLSLSwitchStatement()
	{		
		condition = NULL;
		
		for (int i = 0; i < 128; i++)
		{
			caseNumber[i] = NULL;
			caseStatement[i] = NULL;
		}
		
		caseDefault = NULL;




		caseCounter = 0;
	}
	
	HLSLExpression*     condition;
	
	HLSLExpression*     caseNumber[128];
	HLSLStatement*      caseStatement[128];
	HLSLStatement*		caseDefault;


	int					caseCounter;
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
		//initialization = NULL;
		condition = NULL;
		//increment = NULL;
		statement = NULL;
	}
	//HLSLDeclaration*    initialization;
	HLSLExpression*     condition;
	//HLSLExpression*     increment;
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
        nextExpression = NULL;
		childExpression = NULL;
		functionExpression = NULL;
    }
    HLSLType            expressionType;
	HLSLExpression*     childExpression;
    HLSLExpression*     nextExpression; // Used when the expression is part of a list, like in a function call.

	HLSLExpression*		functionExpression; // this is for function call from texture / buffer type with '.'

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
    HLSLCastingExpression()
    {
        expression = NULL;
    }
    HLSLType            type;
    HLSLExpression*     expression;
};


struct HLSLpreprocessor : public HLSLStatement
{
	static const HLSLNodeType s_type = HLSLNodeType_Preprocessor;

	HLSLpreprocessor()
	{
		name = NULL;
		identifier[0] = 0;
		//contents[0] = 0;
		contents = NULL;
		preprocessorType = 0;
		expression = NULL;
		macroFunction = NULL;

		userMacroExpression = NULL;
	}

	HLSLBaseType    type;   // Note, not all types can be literals.

	const char*     name;
	char			identifier[64];
	//char			contents[1024];
	const char*		contents;
	
	unsigned int	preprocessorType;

	HLSLExpression* expression;

	HLSLFunction* macroFunction;

	HLSLExpression* userMacroExpression;
};

struct HLSLPreprocessorExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_PreprocessorExpression;
	HLSLBaseType        type;   // should get it from HLSLpreprocessor

	HLSLPreprocessorExpression()
	{
		name[0] = 0;


		//contents[0] = 0;
		contents = NULL;
		preprocessorType = 0;
	}

	char         name[128];
	//char			contents[1024];
	const char*		contents;
	unsigned int	preprocessorType;
};


struct HLSLTextureStateExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_TextureStateExpression;
	HLSLBaseType        type;   // Note, not all types can be literals.

	HLSLTextureStateExpression()
	{
		name[0] = 0;
		//fuctionExpression = NULL;

		arrayIdentifier[0][0] = 0;
		arrayIdentifier[1][0] = 0;
		arrayIdentifier[2][0] = 0;

		arrayIndex[0] = 0;
		arrayIndex[1] = 0;
		arrayIndex[2] = 0;

		arrayDimension = 0;

		bArray = false;
		arrayExpression = NULL;

		indexExpression = NULL;

		memberAccessExpression = NULL;
	}
	char         name[128];

	//HLSLExpression* fuctionExpression;

	//assuming that texture can be multidimension up to 3-D
	bool				bArray;
	char				arrayIdentifier[3][64];
	unsigned int		arrayIndex[3];
	unsigned int		arrayDimension;

	HLSLExpression*		arrayExpression;

	HLSLExpression*		indexExpression;

	HLSLMemberAccess*	memberAccessExpression;
};

/*
struct HLSLRWTextureStateExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_RWTextureStateExpression;
	HLSLBaseType        type;   // Note, not all types can be literals.

	HLSLRWTextureStateExpression()
	{
		name[0] = 0;
		fuctionExpression = NULL;

		arrayIdentifier[0][0] = 0;
		arrayIdentifier[1][0] = 0;
		arrayIdentifier[2][0] = 0;

		arrayIndex[0] = 0;
		arrayIndex[1] = 0;
		arrayIndex[2] = 0;

		arrayDimension = 0;

		bArray = false;
		arrayExpression = NULL;
	}
	char         name[128];

	HLSLExpression* fuctionExpression;

	//assuming that texture can be multidimension up to 3-D
	bool				bArray;
	char				arrayIdentifier[3][64];
	unsigned int		arrayIndex[3];
	unsigned int		arrayDimension;

	HLSLExpression*		arrayExpression;
};
*/

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
        name     = NULL;
        global  = false;

		fuctionExpression = NULL;

		arrayIdentifier[0][0] = 0;
		arrayIdentifier[1][0] = 0;
		arrayIdentifier[2][0] = 0;

		arrayIndex[0] = 0;
		arrayIndex[1] = 0;
		arrayIndex[2] = 0;

		arrayDimension = 0;

		bArray = false;
		arrayExpression = NULL;
    }
    const char*         name;
    bool                global; // This is a global variable.


	HLSLExpression* fuctionExpression;

	//assuming that texture can be multidimension up to 3-D
	bool				bArray;
	char				arrayIdentifier[3][64];
	unsigned int		arrayIndex[3];
	unsigned int		arrayDimension;

	HLSLExpression*		arrayExpression;
};




/** float2(1, 2) */
struct HLSLConstructorExpression : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_ConstructorExpression;
	HLSLConstructorExpression()
	{
		argument = NULL;
	}
    HLSLType            type;
    HLSLExpression*     argument;
};

/** object.member **/
struct HLSLMemberAccess : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_MemberAccess;
	HLSLMemberAccess()
	{
		object  = NULL;
		functionExpression = NULL;
		field   = NULL;
		swizzle = false;
		function = false;
	}
    HLSLExpression*     object;
	HLSLExpression*     functionExpression;
    const char*         field;
    bool                swizzle;
	bool				function;
};

/** array[index] **/
struct HLSLArrayAccess : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_ArrayAccess;
	HLSLArrayAccess()
	{
		array = NULL;
		index = NULL;

		identifier = NULL;
	}
    HLSLExpression*     array;
    HLSLExpression*     index;

	const char*         identifier;
};

struct HLSLFunctionCall : public HLSLExpression
{
    static const HLSLNodeType s_type = HLSLNodeType_FunctionCall;
	HLSLFunctionCall()
	{
		function     = NULL;
		argument     = NULL;
		numArguments = 0;
		//functionCaller[0] = NULL;

		//bTextureFunction = false;

		pTextureState = NULL;
		//pRWTextureState = NULL;

		pTextureStateExpression = NULL;
		//pRWTextureStateExpression = NULL;

		//pStructuredBuffer = NULL;
		pBuffer = NULL;

	}
    const HLSLFunction* function;
    HLSLExpression*     argument;
    int                 numArguments;
	//char				functionCaller[64];

	//bool				bTextureFunction;
	//HLSLBaseType		textureType;

	const HLSLTextureState* pTextureState;
	//const HLSLRWTextureState* pRWTextureState;

	const HLSLTextureStateExpression* pTextureStateExpression;
	//const HLSLRWTextureStateExpression* pRWTextureStateExpression;

	//const HLSLExpression* pStructuredBuffer;
	const HLSLBuffer* pBuffer;
};

struct HLSLStateAssignment : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_StateAssignment;
    HLSLStateAssignment()
    {
        stateName = NULL;
        sValue = NULL;
        nextStateAssignment = NULL;
    }

    const char*             stateName;
    int                     d3dRenderState;
    union {
		unsigned int		uiValue;
        int                 iValue;
        float               fValue;
        const char *        sValue;
    };
    HLSLStateAssignment*    nextStateAssignment;
};

struct HLSLSamplerStateExpression : public HLSLExpression
{
	static const HLSLNodeType s_type = HLSLNodeType_SamplerStateExpression;
	HLSLSamplerStateExpression()
	{
		//name = NULL;
		nextExpression = NULL;
		hidden = false;
	}
	char							name[128];
	HLSLType						type;
	HLSLBaseType					baseType;
	HLSLSamplerStateExpression*		nextExpression;      // Next field in the structure.
	//HLSLArrayAccess*				arrayAcess;
	bool							hidden;

	char							lvalue[128];
	char							rvalue[128];
};


/*
struct HLSLSamplerState : public HLSLExpression // @@ Does this need to be an expression? Does it have a type? I guess type is useful.
{
    static const HLSLNodeType s_type = HLSLNodeType_SamplerState;
    HLSLSamplerState()
    {
        numStateAssignments = 0;
        stateAssignments = NULL;
    }

    int                     numStateAssignments;
    HLSLStateAssignment*    stateAssignments;
};
*/

struct HLSLPass : public HLSLNode
{
    static const HLSLNodeType s_type = HLSLNodeType_Pass;
    HLSLPass()
    {
        name = NULL;
        numStateAssignments = 0;
        stateAssignments = NULL;
        nextPass = NULL;
    }
    
    const char*             name;
    int                     numStateAssignments;
    HLSLStateAssignment*    stateAssignments;
    HLSLPass*               nextPass;
};

struct HLSLTechnique : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Technique;
    HLSLTechnique()
    {
        name = NULL;
        numPasses = 0;
        passes = NULL;
    }

    const char*         name;
    int                 numPasses;
    HLSLPass*           passes;
};

struct HLSLPipeline : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Pipeline;
    HLSLPipeline()
    {
        name = NULL;
        numStateAssignments = 0;
        stateAssignments = NULL;
    }
    
    const char*             name;
    int                     numStateAssignments;
    HLSLStateAssignment*    stateAssignments;
};

struct HLSLStage : public HLSLStatement
{
    static const HLSLNodeType s_type = HLSLNodeType_Stage;
    HLSLStage()
    {
        name = NULL;
        statement = NULL;
        inputs = NULL;
        outputs = NULL;
    }

    const char*             name;
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

    explicit HLSLTree(Allocator* allocator);
    ~HLSLTree();

    /** Adds a string to the string pool used by the tree. */
    const char* AddString(const char* string);
	const char* AddDefineString(const char* string);
    const char* AddStringFormat(const char* string, ...);

    /** Returns true if the string is contained within the tree. */
    bool GetContainsString(const char* string) const;

    /** Returns the root block in the tree */
    HLSLRoot* GetRoot() const;

    /** Adds a new node to the tree with the specified type. */
    template <class T>
    T* AddNode(const char* fileName, int line)
    {
        HLSLNode* node = new (AllocateMemory(sizeof(T))) T();
        node->nodeType  = T::s_type;
        node->fileName  = fileName;
        node->line      = line;
        return static_cast<T*>(node);
    }

    HLSLFunction * FindFunction(const char * name, int index = 0);
    HLSLDeclaration * FindGlobalDeclaration(const char * name, HLSLBuffer ** buffer_out = NULL);
    HLSLStruct * FindGlobalStruct(const char * name);
	const char* FindGlobalStructMember(const char *memberName);
	const char* FindBuffertMember(const char *memberName);
    HLSLTechnique * FindTechnique(const char * name);
    HLSLPipeline * FindFirstPipeline();
    HLSLPipeline * FindNextPipeline(HLSLPipeline * current);
    HLSLPipeline * FindPipeline(const char * name);
    HLSLBuffer * FindBuffer(const char * name);
	HLSLTextureStateExpression * FindTextureStateExpression(const char * name);

    bool GetExpressionValue(HLSLExpression * expression, int & value);
    int GetExpressionValue(HLSLExpression * expression, float values[4]);

    bool NeedsFunction(const char * name);


	bool NeedsExtension(GlobalExtension ext)
	{
		return gExtension[ext];
	}

	bool gExtension[MAX_GLOBAL_EXTENSION];

private:

    void* AllocateMemory(size_t size);
    void  AllocatePage();

private:

    static const size_t s_nodePageSize = 1024 * 4;

    struct NodePage
    {
        NodePage*   next;
        char        buffer[s_nodePageSize];
    };

    Allocator*      m_allocator;

    StringPool      m_stringPool;
	StringPool      m_defineStringPool;

    HLSLRoot*       m_root;

    NodePage*       m_firstPage;
    NodePage*       m_currentPage;
    size_t          m_currentPageOffset;

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
    //virtual void VisitBufferField(HLSLBufferField * node);
    virtual void VisitFunction(HLSLFunction * node);
    virtual void VisitArgument(HLSLArgument * node);
    virtual void VisitExpressionStatement(HLSLExpressionStatement * node);
    virtual void VisitExpression(HLSLExpression * node);
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
    virtual void VisitPass(HLSLPass * node);
    virtual void VisitTechnique(HLSLTechnique * node);

    virtual void VisitFunctions(HLSLRoot * root);
    virtual void VisitParameters(HLSLRoot * root);

    HLSLFunction * FindFunction(HLSLRoot * root, const char * name);
    HLSLDeclaration * FindGlobalDeclaration(HLSLRoot * root, const char * name);
    HLSLStruct * FindGlobalStruct(HLSLRoot * root, const char * name);
};


// Tree transformations:
extern void PruneTree(HLSLTree* tree, const char* entryName0, const char* entryName1 = NULL);
extern void SortTree(HLSLTree* tree);
extern void GroupParameters(HLSLTree* tree);
extern void HideUnusedArguments(HLSLFunction * function);
extern bool EmulateAlphaTest(HLSLTree* tree, const char* entryName, float alphaRef = 0.5f);

static const char* getElementTypeAsStr(HLSLType type)
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
		return type.typeName;
	default:
		return "UnknownElementType";
	}
}


static const char* getElementTypeAsStrGLSL(HLSLType type)
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
		return type.typeName;
	default:
		return "UnknownElementType";
	}
}



#endif
