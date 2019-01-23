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

//#include "Engine/String.h"
//#include "Engine/Log.h"
#include "Engine.h"

#include <stdarg.h>
#include <string.h>

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

static const char* GetTypeName(const HLSLType& type)
{
	HLSLBaseType baseType = type.baseType;

	if (type.baseType == HLSLBaseType_Unknown)
		baseType = type.elementType;

    switch (baseType)
    {
	case HLSLBaseType_Void:         return "void";
	case HLSLBaseType_Float:        return "float";
	case HLSLBaseType_Float1x2:        return "mat1x2";
	case HLSLBaseType_Float1x3:        return "mat1x3";
	case HLSLBaseType_Float1x4:        return "mat1x4";
	case HLSLBaseType_Float2:       return "vec2";
	case HLSLBaseType_Float2x2:        return "mat2";
	case HLSLBaseType_Float2x3:        return "mat2x3";
	case HLSLBaseType_Float2x4:        return "mat2x4";
	case HLSLBaseType_Float3:       return "vec3";
	case HLSLBaseType_Float3x2:        return "mat3x2";
	case HLSLBaseType_Float3x3:        return "mat3";
	case HLSLBaseType_Float3x4:        return "mat3x4";
	case HLSLBaseType_Float4:       return "vec4";
	case HLSLBaseType_Float4x2:        return "mat4x2";
	case HLSLBaseType_Float4x3:        return "mat4x3";
	case HLSLBaseType_Float4x4:        return "mat4";

	case HLSLBaseType_Half:         return "float";
	case HLSLBaseType_Half1x2:        return "mat1x2";
	case HLSLBaseType_Half1x3:        return "mat1x3";
	case HLSLBaseType_Half1x4:        return "mat1x4";
	case HLSLBaseType_Half2:       return "vec2";
	case HLSLBaseType_Half2x2:        return "mat2";
	case HLSLBaseType_Half2x3:        return "mat2x3";
	case HLSLBaseType_Half2x4:        return "mat2x4";
	case HLSLBaseType_Half3:       return "vec3";
	case HLSLBaseType_Half3x2:        return "mat3x2";
	case HLSLBaseType_Half3x3:        return "mat3";
	case HLSLBaseType_Half3x4:        return "mat3x4";
	case HLSLBaseType_Half4:       return "vec4";
	case HLSLBaseType_Half4x2:        return "mat4x2";
	case HLSLBaseType_Half4x3:        return "mat4x3";
	case HLSLBaseType_Half4x4:        return "mat4";

	case HLSLBaseType_Min16Float:        return "min16float";
	case HLSLBaseType_Min16Float1x2:        return "min16float1x2";
	case HLSLBaseType_Min16Float1x3:        return "min16float1x3";
	case HLSLBaseType_Min16Float1x4:        return "min16float1x4";
	case HLSLBaseType_Min16Float2:       return "min16float2";
	case HLSLBaseType_Min16Float2x2:        return "min16float2x2";
	case HLSLBaseType_Min16Float2x3:        return "min16float2x3";
	case HLSLBaseType_Min16Float2x4:        return "min16float2x4";
	case HLSLBaseType_Min16Float3:       return "min16float3";
	case HLSLBaseType_Min16Float3x2:        return "min16float3x2";
	case HLSLBaseType_Min16Float3x3:        return "min16float3x3";
	case HLSLBaseType_Min16Float3x4:        return "min16float3x4";
	case HLSLBaseType_Min16Float4:       return "min16float4";
	case HLSLBaseType_Min16Float4x2:        return "min16float4x2";
	case HLSLBaseType_Min16Float4x3:        return "min16float4x3";
	case HLSLBaseType_Min16Float4x4:        return "min16float4x4";

	case HLSLBaseType_Min10Float:        return "min10float";
	case HLSLBaseType_Min10Float1x2:        return "min10float1x2";
	case HLSLBaseType_Min10Float1x3:        return "min10float1x3";
	case HLSLBaseType_Min10Float1x4:        return "min10float1x4";
	case HLSLBaseType_Min10Float2:       return "min10float2";
	case HLSLBaseType_Min10Float2x2:        return "min10float2x2";
	case HLSLBaseType_Min10Float2x3:        return "min10float2x3";
	case HLSLBaseType_Min10Float2x4:        return "min10float2x4";
	case HLSLBaseType_Min10Float3:       return "min10float3";
	case HLSLBaseType_Min10Float3x2:        return "min10float3x2";
	case HLSLBaseType_Min10Float3x3:        return "min10float3x3";
	case HLSLBaseType_Min10Float3x4:        return "min10float3x4";
	case HLSLBaseType_Min10Float4:       return "min10float4";
	case HLSLBaseType_Min10Float4x2:        return "min10float4x2";
	case HLSLBaseType_Min10Float4x3:        return "min10float4x3";
	case HLSLBaseType_Min10Float4x4:        return "min10float4x4";

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

	case HLSLBaseType_InputPatch:     return type.InputPatchName;
	case HLSLBaseType_OutputPatch:     return type.OutputPatchName;

	case HLSLBaseType_TriangleStream:	return type.structuredTypeName;

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
    case HLSLBaseType_UserDefined:  return type.typeName;
    default: return "?";
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

static int GetFunctionArguments(HLSLFunctionCall* functionCall, HLSLExpression* expression[], int maxArguments)
{
    HLSLExpression* argument = functionCall->argument;
    int numArguments = 0;
    while (argument != NULL)
    {
        if (numArguments < maxArguments)
        {
            expression[numArguments] = argument;
        }
        argument = argument->nextExpression;
        ++numArguments;
    }
    return numArguments;
}

GLSLGenerator::GLSLGenerator() :
    m_writer(/* writeFileNames= */ false)
{
    m_tree                      = NULL;
    m_entryName                 = NULL;
    m_target                    = Target_VertexShader;
    m_version                   = Version_140;
    m_versionLegacy             = false;
    m_inAttribPrefix            = NULL;
    m_outAttribPrefix           = NULL;
    m_error                     = false;
    m_matrixRowFunction[0]      = 0;
    m_matrixCtorFunction[0]     = 0;
    m_matrixMulFunction[0]      = 0;
    m_clipFunction[0]           = 0;
    m_tex2DlodFunction[0]       = 0;
    m_tex2DbiasFunction[0]      = 0;
    m_tex3DlodFunction[0]       = 0;
    m_texCUBEbiasFunction[0]    = 0;
	m_texCUBElodFunction[ 0 ] 	= 0;
    m_scalarSwizzle2Function[0] = 0;
    m_scalarSwizzle3Function[0] = 0;
    m_scalarSwizzle4Function[0] = 0;
    m_sinCosFunction[0]         = 0;
	m_bvecTernary[ 0 ]			= 0;
    m_outputPosition            = false;
    m_outputTargets             = 0;
	m_outputGeometryType[0]		= 0;

	m_f16tof32Function[0]		= 0;
	m_f32tof16Function[0]		= 0;

}

bool GLSLGenerator::Generate(HLSLTree* tree, Target target, Version version, const char* entryName, const Options& options)
{

    m_tree      = tree;
    m_entryName = entryName;
    m_target    = target;
    m_version   = version;
    m_versionLegacy = (version == Version_110 || version == Version_100_ES);
    m_options   = options;

	m_domain = NULL;
	m_partitioning = NULL;
	m_outputtopology = NULL;
	m_patchconstantfunc = NULL;

	m_geoInputdataType = NULL;
	m_geoOutputdataType = NULL;

	//memcpy_s(m_preprocessorPackage, sizeof(GLSLPreprocessorPackage) * 128, preprocessors, sizeof(GLSLPreprocessorPackage) * 128);
	m_StructuredBufferCounter = 0;

	m_PushConstantBufferCounter = 0;

	//m_options.flags |= Flag_PackMatrixRowMajor;

    ChooseUniqueName("matrix_row", m_matrixRowFunction, sizeof(m_matrixRowFunction));
    ChooseUniqueName("matrix_ctor", m_matrixCtorFunction, sizeof(m_matrixCtorFunction));
    ChooseUniqueName("matrix_mul", m_matrixMulFunction, sizeof(m_matrixMulFunction));
    ChooseUniqueName("clip", m_clipFunction, sizeof(m_clipFunction));
	ChooseUniqueName("f16tof32", m_f16tof32Function, sizeof(m_f16tof32Function));
	ChooseUniqueName("f32tof16", m_f32tof16Function, sizeof(m_f32tof16Function));

    ChooseUniqueName("tex2Dlod", m_tex2DlodFunction, sizeof(m_tex2DlodFunction));
    ChooseUniqueName("tex2Dbias", m_tex2DbiasFunction, sizeof(m_tex2DbiasFunction));
    ChooseUniqueName("tex2Dgrad", m_tex2DgradFunction, sizeof(m_tex2DgradFunction));
    ChooseUniqueName("tex3Dlod", m_tex3DlodFunction, sizeof(m_tex3DlodFunction));
    ChooseUniqueName("texCUBEbias", m_texCUBEbiasFunction, sizeof(m_texCUBEbiasFunction));
	ChooseUniqueName( "texCUBElod", m_texCUBElodFunction, sizeof( m_texCUBElodFunction ) );

    for (int i = 0; i < s_numReservedWords; ++i)
    {
        ChooseUniqueName( s_reservedWord[i], m_reservedWord[i], sizeof(m_reservedWord[i]) );
    }

    ChooseUniqueName("m_scalar_swizzle2", m_scalarSwizzle2Function, sizeof(m_scalarSwizzle2Function));
    ChooseUniqueName("m_scalar_swizzle3", m_scalarSwizzle3Function, sizeof(m_scalarSwizzle3Function));
    ChooseUniqueName("m_scalar_swizzle4", m_scalarSwizzle4Function, sizeof(m_scalarSwizzle4Function));

    ChooseUniqueName("sincos", m_sinCosFunction, sizeof(m_sinCosFunction));

	ChooseUniqueName( "bvecTernary", m_bvecTernary, sizeof( m_bvecTernary ) );

    if (target == Target_VertexShader)
    {
        m_inAttribPrefix  = "";
        m_outAttribPrefix = "vertOutput_";
    }
    else if (target == Target_FragmentShader)
    {
        m_inAttribPrefix  = "fragInput_";
        m_outAttribPrefix = "fragOutput_";
    }
	else if (target == Target_HullShader)
	{
		m_inAttribPrefix = "tescInput_";
		m_outAttribPrefix = "tescOutput_";
	}
	else if (target == Target_DomainShader)
	{
		m_inAttribPrefix = "teseInput_";
		m_outAttribPrefix = "teseOutput_";
	}
	else if (target == Target_GeometryShader)
	{
		m_inAttribPrefix = "geomInput_";
		m_outAttribPrefix = "geomOutput_";
	}
	else
	{
		m_inAttribPrefix = "";
		m_outAttribPrefix = "";
	}

    HLSLRoot* root = m_tree->GetRoot();
    HLSLStatement* statement = root->statement;

    // Find the entry point function.
    HLSLFunction* entryFunction = FindFunction(root, m_entryName);
    if (entryFunction == NULL)
    {
        Error("Entry point '%s' doesn't exist", m_entryName);
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

    // Output the special function used to access rows in a matrix.
	/*
    m_writer.WriteLine(0, "vec3 %s(mat3 m, int i) { return vec3( m[0][i], m[1][i], m[2][i] ); }", m_matrixRowFunction);
    m_writer.WriteLine(0, "vec4 %s(mat4 m, int i) { return vec4( m[0][i], m[1][i], m[2][i], m[3][i] ); }", m_matrixRowFunction);
	*/

    // Output the special function used to do matrix cast for OpenGL 2.0
    if (m_version == Version_110)
    {
        m_writer.WriteLine(0, "mat3 %s(mat4 m) { return mat3(m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]); }", m_matrixCtorFunction);
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

	// Output the special function used to emulate HLSL clip.
	if (m_tree->NeedsFunction("f16tof32"))
	{
		m_writer.WriteLine(0, "float %s(uint x) { return ((x & 0x8000) << 16) | (((x & 0x7c00) + 0x1C000) << 13) | ((x & 0x03FF) << 13); }", m_f16tof32Function);
	}

	if (m_tree->NeedsFunction("f32tof16"))
	{		
		m_writer.WriteLine(0, "uint %s(float x) { uint f32 = floatBitsToUint(val); return ((f32 >> 16) & 0x8000) | ((((f32 & 0x7f800000) - 0x38000000) >> 13) & 0x7c00) | ((f32 >> 13) & 0x03ff); }", m_f32tof16Function);
	}

	// Output the special function used to emulate HLSL clip.
	if (m_tree->NeedsFunction("clip"))
	{
		const char* discard = m_target == Target_FragmentShader ? "discard" : "";
		m_writer.WriteLine(0, "void %s(float x) { if (x < 0.0) %s;  }", m_clipFunction, discard);
		m_writer.WriteLine(0, "void %s(vec2  x) { if (any(lessThan(x, vec2(0.0, 0.0)))) %s;  }", m_clipFunction, discard);
		m_writer.WriteLine(0, "void %s(vec3  x) { if (any(lessThan(x, vec3(0.0, 0.0, 0.0)))) %s;  }", m_clipFunction, discard);
		m_writer.WriteLine(0, "void %s(vec4  x) { if (any(lessThan(x, vec4(0.0, 0.0, 0.0, 0.0)))) %s;  }", m_clipFunction, discard);
	}

    // Output the special function used to emulate tex2Dlod.
    if (m_tree->NeedsFunction("tex2Dlod"))
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

        m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec4 texCoord) { return %s(samp, texCoord.xy, texCoord.w);  }", m_tex2DlodFunction, function);
    }

    // Output the special function used to emulate tex2Dgrad.
    if (m_tree->NeedsFunction("tex2Dgrad"))
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

        m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec2 texCoord, vec2 dx, vec2 dy) { return %s(samp, texCoord, dx, dy);  }", m_tex2DgradFunction, function);
    }

    // Output the special function used to emulate tex2Dbias.
    if (m_tree->NeedsFunction("tex2Dbias"))
    {
        if (target == Target_FragmentShader)
        {
            m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec4 texCoord) { return %s(samp, texCoord.xy, texCoord.w);  }", m_tex2DbiasFunction, m_versionLegacy ? "texture2D" : "texture" );
        }
        else
        {
            // Bias value is not supported in vertex shader.
            m_writer.WriteLine(0, "vec4 %s(sampler2D samp, vec4 texCoord) { return texture(samp, texCoord.xy);  }", m_tex2DbiasFunction );
        }
    }

    // Output the special function used to emulate tex2DMSfetch.
    if (m_tree->NeedsFunction("tex2DMSfetch"))
    {
        m_writer.WriteLine(0, "vec4 tex2DMSfetch(sampler2DMS samp, ivec2 texCoord, int sample) {");
        m_writer.WriteLine(1, "return texelFetch(samp, texCoord, sample);");
        m_writer.WriteLine(0, "}");
    }

	if (m_tree->NeedsExtension(USE_SAMPLESS))
	{
		m_writer.WriteLine(0, "#extension GL_EXT_samplerless_texture_functions : enable");
	}

	if (m_tree->NeedsExtension(USE_INCLUDE))
	{
		m_writer.WriteLine(0, "#extension GL_GOOGLE_include_directive : enable");
	}


	if (m_tree->NeedsExtension(USE_NonUniformResourceIndex))
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

	if (m_tree->NeedsExtension(USE_Subgroup_Basic))
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_basic : require");
	}

	if (m_tree->NeedsExtension(USE_Subgroup_Quad))
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_arithmetic : require");
	}

	if (m_tree->NeedsExtension(USE_Subgroup_Ballot))
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_ballot : require");
	}

	if (m_tree->NeedsExtension(USE_Subgroup_Arithmetic))
	{
		m_writer.WriteLine(0, "#extension GL_KHR_shader_subgroup_quad : require");
	}


    // Output the special function used to emulate tex3Dlod.
    if (m_tree->NeedsFunction("tex3Dlod"))
    {
        m_writer.WriteLine(0, "vec4 %s(sampler3D samp, vec4 texCoord) { return %s(samp, texCoord.xyz, texCoord.w);  }", m_tex3DlodFunction, m_versionLegacy ? "texture3D" : "texture" );
    }

    // Output the special function used to emulate texCUBEbias.
    if (m_tree->NeedsFunction("texCUBEbias"))
    {
        if (target == Target_FragmentShader)
        {
            m_writer.WriteLine(0, "vec4 %s(samplerCube samp, vec4 texCoord) { return %s(samp, texCoord.xyz, texCoord.w);  }", m_texCUBEbiasFunction, m_versionLegacy ? "textureCube" : "texture" );
        }
        else
        {
            // Bias value is not supported in vertex shader.
            m_writer.WriteLine(0, "vec4 %s(samplerCube samp, vec4 texCoord) { return texture(samp, texCoord.xyz);  }", m_texCUBEbiasFunction );
        }
    }

	// Output the special function used to emulate texCUBElod
	if (m_tree->NeedsFunction("texCUBElod"))
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

		m_writer.WriteLine( 0, "vec4 %s(samplerCube samp, vec4 texCoord) { return %s(samp, texCoord.xyz, texCoord.w);  }", m_texCUBElodFunction, function);
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

	
	
    if (m_tree->NeedsFunction("sincos"))
    {
        const char* floatTypes[] = { "float", "vec2", "vec3", "vec4" };
        for (int i = 0; i < 4; ++i)
        {
            m_writer.WriteLine(0, "void %s(%s x, out %s s, out %s c) { s = sin(x); c = cos(x); }", m_sinCosFunction,
                floatTypes[i], floatTypes[i], floatTypes[i]);
        }
    }

	if (m_tree->NeedsFunction("asfloat"))
	{
		m_writer.WriteLine(0, "%s %s(%s x) { return uintBitsToFloat(x); }", "float", "asfloat", "uint");
		m_writer.WriteLine(0, "%s %s(%s x) { return intBitsToFloat(x); }", "float", "asfloat", "int");
	}

	if (m_tree->NeedsFunction("asuint"))
	{
		m_writer.WriteLine(0, "%s %s(%s x) { return floatBitsToUint(x); }", "uint", "asuint", "float");
	}

	if (m_tree->NeedsFunction("asint"))
	{
		m_writer.WriteLine(0, "%s %s(%s x) { return floatBitsToint(x); }", "int", "asint", "float");
	}
	

	// special function to emulate ?: with bool{2,3,4} condition type
	/*
	m_writer.WriteLine( 0, "vec2 %s(bvec2 cond, vec2 trueExpr, vec2 falseExpr) { vec2 ret; ret.x = cond.x ? trueExpr.x : falseExpr.x; ret.y = cond.y ? trueExpr.y : falseExpr.y; return ret; }", m_bvecTernary );
	m_writer.WriteLine( 0, "vec3 %s(bvec3 cond, vec3 trueExpr, vec3 falseExpr) { vec3 ret; ret.x = cond.x ? trueExpr.x : falseExpr.x; ret.y = cond.y ? trueExpr.y : falseExpr.y; ret.z = cond.z ? trueExpr.z : falseExpr.z; return ret; }", m_bvecTernary );
	m_writer.WriteLine( 0, "vec4 %s(bvec4 cond, vec4 trueExpr, vec4 falseExpr) { vec4 ret; ret.x = cond.x ? trueExpr.x : falseExpr.x; ret.y = cond.y ? trueExpr.y : falseExpr.y; ret.z = cond.z ? trueExpr.z : falseExpr.z; ret.w = cond.w ? trueExpr.w : falseExpr.w; return ret; }", m_bvecTernary );
	*/

    // Output the extension used for dFdx/dFdy in GLES2
    if (m_version == Version_100_ES && (m_tree->NeedsFunction("ddx") || m_tree->NeedsFunction("ddy")))
    {
        m_writer.WriteLine(0, "#extension GL_OES_standard_derivatives : require");
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
			for (int i = 0; i < m_outputTargets; i++)
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

void GLSLGenerator::PrintPreprocessors(int currentLine)
{

	for (int i = 0; i < 128; i++)
	{
		if (m_preprocessorPackage[i].m_line <= -100)
			return;

		if (m_preprocessorPackage[i].m_line <= 0)
			continue;

		if (m_writer.m_previousLine < m_preprocessorPackage[i].m_line &&  currentLine > m_preprocessorPackage[i].m_line)
		{
			//print it
			m_writer.Write("%s", m_preprocessorPackage[i].m_storedPreprocessors);
			m_writer.EndLine("");

			m_writer.m_previousLine = m_preprocessorPackage[i].m_line;
			m_preprocessorPackage[i].m_line = -1;
		}
	}
}

const char* GLSLGenerator::GetResult() const
{
    return m_writer.GetResult();
}

void GLSLGenerator::OutputExpressionList(HLSLExpression* expression, HLSLArgument* argument)
{
    int numExpressions = 0;
	bool bPre = false;
    while (expression != NULL)
    {
        if (!bPre && numExpressions > 0)
        {
            m_writer.Write(", ");
        }

		bPre = false;
        
        HLSLType* expectedType = NULL;
        if (argument != NULL)
        {
			//handle preprocessor
			if (argument->type.baseType == HLSLBaseType_Unknown)
			{
				argument = argument->nextArgument;
				bPre = true;
				continue;
			}

            expectedType = &argument->type;
            argument = argument->nextArgument;
        }

		
		//OutputExpression(expression, expectedType);
		
		if (expectedType)
		{
			if (expectedType->baseType != expression->expressionType.baseType)
			{
				//if it is different, do type cast
				//bPre = true;
								
				//m_writer.Write("%s(", getElementTypeAsStrGLSL(*expectedType));
				OutputExpression(expression, expectedType);
				//m_writer.Write(")");
				expression = expression->nextExpression;
			}
			else
			{
				OutputExpression(expression, expectedType);
				expression = expression->nextExpression;
			}
		}
		else
		{
			OutputExpression(expression, expectedType);
			expression = expression->nextExpression;
		}
		
        
        ++numExpressions;
    }
}

const HLSLType* commonScalarType(const HLSLType& lhs, const HLSLType& rhs)
{
    if (!isScalarType(lhs) || !isScalarType(rhs))
        return NULL;

    if (lhs.baseType == HLSLBaseType_Float || lhs.baseType == HLSLBaseType_Half ||
        rhs.baseType == HLSLBaseType_Float || rhs.baseType == HLSLBaseType_Half)
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
		if (expression->expressionType.baseType == HLSLBaseType_UserDefined && expression->expressionType.typeName)
		{
			m_writer.Write(".%s_Data", expression->expressionType.typeName);
		}
	}
}

void GLSLGenerator::OutputExpression(HLSLExpression* expression, const HLSLType* dstType)
{

    bool cast = dstType != NULL && !GetCanImplicitCast(expression->expressionType, *dstType);
    if (expression->nodeType == HLSLNodeType_CastingExpression)
    {
        // No need to include a cast if the expression is already doing it.
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
	else if (expression->childExpression)
	{
		m_writer.Write("{");
		OutputExpressionList(expression->childExpression);
		m_writer.Write("}");
	}
    else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
		/*
		if (expression->expressionType.baseType == HLSLBaseType_InputPatch)
		{

		}
		else
		{
		*/
			HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
			OutputIdentifier(identifierExpression->name);


			if (expression->functionExpression)
			{
				OutputExpression(expression->functionExpression);
			}
		//}
		//}

        
    }
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression* constructorExpression = static_cast<HLSLConstructorExpression*>(expression);
        m_writer.Write("%s(", GetTypeName(constructorExpression->type));
        OutputExpressionList(constructorExpression->argument);
        m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_CastingExpression)
    {
        HLSLCastingExpression* castingExpression = static_cast<HLSLCastingExpression*>(expression);
        OutputCast(castingExpression->type);
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
            {
                // Don't use printf directly so that we don't use the system locale.
                char buffer[64];
                String_FormatFloat(buffer, sizeof(buffer), literalExpression->fValue);
                m_writer.Write("%s", buffer);
            }
            break;
        case HLSLBaseType_Int:
            m_writer.Write("%d", literalExpression->iValue);
            break;
        case HLSLBaseType_Uint:
            m_writer.Write("%uu", literalExpression->uiValue);
	    break;
	case HLSLBaseType_Bool:
            m_writer.Write("%s", literalExpression->bValue ? "true" : "false");
            break;
        default:
            ASSERT(0);
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
            m_writer.Write("%s", op);
            OutputExpression(unaryExpression->expression, dstType);
        }
        else
        {
            OutputExpression(unaryExpression->expression, dstType);
            m_writer.Write("%s", op);
        }
        m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_BinaryExpression)
    {
        HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(expression);
        const char* op = "?";
        const HLSLType* dstType1 = NULL;
        const HLSLType* dstType2 = NULL;

		//
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
				ASSERT(0); // is so, check isCompareOp
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

			default:
				ASSERT(0);
			}

			// Exception Handling for imageStore, this might make errors
			// Need to change better form
			//HLSLArrayAccess* ArrayAcess = static_cast<HLSLArrayAccess*>(binaryExpression->expression1);

			//if (binaryExpression->binaryOp == HLSLBinaryOp_Assign && ArrayAcess->array != NULL && ArrayAcess->array->nodeType == HLSLNodeType_RWTextureStateExpression)
			if (binaryExpression->binaryOp == HLSLBinaryOp_Assign && binaryExpression->expression1->expressionType.baseType >= HLSLBaseType_RWTexture1D && binaryExpression->expression1->expressionType.baseType <= HLSLBaseType_RWTexture3D)
			{
				//HLSLRWTextureStateExpression* rwTextureStateExpression = static_cast<HLSLRWTextureStateExpression*>(ArrayAcess->array);
				//HLSLTextureStateExpression* textureStateExpression = static_cast<HLSLTextureStateExpression*>(ArrayAcess->array);

				HLSLTextureStateExpression* textureStateExpression = static_cast<HLSLTextureStateExpression*>(binaryExpression->expression1);

				
				if (IsRWTexture(textureStateExpression->expressionType.baseType))
				{
					m_writer.Write("imageStore(");

					m_writer.Write("%s", textureStateExpression->name);

					if (textureStateExpression->bArray)
					{
						for (int i = 0; i < (int)textureStateExpression->arrayDimension; i++)
						{
							if (textureStateExpression->arrayExpression)
							{
								m_writer.Write("[");
								OutputExpressionList(textureStateExpression->arrayExpression);
								m_writer.Write("]");
							}
							else if (textureStateExpression->arrayIndex[i] > 0)
								m_writer.Write("[%u]", textureStateExpression->arrayIndex[i]);
							else
								m_writer.Write("[]");
						}
					}

					switch (textureStateExpression->expressionType.baseType)
					{
					case HLSLBaseType_RWTexture1D:
						m_writer.Write(", int(", textureStateExpression->name);
						break;
					case HLSLBaseType_RWTexture1DArray:
						m_writer.Write(", ivec2(", textureStateExpression->name);
						break;
					case HLSLBaseType_RWTexture2D:
						m_writer.Write(", ivec2(", textureStateExpression->name);
						break;
					case HLSLBaseType_RWTexture2DArray:
						m_writer.Write(", ivec3(", textureStateExpression->name);
						break;
					case HLSLBaseType_RWTexture3D:
						m_writer.Write(", ivec3(", textureStateExpression->name);
						break;
					default:
						break;
					}

					//OutputExpression(ArrayAcess->index);
					if (textureStateExpression->indexExpression)
					{						
						OutputExpressionList(textureStateExpression->indexExpression);						
					}					

					m_writer.Write("), ");


					switch (binaryExpression->expression2->expressionType.baseType)
					{
					case HLSLBaseType_Float:
						m_writer.Write("vec4(");
						OutputExpression(binaryExpression->expression2, &binaryExpression->expression2->expressionType);
						m_writer.Write(", 0.0, 0.0, 0.0)");
						break;
					case HLSLBaseType_Float2:
						m_writer.Write("vec4(");
						OutputExpression(binaryExpression->expression2, &binaryExpression->expression2->expressionType);
						m_writer.Write(", 0.0, 0.0)");
						break;
					case HLSLBaseType_Float3:
						m_writer.Write("vec3(");
						OutputExpression(binaryExpression->expression2, &binaryExpression->expression2->expressionType);
						m_writer.Write(", 0.0)");
						break;
					case HLSLBaseType_Float4:
						OutputExpression(binaryExpression->expression2, &binaryExpression->expression2->expressionType);
						break;
					default:
						OutputExpression(binaryExpression->expression2, &binaryExpression->expression2->expressionType);
						break;
					}

					
					m_writer.Write(")");
				}
				else
				{
					m_writer.Write("(");
					OutputExpression(binaryExpression->expression1, dstType1);
					m_writer.Write("%s", op);
					OutputExpression(binaryExpression->expression2, dstType2);
					m_writer.Write(")");
				}				
				
			}
			else
			{
				m_writer.Write("(");
				OutputExpression(binaryExpression->expression1, dstType1);
				m_writer.Write("%s", op);
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
			m_writer.Write( "%s", m_bvecTernary );
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
			ASSERT(structure != NULL);
			
			
			HLSLStructField* field = structure->field;
			while (field != NULL)
			{
				if (String_Equal(field->name, memberAccess->field))
				{
					//find it from input
					char attribName[64];
					String_Printf(attribName, 64, "%s%s", m_inAttribPrefix, field->semantic);
					m_writer.Write("%s", attribName);

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
						m_writer.Write("%s%s", m_outAttribPrefix, field->semantic);
					}
					else
					{
						m_writer.Write("(");
						OutputExpression(memberAccess->object);
						m_writer.Write(")");
						m_writer.Write(".%s", memberAccess->field);
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
            memberAccess->object->expressionType.baseType == HLSLBaseType_Int   ||
            memberAccess->object->expressionType.baseType == HLSLBaseType_Uint)
        {
            m_writer.Write("(");
            OutputExpression(memberAccess->object);
            m_writer.Write(")");

			m_writer.Write(".%s", memberAccess->field);
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
				memberAccess->object->expressionType.baseType == HLSLBaseType_Half4x4 )
            {
                // Handle HLSL matrix "swizzling".
                // TODO: Properly handle multiple element selection such as _m00_m12
                const char* n = memberAccess->field;
                while (n[0] != 0)
                {
                    if ( n[0] != '_' )
                    {
                        ASSERT(0);
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
                        ASSERT(0);
                        break;
                    }
                }
            }
            else
            {
                m_writer.Write(".%s", memberAccess->field);
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
			(arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Float2x2 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Float3x3 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Float4x4 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Half2x2 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Half3x3 ||
				arrayAccessInMatrix->array->expressionType.baseType == HLSLBaseType_Half4x4))
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
        else
        {
			///!!!!!!!!!!!!!!!!
            OutputExpression(arrayAccess->array);
			
            m_writer.Write("[");
            OutputExpression(arrayAccess->index);
            m_writer.Write("]");

			if (arrayAccess->identifier)
			{
				m_writer.Write(".%s_Data", arrayAccess->identifier);
			}

			
        }

    }
    else if (expression->nodeType == HLSLNodeType_FunctionCall)
    {
        HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(expression);

        // Handle intrinsic funtions that are different between HLSL and GLSL.
        bool handled = false;
        const char* functionName = functionCall->function->name;

        if (String_Equal(functionName, "mul"))
        {
            HLSLExpression* argument[2];
            if (GetFunctionArguments(functionCall, argument, 2) != 2)
            {
                Error("mul expects 2 arguments");
                return;
            }

            const HLSLType& type0 = functionCall->function->argument->type;
            const HLSLType& type1 = functionCall->function->argument->nextArgument->type;

            const char* prefix = (m_options.flags & Flag_LowerMatrixMultiplication) ? m_matrixMulFunction : "";
            const char* infix = (m_options.flags & Flag_LowerMatrixMultiplication) ? "," : "*";

            if (m_options.flags & Flag_PackMatrixRowMajor)
            {
                m_writer.Write("%s((", prefix);
                OutputExpression(argument[1], &type1);
                m_writer.Write(")%s(", infix);
                OutputExpression(argument[0], &type0);
                m_writer.Write("))");
            }
            else
            {
                m_writer.Write("%s((", prefix);
                OutputExpression(argument[0], &type0);
                m_writer.Write(")%s(", infix);
                OutputExpression(argument[1], &type1);
                m_writer.Write("))");
            }

            handled = true;
        }
        else if (String_Equal(functionName, "saturate"))
        {
            HLSLExpression* argument[1];
            if (GetFunctionArguments(functionCall, argument, 1) != 1)
            {
                Error("saturate expects 1 argument");
                return;
            }
            m_writer.Write("clamp(");
            OutputExpression(argument[0]);
            m_writer.Write(", 0.0, 1.0)");
            handled = true;
        }	
		else if (String_Equal(functionName, "rcp"))
		{
			HLSLExpression* argument[1];
			if (GetFunctionArguments(functionCall, argument, 1) != 1)
			{
				Error("rcp expects 1 argument");
				return;
			}
			m_writer.Write("1 / ");
			OutputExpression(argument[0]);
			handled = true;
		}
		else if (String_Equal(functionName, "pow"))
		{
			HLSLExpression* argument[2];
			if (GetFunctionArguments(functionCall, argument, 2) != 2)
			{
				Error("pow expects 2 argument");
				return;
			}
			m_writer.Write("pow(");
			OutputExpression(argument[0]);
			m_writer.Write(",");

			const char* type = getElementTypeAsStrGLSL(argument[0]->expressionType);
			if (!String_Equal(type, "UnknownElementType"))
			{
				m_writer.Write("%s", getElementTypeAsStrGLSL(argument[0]->expressionType));
				m_writer.Write("(");
			}
			OutputExpression(argument[1]);
			if (!String_Equal(type, "UnknownElementType"))
			{
				m_writer.Write(")");
			}

			m_writer.Write(")");

			handled = true;
		}
		else if (String_Equal(functionName, "WaveGetLaneIndex"))
		{
			m_writer.Write("gl_SubgroupInvocationID");

			handled = true;
		}
		else if (String_Equal(functionName, "WaveGetLaneCount"))
		{
			m_writer.Write("gl_SubgroupSize");

			handled = true;
		}
		else if (String_Equal(functionName, "Sample") || String_Equal(functionName, "SampleLevel") || String_Equal(functionName, "SampleCmp") ||
			String_Equal(functionName, "SampleCmpLevelZero") || String_Equal(functionName, "SampleBias") || String_Equal(functionName, "GatherRed") || String_Equal(functionName, "SampleGrad"))
		{
			if(String_Equal(functionName, "Sample"))
			{
				m_writer.Write("texture(");
			}
			else if (String_Equal(functionName, "SampleLevel"))
			{
				m_writer.Write("textureLod(");
			}
			else if (String_Equal(functionName, "SampleCmp"))
			{
				m_writer.Write("texture(");
			}
			else if (String_Equal(functionName, "SampleCmpLevelZero"))
			{
				m_writer.Write("texture(");
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

			int dimension = 0;

		
			const char* arguementType = NULL;

			switch (functionCall->pTextureStateExpression->expressionType.baseType)
			{
			case HLSLBaseType_Texture1D:
				m_writer.Write("sampler1D"); dimension = 1;
				arguementType = "float";
				break;
			case HLSLBaseType_Texture1DArray:
				m_writer.Write("sampler1DArray"); dimension = 2;
				arguementType = "vec2";
				break;
			case HLSLBaseType_Texture2D:
				if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
				{
					m_writer.Write("sampler2DShadow"); dimension = 3;
					arguementType = "vec4";
				}
				else
				{
					m_writer.Write("sampler2D"); dimension = 2;
					arguementType = "vec2";
				}
				
				break;
			case HLSLBaseType_Texture2DArray:
				if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
				{
					m_writer.Write("sampler2DArrayShadow"); dimension = 4;
					arguementType = "vec4";
				}
				else
				{
					m_writer.Write("sampler2DArray"); dimension = 3;
					arguementType = "vec3";
				}				
				break;
			case HLSLBaseType_Texture3D: 
				m_writer.Write("sampler3D"); dimension = 3;
				arguementType = "vec3";
				break;
			case HLSLBaseType_Texture2DMS:
				m_writer.Write("sampler2DMS"); dimension = 2;
				arguementType = "vec2";
				break;
			case HLSLBaseType_Texture2DMSArray:
				m_writer.Write("sampler2DMSArray"); dimension = 3;
				arguementType = "vec3";
				break;
			case HLSLBaseType_TextureCube:
				m_writer.Write("samplerCube"); dimension = 3;
				arguementType = "vec3";
				break;
			case HLSLBaseType_TextureCubeArray:
				if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
				{
					m_writer.Write("samplerCubeArrayShadow"); dimension = 4;
					arguementType = "vec4";
				}
				else
				{
					m_writer.Write("samplerCubeArray"); dimension = 4;
					arguementType = "vec4";
				}				
				break;
			default:
				break;
			}

			if (String_Equal(functionName, "SampleLevel"))
				dimension++;

			m_writer.Write("(");
			//m_writer.Write("(%s, ", functionCall->functionCaller);

						
			if (functionCall->pTextureStateExpression)
			{
				const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;

				m_writer.Write(" %s", pTextureStateExpression->name);

				if (pTextureStateExpression->bArray)
				{
					for (int i = 0; i < (int)pTextureStateExpression->arrayDimension; i++)
					{
						if (pTextureStateExpression->arrayExpression)
						{
							m_writer.Write("[");
							OutputExpressionList(pTextureStateExpression->arrayExpression);
							m_writer.Write("]");
						}
						else if (pTextureStateExpression->arrayIndex[i] > 0)
							m_writer.Write("[%u]", pTextureStateExpression->arrayIndex[i]);
						else
							m_writer.Write("[]");
					}
				}
			}
			

			//m_writer.Write(".%s(", name);
			//OutputExpressionList(functionCall->argument);
			m_writer.Write(", ");

			HLSLExpression* expression = functionCall->argument;
			HLSLArgument* argument = functionCall->function->argument;


			int numExpressions = 0;

			if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
			{

			}
			else
			{

			}


			while (expression != NULL)
			{
				if (numExpressions == 1)
				{
					m_writer.Write("), ");
				}
				else if (numExpressions >= 2)
				{
					m_writer.Write(", ");
				}
				HLSLType* expectedType = NULL;
				if (argument != NULL)
				{
					expectedType = &argument->type;
					argument = argument->nextArgument;
				}

				if (numExpressions == 1)
					m_writer.Write("%s(", arguementType);

				OutputExpression(expression, expectedType);

				if (String_Equal(functionName, "SampleCmp") || String_Equal(functionName, "SampleCmpLevelZero"))
				{
					if (numExpressions == 2)
						m_writer.Write(")");
				}
				else
				{
					if (numExpressions == 1)
						m_writer.Write(")");
				}
				

				expression = expression->nextExpression;
				++numExpressions;
			}
			m_writer.Write(")");
			
			handled = true;
		}
		else if (String_Equal(functionName, "GetDimensions"))
		{
			HLSLExpression* expression = functionCall->argument;
			HLSLArgument* argument = functionCall->function->argument;

			//expression->nodeType == HLSLNodeType_MemberAccess;
			//HLSLMemberAccess* memExpression = expression;

			HLSLType* expectedType = NULL;
			if (argument != NULL)
			{
				expectedType = &argument->type;
				argument = argument->nextArgument;
			}

			//if it is from texture
			if (functionCall->pTextureStateExpression)
			{
				HLSLExpression* exp = functionCall->argument;

					int argCount = 0;

					while (exp)
					{
						if (argCount > 0)
						{
							m_writer.Write(";\n");
							m_writer.Write(1, "");
						}

						OutputExpression(exp, expectedType);

						m_writer.Write(" = textureSize(");

						const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;

						m_writer.Write("%s", pTextureStateExpression->name);

						if (pTextureStateExpression->bArray)
						{
							for (int i = 0; i < (int)pTextureStateExpression->arrayDimension; i++)
							{
								if (pTextureStateExpression->arrayExpression)
								{
									m_writer.Write("[");
									OutputExpressionList(pTextureStateExpression->arrayExpression);
									m_writer.Write("]");
								}
								else if (pTextureStateExpression->arrayIndex[i] > 0)
									m_writer.Write("[%u]", pTextureStateExpression->arrayIndex[i]);
								else
									m_writer.Write("[]");
							}
						}

						if(argCount == 0)
							m_writer.Write(").x");
						else if (argCount == 1)
							m_writer.Write(").y");
						else if (argCount == 2)
							m_writer.Write(").z");
						else if (argCount == 3)
							m_writer.Write(").w");

						exp = exp->nextExpression;
						argCount++;
					}
				
			}
			else if (functionCall->pBuffer)
			{
				m_writer.Write("[");

				OutputExpression(functionCall->argument);

				m_writer.Write("]");
			}

			handled = true;
		}
		/// !!! need to check it later
		else if (String_Equal(functionName, "Load"))
		{
			HLSLExpression* expression = functionCall->argument;
			HLSLArgument* argument = functionCall->function->argument;

			//expression->nodeType == HLSLNodeType_MemberAccess;
			//HLSLMemberAccess* memExpression = expression;

			HLSLType* expectedType = NULL;
			if (argument != NULL)
			{
				expectedType = &argument->type;
				argument = argument->nextArgument;
			}
			
			//if it is from texture
			if (functionCall->pTextureStateExpression)
			{
				if (HLSLBaseType_Texture1D <= functionCall->pTextureStateExpression->expressionType.baseType  &&
					HLSLBaseType_TextureCubeArray >= functionCall->pTextureStateExpression->expressionType.baseType)
				{
					m_writer.Write("texelFetch(");

					const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;

					m_writer.Write("%s", pTextureStateExpression->name);

					if (pTextureStateExpression->bArray)
					{
						for (int i = 0; i < (int)pTextureStateExpression->arrayDimension; i++)
						{
							if (pTextureStateExpression->arrayExpression)
							{
								m_writer.Write("[");
								OutputExpressionList(pTextureStateExpression->arrayExpression);
								m_writer.Write("]");
							}
							else if (pTextureStateExpression->arrayIndex[i] > 0)
								m_writer.Write("[%u]", pTextureStateExpression->arrayIndex[i]);
							else
								m_writer.Write("[]");
						}
					}					

					m_writer.Write(", ");

					
					switch (functionCall->pTextureStateExpression->expressionType.baseType)
					{
					
					case HLSLBaseType_Texture1D:
					
						m_writer.Write("(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").x, ");

						m_writer.Write("(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").y");
						break;

					case HLSLBaseType_Texture1DArray:
						
						m_writer.Write("ivec2(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").xy, ");

						m_writer.Write("(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").z");
						break;

					case HLSLBaseType_Texture2D:
						
						m_writer.Write("ivec2(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").xy");	

						//offset
						if (functionCall->argument->nextExpression)
						{
							if (functionCall->argument->nextExpression->nextExpression)
							{
								m_writer.Write("+");
								OutputExpression(functionCall->argument->nextExpression->nextExpression);
							}
						}

						m_writer.Write(", ");


						m_writer.Write("(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").z");

						break;

					case HLSLBaseType_Texture2DMS:
						
						m_writer.Write("ivec2(");
						OutputExpression(functionCall->argument);
						m_writer.Write(").xy");

						//offset
						if (functionCall->argument->nextExpression->nextExpression)
						{
							m_writer.Write("+");
							OutputExpression(functionCall->argument->nextExpression->nextExpression);
						}

						m_writer.Write(", ");



						if (functionCall->argument->nextExpression)
						{
							m_writer.Write("int(");
							OutputExpression(functionCall->argument->nextExpression);
							m_writer.Write(")");
						}
						else
						{						
							OutputExpression(functionCall->argument);
							m_writer.Write(".z");
						}					

						
						break;

					case HLSLBaseType_Texture2DArray:
						
						m_writer.Write("ivec3(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").xyz, ");

						m_writer.Write("(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").w");
						break;

					case HLSLBaseType_Texture2DMSArray:
						
						m_writer.Write("ivec3(");
						OutputExpression(functionCall->argument);
						m_writer.Write(").xyz, ");

						if (functionCall->argument->nextExpression)
						{
							m_writer.Write("int(");
							OutputExpression(functionCall->argument->nextExpression);
							m_writer.Write(")");
						}
						else
						{
							OutputExpression(functionCall->argument);
							m_writer.Write(".w");
						}
						break;

					case HLSLBaseType_Texture3D:
						
						m_writer.Write("ivec3(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").xyz");

						//offset
						if (pTextureStateExpression->indexExpression->nextExpression && pTextureStateExpression->indexExpression->nextExpression->nextExpression)
						{
							m_writer.Write("+");
							OutputExpression(pTextureStateExpression->indexExpression->nextExpression->nextExpression);
						}

						m_writer.Write(", ");

						m_writer.Write("(");
						OutputExpression(expression, expectedType);
						m_writer.Write(").w");
						break;
					default:
						break;
					}
					



					m_writer.Write(")");
				}
			}
			else if(functionCall->pBuffer)
			{
				m_writer.Write("[");

				OutputExpression(functionCall->argument);

				m_writer.Write("]");
			}
			
			handled = true;
		}
		else if (String_Equal(functionName, "Store"))
		{
			HLSLExpression* expression = functionCall->argument;
			HLSLArgument* argument = functionCall->function->argument;

			HLSLType* expectedType = NULL;
			if (argument != NULL)
			{
				expectedType = &argument->type;
				argument = argument->nextArgument;
			}

			if (functionCall->pBuffer)
			{
				m_writer.Write("[");

				OutputExpression(functionCall->argument);

				m_writer.Write("]");

				m_writer.Write(" = ");

				OutputExpression(functionCall->argument->nextExpression);
			}

			handled = true;
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

			HLSLExpression* expression = functionCall->argument;
			HLSLArgument* argument = functionCall->function->argument;

			//check the number of arguements

			HLSLExpression* expressionTemp = expression;
			int numArguments = 0;
			while (expressionTemp != NULL)
			{
				expressionTemp = expressionTemp->nextExpression;
				++numArguments;
			}

			if (numArguments >= 3)
			{
				HLSLType* expectedType = NULL;
				if (argument != NULL)
					expectedType = &argument->nextArgument->nextArgument->type;

				OutputExpression(expression->nextExpression->nextExpression, expectedType);

				m_writer.Write(" = ");

				if(String_Equal(functionName, "InterlockedAdd"))
				{
					m_writer.Write("atomicAdd(");
				}
				else if (String_Equal(functionName, "InterlockedAnd"))
				{
					m_writer.Write("atomicAnd(");
				}
				else if (String_Equal(functionName, "InterlockedCompareExchange"))
				{
					m_writer.Write("atomicCompSwap(");
				}
				else if (String_Equal(functionName, "InterlockedExchange"))
				{
					m_writer.Write("atomicExchange(");
				}
				else if (String_Equal(functionName, "InterlockedMax"))
				{
					m_writer.Write("atomicMax(");
				}
				else if (String_Equal(functionName, "InterlockedMin"))
				{
					m_writer.Write("atomicMin(");
				}
				else if (String_Equal(functionName, "InterlockedOr"))
				{
					m_writer.Write("atomicOr(");
				}
				else if (String_Equal(functionName, "InterlockedXor"))
				{
					m_writer.Write("atomicXor(");
				}

				int numExpressions2 = 0;

				while (expression != NULL)
				{
					
					if (numExpressions2 == 2)
					{
						m_writer.Write(")");
						break;
					}					

					if (numExpressions2 > 0)
						m_writer.Write(", ");

					HLSLType* expectedType = NULL;
					if (argument != NULL)
					{
						expectedType = &argument->type;
						argument = argument->nextArgument;
					}

					OutputExpression(expression, expectedType);
					expression = expression->nextExpression;

					numExpressions2++;
				}


				handled = true;
			}
			
		}

        if (!handled)
        {
            OutputIdentifier(functionName);
            m_writer.Write("(");
            OutputExpressionList(functionCall->argument, functionCall->function->argument);
            m_writer.Write(")");
        }
    }
	else if (expression->nodeType == HLSLNodeType_PreprocessorExpression)
	{
		HLSLPreprocessorExpression* preprecessorExpression = static_cast<HLSLPreprocessorExpression*>(expression);

		m_writer.Write("%s", preprecessorExpression->name);
	}
	else if (expression->nodeType == HLSLNodeType_SamplerStateExpression)
	{
		HLSLSamplerStateExpression* samplerStateExpression = static_cast<HLSLSamplerStateExpression*>(expression);

		m_writer.Write("%s", samplerStateExpression->name);
	}
	else if (expression->nodeType == HLSLNodeType_TextureStateExpression)
	{
		HLSLTextureStateExpression* textureStateExpression = static_cast<HLSLTextureStateExpression*>(expression);

		if (textureStateExpression->memberAccessExpression)
		{
			/*
			HLSLMemberAccess* ma = static_cast<HLSLMemberAccess*>(textureStateExpression->memberAccessExpression);
			HLSLFunctionCall* fc = static_cast<HLSLFunctionCall*>(ma->object);
			fc->pTextureStateExpression = textureStateExpression;
			*/

			if (textureStateExpression->indexExpression)
			{
				if (HLSLBaseType_Texture1D <= textureStateExpression->expressionType.baseType  &&
					HLSLBaseType_TextureCubeArray >= textureStateExpression->expressionType.baseType)
				{
					m_writer.Write("texelFetch(");

					const HLSLTextureStateExpression* pTextureStateExpression = textureStateExpression;

					m_writer.Write("%s", pTextureStateExpression->name);

					if (pTextureStateExpression->bArray)
					{
						for (int i = 0; i < (int)pTextureStateExpression->arrayDimension; i++)
						{
							if (pTextureStateExpression->arrayExpression)
							{
								m_writer.Write("[");
								OutputExpressionList(pTextureStateExpression->arrayExpression);
								m_writer.Write("]");
							}
							else if (pTextureStateExpression->arrayIndex[i] > 0)
								m_writer.Write("[%u]", pTextureStateExpression->arrayIndex[i]);
							else
								m_writer.Write("[]");
						}
					}

					m_writer.Write(", ");


					switch (pTextureStateExpression->expressionType.baseType)
					{

					case HLSLBaseType_Texture1D:

						m_writer.Write("(");
						OutputExpression(textureStateExpression->indexExpression);
						
						//offset
						if (textureStateExpression->indexExpression->nextExpression && textureStateExpression->indexExpression->nextExpression->nextExpression)
						{
							m_writer.Write("+");
							OutputExpression(textureStateExpression->indexExpression->nextExpression->nextExpression);
						}

						m_writer.Write(").x, ");


						if (textureStateExpression->indexExpression->nextExpression)
						{
							m_writer.Write("(");
							OutputExpression(textureStateExpression->indexExpression->nextExpression);
							m_writer.Write(").y");
						}
						else
							m_writer.Write("0");
						
						break;

					case HLSLBaseType_Texture1DArray:

						m_writer.Write("ivec2(");
						OutputExpression(textureStateExpression->indexExpression);
						m_writer.Write(").xy, ");


						if (textureStateExpression->indexExpression->nextExpression)
						{
							m_writer.Write("(");
							OutputExpression(textureStateExpression->indexExpression->nextExpression);
							m_writer.Write(").z");
						}
						else
							m_writer.Write("0");

						
						break;

					case HLSLBaseType_Texture2D:

						m_writer.Write("ivec2(");
						OutputExpression(textureStateExpression->indexExpression);
						m_writer.Write(").xy");

						//offset
						if (textureStateExpression->indexExpression->nextExpression && textureStateExpression->indexExpression->nextExpression->nextExpression)
						{
							m_writer.Write("+");
							OutputExpression(textureStateExpression->indexExpression->nextExpression->nextExpression);
						}

						m_writer.Write(", ");

						if (textureStateExpression->indexExpression->nextExpression)
						{
							m_writer.Write("(");
							OutputExpression(textureStateExpression->indexExpression->nextExpression);
							m_writer.Write(").z");
						}
						else
							m_writer.Write("0");
						break;

					case HLSLBaseType_Texture2DMS:
						
						m_writer.Write("ivec2(");
						OutputExpression(textureStateExpression->indexExpression);
						m_writer.Write(").xy");

						//offset

						if (textureStateExpression->indexExpression->nextExpression == NULL)
						{
							m_writer.Write("+ ivec2(0, 0), 1)");
						}
						else
						{
							if (textureStateExpression->indexExpression->nextExpression->nextExpression)
							{
								m_writer.Write("+");
								OutputExpression(textureStateExpression->indexExpression->nextExpression->nextExpression);
							}

							m_writer.Write(", ");

							m_writer.Write("int(");
							OutputExpression(textureStateExpression->indexExpression->nextExpression);
							m_writer.Write(")");
						}

						
						break;

					case HLSLBaseType_Texture2DArray:

						m_writer.Write("ivec3(");
						OutputExpression(textureStateExpression->indexExpression);
						m_writer.Write(").xyz, ");


						if (textureStateExpression->indexExpression->nextExpression)
						{
							m_writer.Write("(");
							OutputExpression(textureStateExpression->indexExpression->nextExpression);
							m_writer.Write(").w");
						}
						else
							m_writer.Write("0");
						
						break;

					case HLSLBaseType_Texture2DMSArray:


						m_writer.Write("ivec3(");
						OutputExpression(textureStateExpression->indexExpression);
						m_writer.Write(").xyz, ");

						m_writer.Write("int(");
						OutputExpression(textureStateExpression->indexExpression->nextExpression);
						m_writer.Write(")");
						break;

					case HLSLBaseType_Texture3D:

						m_writer.Write("ivec3(");
						OutputExpression(textureStateExpression->indexExpression);
						m_writer.Write(").xyz");

						//offset
						if (textureStateExpression->indexExpression->nextExpression && textureStateExpression->indexExpression->nextExpression->nextExpression)
						{
							m_writer.Write("+");
							OutputExpression(textureStateExpression->indexExpression->nextExpression->nextExpression);
						}

						m_writer.Write(", ");


						if (textureStateExpression->indexExpression->nextExpression)
						{
							m_writer.Write("(");
							OutputExpression(textureStateExpression->indexExpression->nextExpression);
							m_writer.Write(").w");
						}
						else
							m_writer.Write("0");
						
						break;
					default:
						break;
					}




					m_writer.Write(")");
				}

				//OutputExpression(textureStateExpression->indexExpression);
			}

			if(textureStateExpression->functionExpression)
				OutputExpression(textureStateExpression->functionExpression);
		}
		else if (textureStateExpression->functionExpression)
		{

			OutputExpression(textureStateExpression->functionExpression);
			
		}		
		else
			m_writer.Write("%s", textureStateExpression->name);
	}
    else
    {
        m_writer.Write("<unknown expression>");
    }

    if (cast)
    {
/*
        const BaseTypeDescription& srcTypeDesc = _baseTypeDescriptions[expression->expressionType.baseType];
        const BaseTypeDescription& dstTypeDesc = _baseTypeDescriptions[dstType->baseType];

        if (dstTypeDesc.numDimensions == 1 && dstTypeDesc.numComponents > 1)
        {
            // Casting to a vector - pad with 0s
            for (int i = srcTypeDesc.numComponents; i < dstTypeDesc.numComponents; ++i)
            {
                m_writer.Write(", 0");
            }
        }
*/

        m_writer.Write(")");
    }

}

void GLSLGenerator::OutputIdentifier(const char* name)
{

    // Remap intrinstic functions.
    if (String_Equal(name, "tex2D"))
    {
        name = m_versionLegacy ? "texture2D" : "texture";
    }
    else if (String_Equal(name, "tex2Dproj"))
    {
        name = m_versionLegacy ? "texture2DProj" : "textureProj";
    }
    else if (String_Equal(name, "texCUBE"))
    {
        name = m_versionLegacy ? "textureCube" : "texture";
    }
    else if (String_Equal(name, "tex3D"))
    {
        name = m_versionLegacy ? "texture3D" : "texture";
    }
    else if (String_Equal(name, "clip"))
    {
        name = m_clipFunction;
    }
	else if (String_Equal(name, "f16tof32"))
	{
		name = m_f16tof32Function;
	}
	else if (String_Equal(name, "f32tof16"))
	{
		name = m_f32tof16Function;
	}
    else if (String_Equal(name, "tex2Dlod"))
    {
        name = m_tex2DlodFunction;
    }
    else if (String_Equal(name, "tex2Dbias"))
    {
        name = m_tex2DbiasFunction;
    }
    else if (String_Equal(name, "tex2Dgrad"))
    {
        name = m_tex2DgradFunction;
    }
    else if (String_Equal(name, "tex2DArray"))
    {
        name = "texture";
    }
    else if (String_Equal(name, "texCUBEbias"))
    {
        name = m_texCUBEbiasFunction;
    }
	else if( String_Equal( name, "texCUBElod" ) )
	{
		name = m_texCUBElodFunction;
	}
    else if (String_Equal(name, "atan2"))
    {
        name = "atan";
    }
    else if (String_Equal(name, "sincos"))
    {
        name = m_sinCosFunction;

    }
    else if (String_Equal(name, "fmod"))
    {
        // mod is not the same as fmod if the parameter is negative!
        // The equivalent of fmod(x, y) is x - y * floor(x/y)
        // We use the mod version for performance.
        name = "mod";
    }
    else if (String_Equal(name, "lerp"))
    {
        name = "mix";
    }
    else if (String_Equal(name, "frac"))
    {
        name = "fract";
    }
    else if (String_Equal(name, "ddx"))
    {
        name = "dFdx";
    }
    else if (String_Equal(name, "ddy"))
    {
        name = "dFdy";
    }

	else if (String_Equal(name, "countbits"))
	{
		name = "bitCount";
	}

	else if (String_Equal(name, "QuadReadAcrossDiagonal"))
	{
		name = "subgroupQuadSwapDiagonal";
	}

	else if (String_Equal(name, "QuadReadLaneAt"))
	{
		name = "subgroupQuadBroadcast";
	}


	else if (String_Equal(name, "QuadReadAcrossX"))
	{
		name = "subgroupQuadSwapHorizontal";
	}

	else if (String_Equal(name, "QuadReadAcrossY"))
	{
		name = "subgroupQuadSwapVertical";
	}

	else if (String_Equal(name, "WaveActiveAllEqual"))
	{
		name = "subgroupAllEqual";
	}

	else if (String_Equal(name, "WaveActiveBitAnd"))
	{
		name = "subgroupAnd";
	}

	else if (String_Equal(name, "WaveActiveBitOr"))
	{
		name = "subgroupOr";
	}

	else if (String_Equal(name, "WaveActiveBitXor"))
	{
		name = "subgroupXor";
	}

	else if (String_Equal(name, "WaveActiveCountBits"))
	{
		name = "subgroupBallotBitCount";
	}

	else if (String_Equal(name, "WaveActiveMax"))
	{
		name = "subgroupMax";
	}
	else if (String_Equal(name, "WaveActiveMin"))
	{
		name = "subgroupMin";
	}
	else if (String_Equal(name, "WaveActiveProduct"))
	{
		name = "subgroupMul";
	}
	else if (String_Equal(name, "WaveActiveSum"))
	{
		name = "subgroupAdd";
	}
	else if (String_Equal(name, "WaveActiveAllTrue"))
	{
		name = "subgroupAll";
	}
	else if (String_Equal(name, "WaveActiveAnyTrue"))
	{
		name = "subgroupAny";
	}
	else if (String_Equal(name, "WaveActiveBallot"))
	{
		name = "subgroupBallot";
	}

	else if (String_Equal(name, "WaveIsFirstLane"))
	{
		name = "subgroupElect";
	}

	else if (String_Equal(name, "WavePrefixCountBits"))
	{
		name = "subgroupBallotExclusiveBitCount";
	}
	
	else if (String_Equal(name, "WavePrefixProduct"))
	{
		name = "subgroupExclusiveMul";
	}

	else if (String_Equal(name, "WavePrefixSum"))
	{
		name = "subgroupExclusiveAdd";
	}

	else if (String_Equal(name, "WaveReadLaneFirst"))
	{
		name = "subgroupBroadcastFirst";
	}

	else if (String_Equal(name, "WaveReadLaneAt"))
	{
		name = "subgroupBroadcast";
	}	

	else if (String_Equal(name, "InterlockedAdd"))
	{
		name = "atomicAdd";
	}
	else if (String_Equal(name, "InterlockedAnd"))
	{
		name = "atomicAnd";
	}
	else if (String_Equal(name, "InterlockedOr"))
	{
		name = "atomicOr";
	}
	else if (String_Equal(name, "InterlockedXor"))
	{
		name = "atomicXor";
	}
	else if (String_Equal(name, "InterlockedMin"))
	{
		name = "atomicMin";
	}
	else if (String_Equal(name, "InterlockedMax"))
	{
		name = "atomicMax";
	}
	else if (String_Equal(name, "InterlockedExchange"))
	{
		name = "atomicExchange";
	}
	else if (String_Equal(name, "InterlockedCompareExchange"))
	{
		name = "atomicCompSwap";
	}	
	else if (String_Equal(name, "InterlockedAnd"))
	{
		name = "atomicAnd";
	}
	else if (String_Equal(name, "InterlockedOr"))
	{
		name = "atomicOr";
	}
	else if (String_Equal(name, "InterlockedXor"))
	{
		name = "atomicXor";
	}
	else if (String_Equal(name, "InterlockedMin"))
	{
		name = "atomicMin";
	}
	else if (String_Equal(name, "InterlockedMax"))
	{
		name = "atomicMax";
	}
	else if (String_Equal(name, "InterlockedExchange"))
	{
		name = "atomicExchange";
	}
	else if (String_Equal(name, "InterlockedCompareExchange"))
	{
		name = "atomicCompSwap";
	}	
	else if (String_Equal(name, "GroupMemoryBarrierWithGroupSync"))
	{
		name = "barrier(); groupMemoryBarrier";
	}
	else if (String_Equal(name, "GroupMemoryBarrier"))
	{
		name = "groupMemoryBarrier";
	}
	else if (String_Equal(name, "DeviceMemoryBarrierWithGroupSync"))
	{
		name = "barrier(); memoryBarrierImage(); memoryBarrier";
	}
	else if (String_Equal(name, "DeviceMemoryBarrier"))
	{
		name = "memoryBarrierImage(); memoryBarrier";
	}
	else if (String_Equal(name, "AllMemoryBarrierWithGroupSync"))
	{
		name = "barrier(); groupMemoryBarrier();  memoryBarrierImage(); memoryBarrier";
	}
	else if (String_Equal(name, "AllMemoryBarrier"))
	{
		name = "barrier(); groupMemoryBarrier();  memoryBarrierImage(); memoryBarrier";
	}
    else 
    {
		// if it is one of StructuredBuffer's Name
		for (int index = 0; index < m_StructuredBufferCounter; index++)
		{
			if (String_Equal(m_StructuredBufferNames[index], name))
			{
				HLSLBuffer *buffer =  m_tree->FindBuffer(name);

				if (buffer->bArray && buffer->arrayDimension > 0)
					m_writer.Write("%s", name, name);			
				else
					m_writer.Write("%s_Data", name);
				return;
			}
		}

		// if it is one of PushConstaantBuffer's data's Name
		
			for (int index = 0; index < m_PushConstantBufferCounter; index++)
			{
				//HLSLConstantBuffer* buffer = static_cast<HLSLConstantBuffer*>(m_PushConstantBuffers[index]);

				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(m_PushConstantBuffers[index]);
				HLSLDeclaration* field = buffer->field;

				while (field != NULL)
				{
					if (!field->hidden)
					{
						if (String_Equal(field->name, name))
						{
							m_writer.Write("%s.%s", buffer->name, name);
							return;
						}
					}
					field = (HLSLDeclaration*)field->nextStatement;
				}
			}
		
		

        // The identifier could be a GLSL reserved word (if it's not also a HLSL reserved word).
        name = GetSafeIdentifierName(name);
    }
    m_writer.Write("%s", name);

}

void GLSLGenerator::OutputArguments(HLSLArgument* argument)
{
    int numArgs = 0;

	bool bExist = true;

	//DomainShader
	if (argument != NULL && m_target == Target_DomainShader)
	{
		argument = argument->nextArgument;
	}

	bool bcount = true;

	HLSLArgument* prevArg = NULL;

    while (argument != NULL)
    {
		if (argument->type.baseType == HLSLBaseType_InputPatch || argument->type.baseType == HLSLBaseType_OutputPatch)
		{
			bExist = false;
			argument = argument->nextArgument;
			continue;
		}
		else if (m_target == Target_GeometryShader && argument->modifier == HLSLArgumentModifier_Inout)
		{
			bExist = false;
			argument = argument->nextArgument;
			continue;
		}
		else
		{			
			bExist = true;
		}

		if (prevArg == NULL)
		{

		}
		else if (prevArg->preprocessor == NULL && numArgs > 0 && bExist)
		{
			m_writer.Write(", ");
		}

		if (argument->preprocessor)
		{

			HLSLpreprocessor* pre = (HLSLpreprocessor*)argument->preprocessor;
			m_writer.Write("\n");
			if (pre->type == HLSLBaseType_PreProcessorIf)
			{
				OutputStatements(0, argument->preprocessor);
				bcount = true;
			}
			else if (pre->type == HLSLBaseType_PreProcessorIfDef)
			{
				OutputStatements(0, argument->preprocessor);
				bcount = true;
			}
			else if (pre->type == HLSLBaseType_PreProcessorElse)
			{
				OutputStatements(0, argument->preprocessor);
				bcount = false;
			}
			else if (pre->type == HLSLBaseType_PreProcessorEndif)
			{
				OutputStatements(0, argument->preprocessor);
				bcount = true;
			}
			else
			{
				bcount = false;
			}

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

		if (argument->preprocessor)
		{

		}
		else
		{
			if (bExist)
				OutputDeclaration(argument->type, argument->name);

			if (bcount)
				++numArgs;
		}

		prevArg = argument;
		argument = argument->nextArgument;
    }
}

void GLSLGenerator::GetRegisterNumbering(const char* registerName, char* dst)
{
	int numberingCounter = 0;
	for (int i = 0; i < (int)strlen(registerName); i++)
	{
		if ('0' <= registerName[i] && registerName[i] <= '9')
		{
			dst[numberingCounter++] = registerName[i];
		}
	}

	dst[numberingCounter] = NULL;
}

int GLSLGenerator::GetLayoutSetNumbering(const char* registerSpaceName)
{
	if (registerSpaceName == NULL)
		return 0;

	//space
	char temp[128];
	strcpy(temp, registerSpaceName);

	char* end = temp + (int)strlen(temp);
	return String_ToInteger(&registerSpaceName[5], &end);
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
					m_writer.Write("local_size_x = %s, ", attribute->numGroupXstr);

				if (attribute->numGroupY != 0)
					m_writer.Write("local_size_y = %d, ", attribute->numGroupY);
				else
					m_writer.Write("local_size_y = %s, ", attribute->numGroupYstr);


				if (attribute->numGroupZ != 0)
					m_writer.Write("local_size_z = %d", attribute->numGroupZ);
				else
					m_writer.Write("local_size_z = %s", attribute->numGroupZstr);

				m_writer.Write(") in;");

				m_writer.EndLine();
			}
			else if (String_Equal(attributeName, "maxvertexcount"))
			{
				m_writer.EndLine();
				m_writer.Write("layout(%s, max_vertices = %d) out;", m_outputGeometryType, attribute->maxVertexCount);
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

			


			if (m_target == Target_DomainShader && m_domain != NULL && m_partitioning != NULL && m_outputtopology != NULL)
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

		if (statement->nodeType == HLSLNodeType_Preprocessor)
		{
			HLSLpreprocessor* preprocessor = static_cast<HLSLpreprocessor*>(statement);

			switch (preprocessor->type)
			{
			case HLSLBaseType_UserMacro:
			{
				m_writer.Write(0, "// USERMACRO: %s ", preprocessor->name);
				HLSLExpression* hlslExp = preprocessor->userMacroExpression;
				m_writer.Write(0, "[");

				while (hlslExp)
				{
					if (hlslExp != preprocessor->userMacroExpression)
					{
						m_writer.Write(0, ",");
					}

					OutputExpression(hlslExp, false);
					hlslExp = hlslExp->nextExpression;
				}

				m_writer.Write(0, "]");
				m_writer.EndLine();
			}
			break;
				break;
			case HLSLBaseType_PreProcessorDefine:
				m_writer.Write(0, "#define %s", preprocessor->name);
				break;
			case HLSLBaseType_PreProcessorIf:
				m_writer.Write(0, "#if %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorElif:
				m_writer.Write(0, "#elif %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorElse:
				m_writer.Write(0, "#else");
				break;
			case HLSLBaseType_PreProcessorEndif:
				m_writer.WriteLine(0, "#endif");
				break;
			case HLSLBaseType_PreProcessorIfDef:
				m_writer.Write(0, "#ifdef %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorIfnDef:
				m_writer.Write(0, "#ifndef %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorUndef:
				m_writer.Write(0, "#undef %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorInclude:
				m_writer.WriteLine(0, "#include %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorLine:
				break;
			case HLSLBaseType_PreProcessorPragma:
				break;
			default:

				if (preprocessor->expression)
				{
					m_writer.Write("#define %s ", preprocessor->name);
					HLSLExpression* expression = preprocessor->expression;

					while (expression)
					{
						OutputExpression(expression);
						expression = expression->nextExpression;
					}
				}

				//m_writer.EndLine("");
				break;
			}

			m_writer.EndLine("");

		}
		else if (statement->nodeType == HLSLNodeType_TextureState)
		{
			HLSLTextureState* textureState = static_cast<HLSLTextureState*>(statement);

			if (IsTexture(textureState->type.baseType))
			{
				PrintPreprocessors(textureState->line);
				m_writer.BeginLine(indent, textureState->fileName, textureState->line);

				char registerNumbering[4];
				GetRegisterNumbering(textureState->registerName, registerNumbering);


				m_writer.Write("layout(set = %d, binding = %s) uniform ", GetLayoutSetNumbering(textureState->registerSpaceName), registerNumbering);

				switch (textureState->type.baseType)
				{
				case HLSLBaseType_Texture1D:
					m_writer.Write("texture1D");
					break;
				case HLSLBaseType_Texture1DArray:
					m_writer.Write("texture1DArray");
					break;
				case HLSLBaseType_Texture2D:
					m_writer.Write("texture2D");
					break;
				case HLSLBaseType_Texture2DArray:
					m_writer.Write("texture2DArray");
					break;
				case HLSLBaseType_Texture3D:
					m_writer.Write("texture3D");
					break;
				case HLSLBaseType_Texture2DMS:
					m_writer.Write("texture2DMS");
					break;
				case HLSLBaseType_Texture2DMSArray:
					m_writer.Write("texture2DMSArray");
					break;
				case HLSLBaseType_TextureCube:
					m_writer.Write("textureCube");
					break;
				case HLSLBaseType_TextureCubeArray:
					m_writer.Write("textureCubeArray");
					break;
				default:
					break;
				}

				m_writer.Write(" %s", textureState->name);

				if (textureState->bArray)
				{
					for (int i = 0; i < (int)textureState->arrayDimension; i++)
					{
						if (!String_Equal(textureState->arrayIdentifier[i], ""))
						{
							m_writer.Write("[%s]", textureState->arrayIdentifier);
						}
						else if (textureState->arrayIndex[i] > 0)
						{
							m_writer.Write("[%u]", textureState->arrayIndex[i]);
						}
						else
						{
							//glsl are not allow unbound texture array
							m_writer.Write("[256]");
						}
					}
				}

				m_writer.EndLine(";");
			}
			else if (IsRWTexture(textureState->type.baseType) || IsRasterizerOrderedTexture(textureState->type.baseType))
			{
				PrintPreprocessors(textureState->line);
				m_writer.BeginLine(indent, textureState->fileName, textureState->line);

				char registerNumbering[4];
				GetRegisterNumbering(textureState->registerName, registerNumbering);

				m_writer.Write("layout(set = %d, binding = %s, ", GetLayoutSetNumbering(textureState->registerSpaceName), registerNumbering);

				bool bUint = false;

				switch (textureState->type.elementType)
				{
				case HLSLBaseType_Float:
					m_writer.Write("r32f");
					break;
				case HLSLBaseType_Float2:
					m_writer.Write("rg32f");
					break;
				case HLSLBaseType_Float4:
					m_writer.Write("rgba32f");
					break;

				case HLSLBaseType_Half:
					m_writer.Write("r16f");
					break;
				case HLSLBaseType_Half2:
					m_writer.Write("rg16f");
					break;
				case HLSLBaseType_Half4:
					m_writer.Write("rgba16f");
					break;

				case HLSLBaseType_Int:
					m_writer.Write("r32i");
					break;
				case HLSLBaseType_Int2:
					m_writer.Write("rg32i");
					break;
				case HLSLBaseType_Int4:
					m_writer.Write("rgba32i");
					break;

				case HLSLBaseType_Uint:
					m_writer.Write("r32ui"); bUint = true;
					break;
				case HLSLBaseType_Uint2:
					m_writer.Write("rg32ui"); bUint = true;
					break;
				case HLSLBaseType_Uint4:
					m_writer.Write("rgba32ui"); bUint = true;
					break;
				default:
					break;
				}

				m_writer.Write(") uniform ");


				switch (textureState->type.baseType)
				{
				case HLSLBaseType_RWTexture1D:

					if (bUint)
						m_writer.Write("uimage1D");
					else
						m_writer.Write("image1D");
					break;
				case HLSLBaseType_RWTexture1DArray:
					if (bUint)
						m_writer.Write("uimage1DArray");
					else
						m_writer.Write("image1DArray");
					break;
				case HLSLBaseType_RWTexture2D:
					if (bUint)
						m_writer.Write("uimage2D");
					else
						m_writer.Write("image2D");
					break;
				case HLSLBaseType_RWTexture2DArray:
					if (bUint)
						m_writer.Write("uimage2DArray");
					else
						m_writer.Write("image2DArray");
					break;
				case HLSLBaseType_RWTexture3D:
					if (bUint)
						m_writer.Write("uimage3D");
					else
						m_writer.Write("image3D");
					break;
				default:
					break;
				}

				m_writer.Write(" %s", textureState->name);

				if (textureState->bArray)
				{
					for (int i = 0; i < (int)textureState->arrayDimension; i++)
					{
						if (!String_Equal(textureState->arrayIdentifier[i], ""))
						{
							m_writer.Write("[%s]", textureState->arrayIdentifier);
						}
						else if (textureState->arrayIndex[i] > 0)
						{
							m_writer.Write("[%u]", textureState->arrayIndex[i]);
						}
						else
						{
							m_writer.Write("[]");
						}
					}
				}

				m_writer.EndLine(";");
			}
			
		}	
		/*
		else if (statement->nodeType == HLSLNodeType_RWTextureState)
		{
			HLSLRWTextureState* textureState = static_cast<HLSLRWTextureState*>(statement);

			
		}
		*/
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

			PrintPreprocessors(samplerState->line);
			m_writer.BeginLine(indent, samplerState->fileName, samplerState->line);			

			if (samplerState->bStructured)
			{

				if (samplerState->IsComparisionState)
				{
					m_writer.WriteLineTagged(indent, samplerState->fileName, samplerState->line, "samplerShadow %s {", samplerState->name);
				}
				else
				{
					m_writer.WriteLineTagged(indent, samplerState->fileName, samplerState->line, "sampler %s {", samplerState->name);
				}

				HLSLSamplerStateExpression* expression = samplerState->expression;

				while (expression != NULL)
				{
					if (!expression->hidden)
					{
						PrintPreprocessors(expression->line);
						m_writer.BeginLine(indent + 1, expression->fileName, expression->line);

						m_writer.Write("%s = %s", expression->lvalue, expression->rvalue);
						m_writer.Write(";");
						m_writer.EndLine();
					}
					expression = expression->nextExpression;
				}
				m_writer.WriteLine(indent, "};");
			}
			else
			{
				char registerNumbering[4];
				GetRegisterNumbering(samplerState->registerName, registerNumbering);

				m_writer.Write("layout(set = %d, binding = %s) uniform ", GetLayoutSetNumbering(samplerState->registerSpaceName), registerNumbering);

				if (samplerState->IsComparisionState)
				{
					m_writer.Write("samplerShadow %s", samplerState->name);
				}
				else
				{
					m_writer.Write("sampler %s", samplerState->name);
				}

				if (samplerState->type.array)
				{
					m_writer.Write("[");
					OutputExpression(samplerState->type.arraySize, false);
					m_writer.Write("]");
				}

				m_writer.EndLine(";");
			}
		}
		else if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);

            // GLSL doesn't seem have texture uniforms, so just ignore them.
            if (declaration->type.baseType != HLSLBaseType_Texture)
            {
                m_writer.BeginLine(indent, declaration->fileName, declaration->line);
                if (indent == 0)
                {
                    // At the top level, we need the "uniform" keyword.
                    //m_writer.Write("uniform ");
					m_writer.Write("const ");
                }
                OutputDeclaration(declaration);
                m_writer.EndLine(";");
            }
        }
        else if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
			
            m_writer.WriteLine(indent, "struct %s", structure->name);
			m_writer.WriteLine(indent, "{");
            HLSLStructField* field = structure->field;
            while (field != NULL)
            {
                m_writer.BeginLine(indent + 1, field->fileName, field->line);

				if (m_target == Target_HullShader)
				{
					if (field->type.array && field->type.arraySize == NULL)
						field->type.array = false;					
				}

				if (field->preProcessor)
				{
					if (field->preProcessor->expression)
						OutputExpression(field->preProcessor->expression);
					else
					{
						switch (field->preProcessor->type)
						{
						case HLSLBaseType_PreProcessorIf:
							m_writer.Write(0, "#if %s", field->preProcessor->contents);
							break;
						case HLSLBaseType_PreProcessorElif:
							m_writer.Write(0, "#elif %s", field->preProcessor->contents);
							break;
						case HLSLBaseType_PreProcessorElse:
							m_writer.Write(0, "#else");
							break;
						case HLSLBaseType_PreProcessorEndif:
							m_writer.WriteLine(0, "#endif");
							break;
						case HLSLBaseType_PreProcessorIfDef:
							m_writer.Write(0, "#ifdef %s", field->preProcessor->contents);
							break;
						case HLSLBaseType_PreProcessorIfnDef:
							m_writer.Write(0, "#ifndef %s", field->preProcessor->contents);
							break;
						case HLSLBaseType_PreProcessorUndef:
							m_writer.Write(0, "#undef %s", field->preProcessor->contents);
							break;
						case HLSLBaseType_PreProcessorInclude:
							m_writer.WriteLine(0, "#include %s", field->preProcessor->contents);
							break;
						case HLSLBaseType_PreProcessorLine:

							break;
						case HLSLBaseType_PreProcessorPragma:

							break;
						default:
							break;
						}
					}
				}
				else
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
			if (statement->type.baseType == HLSLBaseType_RWBuffer)
			{
				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
				HLSLDeclaration* field = buffer->field;

				//PrintPreprocessors(buffer->line);
				m_writer.BeginLine(indent, buffer->fileName, buffer->line);
				m_writer.Write("RWBuffer<%s> %s", getElementTypeAsStrGLSL(buffer->type), buffer->name);


				m_writer.EndLine(";");

			}
			else if (statement->type.baseType == HLSLBaseType_RWStructuredBuffer)
			{
				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
				HLSLDeclaration* field = buffer->field;


				strcpy(m_StructuredBufferNames[m_StructuredBufferCounter++], buffer->name);

				PrintPreprocessors(buffer->line);
				m_writer.BeginLine(indent, buffer->fileName, buffer->line);

				char registerNumbering[4];
				GetRegisterNumbering(buffer->registerName, registerNumbering);

				m_writer.Write("layout(set=%d, binding=%s) ", GetLayoutSetNumbering(buffer->registerSpaceName), registerNumbering);

				if (buffer->bArray && buffer->arrayDimension > 0)
					m_writer.Write("buffer %s_Block", buffer->name);
				else
					m_writer.Write("buffer %s", buffer->name);
								
				m_writer.WriteLine(0, "\n{");



				if (buffer->type.elementType != HLSLBaseType_Unknown)
				{
					//avoid for duplicating buffer name and its element name
					m_writer.WriteLine(indent + 1, "%s %s_Data[];", getElementTypeAsStrGLSL(buffer->type), buffer->name);
				}
				else
				{
					//m_writer.Write("RWStructuredBuffer");

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

					m_writer.Write(" %s_Data[];", buffer->name);
					m_writer.EndLine();
				}

				if (buffer->bArray && buffer->arrayDimension > 0)
				{
					m_writer.Write("}%s", buffer->name);

					for (int i = 0; i < (int)buffer->arrayDimension; i++)
					{
						if (!String_Equal(buffer->arrayIdentifier[i], ""))
							m_writer.Write("[%s]", buffer->arrayIdentifier);
						else if (buffer->arrayIndex[i] > 0)
							m_writer.Write("[%u]", buffer->arrayIndex[i]);
						else
							m_writer.Write("[]");
					}

					m_writer.EndLine(";");
				}
				else
				{
					m_writer.WriteLine(0, "};");
				}
			}
			else if (statement->type.baseType == HLSLBaseType_CBuffer || statement->type.baseType == HLSLBaseType_TBuffer)
			{
				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
				//OutputBuffer(indent, buffer);

				m_writer.BeginLine(indent, buffer->fileName, buffer->line);

				char registerNumbering[4];
				GetRegisterNumbering(buffer->registerName, registerNumbering);

				if (buffer->bPushConstant)
				{
					m_PushConstantBuffers[m_PushConstantBufferCounter++] = buffer;
					m_writer.Write("layout(push_constant) ");
				}
				else
					m_writer.Write("layout(set = %d, binding = %s) ", GetLayoutSetNumbering(buffer->registerSpaceName), registerNumbering);


				if (buffer->bPushConstant)
					m_writer.Write("uniform %s_Block", buffer->name);
				else
				{
					m_writer.Write("uniform %s", buffer->name);
				}

				m_writer.EndLine();
				m_writer.EndLine("{");

				//m_isInsideBuffer = true;


				HLSLDeclaration* field = buffer->field;

				while (field != NULL)
				{
					if (!field->hidden)
					{

						m_writer.BeginLine(indent + 1, field->fileName, field->line);
						OutputDeclaration(field->type, field->name);
						m_writer.Write(";");
						m_writer.EndLine();
					}
					field = (HLSLDeclaration*)field->nextStatement;
				}

				if (buffer->bPushConstant)
					m_writer.WriteLine(indent, "}%s;", buffer->name);
				else
					m_writer.WriteLine(indent, "};");


			}
			else if (statement->type.baseType ==  HLSLBaseType_ByteAddressBuffer)
			{
				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
				HLSLDeclaration* field = buffer->field;

				strcpy(m_StructuredBufferNames[m_StructuredBufferCounter++], buffer->name);

				PrintPreprocessors(buffer->line);
				m_writer.BeginLine(indent, buffer->fileName, buffer->line);

				char registerNumbering[4];
				GetRegisterNumbering(buffer->registerName, registerNumbering);

				m_writer.Write("layout(set = %d, binding = %s) ", GetLayoutSetNumbering(buffer->registerSpaceName), registerNumbering);

				if (buffer->bArray && buffer->arrayDimension > 0)
					m_writer.Write("buffer %s_Block", buffer->name);
				else
					m_writer.Write("buffer %s", buffer->name);

				m_writer.WriteLine(0, "\n{");
				m_writer.WriteLine(1, "%s %s_Data[];", "uint", buffer->name);

				if (buffer->bArray && buffer->arrayDimension > 0)
				{
					m_writer.Write("}%s", buffer->name);

					for (int i = 0; i < (int)buffer->arrayDimension; i++)
					{
						if (!String_Equal(buffer->arrayIdentifier[i], ""))
							m_writer.Write("[%s]", buffer->arrayIdentifier);
						else if (buffer->arrayIndex[i] > 0)
							m_writer.Write("[%u]", buffer->arrayIndex[i]);
						else
							m_writer.Write("[]");
					}

					m_writer.EndLine(";");
				}
				else
				{
					m_writer.WriteLine(0, "};");
				}

			}
			else if (statement->type.baseType == HLSLBaseType_ConstantBuffer)
			{
				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
				


				m_writer.BeginLine(indent, buffer->fileName, buffer->line);

				char registerNumbering[4];
				GetRegisterNumbering(buffer->registerName, registerNumbering);


				if (buffer->bPushConstant)
				{
					m_PushConstantBuffers[m_PushConstantBufferCounter++] = buffer;
					m_writer.Write("layout(push_constant) ");
				}
				else
					m_writer.Write("layout(set= % d, binding = %s) ", GetLayoutSetNumbering(buffer->registerSpaceName), registerNumbering);


				if (buffer->bPushConstant)
					m_writer.Write("uniform %s_Block", buffer->name);
				else
				{
					if (buffer->type.elementType != HLSLBaseType_Unknown)
						m_writer.Write("uniform %s_Block", buffer->name);
					else
						m_writer.Write("uniform %s", buffer->name);
				}

				m_writer.EndLine();
				m_writer.EndLine("{");

				//if (buffer->type.elementType != HLSLBaseType_Unknown)
				//{
				//	m_writer.WriteLine(indent + 1, "%s %s_Data;", getElementTypeAsStrGLSL(buffer->type), buffer->name);
				//}
				//else
				{
					HLSLStruct* pStruct = m_tree->FindGlobalStruct(getElementTypeAsStrGLSL(buffer->type));
					HLSLStructField* field = pStruct->field;
					//HLSLDeclaration* field = pStruct->field;//->field;

					while (field != NULL)
					{
						if (!field->hidden)
						{

							m_writer.BeginLine(indent + 1, field->fileName, field->line);
							OutputDeclaration(field->type, field->name);
							m_writer.Write(";");
							m_writer.EndLine();
						}
						field = (HLSLStructField*)field->nextField;
					}					
				}

				if (buffer->bPushConstant)
					m_writer.WriteLine(indent, "}%s;", buffer->name);
				else
				{
					if (buffer->type.elementType != HLSLBaseType_Unknown)
						m_writer.WriteLine(indent, "}%s;", buffer->name);
					else
						m_writer.WriteLine(indent, "};");
				}
				
			}
			else if (statement->type.baseType == HLSLBaseType_StructuredBuffer || statement->type.baseType == HLSLBaseType_PureBuffer)
			{
				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
				HLSLDeclaration* field = buffer->field;

				strcpy(m_StructuredBufferNames[m_StructuredBufferCounter++], buffer->name);

				PrintPreprocessors(buffer->line);
				m_writer.BeginLine(indent, buffer->fileName, buffer->line);

				char registerNumbering[4];
				GetRegisterNumbering(buffer->registerName, registerNumbering);

				m_writer.Write("layout(set = %d, binding = %s) ", GetLayoutSetNumbering(buffer->registerSpaceName), registerNumbering);

				if (buffer->bArray && buffer->arrayDimension > 0)
					m_writer.Write("buffer %s_Block", buffer->name);
				else
					m_writer.Write("buffer %s", buffer->name);

				m_writer.WriteLine(0, "\n{");
				m_writer.WriteLine(1, "%s %s_Data[];", getElementTypeAsStrGLSL(buffer->type), buffer->name);

				if (buffer->bArray && buffer->arrayDimension > 0)
				{
					m_writer.Write("}%s", buffer->name);

					for (int i = 0; i < (int)buffer->arrayDimension; i++)
					{
						if (!String_Equal(buffer->arrayIdentifier[i], ""))
							m_writer.Write("[%s]", buffer->arrayIdentifier);
						else if (buffer->arrayIndex[i] > 0)
							m_writer.Write("[%u]", buffer->arrayIndex[i]);
						else
							m_writer.Write("[]");
					}

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
            const char* functionName   = GetSafeIdentifierName(function->name);
            const char* returnTypeName = GetTypeName(function->returnType);

            m_writer.BeginLine(indent, function->fileName, function->line);

			if (String_Equal(functionName, "main"))
				m_writer.Write("%s HLSL%s(", returnTypeName, functionName);	
			else
				m_writer.Write("%s %s(", returnTypeName, functionName);
								
			OutputArguments(function->argument);

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
            m_writer.BeginLine(indent, statement->fileName, statement->line);
            OutputExpression(expressionStatement->expression);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_ReturnStatement)
        {
            HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
            if (returnStatement->expression != NULL)
            {
                m_writer.BeginLine(indent, returnStatement->fileName, returnStatement->line);
                m_writer.Write("return ");
                OutputExpression(returnStatement->expression, returnType);
                m_writer.EndLine(";");
            }
            else
            {
                m_writer.WriteLineTagged(indent, returnStatement->fileName, returnStatement->line, "return;");
            }
        }
        else if (statement->nodeType == HLSLNodeType_DiscardStatement)
        {
            HLSLDiscardStatement* discardStatement = static_cast<HLSLDiscardStatement*>(statement);
            if (m_target == Target_FragmentShader)
            {
                m_writer.WriteLineTagged(indent, discardStatement->fileName, discardStatement->line, "discard;");
            }
        }
        else if (statement->nodeType == HLSLNodeType_BreakStatement)
        {
            HLSLBreakStatement* breakStatement = static_cast<HLSLBreakStatement*>(statement);
            m_writer.WriteLineTagged(indent, breakStatement->fileName, breakStatement->line, "break;");
        }
        else if (statement->nodeType == HLSLNodeType_ContinueStatement)
        {
            HLSLContinueStatement* continueStatement = static_cast<HLSLContinueStatement*>(statement);
            m_writer.WriteLineTagged(indent, continueStatement->fileName, continueStatement->line, "continue;");
        }
        else if (statement->nodeType == HLSLNodeType_IfStatement)
        {
            HLSLIfStatement* ifStatement = static_cast<HLSLIfStatement*>(statement);
            //m_writer.BeginLine(indent, ifStatement->fileName, ifStatement->line);
            m_writer.Write(indent, "if(");
            OutputExpression(ifStatement->condition, &kBoolType);
			m_writer.EndLine(")");

			if (ifStatement->statement != NULL)
			{
				if (ifStatement->statement->nodeType == HLSLNodeType_Preprocessor)
				{
					OutputStatements(indent + 1, ifStatement->statement, returnType);
				}
				else
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

			


			for (int i = 0; i< ifStatement->elseifStatementCounter; i++)
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
			for (int i = 0; i< switchStatement->caseCounter; i++)
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
            m_writer.BeginLine(indent, forStatement->fileName, forStatement->line);
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

			m_writer.BeginLine(indent, whileStatement->fileName, whileStatement->line);
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
			PrintPreprocessors(blockStatement->line);
			m_writer.WriteLineTagged(indent, blockStatement->fileName, blockStatement->line, "{");
			OutputStatements(indent + 1, blockStatement->statement);
			m_writer.WriteLine(indent, "}");
		}
        else
        {
            // Unhanded statement type.
            ASSERT(0);
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

        m_writer.WriteLineTagged(indent, buffer->fileName, buffer->line, "uniform vec4 %s%s[%d];", m_options.constantBufferPrefix, buffer->name, uniformSize);
    }
    else
    {
        m_writer.WriteLineTagged(indent, buffer->fileName, buffer->line, "layout (std140) uniform %s%s", m_options.constantBufferPrefix, buffer->name);
		m_writer.Write(indent, "{");
        HLSLDeclaration* field = buffer->field;
        while (field != NULL)
        {
            m_writer.BeginLine(indent + 1, field->fileName, field->line);
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
    ASSERT(size <= 4);

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
        int arraySize = 0;
        m_tree->GetExpressionValue(type.arraySize, arraySize);

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
            Error("Unknown type %s", type.typeName);
        }
    }
    else
    {
        Error("Constant buffer layout is not supported for %s", GetTypeName(type));
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
        Error("Constant buffer layout is not supported for %s", GetTypeName(type));
    }
}

HLSLBuffer* GLSLGenerator::GetBufferAccessExpression(HLSLExpression* expression)
{
    if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);

        if (identifierExpression->global)
        {
            HLSLDeclaration * declaration = m_tree->FindGlobalDeclaration(identifierExpression->name);
            if (declaration && declaration->buffer)
                return declaration->buffer;
        }
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
    else if (type.baseType == HLSLBaseType_Float)
    {
        m_writer.Write("%s%s[", m_options.constantBufferPrefix, buffer->name);
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
        m_writer.Write("%d].%c", index / 4, "xyzw"[index % 4]);
    }
    else if (type.baseType == HLSLBaseType_Float2)
    {
        m_writer.Write("%s%s[", m_options.constantBufferPrefix, buffer->name);
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
        m_writer.Write("%d].%s", index / 4, index % 4 == 0 ? "xy" : index % 4 == 1 ? "yz" : "zw");
    }
    else if (type.baseType == HLSLBaseType_Float3)
    {
        m_writer.Write("%s%s[", m_options.constantBufferPrefix, buffer->name);
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
        m_writer.Write("%d].%s", index / 4, index % 4 == 0 ? "xyz" : "yzw");
    }
    else if (type.baseType == HLSLBaseType_Float4)
    {
        m_writer.Write("%s%s[", m_options.constantBufferPrefix, buffer->name);
        unsigned int index = OutputBufferAccessIndex(expression, postOffset);
        ASSERT(index % 4 == 0);
        m_writer.Write("%d]", index / 4);
    }
    else if (type.baseType == HLSLBaseType_Float4x4)
    {
        m_writer.Write("mat4(");
        for (int i = 0; i < 4; ++i)
        {
            m_writer.Write("%s%s[", m_options.constantBufferPrefix, buffer->name);
            unsigned int index = OutputBufferAccessIndex(expression, postOffset + i * 4);
            ASSERT(index % 4 == 0);
            m_writer.Write("%d]%c", index / 4, i == 3 ? ')' : ',');
        }
    }
    else if (type.baseType == HLSLBaseType_UserDefined)
    {
        HLSLStruct * st = m_tree->FindGlobalStruct(type.typeName);

        if (st)
        {
            m_writer.Write("%s(", st->name);

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
            Error("Unknown type %s", type.typeName);
        }
    }
    else
    {
        Error("Constant buffer layout is not supported for %s", GetTypeName(type));
    }
}

unsigned int GLSLGenerator::OutputBufferAccessIndex(HLSLExpression* expression, unsigned int postOffset)
{
    if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
        ASSERT(identifierExpression->global);

        HLSLDeclaration * declaration = m_tree->FindGlobalDeclaration(identifierExpression->name);
        ASSERT(declaration);

        HLSLBuffer * buffer = declaration->buffer;
        ASSERT(buffer);

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
        ASSERT(type.baseType == HLSLBaseType_UserDefined);

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
            Error("Unknown type %s", type.typeName);
        }
    }
    else if (expression->nodeType == HLSLNodeType_ArrayAccess)
    {
        HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);

        const HLSLType& type = arrayAccess->array->expressionType;
        ASSERT(type.array);

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
        ASSERT(!"IsBufferAccessExpression should have returned false");
    }

    return 0;
}

HLSLFunction* GLSLGenerator::FindFunction(HLSLRoot* root, const char* name)
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

HLSLStruct* GLSLGenerator::FindStruct(HLSLRoot* root, const char* name)
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


const char* GLSLGenerator::GetAttribQualifier(AttributeModifier modifier)
{
    if (m_versionLegacy)
    {
        if (m_target == Target_VertexShader)
            return (modifier == AttributeModifier_In) ? "attribute" : "varying";
        else
            return (modifier == AttributeModifier_In) ? "varying" : "out";
    }
    else
    {
        return (modifier == AttributeModifier_In) ? "in" : "out";
    }
}

void GLSLGenerator::OutputAttribute(const HLSLType& type, const char* semantic, AttributeModifier modifier, int *counter)
{
    const char* qualifier = GetAttribQualifier(modifier);
    const char* prefix = (modifier == AttributeModifier_In) ? m_inAttribPrefix : m_outAttribPrefix;

    HLSLRoot* root = m_tree->GetRoot();
    if (type.baseType == HLSLBaseType_UserDefined)
    {
        // If the argument is a struct with semantics specified, we need to
        // grab them.
        HLSLStruct* structDeclaration = FindStruct(root, type.typeName);
        ASSERT(structDeclaration != NULL);
        HLSLStructField* field = structDeclaration->field;
        while (field != NULL)
        {
            if (field->semantic != NULL && GetBuiltInSemantic(field->semantic, modifier, field->type ) == NULL)
            {
				m_writer.Write("layout(location = %d) ", (*counter)++);

				if( (m_target == Target_HullShader && modifier == AttributeModifier_Out) || (m_target == Target_DomainShader && modifier == AttributeModifier_In))
					m_writer.Write("patch %s ", qualifier);
				else
					m_writer.Write("%s ", qualifier );
				
				char attribName[ 64 ];
				String_Printf( attribName, 64, "%s%s", prefix, field->semantic );

				if(field->type.baseType == HLSLBaseType_Int || field->type.baseType == HLSLBaseType_Uint && m_target != Target_VertexShader)
					m_writer.Write("flat ");				
					
				OutputDeclaration(field->type, attribName);

				if (m_target == Target_GeometryShader && modifier == AttributeModifier_In)
					m_writer.Write("[]");
				
				m_writer.EndLine(";");
            }
            field = field->nextField;
        }
    }
    else if (semantic != NULL && GetBuiltInSemantic(semantic, modifier, type) == NULL)
    {
		m_writer.Write("layout(location = %d) ", (*counter)++);
		m_writer.Write( "%s ", qualifier );
		char attribName[ 64 ];
		String_Printf( attribName, 64, "%s%s", prefix, semantic );

		if (type.baseType == HLSLBaseType_Int || type.baseType == HLSLBaseType_Uint && m_target != Target_VertexShader)
			m_writer.Write("flat ");

		OutputDeclaration( type, attribName );
		m_writer.EndLine(";");
    }
	else if (semantic ==  NULL && (m_target == Target_HullShader || m_target == Target_DomainShader))
	{
		HLSLStruct* structDeclaration = FindStruct(root, type.typeName);
		ASSERT(structDeclaration != NULL);

		HLSLStructField* field = structDeclaration->field;
		while (field != NULL)
		{
			if (field->semantic != NULL)
			{
				const char* builtInSemantic = GetBuiltInSemantic(field->semantic, AttributeModifier_In);

				m_writer.Write("layout(location = %d) ", (*counter)++);


				if (m_target == Target_DomainShader && modifier == AttributeModifier_In)
					m_writer.Write("patch ");

				m_writer.Write("%s ", qualifier);
				
				if (m_target == Target_HullShader && type.array)
				{
					field->type.array = true;
					field->type.arraySize = type.arraySize;
				}
				else if (m_target == Target_DomainShader)
				{					
					field->type.array = false;
				}

				char attribName[64];
				String_Printf(attribName, 64, "%s%s", m_inAttribPrefix, field->semantic);
				

				if (builtInSemantic)
				{
					//m_writer.WriteLine(1, "%s.%s = %s;", GetSafeIdentifierName(argument->name), GetSafeIdentifierName(field->name), builtInSemantic);
				}
				else
				{
					OutputDeclaration(field->type, attribName);
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
    HLSLArgument* argument = entryFunction->argument;

	int inputCounter = 0;
	int outputCounter = 0;

    while (argument != NULL)
    {
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
				String_Equal(m_geoInputdataType, argument->type.typeName);
			}
			else if (argument->modifier == HLSLArgumentModifier_Inout)
			{
				if (argument->type.baseType == HLSLBaseType_PointStream)
				{
					strcpy(m_outputGeometryType, "points");
				}
				else if (argument->type.baseType == HLSLBaseType_LineStream)
				{
					strcpy(m_outputGeometryType, "line_strip");
				}
				else if (argument->type.baseType == HLSLBaseType_TriangleStream)
				{
					strcpy(m_outputGeometryType, "triangle_strip");
				}

				if (argument->type.structuredTypeName)
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

        argument = argument->nextArgument;
    }

    // Write out the output attributes from the shader.
    OutputAttribute(entryFunction->returnType, entryFunction->semantic, AttributeModifier_Out, &outputCounter);
}

void GLSLGenerator::OutputSetOutAttribute(const char* semantic, const char* resultName)
{
    int outputIndex = -1;
    const char* builtInSemantic = GetBuiltInSemantic(semantic, AttributeModifier_Out, &outputIndex);
    if (builtInSemantic != NULL)
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
                m_writer.WriteLine(1, "vec4 temp = %s;", resultName);
                m_writer.WriteLine(1, "%s = temp * vec4(1,-1,2,1) - vec4(0,0,temp.w,0);", builtInSemantic);
            }
            else
            {
                m_writer.WriteLine(1, "%s = %s;", builtInSemantic, resultName);
            }

			if (String_Equal(semantic, "POSITION"))
			{
				m_writer.WriteLine(1, "%s%s = %s;", m_outAttribPrefix, semantic, resultName);
			}

            m_outputPosition = true;
        }
        else if (String_Equal(builtInSemantic, "gl_FragDepth"))
        {
            // If the value goes outside of the 0 to 1 range, the
            // fragment will be rejected unlike in D3D, so clamp it.
            m_writer.WriteLine(1, "%s = clamp(float(%s), 0.0, 1.0);", builtInSemantic, resultName);
        }
        else if (outputIndex >= 0)
        {
            //m_writer.WriteLine(1, "%s[%d] = %s;", builtInSemantic, outputIndex, resultName);
			m_writer.WriteLine(1, "%s%d = %s;", builtInSemantic, outputIndex, resultName);
        }
        else
        {
            m_writer.WriteLine(1, "%s = %s;", builtInSemantic, resultName);
        }
    }
    else if (m_target == Target_FragmentShader)
    {
        Error("Output attribute %s does not map to any built-ins", semantic);
    }
    else
    {
        m_writer.WriteLine(1, "%s%s = %s;", m_outAttribPrefix, semantic, resultName);
    }
}

void GLSLGenerator::OutputEntryCaller(HLSLFunction* entryFunction)
{
    HLSLRoot* root = m_tree->GetRoot();

    m_writer.WriteLine(0, "void main()");
	m_writer.WriteLine(0, "{");

    // Create local variables for each of the parameters we'll need to pass
    // into the entry point function.
    HLSLArgument* argument = entryFunction->argument;
    while (argument != NULL)
    {
		//DomainShader
		if (m_target == Target_DomainShader && (entryFunction->argument == argument || argument->type.baseType == HLSLBaseType_OutputPatch ))
		{
			argument = argument->nextArgument;
			continue;
		}

        m_writer.BeginLine(1);
        OutputDeclaration(argument->type, argument->name);
        m_writer.EndLine(";");

        //if (argument->modifier != HLSLArgumentModifier_Out)
		if ( !( (argument->modifier == HLSLArgumentModifier_Out) || (argument->modifier == HLSLArgumentModifier_Inout)))
        {
            // Set the value for the local variable.
            if (argument->type.baseType == HLSLBaseType_UserDefined)
            {
				HLSLStruct* structDeclaration = FindStruct(root, argument->type.typeName);
                ASSERT(structDeclaration != NULL);
                
				
				if (argument->type.array)
				{
					int arraySize = 0;
					m_tree->GetExpressionValue(argument->type.arraySize, arraySize);

					for (int i = 0; i < arraySize; i++)
					{
						HLSLStructField* field = structDeclaration->field;

						while (field != NULL)
						{
							if (field->semantic != NULL)
							{
								const char* builtInSemantic = GetBuiltInSemantic(field->semantic, AttributeModifier_In);
								if (builtInSemantic)
								{
									if(String_Equal(builtInSemantic, "gl_in"))
										m_writer.WriteLine(1, "%s[%d].%s = %s[%d].gl_Position;", GetSafeIdentifierName(argument->name), i, GetSafeIdentifierName(field->name), builtInSemantic, i);
									else
										m_writer.WriteLine(1, "%s[%d].%s = %s;", GetSafeIdentifierName(argument->name), i, GetSafeIdentifierName(field->name), builtInSemantic);
								}
								else
								{
									m_writer.WriteLine(1, "%s[%d].%s = %s%s[%d];", GetSafeIdentifierName(argument->name), i, GetSafeIdentifierName(field->name), m_inAttribPrefix, field->semantic, i);
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
						if (field->semantic != NULL)
						{
							const char* builtInSemantic = GetBuiltInSemantic(field->semantic, AttributeModifier_In);
							if (builtInSemantic)
							{
								m_writer.WriteLine(1, "%s.%s = %s;", GetSafeIdentifierName(argument->name), GetSafeIdentifierName(field->name), builtInSemantic);
							}
							else
							{
								m_writer.WriteLine(1, "%s.%s = %s%s;", GetSafeIdentifierName(argument->name), GetSafeIdentifierName(field->name), m_inAttribPrefix, field->semantic);
							}
						}
						field = field->nextField;
					}
				}
				
				
            }
            else if (argument->semantic != NULL)
            {
                const char* builtInSemantic = GetBuiltInSemantic(argument->semantic, AttributeModifier_In);
                if (builtInSemantic)
                {
					if (m_target == Target_DomainShader && String_Equal("gl_TessCoord", builtInSemantic))
					{
						char additionalBuiltIn[64];

						if (String_Equal(m_domain, "\"quad\"") || String_Equal(m_domain, "\"isoline\""))
							String_Printf(additionalBuiltIn, 64, "%s.xy", builtInSemantic);
						else if (String_Equal(m_domain, "\"tri\""))
							String_Printf(additionalBuiltIn, 64, "%s.xyz", builtInSemantic);

						m_writer.WriteLine(1, "%s = %s;", GetSafeIdentifierName(argument->name), additionalBuiltIn);
					}
					else
						m_writer.WriteLine(1, "%s = %s;", GetSafeIdentifierName(argument->name), builtInSemantic);
                }
                else
                {
                    m_writer.WriteLine(1, "%s = %s%s;", GetSafeIdentifierName(argument->name), m_inAttribPrefix, argument->semantic);
                }
            }
        }

        argument = argument->nextArgument;
    }


	m_writer.BeginLine(0);
	//Need to call PatchConstantFunction in Main, if it is a Hull shader
	if (m_target == Target_HullShader)
	{
		//remove double quotes

		if (String_Equal(m_patchconstantfunc, "") || m_patchconstantfunc == NULL)
		{
			// need error handling
			return;
		}

		char newFuncName[64];
		strcpy(newFuncName, m_patchconstantfunc + 1);
		newFuncName[strlen(newFuncName)-1] = 0;

		m_writer.Write("%s(", newFuncName);

		int numArgs = 0;
		argument = entryFunction->argument;

		bool bExist = true;

		//DomainShader
		if (m_target == Target_DomainShader && entryFunction->argument == argument)
		{
			argument = argument->nextArgument;
		}

		while (argument != NULL)
		{
			if (argument->type.baseType == HLSLBaseType_InputPatch || argument->type.baseType == HLSLBaseType_OutputPatch)
			{
				bExist = false;
				argument = argument->nextArgument;
				continue;
			}
			else
			{
				bExist = true;
			}

			if (numArgs > 0 && bExist)
			{
				m_writer.Write(", ");
			}

			if (bExist)
				m_writer.Write("%s", GetSafeIdentifierName(argument->name));

			argument = argument->nextArgument;
			++numArgs;
		}

		m_writer.EndLine(");");
	}

    const char* resultName = "result";

    // Call the original entry function.
    m_writer.BeginLine(1);
    if (entryFunction->returnType.baseType != HLSLBaseType_Void)
        m_writer.Write("%s %s = ", GetTypeName(entryFunction->returnType), resultName);
    
	if(String_Equal(m_entryName, "main"))
		m_writer.Write("HLSL%s(", m_entryName);
	else
		m_writer.Write("%s(", m_entryName);
	

    int numArgs = 0;
    argument = entryFunction->argument;

	bool bExist = true;

	//DomainShader
	if (m_target == Target_DomainShader && entryFunction->argument == argument)
	{
		argument = argument->nextArgument;
	}

    while (argument != NULL)
    {		

		if (argument->type.baseType == HLSLBaseType_InputPatch || argument->type.baseType == HLSLBaseType_OutputPatch)
		{
			bExist = false;
			argument = argument->nextArgument;
			continue;
		}
		else if (m_target == Target_GeometryShader && argument->modifier == HLSLArgumentModifier_Inout)
		{
			bExist = false;
			argument = argument->nextArgument;
			continue;
		}
		else
		{
			bExist = true;
		}

        if (numArgs > 0 && bExist)
        {
            m_writer.Write(", ");
        }


		if(bExist)
			m_writer.Write("%s", GetSafeIdentifierName(argument->name));
        

        argument = argument->nextArgument;
        ++numArgs;
    }
    m_writer.EndLine(");");

    // Copy values from the result into the out attributes as necessary.
    argument = entryFunction->argument;
    while (argument != NULL)
    {
        if (argument->modifier == HLSLArgumentModifier_Out && argument->semantic)
            OutputSetOutAttribute(argument->semantic, GetSafeIdentifierName(argument->name));

        argument = argument->nextArgument;
    }

    if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
    {
		HLSLStruct* structDeclaration = FindStruct(root, entryFunction->returnType.typeName);
		ASSERT(structDeclaration != NULL);
		HLSLStructField* field = structDeclaration->field;
		while (field != NULL)
		{
			char fieldResultName[1024];

			if (!field->name)
			{
				field = field->nextField;
				continue;
			}


			String_Printf(fieldResultName, sizeof(fieldResultName), "%s.%s", resultName, field->name);
			OutputSetOutAttribute(field->semantic, fieldResultName);

			if (m_target == Target_HullShader)
			{
				if (field->semantic)
				{
					if(String_Equal(field->semantic, "POSITION"))
					{
						m_writer.WriteLine(1, "gl_out[gl_InvocationID].gl_Position = %s%s;", m_outAttribPrefix, field->semantic);
					}
				}

				
			}


			field = field->nextField;
		}
    }
    else if (entryFunction->semantic != NULL)
    {
        OutputSetOutAttribute(entryFunction->semantic, resultName);
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

		OutputDeclarationBody( declaration->type, GetSafeIdentifierName( declaration->name ) );

		if( declaration->assignment != NULL )
		{
			m_writer.Write( " = " );
			if( declaration->type.array )
			{
				//m_writer.Write( "%s[]( ", GetTypeName( declaration->type ) );
				m_writer.Write("{");
				OutputExpressionList( declaration->assignment );
				m_writer.Write("}");
				//m_writer.Write( " )" );
			}
			else if (declaration->assignment->nextExpression)
			{
				// matrix's element initialization syntax.
				m_writer.Write("{");
				OutputExpressionList(declaration->assignment);
				m_writer.Write("}");
			}
			else
			{
				OutputExpression( declaration->assignment, &declaration->type );
			}
		}

		lastDecl = declaration;
		declaration = declaration->nextDeclaration;
	}
}

void GLSLGenerator::OutputDeclaration(const HLSLType& type, const char* name)
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
	
	m_writer.Write( "%s ", GetTypeName( type ) );
}

void GLSLGenerator::OutputDeclarationBody( const HLSLType& type, const char* name )
{
	if( !type.array )
	{
		m_writer.Write( "%s", GetSafeIdentifierName( name ) );
	}
	else
	{
		
		m_writer.Write("%s[", GetSafeIdentifierName(name));

		if (type.arraySize != NULL)		
			OutputExpression(type.arraySize);
		
		m_writer.Write("]");
	
	}
}

void GLSLGenerator::OutputCast(const HLSLType& type)
{
    if (m_version == Version_110 && type.baseType == HLSLBaseType_Float3x3)
        m_writer.Write("%s", m_matrixCtorFunction);
    else
        OutputDeclaration(type, "");
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

const char* GLSLGenerator::GetSafeIdentifierName(const char* name) const
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

bool GLSLGenerator::ChooseUniqueName(const char* base, char* dst, int dstLength) const
{
    for (int i = 0; i < 1024; ++i)
    {
        String_Printf(dst, dstLength, "%s%d", base, i);
        if (!m_tree->GetContainsString(dst))
        {
            return true;
        }
    }
    return false;
}

const char* GLSLGenerator::GetBuiltInSemantic(const char* semantic, AttributeModifier modifier, int* outputIndex)
{
    if (outputIndex)
        *outputIndex = -1;
	
    if ((m_target == Target_VertexShader || m_target == Target_DomainShader) && modifier == AttributeModifier_Out && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
        return "gl_Position";

	if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "POSITION") && !m_outputPosition)
		return "gl_Position";

    if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "PSIZE"))
        return "gl_PointSize";

    if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_InstanceID"))
        return "gl_InstanceIndex";

	if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_VertexID"))
		return "gl_VertexIndex";

    if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out && String_Equal(semantic, "SV_Depth"))
        return "gl_FragDepth";

	//https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return "vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w)";

	if (m_target == Target_GeometryShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return "gl_in";

	if (m_target == Target_GeometryShader && modifier == AttributeModifier_Out && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
			return "gl_Position";
	
	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_DispatchThreadID")))
		return "gl_GlobalInvocationID";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return "gl_WorkGroupID";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return "gl_WorkGroupID";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupIndex")))
		return "gl_LocalInvocationIndex";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupThreadID")))
		return "gl_LocalInvocationID";

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_SampleIndex")))
		return "gl_SampleID";

	if ((m_target == Target_HullShader || m_target == Target_DomainShader) && (String_Equal(semantic, "SV_InsideTessFactor")))
		return "gl_TessLevelInner";

	if ((m_target == Target_HullShader || m_target == Target_DomainShader)&& (String_Equal(semantic, "SV_TessFactor")))
		return "gl_TessLevelOuter";

	if (m_target == Target_HullShader && (String_Equal(semantic, "SV_OutputControlPointID")))
		return "gl_InvocationID";

	if (m_target == Target_DomainShader && (String_Equal(semantic, "SV_DomainLocation")))
		return "gl_TessCoord";
	

    if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out)
    {
        int index = -1;

        if (strncmp(semantic, "COLOR", 5) == 0)
            index = atoi(semantic + 5);
        else if (strncmp(semantic, "SV_Target", 9) == 0 || strncmp(semantic, "SV_TARGET", 9) == 0)
            index = atoi(semantic + 9);

        if (index >= 0)
        {
			if (m_outputTargets <= index)
				m_outputTargets = index + 1;

            if (outputIndex)
                *outputIndex = index;

            return m_versionLegacy ? "gl_FragData" : "rast_FragData";
        }
    }

	

    return NULL;
}


const char* GLSLGenerator::GetBuiltInSemantic(const char* semantic, AttributeModifier modifier, const HLSLType& type, int* outputIndex)
{
	if (outputIndex)
		*outputIndex = -1;

	if ((m_target == Target_VertexShader || m_target == Target_DomainShader) && modifier == AttributeModifier_Out && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return "gl_Position";

	//if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "POSITION") && !m_outputPosition)
	//	return "gl_Position";

	if (m_target == Target_GeometryShader && (modifier == AttributeModifier_In || modifier == AttributeModifier_Out) && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return "gl_Position";


	if (m_target == Target_VertexShader && modifier == AttributeModifier_Out && String_Equal(semantic, "PSIZE"))
		return "gl_PointSize";

	if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_InstanceID"))
		return "gl_InstanceIndex";

	if (m_target == Target_VertexShader && modifier == AttributeModifier_In && String_Equal(semantic, "SV_VertexID"))
		return "gl_VertexIndex";

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out && String_Equal(semantic, "SV_Depth"))
		return "gl_FragDepth";

	//https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/gl_FragCoord.xhtml
	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
		return "vec4(gl_FragCoord.xyz, 1.0 / gl_FragCoord.w)";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_DispatchThreadID")))
		return "gl_GlobalInvocationID";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return "gl_WorkGroupID";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupID")))
		return "gl_WorkGroupID";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupIndex")))
		return "gl_LocalInvocationIndex";

	if (m_target == Target_ComputeShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_GroupThreadID")))
		return "gl_LocalInvocationID";

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_SampleIndex")))
		return "gl_SampleID";

	if ((m_target == Target_HullShader || m_target == Target_DomainShader) && (String_Equal(semantic, "SV_InsideTessFactor")))
		return "gl_TessLevelInner";

	if ((m_target == Target_HullShader || m_target == Target_DomainShader) && (String_Equal(semantic, "SV_TessFactor")))
		return "gl_TessLevelOuter";

	if (m_target == Target_HullShader && modifier == AttributeModifier_In && (String_Equal(semantic, "SV_OutputControlPointID")))
		return "gl_InvocationID";

	if (m_target == Target_DomainShader && (String_Equal(semantic, "SV_DomainLocation")))
		return "gl_TessCoord";

	if (m_target == Target_FragmentShader && modifier == AttributeModifier_Out)
	{
		int index = -1;

		if (strncmp(semantic, "COLOR", 5) == 0)
			index = atoi(semantic + 5);
		else if (strncmp(semantic, "SV_Target", 9) == 0 || strncmp(semantic, "SV_TARGET", 9) == 0)
			index = atoi(semantic + 9);

		if (index >= 0)
		{
			if (m_outputTargets <= index)
			{
				m_outputTypes[m_outputTargets] = type.baseType;
				m_outputTargets = index + 1;				
			}

			if (outputIndex)
				*outputIndex = index;

			return m_versionLegacy ? "gl_FragData" : "rast_FragData";
		}
	}



	return NULL;
}

void GLSLGenerator::OutPushConstantIdentifierTextureStateExpression(int size, int counter, const HLSLTextureStateExpression* pTextureStateExpression, bool* bWritten)
{
	for (int index = 0; index < size; index++)
	{
		HLSLBuffer* buffer = static_cast<HLSLBuffer*>(m_PushConstantBuffers[index]);
		HLSLDeclaration* field = buffer->field;

		while (field != NULL)
		{
			if (!field->hidden)
			{
				if (String_Equal(field->name, pTextureStateExpression->arrayIdentifier[counter]))
				{
					*bWritten = true;
					m_writer.Write("%s.%s", buffer->name, pTextureStateExpression->arrayIdentifier[counter]);
					break;
				}
			}
			field = (HLSLDeclaration*)field->nextStatement;
		}

		if (*bWritten)
			break;
	}
}

/*
void GLSLGenerator::OutPushConstantIdentifierRWTextureStateExpression(int size, int counter, const HLSLRWTextureStateExpression* pRWTextureStateExpression, bool* bWritten)
{
	for (int index = 0; index < m_PushConstantBufferCounter; index++)
	{
		HLSLConstantBuffer* buffer = static_cast<HLSLConstantBuffer*>(m_PushConstantBuffers[index]);
		HLSLDeclaration* field = buffer->field;

		while (field != NULL)
		{
			if (!field->hidden)
			{
				if (String_Equal(field->name, pRWTextureStateExpression->arrayIdentifier[index]))
				{
					*bWritten = true;
					m_writer.Write("%s.%s", buffer->name, pRWTextureStateExpression->arrayIdentifier[index]);
					break;
				}
			}
			field = (HLSLDeclaration*)field->nextStatement;
		}

		if (*bWritten)
			break;
	}
}
*/


