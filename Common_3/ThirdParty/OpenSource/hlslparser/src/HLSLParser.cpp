//=============================================================================
//
// Render/HLSLParser.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

//#include "Engine/String.h"
#include "Engine.h"

#include "HLSLParser.h"
#include "HLSLTree.h"

#include <algorithm>
#include <ctype.h>
#include <string.h>

enum CompareFunctionsResult
{
    FunctionsEqual,
    Function1Better,
    Function2Better
};

/** This structure stores a HLSLFunction-like declaration for an intrinsic function */
struct Intrinsic
{
    explicit Intrinsic(const char* name, HLSLBaseType returnType)
    {
        function.name                   = name;
        function.returnType.baseType    = returnType;
        function.numArguments           = 0;
    }
    explicit Intrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType arg1)
    {
        function.name                   = name;
        function.returnType.baseType    = returnType;
        function.numArguments           = 1;
        function.argument               = argument + 0;
        argument[0].type.baseType       = arg1;
        argument[0].type.flags          = HLSLTypeFlag_Const;
    }
    explicit Intrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2)
    {
        function.name                   = name;
        function.returnType.baseType    = returnType;
        function.argument               = argument + 0;
        function.numArguments           = 2;
        argument[0].type.baseType       = arg1;
        argument[0].type.flags          = HLSLTypeFlag_Const;
        argument[0].nextArgument        = argument + 1;
        argument[1].type.baseType       = arg2;
        argument[1].type.flags          = HLSLTypeFlag_Const;
    }
    explicit Intrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3)
    {
        function.name                   = name;
        function.returnType.baseType    = returnType;
        function.argument               = argument + 0;
        function.numArguments           = 3;
        argument[0].type.baseType       = arg1;
        argument[0].type.flags          = HLSLTypeFlag_Const;
        argument[0].nextArgument        = argument + 1;
        argument[1].type.baseType       = arg2;
        argument[1].type.flags          = HLSLTypeFlag_Const;
        argument[1].nextArgument        = argument + 2;
        argument[2].type.baseType       = arg3;
        argument[2].type.flags          = HLSLTypeFlag_Const;
    }
    explicit Intrinsic(const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4)
    {
        function.name                   = name;
        function.returnType.baseType    = returnType;
        function.argument               = argument + 0;
        function.numArguments           = 4;
        argument[0].type.baseType       = arg1;
        argument[0].type.flags          = HLSLTypeFlag_Const;
        argument[0].nextArgument        = argument + 1;
        argument[1].type.baseType       = arg2;
        argument[1].type.flags          = HLSLTypeFlag_Const;
        argument[1].nextArgument        = argument + 2;
        argument[2].type.baseType       = arg3;
        argument[2].type.flags          = HLSLTypeFlag_Const;
        argument[2].nextArgument        = argument + 3;
        argument[3].type.baseType       = arg4;
        argument[3].type.flags          = HLSLTypeFlag_Const;
    }
    HLSLFunction    function;
    HLSLArgument    argument[4];
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

static const int _numberTypeRank[NumericType_Count][NumericType_Count] = 
{
    //F  H  B  I  U    
    { 0, 4, 4, 4, 4 },  // NumericType_Float
    { 1, 0, 4, 4, 4 },  // NumericType_Half
    { 5, 5, 0, 5, 5 },  // NumericType_Bool
    { 5, 5, 4, 0, 3 },  // NumericType_Int
    { 5, 5, 4, 2, 0 }   // NumericType_Uint
};


struct EffectStateValue
{
    const char * name;
    int value;
};

static const EffectStateValue textureFilteringValues[] = {
    {"None", 0},
    {"Point", 1},
    {"Linear", 2},
    {"Anisotropic", 3},
    {NULL, 0}
};

static const EffectStateValue textureAddressingValues[] = {
    {"Wrap", 1},
    {"Mirror", 2},
    {"Clamp", 3},
    {"Border", 4},
    {"MirrorOnce", 5},
    {NULL, 0}
};

static const EffectStateValue booleanValues[] = {
    {"False", 0},
    {"True", 1},
    {NULL, 0}
};

static const EffectStateValue cullValues[] = {
    {"None", 1},
    {"CW", 2},
    {"CCW", 3},
    {NULL, 0}
};

static const EffectStateValue cmpValues[] = {
    {"Never", 1},
    {"Less", 2},
    {"Equal", 3},
    {"LessEqual", 4},
    {"Greater", 5},
    {"NotEqual", 6},
    {"GreaterEqual", 7},
    {"Always", 8},
    {NULL, 0}
};

static const EffectStateValue blendValues[] = {
    {"Zero", 1},
    {"One", 2},
    {"SrcColor", 3},
    {"InvSrcColor", 4},
    {"SrcAlpha", 5},
    {"InvSrcAlpha", 6},
    {"DestAlpha", 7},
    {"InvDestAlpha", 8},
    {"DestColor", 9},
    {"InvDestColor", 10},
    {"SrcAlphaSat", 11},
    {"BothSrcAlpha", 12},
    {"BothInvSrcAlpha", 13},
    {"BlendFactor", 14},
    {"InvBlendFactor", 15},
    {"SrcColor2", 16},          // Dual source blending. D3D9Ex only.
    {"InvSrcColor2", 17},
    {NULL, 0}
};

static const EffectStateValue blendOpValues[] = {
    {"Add", 1},
    {"Subtract", 2},
    {"RevSubtract", 3},
    {"Min", 4},
    {"Max", 5},
    {NULL, 0}
};

static const EffectStateValue fillModeValues[] = {
    {"Point", 1},
    {"Wireframe", 2},
    {"Solid", 3},
    {NULL, 0}
};

static const EffectStateValue stencilOpValues[] = {
    {"Keep", 1},
    {"Zero", 2},
    {"Replace", 3},
    {"IncrSat", 4},
    {"DecrSat", 5},
    {"Invert", 6},
    {"Incr", 7},
    {"Decr", 8},
    {NULL, 0}
};

// These are flags.
static const EffectStateValue colorMaskValues[] = {
    {"False", 0},
    {"Red",   1<<0},
    {"Green", 1<<1},
    {"Blue",  1<<2},
    {"Alpha", 1<<3},
    {"X", 1<<0},
    {"Y", 1<<1},
    {"Z", 1<<2},
    {"W", 1<<3},
    {NULL, 0}
};

static const EffectStateValue integerValues[] = {
    {NULL, 0}
};

static const EffectStateValue uintegerValues[] = {
	{ NULL, 0 }
};

static const EffectStateValue floatValues[] = {
    {NULL, 0}
};


struct EffectState
{
    const char * name;
    int d3drs;
    const EffectStateValue * values;
};

static const EffectState samplerStates[] = {
    {"AddressU", 1, textureAddressingValues},
    {"AddressV", 2, textureAddressingValues},
    {"AddressW", 3, textureAddressingValues},
    // "BorderColor", 4, D3DCOLOR
    {"MagFilter", 5, textureFilteringValues},
    {"MinFilter", 6, textureFilteringValues},
    {"MipFilter", 7, textureFilteringValues},
    {"MipMapLodBias", 8, floatValues},
    {"MaxMipLevel", 9, integerValues},
    {"MaxAnisotropy", 10, integerValues},
    {"sRGBTexture", 11, booleanValues},    
};

static const EffectState effectStates[] = {
    {"VertexShader", 0, NULL},
    {"PixelShader", 0, NULL},
    {"AlphaBlendEnable", 27, booleanValues},
    {"SrcBlend", 19, blendValues},
    {"DestBlend", 20, blendValues},
    {"BlendOp", 171, blendOpValues},
    {"SeparateAlphaBlendEanble", 206, booleanValues},
    {"SrcBlendAlpha", 207, blendValues},
    {"DestBlendAlpha", 208, blendValues},
    {"BlendOpAlpha", 209, blendOpValues},
    {"AlphaTestEnable", 15, booleanValues},
    {"AlphaRef", 24, integerValues},
    {"AlphaFunc", 25, cmpValues},
    {"CullMode", 22, cullValues},
    {"ZEnable", 7, booleanValues},
    {"ZWriteEnable", 14, booleanValues},
    {"ZFunc", 23, cmpValues},
    {"StencilEnable", 52, booleanValues},
    {"StencilFail", 53, stencilOpValues},
    {"StencilZFail", 54, stencilOpValues},
    {"StencilPass", 55, stencilOpValues},
    {"StencilFunc", 56, cmpValues},
    {"StencilRef", 57, integerValues},
    {"StencilMask", 58, integerValues},
    {"StencilWriteMask", 59, integerValues},
    {"TwoSidedStencilMode", 185, booleanValues},
    {"CCW_StencilFail", 186, stencilOpValues},
    {"CCW_StencilZFail", 187, stencilOpValues},
    {"CCW_StencilPass", 188, stencilOpValues},
    {"CCW_StencilFunc", 189, cmpValues},
    {"ColorWriteEnable", 168, colorMaskValues},
    {"FillMode", 8, fillModeValues},
    {"MultisampleAlias", 161, booleanValues},
    {"MultisampleMask", 162, integerValues},
    {"ScissorTestEnable", 174, booleanValues},
    {"SlopeScaleDepthBias", 175, floatValues},
    {"DepthBias", 195, floatValues}
};


static const EffectStateValue witnessCullModeValues[] = {
    {"None", 0},
    {"Back", 1},
    {"Front", 2},
    {NULL, 0}
};

static const EffectStateValue witnessFillModeValues[] = {
    {"Solid", 0},
    {"Wireframe", 1},
    {NULL, 0}
};

static const EffectStateValue witnessBlendModeValues[] = {
    {"Disabled", 0},
    {"AlphaBlend", 1},          // src * a + dst * (1-a)
    {"Add", 2},                 // src + dst
    {"Mixed", 3},               // src + dst * (1-a)
    {"Multiply", 4},            // src * dst
    {"Multiply2", 5},           // 2 * src * dst
    {NULL, 0}
};

static const EffectStateValue witnessDepthFuncValues[] = {
    {"LessEqual", 0},
    {"Less", 1},
    {"Equal", 2},
    {"Greater", 3},
    {"Always", 4},
    {NULL, 0}
};

static const EffectStateValue witnessStencilModeValues[] = {
    {"Disabled", 0},
    {"Set", 1},
    {"Test", 2},
    {NULL, 0}
};

static const EffectStateValue witnessFilterModeValues[] = {
    {"Point", 0},
    {"Linear", 1},
    {"Mipmap_Nearest", 2},
    {"Mipmap_Best", 3},     // Quality of mipmap filtering depends on render settings.
    {"Anisotropic", 4},     // Aniso without mipmaps for reflection maps.
    {NULL, 0}
};

static const EffectStateValue witnessWrapModeValues[] = {
    {"Repeat", 0},
    {"Clamp", 1},
    {"ClampToBorder", 2},
    {NULL, 0}
};

static const EffectState pipelineStates[] = {
    {"VertexShader", 0, NULL},
    {"PixelShader", 0, NULL},

    // Depth_Stencil_State
    {"DepthWrite", 0, booleanValues},
    {"DepthEnable", 0, booleanValues},
    {"DepthFunc", 0, witnessDepthFuncValues},
    {"StencilMode", 0, witnessStencilModeValues},

    // Raster_State
    {"CullMode", 0, witnessCullModeValues},
    {"FillMode", 0, witnessFillModeValues},
    {"MultisampleEnable", 0, booleanValues},
    {"PolygonOffset", 0, booleanValues},

    // Blend_State
    {"BlendMode", 0, witnessBlendModeValues},
    {"ColorWrite", 0, booleanValues},
    {"AlphaWrite", 0, booleanValues},
    {"AlphaTest", 0, booleanValues},       // This is really alpha to coverage.
};



struct BaseTypeDescription
{
    const char*     typeName;
    NumericType     numericType;
    int             numComponents;
    int             numDimensions;
    int             height;
    int             binaryOpRank;
};

#define INTRINSIC_VOID_FUNCTION(name) \
		Intrinsic( name, HLSLBaseType_Void )


#define INTRINSIC_FLOAT1_FUNCTION(name) \
        Intrinsic( name, HLSLBaseType_Float,   HLSLBaseType_Float  ),   \
        Intrinsic( name, HLSLBaseType_Float2,  HLSLBaseType_Float2 ),   \
        Intrinsic( name, HLSLBaseType_Float3,  HLSLBaseType_Float3 ),   \
        Intrinsic( name, HLSLBaseType_Float4,  HLSLBaseType_Float4 ),   \
        Intrinsic( name, HLSLBaseType_Half,    HLSLBaseType_Half   ),   \
        Intrinsic( name, HLSLBaseType_Half2,   HLSLBaseType_Half2  ),   \
        Intrinsic( name, HLSLBaseType_Half3,   HLSLBaseType_Half3  ),   \
        Intrinsic( name, HLSLBaseType_Half4,   HLSLBaseType_Half4  ),	\
		Intrinsic( name, HLSLBaseType_Min16Float,   HLSLBaseType_Min16Float  ),   \
        Intrinsic( name, HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float2 ),   \
        Intrinsic( name, HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float3 ),   \
        Intrinsic( name, HLSLBaseType_Min16Float4,  HLSLBaseType_Min16Float4 ),	  \
		Intrinsic( name, HLSLBaseType_Min10Float,   HLSLBaseType_Min10Float  ),   \
        Intrinsic( name, HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float2 ),   \
        Intrinsic( name, HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float3 ),   \
        Intrinsic( name, HLSLBaseType_Min10Float4,  HLSLBaseType_Min10Float4 )

#define INTRINSIC_FLOAT2_FUNCTION(name) \
        Intrinsic( name, HLSLBaseType_Float,   HLSLBaseType_Float,   HLSLBaseType_Float  ),   \
        Intrinsic( name, HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float2 ),   \
        Intrinsic( name, HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3 ),   \
        Intrinsic( name, HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float4 ),   \
        Intrinsic( name, HLSLBaseType_Half,    HLSLBaseType_Half,    HLSLBaseType_Half   ),   \
        Intrinsic( name, HLSLBaseType_Half2,   HLSLBaseType_Half2,   HLSLBaseType_Half2  ),   \
        Intrinsic( name, HLSLBaseType_Half3,   HLSLBaseType_Half3,   HLSLBaseType_Half3  ),   \
        Intrinsic( name, HLSLBaseType_Half4,   HLSLBaseType_Half4,   HLSLBaseType_Half4  ),	  \
		Intrinsic( name, HLSLBaseType_Min16Float,   HLSLBaseType_Min16Float,  HLSLBaseType_Min16Float ),   \
        Intrinsic( name, HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2),   \
        Intrinsic( name, HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3 ),   \
        Intrinsic( name, HLSLBaseType_Min16Float4,  HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4 ),	  \
		Intrinsic( name, HLSLBaseType_Min10Float,   HLSLBaseType_Min10Float,  HLSLBaseType_Min10Float ),   \
        Intrinsic( name, HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2 ),   \
        Intrinsic( name, HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3 ),   \
        Intrinsic( name, HLSLBaseType_Min10Float4,  HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4 )

#define INTRINSIC_FLOAT3_FUNCTION(name) \
        Intrinsic( name, HLSLBaseType_Float,   HLSLBaseType_Float,   HLSLBaseType_Float,  HLSLBaseType_Float ),   \
        Intrinsic( name, HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float,  HLSLBaseType_Float2 ),  \
        Intrinsic( name, HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float,  HLSLBaseType_Float3 ),  \
        Intrinsic( name, HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float,  HLSLBaseType_Float4 ),  \
        Intrinsic( name, HLSLBaseType_Half,    HLSLBaseType_Half,    HLSLBaseType_Half,   HLSLBaseType_Half ),    \
        Intrinsic( name, HLSLBaseType_Half2,   HLSLBaseType_Half2,   HLSLBaseType_Half2,  HLSLBaseType_Half2 ),   \
        Intrinsic( name, HLSLBaseType_Half3,   HLSLBaseType_Half3,   HLSLBaseType_Half3,  HLSLBaseType_Half3 ),   \
        Intrinsic( name, HLSLBaseType_Half4,   HLSLBaseType_Half4,   HLSLBaseType_Half4,  HLSLBaseType_Half4 ),	  \
		Intrinsic( name, HLSLBaseType_Min16Float,   HLSLBaseType_Min16Float,  HLSLBaseType_Min16Float, HLSLBaseType_Min16Float ),   \
        Intrinsic( name, HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2),   \
        Intrinsic( name, HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3 ),   \
        Intrinsic( name, HLSLBaseType_Min16Float4,  HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4 ),	  \
		Intrinsic( name, HLSLBaseType_Min10Float,   HLSLBaseType_Min10Float,  HLSLBaseType_Min10Float, HLSLBaseType_Min10Float ),   \
        Intrinsic( name, HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2 ),   \
        Intrinsic( name, HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3 ),   \
        Intrinsic( name, HLSLBaseType_Min10Float4,  HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4 )

const Intrinsic _intrinsic[] = 
    {
        INTRINSIC_FLOAT1_FUNCTION( "abs" ),
        INTRINSIC_FLOAT1_FUNCTION( "acos" ),

        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float4 ),
		Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float2x2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float3x3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float4x4 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float4x3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Float4x2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half4 ),
		Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half2x2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half3x3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half4x4 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half4x3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Half4x2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Bool ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Int ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Int2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Int3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Int4 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Uint ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Uint2 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Uint3 ),
        Intrinsic( "any", HLSLBaseType_Bool, HLSLBaseType_Uint4 ),

        INTRINSIC_FLOAT1_FUNCTION( "asin" ),
        INTRINSIC_FLOAT1_FUNCTION( "atan" ),
        INTRINSIC_FLOAT2_FUNCTION( "atan2" ),
        INTRINSIC_FLOAT3_FUNCTION( "clamp" ),
        INTRINSIC_FLOAT1_FUNCTION( "cos" ),

        INTRINSIC_FLOAT3_FUNCTION( "lerp" ),
        INTRINSIC_FLOAT3_FUNCTION( "smoothstep" ),

		INTRINSIC_FLOAT1_FUNCTION( "round" ),
        INTRINSIC_FLOAT1_FUNCTION( "floor" ),
        INTRINSIC_FLOAT1_FUNCTION( "ceil" ),
        INTRINSIC_FLOAT1_FUNCTION( "frac" ),

        INTRINSIC_FLOAT2_FUNCTION( "fmod" ),

        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Float    ),
        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Float2   ),
        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Float3   ),
        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Float4   ),
        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Half     ),
        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Half2    ),
        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Half3    ),
        Intrinsic( "clip", HLSLBaseType_Void,  HLSLBaseType_Half4    ),

        Intrinsic( "dot", HLSLBaseType_Float,  HLSLBaseType_Float,   HLSLBaseType_Float  ),
        Intrinsic( "dot", HLSLBaseType_Float,  HLSLBaseType_Float2,  HLSLBaseType_Float2 ),
        Intrinsic( "dot", HLSLBaseType_Float,  HLSLBaseType_Float3,  HLSLBaseType_Float3 ),
        Intrinsic( "dot", HLSLBaseType_Float,  HLSLBaseType_Float4,  HLSLBaseType_Float4 ),
        Intrinsic( "dot", HLSLBaseType_Half,   HLSLBaseType_Half,    HLSLBaseType_Half   ),
        Intrinsic( "dot", HLSLBaseType_Half,   HLSLBaseType_Half2,   HLSLBaseType_Half2  ),
        Intrinsic( "dot", HLSLBaseType_Half,   HLSLBaseType_Half3,   HLSLBaseType_Half3  ),
        Intrinsic( "dot", HLSLBaseType_Half,   HLSLBaseType_Half4,   HLSLBaseType_Half4  ),

		Intrinsic( "distance", HLSLBaseType_Float,   HLSLBaseType_Float3,   HLSLBaseType_Float3),

        Intrinsic( "cross", HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3 ),


		INTRINSIC_VOID_FUNCTION("GroupMemoryBarrierWithGroupSync"),
		INTRINSIC_VOID_FUNCTION("GroupMemoryBarrier"),
		INTRINSIC_VOID_FUNCTION("DeviceMemoryBarrierWithGroupSync"),
		INTRINSIC_VOID_FUNCTION("DeviceMemoryBarrier"),
		INTRINSIC_VOID_FUNCTION("AllMemoryBarrierWithGroupSync"),
		INTRINSIC_VOID_FUNCTION("AllMemoryBarrier"),

        Intrinsic( "length", HLSLBaseType_Float,  HLSLBaseType_Float  ),
        Intrinsic( "length", HLSLBaseType_Float,  HLSLBaseType_Float2 ),
        Intrinsic( "length", HLSLBaseType_Float,  HLSLBaseType_Float3 ),
        Intrinsic( "length", HLSLBaseType_Float,  HLSLBaseType_Float4 ),
        Intrinsic( "length", HLSLBaseType_Half,   HLSLBaseType_Half   ),
        Intrinsic( "length", HLSLBaseType_Half,   HLSLBaseType_Half2  ),
        Intrinsic( "length", HLSLBaseType_Half,   HLSLBaseType_Half3  ),
        Intrinsic( "length", HLSLBaseType_Half,   HLSLBaseType_Half4  ),

		//Buffer.Load
		Intrinsic("Load", HLSLBaseType_Float,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float1x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float1x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float1x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float2x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float2x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float2x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float3x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float3x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float3x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float4x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float4x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float4x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Float4x4,  HLSLBaseType_Int),

		Intrinsic("Load", HLSLBaseType_Uint,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint1x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint1x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint1x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint2x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint2x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint2x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint3x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint3x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint3x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint4x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint4x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint4x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Uint4x4,  HLSLBaseType_Int),

		Intrinsic("Load", HLSLBaseType_Int,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int1x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int1x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int1x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int2x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int2x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int2x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int3x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int3x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int3x4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int4,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int4x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int4x2,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int4x3,  HLSLBaseType_Int),
		Intrinsic("Load", HLSLBaseType_Int4x4,  HLSLBaseType_Int),

		Intrinsic("Load", HLSLBaseType_UserDefined,  HLSLBaseType_Int),

		//Texture.Load
		//Texture1D, Texture2DMS
		Intrinsic("Load", HLSLBaseType_Float, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Float2, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Float3, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Float4, HLSLBaseType_Int2),

		Intrinsic("Load", HLSLBaseType_Uint, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Uint2, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Uint3, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Uint4, HLSLBaseType_Int2),

		Intrinsic("Load", HLSLBaseType_Int, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Int2, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Int3, HLSLBaseType_Int2),
		Intrinsic("Load", HLSLBaseType_Int4, HLSLBaseType_Int2),

		//Texture1DArray, Texture 2D, Texture2DMSArray
		Intrinsic("Load", HLSLBaseType_Float, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Float2, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Float3, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Float4, HLSLBaseType_Int3),

		Intrinsic("Load", HLSLBaseType_Uint, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Uint2, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Uint3, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Uint4, HLSLBaseType_Int3),

		Intrinsic("Load", HLSLBaseType_Int, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Int2, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Int3, HLSLBaseType_Int3),
		Intrinsic("Load", HLSLBaseType_Int4, HLSLBaseType_Int3),

		//Texture2DArray, Texture 3D
		Intrinsic("Load", HLSLBaseType_Float, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Float2, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Float3, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Float4, HLSLBaseType_Int4),

		Intrinsic("Load", HLSLBaseType_Uint, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Uint2, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Uint3, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Uint4, HLSLBaseType_Int4),

		Intrinsic("Load", HLSLBaseType_Int, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Int2, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Int3, HLSLBaseType_Int4),
		Intrinsic("Load", HLSLBaseType_Int4, HLSLBaseType_Int4),


        INTRINSIC_FLOAT2_FUNCTION( "max" ),
        INTRINSIC_FLOAT2_FUNCTION( "min" ),

        // @@ Add all combinations.
        
        
        // vector<N> = mul(vector<N>, scalar)
        // vector<N> = mul(vector<N>, vector<N>)

        // vector<N> = mul(matrix<N,M>, vector<M>)
        // matrix<N,N> = mul(matrix<N,M>, matrix<M,N>)
        
		// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509628(v=vs.85).aspx
		// Multiplies x and y using matrix math. The inner dimension x-columns and y-rows must be equal.
		// x : [in] The x input value.If x is a vector, it treated as a row vector.
		// y : [in] The y input value.If y is a vector, it treated as a column vector.
        
		INTRINSIC_FLOAT2_FUNCTION( "mul" ),

		// scalar = mul(scalar, scalar)
		Intrinsic("mul", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float),

		// vector<N> = mul(scalar, vector<N>)
		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float2),
		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Float3),
		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Float4),

		// vector<NxM> = mul(scalar, Matrix<NxM>)
		Intrinsic("mul", HLSLBaseType_Float2x2, HLSLBaseType_Float, HLSLBaseType_Float2x2),
		Intrinsic("mul", HLSLBaseType_Float2x3, HLSLBaseType_Float, HLSLBaseType_Float2x3),
		Intrinsic("mul", HLSLBaseType_Float2x4, HLSLBaseType_Float, HLSLBaseType_Float2x4),
		
		Intrinsic("mul", HLSLBaseType_Float3x2, HLSLBaseType_Float, HLSLBaseType_Float3x2),
		Intrinsic("mul", HLSLBaseType_Float3x3, HLSLBaseType_Float, HLSLBaseType_Float3x3),
		Intrinsic("mul", HLSLBaseType_Float3x4, HLSLBaseType_Float, HLSLBaseType_Float3x4),

		Intrinsic("mul", HLSLBaseType_Float4x2, HLSLBaseType_Float, HLSLBaseType_Float4x2),
		Intrinsic("mul", HLSLBaseType_Float4x3, HLSLBaseType_Float, HLSLBaseType_Float4x3),
		Intrinsic("mul", HLSLBaseType_Float4x4, HLSLBaseType_Float, HLSLBaseType_Float4x4),

		// vector<N> = mul(vector<N>, scalar)
		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float),
		
		// scalar = mul(vector<N>, vector<N>) !!!!
		Intrinsic("mul", HLSLBaseType_Float, HLSLBaseType_Float2, HLSLBaseType_Float2),
		Intrinsic("mul", HLSLBaseType_Float, HLSLBaseType_Float3, HLSLBaseType_Float3),
		Intrinsic("mul", HLSLBaseType_Float, HLSLBaseType_Float4, HLSLBaseType_Float4),

		// vector<N> = mul(vector<M>, matrix<M,N>) rows = same dimension(s) as input x, columns = any

		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2x2),
		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2x3),
		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float2, HLSLBaseType_Float2x4),

		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float3, HLSLBaseType_Float3x2),
		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3x3),
		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3x4),

		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float4, HLSLBaseType_Float4x2),
		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float4, HLSLBaseType_Float4x3),
		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float4x4),

		// matrix<MxN> = mul(matrix<MxN>, scalar)
		Intrinsic("mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x2, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x3, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x4, HLSLBaseType_Float),

		Intrinsic("mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x2, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x3, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x4, HLSLBaseType_Float),

		Intrinsic("mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x2, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x3, HLSLBaseType_Float),
		Intrinsic("mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x4, HLSLBaseType_Float),

		// vector<M> = mul(matrix<MxN>, vector<N>)
		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2),
		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float2x3, HLSLBaseType_Float3),
		Intrinsic("mul", HLSLBaseType_Float2, HLSLBaseType_Float2x4, HLSLBaseType_Float4),

		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float3x2, HLSLBaseType_Float2),
		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float3x3, HLSLBaseType_Float3),
		Intrinsic("mul", HLSLBaseType_Float3, HLSLBaseType_Float3x4, HLSLBaseType_Float4),

		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float4x2, HLSLBaseType_Float2),
		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float4x3, HLSLBaseType_Float3),
		Intrinsic("mul", HLSLBaseType_Float4, HLSLBaseType_Float4x4, HLSLBaseType_Float4),

		// vector<MxL> = mul(matrix<MxN>, matrix<NxL>)
		Intrinsic("mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x2, HLSLBaseType_Float2x2),
		Intrinsic("mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x2, HLSLBaseType_Float2x3),
		Intrinsic("mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x2, HLSLBaseType_Float2x4),
		Intrinsic("mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x3, HLSLBaseType_Float3x2),
		Intrinsic("mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x3, HLSLBaseType_Float3x3),
		Intrinsic("mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x3, HLSLBaseType_Float3x4),
		Intrinsic("mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x4, HLSLBaseType_Float4x2),
		Intrinsic("mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x4, HLSLBaseType_Float4x3),
		Intrinsic("mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x4, HLSLBaseType_Float4x4),

		Intrinsic("mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x2, HLSLBaseType_Float2x2),
		Intrinsic("mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x2, HLSLBaseType_Float2x3),
		Intrinsic("mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x2, HLSLBaseType_Float2x4),
		Intrinsic("mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x3, HLSLBaseType_Float3x2),
		Intrinsic("mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x3, HLSLBaseType_Float3x3),
		Intrinsic("mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x3, HLSLBaseType_Float3x4),
		Intrinsic("mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x4, HLSLBaseType_Float4x2),
		Intrinsic("mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x4, HLSLBaseType_Float4x3),
		Intrinsic("mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x4, HLSLBaseType_Float4x4),

		Intrinsic("mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x2, HLSLBaseType_Float2x2),
		Intrinsic("mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x2, HLSLBaseType_Float2x3),
		Intrinsic("mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x2, HLSLBaseType_Float2x4),
		Intrinsic("mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x3, HLSLBaseType_Float3x2),
		Intrinsic("mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x3, HLSLBaseType_Float3x3),
		Intrinsic("mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x3, HLSLBaseType_Float3x4),
		Intrinsic("mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x4, HLSLBaseType_Float4x2),
		Intrinsic("mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x4, HLSLBaseType_Float4x3),
		Intrinsic("mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x4, HLSLBaseType_Float4x4),			

		Intrinsic( "transpose", HLSLBaseType_Float2x2, HLSLBaseType_Float2x2 ),
        Intrinsic( "transpose", HLSLBaseType_Float3x3, HLSLBaseType_Float3x3 ),
        Intrinsic( "transpose", HLSLBaseType_Float4x4, HLSLBaseType_Float4x4 ),

        INTRINSIC_FLOAT1_FUNCTION( "normalize" ),
        INTRINSIC_FLOAT2_FUNCTION( "pow" ),
        INTRINSIC_FLOAT1_FUNCTION( "saturate" ),
        INTRINSIC_FLOAT1_FUNCTION( "sin" ),
        INTRINSIC_FLOAT1_FUNCTION( "sqrt" ),
        INTRINSIC_FLOAT1_FUNCTION( "rsqrt" ),
        INTRINSIC_FLOAT1_FUNCTION( "rcp" ),
        INTRINSIC_FLOAT1_FUNCTION( "exp" ),
        INTRINSIC_FLOAT1_FUNCTION( "exp2" ),
        INTRINSIC_FLOAT1_FUNCTION( "log" ),
        INTRINSIC_FLOAT1_FUNCTION( "log2" ),
        
        INTRINSIC_FLOAT1_FUNCTION( "ddx" ),
        INTRINSIC_FLOAT1_FUNCTION( "ddy" ),
        
        INTRINSIC_FLOAT1_FUNCTION( "sign" ),
        INTRINSIC_FLOAT2_FUNCTION( "step" ),
        INTRINSIC_FLOAT2_FUNCTION( "reflect" ),

		INTRINSIC_FLOAT1_FUNCTION("isnan"),
		INTRINSIC_FLOAT1_FUNCTION("isinf"),

		Intrinsic("asuint",    HLSLBaseType_Uint, HLSLBaseType_Float),
        Intrinsic("tex2D",     HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2),
        Intrinsic("tex2Dproj", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4),
        Intrinsic("tex2Dlod",  HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4),
        Intrinsic("tex2Dlod",  HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4, HLSLBaseType_Int2),   // With offset.
        Intrinsic("tex2Dbias", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4),
        Intrinsic("tex2Dgrad", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2),
        Intrinsic("tex2Dgather", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Int),
        Intrinsic("tex2Dgather", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Int2, HLSLBaseType_Int),    // With offset.
        Intrinsic("tex2Dsize", HLSLBaseType_Int2, HLSLBaseType_Sampler2D),
        Intrinsic("tex2Dfetch", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Int3),    // u,v,mipmap

        Intrinsic("tex2Dcmp", HLSLBaseType_Float4, HLSLBaseType_Sampler2DShadow, HLSLBaseType_Float4),                // @@ IC: This really takes a float3 (uvz) and returns a float.

        Intrinsic("tex2DMSfetch", HLSLBaseType_Float4, HLSLBaseType_Sampler2DMS, HLSLBaseType_Int2, HLSLBaseType_Int),
        Intrinsic("tex2DMSsize", HLSLBaseType_Int3, HLSLBaseType_Sampler2DMS),

        Intrinsic("tex2DArray", HLSLBaseType_Float4, HLSLBaseType_Sampler2DArray, HLSLBaseType_Float3),

        Intrinsic("tex3D",     HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float3),
        Intrinsic("tex3Dlod",  HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float4),
        Intrinsic("tex3Dbias", HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float4),
        Intrinsic("tex3Dsize", HLSLBaseType_Int3, HLSLBaseType_Sampler3D),

		Intrinsic("Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float),
		Intrinsic("Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2),
		Intrinsic("Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3),
		Intrinsic("Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4),

		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float),
		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int),
		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float),
		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int),
		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float),
		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int),
		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float),
		Intrinsic("SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int),

		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float),
		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int),
		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float),
		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int),
		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float),
		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int),
		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float),
		Intrinsic("SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int),

        Intrinsic("texCUBE",    HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float3),
        Intrinsic("texCUBElod", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float4),
        Intrinsic("texCUBEbias", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float4),
        Intrinsic("texCUBEsize", HLSLBaseType_Int, HLSLBaseType_SamplerCube),

		//GLSL doesn't support float type atomic functions, thus resticted Interlocked functions' data type
		Intrinsic("InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),

		Intrinsic("InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),

		Intrinsic("InterlockedCompareStore", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedCompareStore", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),

		Intrinsic("InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),

		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint),

		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint),

		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint),

		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int),
		Intrinsic("InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint),


        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Float,   HLSLBaseType_Float,  HLSLBaseType_Float ),
        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Float2,  HLSLBaseType_Float,  HLSLBaseType_Float2 ),
        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Float3,  HLSLBaseType_Float,  HLSLBaseType_Float3 ),
        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Float4,  HLSLBaseType_Float,  HLSLBaseType_Float4 ),
        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Half,    HLSLBaseType_Half,   HLSLBaseType_Half ),
        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Half2,   HLSLBaseType_Half2,  HLSLBaseType_Half2 ),
        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Half3,   HLSLBaseType_Half3,  HLSLBaseType_Half3 ),
        Intrinsic( "sincos", HLSLBaseType_Void,  HLSLBaseType_Half4,   HLSLBaseType_Half4,  HLSLBaseType_Half4 ),

        Intrinsic( "mad", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float ),
        Intrinsic( "mad", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2 ),
        Intrinsic( "mad", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3 ),
        Intrinsic( "mad", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float4 ),
        Intrinsic( "mad", HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half ),
        Intrinsic( "mad", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2 ),
        Intrinsic( "mad", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3 ),
        Intrinsic( "mad", HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half4 ),
    };

const int _numIntrinsics = sizeof(_intrinsic) / sizeof(Intrinsic);

// The order in this array must match up with HLSLBinaryOp
const int _binaryOpPriority[] =
    {
        2, 1, //  &&, ||
        8, 8, //  +,  -
        9, 9, //  *,  /
        7, 7, //  <,  >,
        7, 7, //  <=, >=,
        6, 6, //  ==, !=
        5, 3, 4, // &, |, ^
    };

const BaseTypeDescription _baseTypeDescriptions[HLSLBaseType_Count] = 
    {
        { "unknown type",       NumericType_NaN,        0, 0, 0, -1 },      // HLSLBaseType_Unknown
        { "void",               NumericType_NaN,        0, 0, 0, -1 },      // HLSLBaseType_Void

        { "float",              NumericType_Float,      1, 0, 1,  0 },      // HLSLBaseType_Float
		{ "float1x2",           NumericType_Float,      1, 2, 2,  0 },      // HLSLBaseType_Float1x2
		{ "float1x3",           NumericType_Float,      1, 2, 3,  0 },      // HLSLBaseType_Float1x3
		{ "float1x4",           NumericType_Float,      1, 2, 4,  0 },      // HLSLBaseType_Float1x4
        { "float2",             NumericType_Float,      2, 1, 1,  0 },      // HLSLBaseType_Float2
		{ "float2x2",           NumericType_Float,      2, 2, 2,  0 },      // HLSLBaseType_Float2x2
		{ "float2x3",           NumericType_Float,      2, 2, 3,  0 },      // HLSLBaseType_Float2x3
		{ "float2x4",           NumericType_Float,      2, 2, 4,  0 },      // HLSLBaseType_Float2x4
        { "float3",             NumericType_Float,      3, 1, 1,  0 },      // HLSLBaseType_Float3
		{ "float3x2",           NumericType_Float,      3, 2, 2,  0 },      // HLSLBaseType_Float3x2
		{ "float3x3",           NumericType_Float,      3, 2, 3,  0 },      // HLSLBaseType_Float3x3
		{ "float3x4",           NumericType_Float,      3, 2, 4,  0 },      // HLSLBaseType_Float3x4
        { "float4",             NumericType_Float,      4, 1, 1,  0 },      // HLSLBaseType_Float4
		{ "float4x2",           NumericType_Float,      4, 2, 2,  0 },      // HLSLBaseType_Float4x2
		{ "float4x3",           NumericType_Float,      4, 2, 3,  0 },      // HLSLBaseType_Float4x3
		{ "float4x4",           NumericType_Float,      4, 2, 4,  0 },      // HLSLBaseType_Float4x4


        { "half",               NumericType_Half,      1, 0, 1,  1 },      // HLSLBaseType_Half
		{ "half1x2",            NumericType_Half,      1, 2, 2,  1 },      // HLSLBaseType_Half1x2
		{ "half1x3",            NumericType_Half,      1, 2, 3,  1 },      // HLSLBaseType_Half1x3
		{ "half1x4",            NumericType_Half,      1, 2, 4,  1 },      // HLSLBaseType_Half1x4
        { "half2",              NumericType_Half,      2, 1, 1,  1 },      // HLSLBaseType_Half2
		{ "half2x2",            NumericType_Half,      2, 2, 2,  1 },      // HLSLBaseType_Half2x2
		{ "half2x3",            NumericType_Half,      2, 2, 3,  1 },      // HLSLBaseType_Half2x3
		{ "half2x4",            NumericType_Half,      2, 2, 4,  1 },      // HLSLBaseType_Half2x4
        { "half3",              NumericType_Half,      3, 1, 1,  1 },      // HLSLBaseType_Half3
		{ "half3x2",            NumericType_Half,      3, 2, 2,  1 },      // HLSLBaseType_Half3x2
		{ "half3x3",            NumericType_Half,      3, 2, 3,  1 },      // HLSLBaseType_Half3x3
		{ "half3x4",            NumericType_Half,      3, 2, 4,  1 },      // HLSLBaseType_Half3x4
        { "half4",              NumericType_Half,      4, 1, 1,  1 },      // HLSLBaseType_Half4
		{ "half4x2",            NumericType_Half,      4, 2, 2,  1 },      // HLSLBaseType_Half4x2
		{ "half4x3",            NumericType_Half,      4, 2, 3,  1 },      // HLSLBaseType_Half4x3
		{ "half4x4",            NumericType_Half,      4, 2, 4,  1 },      // HLSLBaseType_Half4x4


		{ "min16float",              NumericType_Min16Float,      1, 0, 1,  1 },      // HLSLBaseType_Float
		{ "min16float1x2",           NumericType_Min16Float,      1, 2, 2,  1 },      // HLSLBaseType_Float1x2
		{ "min16float1x3",           NumericType_Min16Float,      1, 2, 3,  1 },      // HLSLBaseType_Float1x3
		{ "min16float1x4",           NumericType_Min16Float,      1, 2, 4,  1 },      // HLSLBaseType_Float1x4
        { "min16float2",             NumericType_Min16Float,      2, 1, 1,  1 },      // HLSLBaseType_Float2
		{ "min16float2x2",           NumericType_Min16Float,      2, 2, 2,  1 },      // HLSLBaseType_Float2x2
		{ "min16float2x3",           NumericType_Min16Float,      2, 2, 3,  1 },      // HLSLBaseType_Float2x3
		{ "min16float2x4",           NumericType_Min16Float,      2, 2, 4,  1 },      // HLSLBaseType_Float2x4
        { "min16float3",             NumericType_Min16Float,      3, 1, 1,  1 },      // HLSLBaseType_Float3
		{ "min16float3x2",           NumericType_Min16Float,      3, 2, 2,  1 },      // HLSLBaseType_Float3x2
		{ "min16float3x3",           NumericType_Min16Float,      3, 2, 3,  1 },      // HLSLBaseType_Float3x3
		{ "min16float3x4",           NumericType_Min16Float,      3, 2, 4,  1 },      // HLSLBaseType_Float3x4
        { "min16float4",             NumericType_Min16Float,      4, 1, 1,  1 },      // HLSLBaseType_Float4
		{ "min16float4x2",           NumericType_Min16Float,      4, 2, 2,  1 },      // HLSLBaseType_Float4x2
		{ "min16float4x3",           NumericType_Min16Float,      4, 2, 3,  1 },      // HLSLBaseType_Float4x3
		{ "min16float4x4",           NumericType_Min16Float,      4, 2, 4,  1 },      // HLSLBaseType_Float4x4


		{ "min10float",              NumericType_Min10Float,      1, 0, 1,  1 },      // HLSLBaseType_Float
		{ "min10float1x2",           NumericType_Min10Float,      1, 2, 2,  1 },      // HLSLBaseType_Float1x2
		{ "min10float1x3",           NumericType_Min10Float,      1, 2, 3,  1 },      // HLSLBaseType_Float1x3
		{ "min10float1x4",           NumericType_Min10Float,      1, 2, 4,  1 },      // HLSLBaseType_Float1x4
        { "min10float2",             NumericType_Min10Float,      2, 1, 1,  1 },      // HLSLBaseType_Float2
		{ "min10float2x2",           NumericType_Min10Float,      2, 2, 2,  1 },      // HLSLBaseType_Float2x2
		{ "min10float2x3",           NumericType_Min10Float,      2, 2, 3,  1 },      // HLSLBaseType_Float2x3
		{ "min10float2x4",           NumericType_Min10Float,      2, 2, 4,  1 },      // HLSLBaseType_Float2x4
        { "min10float3",             NumericType_Min10Float,      3, 1, 1,  1 },      // HLSLBaseType_Float3
		{ "min10float3x2",           NumericType_Min10Float,      3, 2, 2,  1 },      // HLSLBaseType_Float3x2
		{ "min10float3x3",           NumericType_Min10Float,      3, 2, 3,  1 },      // HLSLBaseType_Float3x3
		{ "min10float3x4",           NumericType_Min10Float,      3, 2, 4,  1 },      // HLSLBaseType_Float3x4
        { "min10float4",             NumericType_Min10Float,      4, 1, 1,  1 },      // HLSLBaseType_Float4
		{ "min10float4x2",           NumericType_Min10Float,      4, 2, 2,  1 },      // HLSLBaseType_Float4x2
		{ "min10float4x3",           NumericType_Min10Float,      4, 2, 3,  1 },      // HLSLBaseType_Float4x3
		{ "min10float4x4",           NumericType_Min10Float,      4, 2, 4,  1 },      // HLSLBaseType_Float4x4

        { "bool",               NumericType_Bool,      1, 0, 1,  4 },      // HLSLBaseType_Bool
		{ "bool1x2",            NumericType_Bool,      1, 2, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool1x3",            NumericType_Bool,      1, 2, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool1x4",            NumericType_Bool,      1, 2, 4,  4 },      // HLSLBaseType_Bool1x4
		{ "bool2",				NumericType_Bool,	   2, 1, 1,  4 },      // HLSLBaseType_Bool2
		{ "bool2x2",            NumericType_Bool,      2, 2, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool2x3",            NumericType_Bool,      2, 2, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool2x4",            NumericType_Bool,      2, 2, 4,  4 },      // HLSLBaseType_Bool1x4
		{ "bool3",				NumericType_Bool,	   3, 1, 1,  4 },      // HLSLBaseType_Bool3
		{ "bool3x2",            NumericType_Bool,      3, 2, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool3x3",            NumericType_Bool,      3, 2, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool3x4",            NumericType_Bool,      3, 2, 4,  4 },      // HLSLBaseType_Bool1x4
		{ "bool4",				NumericType_Bool,	   4, 1, 1,  4 },      // HLSLBaseType_Bool4
		{ "bool4x2",            NumericType_Bool,      4, 2, 2,  4 },      // HLSLBaseType_Bool1x2
		{ "bool4x3",            NumericType_Bool,      4, 2, 3,  4 },      // HLSLBaseType_Bool1x3
		{ "bool4x4",            NumericType_Bool,      4, 2, 4,  4 },      // HLSLBaseType_Bool1x4

        { "int",                NumericType_Int,       1, 0, 1,  3 },      // HLSLBaseType_Int
		{ "int1x2",             NumericType_Int,	   1, 2, 2,  3 },      // HLSLBaseType_Int1x2
		{ "int1x3",             NumericType_Int,	   1, 2, 3,  3 },      // HLSLBaseType_Int1x3
		{ "int1x4",             NumericType_Int,	   1, 2, 4,  3 },      // HLSLBaseType_Int1x4
        { "int2",               NumericType_Int,       2, 1, 1,  3 },      // HLSLBaseType_Int2
		{ "int2x2",             NumericType_Int,	   2, 2, 2,  3 },      // HLSLBaseType_Int2x2
		{ "int2x3",             NumericType_Int,	   2, 2, 3,  3 },      // HLSLBaseType_Int2x3
		{ "int2x4",             NumericType_Int,	   2, 2, 4,  3 },      // HLSLBaseType_Int2x4
        { "int3",               NumericType_Int,       3, 1, 1,  3 },      // HLSLBaseType_Int3
		{ "int3x2",             NumericType_Int,	   3, 2, 2,  3 },      // HLSLBaseType_Int3x2
		{ "int3x3",             NumericType_Int,	   3, 2, 3,  3 },      // HLSLBaseType_Int3x3
		{ "int3x4",             NumericType_Int,	   3, 2, 4,  3 },      // HLSLBaseType_Int3x4
        { "int4",               NumericType_Int,       4, 1, 1,  3 },      // HLSLBaseType_Int4
		{ "int4x2",             NumericType_Int,	   4, 2, 2,  3 },      // HLSLBaseType_Int4x2
		{ "int4x3",             NumericType_Int,	   4, 2, 3,  3 },      // HLSLBaseType_Int4x3
		{ "int4x4",             NumericType_Int,	   4, 2, 4,  3 },      // HLSLBaseType_Int4x4
		
        { "uint",               NumericType_Uint,      1, 0, 1,  2 },      // HLSLBaseType_Uint
		{ "uint1x2",            NumericType_Uint,	   1, 2, 2,  2 },      // HLSLBaseType_Int1x2
		{ "uint1x3",            NumericType_Uint,	   1, 2, 3,  2 },      // HLSLBaseType_Int1x3
		{ "uint1x4",            NumericType_Uint,	   1, 2, 4,  2 },      // HLSLBaseType_Int1x4
        { "uint2",              NumericType_Uint,      2, 1, 1,  2 },      // HLSLBaseType_Uint2
		{ "uint2x2",            NumericType_Uint,	   1, 2, 2,  2 },      // HLSLBaseType_Uint1x2
		{ "uint2x3",            NumericType_Uint,	   1, 2, 3,  2 },      // HLSLBaseType_Uint1x3
		{ "uint2x4",            NumericType_Uint,	   1, 2, 4,  2 },      // HLSLBaseType_Uint1x4
        { "uint3",              NumericType_Uint,      3, 1, 1,  2 },      // HLSLBaseType_Uint3
		{ "uint3x2",            NumericType_Uint,	   1, 2, 2,  2 },      // HLSLBaseType_Uint1x2
		{ "uint3x3",            NumericType_Uint,	   1, 2, 3,  2 },      // HLSLBaseType_Uint1x3
		{ "uint3x4",            NumericType_Uint,	   1, 2, 4,  2 },      // HLSLBaseType_Uint1x4
        { "uint4",              NumericType_Uint,      4, 1, 1,  2 },      // HLSLBaseType_Uint4
		{ "uint4x2",            NumericType_Uint,	   1, 2, 2,  2 },      // HLSLBaseType_Uint1x2
		{ "uint4x3",            NumericType_Uint,	   1, 2, 3,  2 },      // HLSLBaseType_Uint1x3
		{ "uint4x4",            NumericType_Uint,	   1, 2, 4,  2 },      // HLSLBaseType_Uint1x4


        { "texture",            NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture
		{ "Texture1D",          NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture1D
		{ "Texture1DArray",     NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture1DArray
		{ "Texture2D",          NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture2D
		{ "Texture2DArray",     NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture2DArray
		{ "Texture3D",          NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture3D
		{ "Texture2DMS",        NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture2DMS
		{ "Texture2DMSArray",   NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Texture2DMSArray
		{ "TextureCube",        NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_TextureCube
		{ "TextureCubeArray",   NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_TextureCubeArray

		{ "RWTexture1D",          NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_RWTexture1D
		{ "RWTexture1DArray",     NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_RWTexture1DArray
		{ "RWTexture2D",          NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_RWTexture2D
		{ "RWTexture2DArray",     NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_RWTexture2DArray
		{ "RWTexture3D",          NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_RWTexture3D


        { "sampler",            NumericType_NaN,        1, 0, 0, -1 },      // HLSLBaseType_Sampler
        { "sampler2D",          NumericType_NaN,        1, 0, 0, -1 },      // HLSLBaseType_Sampler2D
        { "sampler3D",          NumericType_NaN,        1, 0, 0, -1 },      // HLSLBaseType_Sampler3D
        { "samplerCUBE",        NumericType_NaN,        1, 0, 0, -1 },      // HLSLBaseType_SamplerCube
        { "sampler2DShadow",    NumericType_NaN,        1, 0, 0, -1 },      // HLSLBaseType_Sampler2DShadow
        { "sampler2DMS",        NumericType_NaN,        1, 0, 0, -1 },      // HLSLBaseType_Sampler2DMS
        { "sampler2DArray",     NumericType_NaN,        1, 0, 0, -1 },      // HLSLBaseType_Sampler2DArray
        { "user defined",       NumericType_NaN,        1, 0, 0, -1 }       // HLSLBaseType_UserDefined
    };

// IC: I'm not sure this table is right, but any errors should be caught by the backend compiler.
// Also, this is operator dependent. The type resulting from (float4 * float4x4) is not the same as (float4 + float4x4).
// We should probably distinguish between component-wise operator and only allow same dimensions
HLSLBaseType _binaryOpTypeLookup[HLSLBaseType_NumericCount][HLSLBaseType_NumericCount] = 
    {
        {   // float 
            HLSLBaseType_Float, HLSLBaseType_Float1x2, HLSLBaseType_Float1x3, HLSLBaseType_Float1x4,
			HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2x3, HLSLBaseType_Float2x4,
			HLSLBaseType_Float3, HLSLBaseType_Float3x2, HLSLBaseType_Float3x3, HLSLBaseType_Float3x4,
			HLSLBaseType_Float4, HLSLBaseType_Float4x2, HLSLBaseType_Float4x3, HLSLBaseType_Float4x4,

			HLSLBaseType_Float, HLSLBaseType_Float1x2, HLSLBaseType_Float1x3, HLSLBaseType_Float1x4,
			HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2x3, HLSLBaseType_Float2x4,
			HLSLBaseType_Float3, HLSLBaseType_Float3x2, HLSLBaseType_Float3x3, HLSLBaseType_Float3x4,
			HLSLBaseType_Float4, HLSLBaseType_Float4x2, HLSLBaseType_Float4x3, HLSLBaseType_Float4x4,
			
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // float 1x2
			HLSLBaseType_Float1x2, HLSLBaseType_Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float1x2, HLSLBaseType_Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x3
			HLSLBaseType_Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x4
			HLSLBaseType_Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

        {   // float2
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // float 2x2
			HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x3
			HLSLBaseType_Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x4
			HLSLBaseType_Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

	   {   // float3
		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
	   },
		{   // float 3x2
			HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x3
			HLSLBaseType_Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x4
			HLSLBaseType_Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float4
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown

		},
		{   // float 4x2
			HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x3
			HLSLBaseType_Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x4
			HLSLBaseType_Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x4,

			HLSLBaseType_Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},


        {   // half
			HLSLBaseType_Float, HLSLBaseType_Float1x2, HLSLBaseType_Float1x3, HLSLBaseType_Float1x4,
			HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2x3, HLSLBaseType_Float2x4,
			HLSLBaseType_Float3, HLSLBaseType_Float3x2, HLSLBaseType_Float3x3, HLSLBaseType_Float3x4,
			HLSLBaseType_Float4, HLSLBaseType_Float4x2, HLSLBaseType_Float4x3, HLSLBaseType_Float4x4,

			HLSLBaseType_Half, HLSLBaseType_Half1x2, HLSLBaseType_Half1x3, HLSLBaseType_Half1x4,
			HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half2x4,
			HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4,
			HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // half 1x2
			HLSLBaseType_Float1x2, HLSLBaseType_Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x2, HLSLBaseType_Half1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half 1x3
			HLSLBaseType_Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Half1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half 1x4
			HLSLBaseType_Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

        {   // half2
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // half2x2
			HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half2x3
			HLSLBaseType_Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half2x4
			HLSLBaseType_Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
        {   // half3
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // half3x2
			HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half3x3
			HLSLBaseType_Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half3x4
			HLSLBaseType_Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
        {   // half4
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // half4x2
			HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half4x3
			HLSLBaseType_Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // half4x4
			HLSLBaseType_Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x4,

			HLSLBaseType_Half4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
	

		{   // min16float 
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float, HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x3, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Min16Float, HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x3, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // float 1x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,			

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

        {   // float2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,			

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // float 2x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,			

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

	   {   // float3
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   
		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		  

		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
	   },
		{   // float 3x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			
			HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			
			HLSLBaseType_Min16Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown

		},
		{   // float 4x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown,

		

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Min16Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},


		{   // min10float 
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float, HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x3, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Min10Float, HLSLBaseType_Min10Float1x2, HLSLBaseType_Min10Float1x3, HLSLBaseType_Min10Float1x4,
			HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float2x4,
			HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x4,
			HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float1x2, HLSLBaseType_Min10Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,


			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min10Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

	   {   // float3
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,



		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
	   },
		{   // float 3x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,



			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,



			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,


			HLSLBaseType_Min16Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,


			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,



			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown

		},
		{   // float 4x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,



			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x3, HLSLBaseType_Unknown,



			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Min10Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},




        {   // bool
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool, HLSLBaseType_Bool1x2, HLSLBaseType_Bool1x3, HLSLBaseType_Bool1x4,
			HLSLBaseType_Bool2, HLSLBaseType_Bool2x2, HLSLBaseType_Bool2x3, HLSLBaseType_Bool2x4,
			HLSLBaseType_Bool3, HLSLBaseType_Bool3x2, HLSLBaseType_Bool3x3, HLSLBaseType_Bool3x4,
			HLSLBaseType_Bool4, HLSLBaseType_Bool4x2, HLSLBaseType_Bool4x3, HLSLBaseType_Bool4x4,

			HLSLBaseType_Int, HLSLBaseType_Int1x2, HLSLBaseType_Int1x3, HLSLBaseType_Int1x4,
			HLSLBaseType_Int2, HLSLBaseType_Int2x2, HLSLBaseType_Int2x3, HLSLBaseType_Int2x4,
			HLSLBaseType_Int3, HLSLBaseType_Int3x2, HLSLBaseType_Int3x3, HLSLBaseType_Int3x4,
			HLSLBaseType_Int4, HLSLBaseType_Int4x2, HLSLBaseType_Int4x3, HLSLBaseType_Int4x4,

			HLSLBaseType_Uint, HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x3, HLSLBaseType_Uint1x4,
			HLSLBaseType_Uint2, HLSLBaseType_Uint2x2, HLSLBaseType_Uint2x3, HLSLBaseType_Uint2x4,
			HLSLBaseType_Uint3, HLSLBaseType_Uint3x2, HLSLBaseType_Uint3x3, HLSLBaseType_Uint3x4,
			HLSLBaseType_Uint4, HLSLBaseType_Uint4x2, HLSLBaseType_Uint4x3, HLSLBaseType_Uint4x4
        },

		{   // bool1x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool1x2, HLSLBaseType_Bool1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x2, HLSLBaseType_Int1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

			{   // bool1x3
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Bool1x3, HLSLBaseType_Unknown, HLSLBaseType_Bool1x3, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Int1x3, HLSLBaseType_Unknown, HLSLBaseType_Int1x3, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Uint1x3, HLSLBaseType_Unknown, HLSLBaseType_Uint1x3, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
			},
		{   // bool1x4
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Bool1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Bool1x4,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Int1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int1x4,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

				HLSLBaseType_Uint1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint1x4,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
				HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
        {   // bool2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Bool2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
        },
		{   // bool2x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Bool2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // bool2x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Bool2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // bool2x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Bool2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // bool3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Bool3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // bool3x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Bool3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // bool3x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Bool3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // bool3x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Bool3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // bool4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Bool4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
				{   // bool4x2
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

					HLSLBaseType_Bool4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Bool4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

					HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

					HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
					HLSLBaseType_Unknown, HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown
				},
		{   // bool4x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Bool4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x3, HLSLBaseType_Unknown
		},
		{   // bool4x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Bool4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Bool4x4,

			HLSLBaseType_Int4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x4,

			HLSLBaseType_Uint4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x4
		},
        {   // int
			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int, HLSLBaseType_Int1x2, HLSLBaseType_Int1x3, HLSLBaseType_Int1x4,
			HLSLBaseType_Int2, HLSLBaseType_Int2x2, HLSLBaseType_Int2x3, HLSLBaseType_Int2x4,
			HLSLBaseType_Int3, HLSLBaseType_Int3x2, HLSLBaseType_Int3x3, HLSLBaseType_Int3x4,
			HLSLBaseType_Int4, HLSLBaseType_Int4x2, HLSLBaseType_Int4x3, HLSLBaseType_Int4x4,

			HLSLBaseType_Int, HLSLBaseType_Int1x2, HLSLBaseType_Int1x3, HLSLBaseType_Int1x4,
			HLSLBaseType_Int2, HLSLBaseType_Int2x2, HLSLBaseType_Int2x3, HLSLBaseType_Int2x4,
			HLSLBaseType_Int3, HLSLBaseType_Int3x2, HLSLBaseType_Int3x3, HLSLBaseType_Int3x4,
			HLSLBaseType_Int4, HLSLBaseType_Int4x2, HLSLBaseType_Int4x3, HLSLBaseType_Int4x4,

			HLSLBaseType_Int, HLSLBaseType_Int1x2, HLSLBaseType_Int1x3, HLSLBaseType_Int1x4,
			HLSLBaseType_Int2, HLSLBaseType_Int2x2, HLSLBaseType_Int2x3, HLSLBaseType_Int2x4,
			HLSLBaseType_Int3, HLSLBaseType_Int3x2, HLSLBaseType_Int3x3, HLSLBaseType_Int3x4,
			HLSLBaseType_Int4, HLSLBaseType_Int4x2, HLSLBaseType_Int4x3, HLSLBaseType_Int4x4
        },
		{   // int1x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x2, HLSLBaseType_Int1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x2, HLSLBaseType_Int1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x2, HLSLBaseType_Int1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int1x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x3, HLSLBaseType_Unknown, HLSLBaseType_Int1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x3, HLSLBaseType_Unknown, HLSLBaseType_Int1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x3, HLSLBaseType_Unknown, HLSLBaseType_Int1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int1x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},


		{   // int2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int2x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int2x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int2x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // int3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int3x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int3x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int3x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // int4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Int4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int4x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Int4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // int4x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x3, HLSLBaseType_Unknown
		},
		{   // int4x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Int4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x4,

			HLSLBaseType_Int4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x4,

			HLSLBaseType_Int4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Int4x4
		},
		{   // uint
			HLSLBaseType_Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint, HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x3, HLSLBaseType_Uint1x4,
			HLSLBaseType_Uint2, HLSLBaseType_Uint2x2, HLSLBaseType_Uint2x3, HLSLBaseType_Uint2x4,
			HLSLBaseType_Uint3, HLSLBaseType_Uint3x2, HLSLBaseType_Uint3x3, HLSLBaseType_Uint3x4,
			HLSLBaseType_Uint4, HLSLBaseType_Uint4x2, HLSLBaseType_Uint4x3, HLSLBaseType_Uint4x4,

			HLSLBaseType_Uint, HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x3, HLSLBaseType_Uint1x4,
			HLSLBaseType_Uint2, HLSLBaseType_Uint2x2, HLSLBaseType_Uint2x3, HLSLBaseType_Uint2x4,
			HLSLBaseType_Uint3, HLSLBaseType_Uint3x2, HLSLBaseType_Uint3x3, HLSLBaseType_Uint3x4,
			HLSLBaseType_Uint4, HLSLBaseType_Uint4x2, HLSLBaseType_Uint4x3, HLSLBaseType_Uint4x4,

			HLSLBaseType_Uint, HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x3, HLSLBaseType_Uint1x4,
			HLSLBaseType_Uint2, HLSLBaseType_Uint2x2, HLSLBaseType_Uint2x3, HLSLBaseType_Uint2x4,
			HLSLBaseType_Uint3, HLSLBaseType_Uint3x2, HLSLBaseType_Uint3x3, HLSLBaseType_Uint3x4,
			HLSLBaseType_Uint4, HLSLBaseType_Uint4x2, HLSLBaseType_Uint4x3, HLSLBaseType_Uint4x4
		},
		{   // uUint1x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x2, HLSLBaseType_Uint1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint1x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x3, HLSLBaseType_Unknown, HLSLBaseType_Uint1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x3, HLSLBaseType_Unknown, HLSLBaseType_Uint1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x3, HLSLBaseType_Unknown, HLSLBaseType_Uint1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint1x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},


		{   // uUint2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint2x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint2x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint2x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // uUint3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint3x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint3x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint3x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // uUint4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Uint4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint4x2
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Uint4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // uUint4x3
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x3, HLSLBaseType_Unknown
		},
		{   // uUint4x4
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Uint4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x4,

			HLSLBaseType_Uint4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x4,

			HLSLBaseType_Uint4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Uint4x4
		},
    };

// Priority of the ? : operator.
const int _conditionalOpPriority = 1;

static const char* GetTypeName(const HLSLType& type)
{
    if (type.baseType == HLSLBaseType_UserDefined)
    {
        return type.typeName;
    }
    else
    {
        return _baseTypeDescriptions[type.baseType].typeName;
    }
}

static const char* GetBinaryOpName(HLSLBinaryOp binaryOp)
{
    switch (binaryOp)
    {
    case HLSLBinaryOp_And:          return "&&";
    case HLSLBinaryOp_Or:           return "||";
    case HLSLBinaryOp_Add:          return "+";
    case HLSLBinaryOp_Sub:          return "-";
    case HLSLBinaryOp_Mul:          return "*";
    case HLSLBinaryOp_Div:          return "/";
    case HLSLBinaryOp_Less:         return "<";
    case HLSLBinaryOp_Greater:      return ">";
    case HLSLBinaryOp_LessEqual:    return "<=";
    case HLSLBinaryOp_GreaterEqual: return ">=";
    case HLSLBinaryOp_Equal:        return "==";
    case HLSLBinaryOp_NotEqual:     return "!=";
    case HLSLBinaryOp_BitAnd:       return "&";
    case HLSLBinaryOp_BitOr:        return "|";
    case HLSLBinaryOp_BitXor:       return "^";
    case HLSLBinaryOp_Assign:       return "=";
    case HLSLBinaryOp_AddAssign:    return "+=";
    case HLSLBinaryOp_SubAssign:    return "-=";
    case HLSLBinaryOp_MulAssign:    return "*=";
    case HLSLBinaryOp_DivAssign:    return "/=";
    default:
        ASSERT(0);
        return "???";
    }
}


/*
 * 1.) Match
 * 2.) Scalar dimension promotion (scalar -> vector/matrix)
 * 3.) Conversion
 * 4.) Conversion + scalar dimension promotion
 * 5.) Truncation (vector -> scalar or lower component vector, matrix -> scalar or lower component matrix)
 * 6.) Conversion + truncation
 */    
static int GetTypeCastRank(HLSLTree * tree, const HLSLType& srcType, const HLSLType& dstType)
{
    /*if (srcType.array != dstType.array || srcType.arraySize != dstType.arraySize)
    {
        return -1;
    }*/

    if (srcType.array != dstType.array)
    {
        return -1;
    }

    if (srcType.array == true)
    {
        ASSERT(dstType.array == true);
        int srcArraySize = -1;
        int dstArraySize = -1;

        tree->GetExpressionValue(srcType.arraySize, srcArraySize);
        tree->GetExpressionValue(dstType.arraySize, dstArraySize);

        if (srcArraySize != dstArraySize) {
            return -1;
        }
    }

    if (srcType.baseType == HLSLBaseType_UserDefined && dstType.baseType == HLSLBaseType_UserDefined)
    {
        return strcmp(srcType.typeName, dstType.typeName) == 0 ? 0 : -1;
    }

    if (srcType.baseType == dstType.baseType)
    {
        return 0;
    }

    const BaseTypeDescription& srcDesc = _baseTypeDescriptions[srcType.baseType];
    const BaseTypeDescription& dstDesc = _baseTypeDescriptions[dstType.baseType];
    if (srcDesc.numericType == NumericType_NaN || dstDesc.numericType == NumericType_NaN)
    {
        return -1;
    }

    // Result bits: T R R R P (T = truncation, R = conversion rank, P = dimension promotion)
    int result = _numberTypeRank[srcDesc.numericType][dstDesc.numericType] << 1;

    if (srcDesc.numDimensions == 0 && dstDesc.numDimensions > 0)
    {
        // Scalar dimension promotion
        result |= (1 << 0);
    }
    else if ((srcDesc.numDimensions == dstDesc.numDimensions && (srcDesc.numComponents > dstDesc.numComponents || srcDesc.height > dstDesc.height)) ||
             (srcDesc.numDimensions > 0 && dstDesc.numDimensions == 0))
    {
        // Truncation
        result |= (1 << 4);
    }
    else if (srcDesc.numDimensions != dstDesc.numDimensions ||
             srcDesc.numComponents != dstDesc.numComponents ||
             srcDesc.height != dstDesc.height)
    {
        // Can't convert
        return -1;
    }
    
    return result;
    
}

static bool GetFunctionCallCastRanks(HLSLTree* tree, const HLSLFunctionCall* call, const HLSLFunction* function, int* rankBuffer)
{

    if (function == NULL || function->numArguments < call->numArguments)
    {
        // Function not viable
        return false;
    }

    const HLSLExpression* expression = call->argument;
    const HLSLArgument* argument = function->argument;
   
    for (int i = 0; i < call->numArguments; ++i)
    {
        int rank = GetTypeCastRank(tree, expression->expressionType, argument->type);
        if (rank == -1)
        {
            return false;
        }

        rankBuffer[i] = rank;
        
        argument = argument->nextArgument;
        expression = expression->nextExpression;
    }

    for (int i = call->numArguments; i < function->numArguments; ++i)
    {
        if (argument->defaultValue == NULL)
        {
            // Function not viable.
            return false;
        }
    }

    return true;

}

struct CompareRanks
{
    bool operator() (const int& rank1, const int& rank2) { return rank1 > rank2; }
};

static CompareFunctionsResult CompareFunctions(HLSLTree* tree, const HLSLFunctionCall* call, const HLSLFunction* function1, const HLSLFunction* function2)
{ 

    int* function1Ranks = static_cast<int*>(alloca(sizeof(int) * call->numArguments));
    int* function2Ranks = static_cast<int*>(alloca(sizeof(int) * call->numArguments));

    const bool function1Viable = GetFunctionCallCastRanks(tree, call, function1, function1Ranks);
    const bool function2Viable = GetFunctionCallCastRanks(tree, call, function2, function2Ranks);

    // Both functions have to be viable to be able to compare them
    if (!(function1Viable && function2Viable))
    {
        if (function1Viable)
        {
            return Function1Better;
        }
        else if (function2Viable)
        {
            return Function2Better;
        }
        else
        {
            return FunctionsEqual;
        }
    }

    std::sort(function1Ranks, function1Ranks + call->numArguments, CompareRanks());
    std::sort(function2Ranks, function2Ranks + call->numArguments, CompareRanks());
    
    for (int i = 0; i < call->numArguments; ++i)
    {
        if (function1Ranks[i] < function2Ranks[i])
        {
            return Function1Better;
        }
        else if (function2Ranks[i] < function1Ranks[i])
        {
            return Function2Better;
        }
    }

    return FunctionsEqual;

}

static bool GetBinaryOpResultType(HLSLBinaryOp binaryOp, const HLSLType& type1, const HLSLType& type2, HLSLType& result)
{

    if (type1.baseType < HLSLBaseType_FirstNumeric || type1.baseType > HLSLBaseType_LastNumeric || type1.array ||
        type2.baseType < HLSLBaseType_FirstNumeric || type2.baseType > HLSLBaseType_LastNumeric || type2.array)
    {
         return false;
    }

    if (binaryOp == HLSLBinaryOp_BitAnd || binaryOp == HLSLBinaryOp_BitOr || binaryOp == HLSLBinaryOp_BitXor)
    {
        if (type1.baseType < HLSLBaseType_FirstInteger || type1.baseType > HLSLBaseType_LastInteger)
        {
            return false;
        }
    }

    switch (binaryOp)
    {
    case HLSLBinaryOp_And:
    case HLSLBinaryOp_Or:
    case HLSLBinaryOp_Less:
    case HLSLBinaryOp_Greater:
    case HLSLBinaryOp_LessEqual:
    case HLSLBinaryOp_GreaterEqual:
    case HLSLBinaryOp_Equal:
	case HLSLBinaryOp_NotEqual:
		{
			int numComponents = std::max( _baseTypeDescriptions[ type1.baseType ].numComponents, _baseTypeDescriptions[ type2.baseType ].numComponents );
			result.baseType = HLSLBaseType( HLSLBaseType_Bool + numComponents - 1 );
			break;
		}
    default:
        result.baseType = _binaryOpTypeLookup[type1.baseType - HLSLBaseType_FirstNumeric][type2.baseType - HLSLBaseType_FirstNumeric];
        break;
    }

    result.typeName     = NULL;
    result.array        = false;
    result.arraySize    = NULL;
    result.flags        = (type1.flags & type2.flags) & HLSLTypeFlag_Const; // Propagate constness.
    
    return result.baseType != HLSLBaseType_Unknown;

}

HLSLParser::HLSLParser(Allocator* allocator, const char* fileName, const char* buffer, size_t length, const char* entryName) :
    m_tokenizer(fileName, buffer, length),
    m_userTypes(allocator),
	m_samplerStates(allocator),
	m_preProcessors(allocator),
	m_textureStates(allocator),
	m_rwtextureStates(allocator),
	m_textureStateExpressions(allocator),
	m_rwtextureStateExpressions(allocator),
    m_variables(allocator),
    m_functions(allocator)
{
    m_numGlobals = 0;
    m_tree = NULL;
	m_entryName = entryName;
}

bool HLSLParser::Check(int token)
{
	if (m_tokenizer.GetToken() == token)
	{
		return true;
	}
	return false;
}

bool HLSLParser::Accept(int token)
{
    if (m_tokenizer.GetToken() == token)
    {
       m_tokenizer.Next();
       return true;
    }
    return false;
}

bool HLSLParser::Accept(const char* token)
{
    if (m_tokenizer.GetToken() == HLSLToken_Identifier && String_Equal( token, m_tokenizer.GetIdentifier() ) )
    {
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::Expect(int token)
{
    if (!Accept(token))
    {
        char want[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(token, want);
        char near[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected '%s' near '%s'", want, near);
        return false;
    }
    return true;
}

bool HLSLParser::Expect(const char * token)
{
    if (!Accept(token))
    {
        const char * want = token;
        char near[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected '%s' near '%s'", want, near);
        return false;
    }
    return true;
}


bool HLSLParser::AcceptIdentifier(const char*& identifier)
{
    if (m_tokenizer.GetToken() == HLSLToken_Identifier)
    {
        identifier = m_tree->AddString( m_tokenizer.GetIdentifier() );
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::ExpectIdentifier(const char*& identifier)
{
    if (!AcceptIdentifier(identifier))
    {
        char near[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected identifier near '%s'", near);
        identifier = "";
        return false;
    }
    return true;
}

bool HLSLParser::AcceptFloat(float& value)
{
    if (m_tokenizer.GetToken() == HLSLToken_FloatLiteral)
    {
        value = m_tokenizer.GetFloat();
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::AcceptHalf( float& value )
{
	if( m_tokenizer.GetToken() == HLSLToken_HalfLiteral )
	{
		value = m_tokenizer.GetFloat();
		m_tokenizer.Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptInt(int& value)
{
    if (m_tokenizer.GetToken() == HLSLToken_IntLiteral)
    {
        value = m_tokenizer.GetInt();
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::AcceptUint(unsigned int& value)
{
	if (m_tokenizer.GetToken() == HLSLToken_UintLiteral)
	{
		value = m_tokenizer.GetuInt();
		m_tokenizer.Next();
		return true;
	}
	return false;
}

bool HLSLParser::ParseTopLevel(HLSLStatement*& statement)
{
    HLSLAttribute * attributes = NULL;
    ParseAttributeBlock(attributes);

    int line             = GetLineNumber();
    const char* fileName = GetFileName();
    
    HLSLBaseType type;
    const char*  typeName = NULL;
    int          typeFlags = false;

    bool doesNotExpectSemicolon = false;

	if (Accept(HLSLToken_P_Define))
	{
		//Define preprocessor
		//Doesn't support Macro function yet
		const char* defineIndentifier = NULL;

		if (!AcceptIdentifier(defineIndentifier))
		{
			return false;
		}

		HLSLpreprocessor * preProcessor = m_tree->AddNode<HLSLpreprocessor>(defineIndentifier, line);
		preProcessor->type = HLSLBaseType_PreProcessorDefine;
		preProcessor->preprocessorType = HLSLToken_P_Define;

		preProcessor->name = defineIndentifier;
		strcpy(preProcessor->identifier, m_tokenizer.GetIdentifier());

		bool needEndParen;
		if (!ParseTerminalExpression(preProcessor->expression, needEndParen))
			return false;
		else
		{
			// if it is just single type
			//preProcessor->type = HLSLBaseType_PreProcessorDefine;
			preProcessor->type = preProcessor->expression->expressionType.baseType;

			HLSLType temp;
			temp.array = false;
			temp.baseType = preProcessor->type;

			DeclareVariable(preProcessor->name, temp);
		}

		m_preProcessors.PushBack(preProcessor);
		doesNotExpectSemicolon = true;

		statement = preProcessor;
	}
	else if (HLSLToken_P_If <= m_tokenizer.GetToken() && HLSLToken_P_Error >= m_tokenizer.GetToken())
	{
		const char* defineIndentifier = "IftoElif";

		HLSLpreprocessor * preProcessor = m_tree->AddNode<HLSLpreprocessor>(defineIndentifier, line);

		switch (m_tokenizer.GetToken())
		{
		case HLSLToken_P_If: preProcessor->type = HLSLBaseType_PreProcessorIf;
			break;
		case HLSLToken_P_Elif: preProcessor->type = HLSLBaseType_PreProcessorElif;
			break;
		case HLSLToken_P_Else: preProcessor->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessor->type = HLSLBaseType_PreProcessorEndif;
			break;
		case HLSLToken_P_IfDef: preProcessor->type = HLSLBaseType_PreProcessorIfDef;
			break;
		case HLSLToken_P_IfnDef: preProcessor->type = HLSLBaseType_PreProcessorIfnDef;
			break;
		case HLSLToken_P_Undef: preProcessor->type = HLSLBaseType_PreProcessorUndef;
			break;
		case HLSLToken_P_Include: preProcessor->type = HLSLBaseType_PreProcessorInclude;
			break;
		case HLSLToken_P_Line: preProcessor->type = HLSLBaseType_PreProcessorLine;
			break;
		case HLSLToken_P_Pragma: preProcessor->type = HLSLBaseType_PreProcessorPragma;
			break;
		default:
			break;
		}

		preProcessor->preprocessorType = m_tokenizer.GetToken();

		char preprocessorContents[1024];
		m_tokenizer.GetRestofWholeline(preprocessorContents);

		strcpy(preProcessor->contents, preprocessorContents);
		m_tokenizer.Next();

		doesNotExpectSemicolon = true;

		statement = preProcessor;
	}
	else if (HLSLToken_P_Else <= m_tokenizer.GetToken() && HLSLToken_P_Endif >= m_tokenizer.GetToken())
	{
		HLSLpreprocessor * preProcessor = m_tree->AddNode<HLSLpreprocessor>(m_tokenizer.GetToken() == HLSLToken_P_Else ? "Else" : "Endif", line);

		switch (m_tokenizer.GetToken())
		{		
		case HLSLToken_P_Else: preProcessor->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessor->type = HLSLBaseType_PreProcessorEndif;
			break;
		default:
			break;
		}

		preProcessor->preprocessorType = m_tokenizer.GetToken();
		m_tokenizer.Next();

		doesNotExpectSemicolon = true;

		statement = preProcessor;
	}
    else if (Accept(HLSLToken_Struct))
    {
        // Struct declaration.

        const char* structName = NULL;
        if (!ExpectIdentifier(structName))
        {
            return false;
        }
        if (FindUserDefinedType(structName) != NULL)
        {
            m_tokenizer.Error("struct %s already defined", structName);
            return false;
        }

        if (!Expect('{'))
        {
            return false;
        }

        HLSLStruct* structure = m_tree->AddNode<HLSLStruct>(fileName, line);
        structure->name = structName;

        m_userTypes.PushBack(structure);
 
        HLSLStructField* lastField = NULL;

        // Add the struct to our list of user defined types.
        while (!Accept('}'))
        {
            if (CheckForUnexpectedEndOfStream('}'))
            {
                return false;
            }
            HLSLStructField* field = NULL;

			//fist check if there is a preprocessor
			/*
			if (HLSLToken_P_If <= m_tokenizer.GetToken() && HLSLToken_P_Undef >= m_tokenizer.GetToken())
			{
				field = m_tree->AddNode<HLSLStructField>(GetFileName(), GetLineNumber());

				if (HLSLToken_P_If <= m_tokenizer.GetToken() && HLSLToken_P_Elif >= m_tokenizer.GetToken())
				{
					field->preProcessor = m_tree->AddNode<HLSLpreprocessor>(GetFileName(), line);
					field->preProcessor->preprocessorType = m_tokenizer.GetToken();
					m_tokenizer.Next();

					if (!ParseExpression(field->preProcessor->expression))
						return false;
				}
				else if (HLSLToken_P_Else <= m_tokenizer.GetToken() && HLSLToken_P_Endif >= m_tokenizer.GetToken())
				{
					field->preProcessor = m_tree->AddNode<HLSLpreprocessor>(GetFileName(), line);
					field->preProcessor->preprocessorType = m_tokenizer.GetToken();

					m_tokenizer.Next();
				}
				

			}
			else 
			*/if (!ParseFieldDeclaration(field))
            {
                return false;
            }
            ASSERT(field != NULL);
            if (lastField == NULL)
            {
                structure->field = field;
            }
            else
            {
                lastField->nextField = field;
            }
            lastField = field;
        }

        statement = structure;
    }
	else if(Accept(HLSLToken_RWBuffer))
	{
		HLSLRWBuffer* buffer = m_tree->AddNode<HLSLRWBuffer>(fileName, line);
		
		if (Accept('<'))
		{
			if (HLSLToken_First_Numeric_Type <= m_tokenizer.GetToken() && m_tokenizer.GetToken() <= HLSLToken_Last_Numeric_Type)
			{
				buffer->elementType = _reservedWords[m_tokenizer.GetToken() - HLSLToken_First_Numeric_Type];
				AcceptType(false, type, typeName, &typeFlags);				
			}
		}

		if (!Expect('>'))
			return false;

		AcceptIdentifier(buffer->name);

		// Optional register assignment.
		if (Accept(':'))
		{
			if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(buffer->registerName))
			{
				return false;
			}

			// if there is space
			if (Check(','))
			{
				m_tokenizer.Next();
				//get space name
				ExpectIdentifier(buffer->registerSpaceName);
			}
			else
			{
				//default Sapce Name
				buffer->registerSpaceName = "space0";
			}

			if (!Expect(')'))
			{
				return false;
			}
		}

		HLSLType temp;
		temp.array = true;
		temp.baseType = type;

		DeclareVariable(buffer->name, temp);

		statement = buffer;
	}	
	else if (Accept(HLSLToken_RWStructuredBuffer))
	{
		HLSLRWStructuredBuffer* buffer = m_tree->AddNode<HLSLRWStructuredBuffer>(fileName, line);

		if (Accept('<'))
		{
			if(!AcceptIdentifier(buffer->elementType))
				AcceptType(false, type, typeName, &typeFlags);
		}

		if (!Expect('>'))
			return false;

		AcceptIdentifier(buffer->name);		

		// Optional register assignment.
		if (Accept(':'))
		{
			if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(buffer->registerName))
			{
				return false;
			}

			// if there is space
			if (Check(','))
			{
				m_tokenizer.Next();
				//get space name
				ExpectIdentifier(buffer->registerSpaceName);
			}
			else
			{
				//default Sapce Name
				buffer->registerSpaceName = "space0";
			}

			if (!Expect(')'))
			{
				return false;
			}
		}

		HLSLType temp;
		temp.array = true;
		
		if (buffer->elementType)
		{
			temp.baseType = HLSLBaseType_UserDefined;
			temp.typeName = buffer->elementType;
			buffer->dataType = HLSLBaseType_UserDefined;
		}
		else
		{
			temp.baseType = type;
			buffer->dataType = type;
		}		

		DeclareVariable(buffer->name, temp);

		statement = buffer;
	}
	else if (Accept(HLSLToken_StructuredBuffer) || Accept(HLSLToken_ConstantBuffer))
	{
		HLSLStructuredBuffer* buffer = m_tree->AddNode<HLSLStructuredBuffer>(fileName, line);

		if (Accept('<'))
		{
			if (!AcceptIdentifier(buffer->elementType))
				AcceptType(false, type, typeName, &typeFlags);
		}

		if (!Expect('>'))
			return false;

		AcceptIdentifier(buffer->name);

		/*
		if (strstr(buffer->name, "RootConstant") || strstr(buffer->name, "rootConstant") || strstr(buffer->name, "rootconstant"))
		{
			buffer->bPush_Constant = true;
		}
		*/

		// Optional register assignment.
		if (Accept(':'))
		{
			if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(buffer->registerName))
			{
				return false;
			}

			// if there is space
			if (Check(','))
			{
				m_tokenizer.Next();
				//get space name
				ExpectIdentifier(buffer->registerSpaceName);
			}
			else
			{
				//default Sapce Name
				buffer->registerSpaceName = "space0";
			}

			if (!Expect(')'))
			{
				return false;
			}
		}

		HLSLType temp;
		temp.array = true;

		if (buffer->elementType)
		{
			temp.baseType = HLSLBaseType_UserDefined;
			temp.typeName = buffer->elementType;
		}
		else
		{
			temp.baseType = type;
			buffer->dataType = type;
		}

		DeclareVariable(buffer->name, temp);

		statement = buffer;
	}
    else if (Accept(HLSLToken_CBuffer) || Accept(HLSLToken_TBuffer))
    {
        // cbuffer/tbuffer declaration.

		HLSLConstantBuffer* buffer = m_tree->AddNode<HLSLConstantBuffer>(fileName, line);
        AcceptIdentifier(buffer->name);


		if (strstr(buffer->name, "RootConstant") || strstr(buffer->name, "rootConstant") || strstr(buffer->name, "rootconstant"))
		{
			buffer->bPush_Constant = true;
		}

        // Optional register assignment.
        if (Accept(':'))
        {
			if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(buffer->registerName))
			{
				return false;
			}

			// if there is space
			if (Check(','))
			{
				m_tokenizer.Next();
				//get space name
				ExpectIdentifier(buffer->registerSpaceName);
			}
			else
			{
				//default Sapce Name
				buffer->registerSpaceName = "space0";
			}

			if (!Expect(')'))
			{
				return false;
			}
        }

        // Fields.
        if (!Expect('{'))
        {
            return false;
        }
        HLSLDeclaration* lastField = NULL;
        while (!Accept('}'))
        {
            if (CheckForUnexpectedEndOfStream('}'))
            {
                return false;
            }
            HLSLDeclaration* field = NULL;
            if (!ParseDeclaration(field))
            {
                m_tokenizer.Error("Expected variable declaration");
                return false;
            }
            DeclareVariable( field->name, field->type );			
			
			field->buffer = buffer;
            if (buffer->field == NULL)
            {
                buffer->field = field;
            }
            else
            {
                lastField->nextStatement = field;
            }
            lastField = field;

			
            if (!Expect(';')) {
                return false;
            }			
        }

		if(!Check(';'))
			doesNotExpectSemicolon = true;

        statement = buffer;
    }	
	else if (Accept(HLSLToken_SamplerState) || Accept(HLSLToken_Sampler))
	{
		//SamplerState can be declared or be passed from outside

		// SamplerState declaration.
		const char* samplerStateName = NULL;
		if (!ExpectIdentifier(samplerStateName))
		{
			return false;
		}

		HLSLSamplerState* samplerstate = m_tree->AddNode<HLSLSamplerState>(fileName, line);
		samplerstate->name = samplerStateName;

		m_samplerStates.PushBack(samplerstate);

		if (Check('{'))
		{
			m_tokenizer.Next();			
			samplerstate->bStructured = true;
			HLSLSamplerStateExpression* lastExpression = NULL;

			// Add the struct to our list of user defined types.
			while (!Accept('}'))
			{
				if (CheckForUnexpectedEndOfStream('}'))
				{
					return false;
				}

				HLSLSamplerStateExpression* expression = m_tree->AddNode<HLSLSamplerStateExpression>(fileName, GetLineNumber());

				ParseSamplerStateExpression(expression);


				ASSERT(expression != NULL);
				if (lastExpression == NULL)
				{
					samplerstate->expression = expression;
				}
				else
				{
					lastExpression->nextExpression = expression;
				}
				lastExpression = expression;
			}

			statement = samplerstate;
		}
		else if (Accept(':'))// Handle optional register.
		{			
			if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(samplerstate->registerName))
			{
				return false;
			}

			// if there is space
			if (Check(','))
			{
				m_tokenizer.Next();
				//get space name
				ExpectIdentifier(samplerstate->registerSpaceName);
			}
			else
			{
				//default Sapce Name
				samplerstate->registerSpaceName = "space0";
			}

			if (!Expect(')'))
			{
				return false;
			}

			statement = samplerstate;
		}	
		else
		{
			m_tokenizer.Error("Missed SamplerState's register");
			return false;
		}
	}	
	else if (HLSLToken_Texture1D <= m_tokenizer.GetToken() && HLSLToken_TextureCubeArray >= m_tokenizer.GetToken())
	{
		HLSLTextureState* texturestate = m_tree->AddNode<HLSLTextureState>(fileName, line);

		switch (m_tokenizer.GetToken())
		{
		case HLSLToken_Texture1D:
			texturestate->baseType = HLSLBaseType_Texture1D; break;	
		case HLSLToken_Texture1DArray:
			texturestate->baseType = HLSLBaseType_Texture1DArray; break;
		case HLSLToken_Texture2D:
			texturestate->baseType = HLSLBaseType_Texture2D; break;
		case HLSLToken_Texture2DArray:
			texturestate->baseType = HLSLBaseType_Texture2DArray; break;
		case HLSLToken_Texture3D:
			texturestate->baseType = HLSLBaseType_Texture3D; break;
		case HLSLToken_Texture2DMS:
			texturestate->baseType = HLSLBaseType_Texture2DMS; break;
		case HLSLToken_Texture2DMSArray:
			texturestate->baseType = HLSLBaseType_Texture2DMSArray;	break;
		case HLSLToken_TextureCube:
			texturestate->baseType = HLSLBaseType_TextureCube; break;
		case HLSLToken_TextureCubeArray:
			texturestate->baseType = HLSLBaseType_TextureCubeArray;	break;
		default:
			return false;
			break;
		}
				
		//Texture
		m_tokenizer.Next();

		//Handle elementType
		if (Accept('<'))
		{
			AcceptType(false, type, typeName, &typeFlags);
			texturestate->dataType = type;

			if (!Expect('>'))
				return false;
		}

		const char* textureName = NULL;
		if (!ExpectIdentifier(textureName))
		{
			return false;
		}

		// Handle array syntax.
		while (Accept('['))
		{
			texturestate->bArray = true;
			

			if (Check(']')) //unboundedSize
			{
				texturestate->arrayIdentifier[texturestate->arrayDimension][0] = NULL;
				m_tokenizer.Next();
			}
			else if (!Accept(']'))
			{				
				if (!String_Equal(m_tokenizer.GetIdentifier(), ""))
				{
					strcpy(texturestate->arrayIdentifier[texturestate->arrayDimension], m_tokenizer.GetIdentifier());
					m_tokenizer.Next();
					Expect(']');
				}
				else
				{
					texturestate->arrayIndex[texturestate->arrayDimension] = m_tokenizer.GetuInt();
					m_tokenizer.Next();
					Expect(']');
				}
			}

			texturestate->arrayDimension++;
		}

		texturestate->name = textureName;
		

		if (Accept(':'))// Handle optional register.
		{
			if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(texturestate->registerName))
			{
				return false;
			}

			// if there is space
			if (Check(','))
			{
				m_tokenizer.Next();
				//get space name
				ExpectIdentifier(texturestate->registerSpaceName);
			}
			else
			{
				//default Sapce Name
				texturestate->registerSpaceName = "space0";
			}

			if (!Expect(')'))
			{
				return false;
			}

			statement = texturestate;

			m_textureStates.PushBack(texturestate);
		}
		else
			return false;
	}
	else if (HLSLToken_RWTexture1D <= m_tokenizer.GetToken() && HLSLToken_RWTexture3D >= m_tokenizer.GetToken())
	{
		HLSLRWTextureState* rwtexturestate = m_tree->AddNode<HLSLRWTextureState>(fileName, line);

		switch (m_tokenizer.GetToken())
		{
		case HLSLToken_RWTexture1D:
			rwtexturestate->baseType = HLSLBaseType_RWTexture1D; break;
		case HLSLToken_RWTexture1DArray:
			rwtexturestate->baseType = HLSLBaseType_RWTexture1DArray; break;
		case HLSLToken_RWTexture2D:
			rwtexturestate->baseType = HLSLBaseType_RWTexture2D; break;
		case HLSLToken_RWTexture2DArray:
			rwtexturestate->baseType = HLSLBaseType_RWTexture2DArray; break;
		case HLSLToken_RWTexture3D:
			rwtexturestate->baseType = HLSLBaseType_RWTexture3D; break;
		default:
			return false;
			break;
		}

		m_tokenizer.Next();

		//elementType
		if (Accept('<'))
		{
			AcceptType(false, type, typeName, &typeFlags);
			rwtexturestate->dataType = type;

			if (!Expect('>'))
				return false;
		}	


		// SamplerState declaration.
		const char* textureName = NULL;
		if (!ExpectIdentifier(textureName))
		{
			return false;
		}

		rwtexturestate->name = textureName;

		// Handle array syntax.
		while (Accept('['))
		{
			rwtexturestate->bArray = true;


			if (Check(']')) //unboundedSize
			{
				rwtexturestate->arrayIdentifier[rwtexturestate->arrayDimension][0] = NULL;
				m_tokenizer.Next();
			}
			else if (!Accept(']'))
			{
				if (!String_Equal(m_tokenizer.GetIdentifier(), ""))
				{
					strcpy(rwtexturestate->arrayIdentifier[rwtexturestate->arrayDimension], m_tokenizer.GetIdentifier());
					m_tokenizer.Next();
				}
				else
				{
					rwtexturestate->arrayIndex[rwtexturestate->arrayDimension] = m_tokenizer.GetuInt();
					m_tokenizer.Next();
				}
			}

			rwtexturestate->arrayDimension++;
		}


		if (Accept(':'))// Handle optional register.
		{
			if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(rwtexturestate->registerName))
			{
				return false;
			}

			// if there is space
			if (Check(','))
			{
				m_tokenizer.Next();
				//get space name
				ExpectIdentifier(rwtexturestate->registerSpaceName);
			}
			else
			{
				//default Sapce Name
				rwtexturestate->registerSpaceName = "space0";
			}

			if (!Expect(')'))
			{
				return false;
			}

			statement = rwtexturestate;

			m_rwtextureStates.PushBack(rwtexturestate);
		}
		else
			return false;
	}	
    else if (AcceptType(true, type, typeName, &typeFlags))
    {
        // Global declaration (uniform or function).
        const char* globalName = NULL;
        if (!ExpectIdentifier(globalName))
        {
            return false;
        }

        if (Accept('('))
        {
            // Function declaration.

            HLSLFunction* function = m_tree->AddNode<HLSLFunction>(fileName, line);
            function->name                  = globalName;
            function->returnType.baseType   = type;
            function->returnType.typeName   = typeName;

            BeginScope();

            if (!ParseArgumentList(function->argument, function->numArguments))
            {
                return false;
            }

            const HLSLFunction* declaration = FindFunction(function);

            // Forward declaration
            if (Accept(';'))
            {
                // Add a function entry so that calls can refer to it
                if (!declaration)
                {
                    m_functions.PushBack( function );
                    statement = function;
                }
                EndScope();
                return true;
            }

            // Optional semantic.
            if (Accept(':') && !ExpectIdentifier(function->semantic))
            {
                return false;
            }

            if (declaration)
            {
                if (declaration->forward || declaration->statement)
                {
                    m_tokenizer.Error("Duplicate function definition");
                    return false;
                }

                const_cast<HLSLFunction*>(declaration)->forward = function;
            }
            else
            {
                m_functions.PushBack( function );
            }

            if (!Expect('{') || !ParseBlock(function->statement, function->returnType))
            {
                return false;
            }

            EndScope();

            // Note, no semi-colon at the end of a function declaration.
            statement = function;
			doesNotExpectSemicolon = true;
			// return true;
        }
        else
        {
            // Uniform declaration.
            HLSLDeclaration* declaration = m_tree->AddNode<HLSLDeclaration>(fileName, line);
            declaration->name            = globalName;
            declaration->type.baseType   = type;
            declaration->type.flags      = typeFlags;

            // Handle array syntax.
            if (Accept('['))
            {
                if (!Accept(']'))
                {
                    if (!ParseExpression(declaration->type.arraySize) || !Expect(']'))
                    {
                        return false;
                    }
                }
                declaration->type.array = true;
            }

            // Handle optional register.
            if (Accept(':'))
            {
                // @@ Currently we support either a semantic or a register, but not both.
                if (AcceptIdentifier(declaration->semantic)) {
                    int k = 1;
                }
                else if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(declaration->registerName) || !Expect(')'))
                {
                    return false;
                }
            }

            DeclareVariable( globalName, declaration->type );

            if (!ParseDeclarationAssignment(declaration))
            {
                return false;
            }

            // TODO: Multiple variables declared on one line.
            
            statement = declaration;
        }
    }
    else if (ParseTechnique(statement)) {
        doesNotExpectSemicolon = true;
    }
    else if (ParsePipeline(statement)) {
        doesNotExpectSemicolon = true;
    }
    else if (ParseStage(statement)) {
        doesNotExpectSemicolon = true;
    }

    if (statement != NULL) {
        statement->attributes = attributes;
    }

    return doesNotExpectSemicolon || Expect(';');
}

bool HLSLParser::ParseStatementOrBlock(HLSLStatement*& firstStatement, const HLSLType& returnType)
{
    if (Accept('{'))
    {
        BeginScope();
        if (!ParseBlock(firstStatement, returnType))
        {
            return false;
        }
        EndScope();
        return true;
    }
    else
    {
        return ParseStatement(firstStatement, returnType);
    }
}

bool HLSLParser::ParseBlock(HLSLStatement*& firstStatement, const HLSLType& returnType)
{
    HLSLStatement* lastStatement = NULL;
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }
        HLSLStatement* statement = NULL;
        if (!ParseStatement(statement, returnType))
        {
            return false;
        }
        if (statement != NULL)
        {
            if (firstStatement == NULL)
            {
                firstStatement = statement;
            }
            else
            {
                lastStatement->nextStatement = statement;
            }
            lastStatement = statement;
        }
    }
    return true;
}

bool HLSLParser::ParseStatement(HLSLStatement*& statement, const HLSLType& returnType)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

	if (line == 154)
	{
		int debug = 345;
	}

    // Empty statements.
    if (Accept(';'))
    {
        return true;
    }

    HLSLAttribute * attributes = NULL;
    ParseAttributeBlock(attributes);    // @@ Leak if not assigned to node? 

    
	
	// If statement.
    if (Accept(HLSLToken_If))
    {
		HLSLIfStatement* ifStatement = m_tree->AddNode<HLSLIfStatement>(fileName, line);
		ifStatement->attributes = attributes;
					
		if (!Expect('(') || !ParseExpression(ifStatement->condition) || !Expect(')'))
		{
			return false;
		}
		statement = ifStatement;
		if (!ParseStatementOrBlock(ifStatement->statement, returnType))
		{
			return false;
		}

		for (int i = 0; i < 128; i++)
		{
			if (Accept(HLSLToken_ElseIf))
			{
				HLSLIfStatement* elseifStatement = m_tree->AddNode<HLSLIfStatement>(fileName, line);
				elseifStatement->attributes = attributes;

				if (!Expect('(') || !ParseExpression(elseifStatement->condition) || !Expect(')'))
				{
					return false;
				}

				ifStatement->elseifStatementCounter++;
				ifStatement->elseifStatement[i] = elseifStatement;

				bool result = ParseStatementOrBlock(ifStatement->elseifStatement[i]->statement, returnType);

				if (!result)
					return false;
			}
			else
				break;
		}
		

		if (Accept(HLSLToken_Else))
		{
			return ParseStatementOrBlock(ifStatement->elseStatement, returnType);
		}
		return true;        
    }
    // For statement.
    if (Accept(HLSLToken_For))
    {
        HLSLForStatement* forStatement = m_tree->AddNode<HLSLForStatement>(fileName, line);
        forStatement->attributes = attributes;
        if (!Expect('('))
        {
            return false;
        }
        BeginScope();
        if (!ParseDeclaration(forStatement->initialization))
        {
            return false;
        }
        if (!Expect(';'))
        {
            return false;
        }
        ParseExpression(forStatement->condition);
        if (!Expect(';'))
        {
            return false;
        }
        ParseExpression(forStatement->increment);
        if (!Expect(')'))
        {
            return false;
        }
        statement = forStatement;
        if (!ParseStatementOrBlock(forStatement->statement, returnType))
        {
            return false;
        }
        EndScope();
        return true;
    }
	// While statement.
	if (Accept(HLSLToken_While))
	{
		HLSLWhileStatement* whileStatement = m_tree->AddNode<HLSLWhileStatement>(fileName, line);
		whileStatement->attributes = attributes;
		if (!Expect('('))
		{
			return false;
		}
		BeginScope();

		/*(
		if (!ParseDeclaration(whileStatement->initialization))
		{
			return false;
		}
		if (!Expect(';'))
		{
			return false;
		}
		*/
		ParseExpression(whileStatement->condition);		
		/*
		if (!Expect(';'))
		{
			return false;
		}
		*/
		EndScope();
		if (!Expect(')'))
		{
			return false;
		}

		statement = whileStatement;
		if (!ParseStatementOrBlock(whileStatement->statement, returnType))
		{
			return false;
		}
		
		return true;
	}
	else if (HLSLToken_P_If <= m_tokenizer.GetToken() && HLSLToken_P_Error >= m_tokenizer.GetToken())
	{
		const char* defineIndentifier = "IftoElif";

		HLSLpreprocessor * preProcessor = m_tree->AddNode<HLSLpreprocessor>(defineIndentifier, line);

		switch (m_tokenizer.GetToken())
		{
		case HLSLToken_P_If: preProcessor->type = HLSLBaseType_PreProcessorIf;
			break;
		case HLSLToken_P_Elif: preProcessor->type = HLSLBaseType_PreProcessorElif;
			break;
		case HLSLToken_P_Else: preProcessor->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessor->type = HLSLBaseType_PreProcessorEndif;
			break;
		case HLSLToken_P_IfDef: preProcessor->type = HLSLBaseType_PreProcessorIfDef;
			break;
		case HLSLToken_P_IfnDef: preProcessor->type = HLSLBaseType_PreProcessorIfnDef;
			break;
		case HLSLToken_P_Undef: preProcessor->type = HLSLBaseType_PreProcessorUndef;
			break;
		case HLSLToken_P_Include: preProcessor->type = HLSLBaseType_PreProcessorInclude;
			break;
		case HLSLToken_P_Line: preProcessor->type = HLSLBaseType_PreProcessorLine;
			break;
		case HLSLToken_P_Pragma: preProcessor->type = HLSLBaseType_PreProcessorPragma;
			break;
		default:
			break;
		}

		preProcessor->preprocessorType = m_tokenizer.GetToken();

		char preprocessorContents[1024];
		m_tokenizer.GetRestofWholeline(preprocessorContents);

		strcpy(preProcessor->contents, preprocessorContents);
		m_tokenizer.Next();

		statement = preProcessor;

		return true;
	}
	else if (HLSLToken_P_Else <= m_tokenizer.GetToken() && HLSLToken_P_Endif >= m_tokenizer.GetToken())
	{
		HLSLpreprocessor * preProcessor = m_tree->AddNode<HLSLpreprocessor>(m_tokenizer.GetToken() == HLSLToken_P_Else ? "Else" : "Endif", line);

		switch (m_tokenizer.GetToken())
		{		
		case HLSLToken_P_Else: preProcessor->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessor->type = HLSLBaseType_PreProcessorEndif;
			break;
		default:
			break;
		}

		preProcessor->preprocessorType = m_tokenizer.GetToken();
		m_tokenizer.Next();

		statement = preProcessor;

		return true;
	}

    if (attributes != NULL)
    {
        // @@ Error. Unexpected attribute. We only support attributes associated to if and for statements.
    }

    // Block statement.
    if (Accept('{'))
    {
        HLSLBlockStatement* blockStatement = m_tree->AddNode<HLSLBlockStatement>(fileName, line);
        statement = blockStatement;
        BeginScope();
        bool success = ParseBlock(blockStatement->statement, returnType);
        EndScope();
        return success;
    }

    // Discard statement.
    if (Accept(HLSLToken_Discard))
    {
        HLSLDiscardStatement* discardStatement = m_tree->AddNode<HLSLDiscardStatement>(fileName, line);
        statement = discardStatement;
        return Expect(';');
    }

    // Break statement.
    if (Accept(HLSLToken_Break))
    {
        HLSLBreakStatement* breakStatement = m_tree->AddNode<HLSLBreakStatement>(fileName, line);
        statement = breakStatement;
        return Expect(';');
    }

    // Continue statement.
    if (Accept(HLSLToken_Continue))
    {
        HLSLContinueStatement* continueStatement = m_tree->AddNode<HLSLContinueStatement>(fileName, line);
        statement = continueStatement;
        return Expect(';');
    }

    // Return statement
    if (Accept(HLSLToken_Return))
    {
        HLSLReturnStatement* returnStatement = m_tree->AddNode<HLSLReturnStatement>(fileName, line);
        
		if (Check(';'))
		{

		}
		else if (/*!Accept(';') &&*/ !ParseExpression(returnStatement->expression))
        {
			 return false;
        }
        // Check that the return expression can be cast to the return type of the function.
        HLSLType voidType(HLSLBaseType_Void);
        if (!CheckTypeCast(returnStatement->expression ? returnStatement->expression->expressionType : voidType, returnType))
        {
            return false;
        }

        statement = returnStatement;
		return Expect(';');
    }

    HLSLDeclaration* declaration = NULL;
    HLSLExpression*  expression  = NULL;

    if (ParseDeclaration(declaration))
    {
        statement = declaration;
    }
    else if (ParseExpression(expression))
    {
        HLSLExpressionStatement* expressionStatement;
        expressionStatement = m_tree->AddNode<HLSLExpressionStatement>(fileName, line);
        expressionStatement->expression = expression;
        statement = expressionStatement;
    }

    return Expect(';');
}


// IC: This is only used in block statements, or within control flow statements. So, it doesn't support semantics or layout modifiers.
// @@ We should add suport for semantics for inline input/output declarations.
bool HLSLParser::ParseDeclaration(HLSLDeclaration*& declaration)
{
    const char* fileName    = GetFileName();
    int         line        = GetLineNumber();

    HLSLType type;
    if (!AcceptType(/*allowVoid=*/false, type.baseType, type.typeName, &type.flags))
    {
        return false;
    }

    bool allowUnsizedArray = true;  // @@ Really?

    HLSLDeclaration * firstDeclaration = NULL;
    HLSLDeclaration * lastDeclaration = NULL;

    do {
        const char* name;
        if (!ExpectIdentifier(name))
        {
            // TODO: false means we didn't accept a declaration and we had an error!
            return false;
        }
        // Handle array syntax.
        if (Accept('['))
        {
            type.array = true;
            // Optionally allow no size to the specified for the array.
            if (Accept(']') && allowUnsizedArray)
            {
                return true;
            }
            if (!ParseExpression(type.arraySize) || !Expect(']'))
            {
                return false;
            }
        }

        HLSLDeclaration * declaration = m_tree->AddNode<HLSLDeclaration>(fileName, line);
        declaration->type  = type;
        declaration->name  = name;

        DeclareVariable( declaration->name, declaration->type );

        // Handle option assignment of the declared variables(s).
        if (!ParseDeclarationAssignment( declaration )) {
            return false;
        }

        if (firstDeclaration == NULL) firstDeclaration = declaration;
        if (lastDeclaration != NULL) lastDeclaration->nextDeclaration = declaration;
        lastDeclaration = declaration;

    } while(Accept(','));

    declaration = firstDeclaration;

    return true;
}

bool HLSLParser::ParseDeclarationAssignment(HLSLDeclaration* declaration)
{
    if (Accept('='))
    {
        // Handle array initialization syntax.
        if (declaration->type.array)
        {
            int numValues = 0;
            if (!Expect('{') || !ParseExpressionList('}', true, declaration->assignment, numValues))
            {
                return false;
            }
        }
		else if (IsTextureType(declaration->type.baseType))
		{
			///!!!!!!!!!!!!!!!!!!
			if (!ParseSamplerState(declaration->assignment))
			{
				return false;
			}
		}
        else if (IsSamplerType(declaration->type.baseType))
        {
            if (!ParseSamplerState(declaration->assignment))
            {
                return false;
            }
        }
        else if (!ParseExpression(declaration->assignment))
        {
            return false;
        }
    }
    return true;
}

bool HLSLParser::ParseFieldDeclaration(HLSLStructField*& field)
{
    field = m_tree->AddNode<HLSLStructField>( GetFileName(), GetLineNumber() );

	if (HLSLToken_P_If <= m_tokenizer.GetToken() && HLSLToken_P_Pragma >= m_tokenizer.GetToken())
	{
		HLSLpreprocessor * preProcessor = m_tree->AddNode<HLSLpreprocessor>(GetFileName(), GetLineNumber());

		switch (m_tokenizer.GetToken())
		{
		case HLSLToken_P_If: preProcessor->type = HLSLBaseType_PreProcessorIf;
			break;
		case HLSLToken_P_Elif: preProcessor->type = HLSLBaseType_PreProcessorElif;
			break;
		case HLSLToken_P_Else: preProcessor->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessor->type = HLSLBaseType_PreProcessorEndif;
			break;
		case HLSLToken_P_IfDef: preProcessor->type = HLSLBaseType_PreProcessorIfDef;
			break;
		case HLSLToken_P_IfnDef: preProcessor->type = HLSLBaseType_PreProcessorIfnDef;
			break;
		case HLSLToken_P_Undef: preProcessor->type = HLSLBaseType_PreProcessorUndef;
			break;
		case HLSLToken_P_Include: preProcessor->type = HLSLBaseType_PreProcessorInclude;
			break;
		case HLSLToken_P_Line: preProcessor->type = HLSLBaseType_PreProcessorLine;
			break;
		case HLSLToken_P_Pragma: preProcessor->type = HLSLBaseType_PreProcessorPragma;
			break;
		default:
			break;
		}

		preProcessor->preprocessorType = m_tokenizer.GetToken();

		char preprocessorContents[1024];
		m_tokenizer.GetRestofWholeline(preprocessorContents);

		strcpy(preProcessor->contents, preprocessorContents);
		m_tokenizer.Next();

		field->preProcessor = preProcessor;

		return true;
	}
	else if (HLSLToken_P_Else <= m_tokenizer.GetToken() && HLSLToken_P_Endif >= m_tokenizer.GetToken())
	{
		HLSLpreprocessor * preProcessor = m_tree->AddNode<HLSLpreprocessor>(GetFileName(), GetLineNumber());
		
		switch (m_tokenizer.GetToken())
		{
		case HLSLToken_P_Else: preProcessor->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessor->type = HLSLBaseType_PreProcessorEndif;
			break;
		default:
			break;
		}

		preProcessor->preprocessorType = m_tokenizer.GetToken();

		/*
		char preprocessorContents[1024];
		m_tokenizer.GetRestofWholeline(preprocessorContents);
		strcpy(preProcessorExpression->contents, preprocessorContents);
		*/

		m_tokenizer.Next();

		field->preProcessor = preProcessor;

		return true;
	}
    if (!ExpectDeclaration(false, field->type, field->name))
    {
        return false;
    }
    // Handle optional semantics.
    if (Accept(':'))
    {
        if (!ExpectIdentifier(field->semantic))
        {
            return false;
        }
    }
    return Expect(';');
}

/*
bool HLSLParser::ParseSamplerStateFieldDeclaration(HLSLSamplerStateField*& field)
{
	field = m_tree->AddNode<HLSLSamplerStateField>(GetFileName(), GetLineNumber());
	if (!ExpectDeclaration(false, field->type, field->name))
	{
		return false;
	}

	return Expect(';');
}
*/
// @@ Add support for packoffset to general declarations.
/*bool HLSLParser::ParseBufferFieldDeclaration(HLSLBufferField*& field)
{
    field = m_tree->AddNode<HLSLBufferField>( GetFileName(), GetLineNumber() );
    if (AcceptDeclaration(false, field->type, field->name))
    {
        // Handle optional packoffset.
        if (Accept(':'))
        {
            if (!Expect("packoffset"))
            {
                return false;
            }
            const char* constantName = NULL;
            const char* swizzleMask  = NULL;
            if (!Expect('(') || !ExpectIdentifier(constantName) || !Expect('.') || !ExpectIdentifier(swizzleMask) || !Expect(')'))
            {
                return false;
            }
        }
        return Expect(';');
    }
    return false;
}*/

bool HLSLParser::CheckTypeCast(const HLSLType& srcType, const HLSLType& dstType)
{
    if (GetTypeCastRank(m_tree, srcType, dstType) == -1)
    {
        const char* srcTypeName = GetTypeName(srcType);
        const char* dstTypeName = GetTypeName(dstType);
        m_tokenizer.Error("Cannot implicitly convert from '%s' to '%s'", srcTypeName, dstTypeName);
        return false;
    }
    return true;
}

bool HLSLParser::ParseSamplerStateExpression(HLSLSamplerStateExpression*& expression)
{
	int lvalueType;

	//set l-value
	if (Check(HLSLToken_AddressU) ||
		Check(HLSLToken_AddressV) ||
		Check(HLSLToken_AddressW) ||
		Check(HLSLToken_BorderColor) ||
		Check(HLSLToken_Filter) ||
		Check(HLSLToken_MaxAnisotropy) ||
		Check(HLSLToken_MaxLOD) ||
		Check(HLSLToken_MinLOD) ||
		Check(HLSLToken_MipLODBias) ||
		Check(HLSLToken_ComparisonFunc))
	{
		lvalueType = m_tokenizer.GetToken();
		strcpy( expression->lvalue, m_tokenizer.GetIdentifier());
		m_tokenizer.Next();
	}
	else
		return false;

	if(!Expect('='))
		return false;

	//set r-value
	if((lvalueType == HLSLToken_AddressU) || (lvalueType == HLSLToken_AddressV) || (lvalueType == HLSLToken_AddressW))
	{
		if (Check(HLSLToken_WRAP) ||
			Check(HLSLToken_MIRROR) ||
			Check(HLSLToken_CLAMP) ||
			Check(HLSLToken_BORDER) ||
			Check(HLSLToken_MIRROR_ONCE))
		{
			strcpy(expression->rvalue, m_tokenizer.GetIdentifier());
			m_tokenizer.Next();
		}
	}
	else if (lvalueType == HLSLToken_Filter)
	{
		if (HLSLToken_MIN_MAG_MIP_POINT <= m_tokenizer.GetToken() && m_tokenizer.GetToken() <= HLSLToken_MAXIMUM_ANISOTROPIC)
		{
			strcpy(expression->rvalue, m_tokenizer.GetIdentifier());
			m_tokenizer.Next();
		}
	}
	else if (lvalueType == HLSLToken_ComparisonFunc)
	{
		if (HLSLToken_NEVER <= m_tokenizer.GetToken() && m_tokenizer.GetToken() <= HLSLToken_ALWAYS)
		{
			strcpy(expression->rvalue, m_tokenizer.GetIdentifier());
			m_tokenizer.Next();
		}
	}
	
	if (!Expect(';'))
		return false;

	return true;
}

bool HLSLParser::ParseExpression(HLSLExpression*& expression)
{
	if (HLSLToken_P_If <= m_tokenizer.GetToken() && HLSLToken_P_Error >= m_tokenizer.GetToken())
	{
		HLSLPreprocessorExpression * preProcessorExpression = m_tree->AddNode<HLSLPreprocessorExpression>(GetFileName(), GetLineNumber());

		switch (m_tokenizer.GetToken())
		{
		case HLSLToken_P_If: preProcessorExpression->type = HLSLBaseType_PreProcessorIf;
			break;
		case HLSLToken_P_Elif: preProcessorExpression->type = HLSLBaseType_PreProcessorElif;
			break;
		case HLSLToken_P_Else: preProcessorExpression->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessorExpression->type = HLSLBaseType_PreProcessorEndif;
			break;
		case HLSLToken_P_IfDef: preProcessorExpression->type = HLSLBaseType_PreProcessorIfDef;
			break;
		case HLSLToken_P_IfnDef: preProcessorExpression->type = HLSLBaseType_PreProcessorIfnDef;
			break;
		case HLSLToken_P_Undef: preProcessorExpression->type = HLSLBaseType_PreProcessorUndef;
			break;
		case HLSLToken_P_Include: preProcessorExpression->type = HLSLBaseType_PreProcessorInclude;
			break;
		case HLSLToken_P_Line: preProcessorExpression->type = HLSLBaseType_PreProcessorLine;
			break;
		case HLSLToken_P_Pragma: preProcessorExpression->type = HLSLBaseType_PreProcessorPragma;
			break;
		default:
			break;
		}
	
		preProcessorExpression->preprocessorType = m_tokenizer.GetToken();

		char preprocessorContents[1024];
		m_tokenizer.GetRestofWholeline(preprocessorContents);

		strcpy(preProcessorExpression->contents, preprocessorContents);
		m_tokenizer.Next();

		expression = preProcessorExpression;
		return true;
	}
	else if (HLSLToken_P_Else <= m_tokenizer.GetToken() && HLSLToken_P_Endif >= m_tokenizer.GetToken())
	{
		HLSLPreprocessorExpression * preProcessorExpression = m_tree->AddNode<HLSLPreprocessorExpression>(GetFileName(), GetLineNumber());
		
		switch (m_tokenizer.GetToken())
		{		
		case HLSLToken_P_Else: preProcessorExpression->type = HLSLBaseType_PreProcessorElse;
			break;
		case HLSLToken_P_Endif: preProcessorExpression->type = HLSLBaseType_PreProcessorEndif;
			break;
		default:
			break;
		}
				
		preProcessorExpression->preprocessorType = m_tokenizer.GetToken();

		
		m_tokenizer.Next();

		expression = preProcessorExpression;
		return true;
	}


    if (!ParseBinaryExpression(0, expression))
    {
        return false;
    }

    HLSLBinaryOp assignOp;
    if (AcceptAssign(assignOp))
    {
        HLSLExpression* expression2 = NULL;
        if (!ParseExpression(expression2))
        {
            return false;
        }
        HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(expression->fileName, expression->line);
        binaryExpression->binaryOp = assignOp;
        binaryExpression->expression1 = expression;
        binaryExpression->expression2 = expression2;
        // This type is not strictly correct, since the type should be a reference.
        // However, for our usage of the types it should be sufficient.
        binaryExpression->expressionType = expression->expressionType;

		//it can be from #define
		if (expression2->expressionType.baseType == HLSLBaseType_UserDefined)
		{
			HLSLPreprocessorExpression* preprecessorExpression = static_cast<HLSLPreprocessorExpression*>(expression2);

			if (preprecessorExpression != NULL)
			{
				for (int i = 0; i < m_preProcessors.GetSize(); i++)
				{
					if (String_Equal(m_preProcessors[i]->name, preprecessorExpression->name))
					{
						HLSLType newType;
						newType.flags = HLSLTypeFlag_Const;

						expression2->expressionType.baseType = preprecessorExpression->expressionType.baseType;
					}
				}
			}
		}

        if (!CheckTypeCast(expression2->expressionType, expression->expressionType))
        {
            const char* srcTypeName = GetTypeName(expression2->expressionType);
            const char* dstTypeName = GetTypeName(expression->expressionType);
            m_tokenizer.Error("Cannot implicitly convert from '%s' to '%s'", srcTypeName, dstTypeName);
            return false;
        }

        expression = binaryExpression;
    }


    return true;
}

bool HLSLParser::AcceptBinaryOperator(int priority, HLSLBinaryOp& binaryOp)
{
    int token = m_tokenizer.GetToken();
    switch (token)
    {
    case HLSLToken_AndAnd:          binaryOp = HLSLBinaryOp_And;          break;
    case HLSLToken_BarBar:          binaryOp = HLSLBinaryOp_Or;           break;
    case '+':                       binaryOp = HLSLBinaryOp_Add;          break;
    case '-':                       binaryOp = HLSLBinaryOp_Sub;          break;
    case '*':                       binaryOp = HLSLBinaryOp_Mul;          break;
    case '/':                       binaryOp = HLSLBinaryOp_Div;          break;
    case '<':                       binaryOp = HLSLBinaryOp_Less;         break;
    case '>':                       binaryOp = HLSLBinaryOp_Greater;      break;
    case HLSLToken_LessEqual:       binaryOp = HLSLBinaryOp_LessEqual;    break;
    case HLSLToken_GreaterEqual:    binaryOp = HLSLBinaryOp_GreaterEqual; break;
    case HLSLToken_EqualEqual:      binaryOp = HLSLBinaryOp_Equal;        break;
    case HLSLToken_NotEqual:        binaryOp = HLSLBinaryOp_NotEqual;     break;
    case '&':                       binaryOp = HLSLBinaryOp_BitAnd;       break;
    case '|':                       binaryOp = HLSLBinaryOp_BitOr;        break;
    case '^':                       binaryOp = HLSLBinaryOp_BitXor;       break;
	case HLSLToken_LeftShift:       binaryOp = HLSLBinaryOp_LeftShift;    break;
	case HLSLToken_RightShift:      binaryOp = HLSLBinaryOp_RightShift;   break;
    default:
        return false;
    }
    if (_binaryOpPriority[binaryOp] > priority)
    {
        m_tokenizer.Next();
        return true;
    }
    return false;
}

bool HLSLParser::AcceptUnaryOperator(bool pre, HLSLUnaryOp& unaryOp)
{
    int token = m_tokenizer.GetToken();
    if (token == HLSLToken_PlusPlus)
    {
        unaryOp = pre ? HLSLUnaryOp_PreIncrement : HLSLUnaryOp_PostIncrement;
    }
    else if (token == HLSLToken_MinusMinus)
    {
        unaryOp = pre ? HLSLUnaryOp_PreDecrement : HLSLUnaryOp_PostDecrement;
    }
    else if (pre && token == '-')
    {
        unaryOp = HLSLUnaryOp_Negative;
    }
    else if (pre && token == '+')
    {
        unaryOp = HLSLUnaryOp_Positive;
    }
    else if (pre && token == '!')
    {
        unaryOp = HLSLUnaryOp_Not;
    }
    else if (pre && token == '~')
    {
        unaryOp = HLSLUnaryOp_Not;
    }
    else
    {
        return false;
    }
    m_tokenizer.Next();
    return true;
}

bool HLSLParser::AcceptAssign(HLSLBinaryOp& binaryOp)
{
    if (Accept('='))
    {
        binaryOp = HLSLBinaryOp_Assign;
    }
    else if (Accept(HLSLToken_PlusEqual))
    {
        binaryOp = HLSLBinaryOp_AddAssign;
    }
    else if (Accept(HLSLToken_MinusEqual))
    {
        binaryOp = HLSLBinaryOp_SubAssign;
    }     
    else if (Accept(HLSLToken_TimesEqual))
    {
        binaryOp = HLSLBinaryOp_MulAssign;
    }     
    else if (Accept(HLSLToken_DivideEqual))
    {
        binaryOp = HLSLBinaryOp_DivAssign;
    }     
    else
    {
        return false;
    }
    return true;
}

bool HLSLParser::ParseBinaryExpression(int priority, HLSLExpression*& expression)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    bool needsEndParen;

    if (!ParseTerminalExpression(expression, needsEndParen))
    {
        return false;
    }

	// reset priority cause openned parenthesis
	if( needsEndParen )
		priority = 0;

    while (1)
    {
        HLSLBinaryOp binaryOp;
        if (AcceptBinaryOperator(priority, binaryOp))
        {

            HLSLExpression* expression2 = NULL;
            ASSERT( binaryOp < sizeof(_binaryOpPriority) / sizeof(int) );
            if (!ParseBinaryExpression(_binaryOpPriority[binaryOp], expression2))
            {
                return false;
            }
            HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(fileName, line);
            binaryExpression->binaryOp    = binaryOp;
            binaryExpression->expression1 = expression;
            binaryExpression->expression2 = expression2;

			//it can be from #define
			if (expression->expressionType.baseType == HLSLBaseType_UserDefined && expression2->expressionType.baseType == HLSLBaseType_UserDefined)
			{
				HLSLPreprocessorExpression* preprecessorExpression = static_cast<HLSLPreprocessorExpression*>(expression);
				HLSLPreprocessorExpression* preprecessorExpression2 = static_cast<HLSLPreprocessorExpression*>(expression2);

				if (preprecessorExpression != NULL && preprecessorExpression2 != NULL)
				{
					HLSLType newType;
					HLSLType newType2;

					bool findFirst = false;
					bool findSecond = false;

					for (int i = 0; i < m_preProcessors.GetSize(); i++)
					{
						if (String_Equal(m_preProcessors[i]->name, preprecessorExpression->name) && !findFirst)
						{
							expression->expressionType.baseType = m_preProcessors[i]->type;
							findFirst = true;
						}

						if (String_Equal(m_preProcessors[i]->name, preprecessorExpression2->name) && !findSecond)
						{
							expression2->expressionType.baseType = m_preProcessors[i]->type;
							findFirst = true;

							findSecond = true;
						}

						if (findFirst && findSecond)
						{
							if (!GetBinaryOpResultType(binaryOp, expression2->expressionType, expression2->expressionType, binaryExpression->expressionType))
							{
								const char* typeName1 = GetTypeName(binaryExpression->expression1->expressionType);
								const char* typeName2 = GetTypeName(binaryExpression->expression2->expressionType);
								m_tokenizer.Error("binary '%s' : no global operator found which takes types '%s' and '%s' (or there is no acceptable conversion)",
									GetBinaryOpName(binaryOp), typeName1, typeName2);

								return false;
							}
							else
								break;
						}
					}
				}
			}
			else if (expression->expressionType.baseType == HLSLBaseType_UserDefined)
			{
				HLSLPreprocessorExpression* preprecessorExpression = static_cast<HLSLPreprocessorExpression*>(expression);

				if (preprecessorExpression != NULL)
				{
					for (int i = 0; i < m_preProcessors.GetSize(); i++)
					{
						if (String_Equal(m_preProcessors[i]->name, preprecessorExpression->name))
						{
							expression->expressionType.baseType = m_preProcessors[i]->type;
							
							if (!GetBinaryOpResultType(binaryOp, expression->expressionType, expression2->expressionType, binaryExpression->expressionType))
							{
								const char* typeName1 = GetTypeName(binaryExpression->expression1->expressionType);
								const char* typeName2 = GetTypeName(binaryExpression->expression2->expressionType);
								m_tokenizer.Error("binary '%s' : no global operator found which takes types '%s' and '%s' (or there is no acceptable conversion)",
									GetBinaryOpName(binaryOp), typeName1, typeName2);

								return false;
							}
							else
								break;
						}
					}
				}
			}
			else if (expression2->expressionType.baseType == HLSLBaseType_UserDefined)
			{
				HLSLPreprocessorExpression* preprecessorExpression = static_cast<HLSLPreprocessorExpression*>(expression2);

				if (preprecessorExpression != NULL)
				{
					for (int i = 0; i < m_preProcessors.GetSize(); i++)
					{
						if (String_Equal(m_preProcessors[i]->name, preprecessorExpression->name))
						{
							expression2->expressionType.baseType = m_preProcessors[i]->type;

							if (!GetBinaryOpResultType(binaryOp, expression->expressionType, expression2->expressionType, binaryExpression->expressionType))
							{
								const char* typeName1 = GetTypeName(binaryExpression->expression1->expressionType);
								const char* typeName2 = GetTypeName(binaryExpression->expression2->expressionType);
								m_tokenizer.Error("binary '%s' : no global operator found which takes types '%s' and '%s' (or there is no acceptable conversion)",
									GetBinaryOpName(binaryOp), typeName1, typeName2);

								return false;
							}
							else
								break;
						}
					}
				}
			}
			else if (!GetBinaryOpResultType( binaryOp, expression->expressionType, expression2->expressionType, binaryExpression->expressionType ))
            {
                const char* typeName1 = GetTypeName( binaryExpression->expression1->expressionType );
                const char* typeName2 = GetTypeName( binaryExpression->expression2->expressionType );
                m_tokenizer.Error("binary '%s' : no global operator found which takes types '%s' and '%s' (or there is no acceptable conversion)",
                    GetBinaryOpName(binaryOp), typeName1, typeName2);

                return false;
            }

            expression = binaryExpression;
        }
        else if (_conditionalOpPriority > priority && Accept('?'))
        {

            HLSLConditionalExpression* conditionalExpression = m_tree->AddNode<HLSLConditionalExpression>(fileName, line);
            conditionalExpression->condition = expression;
            
            HLSLExpression* expression1 = NULL;
            HLSLExpression* expression2 = NULL;
            if (!ParseBinaryExpression(_conditionalOpPriority, expression1) || !Expect(':') || !ParseBinaryExpression(_conditionalOpPriority, expression2))
            {
                return false;
            }

            // Make sure both cases have compatible types.
            if (GetTypeCastRank(m_tree, expression1->expressionType, expression2->expressionType) == -1)
            {
                const char* srcTypeName = GetTypeName(expression2->expressionType);
                const char* dstTypeName = GetTypeName(expression1->expressionType);
                m_tokenizer.Error("':' no possible conversion from from '%s' to '%s'", srcTypeName, dstTypeName);
                return false;
            }

            conditionalExpression->trueExpression  = expression1;
            conditionalExpression->falseExpression = expression2;
            conditionalExpression->expressionType  = expression1->expressionType;

            expression = conditionalExpression;
        }
        else
        {
            break;
        }

		if( needsEndParen )
		{
			if( !Expect( ')' ) )
				return false;
			needsEndParen = false;
		}
    }

    return !needsEndParen || Expect(')');
}

bool HLSLParser::ParsePartialConstructor(HLSLExpression*& expression, HLSLBaseType type, const char* typeName)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    HLSLConstructorExpression* constructorExpression = m_tree->AddNode<HLSLConstructorExpression>(fileName, line);
    constructorExpression->type.baseType = type;
    constructorExpression->type.typeName = typeName;
    int numArguments = 0;
    if (!ParseExpressionList(')', false, constructorExpression->argument, numArguments))
    {
        return false;
    }    
    constructorExpression->expressionType = constructorExpression->type;
    constructorExpression->expressionType.flags = HLSLTypeFlag_Const;
    expression = constructorExpression;
    return true;
}

bool HLSLParser::ParseTerminalExpression(HLSLExpression*& expression, bool& needsEndParen)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    needsEndParen = false;

    HLSLUnaryOp unaryOp;
    if (AcceptUnaryOperator(true, unaryOp))
    {
        HLSLUnaryExpression* unaryExpression = m_tree->AddNode<HLSLUnaryExpression>(fileName, line);
        unaryExpression->unaryOp = unaryOp;
        if (!ParseTerminalExpression(unaryExpression->expression, needsEndParen))
        {
            return false;
        }
        if (unaryOp == HLSLUnaryOp_BitNot)
        {
            if (unaryExpression->expression->expressionType.baseType < HLSLBaseType_FirstInteger || 
                unaryExpression->expression->expressionType.baseType > HLSLBaseType_LastInteger)
            {
                const char * typeName = GetTypeName(unaryExpression->expression->expressionType);
                m_tokenizer.Error("unary '~' : no global operator found which takes type '%s' (or there is no acceptable conversion)", typeName);
                return false;
            }
        }
        if (unaryOp == HLSLUnaryOp_Not)
        {
            unaryExpression->expressionType = HLSLType(HLSLBaseType_Bool);
        }
        else
        {
            unaryExpression->expressionType = unaryExpression->expression->expressionType;
        }
        expression = unaryExpression;
        return true;
    }
    
    // Expressions inside parenthesis or casts.
    if (Accept('('))
    {
        // Check for a casting operator.
		
        HLSLType type;
        if (AcceptType(false, type.baseType, type.typeName, &type.flags))
        {
            // This is actually a type constructor like (float2(...
            if (Accept('('))
            {
                needsEndParen = true;
                return ParsePartialConstructor(expression, type.baseType, type.typeName);
            }
            HLSLCastingExpression* castingExpression = m_tree->AddNode<HLSLCastingExpression>(fileName, line);
            castingExpression->type = type;
            expression = castingExpression;
            castingExpression->expressionType = type;
			return Expect(')') && ParseExpression(castingExpression->expression);
        }
		
        if (!ParseExpression(expression) || !Expect(')'))
        {
            return false;
        }
    }
    else
    {
        // Terminal values.
        float fValue = 0.0f;
        int   iValue = 0;
		unsigned int   uiValue = 0;
        
        if (AcceptFloat(fValue))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Float;
            literalExpression->fValue = fValue;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }
		if( AcceptHalf( fValue ) )
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>( fileName, line );
			literalExpression->type = HLSLBaseType_Half;
			literalExpression->fValue = fValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return true;
		}
        else if (AcceptInt(iValue))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Int;
            literalExpression->iValue = iValue;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }
		else if (AcceptUint(uiValue))
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type = HLSLBaseType_Uint;
			literalExpression->uiValue = uiValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return true;
		}
		

        else if (Accept(HLSLToken_True))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Bool;
            literalExpression->bValue = true;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }
        else if (Accept(HLSLToken_False))
        {
            HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
            literalExpression->type   = HLSLBaseType_Bool;
            literalExpression->bValue = false;
            literalExpression->expressionType.baseType = literalExpression->type;
            literalExpression->expressionType.flags = HLSLTypeFlag_Const;
            expression = literalExpression;
            return true;
        }
		
		
		if (m_tokenizer.GetToken() == HLSLToken_Identifier)
		{
			//if it is preprocessor's m_identifier
			for (int i = 0; i < m_preProcessors.GetSize(); ++i)
			{				
				if (String_Equal(m_tokenizer.GetIdentifier(), m_preProcessors[i]->name ))
				{
					HLSLPreprocessorExpression* preprocessorExpression = m_tree->AddNode<HLSLPreprocessorExpression>(fileName, line);

					preprocessorExpression->expressionType.baseType = m_preProcessors[i]->type;
					strcpy(preprocessorExpression->name, m_preProcessors[i]->name);

					preprocessorExpression->expressionType.flags = HLSLTypeFlag_Const;
					expression = preprocessorExpression;

					m_tokenizer.Next();
					return true;
				}
			}
		}

        // Type constructor.
        HLSLBaseType    type;
        const char*     typeName = NULL;
        if (AcceptType(/*allowVoid=*/false, type, typeName, NULL))
        {
            Expect('(');
            if (!ParsePartialConstructor(expression, type, typeName))
            {
                return false;
            }
        }
		else if ( FindTextureStateDefinedType(m_tokenizer.GetIdentifier()) != NULL)
		{
			//if it is texture's m_identifier
			for (int i = 0; i < m_textureStates.GetSize(); ++i)
			{
				if (String_Equal(m_tokenizer.GetIdentifier(), m_textureStates[i]->name))
				{
					HLSLTextureStateExpression* textureStateExpression = m_tree->AddNode<HLSLTextureStateExpression>(fileName, line);
					//textureStateExpression->type = HLSLBaseType_TextureState;

					strcpy(textureStateExpression->name, m_textureStates[i]->name);

					textureStateExpression->expressionType.baseType = HLSLBaseType_TextureState;
					textureStateExpression->expressionType.flags = HLSLTypeFlag_Const;
					//expression = textureStateExpression;


					m_textureStateExpressions.PushBack(textureStateExpression);

					m_tokenizer.Next();

					textureStateExpression->arrayDimension = m_textureStates[i]->arrayDimension;

					//handle Array
					for (int index = 0; index < (int)m_textureStates[i]->arrayDimension ; index++)
					{
						textureStateExpression->bArray = true;

						if (Accept('['))
						{
							int innerIndex = 0;
							while (!Accept(']'))
							{
								if (!String_Equal(m_tokenizer.GetIdentifier(), ""))
								{
									strcpy(textureStateExpression->arrayIdentifier[innerIndex++], m_tokenizer.GetIdentifier());
									m_tokenizer.Next();
								}
								else
								{
									textureStateExpression->arrayIndex[innerIndex++] = m_tokenizer.GetuInt();
									m_tokenizer.Next();
								}
								
							}
						}
					}
					
					if (Accept('.'))
					{
						ParseExpression(expression);						
						return true;
					}
					else
						return false;
					break;
				}
			}
		}
		else if (FindRWTextureStateDefinedType(m_tokenizer.GetIdentifier()) != NULL)
		{
			//if it is rwtexture's m_identifier
			for (int i = 0; i < m_rwtextureStates.GetSize(); ++i)
			{
				if (String_Equal(m_tokenizer.GetIdentifier(), m_rwtextureStates[i]->name))
				{
					HLSLRWTextureStateExpression* rwtextureStateExpression = m_tree->AddNode<HLSLRWTextureStateExpression>(fileName, line);
					//textureStateExpression->type = HLSLBaseType_TextureState;

					strcpy(rwtextureStateExpression->name, m_rwtextureStates[i]->name);

					rwtextureStateExpression->expressionType.baseType = HLSLBaseType_RWTextureState;
					rwtextureStateExpression->expressionType.flags = HLSLTypeFlag_Const;
					rwtextureStateExpression->type = m_rwtextureStates[i]->baseType;
					//expression = rwtextureStateExpression;

					m_rwtextureStateExpressions.PushBack(rwtextureStateExpression);

					m_tokenizer.Next();

					rwtextureStateExpression->arrayDimension = m_rwtextureStates[i]->arrayDimension;

					//handle Array
					for (int index = 0; index < (int)m_rwtextureStates[i]->arrayDimension; index++)
					{
						rwtextureStateExpression->bArray = true;

						if (Accept('['))
						{
							int innerIndex = 0;
							while (!Accept(']'))
							{
								if (!String_Equal(m_tokenizer.GetIdentifier(), ""))
								{
									strcpy(rwtextureStateExpression->arrayIdentifier[innerIndex++], m_tokenizer.GetIdentifier());
									m_tokenizer.Next();
								}
								else
								{
									rwtextureStateExpression->arrayIndex[innerIndex++] = m_tokenizer.GetuInt();
									m_tokenizer.Next();
								}
							}
						}
					}

					//handle [] operator (read / write)
					if (Accept('['))
					{
						HLSLArrayAccess* arrayAccess = m_tree->AddNode<HLSLArrayAccess>(fileName, line);
						arrayAccess->array = rwtextureStateExpression;
						if (!ParseExpression(arrayAccess->index) || !Expect(']'))
						{
							return false;
						}

						if (rwtextureStateExpression->expressionType.array)
						{
							arrayAccess->expressionType = rwtextureStateExpression->expressionType;
							arrayAccess->expressionType.array = false;
							arrayAccess->expressionType.arraySize = NULL;
						}

						//arrayAccess->nodeType = HLSLNodeType_RWTextureStateExpression;
						arrayAccess->expressionType.baseType = m_rwtextureStates[i]->dataType;
						expression = arrayAccess;
						

						return true;
					}
					else if (Accept('.')) //Load()
					{
						ParseExpression(expression);
						//ParseExpression(rwtextureStateExpression->fuctionExpression);
						return true;
					}
					else
						return false;
					

					break;
				}
			}
		}
		else if (FindSamplerStateDefinedType(m_tokenizer.GetIdentifier()) != NULL)
		{
			//if it is SamplerState's m_identifier
			for (int i = 0; i < m_samplerStates.GetSize(); ++i)
			{
				if (String_Equal(m_tokenizer.GetIdentifier(), m_samplerStates[i]->name))
				{
					
					HLSLSamplerStateExpression* samplerStateExpression = m_tree->AddNode<HLSLSamplerStateExpression>(fileName, line);
				
					strcpy(samplerStateExpression->name, m_samplerStates[i]->name);

					samplerStateExpression->expressionType.baseType = HLSLBaseType_SamplerState;
					samplerStateExpression->expressionType.flags = HLSLTypeFlag_Const;
					expression = samplerStateExpression;
					
					m_tokenizer.Next();
					//break;
					return true;
				}
			}
		}
        else
        {
            HLSLIdentifierExpression* identifierExpression = m_tree->AddNode<HLSLIdentifierExpression>(fileName, line);
            if (!ExpectIdentifier(identifierExpression->name))
            {
                return false;
            }

            const HLSLType* identifierType = FindVariable(identifierExpression->name, identifierExpression->global);
            if (identifierType != NULL)
            {
                identifierExpression->expressionType = *identifierType;
            }
            else
            {
                if (!GetIsFunction(identifierExpression->name))
                {
                    m_tokenizer.Error("Undeclared identifier '%s'", identifierExpression->name);
                    return false;
                }
                // Functions are always global scope.
                identifierExpression->global = true;
            }

            expression = identifierExpression;
        }
    }

    bool done = false;
    while (!done)
    {
        done = true;

        // Post fix unary operator
        HLSLUnaryOp unaryOp;
        while (AcceptUnaryOperator(false, unaryOp))
        {
            HLSLUnaryExpression* unaryExpression = m_tree->AddNode<HLSLUnaryExpression>(fileName, line);
            unaryExpression->unaryOp = unaryOp;
            unaryExpression->expression = expression;
            unaryExpression->expressionType = unaryExpression->expression->expressionType;
            expression = unaryExpression;
            done = false;
        }

        // Member access operator.
        while (Accept('.'))
        {
            HLSLMemberAccess* memberAccess = m_tree->AddNode<HLSLMemberAccess>(fileName, line);
            memberAccess->object = expression;
            if (!ExpectIdentifier(memberAccess->field))
            {
                return false;
            }

            if (!GetMemberType( expression->expressionType, memberAccess))
            {
                m_tokenizer.Error("Couldn't access '%s'", memberAccess->field);
                return false;
            }
            expression = memberAccess;
            done = false;
        }

        // Handle array access.
        while (Accept('['))
        {
            HLSLArrayAccess* arrayAccess = m_tree->AddNode<HLSLArrayAccess>(fileName, line);
            arrayAccess->array = expression;
            if (!ParseExpression(arrayAccess->index) || !Expect(']'))
            {
                return false;
            }

            if (expression->expressionType.array)
            {
                arrayAccess->expressionType = expression->expressionType;
                arrayAccess->expressionType.array     = false;
                arrayAccess->expressionType.arraySize = NULL;
            }
            else
            {
                switch (expression->expressionType.baseType)
                {
                case HLSLBaseType_Float2:
                case HLSLBaseType_Float3:
                case HLSLBaseType_Float4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float;
                    break;
				case HLSLBaseType_Float2x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Float2;
					break;
                case HLSLBaseType_Float3x3:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float3;
                    break;
                case HLSLBaseType_Float4x4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float4;
                    break;
                case HLSLBaseType_Float4x3:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float3;
                    break;
                case HLSLBaseType_Float4x2:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Float2;
                    break;
                case HLSLBaseType_Half2:
                case HLSLBaseType_Half3:
                case HLSLBaseType_Half4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half;
                    break;
				case HLSLBaseType_Half2x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Half2;
					break;
                case HLSLBaseType_Half3x3:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half3;
                    break;
                case HLSLBaseType_Half4x4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half4;
                    break;
                case HLSLBaseType_Half4x3:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half3;
                    break;
                case HLSLBaseType_Half4x2:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Half2;
                    break;
                case HLSLBaseType_Int2:
                case HLSLBaseType_Int3:
                case HLSLBaseType_Int4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Int;
                    break;
                case HLSLBaseType_Uint2:
                case HLSLBaseType_Uint3:
                case HLSLBaseType_Uint4:
                    arrayAccess->expressionType.baseType = HLSLBaseType_Uint;
                    break;
                default:
                    m_tokenizer.Error("array, matrix, vector, or indexable object type expected in index expression");
                    return false;
                }
            }

            expression = arrayAccess;
            done = false;
        }

        // Handle function calls. Note, HLSL functions aren't like C function
        // pointers -- we can only directly call on an identifier, not on an
        // expression.
        if (Accept('('))
        {
            HLSLFunctionCall* functionCall = m_tree->AddNode<HLSLFunctionCall>(fileName, line);
            done = false;
            if (!ParseExpressionList(')', false, functionCall->argument, functionCall->numArguments))
            {
                return false;
            }

            if (expression->nodeType != HLSLNodeType_IdentifierExpression)
            {
                m_tokenizer.Error("Expected function identifier");
                return false;
            }

            const HLSLIdentifierExpression* identifierExpression = static_cast<const HLSLIdentifierExpression*>(expression);
            const HLSLFunction* function = MatchFunctionCall( functionCall, identifierExpression->name );
            if (function == NULL)
            {
                return false;
            }

            functionCall->function = function;

			//this is only for Read only textures
			if (String_Equal("Sample", identifierExpression->name) ||
				String_Equal("SampleBias", identifierExpression->name) ||
				String_Equal("SampleLevel", identifierExpression->name))
			{
				//find closest Texture's identifier from identifier History
				for (int i = m_tokenizer.GetHistoryCounter(); i >= 0; i--)
				{
					const HLSLTextureState* pTextureState = FindTextureStateDefinedType(m_tokenizer.GetPrevIdentifier(i));

					if (pTextureState)
					{
						//char functionCaller[256];
						//FindClosestTextureIdentifier(pTextureState, functionCaller,  functionCall, i, identifierExpression->name);

						HLSLTextureStateExpression* pTextureStateExpression = m_textureStateExpressions.PopBack();
						pTextureStateExpression->type = pTextureState->baseType;
						functionCall->pTextureStateExpression = pTextureStateExpression;
							
						break;
					}
				}
			}
			else if (String_Equal("Load", identifierExpression->name))
			{
				//find closest Texture's identifier from identifier History
				for (int i = m_tokenizer.GetHistoryCounter(); i >= 0; i--)
				{
					const HLSLTextureState* pTextureState = FindTextureStateDefinedType(m_tokenizer.GetPrevIdentifier(i));

					if (pTextureState)
					{
						//char functionCaller[256];
						//FindClosestTextureIdentifier(pTextureState, functionCaller, functionCall, i, identifierExpression->name);

						//functionCall->pTextureState = pTextureState;

						HLSLTextureStateExpression* pTextureStateExpression = m_textureStateExpressions.PopBack();
						pTextureStateExpression->type = pTextureState->baseType;
						functionCall->pTextureStateExpression = pTextureStateExpression;

						break;
					}

					const HLSLRWTextureState* pRWTextureState = FindRWTextureStateDefinedType(m_tokenizer.GetPrevIdentifier(i));

					if (pRWTextureState)
					{
						//char functionCaller[256];
						//FindClosestTextureIdentifier(pRWTextureState, functionCaller, functionCall, i, identifierExpression->name);

						//functionCall->pRWTextureState = pRWTextureState;

						//functionCall->bTextureFunction = true;
						//functionCall->textureType = pTextureState->baseType;

						HLSLRWTextureStateExpression* prwTextureStateExpression = m_rwtextureStateExpressions.PopBack();
						prwTextureStateExpression->type = pRWTextureState->baseType;
						functionCall->pRWTextureStateExpression = prwTextureStateExpression;

						break;
					}
				}
			}

            functionCall->expressionType = function->returnType;
            expression = functionCall;
        }

    }
    return true;

}

bool HLSLParser::ParseExpressionList(int endToken, bool allowEmptyEnd, HLSLExpression*& firstExpression, int& numExpressions)
{
    numExpressions = 0;
    HLSLExpression* lastExpression = NULL;
    while (!Accept(endToken))
    {
        if (CheckForUnexpectedEndOfStream(endToken))
        {
            return false;
        }
        if (numExpressions > 0 && !Expect(','))
        {
            return false;
        }
        // It is acceptable for the final element in the initialization list to
        // have a trailing comma in some cases, like array initialization such as {1, 2, 3,}
        if (allowEmptyEnd && Accept(endToken))
        {
            break;
        }
        HLSLExpression* expression = NULL;
        if (!ParseExpression(expression))
        {
            return false;
        }
        if (firstExpression == NULL)
        {
            firstExpression = expression;
        }
        else
        {
            lastExpression->nextExpression = expression;
        }
        lastExpression = expression;
        ++numExpressions;
    }
    return true;
}

bool HLSLParser::ParseArgumentList(HLSLArgument*& firstArgument, int& numArguments)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();
        
    HLSLArgument* lastArgument = NULL;
    numArguments = 0;

    while (!Accept(')'))
    {
        if (CheckForUnexpectedEndOfStream(')'))
        {
            return false;
        }
        if (numArguments > 0 && !Expect(','))
        {
            return false;
        }

        HLSLArgument* argument = m_tree->AddNode<HLSLArgument>(fileName, line);

        if (Accept(HLSLToken_Uniform))     { argument->modifier = HLSLArgumentModifier_Uniform; }
        else if (Accept(HLSLToken_In))     { argument->modifier = HLSLArgumentModifier_In;      }
        else if (Accept(HLSLToken_Out))    { argument->modifier = HLSLArgumentModifier_Out;     }
        else if (Accept(HLSLToken_InOut))  { argument->modifier = HLSLArgumentModifier_Inout;   }
        else if (Accept(HLSLToken_Const))  { argument->modifier = HLSLArgumentModifier_Const;   }

		else if (Accept(HLSLToken_Point)) { argument->modifier = HLSLArgumentModifier_Point; }
		else if (Accept(HLSLToken_Line)) { argument->modifier = HLSLArgumentModifier_Line; }
		else if (Accept(HLSLToken_Triangle)) { argument->modifier = HLSLArgumentModifier_Triangle; }
		else if (Accept(HLSLToken_Lineadj)) { argument->modifier = HLSLArgumentModifier_Lineadj; }
		else if (Accept(HLSLToken_Triangleadj)) { argument->modifier = HLSLArgumentModifier_Triangleadj; }

        if (!ExpectDeclaration(/*allowUnsizedArray=*/true, argument->type, argument->name))
        {
            return false;
        }

        DeclareVariable( argument->name, argument->type );

        // Optional semantic.
        if (Accept(':') && !ExpectIdentifier(argument->semantic))
        {
            return false;
        }

        if (Accept('=') && !ParseExpression(argument->defaultValue))
        {
            // @@ Print error!
            return false;
        }

        if (lastArgument != NULL)
        {
            lastArgument->nextArgument = argument;
        }
        else
        {
            firstArgument = argument;
        }
        lastArgument = argument;

        ++numArguments;
    }
    return true;
}


bool HLSLParser::ParseSamplerState(HLSLExpression*& expression)
{
    if (!Expect(HLSLToken_SamplerState))
    {
        return false;
    }

    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    HLSLSamplerState* samplerState = m_tree->AddNode<HLSLSamplerState>(fileName, line);

    if (!Expect('{'))
    {
        return false;
    }

    HLSLStateAssignment* lastStateAssignment = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLStateAssignment* stateAssignment = NULL;
        if (!ParseStateAssignment(stateAssignment, /*isSamplerState=*/true, /*isPipeline=*/false))
        {
            return false;
        }
        ASSERT(stateAssignment != NULL);
        if (lastStateAssignment == NULL)
        {
            samplerState->stateAssignments = stateAssignment;
        }
        else
        {
            lastStateAssignment->nextStateAssignment = stateAssignment;
        }
        lastStateAssignment = stateAssignment;
        samplerState->numStateAssignments++;
    }

    //expression = samplerState;
    return true;
}

bool HLSLParser::ParseTechnique(HLSLStatement*& statement)
{
    if (!Accept(HLSLToken_Technique)) {
        return false;
    }

    const char* techniqueName = NULL;
    if (!ExpectIdentifier(techniqueName))
    {
        return false;
    }

    if (!Expect('{'))
    {
        return false;
    }

    HLSLTechnique* technique = m_tree->AddNode<HLSLTechnique>(GetFileName(), GetLineNumber());
    technique->name = techniqueName;

    //m_techniques.PushBack(technique);

    HLSLPass* lastPass = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLPass* pass = NULL;
        if (!ParsePass(pass))
        {
            return false;
        }
        ASSERT(pass != NULL);
        if (lastPass == NULL)
        {
            technique->passes = pass;
        }
        else
        {
            lastPass->nextPass = pass;
        }
        lastPass = pass;
        technique->numPasses++;
    }

    statement = technique;
    return true;
}

bool HLSLParser::ParsePass(HLSLPass*& pass)
{
    if (!Accept(HLSLToken_Pass)) {
        return false;
    }

    // Optional pass name.
    const char* passName = NULL;
    AcceptIdentifier(passName);

    if (!Expect('{'))
    {
        return false;
    }

    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    pass = m_tree->AddNode<HLSLPass>(fileName, line);
    pass->name = passName;

    HLSLStateAssignment* lastStateAssignment = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLStateAssignment* stateAssignment = NULL;
        if (!ParseStateAssignment(stateAssignment, /*isSamplerState=*/false, /*isPipelineState=*/false))
        {
            return false;
        }
        ASSERT(stateAssignment != NULL);
        if (lastStateAssignment == NULL)
        {
            pass->stateAssignments = stateAssignment;
        }
        else
        {
            lastStateAssignment->nextStateAssignment = stateAssignment;
        }
        lastStateAssignment = stateAssignment;
        pass->numStateAssignments++;
    }
    return true;
}


bool HLSLParser::ParsePipeline(HLSLStatement*& statement)
{
    if (!Accept("pipeline")) {
        return false;
    }

    // Optional pipeline name.
    const char* pipelineName = NULL;
    AcceptIdentifier(pipelineName);

    if (!Expect('{'))
    {
        return false;
    }

    HLSLPipeline* pipeline = m_tree->AddNode<HLSLPipeline>(GetFileName(), GetLineNumber());
    pipeline->name = pipelineName;

    HLSLStateAssignment* lastStateAssignment = NULL;

    // Parse state assignments.
    while (!Accept('}'))
    {
        if (CheckForUnexpectedEndOfStream('}'))
        {
            return false;
        }

        HLSLStateAssignment* stateAssignment = NULL;
        if (!ParseStateAssignment(stateAssignment, /*isSamplerState=*/false, /*isPipeline=*/true))
        {
            return false;
        }
        ASSERT(stateAssignment != NULL);
        if (lastStateAssignment == NULL)
        {
            pipeline->stateAssignments = stateAssignment;
        }
        else
        {
            lastStateAssignment->nextStateAssignment = stateAssignment;
        }
        lastStateAssignment = stateAssignment;
        pipeline->numStateAssignments++;
    }

    statement = pipeline;
    return true;
}


const EffectState* GetEffectState(const char* name, bool isSamplerState, bool isPipeline)
{
    const EffectState* validStates = effectStates;
    int count = sizeof(effectStates)/sizeof(effectStates[0]);
    
    if (isPipeline)
    {
        validStates = pipelineStates;
        count = sizeof(pipelineStates) / sizeof(pipelineStates[0]);
    }

    if (isSamplerState)
    {
        validStates = samplerStates;
        count = sizeof(samplerStates)/sizeof(samplerStates[0]);
    }

    // Case insensitive comparison.
    for (int i = 0; i < count; i++)
    {
        if (String_EqualNoCase(name, validStates[i].name)) 
        {
            return &validStates[i];
        }
    }

    return NULL;
}

static const EffectStateValue* GetStateValue(const char* name, const EffectState* state)
{
    // Case insensitive comparison.
    for (int i = 0; ; i++) 
    {
        const EffectStateValue & value = state->values[i];
        if (value.name == NULL) break;

        if (String_EqualNoCase(name, value.name)) 
        {
            return &value;
        }
    }

    return NULL;
}


bool HLSLParser::ParseStateName(bool isSamplerState, bool isPipelineState, const char*& name, const EffectState *& state)
{
    if (m_tokenizer.GetToken() != HLSLToken_Identifier)
    {
        char near[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(near);
        m_tokenizer.Error("Syntax error: expected identifier near '%s'", near);
        return false;
    }

    state = GetEffectState(m_tokenizer.GetIdentifier(), isSamplerState, isPipelineState);
    if (state == NULL)
    {
        m_tokenizer.Error("Syntax error: unexpected identifier '%s'", m_tokenizer.GetIdentifier());
        return false;
    }

    m_tokenizer.Next();
    return true;
}

bool HLSLParser::ParseColorMask(int& mask)
{
    mask = 0;

    do {
        if (m_tokenizer.GetToken() == HLSLToken_IntLiteral) {
            mask |= m_tokenizer.GetInt();
        }
		else if (m_tokenizer.GetToken() == HLSLToken_UintLiteral) {
			mask |= m_tokenizer.GetuInt();
		}
        else if (m_tokenizer.GetToken() == HLSLToken_Identifier) {
            const char * ident = m_tokenizer.GetIdentifier();
            const EffectStateValue * stateValue = colorMaskValues;
            while (stateValue->name != NULL) {
                if (String_EqualNoCase(stateValue->name, ident)) {
                    mask |= stateValue->value;
                    break;
                }
                ++stateValue;
            }
        }
        else {
            return false;
        }
        m_tokenizer.Next();
    } while (Accept('|'));

    return true;
}

bool HLSLParser::ParseStateValue(const EffectState * state, HLSLStateAssignment* stateAssignment)
{
    const bool expectsExpression = state->values == colorMaskValues;
    const bool expectsInteger = state->values == integerValues;
	const bool expectsUInteger = state->values == uintegerValues;

    const bool expectsFloat = state->values == floatValues;
    const bool expectsBoolean = state->values == booleanValues;

    if (!expectsExpression && !expectsInteger && !expectsUInteger && !expectsFloat && !expectsBoolean)
    {
        if (m_tokenizer.GetToken() != HLSLToken_Identifier)
        {
            char near[HLSLTokenizer::s_maxIdentifier];
            m_tokenizer.GetTokenName(near);
            m_tokenizer.Error("Syntax error: expected identifier near '%s'", near);
            stateAssignment->iValue = 0;
            return false;
        }
    }

    if (state->values == NULL)
    {
        if (String_Equal(m_tokenizer.GetIdentifier(), "compile"))
        {
            m_tokenizer.Error("Syntax error: unexpected identifier '%s' expected compile statement", m_tokenizer.GetIdentifier());
            stateAssignment->iValue = 0;
            return false;
        }

        // @@ Parse profile name, function name, argument expressions.

        // Skip the rest of the compile statement.
        while(m_tokenizer.GetToken() != ';')
        {
            m_tokenizer.Next();
        }
    }
    else {
        if (expectsInteger)
        {
            if (!AcceptInt(stateAssignment->iValue))
            {
                m_tokenizer.Error("Syntax error: expected integer near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
		if (expectsUInteger)
		{
			if (!AcceptUint(stateAssignment->uiValue))
			{
				m_tokenizer.Error("Syntax error: expected integer near '%s'", m_tokenizer.GetIdentifier());
				stateAssignment->uiValue = 0;
				return false;
			}
		}
        else if (expectsFloat)
        {
            if (!AcceptFloat(stateAssignment->fValue))
            {
                m_tokenizer.Error("Syntax error: expected float near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
        else if (expectsBoolean)
        {
            const EffectStateValue * stateValue = GetStateValue(m_tokenizer.GetIdentifier(), state);

            if (stateValue != NULL)
            {
                stateAssignment->iValue = stateValue->value;

                m_tokenizer.Next();
            }
            else if (AcceptInt(stateAssignment->iValue))
            {
                stateAssignment->iValue = (stateAssignment->iValue != 0);
            }
			else if (AcceptUint(stateAssignment->uiValue))
			{
				stateAssignment->uiValue = (stateAssignment->uiValue != 0);
			}
            else {
                m_tokenizer.Error("Syntax error: expected bool near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
        else if (expectsExpression)
        {
            if (!ParseColorMask(stateAssignment->iValue))
            {
                m_tokenizer.Error("Syntax error: expected color mask near '%s'", m_tokenizer.GetIdentifier());
                stateAssignment->iValue = 0;
                return false;
            }
        }
        else 
        {
            // Expect one of the allowed values.
            const EffectStateValue * stateValue = GetStateValue(m_tokenizer.GetIdentifier(), state);

            if (stateValue == NULL)
            {
                m_tokenizer.Error("Syntax error: unexpected value '%s' for state '%s'", m_tokenizer.GetIdentifier(), state->name);
                stateAssignment->iValue = 0;
                return false;
            }

            stateAssignment->iValue = stateValue->value;

            m_tokenizer.Next();
        }
    }

    return true;
}

bool HLSLParser::ParseStateAssignment(HLSLStateAssignment*& stateAssignment, bool isSamplerState, bool isPipelineState)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();

    stateAssignment = m_tree->AddNode<HLSLStateAssignment>(fileName, line);

    const EffectState * state;
    if (!ParseStateName(isSamplerState, isPipelineState, stateAssignment->stateName, state)) {
        return false;
    }

    //stateAssignment->name = m_tree->AddString(m_tokenizer.GetIdentifier());
    stateAssignment->stateName = state->name;
    stateAssignment->d3dRenderState = state->d3drs;

    if (!Expect('=')) {
        return false;
    }

    if (!ParseStateValue(state, stateAssignment)) {
        return false;
    }

    if (!Expect(';')) {
        return false;
    }

    return true;
}


bool HLSLParser::ParseAttributeList(HLSLAttribute*& firstAttribute)
{
    const char* fileName = GetFileName();
    int         line     = GetLineNumber();
    
    HLSLAttribute * lastAttribute = firstAttribute;
    do {
        const char * identifier = NULL;
        if (!ExpectIdentifier(identifier)) {
            return false;
        }

        HLSLAttribute * attribute = m_tree->AddNode<HLSLAttribute>(fileName, line);
        
        if (strcmp(identifier, "unroll") == 0) 
		{
			attribute->attributeType = HLSLAttributeType_Unroll;

			if (!Expect('('))
				return false;

			attribute->unrollCount = m_tokenizer.GetuInt();
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;

		}
        else if (strcmp(identifier, "flatten") == 0) attribute->attributeType = HLSLAttributeType_Flatten;
        else if (strcmp(identifier, "branch") == 0) attribute->attributeType = HLSLAttributeType_Branch;
		else if (strcmp(identifier, "numthreads") == 0)
		{
			attribute->attributeType = HLSLAttributeType_NumThreads;
			
			if (!Expect('('))
				return false;
			
			attribute->numGroupX = m_tokenizer.GetuInt();
			m_tokenizer.Next();

			if (!Expect(','))
				return false;

			attribute->numGroupY = m_tokenizer.GetuInt();
			m_tokenizer.Next();

			if (!Expect(','))
				return false;

			attribute->numGroupZ = m_tokenizer.GetuInt();
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;

		}
		else if (strcmp(identifier, "maxvertexcount") == 0)
		{
			attribute->attributeType = HLSLAttributeType_MaxVertexCount;

			if (!Expect('('))
				return false;

			attribute->maxVertexCount = m_tokenizer.GetuInt();
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;
		}
		else if (strcmp(identifier, "domain") == 0)
		{
			attribute->attributeType = HLSLAttributeType_Domain;

			if (!Expect('('))
				return false;

			strcpy(attribute->domain, m_tokenizer.GetIdentifier());
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;
		}
		else if (strcmp(identifier, "partitioning") == 0)
		{
			attribute->attributeType = HLSLAttributeType_Partitioning;

			if (!Expect('('))
				return false;

			strcpy(attribute->partitioning, m_tokenizer.GetIdentifier());
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;
		}
		else if (strcmp(identifier, "outputtopology") == 0)
		{
			attribute->attributeType = HLSLAttributeType_OutputTopology;

			if (!Expect('('))
				return false;

			strcpy(attribute->outputtopology, m_tokenizer.GetIdentifier());
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;
		}
		else if (strcmp(identifier, "outputcontrolpoints") == 0)
		{
			attribute->attributeType = HLSLAttributeType_OutputControlPoints;

			if (!Expect('('))
				return false;

			attribute->outputcontrolpoints = m_tokenizer.GetuInt();
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;
		}
		else if (strcmp(identifier, "patchconstantfunc") == 0)
		{
			attribute->attributeType = HLSLAttributeType_PatchConstantFunc;

			if (!Expect('('))
				return false;

			strcpy(attribute->patchconstantfunc, m_tokenizer.GetIdentifier());

			char tempFuncName[128];
			strcpy(tempFuncName, attribute->patchconstantfunc + 1);
			tempFuncName[strlen(tempFuncName)-1] = NULL;

			HLSLFunction* function = FindFunction(tempFuncName);

			if (function)
			{
				//function->bPatchconstantfunc = true;

				//MainEntry
				//HLSLFunction* entryFunction = FindFunction(m_entryName);
				//entryFunction->bPatchconstantfunc;
			}
			else
				return false;


			m_tokenizer.Next();

			if (!Expect(')'))
				return false;
		}
		else if (strcmp(identifier, "maxtessfactor") == 0)
		{
			attribute->attributeType = HLSLAttributeType_MaxtessFactor;

			if (!Expect('('))
				return false;

			attribute->maxTessellationFactor = m_tokenizer.GetFloat();
			m_tokenizer.Next();

			if (!Expect(')'))
				return false;
		}

		

        // @@ parse arguments, () not required if attribute constructor has no arguments.

        if (firstAttribute == NULL)
        {
            firstAttribute = attribute;
        }
        else
        {
            lastAttribute->nextAttribute = attribute;
        }
        lastAttribute = attribute;
        
    } while(Accept(','));

    return true;
}

// Attributes can have all these forms:
//   [A] statement;
//   [A,B] statement;
//   [A][B] statement;
// These are not supported yet:
//   [A] statement [B];
//   [A()] statement;
//   [A(a)] statement;
bool HLSLParser::ParseAttributeBlock(HLSLAttribute*& attribute)
{
    HLSLAttribute ** lastAttribute = &attribute;
    while (*lastAttribute != NULL) { lastAttribute = &(*lastAttribute)->nextAttribute; }

    if (!Accept('['))
    {
        return false;
    }

    // Parse list of attribute constructors.
    ParseAttributeList(*lastAttribute);

    if (!Expect(']'))
    {
        return false;
    }

    // Parse additional [] blocks.
    ParseAttributeBlock(*lastAttribute);

    return true;
}

bool HLSLParser::ParseStage(HLSLStatement*& statement)
{
    if (!Accept("stage"))
    {
        return false;
    }

    // Required stage name.
    const char* stageName = NULL;
    if (!ExpectIdentifier(stageName))
    {
        return false;
    }

    if (!Expect('{'))
    {
        return false;
    }

    HLSLStage* stage = m_tree->AddNode<HLSLStage>(GetFileName(), GetLineNumber());
    stage->name = stageName;

    BeginScope();

    HLSLType voidType(HLSLBaseType_Void);
    if (!Expect('{') || !ParseBlock(stage->statement, voidType))
    {
        return false;
    }

    EndScope();

    // @@ To finish the stage definition we should traverse the statements recursively (including function calls) and find all the input/output declarations.

    statement = stage;
    return true;
}




bool HLSLParser::Parse(HLSLTree* tree, PreprocessorPackage packageArray[])
{
    m_tree = tree;
    
    HLSLRoot* root = m_tree->GetRoot();
    HLSLStatement* lastStatement = NULL;

    while (!Accept(HLSLToken_EndOfStream))
    {
        HLSLStatement* statement = NULL;
        if (!ParseTopLevel(statement))
        {
            return false;
        }
        if (statement != NULL)
        {   
            if (lastStatement == NULL)
            {
                root->statement = statement;
            }
            else
            {
                lastStatement->nextStatement = statement;
            }
            lastStatement = statement;
        }
    }

	// copy the preprocessors
	/*
	for (int i = 0; i < 128; i++)
	{
		if (m_tokenizer.preprocessorPackage[i].m_line <= 0)
			break;

		strcpy(packageArray[i].m_storedPreprocessors, m_tokenizer.preprocessorPackage[i].m_storedPreprocessors);
		packageArray[i].m_line = m_tokenizer.preprocessorPackage[i].m_line;
	}
	*/

    return true;
}

bool HLSLParser::AcceptTypeModifier(int& flags)
{
    if (Accept(HLSLToken_Const))
    {
        flags |= HLSLTypeFlag_Const;
        return true;
    }
    else if (Accept(HLSLToken_Static))
    {
        flags |= HLSLTypeFlag_Static;
        return true;
    }
    else if (Accept(HLSLToken_Uniform))
    {
        //flags |= HLSLTypeFlag_Uniform;      // @@ Ignored.
        return true;
    }
    else if (Accept(HLSLToken_Inline))
    {
        //flags |= HLSLTypeFlag_Uniform;      // @@ Ignored. In HLSL all functions are inline.
        return true;
    }
    /*else if (Accept("in"))
    {
        flags |= HLSLTypeFlag_Input;
        return true;
    }
    else if (Accept("out"))
    {
        flags |= HLSLTypeFlag_Output;
        return true;
    }*/

    // Not an usage keyword.
    return false;
}

bool HLSLParser::AcceptInterpolationModifier(int& flags)
{
    if (Accept("linear"))
    { 
        flags |= HLSLTypeFlag_Linear; 
        return true;
    }
    else if (Accept("centroid"))
    { 
        flags |= HLSLTypeFlag_Centroid;
        return true;
    }
    else if (Accept("nointerpolation"))
    {
        flags |= HLSLTypeFlag_NoInterpolation;
        return true;
    }
    else if (Accept("noperspective"))
    {
        flags |= HLSLTypeFlag_NoPerspective;
        return true;
    }
    else if (Accept("sample"))
    {
        flags |= HLSLTypeFlag_Sample;
        return true;
    }

    return false;
}


bool HLSLParser::AcceptType(bool allowVoid, HLSLBaseType& type, const char*& typeName, int* typeFlags)
{
    if (typeFlags != NULL) {
        *typeFlags = 0;
        while(AcceptTypeModifier(*typeFlags) || AcceptInterpolationModifier(*typeFlags)) {}
    }

    int token = m_tokenizer.GetToken();

    // Check built-in types.
    type = HLSLBaseType_Void;
    switch (token)
    {
	case HLSLToken_Bool:
		type = HLSLBaseType_Bool; break;
	case HLSLToken_Bool1x2:
		type = HLSLBaseType_Bool1x2; break;
	case HLSLToken_Bool1x3:
		type = HLSLBaseType_Bool1x3; break;
	case HLSLToken_Bool1x4:
		type = HLSLBaseType_Bool1x4; break;

	case HLSLToken_Bool2:
		type = HLSLBaseType_Bool2; break;
	case HLSLToken_Bool2x2:
		type = HLSLBaseType_Bool2x2; break;
	case HLSLToken_Bool2x3:
		type = HLSLBaseType_Bool2x3; break;
	case HLSLToken_Bool2x4:
		type = HLSLBaseType_Bool2x4; break;

	case HLSLToken_Bool3:
		type = HLSLBaseType_Bool3; break;
	case HLSLToken_Bool3x2:
		type = HLSLBaseType_Bool3x2; break;
	case HLSLToken_Bool3x3:
		type = HLSLBaseType_Bool3x3; break;
	case HLSLToken_Bool3x4:
		type = HLSLBaseType_Bool3x4; break;

	case HLSLToken_Bool4:
		type = HLSLBaseType_Bool4; break;
	case HLSLToken_Bool4x2:
		type = HLSLBaseType_Bool4x2; break;
	case HLSLToken_Bool4x3:
		type = HLSLBaseType_Bool4x3; break;
	case HLSLToken_Bool4x4:
		type = HLSLBaseType_Bool4x4; break;

    case HLSLToken_Float:
        type = HLSLBaseType_Float; break;
	case HLSLToken_Float1x2:
		type = HLSLBaseType_Float1x2; break;
	case HLSLToken_Float1x3:
		type = HLSLBaseType_Float1x3; break;
	case HLSLToken_Float1x4:
		type = HLSLBaseType_Float1x4; break;

    case HLSLToken_Float2:      
        type = HLSLBaseType_Float2; break;
	case HLSLToken_Float2x2:
		type = HLSLBaseType_Float2x2; break;
	case HLSLToken_Float2x3:
		type = HLSLBaseType_Float2x3; break;
	case HLSLToken_Float2x4:
		type = HLSLBaseType_Float2x4; break;

    case HLSLToken_Float3:
        type = HLSLBaseType_Float3; break;
	case HLSLToken_Float3x2:
		type = HLSLBaseType_Float3x2; break;
	case HLSLToken_Float3x3:
		type = HLSLBaseType_Float3x3; break;
	case HLSLToken_Float3x4:
		type = HLSLBaseType_Float3x4; break;

    case HLSLToken_Float4:
        type = HLSLBaseType_Float4; break;
	case HLSLToken_Float4x2:
		type = HLSLBaseType_Float4x2; break;
	case HLSLToken_Float4x3:
		type = HLSLBaseType_Float4x3; break;
	case HLSLToken_Float4x4:
		type = HLSLBaseType_Float4x4; break;

	
    case HLSLToken_Half:
        type = HLSLBaseType_Half; break;
	case HLSLToken_Half1x2:
		type = HLSLBaseType_Half1x2; break;
	case HLSLToken_Half1x3:
		type = HLSLBaseType_Half1x3; break;
	case HLSLToken_Half1x4:
		type = HLSLBaseType_Half1x4; break;

    case HLSLToken_Half2:      
        type = HLSLBaseType_Half2; break;
	case HLSLToken_Half2x2:
		type = HLSLBaseType_Half2x2; break;
	case HLSLToken_Half2x3:
		type = HLSLBaseType_Half2x3; break;
	case HLSLToken_Half2x4:
		type = HLSLBaseType_Half2x4; break;

    case HLSLToken_Half3:
        type = HLSLBaseType_Half3; break;
	case HLSLToken_Half3x2:
		type = HLSLBaseType_Half3x2; break;
	case HLSLToken_Half3x3:
		type = HLSLBaseType_Half3x3; break;
	case HLSLToken_Half3x4:
		type = HLSLBaseType_Half3x4; break;
		
    case HLSLToken_Half4:
        type = HLSLBaseType_Half4; break;
	case HLSLToken_Half4x2:
		type = HLSLBaseType_Half4x2; break;
	case HLSLToken_Half4x3:
		type = HLSLBaseType_Half4x3; break;
	case HLSLToken_Half4x4:
		type = HLSLBaseType_Half4x4; break;



	case HLSLToken_Min16Float:
		type = HLSLBaseType_Min16Float; break;
	case HLSLToken_Min16Float1x2:
		type = HLSLBaseType_Min16Float1x2; break;
	case HLSLToken_Min16Float1x3:
		type = HLSLBaseType_Min16Float1x3; break;
	case HLSLToken_Min16Float1x4:
		type = HLSLBaseType_Min16Float1x4; break;

	case HLSLToken_Min16Float2:
		type = HLSLBaseType_Min16Float2; break;
	case HLSLToken_Min16Float2x2:
		type = HLSLBaseType_Min16Float2x2; break;
	case HLSLToken_Min16Float2x3:
		type = HLSLBaseType_Min16Float2x3; break;
	case HLSLToken_Min16Float2x4:
		type = HLSLBaseType_Min16Float2x4; break;

	case HLSLToken_Min16Float3:
		type = HLSLBaseType_Min16Float3; break;
	case HLSLToken_Min16Float3x2:
		type = HLSLBaseType_Min16Float3x2; break;
	case HLSLToken_Min16Float3x3:
		type = HLSLBaseType_Min16Float3x3; break;
	case HLSLToken_Min16Float3x4:
		type = HLSLBaseType_Min16Float3x4; break;

	case HLSLToken_Min16Float4:
		type = HLSLBaseType_Min16Float4; break;
	case HLSLToken_Min16Float4x2:
		type = HLSLBaseType_Min16Float4x2; break;
	case HLSLToken_Min16Float4x3:
		type = HLSLBaseType_Min16Float4x3; break;
	case HLSLToken_Min16Float4x4:
		type = HLSLBaseType_Min16Float4x4; break;


	case HLSLToken_Min10Float:
		type = HLSLBaseType_Min10Float; break;
	case HLSLToken_Min10Float1x2:
		type = HLSLBaseType_Min10Float1x2; break;
	case HLSLToken_Min10Float1x3:
		type = HLSLBaseType_Min10Float1x3; break;
	case HLSLToken_Min10Float1x4:
		type = HLSLBaseType_Min10Float1x4; break;

	case HLSLToken_Min10Float2:
		type = HLSLBaseType_Min10Float2; break;
	case HLSLToken_Min10Float2x2:
		type = HLSLBaseType_Min10Float2x2; break;
	case HLSLToken_Min10Float2x3:
		type = HLSLBaseType_Min10Float2x3; break;
	case HLSLToken_Min10Float2x4:
		type = HLSLBaseType_Min10Float2x4; break;

	case HLSLToken_Min10Float3:
		type = HLSLBaseType_Min10Float3; break;
	case HLSLToken_Min10Float3x2:
		type = HLSLBaseType_Min10Float3x2; break;
	case HLSLToken_Min10Float3x3:
		type = HLSLBaseType_Min10Float3x3; break;
	case HLSLToken_Min10Float3x4:
		type = HLSLBaseType_Min10Float3x4; break;

	case HLSLToken_Min10Float4:
		type = HLSLBaseType_Min10Float4; break;
	case HLSLToken_Min10Float4x2:
		type = HLSLBaseType_Min10Float4x2; break;
	case HLSLToken_Min10Float4x3:
		type = HLSLBaseType_Min10Float4x3; break;
	case HLSLToken_Min10Float4x4:
		type = HLSLBaseType_Min10Float4x4; break;


    case HLSLToken_Int:
        type = HLSLBaseType_Int; break;
	case HLSLToken_Int1x2:
		type = HLSLBaseType_Int1x2; break;
	case HLSLToken_Int1x3:
		type = HLSLBaseType_Int1x3; break;
	case HLSLToken_Int1x4:
		type = HLSLBaseType_Int1x4; break;

    case HLSLToken_Int2:
        type = HLSLBaseType_Int2; break;
	case HLSLToken_Int2x2:
		type = HLSLBaseType_Int2x2; break;
	case HLSLToken_Int2x3:
		type = HLSLBaseType_Int2x3; break;
	case HLSLToken_Int2x4:
		type = HLSLBaseType_Int2x4; break;
		
    case HLSLToken_Int3:
        type = HLSLBaseType_Int3; break;
	case HLSLToken_Int3x2:
		type = HLSLBaseType_Int3x2; break;
	case HLSLToken_Int3x3:
		type = HLSLBaseType_Int3x3; break;
	case HLSLToken_Int3x4:
		type = HLSLBaseType_Int3x4; break;
		
    case HLSLToken_Int4:
        type = HLSLBaseType_Int4; break;
	case HLSLToken_Int4x2:
		type = HLSLBaseType_Int4x2; break;
	case HLSLToken_Int4x3:
		type = HLSLBaseType_Int4x3; break;
	case HLSLToken_Int4x4:
		type = HLSLBaseType_Int4x4; break;

    case HLSLToken_Uint:
        type = HLSLBaseType_Uint; break;
	case HLSLToken_Uint1x2:
		type = HLSLBaseType_Uint1x2; break;
	case HLSLToken_Uint1x3:
		type = HLSLBaseType_Uint1x3; break;
	case HLSLToken_Uint1x4:
		type = HLSLBaseType_Uint1x4; break;

    case HLSLToken_Uint2:
        type = HLSLBaseType_Uint2; break;
	case HLSLToken_Uint2x2:
		type = HLSLBaseType_Uint2x2; break;
	case HLSLToken_Uint2x3:
		type = HLSLBaseType_Uint2x3; break;
	case HLSLToken_Uint2x4:
		type = HLSLBaseType_Uint2x4; break;

    case HLSLToken_Uint3:
        type = HLSLBaseType_Uint3; break;
	case HLSLToken_Uint3x2:
		type = HLSLBaseType_Uint3x2; break;
	case HLSLToken_Uint3x3:
		type = HLSLBaseType_Uint3x3; break;
	case HLSLToken_Uint3x4:
		type = HLSLBaseType_Uint3x4; break;

    case HLSLToken_Uint4:
        type = HLSLBaseType_Uint4; break;
	case HLSLToken_Uint4x2:
		type = HLSLBaseType_Uint4x2; break;
	case HLSLToken_Uint4x3:
		type = HLSLBaseType_Uint4x3; break;
	case HLSLToken_Uint4x4:
		type = HLSLBaseType_Uint4x4; break;
		
	case HLSLToken_InputPatch:
		type = HLSLBaseType_InputPatch;
		m_tokenizer.Next();
		return true;
		break;

	case HLSLToken_OutputPatch:
		type = HLSLBaseType_OutputPatch;
		m_tokenizer.Next();
		return true;
		break;

	case HLSLToken_PointStream:
		type = HLSLBaseType_PointStream;
		m_tokenizer.Next();
		return true;
		break;

	case HLSLToken_LineStream:
		type = HLSLBaseType_LineStream;
		m_tokenizer.Next();
		return true;
		break;

	case HLSLToken_TriangleStream:
		type = HLSLBaseType_TriangleStream; 
		m_tokenizer.Next();
		return true;
		break;


	case HLSLToken_Point:
		type = HLSLBaseType_Point; break;
	case HLSLToken_Line:
		type = HLSLBaseType_Line; break;
	case HLSLToken_Triangle:
		type = HLSLBaseType_Triangle; break;
	case HLSLToken_Lineadj:
		type = HLSLBaseType_Lineadj; break;
	case HLSLToken_Triangleadj:
		type = HLSLBaseType_Triangleadj; break;

    case HLSLToken_Texture:
        type = HLSLBaseType_Texture;
        break;
    case HLSLToken_Sampler:
        type = HLSLBaseType_Sampler2D;  // @@ IC: For now we assume that generic samplers are always sampler2D
        break;
    case HLSLToken_Sampler2D:
        type = HLSLBaseType_Sampler2D;
        break;
    case HLSLToken_Sampler3D:
        type = HLSLBaseType_Sampler3D;
        break;
    case HLSLToken_SamplerCube:
        type = HLSLBaseType_SamplerCube;
        break;
    case HLSLToken_Sampler2DShadow:
        type = HLSLBaseType_Sampler2DShadow;
        break;
    case HLSLToken_Sampler2DMS:
        type = HLSLBaseType_Sampler2DMS;
        break;
    case HLSLToken_Sampler2DArray:
        type = HLSLBaseType_Sampler2DArray;
        break;
	case HLSLToken_AddressU:
		type = HLSLBaseType_AddressU;
		break;
	case HLSLToken_AddressV:
		type = HLSLBaseType_AddressV;
		break;
	case HLSLToken_AddressW:
		type = HLSLBaseType_AddressW;
		break;
	case HLSLToken_BorderColor:
		type = HLSLBaseType_BorderColor;
		break;
	case HLSLToken_Filter:
		type = HLSLBaseType_Filter;
		break;
	case HLSLToken_MaxAnisotropy:
		type = HLSLBaseType_MaxAnisotropy;
		break;
	case HLSLToken_MaxLOD:
		type = HLSLBaseType_MaxLOD;
		break;
	case HLSLToken_MinLOD:
		type = HLSLBaseType_MinLOD;
		break;
	case HLSLToken_MipLODBias:
		type = HLSLBaseType_MipLODBias;
		break;
	case HLSLToken_ComparisonFunc:
		type = HLSLBaseType_ComparisonFunc;
		break;
	case HLSLToken_SamplerState:
		type = HLSLBaseType_SamplerState;
		break;
    }
    if (type != HLSLBaseType_Void)
    {
        m_tokenizer.Next();
        return true;
    }

    if (allowVoid && Accept(HLSLToken_Void))
    {
        type = HLSLBaseType_Void;
        return true;
    }

    if (token == HLSLToken_Identifier)
    {
		//Define preprocessor		
		const char* preprocessorIdentifier = m_tree->AddDefineString(m_tokenizer.GetIdentifier());
		if (FindPreprocessorDefinedType(preprocessorIdentifier) != NULL)
		{
			m_tokenizer.Next();
			type = HLSLBaseType_UserDefined;
			typeName = preprocessorIdentifier;
			return true;
		}
		
		//Reserved identifier (ex : for geometry shader)

        const char* identifier = m_tree->AddString( m_tokenizer.GetIdentifier() );
        if (FindUserDefinedType(identifier) != NULL)
        {
            m_tokenizer.Next();
            type        = HLSLBaseType_UserDefined;
            typeName    = identifier;
            return true;
        }
    }
    return false;
}

bool HLSLParser::ExpectType(bool allowVoid, HLSLBaseType& type, const char*& typeName, int* typeFlags)
{
    if (!AcceptType(allowVoid, type, typeName, typeFlags))
    {
        m_tokenizer.Error("Expected type");
        return false;
    }
    return true;
}

bool HLSLParser::AcceptDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name)
{
    if (!AcceptType(/*allowVoid=*/false, type.baseType, type.typeName, &type.flags))
    {
        return false;
    }

	if (Accept('<'))
	{		
		if (!ExpectIdentifier(type.structuredTypeName))
			return false;

		
		if (Accept(','))
		{
			//get max point
			if (!AcceptInt(type.maxPoints))
				false;
			else
			{
				if (type.baseType == HLSLBaseType_InputPatch)
				{
					type.typeName = type.structuredTypeName;

					strcpy(type.InputPatchName, "InputPatch<");
					strcat(type.InputPatchName, type.typeName);
					strcat(type.InputPatchName, ", ");

					char intBuffer[128];
					_itoa_s(type.maxPoints, intBuffer, 10);
					strcat(type.InputPatchName, intBuffer);

					strcat(type.InputPatchName, ">");
				}
				else if (type.baseType == HLSLBaseType_OutputPatch)
				{
					type.typeName = type.structuredTypeName;

					strcpy(type.OutputPatchName, "OutputPatch<");
					strcat(type.OutputPatchName, type.typeName);
					strcat(type.OutputPatchName, ", ");

					char intBuffer[128];
					_itoa_s(type.maxPoints, intBuffer, 10);
					strcat(type.OutputPatchName, intBuffer);

					strcat(type.OutputPatchName, ">");
				}

				type.array = true;
			}
		}

		if (!Expect('>'))
			return false;
	}

    if (!ExpectIdentifier(name))
    {
        // TODO: false means we didn't accept a declaration and we had an error!
        return false;
    }

	

    // Handle array syntax.
    if (Accept('['))
    {
        type.array = true;
        // Optionally allow no size to the specified for the array.
        if (Accept(']') && allowUnsizedArray)
        {
            return true;
        }
        if (!ParseExpression(type.arraySize) || !Expect(']'))
        {
            return false;
        }
    }
    return true;
}

bool HLSLParser::ExpectDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name)
{
    if (!AcceptDeclaration(allowUnsizedArray, type, name))
    {
        m_tokenizer.Error("Expected declaration");
        return false;
    }
    return true;
}

const HLSLpreprocessor* HLSLParser::FindPreprocessorDefinedType(const char* name) const
{
	// Pointer comparison is sufficient for strings since they exist in the
	// string pool.
	for (int i = 0; i < m_preProcessors.GetSize(); ++i)
	{
		if (String_Equal(m_preProcessors[i]->name, name))
		{
			return m_preProcessors[i];
		}
	}
	return NULL;
}

const HLSLSamplerState* HLSLParser::FindSamplerStateDefinedType(const char* name) const
{
	// Pointer comparison is sufficient for strings since they exist in the
	// string pool.
	for (int i = 0; i < m_samplerStates.GetSize(); ++i)
	{
		if (String_Equal(m_samplerStates[i]->name, name))
		{
			return m_samplerStates[i];
		}
	}
	return NULL;
}

const HLSLTextureState* HLSLParser::FindTextureStateDefinedType(const char* name) const
{	
	for (int i = 0; i < m_textureStates.GetSize(); ++i)
	{
		
		if (String_Equal(m_textureStates[i]->name, name))
		{
			return m_textureStates[i];
		}
		
	}
	return NULL;
}

const HLSLTextureState* HLSLParser::FindTextureStateDefinedTypeWithAddress(const char* name) const
{
	//name is unTokenized
	/*
	for (int i = 0; i < m_textureStates.GetSize(); ++i)
	{
		const char* debug = m_textureStates[i]->startAddress;

		if (m_textureStates[i]->startAddress == name)
		{
			return m_textureStates[i];
		}
	}
	return NULL;
	*/

	return NULL;
}

const HLSLRWTextureState* HLSLParser::FindRWTextureStateDefinedType(const char* name) const
{
	for (int i = 0; i < m_rwtextureStates.GetSize(); ++i)
	{
		if (String_Equal(m_rwtextureStates[i]->name, name))
		{
			return m_rwtextureStates[i];
		}
	}
	return NULL;
}

const HLSLStruct* HLSLParser::FindUserDefinedType(const char* name) const
{
    // Pointer comparison is sufficient for strings since they exist in the
    // string pool.
    for (int i = 0; i < m_userTypes.GetSize(); ++i)
    {
        if (m_userTypes[i]->name == name)
        {
            return m_userTypes[i];
        }
    }
    return NULL;
}

bool HLSLParser::CheckForUnexpectedEndOfStream(int endToken)
{
    if (Accept(HLSLToken_EndOfStream))
    {
        char what[HLSLTokenizer::s_maxIdentifier];
        m_tokenizer.GetTokenName(endToken, what);
        m_tokenizer.Error("Unexpected end of file while looking for '%s'", what);
        return true;
    }
    return false;
}

int HLSLParser::GetLineNumber() const
{
    return m_tokenizer.GetLineNumber();
}

const char* HLSLParser::GetFileName()
{
    return m_tree->AddString( m_tokenizer.GetFileName() );
}

void HLSLParser::BeginScope()
{
    // Use NULL as a sentinel that indices a new scope level.
    Variable& variable = m_variables.PushBackNew();
    variable.name = NULL;
}

void HLSLParser::EndScope()
{
    int numVariables = m_variables.GetSize() - 1;
    while (m_variables[numVariables].name != NULL)
    {
        --numVariables;
        ASSERT(numVariables >= 0);
    }
    m_variables.Resize(numVariables);
}

const HLSLType* HLSLParser::FindVariable(const char* name, bool& global) const
{
    for (int i = m_variables.GetSize() - 1; i >= 0; --i)
    {
        if (m_variables[i].name == name)
        {
            global = (i < m_numGlobals);
            return &m_variables[i].type;
        }
    }
    return NULL;
}

HLSLFunction* HLSLParser::FindFunction(const char* name)
{
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        if (String_Equal(m_functions[i]->name, name))
        {
            return m_functions[i];
        }
    }
    return NULL;
}

static bool AreTypesEqual(HLSLTree* tree, const HLSLType& lhs, const HLSLType& rhs)
{
    return GetTypeCastRank(tree, lhs, rhs) == 0;
}

static bool AreArgumentListsEqual(HLSLTree* tree, HLSLArgument* lhs, HLSLArgument* rhs)
{
    while (lhs && rhs)
    {
        if (!AreTypesEqual(tree, lhs->type, rhs->type))
            return false;

        if (lhs->modifier != rhs->modifier)
            return false;

        if (lhs->semantic != rhs->semantic || lhs->sv_semantic != rhs->sv_semantic)
            return false;

        lhs = lhs->nextArgument;
        rhs = rhs->nextArgument;
    }

    return lhs == NULL && rhs == NULL;
}

const HLSLFunction* HLSLParser::FindFunction(const HLSLFunction* fun) const
{
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        if ( String_Equal(m_functions[i]->name, fun->name) &&
            AreTypesEqual(m_tree, m_functions[i]->returnType, fun->returnType) &&
            AreArgumentListsEqual(m_tree, m_functions[i]->argument, fun->argument))
        {
            return m_functions[i];
        }
    }
    return NULL;
}

void HLSLParser::DeclareVariable(const char* name, const HLSLType& type)
{
    if (m_variables.GetSize() == m_numGlobals)
    {
        ++m_numGlobals;
    }
    Variable& variable = m_variables.PushBackNew();
    variable.name = name;
    variable.type = type;
}

bool HLSLParser::GetIsFunction(const char* name) const
{
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        // == is ok here because we're passed the strings through the string pool.
        if (m_functions[i]->name == name)
        {
            return true;
        }
    }
    for (int i = 0; i < _numIntrinsics; ++i)
    {
        // Intrinsic names are not in the string pool (since they are compile time
        // constants, so we need full string compare).
        if (String_Equal(name, _intrinsic[i].function.name))
        {
            return true;
        }
    }

    return false;
}

const HLSLFunction* HLSLParser::MatchFunctionCall(const HLSLFunctionCall* functionCall, const char* name)
{
    const HLSLFunction* matchedFunction     = NULL;

    int  numArguments           = functionCall->numArguments;
    int  numMatchedOverloads    = 0;
    bool nameMatches            = false;

    // Get the user defined functions with the specified name.
    for (int i = 0; i < m_functions.GetSize(); ++i)
    {
        const HLSLFunction* function = m_functions[i];
        if (function->name == name)
        {
            nameMatches = true;
            
            CompareFunctionsResult result = CompareFunctions( m_tree, functionCall, function, matchedFunction );
            if (result == Function1Better)
            {
                matchedFunction = function;
                numMatchedOverloads = 1;
            }
            else if (result == FunctionsEqual)
            {
                ++numMatchedOverloads;
            }
        }
    }

    // Get the intrinsic functions with the specified name.
    for (int i = 0; i < _numIntrinsics; ++i)
    {
        const HLSLFunction* function = &_intrinsic[i].function;
        if (String_Equal(function->name, name))
        {
			if (String_Equal("InterlockedMax", name))
			{
				int debug = 35;
			}

            nameMatches = true;

            CompareFunctionsResult result = CompareFunctions( m_tree, functionCall, function, matchedFunction );
            if (result == Function1Better)
            {
                matchedFunction = function;
                numMatchedOverloads = 1;
            }
            else if (result == FunctionsEqual)
            {
                ++numMatchedOverloads;
            }
        }
    }

    if (matchedFunction != NULL && numMatchedOverloads > 1)
    {
        // Multiple overloads match.
        //m_tokenizer.Error("'%s' %d overloads have similar conversions", name, numMatchedOverloads);
        //return NULL;

		// Skip Overloads
    }
    else if (matchedFunction == NULL)
    {
        if (nameMatches)
        {
            m_tokenizer.Error("'%s' no overloaded function matched all of the arguments", name);
        }
        else
        {
            m_tokenizer.Error("Undeclared identifier '%s'", name);
        }
    }

    return matchedFunction;
}

bool HLSLParser::GetMemberType(const HLSLType& objectType, HLSLMemberAccess * memberAccess)
{
    const char* fieldName = memberAccess->field;

    if (objectType.baseType == HLSLBaseType_UserDefined)
    {
        const HLSLStruct* structure = FindUserDefinedType( objectType.typeName );
        ASSERT(structure != NULL);

        const HLSLStructField* field = structure->field;
        while (field != NULL)
        {
            if (field->name == fieldName)
            {
                memberAccess->expressionType = field->type;
                return true;
            }
            field = field->nextField;
        }

        return false;
    }
	//hull
	else if (objectType.baseType == HLSLBaseType_InputPatch || objectType.baseType == HLSLBaseType_OutputPatch)
	{
		const HLSLStruct* structure = FindUserDefinedType(objectType.typeName);
		ASSERT(structure != NULL);

		const HLSLStructField* field = structure->field;
		while (field != NULL)
		{
			if (field->name == fieldName)
			{
				memberAccess->expressionType = field->type;
				return true;
			}
			field = field->nextField;
		}

		return false;
	}
	//geometry
	else if ((objectType.baseType == HLSLBaseType_PointStream) || (objectType.baseType == HLSLBaseType_LineStream) || (objectType.baseType == HLSLBaseType_TriangleStream))
	{
		if (String_Equal(fieldName, "Append"))
		{
			memberAccess->function = true;

			bool needsEndParen;
			if (!ParseTerminalExpression(memberAccess->functionExpression, needsEndParen))
				return false;

			return true;
		}
		else if (String_Equal(fieldName, "RestartStrip"))
		{
			memberAccess->function = true;

			bool needsEndParen;
			if (!ParseTerminalExpression(memberAccess->functionExpression, needsEndParen))
				return false;

			return true;
		}
	}

    if (_baseTypeDescriptions[objectType.baseType].numericType == NumericType_NaN)
    {
        // Currently we don't have an non-numeric types that allow member access.
        return false;
    }

    int swizzleLength = 0;

    if (_baseTypeDescriptions[objectType.baseType].numDimensions <= 1)
    {
        // Check for a swizzle on the scalar/vector types.
        for (int i = 0; fieldName[i] != 0; ++i)
        {
            if (fieldName[i] != 'x' && fieldName[i] != 'y' && fieldName[i] != 'z' && fieldName[i] != 'w' &&
                fieldName[i] != 'r' && fieldName[i] != 'g' && fieldName[i] != 'b' && fieldName[i] != 'a')
            {
                m_tokenizer.Error("Invalid swizzle '%s'", fieldName);
                return false;
            }
            ++swizzleLength;
        }
        ASSERT(swizzleLength > 0);
    }
    else
    {

        // Check for a matrix element access (e.g. _m00 or _11)

        const char* n = fieldName;
        while (n[0] == '_')
        {
            ++n;
            int base = 1;
            if (n[0] == 'm')
            {
                base = 0;
                ++n;
            }
            if (!isdigit(n[0]) || !isdigit(n[1]))
            {
                return false;
            }

            int r = (n[0] - '0') - base;
            int c = (n[1] - '0') - base;
            if (r >= _baseTypeDescriptions[objectType.baseType].height ||
                c >= _baseTypeDescriptions[objectType.baseType].numComponents)
            {
                return false;
            }
            ++swizzleLength;
            n += 2;

        }

        if (n[0] != 0)
        {
            return false;
        }

    }

    if (swizzleLength > 4)
    {
        m_tokenizer.Error("Invalid swizzle '%s'", fieldName);
        return false;
    }

    static const HLSLBaseType floatType[] = { HLSLBaseType_Float, HLSLBaseType_Float2, HLSLBaseType_Float3, HLSLBaseType_Float4 };
    static const HLSLBaseType halfType[]  = { HLSLBaseType_Half,  HLSLBaseType_Half2,  HLSLBaseType_Half3,  HLSLBaseType_Half4  };
    static const HLSLBaseType intType[]   = { HLSLBaseType_Int,   HLSLBaseType_Int2,   HLSLBaseType_Int3,   HLSLBaseType_Int4   };
    static const HLSLBaseType uintType[]  = { HLSLBaseType_Uint,  HLSLBaseType_Uint2,  HLSLBaseType_Uint3,  HLSLBaseType_Uint4  };
    static const HLSLBaseType boolType[]  = { HLSLBaseType_Bool,  HLSLBaseType_Bool2,  HLSLBaseType_Bool3,  HLSLBaseType_Bool4  };
    
    switch (_baseTypeDescriptions[objectType.baseType].numericType)
    {
    case NumericType_Float:
        memberAccess->expressionType.baseType = floatType[swizzleLength - 1];
        break;
    case NumericType_Half:
        memberAccess->expressionType.baseType = halfType[swizzleLength - 1];
        break;
    case NumericType_Int:
        memberAccess->expressionType.baseType = intType[swizzleLength - 1];
        break;
    case NumericType_Uint:
        memberAccess->expressionType.baseType = uintType[swizzleLength - 1];
            break;
    case NumericType_Bool:
        memberAccess->expressionType.baseType = boolType[swizzleLength - 1];
            break;
    default:
        ASSERT(0);
    }

    memberAccess->swizzle = true;
    
    return true;
}

/*
void HLSLParser::FindClosestTextureIdentifier(const HLSLTextureState* pTextureState, char* functionCaller, HLSLFunctionCall* functionCall, int i, const char* pIdentifierName)
{
	int startIndex = 0;
	//loop until we find the identifier
	for (int j = i; j >= 0; j--)
	{
		const char* buffer = m_tokenizer.GetIndentifierHistoryAddress(j);
		bool bCorrect = true;

		int len = strnlen_s(pTextureState->name, 256);

		for (int k = 0; k < len; k++)
		{
			if (buffer[k] != pTextureState->name[k])
			{
				bCorrect = false; break;
			}
		}

		if (bCorrect)
		{
			startIndex = j;	break;
		}
	}

	int lastIndex;
	//loop until we find the identifier
	for (int j = startIndex; ; j++)
	{
		const char* buffer = m_tokenizer.GetIndentifierHistoryAddress(j);
		bool bCorrect = true;
		int len = strnlen_s(pIdentifierName, 256);

		for (int k = 0; k < len; k++)
		{
			if (buffer[k] != pIdentifierName[k])
			{
				bCorrect = false; break;
			}
		}

		if (bCorrect)
		{
			lastIndex = j; break;
		}
	}

	const char* start = m_tokenizer.GetIndentifierHistoryAddress(startIndex);
	const char* last = m_tokenizer.GetIndentifierHistoryAddress(lastIndex);



	size_t length = last - start - 1;
	memcpy(functionCaller, start, length);
	functionCaller[length] = 0;

	strcpy(functionCall->functionCaller, functionCaller);
}

void HLSLParser::FindClosestTextureIdentifier(const HLSLRWTextureState* pTextureState, char* functionCaller, HLSLFunctionCall* functionCall, int i, const char* pIdentifierName)
{
	int startIndex = 0;
	//loop until we find the identifier
	for (int j = i; j >= 0; j--)
	{
		const char* buffer = m_tokenizer.GetIndentifierHistoryAddress(j);
		bool bCorrect = true;

		int len = strnlen_s(pTextureState->name, 256);

		for (int k = 0; k < len; k++)
		{
			if (buffer[k] != pTextureState->name[k])
			{
				bCorrect = false; break;
			}
		}

		if (bCorrect)
		{
			startIndex = j;	break;
		}
	}

	int lastIndex;
	//loop until we find the identifier
	for (int j = startIndex; ; j++)
	{
		const char* buffer = m_tokenizer.GetIndentifierHistoryAddress(j);
		bool bCorrect = true;
		int len = strnlen_s(pIdentifierName, 256);

		for (int k = 0; k < len; k++)
		{
			if (buffer[k] != pIdentifierName[k])
			{
				bCorrect = false; break;
			}
		}

		if (bCorrect)
		{
			lastIndex = j; break;
		}
	}

	const char* start = m_tokenizer.GetIndentifierHistoryAddress(startIndex);
	const char* last = m_tokenizer.GetIndentifierHistoryAddress(lastIndex);



	size_t length = last - start;
	memcpy(functionCaller, start, length);
	functionCaller[length] = 0;

	strcpy(functionCall->functionCaller, functionCaller);
}
*/

