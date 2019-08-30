#ifndef HLSL_TOKENIZER_H
#define HLSL_TOKENIZER_H

#include <fstream>
#include <sstream>
#include <iostream>

/** In addition to the values in this enum, all of the ASCII characters are
valid tokens. */
enum HLSLToken
{
    // Built-in types.
	
	HLSLToken_Float = 256,
	HLSLToken_First_Numeric_Type = HLSLToken_Float,
	HLSLToken_Float1x2,	
	HLSLToken_Float1x3,	
	HLSLToken_Float1x4,	
	HLSLToken_Float2,	
	HLSLToken_Float2x2,	
	HLSLToken_Float2x3,	
	HLSLToken_Float2x4,	
	HLSLToken_Float3,	
	HLSLToken_Float3x2,	
	HLSLToken_Float3x3,	
	HLSLToken_Float3x4,	
	HLSLToken_Float4,	
	HLSLToken_Float4x2,	
	HLSLToken_Float4x3,	
	HLSLToken_Float4x4,	

	HLSLToken_Half,		//272
	HLSLToken_Half1x2,	//273
	HLSLToken_Half1x3,	//274
	HLSLToken_Half1x4,	//275
	HLSLToken_Half2,	//276
	HLSLToken_Half2x2,	//278
	HLSLToken_Half2x3,	//279
	HLSLToken_Half2x4,	//280
	HLSLToken_Half3,	//281
	HLSLToken_Half3x2,	//282
	HLSLToken_Half3x3,	//283
	HLSLToken_Half3x4,	//284
	HLSLToken_Half4,	//285
	HLSLToken_Half4x2,	//286
	HLSLToken_Half4x3,	//287
	HLSLToken_Half4x4,	//288
   
	HLSLToken_Min16Float,
	HLSLToken_Min16Float1x2,
	HLSLToken_Min16Float1x3,
	HLSLToken_Min16Float1x4,
	HLSLToken_Min16Float2,
	HLSLToken_Min16Float2x2,
	HLSLToken_Min16Float2x3,
	HLSLToken_Min16Float2x4,
	HLSLToken_Min16Float3,
	HLSLToken_Min16Float3x2,
	HLSLToken_Min16Float3x3,
	HLSLToken_Min16Float3x4,
	HLSLToken_Min16Float4,
	HLSLToken_Min16Float4x2,
	HLSLToken_Min16Float4x3,
	HLSLToken_Min16Float4x4,


	HLSLToken_Min10Float,
	HLSLToken_Min10Float1x2,
	HLSLToken_Min10Float1x3,
	HLSLToken_Min10Float1x4,
	HLSLToken_Min10Float2,
	HLSLToken_Min10Float2x2,
	HLSLToken_Min10Float2x3,
	HLSLToken_Min10Float2x4,
	HLSLToken_Min10Float3,
	HLSLToken_Min10Float3x2,
	HLSLToken_Min10Float3x3,
	HLSLToken_Min10Float3x4,
	HLSLToken_Min10Float4,
	HLSLToken_Min10Float4x2,
	HLSLToken_Min10Float4x3,
	HLSLToken_Min10Float4x4,

	HLSLToken_Bool,		//288
	HLSLToken_Bool1x2,	
	HLSLToken_Bool1x3,	
	HLSLToken_Bool1x4,	
	HLSLToken_Bool2,	
	HLSLToken_Bool2x2,	
	HLSLToken_Bool2x3,	
	HLSLToken_Bool2x4,	
	HLSLToken_Bool3,	
	HLSLToken_Bool3x2,	
	HLSLToken_Bool3x3,	
	HLSLToken_Bool3x4,	
	HLSLToken_Bool4,	
	HLSLToken_Bool4x2,	
	HLSLToken_Bool4x3,	
	HLSLToken_Bool4x4,	

	HLSLToken_Int,		//304
	HLSLToken_Int1x2,	
	HLSLToken_Int1x3,	
	HLSLToken_Int1x4,	
	HLSLToken_Int2,		
	HLSLToken_Int2x2,	
	HLSLToken_Int2x3,	
	HLSLToken_Int2x4,	
	HLSLToken_Int3,		
	HLSLToken_Int3x2,	
	HLSLToken_Int3x3,	
	HLSLToken_Int3x4,	
	HLSLToken_Int4,		
	HLSLToken_Int4x2,	
	HLSLToken_Int4x3,	
	HLSLToken_Int4x4,	

	HLSLToken_Uint,		//320
	HLSLToken_Uint1x2,	
	HLSLToken_Uint1x3,	
	HLSLToken_Uint1x4,	
	HLSLToken_Uint2,	
	HLSLToken_Uint2x2,	
	HLSLToken_Uint2x3,	
	HLSLToken_Uint2x4,	
	HLSLToken_Uint3,	
	HLSLToken_Uint3x2,	
	HLSLToken_Uint3x3,	
	HLSLToken_Uint3x4,	
	HLSLToken_Uint4,	
	HLSLToken_Uint4x2,	
	HLSLToken_Uint4x3,	
	HLSLToken_Uint4x4,	
	HLSLToken_Last_Numeric_Type = HLSLToken_Uint4x4,
			
    //hull
	HLSLToken_InputPatch,

	//domain
	HLSLToken_OutputPatch,

	//geometry
	HLSLToken_PointStream,
	HLSLToken_LineStream,
	HLSLToken_TriangleStream,

	HLSLToken_Groupshared,

    HLSLToken_Texture,	//336

	HLSLToken_Texture1D,
	HLSLToken_First_Texture_Type = HLSLToken_Texture1D,
	HLSLToken_Texture1DArray,
	HLSLToken_Texture2D,
	HLSLToken_Texture2DArray,
	HLSLToken_Texture3D,
	HLSLToken_Texture2DMS,
	HLSLToken_Texture2DMSArray,
	HLSLToken_TextureCube,
	HLSLToken_TextureCubeArray,

	HLSLToken_RasterizerOrderedTexture1D,
	HLSLToken_RasterizerOrderedTexture1DArray,
	HLSLToken_RasterizerOrderedTexture2D,
	HLSLToken_RasterizerOrderedTexture2DArray,
	HLSLToken_RasterizerOrderedTexture3D,

	HLSLToken_RWTexture1D,
	HLSLToken_RWTexture1DArray,
	HLSLToken_RWTexture2D,
	HLSLToken_RWTexture2DArray,
	HLSLToken_RWTexture3D,
	HLSLToken_Last_Texture_Type = HLSLToken_RWTexture3D,

	HLSLToken_SamplerState,
    HLSLToken_Sampler,
    HLSLToken_Sampler2D,
    HLSLToken_Sampler3D,
    HLSLToken_SamplerCube,
    HLSLToken_Sampler2DShadow,   
    HLSLToken_Sampler2DMS,
    HLSLToken_Sampler2DArray,

	HLSLToken_SamplerComparisonState,

    // Reserved words.
    HLSLToken_If,
	HLSLToken_ElseIf,
    HLSLToken_Else,
    HLSLToken_For,
    HLSLToken_While,
	HLSLToken_Switch,
	HLSLToken_Case,
	HLSLToken_Default,
    HLSLToken_Break,
    HLSLToken_True,
    HLSLToken_False,
    HLSLToken_Void,				
    HLSLToken_Struct,

    HLSLToken_CBuffer,
	HLSLToken_First_Buffer_Type = HLSLToken_CBuffer,
    HLSLToken_TBuffer,
	HLSLToken_ConstantBuffer,
	HLSLToken_StructuredBuffer,
	HLSLToken_PureBuffer,
	HLSLToken_RWBuffer,
	HLSLToken_RWStructuredBuffer,
	HLSLToken_ByteAddressBuffer,
	HLSLToken_RWByteAddressBuffer,

	HLSLToken_RasterizerOrderedBuffer,
	HLSLToken_RasterizerOrderedStructuredBuffer,
	HLSLToken_RasterizerOrderedByteAddressBuffer,
		
	HLSLToken_Last_Buffer_Type = HLSLToken_RasterizerOrderedByteAddressBuffer,

    HLSLToken_Register,
    HLSLToken_Return,
    HLSLToken_Continue,
    HLSLToken_Discard,
    HLSLToken_Const,
    HLSLToken_Static,
	HLSLToken_Inline,
	HLSLToken_RowMajor,
	HLSLToken_ColumnMajor,

	// sampler state
	HLSLToken_AddressU,
	HLSLToken_AddressV,
	HLSLToken_AddressW,
	HLSLToken_BorderColor,
	HLSLToken_Filter,
	HLSLToken_MaxAnisotropy,
	HLSLToken_MaxLOD,
	HLSLToken_MinLOD,
	HLSLToken_MipLODBias,
	HLSLToken_ComparisonFunc,

	//Texture Filter
	HLSLToken_MIN_MAG_MIP_POINT,
	HLSLToken_MIN_MAG_POINT_MIP_LINEAR,
	HLSLToken_MIN_POINT_MAG_LINEAR_MIP_POINT,
	HLSLToken_MIN_POINT_MAG_MIP_LINEAR,
	HLSLToken_MIN_LINEAR_MAG_MIP_POINT,
	HLSLToken_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	HLSLToken_MIN_MAG_LINEAR_MIP_POINT,
	HLSLToken_MIN_MAG_MIP_LINEAR,
	HLSLToken_ANISOTROPIC,
	HLSLToken_COMPARISON_MIN_MAG_MIP_POINT,
	HLSLToken_COMPARISON_MIN_MAG_POINT_MIP_LINEAR,
	HLSLToken_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT,
	HLSLToken_COMPARISON_MIN_POINT_MAG_MIP_LINEAR,
	HLSLToken_COMPARISON_MIN_LINEAR_MAG_MIP_POINT,
	HLSLToken_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	HLSLToken_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
	HLSLToken_COMPARISON_MIN_MAG_MIP_LINEAR,
	HLSLToken_COMPARISON_ANISOTROPIC,
	HLSLToken_MINIMUM_MIN_MAG_MIP_POINT,
	HLSLToken_MINIMUM_MIN_MAG_POINT_MIP_LINEAR,
	HLSLToken_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
	HLSLToken_MINIMUM_MIN_POINT_MAG_MIP_LINEAR,
	HLSLToken_MINIMUM_MIN_LINEAR_MAG_MIP_POINT,
	HLSLToken_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	HLSLToken_MINIMUM_MIN_MAG_LINEAR_MIP_POINT,
	HLSLToken_MINIMUM_MIN_MAG_MIP_LINEAR,
	HLSLToken_MINIMUM_ANISOTROPIC,
	HLSLToken_MAXIMUM_MIN_MAG_MIP_POINT,
	HLSLToken_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR,
	HLSLToken_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT,
	HLSLToken_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR,
	HLSLToken_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT,
	HLSLToken_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR,
	HLSLToken_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT,
	HLSLToken_MAXIMUM_MIN_MAG_MIP_LINEAR,
	HLSLToken_MAXIMUM_ANISOTROPIC,

	//Texture Address Mode
	HLSLToken_WRAP,
	HLSLToken_MIRROR,
	HLSLToken_CLAMP,
	HLSLToken_BORDER,
	HLSLToken_MIRROR_ONCE,

	//Comparsion Func
	HLSLToken_NEVER,
	HLSLToken_LESS,
	HLSLToken_EQUAL,
	HLSLToken_LESS_EQUAL,
	HLSLToken_GREATER,
	HLSLToken_NOT_EQUAL,
	HLSLToken_GREATER_EQUAL,
	HLSLToken_ALWAYS,
		
    // Input modifiers.
    HLSLToken_Uniform,
    HLSLToken_In,
    HLSLToken_Out,
    HLSLToken_InOut,	
	HLSLToken_Point,
	HLSLToken_Line,
	HLSLToken_Triangle,
	HLSLToken_Lineadj,
	HLSLToken_Triangleadj,

    // Effect keywords.    
    HLSLToken_Technique,
    HLSLToken_Pass,

	HLSLToken_Sizeof,

    // Multi-character symbols.
    HLSLToken_LessEqual,
    HLSLToken_GreaterEqual,
    HLSLToken_EqualEqual,
    HLSLToken_NotEqual,
    HLSLToken_PlusPlus,
    HLSLToken_MinusMinus,
    HLSLToken_PlusEqual,
    HLSLToken_MinusEqual,
    HLSLToken_TimesEqual,
    HLSLToken_DivideEqual,
    HLSLToken_AndAnd,       // &&
    HLSLToken_BarBar,       // ||
	HLSLToken_LeftShift,    // <<
	HLSLToken_RightShift,   // >>
	HLSLToken_Modular,		// % 

	HLSLToken_AndEqual,		// &=
	HLSLToken_BarEqual,		// |=
	HLSLToken_XorEqual,		// ^=

    // Other token types.
    HLSLToken_FloatLiteral,
	HLSLToken_HalfLiteral,
	HLSLToken_Min16FloatLiteral,
	HLSLToken_Min10FloatLiteral,
    HLSLToken_IntLiteral,
	HLSLToken_UintLiteral,
    HLSLToken_Identifier,

    HLSLToken_EndOfStream,
};

// The order here must match the order in the Token enum.
static const char* _reservedWords[] =
{
	// Built-in types.
	"float",
	"float1x2",
	"float1x3",
	"float1x4",
	"float2",
	"float2x2",
	"float2x3",
	"float2x4",
	"float3",
	"float3x2",
	"float3x3",
	"float3x4",
	"float4",
	"float4x2",
	"float4x3",
	"float4x4",

	"half",
	"half1x2",
	"half1x3",
	"half1x4",
	"half2",
	"half2x2",
	"half2x3",
	"half2x4",
	"half3",
	"half3x2",
	"half3x3",
	"half3x4",
	"half4",
	"half4x2",
	"half4x3",
	"half4x4",

	"min16float",
	"min16float1x2",
	"min16float1x3",
	"min16float1x4",
	"min16float2",
	"min16float2x2",
	"min16float2x3",
	"min16float2x4",
	"min16float3",
	"min16float3x2",
	"min16float3x3",
	"min16float3x4",
	"min16float4",
	"min16float4x2",
	"min16float4x3",
	"min16float4x4",

	"min10float",
	"min10float1x2",
	"min10float1x3",
	"min10float1x4",
	"min10float2",
	"min10float2x2",
	"min10float2x3",
	"min10float2x4",
	"min10float3",
	"min10float3x2",
	"min10float3x3",
	"min10float3x4",
	"min10float4",
	"min10float4x2",
	"min10float4x3",
	"min10float4x4",

	"bool",
	"bool1x2",
	"bool1x3",
	"bool1x4",
	"bool2",
	"bool2x2",
	"bool2x3",
	"bool2x4",
	"bool3",
	"bool3x2",
	"bool3x3",
	"bool3x4",
	"bool4",
	"bool4x2",
	"bool4x3",
	"bool4x4",

	"int",
	"int1x2",
	"int1x3",
	"int1x4",
	"int2",
	"int2x2",
	"int2x3",
	"int2x4",
	"int3",
	"int3x2",
	"int3x3",
	"int3x4",
	"int4",
	"int4x2",
	"int4x3",
	"int4x4",

	"uint",
	"uint1x2",
	"uint1x3",
	"uint1x4",
	"uint2",
	"uint2x2",
	"uint2x3",
	"uint2x4",
	"uint3",
	"uint3x2",
	"uint3x3",
	"uint3x4",
	"uint4",
	"uint4x2",
	"uint4x3",
	"uint4x4",

    "InputPatch",
	"OutputPatch",

	"PointStream",
	"LineStream",
	"TriangleStream",

	"groupshared",

	"texture",
	"Texture1D",
	"Texture1DArray",
	"Texture2D",
	"Texture2DArray",
	"Texture3D",
	"Texture2DMS",
	"Texture2DMSArray",
	"TextureCube",
	"TextureCubeArray",

	"RasterizerOrderedTexture1D",
	"RasterizerOrderedTexture1DArray",
	"RasterizerOrderedTexture2D",
	"RasterizerOrderedTexture2DArray",
	"RasterizerOrderedTexture3D",

	"RWTexture1D",
	"RWTexture1DArray",
	"RWTexture2D",
	"RWTexture2DArray",
	"RWTexture3D",

	"SamplerState",
	"sampler",
	"sampler2D",
	"sampler3D",
	"samplerCUBE",
	"sampler2DShadow",
	"sampler2DMS",
	"sampler2DArray",

	"SamplerComparisonState",

	// Reserved words.
	"if",
	"else if",
	"else",
	"for",
	"while",
	"switch",
	"case",
	"default",
	"break",
	"true",
	"false",
	"void",
	"struct",
	"cbuffer",
	"tbuffer",
	"ConstantBuffer",
	"StructuredBuffer",
	"Buffer",
	"RWBuffer",
	"RWStructuredBuffer",
	"ByteAddressBuffer",
	"RWByteAddressBuffer",

	
	"RasterizerOrderedBuffer",
	"RasterizerOrderedStructuredBuffer",	
	"RasterizerOrderedByteAddressBuffer",

	"register",
	"return",
	"continue",
	"discard",
	"const",
	"static",
	"inline",
	"row_major",
	"column_major",

	// sampler state
	"AddressU",
	"AddressV",
	"AddressW",
	"BorderColor",
	"Filter",
	"MaxAnisotropy",
	"MaxLOD",
	"MinLOD",
	"MipLODBias",
	"ComparisonFunc",


	//Texture Filter
	"MIN_MAG_MIP_POINT",
	"MIN_MAG_POINT_MIP_LINEAR",
	"MIN_POINT_MAG_LINEAR_MIP_POINT",
	"MIN_POINT_MAG_MIP_LINEAR",
	"MIN_LINEAR_MAG_MIP_POINT",
	"MIN_LINEAR_MAG_POINT_MIP_LINEAR",
	"MIN_MAG_LINEAR_MIP_POINT",
	"MIN_MAG_MIP_LINEAR",
	"ANISOTROPIC",
	"COMPARISON_MIN_MAG_MIP_POINT",
	"COMPARISON_MIN_MAG_POINT_MIP_LINEAR",
	"COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT",
	"COMPARISON_MIN_POINT_MAG_MIP_LINEAR",
	"COMPARISON_MIN_LINEAR_MAG_MIP_POINT",
	"COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR",
	"COMPARISON_MIN_MAG_LINEAR_MIP_POINT",
	"COMPARISON_MIN_MAG_MIP_LINEAR",
	"COMPARISON_ANISOTROPIC",
	"MINIMUM_MIN_MAG_MIP_POINT",
	"MINIMUM_MIN_MAG_POINT_MIP_LINEAR",
	"MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",
	"MINIMUM_MIN_POINT_MAG_MIP_LINEAR",
	"MINIMUM_MIN_LINEAR_MAG_MIP_POINT",
	"MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",
	"MINIMUM_MIN_MAG_LINEAR_MIP_POINT",
	"MINIMUM_MIN_MAG_MIP_LINEAR",
	"MINIMUM_ANISOTROPIC",
	"MAXIMUM_MIN_MAG_MIP_POINT",
	"MAXIMUM_MIN_MAG_POINT_MIP_LINEAR",
	"MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT",
	"MAXIMUM_MIN_POINT_MAG_MIP_LINEAR",
	"MAXIMUM_MIN_LINEAR_MAG_MIP_POINT",
	"MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR",
	"MAXIMUM_MIN_MAG_LINEAR_MIP_POINT",
	"MAXIMUM_MIN_MAG_MIP_LINEAR",
	"MAXIMUM_ANISOTROPIC",

		//Texture Address Mode
	"WRAP",
	"MIRROR",
	"CLAMP",
	"BORDER",
	"MIRROR_ONCE",

		//Comparsion Func
	"NEVER",
	"LESS",
	"EQUAL",
	"LESS_EQUAL",
	"GREATER",
	"NOT_EQUAL",
	"GREATER_EQUAL",
	"ALWAYS",

	// Input modifiers.
	"uniform",
	"in",
	"out",
	"inout",
	"point",
	"line",
	"triangle",
	"lineadj",
	"triangleadj",
		
	// Effect keywords.	
	"technique",
	"pass",

	"sizeof",
};

class HLSLTokenizer
{

public:

    /// Maximum string length of an identifier.
    static const int s_maxIdentifier = 255 + 1;

	static const int s_maxHistoryIdentifier = 1024;

    /** The file name is only used for error reporting. */
    HLSLTokenizer(const char* fileName, const char* buffer, size_t length);

    /** Advances to the next token in the stream. */
    void Next();

    /** Returns the current token in the stream. */
    int GetToken() const;
	
    /** Returns the number of the current token. */
    float GetFloat() const;
    int   GetInt() const;
	unsigned int GetuInt() const;

    /** Returns the identifier for the current token. */
    const char* GetIdentifier() const;

	const char* GetPrevIdentifier(int index) const;

	int GetHistoryCounter() const;

    /** Returns the line number where the current token began. */
    int GetLineNumber() const;

    /** Returns the file name where the current token began. */
    const char* GetFileName() const;

    /** Gets a human readable text description of the current token. */
    void GetTokenName(char buffer[s_maxIdentifier]) const;

    /** Reports an error using printf style formatting. The current line number
    is included. Only the first error reported will be output. */
    void Error(const char* format, ...);

    /** Gets a human readable text description of the specified token. */
    static void GetTokenName(int token, char buffer[s_maxIdentifier]);

	const char* GetBufferAddress() const;
	const char* GetIndentifierHistoryAddress(int index) const;

	void Undo();

	char errorBuffer[1024];

	int	m_historyCounter;

private:
    bool SkipWhitespace();
    bool SkipComment();
	bool SkipPragmaDirective();
    bool ScanNumber();
    bool ScanLineDirective();

	bool GetRestofWholeline(char* strBuffer);
	bool GetRestofWholelineWOSpace(char* strBuffer);

private:

    const char*         m_fileName;

	const char*         m_prevBuffer;
    const char*         m_buffer;
    const char*         m_bufferEnd;
    int                 m_lineNumber;
    bool                m_error;

    int                 m_token;
	int					m_PrevToken;

    float               m_fValue;
    int                 m_iValue;
	unsigned int		m_uiValue;
    char                m_identifier[s_maxIdentifier];

	char				m_identifierHistory[s_maxHistoryIdentifier][s_maxIdentifier];
	const char*			m_identifierHistoryAddress[s_maxHistoryIdentifier];
	
	
    char                m_lineDirectiveFileName[s_maxIdentifier];
    int                 m_tokenLineNumber;
};

#endif