//=============================================================================
//
// Render/HLSLParser.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

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

// In the original code, there are several areas where it finds the type of a parent expression
// by looking through the recent identifiers list. Unfortunately, this is not robust and the
// only real solution is to keep a proper stack of the nodes/identifiers.
// 
// The catch is that the original code has many divergent paths, and keeping track of the
// stack state is quite messy because of the mix of return/continue/break statements.
//
// So the solution is the HLSLStackTracker. When entering a function, create one of these.
// Add any identifiers that you need to the stack, and when the function exits the destructor
// reverts the stack to its original size.
class HLSLScopedStackTracker
{
public:
	HLSLScopedStackTracker(HLSLParser * parser)
	{
		ASSERT_PARSER(parser != NULL);
		m_parser = parser;

		m_stackSize = m_parser->GetParentStackSize();
	}

	~HLSLScopedStackTracker()
	{
		ASSERT_PARSER(m_parser != NULL);

		m_parser->RevertParentStackToSize(m_stackSize);
		m_parser = NULL;
	}

	HLSLParser * m_parser;
	int m_stackSize;
};



void HLSLParser::Intrinsic::AllocArgs(int num)
{
	fullFunction.argumentVec.resize(num);
	for (int i = 0; i < num; i++)
	{
		fullFunction.argumentVec[i] = &argument[i];
	}
}

HLSLParser::Intrinsic::~Intrinsic()
{
}

CachedString HLSLParser::Intrinsic::MakeCached(StringLibrary & stringLibrary, const char * name)
{
	CachedString ret;
	ret.m_string = name;
	stringLibrary.InsertDirect(eastl::string(name));
	return ret;
}

HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(0);
}

HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(1);
	argument[0].argType.baseType = arg1;
	argument[0].argType.flags = HLSLTypeFlag_Const;
}
HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(2);
	argument[0].argType.baseType = arg1;
	argument[0].argType.flags = HLSLTypeFlag_Const;
	argument[1].argType.baseType = arg2;
	argument[1].argType.flags = HLSLTypeFlag_Const;
}
HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(3);
	argument[0].argType.baseType = arg1;
	argument[0].argType.flags = HLSLTypeFlag_Const;
	argument[1].argType.baseType = arg2;
	argument[1].argType.flags = HLSLTypeFlag_Const;
	argument[2].argType.baseType = arg3;
	argument[2].argType.flags = HLSLTypeFlag_Const;
}
HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(4);
	argument[0].argType.baseType = arg1;
	argument[0].argType.flags = HLSLTypeFlag_Const;
	argument[1].argType.baseType = arg2;
	argument[1].argType.flags = HLSLTypeFlag_Const;
	argument[2].argType.baseType = arg3;
	argument[2].argType.flags = HLSLTypeFlag_Const;
	argument[3].argType.baseType = arg4;
	argument[3].argType.flags = HLSLTypeFlag_Const;
}

HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4, HLSLBaseType arg5)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(5);
	argument[0].argType.baseType = arg1;
	argument[0].argType.flags = HLSLTypeFlag_Const;
	argument[1].argType.baseType = arg2;
	argument[1].argType.flags = HLSLTypeFlag_Const;
	argument[2].argType.baseType = arg3;
	argument[2].argType.flags = HLSLTypeFlag_Const;
	argument[3].argType.baseType = arg4;
	argument[3].argType.flags = HLSLTypeFlag_Const;
	argument[4].argType.baseType = arg5;
	argument[4].argType.flags = HLSLTypeFlag_Const;
}
/*
static HLSLParser::Intrinsic * MakeIntrinsicInOut(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2)
{
	HLSLParser::Intrinsic * pIntrinsic = new HLSLParser::Intrinsic(stringLibrary,name,returnType,arg1,arg2);
	for (int i = 0; i < pIntrinsic->fullFunction.argumentVec.size(); i++)
	{
		pIntrinsic->fullFunction.argumentVec[i]->argType.flags = (HLSLTypeFlag_Input | HLSLTypeFlag_Output);
	}
}
*/



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



// note: going from float to int is more expensive than from int to float, so that things like:
// max(float,int) -> max(float,float) instead of mac(int,int)
static const int _numberTypeRank[NumericType_Count][NumericType_Count] = 
{
	//F  H  m16  m10  B  I  U    
	{ 0, 6,   7,   8, 6, 6, 6 },  // NumericType_Float
	{ 1, 0,   5,   6, 4, 4, 4 },  // NumericType_Half
	{ 6, 7,   0,   7, 6, 6, 6 },  // NumericType_Min16Float
	{ 6, 7,   7,   0, 6, 6, 6 },  // NumericType_Min10Float
	{ 5, 5,   6,   7, 0, 5, 5 },  // NumericType_Bool
	{ 5, 5,   6,   7, 4, 0, 3 },  // NumericType_Int
	{ 5, 5,   6,   7, 4, 2, 0 }   // NumericType_Uint
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

#define INTRINSIC_VOID_FUNCTION_CACHED(name) \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Void ));


#define INTRINSIC_FLOAT1_FUNCTION_CACHED(name) \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float,   HLSLBaseType_Float  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float2,  HLSLBaseType_Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float3,  HLSLBaseType_Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float4,  HLSLBaseType_Float4 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half,    HLSLBaseType_Half   ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half2,   HLSLBaseType_Half2  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half3,   HLSLBaseType_Half3  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half4,   HLSLBaseType_Half4  ));	\
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float,   HLSLBaseType_Min16Float  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float4,  HLSLBaseType_Min16Float4 ));	  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float,   HLSLBaseType_Min10Float  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float4,  HLSLBaseType_Min10Float4 ));

#define INTRINSIC_FLOAT2_FUNCTION_CACHED(name) \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float,   HLSLBaseType_Float,   HLSLBaseType_Float  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float4 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half,    HLSLBaseType_Half,    HLSLBaseType_Half   ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half2,   HLSLBaseType_Half2,   HLSLBaseType_Half2  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half3,   HLSLBaseType_Half3,   HLSLBaseType_Half3  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half4,   HLSLBaseType_Half4,   HLSLBaseType_Half4  ));	  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float,   HLSLBaseType_Min16Float,  HLSLBaseType_Min16Float ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float4,  HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4 ));	  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float,   HLSLBaseType_Min10Float,  HLSLBaseType_Min10Float ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float4,  HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4 ));

#define INTRINSIC_FLOAT3_FUNCTION_CACHED(name) \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float,   HLSLBaseType_Float,   HLSLBaseType_Float,  HLSLBaseType_Float ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float2,  HLSLBaseType_Float2 ));  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3,  HLSLBaseType_Float3 ));  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float4,  HLSLBaseType_Float4 ));  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half,    HLSLBaseType_Half,    HLSLBaseType_Half,   HLSLBaseType_Half ));    \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half2,   HLSLBaseType_Half2,   HLSLBaseType_Half2,  HLSLBaseType_Half2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half3,   HLSLBaseType_Half3,   HLSLBaseType_Half3,  HLSLBaseType_Half3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Half4,   HLSLBaseType_Half4,   HLSLBaseType_Half4,  HLSLBaseType_Half4 ));	  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float,   HLSLBaseType_Min16Float,  HLSLBaseType_Min16Float, HLSLBaseType_Min16Float ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float4,  HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4 ));	  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float,   HLSLBaseType_Min10Float,  HLSLBaseType_Min10Float, HLSLBaseType_Min10Float ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float4,  HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4 ));

#define INTRINSIC_INT2_FUNCTION_CACHED(name) \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Int,   HLSLBaseType_Int,   HLSLBaseType_Int  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Int2,  HLSLBaseType_Int2,  HLSLBaseType_Int2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Int3,  HLSLBaseType_Int3,  HLSLBaseType_Int3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Int4,  HLSLBaseType_Int4,  HLSLBaseType_Int4 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Uint,   HLSLBaseType_Uint,   HLSLBaseType_Uint  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Uint2,  HLSLBaseType_Uint2,  HLSLBaseType_Uint2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Uint3,  HLSLBaseType_Uint3,  HLSLBaseType_Uint3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Uint4,  HLSLBaseType_Uint4,  HLSLBaseType_Uint4 ));   \


#define INTRINSIC_FLOAT1_BOOL_FUNCTION_CACHED(name) \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool,   HLSLBaseType_Float  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool2,  HLSLBaseType_Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool3,  HLSLBaseType_Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool4,  HLSLBaseType_Float4 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool,    HLSLBaseType_Half   ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool2,   HLSLBaseType_Half2  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool3,   HLSLBaseType_Half3  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Bool4,   HLSLBaseType_Half4  ));	\
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float,   HLSLBaseType_Min16Float  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min16Float4,  HLSLBaseType_Min16Float4 ));	  \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float,   HLSLBaseType_Min10Float  ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float2 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float3 ));   \
		m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, name, HLSLBaseType_Min10Float4,  HLSLBaseType_Min10Float4 ));



void HLSLParser::IntrinsicHelper::BuildIntrinsics()
{

	INTRINSIC_FLOAT1_FUNCTION_CACHED("abs")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("acos")

#if 1
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "any", HLSLBaseType_Bool, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"any", HLSLBaseType_Bool, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "all", HLSLBaseType_Bool, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Bool1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Bool1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Bool1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Int1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Int1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Int1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Uint1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Uint1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Uint1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"all", HLSLBaseType_Bool, HLSLBaseType_Uint4));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sizeof", HLSLBaseType_Uint, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sizeof", HLSLBaseType_Uint, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sizeof", HLSLBaseType_Uint, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sizeof", HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sizeof", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sizeof", HLSLBaseType_Uint, HLSLBaseType_Unknown));
	*/

	INTRINSIC_FLOAT1_FUNCTION_CACHED("asin")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("atan")
	INTRINSIC_FLOAT2_FUNCTION_CACHED("atan2")
	INTRINSIC_FLOAT3_FUNCTION_CACHED("clamp")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("cos")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("tan")

	INTRINSIC_FLOAT3_FUNCTION_CACHED("lerp")
	INTRINSIC_FLOAT3_FUNCTION_CACHED("smoothstep")

	INTRINSIC_FLOAT1_FUNCTION_CACHED("round")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("floor")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("ceil")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("frac")

	INTRINSIC_FLOAT2_FUNCTION_CACHED("fmod")

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"clip", HLSLBaseType_Void, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "clip", HLSLBaseType_Void, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Float, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Float, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Float, HLSLBaseType_Float4, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Half, HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Half, HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"dot", HLSLBaseType_Half, HLSLBaseType_Half4, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "dot", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"distance", HLSLBaseType_Float, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"distance", HLSLBaseType_Float, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "distance", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "distance", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "distance", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "distance", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"determinant", HLSLBaseType_Float, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"determinant", HLSLBaseType_Float, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"determinant", HLSLBaseType_Float, HLSLBaseType_Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "determinant", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "determinant", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "determinant", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "determinant", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "determinant", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "determinant", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"cross", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "cross", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "cross", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));



	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f16tof32", HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f16tof32", HLSLBaseType_Float2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f16tof32", HLSLBaseType_Float3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f16tof32", HLSLBaseType_Float4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f32tof16", HLSLBaseType_Uint, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f32tof16", HLSLBaseType_Uint2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f32tof16", HLSLBaseType_Uint3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"f32tof16", HLSLBaseType_Uint4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"NonUniformResourceIndex", HLSLBaseType_Uint, HLSLBaseType_Uint));



	INTRINSIC_VOID_FUNCTION_CACHED("GroupMemoryBarrierWithGroupSync")
	INTRINSIC_VOID_FUNCTION_CACHED("GroupMemoryBarrier")
	INTRINSIC_VOID_FUNCTION_CACHED("DeviceMemoryBarrierWithGroupSync")
	INTRINSIC_VOID_FUNCTION_CACHED("DeviceMemoryBarrier")
	INTRINSIC_VOID_FUNCTION_CACHED("AllMemoryBarrierWithGroupSync")
	INTRINSIC_VOID_FUNCTION_CACHED("AllMemoryBarrier")

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Float, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Float, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Float, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Half, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Half, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"length", HLSLBaseType_Half, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4));

	//Buffer.Load
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float1x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float1x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float1x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4x4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half1x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half1x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half1x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4x4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float1x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float1x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float1x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4x4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float1x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float1x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float1x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4x4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint1x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint1x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint1x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4x4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int1x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int1x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int1x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3x4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4x2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4x3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4x4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_UserDefined, HLSLBaseType_Int));

	//Texture.Load
	//Texture1D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4, HLSLBaseType_Int2));

	//Texture1DArray, Texture 2D, Texture2DMSArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int3));


	//Texture2DMS Texture2DMSArray (Sample Index)
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));


	//Texture2DMS
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4, HLSLBaseType_Int3));

	//Texture2DArray, Texture 3D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Store", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Store", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));

	//Texture1D
	//UINT MipLevel, UINT Width, UINT NumberOfLevels
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	//UINT Width
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint));
	//UINT MipLevel, float Width, float NumberOfLevels
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Float, HLSLBaseType_Float));
	//float Width
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Float));

	//Texture1DArray
	//UINT MipLevel, UINT Width, UINT Elements, UINT NumberOfLevels
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	//UINT Width, UINT Elements
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	//UINT MipLevel, float Width, float Elements, float NumberOfLevels
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	//float Width, float Elements
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Float, HLSLBaseType_Float));

	//UINT MipLevel, UINT Width, UINT Height, UINT Elements, UINT NumberOfLevels

	//Texture2DArray
	//UINT MipLevel, UINT Width, UINT Height, UINT Elements, UINT NumberOfLevels
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	//UINT Width, UINT Height, UINT Elements
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	//UINT MipLevel, float Width, float Height, float Elements, float NumberOfLevels
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	//float Width, float Height, float Elements
	//float Width, float Height, float Depth
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));

	//TextureCube
	//UINT MipLevel, float Width, float Height, UINT NumberOfLevels
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Uint));

	//Texture2DMSArray
	//float Width, float Height, float Elements, float Samples
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));


	INTRINSIC_FLOAT2_FUNCTION_CACHED("max")
	INTRINSIC_FLOAT2_FUNCTION_CACHED("min")

	INTRINSIC_INT2_FUNCTION_CACHED("max")
	INTRINSIC_INT2_FUNCTION_CACHED("min")

	// @@ Add all combinations.

	// vector<N> = mul(vector<N>, scalar)
	// vector<N> = mul(vector<N>, vector<N>)

	// vector<N> = mul(matrix<N,M>, vector<M>)
	// matrix<N,N> = mul(matrix<N,M>, matrix<M,N>)

	// https://msdn.microsoft.com/en-us/library/windows/desktop/bb509628(v=vs.85).aspx
	// Multiplies x and y using matrix math. The inner dimension x-columns and y-rows must be equal.
	// x : [in] The x input value.If x is a vector, it treated as a row vector.
	// y : [in] The y input value.If y is a vector, it treated as a column vector.

	INTRINSIC_FLOAT2_FUNCTION_CACHED("mul")

	// scalar = mul(scalar, scalar)
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));

	// vector<N> = mul(scalar, vector<N>)
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4));

	// vector<NxM> = mul(scalar, Matrix<NxM>)
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x2, HLSLBaseType_Float, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x3, HLSLBaseType_Float, HLSLBaseType_Float2x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x4, HLSLBaseType_Float, HLSLBaseType_Float2x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x2, HLSLBaseType_Float, HLSLBaseType_Float3x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x3, HLSLBaseType_Float, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x4, HLSLBaseType_Float, HLSLBaseType_Float3x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x2, HLSLBaseType_Float, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x3, HLSLBaseType_Float, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x4, HLSLBaseType_Float, HLSLBaseType_Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x2, HLSLBaseType_Half, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x3, HLSLBaseType_Half, HLSLBaseType_Half2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x4, HLSLBaseType_Half, HLSLBaseType_Half2x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x2, HLSLBaseType_Half, HLSLBaseType_Half3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x3, HLSLBaseType_Half, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x4, HLSLBaseType_Half, HLSLBaseType_Half3x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x2, HLSLBaseType_Half, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x3, HLSLBaseType_Half, HLSLBaseType_Half4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x4, HLSLBaseType_Half, HLSLBaseType_Half4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4x4));

	// vector<N> = mul(vector<N>, scalar)
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float));

	// scalar = mul(vector<N>, vector<N>) !!!!
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float, HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half, HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half, HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half, HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	// vector<N> = mul(vector<M>, matrix<M,N>) rows = same dimension(s) as input x, columns = any

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4, HLSLBaseType_Float2, HLSLBaseType_Float2x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2, HLSLBaseType_Float3, HLSLBaseType_Float3x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2, HLSLBaseType_Float4, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3, HLSLBaseType_Float4, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half2, HLSLBaseType_Half2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half2, HLSLBaseType_Half2x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half3, HLSLBaseType_Half3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half3, HLSLBaseType_Half3x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half4, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half4, HLSLBaseType_Half4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x4));

	// matrix<MxN> = mul(matrix<MxN>, scalar)
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x4, HLSLBaseType_Float));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x4, HLSLBaseType_Float));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x4, HLSLBaseType_Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x2, HLSLBaseType_Half2x2, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x3, HLSLBaseType_Half2x3, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x4, HLSLBaseType_Half2x4, HLSLBaseType_Half));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x2, HLSLBaseType_Half3x2, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x3, HLSLBaseType_Half3x3, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x4, HLSLBaseType_Half3x4, HLSLBaseType_Half));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x2, HLSLBaseType_Half4x2, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x3, HLSLBaseType_Half4x3, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x4, HLSLBaseType_Half4x4, HLSLBaseType_Half));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float));

	// vector<M> = mul(matrix<MxN>, vector<N>)
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2, HLSLBaseType_Float2x3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2, HLSLBaseType_Float2x4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3, HLSLBaseType_Float3x2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3, HLSLBaseType_Float3x3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3, HLSLBaseType_Float3x4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4, HLSLBaseType_Float4x2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4, HLSLBaseType_Float4x3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4, HLSLBaseType_Float4x4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half2x3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2, HLSLBaseType_Half2x4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half3x3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3, HLSLBaseType_Half3x4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half4x3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4, HLSLBaseType_Half4x4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4));

	// vector<MxL> = mul(matrix<MxN>, matrix<NxL>)
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x2, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x2, HLSLBaseType_Float2x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x2, HLSLBaseType_Float2x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x3, HLSLBaseType_Float3x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x3, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x3, HLSLBaseType_Float3x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x2, HLSLBaseType_Float2x4, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x3, HLSLBaseType_Float2x4, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float2x4, HLSLBaseType_Float2x4, HLSLBaseType_Float4x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x2, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x2, HLSLBaseType_Float2x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x2, HLSLBaseType_Float2x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x3, HLSLBaseType_Float3x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x3, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x3, HLSLBaseType_Float3x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x2, HLSLBaseType_Float3x4, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x3, HLSLBaseType_Float3x4, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float3x4, HLSLBaseType_Float3x4, HLSLBaseType_Float4x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x2, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x2, HLSLBaseType_Float2x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x2, HLSLBaseType_Float2x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x3, HLSLBaseType_Float3x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x3, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x3, HLSLBaseType_Float3x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x2, HLSLBaseType_Float4x4, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x3, HLSLBaseType_Float4x4, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mul", HLSLBaseType_Float4x4, HLSLBaseType_Float4x4, HLSLBaseType_Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x3, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x4, HLSLBaseType_Half2x2, HLSLBaseType_Half2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x3, HLSLBaseType_Half2x3, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x4, HLSLBaseType_Half2x3, HLSLBaseType_Half3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x2, HLSLBaseType_Half2x4, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x3, HLSLBaseType_Half2x4, HLSLBaseType_Half4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half2x4, HLSLBaseType_Half2x4, HLSLBaseType_Half4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x2, HLSLBaseType_Half3x2, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x3, HLSLBaseType_Half3x2, HLSLBaseType_Half2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x4, HLSLBaseType_Half3x2, HLSLBaseType_Half2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x3, HLSLBaseType_Half3x3, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x4, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x2, HLSLBaseType_Half3x4, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x3, HLSLBaseType_Half3x4, HLSLBaseType_Half4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half3x4, HLSLBaseType_Half3x4, HLSLBaseType_Half4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x2, HLSLBaseType_Half4x2, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x3, HLSLBaseType_Half4x2, HLSLBaseType_Half2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x4, HLSLBaseType_Half4x2, HLSLBaseType_Half2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x3, HLSLBaseType_Half4x3, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x4, HLSLBaseType_Half4x3, HLSLBaseType_Half3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x2, HLSLBaseType_Half4x4, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x3, HLSLBaseType_Half4x4, HLSLBaseType_Half4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Half4x4, HLSLBaseType_Half4x4, HLSLBaseType_Half4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float2x4, HLSLBaseType_Min16Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float3x4, HLSLBaseType_Min16Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float2x4, HLSLBaseType_Min10Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float3x4, HLSLBaseType_Min10Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float2x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float2x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float3x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float3x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mul", HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"transpose", HLSLBaseType_Float2x2, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"transpose", HLSLBaseType_Float3x3, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"transpose", HLSLBaseType_Float4x4, HLSLBaseType_Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Half2x2, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Half3x3, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Half4x4, HLSLBaseType_Half4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Min16Float4x4, HLSLBaseType_Min16Float4x4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "transpose", HLSLBaseType_Min10Float4x4, HLSLBaseType_Min10Float4x4));

	INTRINSIC_FLOAT1_FUNCTION_CACHED("normalize")
	INTRINSIC_FLOAT2_FUNCTION_CACHED("pow")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("saturate")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("sin")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("sqrt")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("rsqrt")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("rcp")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("exp")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("exp2")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("log")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("log2")

	INTRINSIC_FLOAT1_FUNCTION_CACHED("ddx")
	INTRINSIC_FLOAT1_FUNCTION_CACHED("ddy")

	INTRINSIC_FLOAT1_FUNCTION_CACHED("sign")
	INTRINSIC_FLOAT2_FUNCTION_CACHED("step")
	INTRINSIC_FLOAT2_FUNCTION_CACHED("reflect")

	INTRINSIC_FLOAT1_BOOL_FUNCTION_CACHED("isnan")
	INTRINSIC_FLOAT1_BOOL_FUNCTION_CACHED("isinf")

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float1x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float1x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float1x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float1x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float1x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float1x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Bool1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Bool1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Bool1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Int1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Int1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Int1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Uint1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Uint1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Uint1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Uint4));



	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float4x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half2x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half3x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half4x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Half4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float1x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float1x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float1x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min16Float4x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float1x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float1x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float1x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float2x2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float3x3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float4x4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asfloat", HLSLBaseType_Float, HLSLBaseType_Min10Float4x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Bool1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Bool1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Bool1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Int1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Int1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Int1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Uint1x2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Uint1x3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Uint1x4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asfloat", HLSLBaseType_Float, HLSLBaseType_Uint4));


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"countbits", HLSLBaseType_Uint, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"asuint", HLSLBaseType_Uint, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2D", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dproj", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dlod", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dlod", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4, HLSLBaseType_Int2));   // With offset.
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dbias", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dgrad", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dgather", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dgather", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Int2, HLSLBaseType_Int));    // With offset.
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dsize", HLSLBaseType_Int2, HLSLBaseType_Sampler2D));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dfetch", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Int3));    // u,v,mipmap

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2Dcmp", HLSLBaseType_Float4, HLSLBaseType_Sampler2DShadow, HLSLBaseType_Float4));                // @@ IC: This really takes a float3 (uvz) and returns a float.

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2DMSfetch", HLSLBaseType_Float4, HLSLBaseType_Sampler2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2DMSsize", HLSLBaseType_Int3, HLSLBaseType_Sampler2DMS));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex2DArray", HLSLBaseType_Float4, HLSLBaseType_Sampler2DArray, HLSLBaseType_Float3));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex3D", HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex3Dlod", HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex3Dbias", HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"tex3Dsize", HLSLBaseType_Int3, HLSLBaseType_Sampler3D));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Sample", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	//Offset

	//Texture1D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int, HLSLBaseType_Uint));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int, HLSLBaseType_Uint2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint2));

	//Texture3D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleLevel", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int, HLSLBaseType_Uint3));

	//Texture1D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	//TextureCube
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	//TextureCubeArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmp", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	//Texture1D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	//TextureCube
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	//TextureCubeArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Half, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min16Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Min10Float, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleBias", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Int));

	//Texture1D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	//Texture3D //TextureCube
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Int3));


	//TextureCubeArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GatherRed", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GatherRed", HLSLBaseType_Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Half4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min16Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min10Float4, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBE", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBElod", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBEbias", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBEsize", HLSLBaseType_Int, HLSLBaseType_SamplerCube));

	//GLSL doesn't support float type atomic functions, thus resticted Interlocked functions' data type
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedAdd", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedCompareExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedCompareStore", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedCompareStore", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedExchange", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMax", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedMin", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedOr", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Uint, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Uint, HLSLBaseType_Int));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"InterlockedXor", HLSLBaseType_Void, HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[0].modifier = HLSLArgumentModifier_Inout;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"sincos", HLSLBaseType_Void, HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "sincos", HLSLBaseType_Void, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"mad", HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "mad", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"refract", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"refract", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"refract", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"refract", HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"refract", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"refract", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "refract", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "refract", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "refract", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "refract", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "refract", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "refract", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float));


	//Shader Model 6.0
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveGetLaneCount", HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveGetLaneIndex", HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveIsHelperLane", HLSLBaseType_Bool));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAnyTrue", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllTrue", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBallot", HLSLBaseType_Uint4, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveBallot", HLSLBaseType_Uint4, HLSLBaseType_Bool));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Bool, HLSLBaseType_Bool, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Bool2, HLSLBaseType_Bool2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Bool3, HLSLBaseType_Bool3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Bool4, HLSLBaseType_Bool4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneAt", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Uint));


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Int2, HLSLBaseType_Int2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Int3, HLSLBaseType_Int3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Int4, HLSLBaseType_Int4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Uint2, HLSLBaseType_Uint2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Uint3, HLSLBaseType_Uint3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_Uint4, HLSLBaseType_Uint4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneAt", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined, HLSLBaseType_Uint));



	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Bool4, HLSLBaseType_Bool4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveReadLaneFirst", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveReadLaneFirst", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));



	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Bool4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveAllEqual", HLSLBaseType_Bool, HLSLBaseType_UserDefined));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Bool4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Bool, HLSLBaseType_UserDefined));


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitOr", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitOr", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitOr", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitOr", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitXor", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitXor", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitXor", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitXor", HLSLBaseType_Int4, HLSLBaseType_Int4));

	//WaveActiveCountBits 
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Uint, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Uint, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Uint, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveBitAnd", HLSLBaseType_Uint, HLSLBaseType_Bool));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMax", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMax", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveMin", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveMin", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveProduct", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveProduct", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WaveActiveSum", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveActiveSum", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));


	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveGetLaneIndex", HLSLBaseType_Uint, HLSLBaseType_Bool));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixSum", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixSum", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "WavePrefixProduct", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WavePrefixProduct", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveIsHelperLane", HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"WaveIsFirstLane", HLSLBaseType_Bool));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Bool, HLSLBaseType_Bool, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Bool2, HLSLBaseType_Bool2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Bool3, HLSLBaseType_Bool3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Bool4, HLSLBaseType_Bool4, HLSLBaseType_Uint));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Float4, HLSLBaseType_Float4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Half, HLSLBaseType_Half, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Half2, HLSLBaseType_Half2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Half3, HLSLBaseType_Half3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Half4, HLSLBaseType_Half4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadLaneAt", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Int, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Int2, HLSLBaseType_Int2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Int3, HLSLBaseType_Int3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Int4, HLSLBaseType_Int4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Uint2, HLSLBaseType_Uint2, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Uint3, HLSLBaseType_Uint3, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_Uint4, HLSLBaseType_Uint4, HLSLBaseType_Uint));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadLaneAt", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined, HLSLBaseType_Uint));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapX", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapX", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadSwapY", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadSwapY", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossX", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossX", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));

	/*
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Bool, HLSLBaseType_Bool));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Bool2, HLSLBaseType_Bool2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Bool3, HLSLBaseType_Bool3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Bool4, HLSLBaseType_Bool4));
	*/
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Float4, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Half2, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Half3, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Half4, HLSLBaseType_Half4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "QuadReadAcrossY", HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Int, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Int2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Int3, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Int4, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Uint2, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Uint3, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_Uint4, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"QuadReadAcrossY", HLSLBaseType_UserDefined, HLSLBaseType_UserDefined));
#endif
}


HLSLParser::IntrinsicHelper::IntrinsicHelper()
{
}

HLSLParser::IntrinsicHelper::~IntrinsicHelper()
{
	for (int i = 0; i < m_intrinsics.size(); i++)
	{
		if (m_intrinsics[i] != NULL)
		{
			delete m_intrinsics[i];
			m_intrinsics[i] = NULL;
		}
	}

}



// The order in this array must match up with HLSLBinaryOp
const int _binaryOpPriority[] =
	{
		 2, // &&
		 1, // ||
		 9, // +
		 9, // -
		10, // *
		10, // /
		 7, // <
		 7, // >
		 7, // <=
		 7, // >=
		 6, // ==
		 6, // !=
		 5, // &
		 3, // |
		 4, // ^
		 8, // <<
		 8, // >>
		 9 // %
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


		{ "min16float",              NumericType_Min16Float,      1, 0, 1,  1 },      // HLSLBaseType_Min16Float
		{ "min16float1x2",           NumericType_Min16Float,      1, 2, 2,  1 },      // HLSLBaseType_Min16Float1x2
		{ "min16float1x3",           NumericType_Min16Float,      1, 2, 3,  1 },      // HLSLBaseType_Min16Float1x3
		{ "min16float1x4",           NumericType_Min16Float,      1, 2, 4,  1 },      // HLSLBaseType_Min16Float1x4
		{ "min16float2",             NumericType_Min16Float,      2, 1, 1,  1 },      // HLSLBaseType_Min16Float2
		{ "min16float2x2",           NumericType_Min16Float,      2, 2, 2,  1 },      // HLSLBaseType_Min16Float2x2
		{ "min16float2x3",           NumericType_Min16Float,      2, 2, 3,  1 },      // HLSLBaseType_Min16Float2x3
		{ "min16float2x4",           NumericType_Min16Float,      2, 2, 4,  1 },      // HLSLBaseType_Min16Float2x4
		{ "min16float3",             NumericType_Min16Float,      3, 1, 1,  1 },      // HLSLBaseType_Min16Float3
		{ "min16float3x2",           NumericType_Min16Float,      3, 2, 2,  1 },      // HLSLBaseType_Min16Float3x2
		{ "min16float3x3",           NumericType_Min16Float,      3, 2, 3,  1 },      // HLSLBaseType_Min16Float3x3
		{ "min16float3x4",           NumericType_Min16Float,      3, 2, 4,  1 },      // HLSLBaseType_Min16Float3x4
		{ "min16float4",             NumericType_Min16Float,      4, 1, 1,  1 },      // HLSLBaseType_Min16Float4
		{ "min16float4x2",           NumericType_Min16Float,      4, 2, 2,  1 },      // HLSLBaseType_Min16Float4x2
		{ "min16float4x3",           NumericType_Min16Float,      4, 2, 3,  1 },      // HLSLBaseType_Min16Float4x3
		{ "min16float4x4",           NumericType_Min16Float,      4, 2, 4,  1 },      // HLSLBaseType_Min16Float4x4


		{ "min10float",              NumericType_Min10Float,      1, 0, 1,  1 },      // HLSLBaseType_Min10Float
		{ "min10float1x2",           NumericType_Min10Float,      1, 2, 2,  1 },      // HLSLBaseType_Min10Float1x2
		{ "min10float1x3",           NumericType_Min10Float,      1, 2, 3,  1 },      // HLSLBaseType_Min10Float1x3
		{ "min10float1x4",           NumericType_Min10Float,      1, 2, 4,  1 },      // HLSLBaseType_Min10Float1x4
		{ "min10float2",             NumericType_Min10Float,      2, 1, 1,  1 },      // HLSLBaseType_Min10Float2
		{ "min10float2x2",           NumericType_Min10Float,      2, 2, 2,  1 },      // HLSLBaseType_Min10Float2x2
		{ "min10float2x3",           NumericType_Min10Float,      2, 2, 3,  1 },      // HLSLBaseType_Min10Float2x3
		{ "min10float2x4",           NumericType_Min10Float,      2, 2, 4,  1 },      // HLSLBaseType_Min10Float2x4
		{ "min10float3",             NumericType_Min10Float,      3, 1, 1,  1 },      // HLSLBaseType_Min10Float3
		{ "min10float3x2",           NumericType_Min10Float,      3, 2, 2,  1 },      // HLSLBaseType_Min10Float3x2
		{ "min10float3x3",           NumericType_Min10Float,      3, 2, 3,  1 },      // HLSLBaseType_Min10Float3x3
		{ "min10float3x4",           NumericType_Min10Float,      3, 2, 4,  1 },      // HLSLBaseType_Min10Float3x4
		{ "min10float4",             NumericType_Min10Float,      4, 1, 1,  1 },      // HLSLBaseType_Min10Float4
		{ "min10float4x2",           NumericType_Min10Float,      4, 2, 2,  1 },      // HLSLBaseType_Min10Float4x2
		{ "min10float4x3",           NumericType_Min10Float,      4, 2, 3,  1 },      // HLSLBaseType_Min10Float4x3
		{ "min10float4x4",           NumericType_Min10Float,      4, 2, 4,  1 },      // HLSLBaseType_Min10Float4x4

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


		{ "inputPatch",         NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_InputPatch
		{ "outputPatch",        NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_OutputPatch

		{ "pointStream",		NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_PointStream
		{ "lineStream",         NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_LineStream
		{ "triangleStream",     NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_TriangleStream

		{ "point",				NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Point
		{ "line",				NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Line
		{ "triangle",			NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Triangle
		{ "lineadj",			NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Lineadj
		{ "triangleadj",		NumericType_NaN,        1, 0, 0, -1 },     // HLSLBaseType_Triangleadj		

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
// jhable: Adding the component types to use the min. I.e. (float3 + float4) => float3
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

		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown

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

			HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown
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

			HLSLBaseType_Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x3, HLSLBaseType_Unknown
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

			HLSLBaseType_Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x4,

			HLSLBaseType_Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x4
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

			HLSLBaseType_Half, HLSLBaseType_Half1x2, HLSLBaseType_Half1x3, HLSLBaseType_Half1x4,
			HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half2x4,
			HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4,
			HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half4x4,

			HLSLBaseType_Half, HLSLBaseType_Half1x2, HLSLBaseType_Half1x3, HLSLBaseType_Half1x4,
			HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half2x4,
			HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4,
			HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half, HLSLBaseType_Half1x2, HLSLBaseType_Half1x3, HLSLBaseType_Half1x4,
			HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half2x4,
			HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4,
			HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half4x4,

			HLSLBaseType_Half, HLSLBaseType_Half1x2, HLSLBaseType_Half1x3, HLSLBaseType_Half1x4,
			HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half2x4,
			HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4,
			HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half4x4
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

			HLSLBaseType_Half1x2, HLSLBaseType_Half1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half1x2, HLSLBaseType_Half1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x2, HLSLBaseType_Half1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Half1x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Half1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Half1x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half1x4,
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

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // half2
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
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

			HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x4,
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

			HLSLBaseType_Half2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x4,
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

			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Half3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x4,
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

			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
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

			HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown
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

			HLSLBaseType_Half4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x3, HLSLBaseType_Unknown
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

			HLSLBaseType_Half4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x4,

			HLSLBaseType_Half4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x4,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x4,

			HLSLBaseType_Half4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x4
		},
	

		{   // min16float 
			HLSLBaseType_Float, HLSLBaseType_Float1x2, HLSLBaseType_Float1x3, HLSLBaseType_Float1x4,
			HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2x3, HLSLBaseType_Float2x4,
			HLSLBaseType_Float3, HLSLBaseType_Float3x2, HLSLBaseType_Float3x3, HLSLBaseType_Float3x4,
			HLSLBaseType_Float4, HLSLBaseType_Float4x2, HLSLBaseType_Float4x3, HLSLBaseType_Float4x4,

			HLSLBaseType_Half, HLSLBaseType_Half1x2, HLSLBaseType_Half1x3, HLSLBaseType_Half1x4,
			HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half2x4,
			HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4,
			HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half4x4,
			
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

			HLSLBaseType_Min16Float, HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x3, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Min16Float, HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x3, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Min16Float2, HLSLBaseType_Min16Float2x2, HLSLBaseType_Min16Float2x3, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Min16Float3, HLSLBaseType_Min16Float3x2, HLSLBaseType_Min16Float3x3, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Min16Float4, HLSLBaseType_Min16Float4x2, HLSLBaseType_Min16Float4x3, HLSLBaseType_Min16Float4x4,
		},
		{   // min16float 1x2
			HLSLBaseType_Float1x2, HLSLBaseType_Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x2, HLSLBaseType_Half1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x2, HLSLBaseType_Min16Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // min16float 1x3
			HLSLBaseType_Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Half1x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x4
			HLSLBaseType_Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half1x4,
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

			HLSLBaseType_Min16Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float2
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x2
			HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x3
			HLSLBaseType_Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x4
			HLSLBaseType_Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x4,
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

			HLSLBaseType_Min16Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

	   {   // float3
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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
			HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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
			HLSLBaseType_Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x4
			HLSLBaseType_Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x4,
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

			HLSLBaseType_Min16Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float4
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

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
			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x2
			HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

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

			HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x3
			HLSLBaseType_Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x3, HLSLBaseType_Unknown,

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

			HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x3, HLSLBaseType_Unknown
		},
		{   // float 4x4
			HLSLBaseType_Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x4,

			HLSLBaseType_Half4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x4,

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

			HLSLBaseType_Min16Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x4,

			HLSLBaseType_Min16Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float4x4
		},


		{   // min10float 
			HLSLBaseType_Float, HLSLBaseType_Float1x2, HLSLBaseType_Float1x3, HLSLBaseType_Float1x4,
			HLSLBaseType_Float2, HLSLBaseType_Float2x2, HLSLBaseType_Float2x3, HLSLBaseType_Float2x4,
			HLSLBaseType_Float3, HLSLBaseType_Float3x2, HLSLBaseType_Float3x3, HLSLBaseType_Float3x4,
			HLSLBaseType_Float4, HLSLBaseType_Float4x2, HLSLBaseType_Float4x3, HLSLBaseType_Float4x4,

			HLSLBaseType_Half, HLSLBaseType_Half1x2, HLSLBaseType_Half1x3, HLSLBaseType_Half1x4,
			HLSLBaseType_Half2, HLSLBaseType_Half2x2, HLSLBaseType_Half2x3, HLSLBaseType_Half2x4,
			HLSLBaseType_Half3, HLSLBaseType_Half3x2, HLSLBaseType_Half3x3, HLSLBaseType_Half3x4,
			HLSLBaseType_Half4, HLSLBaseType_Half4x2, HLSLBaseType_Half4x3, HLSLBaseType_Half4x4,

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

			HLSLBaseType_Min10Float, HLSLBaseType_Min10Float1x2, HLSLBaseType_Min10Float1x3, HLSLBaseType_Min10Float1x4,
			HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float2x4,
			HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x4,
			HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float4x4,

			HLSLBaseType_Min10Float, HLSLBaseType_Min10Float1x2, HLSLBaseType_Min10Float1x3, HLSLBaseType_Min10Float1x4,
			HLSLBaseType_Min10Float2, HLSLBaseType_Min10Float2x2, HLSLBaseType_Min10Float2x3, HLSLBaseType_Min10Float2x4,
			HLSLBaseType_Min10Float3, HLSLBaseType_Min10Float3x2, HLSLBaseType_Min10Float3x3, HLSLBaseType_Min10Float3x4,
			HLSLBaseType_Min10Float4, HLSLBaseType_Min10Float4x2, HLSLBaseType_Min10Float4x3, HLSLBaseType_Min10Float4x4,
		},
		{   // float 1x2
			HLSLBaseType_Float1x2, HLSLBaseType_Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x2, HLSLBaseType_Half1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min10Float1x2, HLSLBaseType_Min10Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float1x2, HLSLBaseType_Min10Float1x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x3
			HLSLBaseType_Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x3, HLSLBaseType_Unknown, HLSLBaseType_Half1x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min10Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min10Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float1x3, HLSLBaseType_Unknown, HLSLBaseType_Min10Float1x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 1x4
			HLSLBaseType_Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min16Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float1x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float1x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float2
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x2
			HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min10Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x3
			HLSLBaseType_Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min10Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 2x4
			HLSLBaseType_Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half2x4,
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

			HLSLBaseType_Min10Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float2x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float2x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

	   {   // float3
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

		   HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

		   HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		   HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
	   },
		{   // float 3x2
			HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min10Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 3x3
			HLSLBaseType_Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x3, HLSLBaseType_Unknown,
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

			HLSLBaseType_Min10Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float3x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x3, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
		},
		{   // float 3x4
			HLSLBaseType_Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half3x4,
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

			HLSLBaseType_Min10Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float3x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float3x4,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},

		{   // float4
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

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

			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x2
			HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Half4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

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

			HLSLBaseType_Min10Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x2, HLSLBaseType_Unknown, HLSLBaseType_Unknown
		},
		{   // float 4x3
			HLSLBaseType_Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Half4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x3, HLSLBaseType_Unknown,

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

			HLSLBaseType_Min10Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x3, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float4x3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x3, HLSLBaseType_Unknown
		},
		{   // float 4x4
			HLSLBaseType_Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Float4x4,

			HLSLBaseType_Half4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Half4x4,

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

			HLSLBaseType_Min10Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x4,

			HLSLBaseType_Min10Float4x4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Min10Float4x4
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
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

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
			HLSLBaseType_Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Half, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Half4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min16Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min16Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

			HLSLBaseType_Min10Float, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float2, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float3, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,
			HLSLBaseType_Min10Float4, HLSLBaseType_Unknown, HLSLBaseType_Unknown, HLSLBaseType_Unknown,

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

CachedString HLSLParser::GetTypeName(const HLSLType& type)
{
	if (type.baseType == HLSLBaseType_UserDefined)
	{
		return type.typeName;
	}
	else if (type.baseType == HLSLBaseType_ConstantBuffer ||
			 type.baseType == HLSLBaseType_StructuredBuffer ||
			 type.baseType == HLSLBaseType_PureBuffer ||
			 type.baseType == HLSLBaseType_RWStructuredBuffer)
	{
		if (type.elementType == HLSLBaseType_UserDefined)
		{
			return type.typeName;
		}
		else
		{
			return m_tree->AddStringCached(_baseTypeDescriptions[type.elementType].typeName);
		}
	}
	else
	{
		return m_tree->AddStringCached(_baseTypeDescriptions[type.baseType].typeName);
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

	case HLSLBinaryOp_LeftShift:    return "<<";
	case HLSLBinaryOp_RightShift:   return ">>";
	case HLSLBinaryOp_Modular:		return "%";

	case HLSLBinaryOp_BitAndAssign: return "&=";
	case HLSLBinaryOp_BitOrAssign:  return "|=";
	case HLSLBinaryOp_BitXorAssign: return "^=";

	default:
		ASSERT_PARSER(0);
		return "???";
	}
}

static std::string SafeString(const char * pStr)
{
	return pStr != NULL ? std::string(pStr) : "";
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
	HLSLBaseType comparingType = HLSLBaseType_Unknown;
	HLSLBaseType comparedType = HLSLBaseType_Unknown;

	bool IsOriginalComparingTypeTexture = false;
	bool IsOriginalComparedTypeTexture = false;

	if (srcType.baseType >= HLSLBaseType_Texture1D && srcType.baseType <= HLSLBaseType_RWTexture3D)
	{
		//no swizzled 
		comparingType = HLSLBaseType_Float4;
		IsOriginalComparingTypeTexture = true;
	}
	else if (srcType.elementType != HLSLBaseType_Unknown)
		comparingType = srcType.elementType;
	else
		comparingType = srcType.baseType;
	
	if (dstType.baseType >= HLSLBaseType_Texture1D && dstType.baseType <= HLSLBaseType_RWTexture3D)
	{
		comparedType = HLSLBaseType_Float4;
		IsOriginalComparedTypeTexture = true;
	}
	else if (dstType.elementType != HLSLBaseType_Unknown)
		comparedType = dstType.elementType;
	else
		comparedType = dstType.baseType;

	if (comparingType == HLSLBaseType_UserDefined && comparedType == HLSLBaseType_UserDefined)
	{
		return String_Equal(srcType.typeName, dstType.typeName) ? 0 : -1;
	}

	if ((comparingType == HLSLBaseType_UserDefined && comparedType != HLSLBaseType_UserDefined) ||
		(comparingType != HLSLBaseType_UserDefined && comparedType == HLSLBaseType_UserDefined))
	{
		// actually, need to compare between the first struct element and another, but just skip
		return 0;
	}

	if (comparingType == comparedType || ((srcType.typeName.IsNotEmpty() && dstType.typeName.IsNotEmpty()) && String_Equal(srcType.typeName , dstType.typeName))  )
	{
		return 0;
	}

	const BaseTypeDescription& srcDesc = _baseTypeDescriptions[comparingType];
	const BaseTypeDescription& dstDesc = _baseTypeDescriptions[comparedType];
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
		

		if (IsOriginalComparingTypeTexture || IsOriginalComparedTypeTexture)
		{
			return 0;
		}
		else// Can't convert
			return -1;
	}
	
	
	return result;
	
}

static bool GetFunctionCallCastRanks(HLSLTree* tree, const HLSLFunctionCall* call, const HLSLFunction* function, int* rankBuffer)
{
	// Note theat the arguments do not need to be the same, because of default parameters.
	if (function == NULL || function->argumentVec.size() < call->numArguments)
	{
		// Function not viable
		return false;
	}

	eastl::vector < HLSLExpression * > expressionVec = call->GetArguments();

	eastl::vector < HLSLArgument * > argumentVec = function->GetArguments();
	ASSERT_PARSER(expressionVec.size() <= argumentVec.size());

	for (int i = 0; i < expressionVec.size(); ++i)
	{
		HLSLExpression* expression = expressionVec[i];// call->callArgument;
		HLSLType expType = expression->expressionType;

		// if we are a texture state expression, and we hae an index type, then we want to return the type
		// of data inside the texture, not the texture itself.
		if (expression->nodeType == HLSLTextureStateExpression::s_type)
		{
			HLSLTextureStateExpression * texExpression = static_cast<HLSLTextureStateExpression*>(expression);

			if (texExpression->indexExpression != NULL)
			{
				HLSLType elemType;
				elemType.baseType = expType.elementType;

				expType = elemType;
			}
			else if(texExpression->functionExpression != NULL)
			{
				HLSLType elemType;
				elemType.baseType = texExpression->functionExpression->expressionType.baseType;

				expType = elemType;

			}
		}

		const HLSLArgument* argument = argumentVec[i];

		int rank = GetTypeCastRank(tree, expType, argument->argType);
		if (rank == -1)
		{
			return false;
		}

		// if it is an in/out param, don't allow any casts
		if (argument->modifier == HLSLArgumentModifier_Inout ||
			argument->modifier == HLSLArgumentModifier_Out)
		{
			if (rank != 0)
			{
				return false;
			}
		}



		rankBuffer[i] = rank;
	}

	for (int i = (int)expressionVec.size(); i < (int)argumentVec.size(); ++i)
	{
		const HLSLArgument* argument = argumentVec[i];
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

bool HLSLParser::GetBinaryOpResultType(HLSLBinaryOp binaryOp, const HLSLType& type1, const HLSLType& type2, HLSLType& result)
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

	result.typeName.Reset();
	result.array        = false;
	result.arraySize    = NULL;
	result.flags        = (type1.flags & type2.flags) & HLSLTypeFlag_Const; // Propagate constness.
	

	return result.baseType != HLSLBaseType_Unknown;

}

HLSLParser::HLSLParser(StringLibrary * stringLibrary, IntrinsicHelper * intrinsicHelper, FullTokenizer * pTokenizer, const char* entryName, Parser::Target target, Parser::Language language, const char debugTokenFileName[])
{
	m_pStringLibrary = stringLibrary;
	m_pFullTokenizer = pTokenizer;

	if (debugTokenFileName != nullptr)
	{
		if (strlen(debugTokenFileName) > 0)
		{
			m_pFullTokenizer->DumpTokensToFile(debugTokenFileName);
		}
	}

	m_numGlobals = 0;
	m_tree = NULL;

	m_entryNameStr = entryName;

	m_intrinsicHelper = intrinsicHelper;

	eastl::vector < eastl::string > allNames = m_pFullTokenizer->GetAllFileNames();

	// add strings to string library
	for (int i = 0; i < allNames.size(); i++)
	{
		stringLibrary->InsertDirect(allNames[i]);
	}

}

bool HLSLParser::Check(int token)
{
	if (m_pFullTokenizer->GetToken() == token)
	{
		return true;
	}
	return false;
}

bool HLSLParser::Accept(int token)
{
	if (m_pFullTokenizer->GetToken() == token)
	{
		m_pFullTokenizer->Next();
	   return true;
	}
	return false;
}

bool HLSLParser::Accept(const char* token)
{
	if (m_pFullTokenizer->GetToken() == HLSLToken_Identifier && String_Equal( token, m_pFullTokenizer->GetIdentifier() ) )
	{
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::Expect(int token)
{
	if (!Accept(token))
	{
		char want[HLSLTokenizer::s_maxIdentifier];
		m_pFullTokenizer->GetTokenName(token, want);
		char nearToken[HLSLTokenizer::s_maxIdentifier];
		m_pFullTokenizer->GetTokenName(nearToken);
		m_pFullTokenizer->Error("Syntax error: expected '%s' near '%s'", want, nearToken);
		return false;
	}
	return true;
}

bool HLSLParser::Expect(const char * token)
{
	if (!Accept(token))
	{
		const char * want = token;
		char nearToken[HLSLTokenizer::s_maxIdentifier];
		m_pFullTokenizer->GetTokenName(nearToken);
		m_pFullTokenizer->Error("Syntax error: expected '%s' near '%s'", want, nearToken);
		return false;
	}
	return true;
}

bool HLSLParser::AcceptIdentifier(CachedString & identifier)
{
	//return AcceptIdentifier(identifier.m_str);

	if (m_pFullTokenizer->GetToken() == HLSLToken_Identifier)
	{
		identifier = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::ExpectIdentifier(CachedString & identifier)
{
	if (!AcceptIdentifier(identifier))
	{

		char nearToken[HLSLTokenizer::s_maxIdentifier];
		m_pFullTokenizer->GetTokenName(nearToken);
		m_pFullTokenizer->Error("Syntax error: expected identifier near '%s'", nearToken);
		return false;
	}

	return true;

}

bool HLSLParser::AcceptFloat(float& value)
{
	if (m_pFullTokenizer->GetToken() == HLSLToken_FloatLiteral)
	{
		value = m_pFullTokenizer->GetFloat();
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptHalf( float& value )
{
	if(m_pFullTokenizer->GetToken() == HLSLToken_HalfLiteral )
	{
		value = m_pFullTokenizer->GetFloat();
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptMin16Float(float& value)
{
	if (m_pFullTokenizer->GetToken() == HLSLToken_Min16FloatLiteral)
	{
		value = m_pFullTokenizer->GetFloat();
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptMin10Float(float& value)
{
	if (m_pFullTokenizer->GetToken() == HLSLToken_Min10FloatLiteral)
	{
		value = m_pFullTokenizer->GetFloat();
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptInt(int& value)
{
	if (m_pFullTokenizer->GetToken() == HLSLToken_IntLiteral)
	{
		value = m_pFullTokenizer->GetInt();
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptUint(unsigned int& value)
{
	if (m_pFullTokenizer->GetToken() == HLSLToken_UintLiteral)
	{
		value = m_pFullTokenizer->GetuInt();
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

const char * HLSLParser::GetCstr(const CachedString & currStr) const
{
	return FetchCstr(m_pStringLibrary, currStr);
}


bool HLSLParser::ParseTopLevel(HLSLStatement*& statement)
{
	HLSLAttribute * attributes = NULL;
	ParseAttributeBlock(attributes);

	int line             = GetLineNumber();
	const char* fileName = GetFileName();
	
	HLSLBaseType type;
	CachedString typeName;
	int          typeFlags = false;

	bool doesNotExpectSemicolon = false;

	if (Accept(HLSLToken_Struct))
	{
		// Struct declaration

		// 00. add new Struct to the main tree
		HLSLStruct* structure = m_tree->AddNode<HLSLStruct>(fileName, line);

		m_userTypes.push_back(structure);

		// 01. get Identifier
		CachedString structName;
		if (!ExpectIdentifier(structName))
			return false;
		else
			structure->name = structName;
				
		// If it is a Hull shader for MSL, hide it to avoid to print same duplicated one
		if (FindUserDefinedType(structName) != NULL)
		{
			if (m_target == Parser::Target_HullShader && m_language == Parser::Language_MSL)
				structure->hidden = true;
		}

		// 02. Body
		if (Expect('{'))
		{
			HLSLStructField* lastField = NULL;

			// Add the struct to our list of user defined types.
			while (!Accept('}'))
			{
				if (CheckForUnexpectedEndOfStream('}'))
				{
					return false;
				}

				HLSLStructField* field = NULL;
				
				if (!ParseFieldDeclaration(field))
				{
					return false;
				}			

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
		}
		else 
			return false; 
		
		// free to use semicolon
		if (!Check(';'))
			doesNotExpectSemicolon = true;

		statement = structure;
	}
	else if(HLSLToken_First_Buffer_Type <= m_pFullTokenizer->GetToken() && HLSLToken_Last_Buffer_Type >= m_pFullTokenizer->GetToken())
	{
		// Buffer declaration

		// 00. add new buffer to the main tree
		HLSLBuffer* buffer = m_tree->AddNode<HLSLBuffer>(fileName, line);

		//get buffer's baseType
		AcceptBufferType(buffer);

		// Constant Buffer
		if (buffer->type.baseType == HLSLBaseType_CBuffer ||
			buffer->type.baseType == HLSLBaseType_ConstantBuffer ||
			buffer->type.baseType == HLSLBaseType_TBuffer)
		{
			// Constant Buffer declaration  
			// 01. get element type (optional)
			if (!GetBufferElementType(buffer, false, &typeFlags, true))
				return false;

			if (buffer->type.baseType == HLSLBaseType_ConstantBuffer)
			{
				buffer->userDefinedElementTypeStr = buffer->type.typeName;
			}

			// 02. get Identifier			
			if (!ExpectIdentifier(buffer->cachedName))
				return false;

			// If it is a Hull shader for MSL, hide it to avoid to print same duplicated one
			if (FindBuffer(buffer->cachedName) != NULL)
			{
				if (m_target == Parser::Target_HullShader && m_language == Parser::Language_MSL)
					buffer->hidden = true;
			}			

			// Confetti's Rule : if buffer name includes "rootconstant", it is constant buffer			
			if(stristr(GetCstr(buffer->cachedName), "rootconstant"))
				buffer->bPushConstant = true;

			// 03. get assigned register (necessary)
			if (!GetRegisterAssignment(buffer, "Constant Buffer (register b)"))
				return false;
			
			// 04. get Body (optional)
			GetBufferBody(buffer);

			// 05. add it as a Global variable
			if (buffer->bArray)
				buffer->type.array = true;
			else
				buffer->type.array = false;

			DeclareVariable(buffer->cachedName, buffer->type);

			// free to use semicolon
			if (!Check(';'))
				doesNotExpectSemicolon = true;
		}
		else if (buffer->type.baseType == HLSLBaseType_RWBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedBuffer)
		{
			// 01. get element type (necessary) and base type
			if (!GetBufferElementType(buffer, false, &typeFlags, false))
				return false;					

			// 02. get Identifier			
			if (!ExpectIdentifier(buffer->cachedName))
				return false;

			// 03. get array (optional)
			GetBufferArray(buffer);

			// 04. get assigned register (necessary)
			if (!GetRegisterAssignment(buffer, "RWBuffer (register u)"))
				return false;
			
			// 05. add it as a Global variable
			if (buffer->bArray)
				buffer->type.array = true;
			else
				buffer->type.array = false;

			DeclareVariable(buffer->cachedName, buffer->type);
		}
		else if (buffer->type.baseType == HLSLBaseType_StructuredBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedStructuredBuffer)
		{
			// 01. get element type (necessary) and base type
			if (!GetBufferElementType(buffer, false, &typeFlags, false))
				return false;

			// 02. get Identifier			
			if (!ExpectIdentifier(buffer->cachedName))
				return false;

			// 03. get array (optional)
			GetBufferArray(buffer);

			// 04. get assigned register (necessary)
			if (!GetRegisterAssignment(buffer, "Structure Buffer (register t)"))
				return false;

			// 05. add it as a Global variable
			if (buffer->bArray)
				buffer->type.array = true;
			else
				buffer->type.array = false;

			DeclareVariable(buffer->cachedName, buffer->type);
		}
		else if (buffer->type.baseType == HLSLBaseType_PureBuffer)
		{
			// 01. get element type (necessary) and base type
			if (!GetBufferElementType(buffer, false, &typeFlags, false))
				return false;

			// 02. get Identifier			
			if (!ExpectIdentifier(buffer->cachedName))
				return false;

			// 03. get array (optional)
			GetBufferArray(buffer);

			// 04. get assigned register (necessary)
			if (!GetRegisterAssignment(buffer, "Buffer (register t)"))
				return false;

			// 05. add it as a Global variable
			if (buffer->bArray)
				buffer->type.array = true;
			else
				buffer->type.array = false;

			DeclareVariable(buffer->cachedName, buffer->type);
		}
		else if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer)
		{
			// 01. get element type (necessary) and base type
			if (!GetBufferElementType(buffer, false, &typeFlags, false))
				return false;

			// 02. get Identifier			
			if (!ExpectIdentifier(buffer->cachedName))
				return false;

			// 03. get array (optional)
			GetBufferArray(buffer);

			// 04. get assigned register (necessary)
			if (!GetRegisterAssignment(buffer, "RWStructure Buffer (register u)"))
				return false;

			// 05. add it as a Global variable			

			if (buffer->bArray)
				buffer->type.array = true;
			else
				buffer->type.array = false;
			
			DeclareVariable(buffer->cachedName, buffer->type);
		}
		else if (buffer->type.baseType == HLSLBaseType_ByteAddressBuffer)
		{
			// 01. get Identifier			
			if (!ExpectIdentifier(buffer->cachedName))
				return false;

			// 02. get array (optional)
			GetBufferArray(buffer);

			// 03. get assigned register (necessary)
			if (!GetRegisterAssignment(buffer, "ByteAddress Buffer (register t)"))
				return false;

			buffer->type.array = true;

			//assume that ByteAddressBuffer is using for Int
			buffer->type.elementType = HLSLBaseType_Int;


			DeclareVariable(buffer->cachedName, buffer->type);
		}
		else if (buffer->type.baseType == HLSLBaseType_RWByteAddressBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedByteAddressBuffer)
		{
			// 01. get Identifier			
			if (!ExpectIdentifier(buffer->cachedName))
				return false;

			// 02. get array (optional)
			GetBufferArray(buffer);

			// 03. get assigned register (necessary)
			if (!GetRegisterAssignment(buffer, "ByteAddress Buffer (register t)"))
				return false;

			buffer->type.array = true;

			//assume that ByteAddressBuffer is using for Int
			buffer->type.elementType = HLSLBaseType_Int;

			DeclareVariable(buffer->cachedName, buffer->type);
		}
				
		statement = buffer;
		m_Buffers.push_back(buffer);
	}	
	else if (HLSLToken_First_Texture_Type <= m_pFullTokenizer->GetToken() && HLSLToken_Last_Texture_Type >= m_pFullTokenizer->GetToken())
	{
		// 00. add new texture to the main tree
		HLSLTextureState* texturestate = m_tree->AddNode<HLSLTextureState>(fileName, line);
		
		// 01. get texture type
		AcceptTextureType(texturestate);

		// 02. get Element type (optional)
		GetTextureElementType(texturestate, false, &typeFlags, true);

		// 03. get Identifier
		if (!ExpectIdentifier(texturestate->cachedName))
			return false;

		// 04. get array (optional)
		GetTextureArray(texturestate);

		// 05. get register (necessary)
		if (!GetRegisterAssignment(texturestate, "Texture (register t for Read only, register u for R/W)"))
			return false;

		m_textureStates.push_back(texturestate);

		statement = texturestate;

	}
	else if (Accept(HLSLToken_Groupshared))
	{
		HLSLGroupShared* pGroupshared = m_tree->AddNode<HLSLGroupShared>(fileName, line);
		pGroupshared->declaration = m_tree->AddNode<HLSLDeclaration>(fileName, GetLineNumber());

		//DataType
		ParseDeclaration(pGroupshared->declaration);

		statement = pGroupshared;

	}
	else if (Accept(HLSLToken_SamplerState) || Accept(HLSLToken_Sampler) || Check(HLSLToken_SamplerComparisonState))
	{
		//SamplerState can be declared or be passed from outside
		HLSLSamplerState* samplerstate = m_tree->AddNode<HLSLSamplerState>(fileName, line);

		if (m_pFullTokenizer->GetToken() == HLSLToken_SamplerComparisonState)
		{
			samplerstate->IsComparisionState = true;
			m_pFullTokenizer->Next();
		}

		// SamplerState declaration.
		CachedString samplerStateName;
		if (!ExpectIdentifier(samplerStateName))
		{
			return false;
		}
		
		samplerstate->cachedName = samplerStateName;

		// Handle array syntax.
		if (Accept('['))
		{
			if (!Accept(']'))
			{
				if (!ParseExpression(samplerstate->type.arraySize,true,0) || !Expect(']'))
				{
					return false;
				}
			}
			samplerstate->type.array = true;
		}

		m_samplerStates.push_back(samplerstate);

		if (Check('{'))
		{
			m_pFullTokenizer->Next();
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

				ASSERT_PARSER(expression != NULL);
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
		}
		else if (!GetRegisterAssignment(samplerstate, "Sampler (register s)"))
		{
			return false;
		}

		statement = samplerstate;
	}
	else if (AcceptType(true, type, typeName, &typeFlags))
	{
		// Global declaration (uniform or function).
		CachedString globalName;
		if (!ExpectIdentifier(globalName))
		{
			return false;
		}

		if (Accept('('))
		{
			// Function declaration.

			HLSLFunction* function = m_tree->AddNode<HLSLFunction>(fileName, line);
			function->name = globalName;
			function->returnType.baseType   = type;
			function->returnType.typeName = typeName;

			BeginScope();

			eastl::vector < HLSLArgument * > argVec;

			if (!ParseArgumentVec(argVec))
			{
				return false;
			}

			function->argumentVec.resize(argVec.size());
			for (int i = 0; i < argVec.size(); i++)
			{
				function->argumentVec[i] = argVec[i];
			}

			const HLSLFunction* declaration = FindFunction(function);

			// Forward declaration
			if (Accept(';'))
			{
				// Add a function entry so that calls can refer to it
				if (!declaration)
				{
					m_functions.push_back( function );
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
				//allow duplicated function
				/*
				if (declaration->forward || declaration->statement)
				{
					m_pFullTokenizer->Error("Duplicate function definition");
					return false;
				}
				*/

				const_cast<HLSLFunction*>(declaration)->forward = function;
			}
			else
			{
				m_functions.push_back( function );
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
			declaration->cachedName = globalName;
			declaration->type.baseType   = type;
			declaration->type.flags      = typeFlags;

			// Handle array syntax.
			if (Accept('['))
			{
				if (!Accept(']'))
				{
					if (!ParseExpression(declaration->type.arraySize,true,0) || !Expect(']'))
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

bool HLSLParser::ParseStatementOrBlock(HLSLStatement*& firstStatement, const HLSLType& returnType, bool bSwitchStatement)
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
		if (bSwitchStatement)
		{
			return ParseSwitchBlocks(firstStatement, returnType);
		}
		else
		{
			return ParseStatement(firstStatement, returnType);
		}
	}
}

bool HLSLParser::ParseSwitchBlocks(HLSLStatement*& firstStatement, const HLSLType& returnType)
{
	HLSLStatement* lastStatement = NULL;
	HLSLStatement* curStatement = NULL;
	//until it reaches to break;	
	do
	{
		if (CheckForUnexpectedEndOfStream('}'))
		{
			return false;
		}

		
		if (!ParseStatement(curStatement, returnType))
		{
			return false;
		}

		if (curStatement != NULL)
		{
			if (firstStatement == NULL)
			{
				firstStatement = curStatement;
			}
			else
			{
				lastStatement->nextStatement = curStatement;
			}
			lastStatement = curStatement;
		}
	} while (curStatement->nodeType != HLSLNodeType_BreakStatement);

	return true;
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

	// Empty statements.
	if (Accept(';'))
	{
		return true;
	}

	FullToken baseToken = m_pFullTokenizer->GetFullToken();

	bool doesNotExpectSemicolon = false;

	HLSLAttribute * attributes = NULL;
	ParseAttributeBlock(attributes);    // @@ Leak if not assigned to node? 

	// If statement.
	if (Accept(HLSLToken_If))
	{
		HLSLIfStatement* ifStatement = m_tree->AddNode<HLSLIfStatement>(fileName, line);
		ifStatement->attributes = attributes;
					
		if (!Expect('(') || !ParseExpression(ifStatement->condition,true,0) || !Expect(')'))
		{
			return false;
		}
		statement = ifStatement;

		if (!ParseStatementOrBlock(ifStatement->statement, returnType, false))
		{
			return false;
		}

		//for (int i = 0; i < 128; i++)
		while (1)
		{
			if (Accept(HLSLToken_ElseIf))
			{
				HLSLIfStatement* elseifStatement = m_tree->AddNode<HLSLIfStatement>(fileName, line);
				elseifStatement->attributes = attributes;

				if (!Expect('(') || !ParseExpression(elseifStatement->condition,true,0) || !Expect(')'))
				{
					return false;
				}

				int index = (int)ifStatement->elseifStatement.size();
				ifStatement->elseifStatement.push_back(elseifStatement);

				bool result = ParseStatementOrBlock(ifStatement->elseifStatement[index]->statement, returnType, false);

				if (!result)
					return false;
			}
			else
				break;
		}
		

 		if (Accept(HLSLToken_Else))
		{
			return ParseStatementOrBlock(ifStatement->elseStatement, returnType, false);
		}
		return true;        
	}

	// Switch statement
	if (Accept(HLSLToken_Switch))
	{
		HLSLSwitchStatement* switchStatement = m_tree->AddNode<HLSLSwitchStatement>(fileName, line);
		switchStatement->attributes = attributes;

		if (!Expect('(') || !ParseExpression(switchStatement->condition,true,0) || !Expect(')'))
		{
			return false;
		}

		statement = switchStatement;
				
		if (!Expect('{'))
			return false;
		// For reference, the original parsing logic is below. It does assumes that "default:" is always last, but it can be anywhere.
		// So we changed the logic to allow default to be intermixed.
		//
		// The other problem is that it does not support fall through. So this case is not supported by the original logic:
		// case 0:
		// case 1:
		//     x = 34;
		// That's because it expects to find an expression after the colon. Instead, after parsing a case and a colon, we need to check
		// the next token. If the next token is "case", "default", or "}" then we skip the expression parsing.
#if 0
		while (Accept(HLSLToken_Case))
		{
			//get case numbering
			ParseExpression(switchStatement->caseNumber[switchStatement->caseCounter]);			

			if (!Expect(':'))
				return false;

			if (!ParseStatementOrBlock(switchStatement->caseStatement[switchStatement->caseCounter], returnType, true))
			{
				return false;
			}

			switchStatement->caseCounter++;
		}

		if (Accept(HLSLToken_Default))
		{
			if (!Expect(':'))
				return false;

			if (!ParseStatementOrBlock(switchStatement->caseDefault, returnType, false))
			{
				return false;
			}
		}
#endif

		bool isCaseFound = false;
		bool isDefaultFound = false;

		do
		{
			isCaseFound = false;
			isDefaultFound = false;

			// check if we have a case, or if not, then a default
			isCaseFound = Accept(HLSLToken_Case);
			if (isCaseFound)
			{

				HLSLExpression* currNumber = NULL;
				HLSLStatement* currStatement = NULL;

				// the comma operator is actually legal here, but I've never seen anyone do it
				ParseExpression(currNumber,true,0);

				if (!Expect(':'))
					return false;

				HLSLToken nextToken = (HLSLToken)m_pFullTokenizer->GetToken();
				if (nextToken == HLSLToken_Case || nextToken == HLSLToken_Default || nextToken == '}')
				{
					// we do not need to parse this expression
				}
				else
				{
					if (!ParseStatementOrBlock(currStatement, returnType, true))
					{
						return false;
					}
				}

				switchStatement->caseNumber.push_back(currNumber);
				switchStatement->caseStatement.push_back(currStatement);
			}
			else
			{
				// check for default
				isDefaultFound = Accept(HLSLToken_Default);
				if (isDefaultFound)
				{
					if (!Expect(':'))
						return false;

					switchStatement->caseDefaultIndex = (int)switchStatement->caseNumber.size();

					HLSLToken nextToken = (HLSLToken)m_pFullTokenizer->GetToken();
					if (nextToken == HLSLToken_Case || nextToken == HLSLToken_Default || nextToken == '}')
					{
						// we do not need to parse this expression
					}
					else
					{
						if (!ParseStatementOrBlock(switchStatement->caseDefault, returnType, false))
						{
							return false;
						}
					}
				}
			}

		}
		while (isCaseFound || isDefaultFound);

		if (!Expect('}'))
			return false;

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

		// Initializer is allowed to be empty.
		if (Accept(';'))
		{
			// empty initializer, so keep going
		}
		else
		{
			if (!ParseDeclaration(forStatement->initialization))
			{
				if (!ParseExpression(forStatement->initializationWithoutDeclaration,true,0))
				{
					return false;
				}
			}
			if (!Expect(';'))
			{
				return false;
			}
		}

		// Allow empty condition
		if (Accept(';'))
		{
			// empty condition, so keep going
		}
		else
		{
			ParseExpression(forStatement->condition,true,0);
			if (!Expect(';'))
			{
				return false;
			}
		}

		// Allow empty iterator
		if (Accept(')'))
		{
			// iterator is empty
		}
		else
		{
			ParseExpression(forStatement->increment,true,0);
			if (!Expect(')'))
			{
				return false;
			}
		}

		statement = forStatement;
		if (!ParseStatementOrBlock(forStatement->statement, returnType, false))
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

		ParseExpression(whileStatement->condition,true,0);		

		EndScope();
		if (!Expect(')'))
		{
			return false;
		}

		statement = whileStatement;
		if (!ParseStatementOrBlock(whileStatement->statement, returnType, false))
		{
			return false;
		}
		
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
		else if (/*!Accept(';') &&*/ !ParseExpression(returnStatement->expression,true,0))
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
	else if (ParseExpression(expression,true,0))
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

	bool modifierDone = false;
	do
	{
		modifierDone = true;
		int startToken = m_pFullTokenizer->GetToken();
		if (startToken == HLSLToken_RowMajor)
		{
			// TODO: add row major flag
			m_pFullTokenizer->Next();
			modifierDone = false;
		}
		else if (startToken == HLSLToken_ColumnMajor)
		{
			// TODO: add column major flag
			m_pFullTokenizer->Next();
			modifierDone = false;
		}

	} while (!modifierDone);

	HLSLType type;
	if (!AcceptType(/*allowVoid=*/false, type.baseType, type.typeName, &type.flags))
	{
		return false;
	}

	bool allowUnsizedArray = true;  // @@ Really?

	HLSLDeclaration * firstDeclaration = NULL;
	HLSLDeclaration * lastDeclaration = NULL;

	do {
		CachedString name;
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
			// the '[' resets priority
			if (!ParseExpression(type.arraySize,true,0) || !Expect(']'))
			{
				return false;
			}
		}

		HLSLDeclaration * declaration = m_tree->AddNode<HLSLDeclaration>(fileName, line);
		declaration->type  = type;
		declaration->cachedName = name;

		DeclareVariable( declaration->cachedName, declaration->type );

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
		else if (Accept('{'))
		{
			// matrix's element initialization syntax.

			int numValues = 0;
			if (!ParseExpressionList('}', true, declaration->assignment, numValues))
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
		else if (!ParseExpression(declaration->assignment,false,0))
		{
			return false;
		}
	}
	return true;
}

bool HLSLParser::ParseFieldDeclaration(HLSLStructField*& field)
{
	field = m_tree->AddNode<HLSLStructField>( GetFileName(), GetLineNumber() );

	bool doesNotExpectSemicolon = false;

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

bool HLSLParser::CheckTypeCast(const HLSLType& srcType, const HLSLType& dstType)
{
	if (GetTypeCastRank(m_tree, srcType, dstType) == -1)
	{
		const char* srcTypeName = GetCstr(GetTypeName(srcType));
		const char* dstTypeName = GetCstr(GetTypeName(dstType));
		m_pFullTokenizer->Error("Cannot implicitly convert from '%s' to '%s'", srcTypeName, dstTypeName);
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
		lvalueType = m_pFullTokenizer->GetToken();
		expression->lvalue = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
		m_pFullTokenizer->Next();
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
			expression->rvalue = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();
		}
	}
	else if (lvalueType == HLSLToken_Filter)
	{
		if (HLSLToken_MIN_MAG_MIP_POINT <= m_pFullTokenizer->GetToken() && m_pFullTokenizer->GetToken() <= HLSLToken_MAXIMUM_ANISOTROPIC)
		{
			expression->rvalue = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();
		}
	}
	else if (lvalueType == HLSLToken_ComparisonFunc)
	{
		if (HLSLToken_NEVER <= m_pFullTokenizer->GetToken() && m_pFullTokenizer->GetToken() <= HLSLToken_ALWAYS)
		{
			expression->rvalue = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();
		}
	}
	
	if (!Expect(';'))
		return false;

	return true;
}

// How does the comma operator work?
// The comma operator is the lowest precedence operator, below even assignment. So we
// should parse the statement:
//     y = x + 3, z *= 9;
// as
//              ,
//       =          *=  
//     y     +    z    9
//         x   3 
//
// We are only allowing the comma operator if the left side of the subexpression
// does not have any operator. As we parse the expression from left to right, if we know
// that there is no existing operator far to the left, we are allowing the comma operator.
// So in this example, 
//     y = x + 3, z *= 9;
// We eventually hit the point where we have this built tree:
//       =      
//     y     +  
//         x   3
// and need to add the remaining tokens: 
//    , z *= 9;
// 
// The 3 would not be allowed to connect to the comma operator, because it has a + above it in the
// heirarchy. We have to allow the comma operator on a case by case basis.
//
// For example, when we parse a statement, at the top level, we will allow a comma. But if we go into
// a binary subexpression then we are no longer allowed to have a comma (because a comma has lower
// precedence than everything in the tree). However, if a new ( ) block opens up, we can allow
// the comma operator again. We also need similar rules with for(), if(), while(), etc blocks,
// but in general the comma operator is disallowed for all other binary expressions.

bool HLSLParser::ParseExpression(HLSLExpression*& expression, bool allowCommaOperator, int binaryPriority)
{
	bool doesNotExpectSemicolon = false;

	if (!ParseBinaryExpression(binaryPriority, expression))
	{
		return false;
	}

	HLSLBinaryOp assignOp;
	if (AcceptAssign(assignOp))
	{
		// anything on the right side is not allowed to have a , operator because it should be lower precedence than the assignment
		// also, the assign resets priority
		HLSLExpression* expression2 = NULL;
		if (!ParseExpression(expression2,false,0))
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

		if (!CheckTypeCast(expression2->expressionType, expression->expressionType))
		{
			const char* srcTypeName = GetCstr(GetTypeName(expression2->expressionType));
			const char* dstTypeName = GetCstr(GetTypeName(expression->expressionType));
			m_pFullTokenizer->Error("Cannot implicitly convert from '%s' to '%s'", srcTypeName, dstTypeName);

			return false;
		}

		expression = binaryExpression;
	}

	if (allowCommaOperator)
	{
		// comma operator, the only operator with lower precedence than assign
		while (Accept(','))
		{
			HLSLExpression* expression2 = NULL;
			if (!ParseExpression(expression2,false,0))
			{
				return false;
			}

			HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(expression->fileName, expression->line);
			binaryExpression->binaryOp = HLSLBinaryOp_Comma;
			binaryExpression->expression1 = expression;
			binaryExpression->expression2 = expression2;
		
			// In the case of the comma operator, the left side result is discarded and the right side result is returned
			binaryExpression->expressionType = expression2->expressionType;

			// Note: No need to check type casts

			// Save the expression
			expression = binaryExpression;
		}
	}

	return true;
}

bool HLSLParser::AcceptBinaryOperator(int priority, HLSLBinaryOp& binaryOp)
{
	int token = m_pFullTokenizer->GetToken();
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
	case HLSLToken_LeftShift:		binaryOp = HLSLBinaryOp_LeftShift;    break;
	case HLSLToken_RightShift:		binaryOp = HLSLBinaryOp_RightShift;   break;
	case HLSLToken_Modular:			binaryOp = HLSLBinaryOp_Modular;	  break;

	default:
		return false;
	}
	if (_binaryOpPriority[binaryOp] > priority)
	{
		m_pFullTokenizer->Next();
		return true;
	}
	return false;
}

bool HLSLParser::AcceptUnaryOperator(bool pre, HLSLUnaryOp& unaryOp)
{
	int token = m_pFullTokenizer->GetToken();
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
		unaryOp = HLSLUnaryOp_BitNot;
	}
	else
	{
		return false;
	}
	m_pFullTokenizer->Next();
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
	else if (Accept(HLSLToken_AndEqual))
	{
		binaryOp = HLSLBinaryOp_BitAndAssign;
	}
	else if (Accept(HLSLToken_BarEqual))
	{
		binaryOp = HLSLBinaryOp_BitOrAssign;
	}
	else if (Accept(HLSLToken_XorEqual))
	{
		binaryOp = HLSLBinaryOp_BitXorAssign;
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

	// so, ParseTerminalExpression is stopping too early??
	if (!ParseTerminalExpression(expression, needsEndParen, false))
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
			ASSERT_PARSER( binaryOp < sizeof(_binaryOpPriority) / sizeof(int) );
			if (!ParseBinaryExpression(_binaryOpPriority[binaryOp], expression2))
			{
				return false;
			}
			HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(fileName, line);
			binaryExpression->binaryOp    = binaryOp;
			binaryExpression->expression1 = expression;
			binaryExpression->expression2 = expression2;

			HLSLType exp1Type = expression->expressionType;
			HLSLType exp2Type = expression2->expressionType;

			if (expression->functionExpression)
			{
				exp1Type = expression->functionExpression->expressionType;
			}
			else if (expression->expressionType.elementType != HLSLBaseType_Unknown)
			{
				exp1Type.baseType = expression->expressionType.elementType;
			}
			else if (expression->expressionType.baseType >= HLSLBaseType_Texture1D && expression->expressionType.baseType <= HLSLBaseType_RWTexture3D)
			{
				exp1Type.baseType = HLSLBaseType_Float4;
			}
			else if (expression->expressionType.baseType == HLSLBaseType_UserMacro)
			{
				exp1Type.baseType = HLSLBaseType_Uint;
			}

			if (expression2->functionExpression)
			{
				exp2Type = expression2->functionExpression->expressionType;
			}
			else if (expression2->expressionType.elementType != HLSLBaseType_Unknown)
			{
				exp2Type.baseType = expression2->expressionType.elementType;
			}
			else if (expression2->expressionType.baseType >= HLSLBaseType_Texture1D && expression2->expressionType.baseType <= HLSLBaseType_RWTexture3D)
			{
				exp2Type.baseType = HLSLBaseType_Float4;
			}
			else if (expression2->expressionType.baseType == HLSLBaseType_UserMacro)
			{
				exp2Type.baseType = HLSLBaseType_Uint;
			}

			if (!GetBinaryOpResultType( binaryOp, exp1Type, exp2Type, binaryExpression->expressionType ))
			{
				const char* typeName1 = GetCstr(GetTypeName( binaryExpression->expression1->expressionType ));
				const char* typeName2 = GetCstr(GetTypeName( binaryExpression->expression2->expressionType ));

				// debug
				bool temp = GetBinaryOpResultType(binaryOp, exp1Type, exp2Type, binaryExpression->expressionType);

				m_pFullTokenizer->Error("binary '%s' : no global operator found which takes types '%s' and '%s' (or there is no acceptable conversion)",
					CHECK_CSTR(GetBinaryOpName(binaryOp)), CHECK_CSTR(typeName1), CHECK_CSTR(typeName2));
			
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
				const char* srcTypeName = GetCstr(GetTypeName(expression2->expressionType));
				const char* dstTypeName = GetCstr(GetTypeName(expression1->expressionType));
				m_pFullTokenizer->Error("':' no possible conversion from from '%s' to '%s'", srcTypeName, dstTypeName);
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

bool HLSLParser::ParsePartialConstructor(HLSLExpression*& expression, HLSLBaseType type, const CachedString & typeName)
{
	HLSLScopedStackTracker stackTracker(this);

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

// If the next token is a . then replace expression with the new swizzle node.
// Otherwise, do nothing. This helper function was added to avoiding duplicating the code
// a bunch of times for literal expressions.
bool HLSLParser::ApplyMemberAccessToNode(HLSLExpression * & expression)
{
	const char* fileName = GetFileName();
	int         line = GetLineNumber();

	if (Accept('.'))
	{
		HLSLMemberAccess* memberAccess = m_tree->AddNode<HLSLMemberAccess>(fileName, line);
		memberAccess->object = expression;
		if (!ExpectIdentifier(memberAccess->field))
		{
			return false;
		}

		if (!GetMemberType(expression->expressionType, memberAccess))
		{
			m_pFullTokenizer->Error("Couldn't access '%s'", GetCstr(memberAccess->field));
			return false;
		}
		expression = memberAccess;
	}

	return true;
}

bool HLSLParser::ParseTerminalExpression(HLSLExpression*& expression, bool& needsEndParen, bool bPreprocessor)
{
	HLSLScopedStackTracker stackTracker(this);

	const char* fileName = GetFileName();
	int         line     = GetLineNumber();

	needsEndParen = false;

	if(m_pFullTokenizer->GetToken() == HLSLToken_Sizeof)
	{
		//accept
		m_pFullTokenizer->Next();

		if (Accept('('))
		{
			HLSLType type;
			AcceptType(false, type.baseType, type.typeName, &type.flags);

			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type = HLSLBaseType_Uint;

			if (type.baseType == HLSLBaseType_Float)
			{
				literalExpression->uiValue = sizeof(float);
			}
			else if (type.baseType == HLSLBaseType_Half)
			{
				literalExpression->uiValue = sizeof(short);
			}
			else if (type.baseType == HLSLBaseType_Min16Float)
			{
				literalExpression->uiValue = sizeof(short);
			}
			else if (type.baseType == HLSLBaseType_Min10Float)
			{
				literalExpression->uiValue = sizeof(short);
			}
			else if (type.baseType == HLSLBaseType_Int)
			{
				literalExpression->uiValue = sizeof(int);
			}
			else if (type.baseType == HLSLBaseType_Uint)
			{
				
				literalExpression->uiValue = sizeof(unsigned int);
			}

			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;

			if (!Accept(')'))
			{
				return false;
			}

			return true;
		}
		else
			return false;
	}

	HLSLUnaryOp unaryOp;
	if (AcceptUnaryOperator(true, unaryOp))
	{
		HLSLUnaryExpression* unaryExpression = m_tree->AddNode<HLSLUnaryExpression>(fileName, line);
		unaryExpression->unaryOp = unaryOp;
		if (!ParseTerminalExpression(unaryExpression->expression, needsEndParen, false))
		{
			return false;
		}
		if (unaryOp == HLSLUnaryOp_BitNot)
		{
			if (unaryExpression->expression->expressionType.baseType < HLSLBaseType_FirstInteger || 
				unaryExpression->expression->expressionType.baseType > HLSLBaseType_LastInteger)
			{
				const char * typeName = GetCstr(GetTypeName(unaryExpression->expression->expressionType));
				m_pFullTokenizer->Error("unary '~' : no global operator found which takes type '%s' (or there is no acceptable conversion)", typeName);
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
		// original logic
#if 0
		// Check for a casting operator.
		
		HLSLType type;
		if (AcceptType(false, type.baseType, type.typeName, &type.flags))
		{
			//if(!FindPreprocessorDefinedType(type.typeName))
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
			//else
			//{
			//	
			//	m_pFullTokenizer->Undo();
			//}
		
		}
		
		if (!ParseExpression(expression) || !Expect(')'))
		{
			return false;
		}
#endif

		// The original logic is incorrect because it fails on this line:
		// float saSample = 1.2 / (float(SampleCount) * pdf + 0.0001);
		// 
		// The correct heirarchy that it should be making looks like this:
		// float saSample = 
		//                      /
		//                  1.2                             +
		//                        (                   *       0.0001);
		//                         float(SampleCount)   pdf
		// In that example, it started creating the following heirarchy:
		//
		// float saSample = 
		//                      /
		//                  1.2                       *              ;
		//                        (float(SampleCount)   pdf + 0.0001);
		//                                                 ^
		//                                                ERROR
		// because (float(SampleCount) was a single unit, it expected the right hand expression
		// of the '*' to end after "pdf", which caused an error because the parser
		// is expecting a ')', not a '+'.
		//
		// After accepting the initial '(' above, the initial logic assumes
		// that the next level of should be a type constructor "float(SampleCount)",
		// but it should actually be trying to parse a binary expression, which causes the error.
		//
		// The original logic is:
		// 1) If it is a '(', type "float", ')', return a HLSLCastingExpression
		// 2) If it is a '(', type "float", '(', return the float using ParsePartialConstructor
		// 3) If it is a '(', and not a type, then ParseExpression and Expect(')')
		//
		// But the correct logic is:
		// 1) If it is a '(', type "float", ')', return a HLSLCastingExpression     ***SAME***
		// 2) If it is a '(', type "float", and not a ')', then undo the "float", and
		//    follow the same logic as #3.
		// 3) If it is a '(', and not a type, then ParseExpression and Expect(')')  ***SAME***

		// Check for a casting operator.

		HLSLType type;
		if (AcceptType(false, type.baseType, type.typeName, &type.flags))
		{
			// This is actually a type constructor like (float2(...
			// This logic is changed from above.
			if (Accept(')'))
			{
				// Case 1.
				HLSLCastingExpression* castingExpression = m_tree->AddNode<HLSLCastingExpression>(fileName, line);
				castingExpression->type = type;
				expression = castingExpression;
				castingExpression->expressionType = type;

				// This was the other mistake in the original parser: ParseExpression would reset priority. The specific case
				// we ran into is this line:
				//
				// (float)indices.localVertexIndex < GlobalConstraintRange * (float)NumVerticesPerStrand
				//
				// Since ParseExpression() in the original parser reset the priority, any following binary expressions would
				// be under the casting operator. So that line was parsed like this:
				//
				// (float)
				//                                 <
				//        indices.localVertexIndex                         *
				//                                   GlobalConstraintRange   (float)
				//                                                                  NumVerticesPerStrand
				//
				// that's incorrect because indices.localVertexIndex was not casted, and the binary result
				// of the < was casted to a float. The fix is to set a high priority after casting expressions
				// so that the result is parsed as:
				//
				//                                 <
				// (float)                                                 *
				//         indices.localVertexIndex  GlobalConstraintRange   (float)
				//                                                                  NumVerticesPerStrand

				//
				// And that's why we need the 11, so that the priority is high enough to disallow binary expressions.

				// the cast has higher priority than every binary expression, so a priority of 11 should stop it from
				// parsing any binary expressions
				return ParseExpression(castingExpression->expression,false,11);
			}
			else
			{
				// Case 2: Accepted a '(', accepted a type "float", but REJECTED a ')',
				//         so Undo() the "float", and fall to case 3.
				m_pFullTokenizer->Undo();
			}
		}

		// Case 3: Accepted a '(', so ParseExpression() and Expect(')')
		// Also, since we are inside a ( ) block and thus have to operator on the left,
		// we are allowed to use a comma operator.
		if (!ParseExpression(expression,true,0) || !Expect(')'))
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
			return ApplyMemberAccessToNode(expression);
		}
		else if( AcceptHalf( fValue ) )
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>( fileName, line );
			literalExpression->type = HLSLBaseType_Half;
			literalExpression->fValue = fValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return ApplyMemberAccessToNode(expression);
		}
		else if (AcceptMin16Float(fValue))
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type = HLSLBaseType_Min16Float;
			literalExpression->fValue = fValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return ApplyMemberAccessToNode(expression);
		}
		else if (AcceptMin10Float(fValue))
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type = HLSLBaseType_Min10Float;
			literalExpression->fValue = fValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return ApplyMemberAccessToNode(expression);
		}
		else if (AcceptInt(iValue))
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type   = HLSLBaseType_Int;
			literalExpression->iValue = iValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return ApplyMemberAccessToNode(expression);
		}
		else if (AcceptUint(uiValue))
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type = HLSLBaseType_Uint;
			literalExpression->uiValue = uiValue;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return ApplyMemberAccessToNode(expression);
		}
		else if (Accept(HLSLToken_True))
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type   = HLSLBaseType_Bool;
			literalExpression->bValue = true;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return ApplyMemberAccessToNode(expression);
		}
		else if (Accept(HLSLToken_False))
		{
			HLSLLiteralExpression* literalExpression = m_tree->AddNode<HLSLLiteralExpression>(fileName, line);
			literalExpression->type   = HLSLBaseType_Bool;
			literalExpression->bValue = false;
			literalExpression->expressionType.baseType = literalExpression->type;
			literalExpression->expressionType.flags = HLSLTypeFlag_Const;
			expression = literalExpression;
			return ApplyMemberAccessToNode(expression);
		}

		HLSLScopedStackTracker stackTracker(this);

		CachedString baseIdentifierName = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());

		// Type constructor.
		HLSLBaseType    type;
		CachedString    typeName;
		if (AcceptType(/*allowVoid=*/false, type, typeName, NULL) && expression == NULL)
		{
			Expect('(');
			if (!ParsePartialConstructor(expression, type, typeName))
			{
				return false;
			}
		}	
		else if (const HLSLTextureState* pTS = FindTextureStateDefinedType(baseIdentifierName))
		{
			m_parentStackIdentifiers.push_back(baseIdentifierName);

			if (String_Equal(m_pFullTokenizer->GetIdentifier(), pTS->cachedName))
			{
				HLSLTextureStateExpression* textureStateExpression = m_tree->AddNode<HLSLTextureStateExpression>(fileName, line);

				textureStateExpression->name = pTS->cachedName;

				textureStateExpression->expressionType = pTS->type;


				m_textureStateExpressions.push_back(textureStateExpression);

				m_pFullTokenizer->Next();

				textureStateExpression->arrayDimension = pTS->arrayDimension;

				//handle Array
				for (int index = 0; index < (int)pTS->arrayDimension ; index++)
				{
					textureStateExpression->bArray = true;

					if (Accept('['))
					{
						HLSLScopedStackTracker stackTracker(this);

						int innerIndex = 0;
						while (!Accept(']'))
						{
							// after parsing each expression, pop them off the stack
							HLSLScopedStackTracker stackTracker(this);

							// inside we have no precedence, so comma is allowed
							if (!ParseExpression(textureStateExpression->arrayExpression,true,0))
								return false;								
						}
					}
				}

				//handle [] operator
				if(textureStateExpression->expressionType.baseType >= HLSLBaseType_Texture1D &&
					textureStateExpression->expressionType.baseType <= HLSLBaseType_RWTexture3D &&
					Accept('[')
					)
				{
					HLSLScopedStackTracker stackTracker(this);

					if (!ParseExpression(textureStateExpression->indexExpression,true,0))
						return false;

					if (!Accept(']'))
						return false;

					m_tree->gExtension[USE_SAMPLESS] = true;
				}
						
				if (Accept('.'))
				{
					if (String_Equal(m_pFullTokenizer->GetIdentifier(), "Sample") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "SampleLevel") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "SampleGrad") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "SampleCmp") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "SampleCmpLevelZero") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "Load") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "Store") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "SampleBias") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "GatherRed") ||
						String_Equal(m_pFullTokenizer->GetIdentifier(), "GetDimensions")							
						)
					{
						HLSLScopedStackTracker stackTracker(this);

						ParseExpression(textureStateExpression->functionExpression,false,0);

						if (textureStateExpression->functionExpression)
						{
							expression = textureStateExpression;
						}
					}
					else
					{
						// if it is failed, it's swizzling
						HLSLMemberAccess* memberAccess = m_tree->AddNode<HLSLMemberAccess>(fileName, line);

						memberAccess->object = textureStateExpression;
						textureStateExpression->memberAccessExpression = memberAccess;

						if (!ExpectIdentifier(memberAccess->field))
						{
							return false;
						}

						HLSLType type;
						type.typeName = textureStateExpression->expressionType.typeName; //  currentElementType;
						type.baseType = HLSLBaseType_Float4; //  HLSLBaseType_UserDefined;

						if (!GetMemberType(type, memberAccess))
						{
							m_pFullTokenizer->Error("Couldn't access '%s'", GetCstr(memberAccess->field));
							return false;
						}

						expression = memberAccess;
					}

					return true;
				}
				else
				{
					// not it can be just argument
					//return false;

					textureStateExpression->expressionType.baseType = pTS->type.baseType;

					expression = textureStateExpression;
					return true;
				}
			}
		}
		else if (FindSamplerStateDefinedType(baseIdentifierName) != NULL)
		{
			m_parentStackIdentifiers.push_back(baseIdentifierName);

			//if it is SamplerState's m_identifier
			for (int i = 0; i < m_samplerStates.size(); ++i)
			{
				if (String_Equal(m_pFullTokenizer->GetIdentifier(), m_samplerStates[i]->cachedName))
				{
					HLSLSamplerStateExpression* samplerStateExpression = m_tree->AddNode<HLSLSamplerStateExpression>(fileName, line);
				
					samplerStateExpression->name = m_samplerStates[i]->cachedName;

					samplerStateExpression->expressionType.baseType = HLSLBaseType_SamplerState;
					samplerStateExpression->expressionType.flags = HLSLTypeFlag_Const;
					expression = samplerStateExpression;
					
					m_pFullTokenizer->Next();

					if (m_samplerStates[i]->type.array)
					{
						if (Accept('['))
						{
							HLSLArrayAccess* arrayAccess = m_tree->AddNode<HLSLArrayAccess>(fileName, line);
							arrayAccess->array = samplerStateExpression;
							if (!ParseExpression(arrayAccess->index,true,0) || !Expect(']'))
							{
								return false;
							}
							
							samplerStateExpression->expressionType.array = true;

							arrayAccess->expressionType.baseType = HLSLBaseType_SamplerState;
							arrayAccess->expressionType.flags = HLSLTypeFlag_Const;

							expression = arrayAccess;
						}

					}

					return true;
				}
			}
		}
		else if (FindConstantBuffer(baseIdentifierName) != NULL)
		{
			CachedString currIdentifier = baseIdentifierName;

			const HLSLBuffer* pBuffer = FindBuffer(currIdentifier);

			//if it is push_constant
			if (pBuffer->bPushConstant || pBuffer->type.elementType != HLSLBaseType_Unknown)
			{
				m_pFullTokenizer->Next();

				HLSLIdentifierExpression* identifierExpression = m_tree->AddNode<HLSLIdentifierExpression>(fileName, line);
				identifierExpression->nodeType = HLSLNodeType_IdentifierExpression;
				identifierExpression->name = pBuffer->cachedName;

				identifierExpression->expressionType.typeName = GetTypeName(pBuffer->type);

				identifierExpression->expressionType.baseType = pBuffer->type.elementType;

				HLSLExpression* currentExpression = identifierExpression;
				while (Check('[') || Check('.'))
				{
					//handle [] operator (read / write)
					if (Accept('['))
					{
						HLSLArrayAccess* arrayAccess = m_tree->AddNode<HLSLArrayAccess>(fileName, line);
						arrayAccess->array = currentExpression;
						if (!ParseExpression(arrayAccess->index,true,0) || !Expect(']'))
						{
							return false;
						}

						if (currentExpression->expressionType.array)
						{
							arrayAccess->expressionType = currentExpression->expressionType;
							arrayAccess->expressionType.array = false;
							arrayAccess->expressionType.arraySize = NULL;
						}

						arrayAccess->expressionType.baseType = currentExpression->expressionType.baseType;
						currentExpression->nextExpression = arrayAccess;

						currentExpression = currentExpression->nextExpression;
					}

					// Member access operator.
					if (Accept('.'))
					{
						HLSLMemberAccess* memberAccess = m_tree->AddNode<HLSLMemberAccess>(fileName, line);

						memberAccess->object = currentExpression;

						if (!ExpectIdentifier(memberAccess->field))
						{
							return false;
						}

						HLSLType type;
						type.typeName = currentExpression->expressionType.typeName; //  currentElementType;
						type.baseType = currentExpression->expressionType.baseType; //  HLSLBaseType_UserDefined;

						if (!GetMemberType(type, memberAccess))
						{
							m_pFullTokenizer->Error("Couldn't access '%s'", GetCstr(memberAccess->field));
							return false;
						}

						currentExpression->nextExpression = memberAccess;
						currentExpression = currentExpression->nextExpression;
					}

				}

				expression = currentExpression;
				return true;
			}
			else
				return false;
		}
		else if(expression == NULL)
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
					m_pFullTokenizer->Error("Undeclared identifier '%s'", GetCstr(identifierExpression->name));
					return false;
				}
				// Functions are always global scope.
				identifierExpression->global = true;
			}

			expression = identifierExpression;
		}
	}

	if (bPreprocessor)
		return true;

	// if we have a parent identifier, add it to the stack
	{
		CachedString parentIdentifier;
		if (expression->nodeType == HLSLNodeType_IdentifierExpression)
		{
			HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
			parentIdentifier = identifierExpression->name;
		}
		m_parentStackIdentifiers.push_back(parentIdentifier);
	}


	bool done = false;
	while (!done)
	{
		HLSLScopedStackTracker stackTracker(this);

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
		// or it can be for function calling
		while (Accept('.'))
		{
			HLSLScopedStackTracker stackTracker(this);

			//if it is special function for buffer or texture, skip it
			if (String_Equal("Load", m_pFullTokenizer->GetIdentifier()) || 
				String_Equal(m_pFullTokenizer->GetIdentifier(), "Store") ||
				String_Equal("Sample", m_pFullTokenizer->GetIdentifier()) ||
				String_Equal("SampleBias", m_pFullTokenizer->GetIdentifier()) ||
				String_Equal("SampleLevel", m_pFullTokenizer->GetIdentifier()) ||
				String_Equal(m_pFullTokenizer->GetIdentifier(), "SampleCmp") ||
				String_Equal(m_pFullTokenizer->GetIdentifier(), "SampleCmpLevelZero") ||
				String_Equal("SampleGrad", m_pFullTokenizer->GetIdentifier()) ||
				String_Equal("GatherRed", m_pFullTokenizer->GetIdentifier()) ||
				String_Equal(m_pFullTokenizer->GetIdentifier(), "GetDimensions")
				)
			{
				bool bBreak = false;

				HLSLBuffer* pSBuffer = NULL;
				const HLSLTextureState* pTexture = NULL;
				CachedString foundStr = FindParentTextureOrBuffer(pTexture,pSBuffer);

				bBreak = (pTexture != NULL || pSBuffer != NULL);

				if (bBreak)
				{
					ParseTerminalExpression(expression->functionExpression, needsEndParen, false);
					break;
				}
			}
			else
			{
				HLSLMemberAccess* memberAccess = m_tree->AddNode<HLSLMemberAccess>(fileName, line);
				memberAccess->object = expression;
				if (!ExpectIdentifier(memberAccess->field))
				{
					return false;
				}

				if (!GetMemberType(expression->expressionType, memberAccess))
				{
					m_pFullTokenizer->Error("Couldn't access '%s'", GetCstr(memberAccess->field));
					return false;
				}
				expression = memberAccess;
				
			}

			done = false;

		}

		// Handle array access.
		while (Accept('['))
		{
			HLSLScopedStackTracker stackTracker(this);

			CachedString parentIdentifier;
			if (expression->nodeType == HLSLNodeType_IdentifierExpression)
			{
				HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
				parentIdentifier = identifierExpression->name;
			}

			m_parentStackIdentifiers.push_back(parentIdentifier);

			HLSLArrayAccess* arrayAccess = m_tree->AddNode<HLSLArrayAccess>(fileName, line);
			arrayAccess->array = expression;
			// allow comma inside array access
			if (!ParseExpression(arrayAccess->index,true,0) || !Expect(']'))
			{
				return false;
			}

			if (expression->expressionType.array)
			{
				if (expression->nodeType == HLSLNodeType_IdentifierExpression)
				{
					HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);

					//if it is buffer
					const HLSLBuffer* buffer = FindBuffer(identifierExpression->name);

					if (buffer != NULL && buffer->arrayDimension > 0)
					{
						arrayAccess->identifier = identifierExpression->name;
					}	
				}
					

				arrayAccess->expressionType = expression->expressionType;
				arrayAccess->expressionType.array = false;
				arrayAccess->expressionType.arraySize = NULL;

				
			}
			else
			{
				switch (expression->expressionType.baseType)
				{
				case HLSLBaseType_Float:
				case HLSLBaseType_Float2:
				case HLSLBaseType_Float3:
				case HLSLBaseType_Float4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Float;
					break;
				case HLSLBaseType_Float2x2:
				case HLSLBaseType_Float3x2:
				case HLSLBaseType_Float4x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Float2;
					break;
				case HLSLBaseType_Float2x3:
				case HLSLBaseType_Float3x3:
				case HLSLBaseType_Float4x3:
					arrayAccess->expressionType.baseType = HLSLBaseType_Float3;
					break;
				case HLSLBaseType_Float2x4:
				case HLSLBaseType_Float3x4:
				case HLSLBaseType_Float4x4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Float4;
					break;

				case HLSLBaseType_Half:
				case HLSLBaseType_Half2:
				case HLSLBaseType_Half3:
				case HLSLBaseType_Half4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Half;
					break;
				case HLSLBaseType_Half2x2:
				case HLSLBaseType_Half3x2:
				case HLSLBaseType_Half4x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Half2;
					break;
				case HLSLBaseType_Half2x3:
				case HLSLBaseType_Half3x3:
				case HLSLBaseType_Half4x3:
					arrayAccess->expressionType.baseType = HLSLBaseType_Half3;
					break;
				case HLSLBaseType_Half2x4:
				case HLSLBaseType_Half3x4:
				case HLSLBaseType_Half4x4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Half4;
					break;

				case HLSLBaseType_Min16Float:
				case HLSLBaseType_Min16Float2:
				case HLSLBaseType_Min16Float3:
				case HLSLBaseType_Min16Float4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min16Float;
					break;
				case HLSLBaseType_Min16Float2x2:
				case HLSLBaseType_Min16Float3x2:
				case HLSLBaseType_Min16Float4x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min16Float2;
					break;
				case HLSLBaseType_Min16Float2x3:
				case HLSLBaseType_Min16Float3x3:
				case HLSLBaseType_Min16Float4x3:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min16Float3;
					break;
				case HLSLBaseType_Min16Float2x4:
				case HLSLBaseType_Min16Float3x4:
				case HLSLBaseType_Min16Float4x4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min16Float4;
					break;

				case HLSLBaseType_Min10Float:
				case HLSLBaseType_Min10Float2:
				case HLSLBaseType_Min10Float3:
				case HLSLBaseType_Min10Float4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min10Float;
					break;
				case HLSLBaseType_Min10Float2x2:
				case HLSLBaseType_Min10Float3x2:
				case HLSLBaseType_Min10Float4x2:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min10Float2;
					break;
				case HLSLBaseType_Min10Float2x3:
				case HLSLBaseType_Min10Float3x3:
				case HLSLBaseType_Min10Float4x3:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min10Float3;
					break;
				case HLSLBaseType_Min10Float2x4:
				case HLSLBaseType_Min10Float3x4:
				case HLSLBaseType_Min10Float4x4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Min10Float4;
					break;

				case HLSLBaseType_Int:
				case HLSLBaseType_Int2:
				case HLSLBaseType_Int3:
				case HLSLBaseType_Int4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Int;
					break;

				case HLSLBaseType_Uint:
				case HLSLBaseType_Uint2:
				case HLSLBaseType_Uint3:
				case HLSLBaseType_Uint4:
					arrayAccess->expressionType.baseType = HLSLBaseType_Uint;
					break;
				case HLSLBaseType_UserDefined:
					arrayAccess->expressionType.baseType = HLSLBaseType_UserDefined;
					arrayAccess->expressionType.typeName = expression->expressionType.typeName;
					break;

				case HLSLBaseType_RWStructuredBuffer:
				case HLSLBaseType_StructuredBuffer:
				case HLSLBaseType_RWBuffer:
				case HLSLBaseType_PureBuffer:
					// For these buffers, we should treat them as arrays.
					// I.e. if we declare:
					// RWStructuredBuffer<float4> HairVertexPositionsPrev;
					// ...
					// HairVertexPositionsPrev[indices.globalVertexIndex] = currentPosition;
					// 
					// The parsed structure of:
					//                          [] 
					// HairVertexPositionsPrev       indices.globalVertexIndex
					// Should be the original type (float4), not an index inside that type (float)

					//need to clean up here
					switch (expression->expressionType.elementType)
					{
					case HLSLBaseType_Float:
					case HLSLBaseType_Float2:
					case HLSLBaseType_Float3:
					case HLSLBaseType_Float4:
					case HLSLBaseType_Float2x2:
					case HLSLBaseType_Float3x2:
					case HLSLBaseType_Float4x2:
					case HLSLBaseType_Float2x3:
					case HLSLBaseType_Float3x3:
					case HLSLBaseType_Float4x3:
					case HLSLBaseType_Float2x4:
					case HLSLBaseType_Float3x4:
					case HLSLBaseType_Float4x4:
					case HLSLBaseType_Half:
					case HLSLBaseType_Half2:
					case HLSLBaseType_Half3:
					case HLSLBaseType_Half4:
					case HLSLBaseType_Half2x2:
					case HLSLBaseType_Half3x2:
					case HLSLBaseType_Half4x2:
					case HLSLBaseType_Half2x3:
					case HLSLBaseType_Half3x3:
					case HLSLBaseType_Half4x3:
					case HLSLBaseType_Half2x4:
					case HLSLBaseType_Half3x4:
					case HLSLBaseType_Half4x4:

					case HLSLBaseType_Min16Float:
					case HLSLBaseType_Min16Float2:
					case HLSLBaseType_Min16Float3:
					case HLSLBaseType_Min16Float4:
					case HLSLBaseType_Min16Float2x2:
					case HLSLBaseType_Min16Float3x2:
					case HLSLBaseType_Min16Float4x2:
					case HLSLBaseType_Min16Float2x3:
					case HLSLBaseType_Min16Float3x3:
					case HLSLBaseType_Min16Float4x3:
					case HLSLBaseType_Min16Float2x4:
					case HLSLBaseType_Min16Float3x4:
					case HLSLBaseType_Min16Float4x4:

					case HLSLBaseType_Min10Float:
					case HLSLBaseType_Min10Float2:
					case HLSLBaseType_Min10Float3:
					case HLSLBaseType_Min10Float4:
					case HLSLBaseType_Min10Float2x2:
					case HLSLBaseType_Min10Float3x2:
					case HLSLBaseType_Min10Float4x2:
					case HLSLBaseType_Min10Float2x3:
					case HLSLBaseType_Min10Float3x3:
					case HLSLBaseType_Min10Float4x3:
					case HLSLBaseType_Min10Float2x4:
					case HLSLBaseType_Min10Float3x4:
					case HLSLBaseType_Min10Float4x4:

					case HLSLBaseType_Int:
					case HLSLBaseType_Int2:
					case HLSLBaseType_Int3:
					case HLSLBaseType_Int4:
					case HLSLBaseType_Uint:
					case HLSLBaseType_Uint2:
					case HLSLBaseType_Uint3:
					case HLSLBaseType_Uint4:
						arrayAccess->expressionType.elementType = expression->expressionType.elementType;
						break;
					case HLSLBaseType_UserDefined:
						arrayAccess->expressionType.elementType = HLSLBaseType_UserDefined;
						arrayAccess->expressionType.typeName = expression->expressionType.typeName;
						break;
					default:
						m_pFullTokenizer->Error("illegal type inside buffer.");
						return false;
					}
					break;

				case HLSLBaseType_ConstantBuffer:
					// Indexing a constant buffer is not legal.
					m_pFullTokenizer->Error("operator [] is illegal on ConstantBuffer or PureBuffer.");
					return false;

				default:
					m_pFullTokenizer->Error("array, matrix, vector, or indexable object type expected in index expression");
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

			if (!ParseExpressionList(')', false, functionCall->callArgument, functionCall->numArguments))
			{
				return false;
			}

			eastl::vector < HLSLType > foundTypes;
			HLSLExpression* currArg = functionCall->callArgument;
			while (currArg != NULL)
			{
				foundTypes.push_back(currArg->expressionType);
				currArg = currArg->nextExpression;
			}

			if (expression->nodeType != HLSLNodeType_IdentifierExpression)
			{
				m_pFullTokenizer->Error("Expected function identifier");
				return false;
			}

			const HLSLIdentifierExpression* identifierExpression = static_cast<const HLSLIdentifierExpression*>(expression);
			const HLSLFunction* function = MatchFunctionCall( functionCall, identifierExpression->name);
			if (function == NULL)
			{
				return false;
			}

			functionCall->function = function;

			// if it is special function for texture / buffer
			if (String_Equal("Sample", identifierExpression->name) ||
				String_Equal("SampleBias", identifierExpression->name) ||
				String_Equal("SampleLevel", identifierExpression->name) ||
				String_Equal(identifierExpression->name, "SampleCmp") ||
				String_Equal(identifierExpression->name, "SampleCmpLevelZero") ||
				String_Equal("SampleGrad", identifierExpression->name) ||
				String_Equal("GatherRed", identifierExpression->name))
			{
				functionCall->pTextureStateExpression = m_textureStateExpressions[m_textureStateExpressions.size()-1];
				
			}
			else if (String_Equal("Load", identifierExpression->name) ||
					String_Equal(identifierExpression->name, "Store") ||
					String_Equal(identifierExpression->name, "GetDimensions"))
			{
				HLSLBuffer* pSBuffer = NULL;
				const HLSLTextureState* pTexture = NULL;
				CachedString foundStr = FindParentTextureOrBuffer(pTexture, pSBuffer);

				if (pTexture != NULL)
				{
					functionCall->pTextureStateExpression = m_textureStateExpressions[m_textureStateExpressions.size() - 1];
					m_tree->gExtension[USE_SAMPLESS] = true;
				}
				else if (pSBuffer != NULL)
				{
					//does buffer need to handle?
					functionCall->pBuffer = pSBuffer;
				}
				else
				{
					// if we don't have a texture of buffer above us in the heirarchy, then something is wrong
					ASSERT_PARSER(0);
				}
			}
			//marking for MSL
			else if (String_Equal("InterlockedAdd", identifierExpression->name) ||
				String_Equal("InterlockedCompareExchange", identifierExpression->name) ||
				String_Equal("InterlockedCompareStore", identifierExpression->name) ||
				String_Equal("InterlockedExchange", identifierExpression->name) ||
				String_Equal("InterlockedMax", identifierExpression->name) ||
				String_Equal("InterlockedMin", identifierExpression->name) ||
				String_Equal("InterlockedOr", identifierExpression->name) ||
				String_Equal("InterlockedXor", identifierExpression->name))
			{
				m_tree->gExtension[USE_ATOMIC] = true;

				for (int i = m_pFullTokenizer->GetHistoryCounter(); i >= 0; i--)
				{
					CachedString prevIndentifier = m_tree->AddStringCached(m_pFullTokenizer->GetPrevIdentifier(i));

					for (int index = 0; index < m_userTypes.size(); ++index)
					{
						HLSLStructField* field = m_userTypes[index]->field;

						while (field)
						{
							if (String_Equal(field->name, prevIndentifier))
							{
								field->atomic = true;
								break;
							}
							field = field->nextField;
						}
					}

					HLSLBuffer* pBuffer = FindBuffer(prevIndentifier);

					if (pBuffer)
					{
						pBuffer->bAtomic = true;
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

		if (Check('{'))
		{	
			expression = m_tree->AddNode<HLSLExpression>(GetFileName(), GetLineNumber());

			if (Accept('{'))
			{
				int numExpressions;

				//inner array
				HLSLExpression* childExpression = NULL;
				ParseExpressionList('}', false, childExpression, numExpressions);

				expression->childExpression = childExpression;
			}

		}
		else if (!ParseExpression(expression,false,0))
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

bool HLSLParser::ParseArgumentVec(eastl::vector< HLSLArgument* > & argVec)
{
	const char* fileName = GetFileName();
	int         line     = GetLineNumber();
		

	argVec.clear();

	while (!Accept(')'))
	{
		if (CheckForUnexpectedEndOfStream(')'))
		{
			return false;
		}
		if (argVec.size() > 0 && !Expect(','))
		{
			return false;
		}

		HLSLArgument* argument = m_tree->AddNode<HLSLArgument>(fileName, line);


		{
			if (Accept(HLSLToken_Uniform))
			{
				argument->modifier = HLSLArgumentModifier_Uniform;
			}
			else if (Accept(HLSLToken_In))
			{
				argument->modifier = HLSLArgumentModifier_In;
			}
			else if (Accept(HLSLToken_Out))
			{
				argument->modifier = HLSLArgumentModifier_Out;
			}
			else if (Accept(HLSLToken_InOut))
			{
				argument->modifier = HLSLArgumentModifier_Inout;
			}
			else if (Accept(HLSLToken_Const))
			{
				argument->modifier = HLSLArgumentModifier_Const;
			}
			else if (Accept(HLSLToken_Point))
			{ 
				argument->modifier = HLSLArgumentModifier_Point;
			}
			else if (Accept(HLSLToken_Line))
			{
				argument->modifier = HLSLArgumentModifier_Line;
			}
			else if (Accept(HLSLToken_Triangle))
			{
				argument->modifier = HLSLArgumentModifier_Triangle;
			}
			else if (Accept(HLSLToken_Lineadj))
			{
				argument->modifier = HLSLArgumentModifier_Lineadj;
			}
			else if (Accept(HLSLToken_Triangleadj))
			{
				argument->modifier = HLSLArgumentModifier_Triangleadj;
			}

			if (!ExpectDeclaration(/*allowUnsizedArray=*/true, argument->argType, argument->name))
			{
				return false;
			}

			DeclareVariable(argument->name, argument->argType);

			if (argument->argType.baseType >= HLSLBaseType_Texture1D && argument->argType.baseType <= HLSLBaseType_RWTexture3D)
			{
				HLSLTextureState* texturestate = m_tree->AddNode<HLSLTextureState>(fileName, line);
				texturestate->type.baseType = argument->argType.baseType;

				texturestate->cachedName = argument->name;

				m_textureStates.push_back(texturestate);

				
				HLSLTextureStateExpression* textureStateExpression = m_tree->AddNode<HLSLTextureStateExpression>(fileName, line);

				textureStateExpression->name = texturestate->cachedName;
				textureStateExpression->expressionType = texturestate->type;

				m_textureStateExpressions.push_back(textureStateExpression);
			}


			// Optional semantic.
			if (Accept(':') && !ExpectIdentifier(argument->semantic))
			{
				return false;
			}

			if (Accept('=') && !ParseExpression(argument->defaultValue,false,0))
			{
				// @@ Print error!
				return false;
			}
		}

		argVec.push_back(argument);
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
		ASSERT_PARSER(stateAssignment != NULL);
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

	CachedString techniqueName;
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
		ASSERT_PARSER(pass != NULL);
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
	CachedString passName;
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
		ASSERT_PARSER(stateAssignment != NULL);
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
	CachedString pipelineName;
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
		ASSERT_PARSER(stateAssignment != NULL);
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


bool HLSLParser::ParseStateName(bool isSamplerState, bool isPipelineState, CachedString & name, const EffectState *& state)
{
	if (m_pFullTokenizer->GetToken() != HLSLToken_Identifier)
	{
		char nearToken[HLSLTokenizer::s_maxIdentifier];
		m_pFullTokenizer->GetTokenName(nearToken);
		m_pFullTokenizer->Error("Syntax error: expected identifier near '%s'", CHECK_CSTR(nearToken));
		return false;
	}

	state = GetEffectState(m_pFullTokenizer->GetIdentifier(), isSamplerState, isPipelineState);
	if (state == NULL)
	{
		m_pFullTokenizer->Error("Syntax error: unexpected identifier '%s'", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()));
		return false;
	}

	m_pFullTokenizer->Next();
	return true;
}

bool HLSLParser::ParseColorMask(int& mask)
{
	mask = 0;

	do {
		if (m_pFullTokenizer->GetToken() == HLSLToken_IntLiteral) {
			mask |= m_pFullTokenizer->GetInt();
		}
		else if (m_pFullTokenizer->GetToken() == HLSLToken_UintLiteral) {
			mask |= m_pFullTokenizer->GetuInt();
		}
		else if (m_pFullTokenizer->GetToken() == HLSLToken_Identifier) {
			const char * ident = m_pFullTokenizer->GetIdentifier();
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
		m_pFullTokenizer->Next();
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
		if (m_pFullTokenizer->GetToken() != HLSLToken_Identifier)
		{
			char nearName[HLSLTokenizer::s_maxIdentifier];
			m_pFullTokenizer->GetTokenName(nearName);
			m_pFullTokenizer->Error("Syntax error: expected identifier near '%s'", CHECK_CSTR(nearName));
			stateAssignment->iValue = 0;
			return false;
		}
	}

	if (state->values == NULL)
	{
		if (String_Equal(m_pFullTokenizer->GetIdentifier(), "compile"))
		{
			m_pFullTokenizer->Error("Syntax error: unexpected identifier '%s' expected compile statement", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()));
			stateAssignment->iValue = 0;
			return false;
		}

		// @@ Parse profile name, function name, argument expressions.

		// Skip the rest of the compile statement.
		while(m_pFullTokenizer->GetToken() != ';')
		{
			m_pFullTokenizer->Next();
		}
	}
	else {
		if (expectsInteger)
		{
			if (!AcceptInt(stateAssignment->iValue))
			{
				m_pFullTokenizer->Error("Syntax error: expected integer near '%s'", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()));
				stateAssignment->iValue = 0;
				return false;
			}
		}
		if (expectsUInteger)
		{
			if (!AcceptUint(stateAssignment->uiValue))
			{
				m_pFullTokenizer->Error("Syntax error: expected integer near '%s'", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()));
				stateAssignment->uiValue = 0;
				return false;
			}
		}
		else if (expectsFloat)
		{
			if (!AcceptFloat(stateAssignment->fValue))
			{
				m_pFullTokenizer->Error("Syntax error: expected float near '%s'", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()));
				stateAssignment->iValue = 0;
				return false;
			}
		}
		else if (expectsBoolean)
		{
			const EffectStateValue * stateValue = GetStateValue(m_pFullTokenizer->GetIdentifier(), state);

			if (stateValue != NULL)
			{
				stateAssignment->iValue = stateValue->value;

				m_pFullTokenizer->Next();
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
				m_pFullTokenizer->Error("Syntax error: expected bool near '%s'", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()));
				stateAssignment->iValue = 0;
				return false;
			}
		}
		else if (expectsExpression)
		{
			if (!ParseColorMask(stateAssignment->iValue))
			{
				m_pFullTokenizer->Error("Syntax error: expected color mask near '%s'", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()));
				stateAssignment->iValue = 0;
				return false;
			}
		}
		else 
		{
			// Expect one of the allowed values.
			const EffectStateValue * stateValue = GetStateValue(m_pFullTokenizer->GetIdentifier(), state);

			if (stateValue == NULL)
			{
				m_pFullTokenizer->Error("Syntax error: unexpected value '%s' for state '%s'", CHECK_CSTR(m_pFullTokenizer->GetIdentifier()), CHECK_CSTR(state->name));
				stateAssignment->iValue = 0;
				return false;
			}

			stateAssignment->iValue = stateValue->value;

			m_pFullTokenizer->Next();
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

	stateAssignment->stateName = m_tree->AddStringCached(state->name);
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
		CachedString identifier;
		if (!ExpectIdentifier(identifier)) {
			return false;
		}

		HLSLAttribute * attribute = m_tree->AddNode<HLSLAttribute>(fileName, line);
		
		if (String_Equal(identifier, "unroll")) 
		{
			attribute->attributeType = HLSLAttributeType_Unroll;
			m_tree->gExtension[USE_ControlFlowAttributes] = true;

			//optional
			if (Accept('('))
			{
				unsigned int iValue = 0;
				CachedString id;
				int value = 0;

				if (AcceptUint(iValue))
				{
					attribute->unrollCount = iValue;
				}
				else if (AcceptInt(value))
				{
					if (value < 0)
					{
						m_pFullTokenizer->Error("error) unroll can not be a negative number");
					}
					attribute->unrollCount = value;
				}
				else if (AcceptIdentifier(id))
				{
					attribute->unrollIdentifier = id;
				}
				else
				{
					m_pFullTokenizer->Error("error) unroll");
					return false;
				}

				FullToken nextAttrToken = m_pFullTokenizer->GetFullToken();

				if (!Expect(')'))
					return false;
			}

		}
		else if (String_Equal(identifier, "flatten"))
		{
			m_tree->gExtension[USE_ControlFlowAttributes] = true;
			attribute->attributeType = HLSLAttributeType_Flatten;
		}
		else if (String_Equal(identifier, "branch"))
		{
			m_tree->gExtension[USE_ControlFlowAttributes] = true;
			attribute->attributeType = HLSLAttributeType_Branch;
		}
		else if (String_Equal(identifier, "numthreads"))
		{
			attribute->attributeType = HLSLAttributeType_NumThreads;
			
			if (!Expect('('))
				return false;
			
			attribute->numGroupX = m_pFullTokenizer->GetuInt();

			if (attribute->numGroupX == 0)
				attribute->numGroupXstr = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect(','))
				return false;

			attribute->numGroupY = m_pFullTokenizer->GetuInt();

			if (attribute->numGroupY == 0)
				attribute->numGroupYstr = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect(','))
				return false;

			attribute->numGroupZ = m_pFullTokenizer->GetuInt();

			if (attribute->numGroupZ == 0)
				attribute->numGroupZstr = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;

		}
		else if (String_Equal(identifier, "maxvertexcount"))
		{
			attribute->attributeType = HLSLAttributeType_MaxVertexCount;

			if (!Expect('('))
				return false;

			attribute->maxVertexCount = m_pFullTokenizer->GetuInt();
			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;
		}
		else if (String_Equal(identifier, "domain"))
		{
			attribute->attributeType = HLSLAttributeType_Domain;

			if (!Expect('('))
				return false;

			attribute->domain = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;
		}
		else if (String_Equal(identifier, "partitioning"))
		{
			attribute->attributeType = HLSLAttributeType_Partitioning;

			if (!Expect('('))
				return false;

			attribute->partitioning = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;
		}
		else if (String_Equal(identifier, "outputtopology"))
		{
			attribute->attributeType = HLSLAttributeType_OutputTopology;

			if (!Expect('('))
				return false;

			attribute->outputtopology = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;
		}
		else if (String_Equal(identifier, "outputcontrolpoints"))
		{
			attribute->attributeType = HLSLAttributeType_OutputControlPoints;

			if (!Expect('('))
				return false;

			attribute->outputcontrolpoints = m_pFullTokenizer->GetuInt();
			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;
		}
		else if (String_Equal(identifier, "patchconstantfunc"))
		{
			attribute->attributeType = HLSLAttributeType_PatchConstantFunc;

			if (!Expect('('))
				return false;

			attribute->patchconstantfunc = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());

			// TODO: Clean me
			char tempFuncName[128];
			strcpy(tempFuncName, GetCstr(attribute->patchconstantfunc) + 1);
			tempFuncName[strlen(tempFuncName)-1] = NULL;

			HLSLFunction* function = FindFunction(m_tree->AddStringCached(tempFuncName));

			if (function)
			{
				function->bPatchconstantfunc = true;

			}
			else
				return false;


			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;
		}
		else if (String_Equal(identifier, "maxtessfactor"))
		{
			attribute->attributeType = HLSLAttributeType_MaxtessFactor;

			if (!Expect('('))
				return false;

			attribute->maxTessellationFactor = m_pFullTokenizer->GetFloat();
			m_pFullTokenizer->Next();

			if (!Expect(')'))
				return false;
		}
		else if (String_Equal(identifier, "earlydepthstencil"))
		{
			attribute->attributeType = HLSLAttributeType_EarlyDepthStencil;		

			attribute->earlyDepthStencil = true;
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
//   [A()] statement;
//   [A(a)] statement;
// These are not supported yet:
//   [A] statement [B];
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
	CachedString stageName;// = NULL;
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

bool HLSLParser::Parse(HLSLTree* tree)
{
	m_tree = tree;

	m_entryName = m_tree->AddStringCached(m_entryNameStr.c_str());


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

	//check there is right entryFunction
	if (!FindFunction(m_entryName))
	{
		m_pFullTokenizer->Error("There is no matched Entry function!");
		return false;
	}

	
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


bool HLSLParser::AcceptType(bool allowVoid, HLSLBaseType& type, CachedString &typeName, int* typeFlags)
{
	if (typeFlags != NULL) {
		*typeFlags = 0;
		while(AcceptTypeModifier(*typeFlags) || AcceptInterpolationModifier(*typeFlags)) {}
	}

	int token = m_pFullTokenizer->GetToken();

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
		type = HLSLBaseType_Float; typeName = m_tree->AddStringCached("float"); break;
	case HLSLToken_Float1x2:
		type = HLSLBaseType_Float1x2; break;
	case HLSLToken_Float1x3:
		type = HLSLBaseType_Float1x3; break;
	case HLSLToken_Float1x4:
		type = HLSLBaseType_Float1x4; break;

	case HLSLToken_Float2:      
		type = HLSLBaseType_Float2; typeName = m_tree->AddStringCached("float2"); break;
	case HLSLToken_Float2x2:
		type = HLSLBaseType_Float2x2; break;
	case HLSLToken_Float2x3:
		type = HLSLBaseType_Float2x3; break;
	case HLSLToken_Float2x4:
		type = HLSLBaseType_Float2x4; break;

	case HLSLToken_Float3:
		type = HLSLBaseType_Float3; typeName = m_tree->AddStringCached("float3"); break;
	case HLSLToken_Float3x2:
		type = HLSLBaseType_Float3x2; break;
	case HLSLToken_Float3x3:
		type = HLSLBaseType_Float3x3; m_tree->gExtension[USE_3X3_CONVERSION] = true; break;
	case HLSLToken_Float3x4:
		type = HLSLBaseType_Float3x4; break;

	case HLSLToken_Float4:
		type = HLSLBaseType_Float4; typeName = m_tree->AddStringCached("float4"); break;
	case HLSLToken_Float4x2:
		type = HLSLBaseType_Float4x2; break;
	case HLSLToken_Float4x3:
		type = HLSLBaseType_Float4x3; break;
	case HLSLToken_Float4x4:
		type = HLSLBaseType_Float4x4; break;

	
	case HLSLToken_Half:
		type = HLSLBaseType_Half; typeName = m_tree->AddStringCached("half"); break;
	case HLSLToken_Half1x2:
		type = HLSLBaseType_Half1x2; break;
	case HLSLToken_Half1x3:
		type = HLSLBaseType_Half1x3; break;
	case HLSLToken_Half1x4:
		type = HLSLBaseType_Half1x4; break;

	case HLSLToken_Half2:      
		type = HLSLBaseType_Half2; typeName = m_tree->AddStringCached("half2"); break;
	case HLSLToken_Half2x2:
		type = HLSLBaseType_Half2x2; break;
	case HLSLToken_Half2x3:
		type = HLSLBaseType_Half2x3; break;
	case HLSLToken_Half2x4:
		type = HLSLBaseType_Half2x4; break;

	case HLSLToken_Half3:
		type = HLSLBaseType_Half3; typeName = m_tree->AddStringCached("half3"); break;
	case HLSLToken_Half3x2:
		type = HLSLBaseType_Half3x2; break;
	case HLSLToken_Half3x3:
		type = HLSLBaseType_Half3x3; m_tree->gExtension[USE_3X3_CONVERSION] = true; break;
	case HLSLToken_Half3x4:
		type = HLSLBaseType_Half3x4; break;
		
	case HLSLToken_Half4:
		type = HLSLBaseType_Half4; typeName = m_tree->AddStringCached("half4"); break;
	case HLSLToken_Half4x2:
		type = HLSLBaseType_Half4x2; break;
	case HLSLToken_Half4x3:
		type = HLSLBaseType_Half4x3; break;
	case HLSLToken_Half4x4:
		type = HLSLBaseType_Half4x4; break;

	case HLSLToken_Min16Float:
		type = HLSLBaseType_Min16Float; typeName = m_tree->AddStringCached("min16float"); break;
	case HLSLToken_Min16Float1x2:
		type = HLSLBaseType_Min16Float1x2; break;
	case HLSLToken_Min16Float1x3:
		type = HLSLBaseType_Min16Float1x3; break;
	case HLSLToken_Min16Float1x4:
		type = HLSLBaseType_Min16Float1x4; break;

	case HLSLToken_Min16Float2:
		type = HLSLBaseType_Min16Float2; typeName = m_tree->AddStringCached("min16float2"); break;
	case HLSLToken_Min16Float2x2:
		type = HLSLBaseType_Min16Float2x2; break;
	case HLSLToken_Min16Float2x3:
		type = HLSLBaseType_Min16Float2x3; break;
	case HLSLToken_Min16Float2x4:
		type = HLSLBaseType_Min16Float2x4; break;

	case HLSLToken_Min16Float3:
		type = HLSLBaseType_Min16Float3; typeName = m_tree->AddStringCached("min16float3"); break;
	case HLSLToken_Min16Float3x2:
		type = HLSLBaseType_Min16Float3x2; break;
	case HLSLToken_Min16Float3x3:
		type = HLSLBaseType_Min16Float3x3;  m_tree->gExtension[USE_3X3_CONVERSION] = true; break;
	case HLSLToken_Min16Float3x4:
		type = HLSLBaseType_Min16Float3x4; break;

	case HLSLToken_Min16Float4:
		type = HLSLBaseType_Min16Float4; typeName = m_tree->AddStringCached("min16float4"); break;
	case HLSLToken_Min16Float4x2:
		type = HLSLBaseType_Min16Float4x2; break;
	case HLSLToken_Min16Float4x3:
		type = HLSLBaseType_Min16Float4x3; break;
	case HLSLToken_Min16Float4x4:
		type = HLSLBaseType_Min16Float4x4; break;


	case HLSLToken_Min10Float:
		type = HLSLBaseType_Min10Float; typeName = m_tree->AddStringCached("min10float"); break;
	case HLSLToken_Min10Float1x2:
		type = HLSLBaseType_Min10Float1x2; break;
	case HLSLToken_Min10Float1x3:
		type = HLSLBaseType_Min10Float1x3; break;
	case HLSLToken_Min10Float1x4:
		type = HLSLBaseType_Min10Float1x4; break;

	case HLSLToken_Min10Float2:
		type = HLSLBaseType_Min10Float2; typeName = m_tree->AddStringCached("min10float2"); break;
	case HLSLToken_Min10Float2x2:
		type = HLSLBaseType_Min10Float2x2; break;
	case HLSLToken_Min10Float2x3:
		type = HLSLBaseType_Min10Float2x3; break;
	case HLSLToken_Min10Float2x4:
		type = HLSLBaseType_Min10Float2x4; break;

	case HLSLToken_Min10Float3:
		type = HLSLBaseType_Min10Float3; typeName = m_tree->AddStringCached("min10float3"); break;
	case HLSLToken_Min10Float3x2:
		type = HLSLBaseType_Min10Float3x2; break;
	case HLSLToken_Min10Float3x3:
		type = HLSLBaseType_Min10Float3x3;  m_tree->gExtension[USE_3X3_CONVERSION] = true; break;
	case HLSLToken_Min10Float3x4:
		type = HLSLBaseType_Min10Float3x4; break;

	case HLSLToken_Min10Float4:
		type = HLSLBaseType_Min10Float4; typeName = m_tree->AddStringCached("min10float4"); break;
	case HLSLToken_Min10Float4x2:
		type = HLSLBaseType_Min10Float4x2; break;
	case HLSLToken_Min10Float4x3:
		type = HLSLBaseType_Min10Float4x3; break;
	case HLSLToken_Min10Float4x4:
		type = HLSLBaseType_Min10Float4x4; break;


	case HLSLToken_Int:
		type = HLSLBaseType_Int; typeName = m_tree->AddStringCached("int"); break;
	case HLSLToken_Int1x2:
		type = HLSLBaseType_Int1x2; break;
	case HLSLToken_Int1x3:
		type = HLSLBaseType_Int1x3; break;
	case HLSLToken_Int1x4:
		type = HLSLBaseType_Int1x4; break;

	case HLSLToken_Int2:
		type = HLSLBaseType_Int2; typeName = m_tree->AddStringCached("int2"); break;
	case HLSLToken_Int2x2:
		type = HLSLBaseType_Int2x2; break;
	case HLSLToken_Int2x3:
		type = HLSLBaseType_Int2x3; break;
	case HLSLToken_Int2x4:
		type = HLSLBaseType_Int2x4; break;
		
	case HLSLToken_Int3:
		type = HLSLBaseType_Int3; typeName = m_tree->AddStringCached("int3"); break;
	case HLSLToken_Int3x2:
		type = HLSLBaseType_Int3x2; break;
	case HLSLToken_Int3x3:
		type = HLSLBaseType_Int3x3; break;
	case HLSLToken_Int3x4:
		type = HLSLBaseType_Int3x4; break;
		
	case HLSLToken_Int4:
		type = HLSLBaseType_Int4;  typeName = m_tree->AddStringCached("int4"); break;
	case HLSLToken_Int4x2:
		type = HLSLBaseType_Int4x2; break;
	case HLSLToken_Int4x3:
		type = HLSLBaseType_Int4x3; break;
	case HLSLToken_Int4x4:
		type = HLSLBaseType_Int4x4; break;

	case HLSLToken_Uint:
		type = HLSLBaseType_Uint; typeName = m_tree->AddStringCached("uint");  break;
	case HLSLToken_Uint1x2:
		type = HLSLBaseType_Uint1x2; break;
	case HLSLToken_Uint1x3:
		type = HLSLBaseType_Uint1x3; break;
	case HLSLToken_Uint1x4:
		type = HLSLBaseType_Uint1x4; break;

	case HLSLToken_Uint2:
		type = HLSLBaseType_Uint2;  typeName = m_tree->AddStringCached("uint2"); break;
	case HLSLToken_Uint2x2:
		type = HLSLBaseType_Uint2x2; break;
	case HLSLToken_Uint2x3:
		type = HLSLBaseType_Uint2x3; break;
	case HLSLToken_Uint2x4:
		type = HLSLBaseType_Uint2x4; break;

	case HLSLToken_Uint3:
		type = HLSLBaseType_Uint3;  typeName = m_tree->AddStringCached("uint3"); break;
	case HLSLToken_Uint3x2:
		type = HLSLBaseType_Uint3x2; break;
	case HLSLToken_Uint3x3:
		type = HLSLBaseType_Uint3x3; break;
	case HLSLToken_Uint3x4:
		type = HLSLBaseType_Uint3x4; break;

	case HLSLToken_Uint4:
		type = HLSLBaseType_Uint4;  typeName = m_tree->AddStringCached("uint4"); break;
	case HLSLToken_Uint4x2:
		type = HLSLBaseType_Uint4x2; break;
	case HLSLToken_Uint4x3:
		type = HLSLBaseType_Uint4x3; break;
	case HLSLToken_Uint4x4:
		type = HLSLBaseType_Uint4x4; break;
		
	case HLSLToken_InputPatch:
		type = HLSLBaseType_InputPatch;
		m_pFullTokenizer->Next();
		return true;
		break;

	case HLSLToken_OutputPatch:
		type = HLSLBaseType_OutputPatch;
		m_pFullTokenizer->Next();
		return true;
		break;

	case HLSLToken_PointStream:
		type = HLSLBaseType_PointStream;
		m_pFullTokenizer->Next();
		return true;
		break;

	case HLSLToken_LineStream:
		type = HLSLBaseType_LineStream;
		m_pFullTokenizer->Next();
		return true;
		break;

	case HLSLToken_TriangleStream:
		type = HLSLBaseType_TriangleStream; 
		m_pFullTokenizer->Next();
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

	case HLSLToken_Texture1D:
		type = HLSLBaseType_Texture1D;
		break;

	case HLSLToken_Texture1DArray:
		type = HLSLBaseType_Texture1DArray;
		break;

	case HLSLToken_Texture2D:
		type = HLSLBaseType_Texture2D;
		break;

	case HLSLToken_Texture2DArray:
		type = HLSLBaseType_Texture2DArray;
		break;

	case HLSLToken_Texture3D:
		type = HLSLBaseType_Texture3D;
		break;

	case HLSLToken_Texture2DMS:
		type = HLSLBaseType_Texture2DMS;
		break;

	case HLSLToken_Texture2DMSArray:
		type = HLSLBaseType_Texture2DMSArray;
		break;

	case HLSLToken_TextureCubeArray:
		type = HLSLBaseType_TextureCubeArray;
		break;

	case HLSLToken_RWTexture1D:
		type = HLSLBaseType_RWTexture1D;

	case HLSLToken_RWTexture1DArray:
		type = HLSLBaseType_RWTexture1DArray;
		break;

	case HLSLToken_RWTexture2D:
		type = HLSLBaseType_RWTexture2D;
		break;

	case HLSLToken_RWTexture2DArray:
		type = HLSLBaseType_RWTexture2DArray;
		break;

	case HLSLToken_RWTexture3D:
		type = HLSLBaseType_RWTexture3D;
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
		m_pFullTokenizer->Next();
		return true;
	}

	if (allowVoid && Accept(HLSLToken_Void))
	{
		type = HLSLBaseType_Void;
		return true;
	}

	if (token == HLSLToken_Identifier)
	{
		//Confetti's rule
		if (String_Equal(m_pFullTokenizer->GetIdentifier(), "mat4"))
		{
			type = HLSLBaseType_Float4x4;
			m_pFullTokenizer->Next();
			return true;
		}

		//Define preprocessor		
	
		CachedString identifier = m_tree->AddStringCached( m_pFullTokenizer->GetIdentifier() );
		if (FindUserDefinedType(identifier) != NULL)
		{
			m_pFullTokenizer->Next();
			type        = HLSLBaseType_UserDefined;
			typeName    = identifier;
			return true;
		}
	}
	return false;
}

bool HLSLParser::ExpectType(bool allowVoid, HLSLBaseType& type, CachedString & typeName, int* typeFlags)
{
	if (!AcceptType(allowVoid, type, typeName, typeFlags))
	{
		m_pFullTokenizer->Error("Expected type");
		return false;
	}
	return true;
}

bool HLSLParser::AcceptDeclaration(bool allowUnsizedArray, HLSLType& type, CachedString& name)
{
	if (!AcceptType(/*allowVoid=*/false, type.baseType, type.typeName, &type.flags))
	{
		return false;
	}

	
	if (Accept('<'))
	{		

		if (type.baseType == HLSLBaseType_Texture2DMS || type.baseType == HLSLBaseType_Texture2DMSArray)
		{
			type.textureTypeName = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect(','))
				return false;

			CachedString temp;
			int iValue;
			if (AcceptIdentifier(temp))
			{
				type.typeName = temp;
			}
			else if (AcceptInt(iValue))
			{
				type.sampleCount = iValue;
			}
			else
			{
				return false;
			}

			if (!Expect('>'))
				return false;

		}
		else if (type.baseType >= HLSLBaseType_Texture1D && type.baseType <= HLSLBaseType_RWTexture3D)
		{
			type.textureTypeName = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
			m_pFullTokenizer->Next();

			if (!Expect('>'))
				return false;

		}
		else
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

						char buf[2048];

						strcpy(buf, "InputPatch<");
						strcat(buf, GetCstr(type.typeName));
						strcat(buf, ", ");

						char intBuffer[128];
						_itoa_s(type.maxPoints, intBuffer, 10);
						strcat(buf, intBuffer);

						strcat(buf, ">");

						type.InputPatchName = m_tree->AddStringCached(buf);
					}
					else if (type.baseType == HLSLBaseType_OutputPatch)
					{
						type.typeName = type.structuredTypeName;

						char buf[2048];

						strcpy(buf, "OutputPatch<");
						strcat(buf, GetCstr(type.typeName));
						strcat(buf, ", ");

						char intBuffer[128];
						_itoa_s(type.maxPoints, intBuffer, 10);
						strcat(buf, intBuffer);

						strcat(buf, ">");

						type.OutputPatchName = m_tree->AddStringCached(buf);
					}

					type.array = true;
				}
			}

			if (!Expect('>'))
				return false;
		}
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
		if (!ParseExpression(type.arraySize,false,0) || !Expect(']'))
		{
			return false;
		}
	}
	return true;
}

bool HLSLParser::ExpectDeclaration(bool allowUnsizedArray, HLSLType& type, CachedString & name)
{
	if (!AcceptDeclaration(allowUnsizedArray, type, name))
	{
		m_pFullTokenizer->Error("Expected declaration");
		return false;
	}
	return true;
}

const HLSLSamplerState* HLSLParser::FindSamplerStateDefinedType(const CachedString & name) const
{
	// Pointer comparison is sufficient for strings since they exist in the
	// string pool.
	for (int i = 0; i < m_samplerStates.size(); ++i)
	{
		if (String_Equal(m_samplerStates[i]->cachedName, name))
		{
			return m_samplerStates[i];
		}
	}
	return NULL;
}

const HLSLTextureState* HLSLParser::FindTextureStateDefinedType(const CachedString & name) const
{	
	for (int i = 0; i < m_textureStates.size(); ++i)
	{		
		if (String_Equal(m_textureStates[i]->cachedName, name))
		{
			return m_textureStates[i];
		}	
	}

	return NULL;
}

const HLSLTextureState* HLSLParser::FindTextureStateDefinedTypeWithAddress(const CachedString & name) const
{
	return NULL;
}

HLSLBuffer* HLSLParser::FindBuffer(const CachedString & name) const
{
	for (int i = 0; i < m_Buffers.size(); ++i)
	{
		if (String_Equal(m_Buffers[i]->cachedName, name))
		{
			return m_Buffers[i];
		}
	}
	return NULL;
}

HLSLBuffer* HLSLParser::FindConstantBuffer(const CachedString & name) const
{
	for (int i = 0; i < m_Buffers.size(); ++i)
	{
		if (String_Equal(m_Buffers[i]->cachedName, name) && m_Buffers[i]->type.baseType == HLSLBaseType_ConstantBuffer)
		{
			return m_Buffers[i];
		}
	}
	return NULL;
}

CachedString HLSLParser::FindParentTextureOrBuffer(const HLSLTextureState* & foundTexture, HLSLBuffer * & foundBuffer) const
{
	CachedString ret;
	foundTexture = NULL;
	foundBuffer = NULL;

	for (int i = (int)(m_parentStackIdentifiers.size()) - 1; i >= 0; i--)
	{
		CachedString found = m_parentStackIdentifiers[i];

		foundTexture = FindTextureStateDefinedType(found);
		foundBuffer = FindBuffer(found);

		if (foundTexture != NULL ||
			foundBuffer != NULL)
		{
			ret = found;
			break;
		}
	}

	return ret;
}

 HLSLStruct* HLSLParser::FindUserDefinedType(const CachedString & name) const
{
	for (int i = 0; i < m_userTypes.size(); ++i)
	{
		if (String_Equal(m_userTypes[i]->name, name))
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
		m_pFullTokenizer->GetTokenName(endToken, what);
		m_pFullTokenizer->Error("Unexpected end of file while looking for '%s'", CHECK_CSTR(what));
		return true;
	}
	return false;
}

int HLSLParser::GetLineNumber() const
{
	return m_pFullTokenizer->GetLineNumber();
}

const char* HLSLParser::GetFileName()
{
	return m_pFullTokenizer->GetFileName();
}

void HLSLParser::BeginScope()
{
	m_variableScopes.push_back(m_variables.size());
	m_textureStateScopes.push_back(m_textureStates.size());
	m_textureStateExpressionScopes.push_back(m_textureStateExpressions.size());
}

void HLSLParser::EndScope()
{
	ASSERT_PARSER(!m_variableScopes.empty());
	m_variables.resize(m_variableScopes.back());
	m_variableScopes.pop_back();

	ASSERT_PARSER(!m_textureStateScopes.empty());
	m_textureStates.resize(m_textureStateScopes.back());
	m_textureStateScopes.pop_back();

	ASSERT_PARSER(!m_textureStateExpressionScopes.empty());
	m_textureStateExpressions.resize(m_textureStateExpressionScopes.back());
	m_textureStateExpressionScopes.pop_back();
}

const HLSLType* HLSLParser::FindVariable(const CachedString & name, bool& global) const
{
	for (int i = int(m_variables.size()) - 1; i >= 0; --i)
	{
		if (String_Equal(m_variables[i].name,name))
		{
			global = (i < m_numGlobals);
			return &m_variables[i].type;
		}
	}
	return NULL;
}

HLSLFunction* HLSLParser::FindFunction(const CachedString & name)
{
	for (int i = 0; i < m_functions.size(); ++i)
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

static bool AreArgumentListsEqualVec(HLSLTree* tree, const eastl::vector < HLSLArgument* > & lhsVec, const eastl::vector < HLSLArgument* > & rhsVec)
{
	if (lhsVec.size() != rhsVec.size())
	{
		return false;
	}

	for (int i = 0; i < lhsVec.size(); i++)
	{
		HLSLArgument * lhs = lhsVec[i];
		HLSLArgument * rhs = rhsVec[i];

		if (lhs == NULL && rhs != NULL)
		{
			return false;
		}

		if (lhs != NULL && rhs != NULL)
		{
			return false;
		}

		if (!AreTypesEqual(tree, lhs->argType, rhs->argType))
			return false;

		if (lhs->modifier != rhs->modifier)
			return false;

		if (!String_Equal(lhs->semantic,rhs->semantic))
			return false;

		if (!String_Equal(lhs->sv_semantic, rhs->sv_semantic))
			return false;
	}

	return true;
}

const HLSLFunction* HLSLParser::FindFunction(const HLSLFunction* fun) const
{
	for (int i = 0; i < m_functions.size(); ++i)
	{
		if ( String_Equal(m_functions[i]->name, fun->name) &&
			AreTypesEqual(m_tree, m_functions[i]->returnType, fun->returnType) &&
			AreArgumentListsEqualVec(m_tree, m_functions[i]->GetArguments(), fun->GetArguments()))
		{
			return m_functions[i];
		}
	}
	return NULL;
}

void HLSLParser::DeclareVariable(const CachedString & name, const HLSLType& type)
{
	if (m_variables.size() == m_numGlobals)
	{
		++m_numGlobals;
	}

	Variable variable;
	variable.name = name;
	variable.type = type;
	m_variables.push_back(variable);
}

bool HLSLParser::GetIsFunction(const CachedString & name) const
{
	for (int i = 0; i < m_functions.size(); ++i)
	{	
		if (String_Equal(m_functions[i]->name, name))
		{
			return true;
		}
	}
	for (int i = 0; i < m_intrinsicHelper->m_intrinsics.size(); ++i)
	{
		if (String_Equal(name, m_intrinsicHelper->m_intrinsics[i]->rawName))
		{
			if (String_Equal(name, "NonUniformResourceIndex"))
			{
				m_tree->gExtension[USE_NonUniformResourceIndex] = true;
			}
			else if (String_Equal(name, "QuadReadAcrossDiagonal") ||
				String_Equal(name, "QuadReadLaneAt") ||
				String_Equal(name, "QuadReadAcrossX") ||
				String_Equal(name, "QuadReadAcrossY") ||
				String_Equal(name, "WaveActiveAllEqual") ||
				String_Equal(name, "WaveActiveBitAnd") ||
				String_Equal(name, "WaveActiveBitOr") ||
				String_Equal(name, "WaveActiveBitXor") ||
				String_Equal(name, "WaveActiveCountBits") ||
				String_Equal(name, "WaveActiveMax") ||
				String_Equal(name, "WaveActiveMin") ||
				String_Equal(name, "WaveActiveProduct") ||
				String_Equal(name, "WaveActiveSum") ||
				String_Equal(name, "WaveActiveAllTrue") ||
				String_Equal(name, "WaveActiveAnyTrue") ||
				String_Equal(name, "WaveActiveBallot") ||
				String_Equal(name, "WaveGetLaneCount") ||
				String_Equal(name, "WaveGetLaneIndex") ||
				String_Equal(name, "WaveIsFirstLane") ||
				String_Equal(name, "WavePrefixCountBits") ||
				String_Equal(name, "WavePrefixProduct") ||				
				String_Equal(name, "WavePrefixSum") ||
				String_Equal(name, "WaveReadLaneFirst") ||
				String_Equal(name, "WaveReadLaneAt"))
			{
				m_tree->gExtension[USE_Subgroup_Basic] = true;

				if (String_Equal(name, "QuadReadAcrossDiagonal") ||
					String_Equal(name, "QuadReadLaneAt") ||
					String_Equal(name, "QuadReadAcrossX") ||
					String_Equal(name, "QuadReadAcrossY"))
				{
					m_tree->gExtension[USE_Subgroup_Quad] = true;
				}
				else if (String_Equal(name, "WaveActiveBallot"))
				{
					m_tree->gExtension[USE_Subgroup_Ballot] = true;
				}
				else if(String_Equal(name, "WaveGetLaneIndex"))
				{
					m_tree->gExtension[USE_WaveGetLaneIndex] = true;
				}
				else if (String_Equal(name, "WaveGetLaneCount"))
				{
					m_tree->gExtension[USE_WaveGetLaneCount] = true;
				}
				else
				{
					m_tree->gExtension[USE_Subgroup_Arithmetic] = true;
				}	
			}
		
			return true;
		}
	}

	return false;
}

const HLSLFunction* HLSLParser::MatchFunctionCall(const HLSLFunctionCall* functionCall, const CachedString & name)
{
	const HLSLFunction* matchedFunction     = NULL;

	int  numArguments           = functionCall->numArguments;
	int  numMatchedOverloads    = 0;
	bool nameMatches            = false;

	// Get the user defined functions with the specified name.
	for (int i = 0; i < m_functions.size(); ++i)
	{
		const HLSLFunction* function = m_functions[i];
		if (String_Equal(function->name,name))
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
	int intrinsicId = -1;
	for (int i = 0; i < m_intrinsicHelper->m_intrinsics.size(); i++)
	{
		if (String_Equal(m_intrinsicHelper->m_intrinsics[i]->rawName, name))
		{
			nameMatches = true;

			const HLSLFunction* function = &m_intrinsicHelper->m_intrinsics[i]->fullFunction;

			CompareFunctionsResult result = CompareFunctions( m_tree, functionCall, function, matchedFunction );
			if (result == Function1Better)
			{
				matchedFunction = function;
				numMatchedOverloads = 1;
				intrinsicId = i;
			}
			else if (result == FunctionsEqual)
			{
				++numMatchedOverloads;
			}
		}
	}
	
	// that function is owned by a different memory allocator, so create a new function
	if (intrinsicId >= 0)
	{
		int line = GetLineNumber();
		const char* fileName = GetFileName();

		HLSLFunction* buildFunc = m_tree->AddNode<HLSLFunction>(fileName, line);

		const Intrinsic * intrinsic = m_intrinsicHelper->m_intrinsics[intrinsicId];

		buildFunc->name = m_tree->AddStringCached(intrinsic->rawName);
		buildFunc->returnType.baseType = intrinsic->fullFunction.returnType.baseType;
		
		for (int i = 0; i < intrinsic->fullFunction.argumentVec.size(); i++)
		{
			HLSLArgument* arg = m_tree->AddNode<HLSLArgument>(fileName,line);

			arg->argType.baseType = intrinsic->fullFunction.argumentVec[i]->argType.baseType;
			arg->argType.flags = intrinsic->fullFunction.argumentVec[i]->argType.flags;

			buildFunc->argumentVec.push_back(arg);
		}

		matchedFunction = buildFunc;
	}

	if (matchedFunction != NULL && numMatchedOverloads > 1)
	{
		// Multiple overloads match.

		// Skip Overloads
	}
	else if (matchedFunction == NULL)
	{
		if (nameMatches)
		{
			m_pFullTokenizer->Error("'%s' no overloaded function matched all of the arguments", GetCstr(name));
		}
		else
		{
			m_pFullTokenizer->Error("Undeclared identifier '%s'", GetCstr(name));
		}
	}

	return matchedFunction;
}

bool HLSLParser::GetMemberType(HLSLType& objectType, HLSLMemberAccess * memberAccess)
{
	CachedString fieldName = memberAccess->field;

	HLSLBaseType comparingType = HLSLBaseType_Unknown;

	if (objectType.elementType != HLSLBaseType_Unknown)
		comparingType = objectType.elementType;
	else
		comparingType = objectType.baseType;

	if (comparingType == HLSLBaseType_UserDefined)
	{
		const HLSLStruct* structure = FindUserDefinedType( objectType.typeName );
		
		if (structure == NULL)
			return false;

		const HLSLStructField* field = structure->field;
		while (field != NULL)
		{
			if (String_Equal(field->name,fieldName))
			{
				memberAccess->expressionType = field->type;
				return true;
			}
			field = field->nextField;
		}

		return false;
	}
	//hull
	else if (comparingType == HLSLBaseType_InputPatch || comparingType == HLSLBaseType_OutputPatch)
	{
		const HLSLStruct* structure = FindUserDefinedType(objectType.typeName);

		if (structure == NULL)
			return false;

		const HLSLStructField* field = structure->field;
		while (field != NULL)
		{
			if (String_Equal(field->name,fieldName))
			{
				memberAccess->expressionType = field->type;
				return true;
			}
			field = field->nextField;
		}

		return false;
	}
	//geometry
	else if ((comparingType == HLSLBaseType_PointStream) || (comparingType == HLSLBaseType_LineStream) || (comparingType == HLSLBaseType_TriangleStream))
	{
		if (String_Equal(fieldName, "Append"))
		{
			memberAccess->function = true;

			bool needsEndParen;
			if (!ParseTerminalExpression(memberAccess->functionExpression, needsEndParen, false))
				return false;

			return true;
		}
		else if (String_Equal(fieldName, "RestartStrip"))
		{
			memberAccess->function = true;

			bool needsEndParen;
			if (!ParseTerminalExpression(memberAccess->functionExpression, needsEndParen, false))
				return false;

			return true;
		}
	}

	if (_baseTypeDescriptions[comparingType].numericType == NumericType_NaN)
	{
		// Currently we don't have an non-numeric types that allow member access.
		return false;
	}

	int swizzleLength = 0;

	if (_baseTypeDescriptions[comparingType].numDimensions <= 1)
	{
		eastl::string fullFieldName = GetCstr(fieldName);
		// Check for a swizzle on the scalar/vector types.
		for (int i = 0; fullFieldName[i] != 0; ++i)
		{
			if (fullFieldName[i] != 'x' && fullFieldName[i] != 'y' && fullFieldName[i] != 'z' && fullFieldName[i] != 'w' &&
				fullFieldName[i] != 'r' && fullFieldName[i] != 'g' && fullFieldName[i] != 'b' && fullFieldName[i] != 'a')
			{
				m_pFullTokenizer->Error("Invalid swizzle '%s'", GetCstr(fieldName));
				return false;
			}
			++swizzleLength;
		}
		ASSERT_PARSER(swizzleLength > 0);
	}
	else
	{
		// Check for a matrix element access (e.g. _m00 or _11)
		eastl::string fullFieldName = GetCstr(fieldName);

		const char* n = fullFieldName.c_str();
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
			if (r >= _baseTypeDescriptions[comparingType].height ||
				c >= _baseTypeDescriptions[comparingType].numComponents)
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
		m_pFullTokenizer->Error("Invalid swizzle '%s'", GetCstr(fieldName));
		return false;
	}

	static const HLSLBaseType floatType[] = { HLSLBaseType_Float, HLSLBaseType_Float2, HLSLBaseType_Float3, HLSLBaseType_Float4 };
	static const HLSLBaseType halfType[]  = { HLSLBaseType_Half,  HLSLBaseType_Half2,  HLSLBaseType_Half3,  HLSLBaseType_Half4  };
	static const HLSLBaseType min16floatType[] = { HLSLBaseType_Min16Float,  HLSLBaseType_Min16Float2,  HLSLBaseType_Min16Float3,  HLSLBaseType_Min16Float4 };
	static const HLSLBaseType min10floatType[] = { HLSLBaseType_Min10Float,  HLSLBaseType_Min10Float2,  HLSLBaseType_Min10Float3,  HLSLBaseType_Min10Float4 };
	static const HLSLBaseType intType[]   = { HLSLBaseType_Int,   HLSLBaseType_Int2,   HLSLBaseType_Int3,   HLSLBaseType_Int4   };
	static const HLSLBaseType uintType[]  = { HLSLBaseType_Uint,  HLSLBaseType_Uint2,  HLSLBaseType_Uint3,  HLSLBaseType_Uint4  };
	static const HLSLBaseType boolType[]  = { HLSLBaseType_Bool,  HLSLBaseType_Bool2,  HLSLBaseType_Bool3,  HLSLBaseType_Bool4  };
	

	switch (_baseTypeDescriptions[comparingType].numericType)
	{
	case NumericType_Float:
		memberAccess->expressionType.baseType = floatType[swizzleLength - 1];
		break;
	case NumericType_Half:
		memberAccess->expressionType.baseType = halfType[swizzleLength - 1];
		break;
	case NumericType_Min16Float:
		memberAccess->expressionType.baseType = min16floatType[swizzleLength - 1];
		break;
	case NumericType_Min10Float:
		memberAccess->expressionType.baseType = min10floatType[swizzleLength - 1];
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
		ASSERT_PARSER(0);
	}

	memberAccess->swizzle = true;
	
	return true;
}

HLSLBaseType HLSLParser::GetBaseTypeFromElement(const char* element)
{
	if (String_Equal(element, "float"))
	{
		return HLSLBaseType_Float;
	}
	else if (String_Equal(element, "half"))
	{
		return HLSLBaseType_Half;
	}
	else if (String_Equal(element, "min16float"))
	{
		return HLSLBaseType_Min16Float;
	}
	else if (String_Equal(element, "min10float"))
	{
		return HLSLBaseType_Min10Float;
	}
	else if (String_Equal(element, "uint"))
	{
		return HLSLBaseType_Uint;
	}
	else if (String_Equal(element, "int"))
	{
		return HLSLBaseType_Int;
	}
	else if (String_Equal(element, "bool"))
	{
		return HLSLBaseType_Bool;
	}	
	else
	{
		return HLSLBaseType_UserDefined;
	}
}

bool HLSLParser::GetBufferElementType(HLSLBuffer* pBuffer, bool bAllowVoid, int* pTypeFlag, bool optional)
{
	if (Accept('<'))
	{
		if(!AcceptType(false, pBuffer->type.elementType, pBuffer->type.typeName, pTypeFlag))
		{
			return false;
		}

		if (pBuffer->type.typeName.IsNotEmpty())
			pBuffer->userDefinedElementTypeStr = pBuffer->type.typeName;

		if (!Expect('>'))
			return false;
		else
			return true;
	}
	else
		return optional;
}

bool IsRegisterId(CachedString str)
{
	char c;
	int d;
	return sscanf(RawStr(str), "%c%d", &c, &d) == 2;
}

bool IsSpaceId(CachedString str)
{
	int d;
	return sscanf(RawStr(str), "space%d", &d) == 1;
}

bool HLSLParser::GetRegisterAssignment(HLSLStatement* pStatement, const char* errorMsg)
{
	if (Accept(':'))
	{
		CachedString id;
		if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(id))
		{
			return false;
		}

		if (IsRegisterId(id))
		{
			pStatement->registerName = id;
			// if there is space
			if (Check(','))
			{
				m_pFullTokenizer->Next();
				//get space name
				if (!ExpectIdentifier(id) || !IsSpaceId(id))
				{
					return false;
				}
				pStatement->registerSpaceName = id;
			}
			else
			{
				// we need to make a distinction between a space specified by the user, vs
				// no space set, so that the user can specify the default space in the
				// code generation/tree traversal phase, as opposed to the parsing phase.
				pStatement->registerSpaceName = m_tree->AddStringCached("none");
			}
		}
		else if(IsSpaceId(id))
		{
			pStatement->registerName = m_tree->AddStringCached("none");
			pStatement->registerSpaceName = id;
		}
		else
		{
			return false;
		}

		if (!Expect(')'))
		{
			return false;
		}

		return true;
	}
	else
	{
		// allow default register placement
		pStatement->registerName = m_tree->AddStringCached("none");
		pStatement->registerSpaceName = m_tree->AddStringCached("none");

		return true;
	}

}


bool HLSLParser::GetBufferBody(HLSLBuffer* pBuffer)
{
	HLSLScopedStackTracker stackTracker(this);

	if (Accept('{'))
	{
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
				m_pFullTokenizer->Error("Expected variable declaration");
				return false;
			}
			DeclareVariable(field->cachedName, field->type);

			field->buffer = pBuffer;
			if (pBuffer->field == NULL)
			{
				pBuffer->field = field;
			}
			else
			{
				lastField->nextStatement = field;
			}
			lastField = field;

			// adding packoffset parsing logic
			if (Accept(':'))
			{
				if (!Expect("packoffset"))
				{
					return false;
				}

				if (!Expect('('))
				{
					return false;
				}

				CachedString pName;
				CachedString pChannel;
				if (!ExpectIdentifier(pName))
				{
					return false;
				}

				if (Accept('.'))
				{
					if (!ExpectIdentifier(pChannel))
					{
						return false;
					}
				}

				if (!Expect(')'))
				{
					return false;
				}
			}

			if (!Expect(';'))
				return false;
			
		}

		// need to check
		return true;
	}
	else
		return false;
}

void HLSLParser::GetBufferArray(HLSLBuffer* pBuffer)
{
	// Handle array syntax.
	while (Accept('['))
	{
		pBuffer->bArray = true;

		if (Check(']')) //unboundedSize
		{
			pBuffer->arrayIdentifier[pBuffer->arrayDimension].Reset();
			m_pFullTokenizer->Next();
		}
		else if (!Accept(']'))
		{
			if (!String_Equal(m_pFullTokenizer->GetIdentifier(), ""))
			{
				pBuffer->arrayIdentifier[pBuffer->arrayDimension] = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
				m_pFullTokenizer->Next();
				Expect(']');
			}
			else
			{
				pBuffer->arrayIndex[pBuffer->arrayDimension] = m_pFullTokenizer->GetuInt();
				m_pFullTokenizer->Next();
				Expect(']');
			}
		}

		pBuffer->arrayDimension++;
	}
}

void HLSLParser::GetTextureArray(HLSLTextureState* pTextureState)
{
	// Handle array syntax.
	while (Accept('['))
	{
		pTextureState->bArray = true;

		if (Check(']')) //unboundedSize
		{
			pTextureState->arrayIdentifier[pTextureState->arrayDimension].Reset();
			m_pFullTokenizer->Next();
		}
		else if (!Accept(']'))
		{
			if (!String_Equal(m_pFullTokenizer->GetIdentifier(), ""))
			{
				pTextureState->arrayIdentifier[pTextureState->arrayDimension] = m_tree->AddStringCached(m_pFullTokenizer->GetIdentifier());
				m_pFullTokenizer->Next();
				Expect(']');
			}
			else
			{
				pTextureState->arrayIndex[pTextureState->arrayDimension] = m_pFullTokenizer->GetuInt();
				m_pFullTokenizer->Next();
				Expect(']');
			}
		}

		pTextureState->arrayDimension++;
	}
}


bool HLSLParser::GetTextureElementType(HLSLTextureState* pTextureState, bool bAllowVoid, int* pTypeFlag, bool optional)
{
	//Handle elementType
	if (Accept('<'))
	{
		if (!AcceptType(false, pTextureState->type.elementType, pTextureState->type.typeName, pTypeFlag))
			return false;

		//multi sampling
		if (pTextureState->type.baseType == HLSLBaseType_Texture2DMS || pTextureState->type.baseType == HLSLBaseType_Texture2DMSArray)
		{
			if (Accept(','))
			{
				CachedString temp;
				int iValue;
				if (AcceptIdentifier(temp))
					pTextureState->sampleIdentifier = temp;
				else if (AcceptInt(iValue))
					pTextureState->sampleCount = iValue;
				else
					return false;
			}
		}

		if (!Expect('>'))
			return false;
		else
			return true;
	}
	else
		return optional;
}

bool HLSLParser::AcceptBufferType(HLSLBuffer* pBuffer)
{
	if (pBuffer)
	{
		switch (m_pFullTokenizer->GetToken())
		{
		case HLSLToken_CBuffer:
			pBuffer->type.baseType = HLSLBaseType_CBuffer; break;
		case HLSLToken_TBuffer:
			pBuffer->type.baseType = HLSLBaseType_TBuffer; break;
		case HLSLToken_ConstantBuffer:
			pBuffer->type.baseType = HLSLBaseType_ConstantBuffer; break;
		case HLSLToken_StructuredBuffer:
			pBuffer->type.baseType = HLSLBaseType_StructuredBuffer; break;
		case HLSLToken_PureBuffer:
			pBuffer->type.baseType = HLSLBaseType_PureBuffer; break;
		case HLSLToken_RWBuffer:
			pBuffer->type.baseType = HLSLBaseType_RWBuffer; break;
		case HLSLToken_RWStructuredBuffer:
			pBuffer->type.baseType = HLSLBaseType_RWStructuredBuffer; break;
		case HLSLToken_ByteAddressBuffer:
			pBuffer->type.baseType = HLSLBaseType_ByteAddressBuffer; break;
		case HLSLToken_RWByteAddressBuffer:
			pBuffer->type.baseType = HLSLBaseType_RWByteAddressBuffer; break;

		default:
			return false;
			break;
		}
	}

	m_pFullTokenizer->Next();

	return true;
}

bool HLSLParser::AcceptTextureType(HLSLTextureState* pTextureState)
{
	if (pTextureState)
	{
		switch (m_pFullTokenizer->GetToken())
		{
		case HLSLToken_Texture1D:
			pTextureState->type.baseType = HLSLBaseType_Texture1D; break;
		case HLSLToken_Texture1DArray:
			pTextureState->type.baseType = HLSLBaseType_Texture1DArray; break;
		case HLSLToken_Texture2D:
			pTextureState->type.baseType = HLSLBaseType_Texture2D; break;
		case HLSLToken_Texture2DArray:
			pTextureState->type.baseType = HLSLBaseType_Texture2DArray; break;
		case HLSLToken_Texture3D:
			pTextureState->type.baseType = HLSLBaseType_Texture3D; break;
		case HLSLToken_Texture2DMS:
			pTextureState->type.baseType = HLSLBaseType_Texture2DMS; break;
		case HLSLToken_Texture2DMSArray:
			pTextureState->type.baseType = HLSLBaseType_Texture2DMSArray; break;
		case HLSLToken_TextureCube:
			pTextureState->type.baseType = HLSLBaseType_TextureCube; break;
		case HLSLToken_TextureCubeArray:
			pTextureState->type.baseType = HLSLBaseType_TextureCubeArray; break;
		case HLSLToken_RWTexture1D:
			pTextureState->type.baseType = HLSLBaseType_RWTexture1D; break;
		case HLSLToken_RWTexture1DArray:
			pTextureState->type.baseType = HLSLBaseType_RWTexture1DArray; break;
		case HLSLToken_RWTexture2D:
			pTextureState->type.baseType = HLSLBaseType_RWTexture2D; break;
		case HLSLToken_RWTexture2DArray:
			pTextureState->type.baseType = HLSLBaseType_RWTexture2DArray; break;
		case HLSLToken_RWTexture3D:
			pTextureState->type.baseType = HLSLBaseType_RWTexture3D; break;
		case HLSLToken_RasterizerOrderedTexture1D:
			pTextureState->type.baseType = HLSLBaseType_RasterizerOrderedTexture1D; break;
		case HLSLToken_RasterizerOrderedTexture1DArray:
			pTextureState->type.baseType = HLSLBaseType_RasterizerOrderedTexture1DArray; break;
		case HLSLToken_RasterizerOrderedTexture2D:
			pTextureState->type.baseType = HLSLBaseType_RasterizerOrderedTexture2D; break;
		case HLSLToken_RasterizerOrderedTexture2DArray:
			pTextureState->type.baseType = HLSLBaseType_RasterizerOrderedTexture2DArray; break;
		case HLSLToken_RasterizerOrderedTexture3D:
			pTextureState->type.baseType = HLSLBaseType_RasterizerOrderedTexture3D; break;
		default:
			return false;
			break;
		}
	}

	m_pFullTokenizer->Next();

	return true;
}
