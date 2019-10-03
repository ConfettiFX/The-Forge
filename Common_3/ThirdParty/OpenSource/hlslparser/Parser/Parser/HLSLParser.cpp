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

void HLSLParser::Intrinsic::AllocArgs(int num)
{
	fullFunction.args.resize(num);
	for (int i = 0; i < num; i++)
	{
		fullFunction.args[i] = &argument[i];
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
	argument[0].type.baseType = arg1;
	argument[0].type.flags = HLSLTypeFlag_Const;
}
HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(2);
	argument[0].type.baseType = arg1;
	argument[0].type.flags = HLSLTypeFlag_Const;
	argument[1].type.baseType = arg2;
	argument[1].type.flags = HLSLTypeFlag_Const;
}
HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(3);
	argument[0].type.baseType = arg1;
	argument[0].type.flags = HLSLTypeFlag_Const;
	argument[1].type.baseType = arg2;
	argument[1].type.flags = HLSLTypeFlag_Const;
	argument[2].type.baseType = arg3;
	argument[2].type.flags = HLSLTypeFlag_Const;
}
HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(4);
	argument[0].type.baseType = arg1;
	argument[0].type.flags = HLSLTypeFlag_Const;
	argument[1].type.baseType = arg2;
	argument[1].type.flags = HLSLTypeFlag_Const;
	argument[2].type.baseType = arg3;
	argument[2].type.flags = HLSLTypeFlag_Const;
	argument[3].type.baseType = arg4;
	argument[3].type.flags = HLSLTypeFlag_Const;
}

HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4, HLSLBaseType arg5)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(5);
	argument[0].type.baseType = arg1;
	argument[0].type.flags = HLSLTypeFlag_Const;
	argument[1].type.baseType = arg2;
	argument[1].type.flags = HLSLTypeFlag_Const;
	argument[2].type.baseType = arg3;
	argument[2].type.flags = HLSLTypeFlag_Const;
	argument[3].type.baseType = arg4;
	argument[3].type.flags = HLSLTypeFlag_Const;
	argument[4].type.baseType = arg5;
	argument[4].type.flags = HLSLTypeFlag_Const;
}

HLSLParser::Intrinsic::Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4, HLSLBaseType arg5, HLSLBaseType arg6)
{
	rawName = name;
	fullFunction.name = MakeCached(stringLibrary, name);
	fullFunction.returnType.baseType = returnType;
	AllocArgs(5);
	argument[0].type.baseType = arg1;
	argument[0].type.flags = HLSLTypeFlag_Const;
	argument[1].type.baseType = arg2;
	argument[1].type.flags = HLSLTypeFlag_Const;
	argument[2].type.baseType = arg3;
	argument[2].type.flags = HLSLTypeFlag_Const;
	argument[3].type.baseType = arg4;
	argument[3].type.flags = HLSLTypeFlag_Const;
	argument[4].type.baseType = arg5;
	argument[4].type.flags = HLSLTypeFlag_Const;
	argument[5].type.baseType = arg6;
	argument[5].type.flags = HLSLTypeFlag_Const;
}
/*
static HLSLParser::Intrinsic * MakeIntrinsicInOut(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2)
{
	HLSLParser::Intrinsic * pIntrinsic = new HLSLParser::Intrinsic(stringLibrary,name,returnType,arg1,arg2);
	for (int i = 0; i < pIntrinsic->fullFunction.argumentVec.size(); i++)
	{
		pIntrinsic->fullFunction.argumentVec[i]->type.flags = (HLSLTypeFlag_Input | HLSLTypeFlag_Output);
	}
}
*/

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

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Float, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Float, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Float, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Half, HLSLBaseType_Half));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Half, HLSLBaseType_Half2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Half, HLSLBaseType_Half3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Half, HLSLBaseType_Half4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min16Float, HLSLBaseType_Min16Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "length", HLSLBaseType_Min10Float, HLSLBaseType_Min10Float4));

	//Buffer.Load
#define BUFFER_LOAD_FUNC(buffer, type_prefix)																							\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix,      HLSLBaseType_##buffer, HLSLBaseType_Int));		\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##1x2, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##1x3, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##1x4, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##2,   HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##2x2, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##2x3, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##2x4, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##3,   HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##3x2, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##3x3, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##3x4, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##4,   HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##4x2, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##4x2, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##4x3, HLSLBaseType_##buffer, HLSLBaseType_Int));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_##type_prefix##4x4, HLSLBaseType_##buffer, HLSLBaseType_Int));

	BUFFER_LOAD_FUNC(RWBuffer, Float);
	BUFFER_LOAD_FUNC(RWBuffer, Half);
	BUFFER_LOAD_FUNC(RWBuffer, Min16Float);
	BUFFER_LOAD_FUNC(RWBuffer, Min10Float);
	BUFFER_LOAD_FUNC(RWBuffer, Int);
	BUFFER_LOAD_FUNC(RWBuffer, Uint);

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Uint, HLSLBaseType_ByteAddressBuffer, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load2", HLSLBaseType_Uint2, HLSLBaseType_ByteAddressBuffer, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load3", HLSLBaseType_Uint3, HLSLBaseType_ByteAddressBuffer, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load4", HLSLBaseType_Uint4, HLSLBaseType_ByteAddressBuffer, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Uint, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load2", HLSLBaseType_Uint2, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load3", HLSLBaseType_Uint3, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load4", HLSLBaseType_Uint4, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int));

#define BUFFER_STORE_FUNC(buffer, type_prefix)																																\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix));		\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##1x2));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##1x3));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##1x4));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##2));		\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##2x2));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##2x3));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##2x4));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##3));		\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##3x2));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##3x3));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##3x4));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##4));		\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##4x2));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##4x2));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##4x3));	\
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_##buffer, HLSLBaseType_Int, HLSLBaseType_##type_prefix##4x4));

	BUFFER_STORE_FUNC(RWBuffer, Float);
	BUFFER_STORE_FUNC(RWBuffer, Half);
	BUFFER_STORE_FUNC(RWBuffer, Min16Float);
	BUFFER_STORE_FUNC(RWBuffer, Min10Float);
	BUFFER_STORE_FUNC(RWBuffer, Int);
	BUFFER_STORE_FUNC(RWBuffer, Uint);

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store", HLSLBaseType_Void, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int, HLSLBaseType_Uint));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store2", HLSLBaseType_Void, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int, HLSLBaseType_Uint2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store3", HLSLBaseType_Void, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int, HLSLBaseType_Uint3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Store4", HLSLBaseType_Void, HLSLBaseType_RWByteAddressBuffer, HLSLBaseType_Int, HLSLBaseType_Uint4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_StructuredBuffer, HLSLBaseType_UserDefined, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_RWStructuredBuffer, HLSLBaseType_UserDefined, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_UserDefined, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_UserDefined, HLSLBaseType_Int));

	//Texture.Load
	//Texture1D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4, HLSLBaseType_Texture1D, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3, HLSLBaseType_Texture1D, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4, HLSLBaseType_Texture1D, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint2, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint3, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Uint4, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int2, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int3, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Int4, HLSLBaseType_Texture1D, HLSLBaseType_Int2, HLSLBaseType_Int));


	//Texture2DMS
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	//Texture 2D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2D, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_Int3, HLSLBaseType_Int2));


	//Texture2DMS Texture2DMSArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DMS, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int2, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Int3, HLSLBaseType_Int, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_Int4, HLSLBaseType_Int3));

	//Texture3D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture3D, HLSLBaseType_Int4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture3D, HLSLBaseType_Int4));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float2, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"Load", HLSLBaseType_Float3, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half2, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half3, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float2, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float3, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float2, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float3, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Load", HLSLBaseType_Min10Float4, HLSLBaseType_Texture3D, HLSLBaseType_Int4, HLSLBaseType_Int3));

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

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "asuint", HLSLBaseType_Uint, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2D", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dproj", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dlod", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dlod", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4, HLSLBaseType_Int2));   // With offset.
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dbias", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dgrad", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dgather", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dgather", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Float2, HLSLBaseType_Int2, HLSLBaseType_Int));    // With offset.
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dsize", HLSLBaseType_Int2, HLSLBaseType_Sampler2D));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dfetch", HLSLBaseType_Float4, HLSLBaseType_Sampler2D, HLSLBaseType_Int3));    // u,v,mipmap

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2Dcmp", HLSLBaseType_Float4, HLSLBaseType_Sampler2DShadow, HLSLBaseType_Float4));                // @@ IC: This really takes a float3 (uvz) and returns a float.

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2DMSfetch", HLSLBaseType_Float4, HLSLBaseType_Sampler2DMS, HLSLBaseType_Int2, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2DMSsize", HLSLBaseType_Int3, HLSLBaseType_Sampler2DMS));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex2DArray", HLSLBaseType_Float4, HLSLBaseType_Sampler2DArray, HLSLBaseType_Float3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex3D", HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex3Dlod", HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex3Dbias", HLSLBaseType_Float4, HLSLBaseType_Sampler3D, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "tex3Dsize", HLSLBaseType_Int3, HLSLBaseType_Sampler3D));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Half4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Sample", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Sample", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"Sample", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "Sample", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4, HLSLBaseType_Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4, HLSLBaseType_Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4, HLSLBaseType_Float));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_Float4, HLSLBaseType_Float));

	//Offset

	//Texture1D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	//Texture3D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	//TextureCube
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	//TextureCubeArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Half4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleLevel", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	//Texture1D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture1D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture1D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture2D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture2D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	//TextureCube
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_TextureCube, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_TextureCube, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	//TextureCubeArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmp", HLSLBaseType_Float, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	//Texture1D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture1D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture1D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture2D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture2D, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int2));

	//TextureCube
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_TextureCube, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_TextureCube, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int3));

	//TextureCubeArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleCmpLevelZero", HLSLBaseType_Float, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerComparisonState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Half4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float, HLSLBaseType_Int));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleBias", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1D, HLSLBaseType_SamplerState, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture1DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture1DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float, HLSLBaseType_Float, HLSLBaseType_Int));

	//Texture2D
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	//Texture2DArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float2, HLSLBaseType_Float2, HLSLBaseType_Int2));

	//Texture3D
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Int3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_Texture3D, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Int3));

	//TextureCube
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCube, HLSLBaseType_SamplerState, HLSLBaseType_Float3, HLSLBaseType_Float3, HLSLBaseType_Float3));

	//TextureCubeArray
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Half4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min16Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "SampleGrad", HLSLBaseType_Min10Float4, HLSLBaseType_TextureCubeArray, HLSLBaseType_SamplerState, HLSLBaseType_Float4, HLSLBaseType_Float3, HLSLBaseType_Float3));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherRed", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"GatherGreen", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary,"GatherGreen", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherGreen", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherBlue", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Half4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2D, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Half4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min16Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2));
	m_intrinsics.push_back(new Intrinsic(m_intrinsicStringLibrary, "GatherAlpha", HLSLBaseType_Min10Float4, HLSLBaseType_Texture2DArray, HLSLBaseType_SamplerState, HLSLBaseType_Float2, HLSLBaseType_Int2));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBE", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float3));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBElod", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBEbias", HLSLBaseType_Float4, HLSLBaseType_SamplerCube, HLSLBaseType_Float4));
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"texCUBEsize", HLSLBaseType_Int, HLSLBaseType_SamplerCube));

	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Texture1D, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Texture1DArray, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Texture2D, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Texture2DArray, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Texture3D, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Texture2DMS, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_Texture2DMSArray, HLSLBaseType_Uint, HLSLBaseType_Uint,HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[4].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_RasterizerOrderedTexture1D, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_RasterizerOrderedTexture1DArray, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_RasterizerOrderedTexture2D, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_RasterizerOrderedTexture2DArray, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.push_back(new Intrinsic( m_intrinsicStringLibrary,"GetDimensions", HLSLBaseType_Void, HLSLBaseType_RasterizerOrderedTexture3D, HLSLBaseType_Uint, HLSLBaseType_Uint, HLSLBaseType_Uint));
	m_intrinsics.back()->argument[1].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[2].modifier = HLSLArgumentModifier_Out;
	m_intrinsics.back()->argument[3].modifier = HLSLArgumentModifier_Out;

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
			return m_tree->AddStringCached(BASE_TYPE_DESC[type.elementType].typeName);
		}
	}
	else
	{
		return m_tree->AddStringCached(BASE_TYPE_DESC[type.baseType].typeName);
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

	//TODO add array matching

	comparingType = srcType.baseType;
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

	const BaseTypeDescription& srcDesc = BASE_TYPE_DESC[comparingType];
	const BaseTypeDescription& dstDesc = BASE_TYPE_DESC[comparedType];
	if (srcDesc.numericType == NumericType_NaN || dstDesc.numericType == NumericType_NaN)
	{
		return -1;
	}

	// Result bits: T R R R P (T = truncation, R = conversion rank, P = dimension promotion)
	int result = _numberTypeRank[srcDesc.numericType][dstDesc.numericType] << 1;

	if ((srcDesc.numComponents == 1 && srcDesc.numRows == 1) && (dstDesc.numComponents > 1 || dstDesc.numRows > 1))
	{
		// Scalar dimension promotion
		result |= (1 << 0);
	}
	else if ((srcDesc.numComponents > dstDesc.numComponents && srcDesc.numRows >= dstDesc.numRows) ||
		     (srcDesc.numComponents >= dstDesc.numComponents && srcDesc.numRows > dstDesc.numRows))
	{
		// Truncation
		result |= (1 << 4);
	}
	else if (srcDesc.numComponents != dstDesc.numComponents || srcDesc.numRows != dstDesc.numRows)
	{
		return -1;
	}
	
	
	return result;
	
}

static bool AreTypesEqual(HLSLTree* tree, const HLSLType& lhs, const HLSLType& rhs)
{
	return GetTypeCastRank(tree, lhs, rhs) == 0;
}

static bool GetFunctionCallCastRanks(HLSLTree* tree, const HLSLFunctionCall* call, const HLSLFunction* function, int* rankBuffer)
{
	// Note theat the arguments do not need to be the same, because of default parameters.
	if (function == NULL || function->args.size() < call->params.size())
	{
		// Function not viable
		return false;
	}

	const eastl::vector<HLSLExpression*>& expressionVec = call->params;
	const eastl::vector<HLSLArgument*>& argumentVec = function->args;
	ASSERT_PARSER(expressionVec.size() <= argumentVec.size());

	for (int i = 0; i < expressionVec.size(); ++i)
	{
		HLSLExpression* expression = expressionVec[i];// call->callArgument;
		HLSLType expType = expression->expressionType;

		const HLSLArgument* argument = argumentVec[i];

		int rank = GetTypeCastRank(tree, expType, argument->type);
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

	int* function1Ranks = static_cast<int*>(alloca(sizeof(int) * call->params.size()));
	int* function2Ranks = static_cast<int*>(alloca(sizeof(int) * call->params.size()));

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

	std::sort(function1Ranks, function1Ranks + call->params.size(), CompareRanks());
	std::sort(function2Ranks, function2Ranks + call->params.size(), CompareRanks());
	
	for (int i = 0; i < call->params.size(); ++i)
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

bool HLSLParser::GetBinaryOpResultType(HLSLBinaryOp binaryOp, const HLSLType& type1, const HLSLType& type2, HLSLType& argType, HLSLType& resType)
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

	argType.baseType = _binaryOpTypeLookup[type1.baseType - HLSLBaseType_FirstNumeric][type2.baseType - HLSLBaseType_FirstNumeric];
	resType.array =false;
	resType.flags = 0;

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
			int numComponents = std::max( BASE_TYPE_DESC[ type1.baseType ].numComponents, BASE_TYPE_DESC[ type2.baseType ].numComponents );
			resType.baseType = HLSLBaseType( HLSLBaseType_Bool + numComponents - 1 );
			break;
		}
	default:
		resType.baseType = _binaryOpTypeLookup[type1.baseType - HLSLBaseType_FirstNumeric][type2.baseType - HLSLBaseType_FirstNumeric];
		break;
	}

	resType.typeName.Reset();
	resType.array        = false;
	resType.flags        = (type1.flags & type2.flags) & HLSLTypeFlag_Const; // Propagate constness.
	

	return resType.baseType != HLSLBaseType_Unknown;

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

			// 02. get Identifier
			// 03. get assigned register (necessary)
			if (!ExpectIdentifier(buffer->name) || !ParseRegisterAssignment(buffer, "Constant Buffer (register b)"))
				return false;
			
			// 04. get Body (optional)
			GetBufferBody(buffer);

			// If it is a Hull shader for MSL, hide it to avoid to print same duplicated one
			if (FindBuffer(buffer->name) != NULL)
			{
				if (m_target == Parser::Target_HullShader && m_language == Parser::Language_MSL)
					buffer->hidden = true;
			}

			// Confetti's Rule : if buffer name includes "rootconstant", it is constant buffer
			if(stristr(GetCstr(buffer->name), "rootconstant"))
				buffer->bPushConstant = true;

			DeclareVariable(buffer);

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
			// 03. get array (optional)
			// 04. get assigned register (necessary)
			if (!ExpectIdentifier(buffer->name) || !ParseArrayExpression(buffer->type, buffer->arrayDimExpression) || !ParseRegisterAssignment(buffer, "RWBuffer (register u)"))
				return false;
			
			DeclareVariable(buffer);
		}
		else if (buffer->type.baseType == HLSLBaseType_StructuredBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedStructuredBuffer)
		{
			// 01. get element type (necessary) and base type
			if (!GetBufferElementType(buffer, false, &typeFlags, false))
				return false;

			// 02. get Identifier
			// 03. get array (optional)
			// 04. get assigned register (necessary)
			if (!ExpectIdentifier(buffer->name) || !ParseArrayExpression(buffer->type, buffer->arrayDimExpression) || !ParseRegisterAssignment(buffer, "Structure Buffer (register t)"))
				return false;

			DeclareVariable(buffer);
		}
		else if (buffer->type.baseType == HLSLBaseType_PureBuffer)
		{
			// 01. get element type (necessary) and base type
			if (!GetBufferElementType(buffer, false, &typeFlags, false))
				return false;

			// 02. get Identifier			
			// 03. get array (optional)
			// 04. get assigned register (necessary)
			if (!ExpectIdentifier(buffer->name) || !ParseArrayExpression(buffer->type, buffer->arrayDimExpression) || !ParseRegisterAssignment(buffer, "Buffer (register t)"))
				return false;

			DeclareVariable(buffer);
		}
		else if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer)
		{
			// 01. get element type (necessary) and base type
			if (!GetBufferElementType(buffer, false, &typeFlags, false))
				return false;

			// 02. get Identifier			
			// 03. get array (optional)
			// 04. get assigned register (necessary)
			if (!ExpectIdentifier(buffer->name) || !ParseArrayExpression(buffer->type, buffer->arrayDimExpression) || !ParseRegisterAssignment(buffer, "RWStructure Buffer (register u)"))
				return false;

			// 05. add it as a Global variable			

			DeclareVariable(buffer);
		}
		else if (buffer->type.baseType == HLSLBaseType_ByteAddressBuffer)
		{
			// 01. get Identifier
			// 02. get array (optional)
			// 03. get assigned register (necessary)
			if (!ExpectIdentifier(buffer->name) || !ParseArrayExpression(buffer->type, buffer->arrayDimExpression) || !ParseRegisterAssignment(buffer, "ByteAddress Buffer (register t)"))
				return false;

			//assume that ByteAddressBuffer is using for Int
			buffer->type.elementType = HLSLBaseType_Uint;

			DeclareVariable(buffer);
		}
		else if (buffer->type.baseType == HLSLBaseType_RWByteAddressBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedByteAddressBuffer)
		{
			// 01. get Identifier
			// 02. get array (optional)
			// 03. get assigned register (necessary)
			if (!ExpectIdentifier(buffer->name) || !ParseArrayExpression(buffer->type, buffer->arrayDimExpression) || !ParseRegisterAssignment(buffer, "ByteAddress Buffer (register t)"))
				return false;

			//assume that ByteAddressBuffer is using for Int
			buffer->type.elementType = HLSLBaseType_Uint;

			DeclareVariable(buffer);
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
		// 04. get array (optional)
		// 05. get register (necessary)
		if (!ExpectIdentifier(texturestate->name) || !ParseArrayExpression(texturestate->type, texturestate->arrayDimExpression) || !ParseRegisterAssignment(texturestate, "Texture (register t for Read only, register u for R/W)"))
			return false;

		DeclareVariable(texturestate);

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
	else if ((m_pFullTokenizer->GetToken() == HLSLToken_SamplerState) || (m_pFullTokenizer->GetToken() == HLSLToken_Sampler) || (m_pFullTokenizer->GetToken() == HLSLToken_SamplerComparisonState))
	{
		//SamplerState can be declared or be passed from outside
		HLSLSamplerState* samplerstate = m_tree->AddNode<HLSLSamplerState>(fileName, line);
		
		AcceptType(true, samplerstate->type.baseType, samplerstate->type.typeName, &samplerstate->type.flags);

		// SamplerState declaration.
		if (!ExpectIdentifier(samplerstate->name) || !ParseArrayExpression(samplerstate->type, samplerstate->arrayDimExpression) || !ParseRegisterAssignment(samplerstate, "Sampler (register s)"))
		{
			return false;
		}

		DeclareVariable(samplerstate);

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

			if (!ParseArguments(function->args))
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
			declaration->name = globalName;
			declaration->type.baseType   = type;
			declaration->type.flags      = typeFlags;
			if (type == HLSLBaseType_UserDefined)
				declaration->type.typeName = typeName;

			// Handle array syntax.
			if (!ParseArrayExpression(declaration->type, declaration->arrayDimExpression))
			{
				return false;
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

			DeclareVariable( declaration );

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
		if (!CheckTypeCast(returnType, returnStatement->expression ? returnStatement->expression->expressionType : voidType))
		{
			return false;
		}

		returnStatement->expression = OptionallyApplyImplicitCast(returnType, returnStatement->expression);

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
		HLSLExpression* arrayExpression = NULL;
		if (Accept('['))
		{
			type.array = true;
			// Optionally allow no size to the specified for the array.
			if (Accept(']') && allowUnsizedArray)
			{
				return true;
			}
			// the '[' resets priority
			if (!ParseExpression(arrayExpression,true,0) || !Expect(']'))
			{
				return false;
			}
		}

		if (arrayExpression && !m_tree->GetExpressionValue(arrayExpression, type.arrayExtent[0]))
		{
			return false;
		}

		HLSLDeclaration * declaration = m_tree->AddNode<HLSLDeclaration>(fileName, line);
		declaration->type  = type;
		declaration->type.array = int(arrayExpression != NULL);
		declaration->arrayDimExpression[0] = arrayExpression;
		declaration->name = name;

		DeclareVariable( declaration );

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
			HLSLInitListExpression* initListExpression = m_tree->AddNode<HLSLInitListExpression>(GetFileName(), GetLineNumber());
			if (!Expect('{') || !ParseExpressionList('}', true, true, initListExpression->initExpressions))
			{
				return false;
			}
			declaration->assignment = initListExpression;
			declaration->assignment->expressionType = declaration->type;
		}
		else if (Accept('{'))
		{
			// matrix's element initialization syntax.
			HLSLInitListExpression* initListExpression = m_tree->AddNode<HLSLInitListExpression>(GetFileName(), GetLineNumber());
			if (!ParseExpressionList('}', true, true, initListExpression->initExpressions))
			{
				return false;
			}
			declaration->assignment = initListExpression;
			declaration->assignment->expressionType = declaration->type;
		}
		else if (!ParseExpression(declaration->assignment,false,0))
		{
			return false;
		}
		declaration->assignment = OptionallyApplyImplicitCast(declaration->type, declaration->assignment);
	}
	return true;
}

bool HLSLParser::ParseFieldDeclaration(HLSLStructField*& field)
{
	field = m_tree->AddNode<HLSLStructField>( GetFileName(), GetLineNumber() );

	bool doesNotExpectSemicolon = false;

	if (!ExpectDeclaration(false, field->type, field->name, field->arrayDimExpression))
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

bool HLSLParser::CheckTypeCast(const HLSLType& dstType, const HLSLType& srcType)
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

		if (!CheckTypeCast(expression->expressionType, expression2->expressionType))
		{
			return false;
		}

		HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(expression->fileName, expression->line);
		binaryExpression->binaryOp = assignOp;
		binaryExpression->expression1 = expression;
		binaryExpression->expression2 = OptionallyApplyImplicitCast(expression->expressionType, expression2);
		// This type is not strictly correct, since the type should be a reference.
		// However, for our usage of the types it should be sufficient.
		binaryExpression->expressionType = expression->expressionType;

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
			else if (expression2->expressionType.baseType == HLSLBaseType_UserMacro)
			{
				exp2Type.baseType = HLSLBaseType_Uint;
			}

			HLSLType resType, argType;
			if (!GetBinaryOpResultType( binaryOp, exp1Type, exp2Type, argType, resType))
			{
				const char* typeName1 = GetCstr(GetTypeName( expression->expressionType ));
				const char* typeName2 = GetCstr(GetTypeName( expression2->expressionType ));

				// debug
				bool temp = GetBinaryOpResultType(binaryOp, exp1Type, exp2Type, argType, resType);

				m_pFullTokenizer->Error("binary '%s' : no global operator found which takes types '%s' and '%s' (or there is no acceptable conversion)",
					CHECK_CSTR(GetBinaryOpName(binaryOp)), CHECK_CSTR(typeName1), CHECK_CSTR(typeName2));
			
				return false;
			}

			HLSLBinaryExpression* binaryExpression = m_tree->AddNode<HLSLBinaryExpression>(fileName, line);
			binaryExpression->binaryOp    = binaryOp;
			binaryExpression->expressionType = resType;
			// TODO: should we handle case when arg types can be different.
			binaryExpression->expression1 = OptionallyApplyImplicitCast(argType, expression);
			binaryExpression->expression2 = OptionallyApplyImplicitCast(argType, expression2);

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
	const char* fileName = GetFileName();
	int         line     = GetLineNumber();

	HLSLConstructorExpression* constructorExpression = m_tree->AddNode<HLSLConstructorExpression>(fileName, line);
	constructorExpression->expressionType.baseType = type;
	constructorExpression->expressionType.typeName = typeName;
	if (!ParseExpressionList(')', false, false, constructorExpression->params))
	{
		return false;
	}
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

HLSLExpression* HLSLParser::OptionallyApplyImplicitCast(const HLSLType& dstType, HLSLExpression* expr)
{
	const char* fileName = GetFileName();
	int         line     = GetLineNumber();

	if (expr && !AreTypesEqual(m_tree, dstType, expr->expressionType))
	{
		HLSLCastingExpression* castExpr = m_tree->AddNode<HLSLCastingExpression>(fileName, line);
		castExpr->expressionType = dstType;
		castExpr->expression = expr;
		castExpr->implicit = true;
		return castExpr;
	}

	return expr;
}


bool HLSLParser::ParseFunctionCall(CachedString name, HLSLExpression* object, HLSLExpression*& expression)
{
	const char* fileName = GetFileName();
	int         line     = GetLineNumber();

	// Handle function calls. Note, HLSL functions aren't like C function
	// pointers -- we can only directly call on an identifier, not on an
	// expression.
	if (Accept('('))
	{
		HLSLFunctionCall* functionCall = m_tree->AddNode<HLSLFunctionCall>(fileName, line);
		if (object)
			functionCall->params.push_back(object);

		if (!ParseExpressionList(')', false, false, functionCall->params))
		{
			return false;
		}

		const HLSLFunction* function = MatchFunctionCall(functionCall, name);
		if (function == NULL)
		{
			return false;
		}

		functionCall->function = function;
		// Insert implicit casts as AST tree nodes in order to simplify backends
		for (size_t i = 0; i < functionCall->params.size(); ++i)
		{
			functionCall->params[i] = OptionallyApplyImplicitCast(function->args[i]->type, functionCall->params[i]);
		}

		// if it is special function for texture / buffer
		//TODO move to glsl backend
		if (String_Equal("Load", name) || String_Equal(name, "Store") || String_Equal(name, "GetDimensions"))
		{
			if (!functionCall->params.empty() && IsTextureType(functionCall->params[0]->expressionType.baseType))
			{
				m_tree->gExtension[USE_SAMPLESS] = true;
			}
		}
		//TODO: move to MSL backend
		//marking for MSL
		else if (String_Equal("InterlockedAdd", name) ||
			String_Equal("InterlockedCompareExchange", name) ||
			String_Equal("InterlockedCompareStore", name) ||
			String_Equal("InterlockedExchange", name) ||
			String_Equal("InterlockedMax", name) ||
			String_Equal("InterlockedMin", name) ||
			String_Equal("InterlockedOr", name) ||
			String_Equal("InterlockedXor", name))
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

		return true;
	}

	return false;
}

bool HLSLParser::ParseTerminalExpression(HLSLExpression*& expression, bool& needsEndParen, bool bPreprocessor)
{
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
		else if(expression == NULL)
		{
			//TODO: if expression is function then following allocation is not used

			CachedString name;
			if (!ExpectIdentifier(name))
			{
				return false;
			}

			HLSLDeclaration* declaration = FindVariable(name);
			if (declaration)
			{
				HLSLIdentifierExpression* identifierExpression = m_tree->AddNode<HLSLIdentifierExpression>(fileName, line);
				identifierExpression->pDeclaration = declaration;
				identifierExpression->expressionType = declaration->type;
				expression = identifierExpression;
			}
			else if (!GetIsFunction(name) || !ParseFunctionCall(name, NULL, expression))
			{
				m_pFullTokenizer->Error("Undeclared identifier '%s'", GetCstr(name));
				return false;
			}
		}
	}

	if (bPreprocessor)
		return true;

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
		

		// Member access or function call
		while (Accept('.'))
		{
			CachedString identifier;
			if (!ExpectIdentifier(identifier))
			{
				m_pFullTokenizer->Error("Expecting fidentifier");
				return false;
			}

			if (GetIsFunction(identifier))
			{
				if (!ParseFunctionCall(identifier, expression, expression))
				{
					return false;
				}
			}
			else
			{
				HLSLMemberAccess* memberAccess = m_tree->AddNode<HLSLMemberAccess>(fileName, line);
				memberAccess->object = expression;
				memberAccess->field = identifier;

				if (!GetMemberType(expression->expressionType, memberAccess))
				{
					m_pFullTokenizer->Error("Couldn't access '%s'", GetCstr(identifier));
					return false;
				}
				expression = memberAccess;
			}
			done = false;
		}

		// Handle array access.
		while (Accept('['))
		{
			HLSLArrayAccess* arrayAccess = m_tree->AddNode<HLSLArrayAccess>(fileName, line);
			arrayAccess->array = expression;
			// allow comma inside array access
			if (!ParseExpression(arrayAccess->index,false,0) || !Expect(']'))
			{
				return false;
			}

			if (expression->expressionType.array)
			{
				if (expression->nodeType == HLSLNodeType_IdentifierExpression)
				{
					//TODO: fix!!!!!!!!!!!!!!!!
					HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);

					//if it is buffer
					const HLSLBuffer* buffer = FindBuffer(identifierExpression->pDeclaration->name);

					if (buffer != NULL && buffer->type.arrayDimension > 0)
					{
						arrayAccess->identifier = identifierExpression->pDeclaration->name;
					}	
				}

				arrayAccess->expressionType = expression->expressionType;
				arrayAccess->expressionType.array = expression->expressionType.arrayDimension > 1;
				arrayAccess->expressionType.arrayDimension = expression->expressionType.arrayDimension > 0 ? expression->expressionType.arrayDimension - 1 : 0;
				arrayAccess->expressionType.arrayExtent[0] = expression->expressionType.arrayExtent[1];
				arrayAccess->expressionType.arrayExtent[1] = expression->expressionType.arrayExtent[2];
				arrayAccess->expressionType.arrayExtent[2] = 0;
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

				case HLSLBaseType_Texture1D:
				case HLSLBaseType_Texture1DArray:
				case HLSLBaseType_Texture2D:
				case HLSLBaseType_Texture2DArray:
				case HLSLBaseType_Texture3D:
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
					m_tree->gExtension[USE_SAMPLESS] = true;
					arrayAccess->expressionType.baseType = arrayAccess->array->expressionType.elementType;
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
						arrayAccess->expressionType.baseType = expression->expressionType.elementType;
						break;
					case HLSLBaseType_UserDefined:
						arrayAccess->expressionType.baseType = HLSLBaseType_UserDefined;
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
	}
	return true;

}

bool HLSLParser::ParseExpressionList(int endToken, bool allowEmptyEnd, bool initList, eastl::vector<HLSLExpression*>& expressions)
{
	int numExpressions = 0;
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

		// TODO: add type matching
		if (Accept('{'))
		{
			if (!initList)
			{
				return false;
			}

			HLSLInitListExpression* initListExpression = m_tree->AddNode<HLSLInitListExpression>(GetFileName(), GetLineNumber());

			ParseExpressionList('}', false, true, initListExpression->initExpressions);

			expression = initListExpression;
		}
		else if (!ParseExpression(expression,false,0))
		{
			return false;
		}

		expressions.push_back(expression);
		++numExpressions;
	}

	return true;
}

bool HLSLParser::ParseArguments(eastl::vector<HLSLArgument*>& argVec)
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

			if (!ExpectDeclaration(/*allowUnsizedArray=*/true, argument->type, argument->name, argument->arrayDimExpression))
			{
				return false;
			}

			DeclareVariable(argument);

			// Optional semantic.
			if (Accept(':') && !ExpectIdentifier(argument->semantic))
			{
				return false;
			}

			if (Accept('=') && !ParseExpression(argument->defaultValue, false, 0))
			{
				// @@ Print error!
				return false;
			}
		}

		argVec.push_back(argument);
	}

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
	case HLSLToken_Sampler:
	case HLSLToken_Sampler2D:
	case HLSLToken_Sampler3D:
	case HLSLToken_SamplerCube:
	case HLSLToken_Sampler2DMS:
	case HLSLToken_Sampler2DArray:
	case HLSLToken_SamplerState:
		type = HLSLBaseType_SamplerState;
		break;
	case HLSLToken_Sampler2DShadow:
	case HLSLToken_SamplerComparisonState:
		type = HLSLBaseType_SamplerComparisonState;
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

bool HLSLParser::AcceptDeclaration(bool allowUnsizedArray, HLSLType& type, CachedString& name, HLSLExpression* (&arrayDimExpressions)[MAX_DIM])
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
	if (!ParseArrayExpression(type, arrayDimExpressions))
	{
		return false;
	}
	return true;
}

bool HLSLParser::ExpectDeclaration(bool allowUnsizedArray, HLSLType& type, CachedString & name, HLSLExpression* (&arrayDimExpressions)[MAX_DIM])
{
	if (!AcceptDeclaration(allowUnsizedArray, type, name, arrayDimExpressions))
	{
		m_pFullTokenizer->Error("Expected declaration");
		return false;
	}
	return true;
}

HLSLBuffer* HLSLParser::FindBuffer(const CachedString & name) const
{
	for (int i = 0; i < m_Buffers.size(); ++i)
	{
		if (String_Equal(m_Buffers[i]->name, name))
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
		if (String_Equal(m_Buffers[i]->name, name) && m_Buffers[i]->type.baseType == HLSLBaseType_ConstantBuffer)
		{
			return m_Buffers[i];
		}
	}
	return NULL;
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
}

void HLSLParser::EndScope()
{
	ASSERT_PARSER(!m_variableScopes.empty());
	m_variables.resize(m_variableScopes.back());
	m_variableScopes.pop_back();
}

HLSLDeclaration* HLSLParser::FindVariable(const CachedString & name) const
{
	for (int i = int(m_variables.size()) - 1; i >= 0; --i)
	{
		if (String_Equal(m_variables[i]->name,name))
		{
			return m_variables[i];
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

static bool AreArgumentListsEqualVec(HLSLTree* tree, const eastl::vector<HLSLArgument*> & lhsVec, const eastl::vector<HLSLArgument*> & rhsVec)
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

		if (!AreTypesEqual(tree, lhs->type, rhs->type))
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
			AreArgumentListsEqualVec(m_tree, m_functions[i]->args, fun->args))
		{
			return m_functions[i];
		}
	}
	return NULL;
}

void HLSLParser::DeclareVariable(HLSLDeclaration* pDeclaration)
{
	pDeclaration->global = m_variableScopes.empty();
	m_variables.push_back(pDeclaration);
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
	size_t intrinsicId = m_intrinsicHelper->m_intrinsics.size();
	for (size_t i = 0; i < m_intrinsicHelper->m_intrinsics.size(); i++)
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
			else if (result == FunctionsEqual && matchedFunction)
			{
				++numMatchedOverloads;
			}
		}
	}
	
	// that function is owned by a different memory allocator, so create a new function
	if (intrinsicId < m_intrinsicHelper->m_intrinsics.size())
	{
		int line = GetLineNumber();
		const char* fileName = GetFileName();

		HLSLFunction* buildFunc = m_tree->AddNode<HLSLFunction>(fileName, line);

		const Intrinsic * intrinsic = m_intrinsicHelper->m_intrinsics[intrinsicId];

		buildFunc->name = m_tree->AddStringCached(intrinsic->rawName);
		buildFunc->returnType.baseType = intrinsic->fullFunction.returnType.baseType;
		
		for (int i = 0; i < intrinsic->fullFunction.args.size(); i++)
		{
			HLSLArgument* arg = m_tree->AddNode<HLSLArgument>(fileName,line);

			arg->type.baseType = intrinsic->fullFunction.args[i]->type.baseType;
			arg->type.flags = intrinsic->fullFunction.args[i]->type.flags;

			buildFunc->args.push_back(arg);
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

	if (comparingType == HLSLBaseType_UserDefined || comparingType == HLSLBaseType_InputPatch || comparingType == HLSLBaseType_OutputPatch)
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
			bool needsEndParen;
			if (!ParseTerminalExpression(memberAccess->functionExpression, needsEndParen, false))
				return false;

			return true;
		}
		else if (String_Equal(fieldName, "RestartStrip"))
		{
			bool needsEndParen;
			if (!ParseTerminalExpression(memberAccess->functionExpression, needsEndParen, false))
				return false;

			return true;
		}
	}
	if (BASE_TYPE_DESC[comparingType].numericType == NumericType_NaN)
	{
		// Currently we don't have an non-numeric types that allow member access.
		return false;
	}

	int swizzleLength = 0;

	if (BASE_TYPE_DESC[comparingType].numRows == 1)
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
			if (r >= BASE_TYPE_DESC[comparingType].numRows ||
				c >= BASE_TYPE_DESC[comparingType].numComponents)
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
	

	switch (BASE_TYPE_DESC[comparingType].numericType)
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
		return AcceptType(false, pBuffer->type.elementType, pBuffer->type.typeName, pTypeFlag) && Expect('>');
	}
	else
	{
		return optional;
	}
}

bool IsRegisterId(CachedString str, char& type, int& index)
{
	return sscanf(RawStr(str), "%c%d", &type, &index) == 2;
}

bool IsSpaceId(CachedString str, int& index)
{
	return sscanf(RawStr(str), "space%d", &index) == 1;
}

bool HLSLParser::ParseRegisterAssignment(HLSLDeclaration* pDeclaration, const char* errorMsg)
{
	char regType;
	int index;
	if (Accept(':'))
	{
		CachedString id;
		if (!Expect(HLSLToken_Register) || !Expect('(') || !ExpectIdentifier(id))
		{
			return false;
		}

		if (IsRegisterId(id, regType, index))
		{
			pDeclaration->registerName = id;
			pDeclaration->registerType = regType;
			pDeclaration->registerIndex = index;
			// if there is space
			if (Accept(','))
			{
				if (!ExpectIdentifier(id) || !IsSpaceId(id, index))
				{
					return false;
				}
				pDeclaration->registerSpaceName = id;
				pDeclaration->registerSpace = index;
			}
			else
			{
				// we need to make a distinction between a space specified by the user, vs
				// no space set, so that the user can specify the default space in the
				// code generation/tree traversal phase, as opposed to the parsing phase.
				pDeclaration->registerSpaceName = m_tree->AddStringCached("none");
			}
		}
		else if(IsSpaceId(id, index))
		{
			pDeclaration->registerName = m_tree->AddStringCached("none");
			pDeclaration->registerSpaceName = id;
			pDeclaration->registerSpace = index;
		}
		else
		{
			return false;
		}

		return Expect(')');
	}
	else
	{
		// allow default register placement
		pDeclaration->registerName = m_tree->AddStringCached("none");
		pDeclaration->registerSpaceName = m_tree->AddStringCached("none");

		return true;
	}

}


bool HLSLParser::GetBufferBody(HLSLBuffer* pBuffer)
{
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

bool HLSLParser::ParseArrayExpression(HLSLType& type, HLSLExpression* (&arrayDimExpression)[MAX_DIM])
{
	// Handle array syntax
	int i = 0;
	while (Accept('['))
	{
		if (Accept(']')) //unboundedSize
		{
			arrayDimExpression[i] = NULL;
		}
		else if (ParseExpression(arrayDimExpression[i], true,0) && Expect(']'))
		{
			int extent = 0;
			if (!m_tree->GetExpressionValue(arrayDimExpression[i], extent))
				return false;
			type.arrayExtent[i] = extent;
		}
		else
		{
			return false;
		}
		++i;
	}
	type.arrayDimension = i;
	type.array = i > 0;
	return true;
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
	{
		pTextureState->type.elementType = HLSLBaseType_Float4;
		return optional;
	}
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
