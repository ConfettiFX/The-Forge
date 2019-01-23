//=============================================================================
//
// Render/MSLGenerator.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

//#include "Engine/String.h"
//#include "Engine/Log.h"
#include "Engine.h"

#include "MSLGenerator.h"
#include "HLSLParser.h"
#include "HLSLTree.h"

#include <string.h>

static const HLSLType kFloatType(HLSLBaseType_Float);
static const HLSLType kUintType(HLSLBaseType_Uint);
static const HLSLType kIntType(HLSLBaseType_Int);
static const HLSLType kBoolType(HLSLBaseType_Bool);

#define DEFAULT_TEXTURE_COUNT 256

// MSL limitations:
// - Passing swizzled expressions as out or inout arguments. Out arguments are passed by reference in C++, but
//   swizzled expressions are not addressable.
// - Some type conversions and constructors don't work exactly the same way. For example, casts to smaller size vectors are not alloweed in C++. @@ Add more details...
// - Swizzles on scalar types, whether or not it expands them. a_float.x, a_float.xxxx both cause compile errors.
// - Using ints as floats without the trailing .0 makes the compiler sad.
// Unsupported by this generator:
// - Matrix [] access is implemented as a function call, so result cannot be passed as out/inout argument.
// - Matrix [] access is not supported in all l-value expressions. Only simple assignments.
// - No support for boolean vectors and logical operators involving vectors. This is not just in metal.
// - No support for non-float texture types


static const char* GetTypeName(const HLSLType& type)
{
	HLSLBaseType baseType = type.baseType;

	if (type.baseType == HLSLBaseType_Unknown)
		baseType = type.elementType;

	switch (baseType)
	{
	case HLSLBaseType_Void:         return "void";
	case HLSLBaseType_Float:        return "float";
	case HLSLBaseType_Float1x2:        return "float1x2";
	case HLSLBaseType_Float1x3:        return "float1x3";
	case HLSLBaseType_Float1x4:        return "float1x4";
	case HLSLBaseType_Float2:       return "float2";
	case HLSLBaseType_Float2x2:        return "float2x2";
	case HLSLBaseType_Float2x3:        return "float2x3";
	case HLSLBaseType_Float2x4:        return "float2x4";
	case HLSLBaseType_Float3:       return "float3";
	case HLSLBaseType_Float3x2:        return "float3x2";
	case HLSLBaseType_Float3x3:        return "float3x3";
	case HLSLBaseType_Float3x4:        return "float3x4";
	case HLSLBaseType_Float4:       return "float4";
	case HLSLBaseType_Float4x2:        return "float4x2";
	case HLSLBaseType_Float4x3:        return "float4x3";
	case HLSLBaseType_Float4x4:        return "float4x4";

	case HLSLBaseType_Half:         return "float";
	case HLSLBaseType_Half1x2:        return "float1x2";
	case HLSLBaseType_Half1x3:        return "float1x3";
	case HLSLBaseType_Half1x4:        return "float1x4";
	case HLSLBaseType_Half2:        return "float2";
	case HLSLBaseType_Half2x2:        return "float2x2";
	case HLSLBaseType_Half2x3:        return "float2x3";
	case HLSLBaseType_Half2x4:        return "float2x4";
	case HLSLBaseType_Half3:        return "float3";
	case HLSLBaseType_Half3x2:        return "float3x2";
	case HLSLBaseType_Half3x3:        return "float3x3";
	case HLSLBaseType_Half3x4:        return "float3x4";
	case HLSLBaseType_Half4:        return "float4";
	case HLSLBaseType_Half4x2:        return "float4x2";
	case HLSLBaseType_Half4x3:        return "float4x3";
	case HLSLBaseType_Half4x4:        return "float4x4";



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
	case HLSLBaseType_Bool1x2:        return "bool1x2";
	case HLSLBaseType_Bool1x3:        return "bool1x3";
	case HLSLBaseType_Bool1x4:        return "bool1x4";
	case HLSLBaseType_Bool2:        return "bool2";
	case HLSLBaseType_Bool2x2:        return "bool2x2";
	case HLSLBaseType_Bool2x3:        return "bool2x3";
	case HLSLBaseType_Bool2x4:        return "bool2x4";
	case HLSLBaseType_Bool3:        return "bool3";
	case HLSLBaseType_Bool3x2:        return "bool3x2";
	case HLSLBaseType_Bool3x3:        return "bool3x3";
	case HLSLBaseType_Bool3x4:        return "bool3x4";
	case HLSLBaseType_Bool4:        return "bool4";
	case HLSLBaseType_Bool4x2:        return "bool4x2";
	case HLSLBaseType_Bool4x3:        return "bool4x3";
	case HLSLBaseType_Bool4x4:        return "bool4x4";

	case HLSLBaseType_Int:          return "int";
	case HLSLBaseType_Int1x2:        return "int1x2";
	case HLSLBaseType_Int1x3:        return "int1x3";
	case HLSLBaseType_Int1x4:        return "int1x4";
	case HLSLBaseType_Int2:        return "int2";
	case HLSLBaseType_Int2x2:        return "int2x2";
	case HLSLBaseType_Int2x3:        return "int2x3";
	case HLSLBaseType_Int2x4:        return "int2x4";
	case HLSLBaseType_Int3:        return "int3";
	case HLSLBaseType_Int3x2:        return "int3x2";
	case HLSLBaseType_Int3x3:        return "int3x3";
	case HLSLBaseType_Int3x4:        return "int3x4";
	case HLSLBaseType_Int4:        return "int4";
	case HLSLBaseType_Int4x2:        return "int4x2";
	case HLSLBaseType_Int4x3:        return "int4x3";
	case HLSLBaseType_Int4x4:        return "int4x4";

	case HLSLBaseType_Uint:          return "uint";
	case HLSLBaseType_Uint1x2:        return "uint1x2";
	case HLSLBaseType_Uint1x3:        return "uint1x3";
	case HLSLBaseType_Uint1x4:        return "uint1x4";
	case HLSLBaseType_Uint2:        return "uint2";
	case HLSLBaseType_Uint2x2:        return "uint2x2";
	case HLSLBaseType_Uint2x3:        return "uint2x3";
	case HLSLBaseType_Uint2x4:        return "uint2x4";
	case HLSLBaseType_Uint3:        return "uint3";
	case HLSLBaseType_Uint3x2:        return "uint3x2";
	case HLSLBaseType_Uint3x3:        return "uint3x3";
	case HLSLBaseType_Uint3x4:        return "uint3x4";
	case HLSLBaseType_Uint4:        return "uint4";
	case HLSLBaseType_Uint4x2:        return "uint4x2";
	case HLSLBaseType_Uint4x3:        return "uint4x3";
	case HLSLBaseType_Uint4x4:        return "uint4x4";

	case HLSLBaseType_InputPatch:

		return type.typeName;

	case HLSLBaseType_OutputPatch:

		return type.structuredTypeName;

	case HLSLBaseType_PatchControlPoint:

		return type.typeName;

	case HLSLBaseType_Texture:          return "texture";


	case HLSLBaseType_Texture1D:      return "texture1d<float>";
	case HLSLBaseType_Texture1DArray:      return "texture1d_array<float>";
	case HLSLBaseType_Texture2D:      return "texture2d<float>";
	case HLSLBaseType_Texture2DArray:      return "texture2d_array<float>";
	case HLSLBaseType_Texture3D:      return "texture3d<float>";
	case HLSLBaseType_Texture2DMS:      return "texture2d_ms<float>";
	case HLSLBaseType_Texture2DMSArray:      return "texture2d_ms_array<float>";
	case HLSLBaseType_TextureCube:      return "texturecube<float>";
	case HLSLBaseType_TextureCubeArray:      return "texturecube_array<float>";

	case HLSLBaseType_RasterizerOrderedTexture1D:      return "texture1d<float, access::read_write>";
	case HLSLBaseType_RasterizerOrderedTexture1DArray:      return "texture1d_array<float, access::read_write>";
	case HLSLBaseType_RasterizerOrderedTexture2D:      return "texture2d<float, access::read_write>";
	case HLSLBaseType_RasterizerOrderedTexture2DArray:      return "texture2d_array<float, access::read_write>";
	case HLSLBaseType_RasterizerOrderedTexture3D:      return "texture3d<float, access::read_write>";

	case HLSLBaseType_RWTexture1D:      return "texture1d<float, access::read_write>";
	case HLSLBaseType_RWTexture1DArray:      return "texture1d_array<float, access::read_write>";
	case HLSLBaseType_RWTexture2D:      return "texture2d<float, access::read_write>";
	case HLSLBaseType_RWTexture2DArray:      return "texture2d_array<float, access::read_write>";
	case HLSLBaseType_RWTexture3D:      return "texture3d<float, access::read_write>";

	case HLSLBaseType_DepthTexture2D:      return "depth2d<float>";
	case HLSLBaseType_DepthTexture2DArray:      return "depth2d_array<float>";
	case HLSLBaseType_DepthTexture2DMS:      return "depth2d_ms<float>";
	case HLSLBaseType_DepthTexture2DMSArray:      return "depth2d_ms_array<float>";
	case HLSLBaseType_DepthTextureCube:      return "depthcube<float>";
	case HLSLBaseType_DepthTextureCubeArray:      return "depthcube_array<float>";


	case HLSLBaseType_Sampler:          return "sampler";
		// ACoget-TODO: How to detect non-float textures, if relevant?
	case HLSLBaseType_Sampler2D:        return "texture2d<float>";
	case HLSLBaseType_Sampler3D:        return "texture3d<float>";
	case HLSLBaseType_SamplerCube:      return "texturecube<float>";
	case HLSLBaseType_Sampler2DShadow:  return "depth2d<float>";
	case HLSLBaseType_Sampler2DMS:      return "texture2d_ms<float>";
	case HLSLBaseType_Sampler2DArray:   return "texture2d_array<float>";
	case HLSLBaseType_SamplerState:		return "sampler";
	case HLSLBaseType_UserDefined:      return type.typeName;
	default:
		ASSERT(-1);
		return "<unknown type>";
	}
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

static void ParseSemantic(const char* semantic, unsigned int* outputLength, unsigned int* outputIndex)
{
	const char* semanticIndex = semantic;

	while (*semanticIndex && !isdigit(*semanticIndex))
	{
		semanticIndex++;
	}

	*outputLength = (unsigned int)(semanticIndex - semantic);
	*outputIndex = atoi(semanticIndex);
}

// Parse register name and advance next register index.
static int ParseRegister(const char* registerName, int& nextRegister)
{
	if (!registerName)
	{
		return nextRegister++;
	}

	while (*registerName && !isdigit(*registerName))
	{
		registerName++;
	}

	if (!*registerName)
	{
		return nextRegister++;
	}

	int result = atoi(registerName);

	if (nextRegister <= result)
	{
		nextRegister = result + 1;
	}

	return result;
}


MSLGenerator::MSLGenerator()
{
	m_tree = NULL;
	m_entryName = NULL;
	m_target = Target_VertexShader;
	m_error = false;

	m_firstClassArgument = NULL;
	m_lastClassArgument = NULL;

	attributeCounter = 0;
	m_StructBuffersCounter = 0;
	m_PushConstantBufferCounter = 0;
	
}

// Copied from GLSLGenerator
void MSLGenerator::Error(const char* format, ...)
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

inline void MSLGenerator::AddClassArgument(ClassArgument* arg)
{
	if (m_firstClassArgument == NULL)
	{
		m_firstClassArgument = arg;
	}
	else
	{
		m_lastClassArgument->nextArg = arg;
	}
	m_lastClassArgument = arg;
}

void MSLGenerator::Prepass(HLSLTree* tree, Target target, HLSLFunction* entryFunction, HLSLFunction* secondaryEntryFunction)
{
	// Hide unused arguments. @@ It would be good to do this in the other generators too.
	//HideUnusedArguments(entryFunction);

	HLSLRoot* root = tree->GetRoot();
	HLSLStatement* statement = root->statement;
	//ASSERT(m_firstClassArgument == NULL);

	HLSLType samplerType(HLSLBaseType_Sampler);

	int nextTextureRegister = 0;
	int nextSamplerRegister = 0;
	int nextBufferRegister = 1;

	int mainFuncCounter = 0;

	while (statement != NULL)
	{
		if (statement->nodeType == HLSLNodeType_Preprocessor)
		{
			HLSLpreprocessor* preprocessor = static_cast<HLSLpreprocessor*>(statement);

			//if (preprocessor->hidden == false)
			{
				HLSLType tempType;
				tempType.baseType = preprocessor->type;

				switch (preprocessor->type)
				{
				case HLSLBaseType_PreProcessorIf:					
					AddClassArgument(new ClassArgument(preprocessor->contents, preprocessor->contents, tempType));
					break;
				case HLSLBaseType_PreProcessorIfDef:
					AddClassArgument(new ClassArgument(preprocessor->contents, preprocessor->contents, tempType));
					break;
				case HLSLBaseType_PreProcessorIfnDef:
					AddClassArgument(new ClassArgument(preprocessor->contents, preprocessor->contents, tempType));
					break;
				case HLSLBaseType_PreProcessorElif:
					AddClassArgument(new ClassArgument(preprocessor->contents, preprocessor->contents, tempType));
					break;
				case HLSLBaseType_PreProcessorElse:
					{	
						const char* temp = m_tree->AddString("#else");
						AddClassArgument(new ClassArgument(temp, temp, tempType));
					}
					break;
				case HLSLBaseType_PreProcessorEndif:
					{	
						const char* temp = m_tree->AddString("#endif");
						AddClassArgument(new ClassArgument(temp, temp, tempType));
					}
					break;
				default:
					break;
				}
			}			
		}
		else if (statement->nodeType == HLSLNodeType_Declaration)
		{
			HLSLDeclaration* declaration = (HLSLDeclaration*)statement;

			if (!declaration->hidden && IsSamplerType(declaration->type))
			{
				//int textureRegister = ParseRegister(declaration->registerName, nextTextureRegister) + m_options.textureRegisterOffset;
				int textureRegister = nextTextureRegister++ + m_options.textureRegisterOffset;

				const char * textureName = m_tree->AddStringFormat("%s_texture", declaration->name);
				const char * textureRegisterName = m_tree->AddStringFormat("texture(%d)", textureRegister);
				AddClassArgument(new ClassArgument(textureName, declaration->type, textureRegisterName));

				if (declaration->type.baseType != HLSLBaseType_Sampler2DMS)
				{
					int samplerRegister = nextSamplerRegister++;

					const char * samplerName = m_tree->AddStringFormat("%s_sampler", declaration->name);
					const char * samplerRegisterName = m_tree->AddStringFormat("sampler(%d)", samplerRegister);
					AddClassArgument(new ClassArgument(samplerName, samplerType, samplerRegisterName));
				}
			}
		}
		else if (statement->nodeType == HLSLNodeType_Buffer)
		{
			HLSLBuffer * buffer = (HLSLBuffer *)statement;

			if (buffer->type.baseType == HLSLBaseType_CBuffer ||
				buffer->type.baseType == HLSLBaseType_TBuffer)
			{
				//if (!buffer->hidden)
				{
					HLSLType type(HLSLBaseType_UserDefined);
					type.addressSpace = HLSLAddressSpace_Constant;


					type.typeName = m_tree->AddStringFormat("Uniforms_%s", buffer->name);

					//int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;
					int bufferRegister = nextBufferRegister++ + m_options.bufferRegisterOffset;

					const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);

					AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName));
				}
			}
			if (buffer->type.baseType == HLSLBaseType_ConstantBuffer)
			{
				//if (!buffer->hidden)
				{
					HLSLType type(HLSLBaseType_UserDefined);
					type.addressSpace = HLSLAddressSpace_Constant;


					if (buffer->bPushConstant)
					{
						m_PushConstantBuffers[m_PushConstantBufferCounter++] = buffer;
					}

					type.typeName = m_tree->AddStringFormat("Uniforms_%s", buffer->name);

					//int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;
					int bufferRegister = nextBufferRegister++ + m_options.bufferRegisterOffset;

					const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);

					AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName));
				}
			}
			else if (buffer->type.baseType == HLSLBaseType_ByteAddressBuffer || buffer->type.baseType == HLSLBaseType_RWByteAddressBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedByteAddressBuffer)
			{
				//if (!buffer->hidden)
				{
					HLSLType type(HLSLBaseType_Uint);
					type.addressSpace = HLSLAddressSpace_Constant;
					type.typeName = m_tree->AddStringFormat("uint");
					type.structuredTypeName = type.typeName;

					//int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;
					int bufferRegister = nextBufferRegister++ + m_options.bufferRegisterOffset;

					

					if (buffer->type.baseType == HLSLBaseType_RasterizerOrderedByteAddressBuffer)
					{
						const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d), raster_order_group(%d)", bufferRegister, 0);
						AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName, true));
					}
					else
					{
						const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);
						AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName, true));
					}
				}
			}
			else if (buffer->type.baseType == HLSLBaseType_StructuredBuffer || buffer->type.baseType == HLSLBaseType_PureBuffer)
			{
				//if (!buffer->hidden)
				{
					HLSLType type(HLSLBaseType_UserDefined);
					type.addressSpace = HLSLAddressSpace_Constant;


					if (buffer->type.elementType)
					{
						type.structuredTypeName = type.typeName = m_tree->AddStringFormat("%s", getElementTypeAsStr(buffer->type));
					}
					else
					{
						switch (buffer->type.elementType)
						{
						case HLSLBaseType_Float:
							type.typeName = m_tree->AddStringFormat("float");
							break;
						case HLSLBaseType_Float2:
							type.typeName = m_tree->AddStringFormat("float2");
							break;
						case HLSLBaseType_Float3:
							type.typeName = m_tree->AddStringFormat("float3");
							break;
						case HLSLBaseType_Float4:
							type.typeName = m_tree->AddStringFormat("float4");
							break;
						case HLSLBaseType_Bool:
							type.typeName = m_tree->AddStringFormat("bool");
							break;
						case HLSLBaseType_Bool2:
							type.typeName = m_tree->AddStringFormat("bool2");
							break;
						case HLSLBaseType_Bool3:
							type.typeName = m_tree->AddStringFormat("bool3");
							break;
						case HLSLBaseType_Bool4:
							type.typeName = m_tree->AddStringFormat("bool4");
							break;
						case HLSLBaseType_Int:
							type.typeName = m_tree->AddStringFormat("int");
							break;
						case HLSLBaseType_Int2:
							type.typeName = m_tree->AddStringFormat("int2");
							break;
						case HLSLBaseType_Int3:
							type.typeName = m_tree->AddStringFormat("int3");
							break;
						case HLSLBaseType_Int4:
							type.typeName = m_tree->AddStringFormat("int4");
							break;
						case HLSLBaseType_Uint:
							type.typeName = m_tree->AddStringFormat("uint");
							break;
						case HLSLBaseType_Uint2:
							type.typeName = m_tree->AddStringFormat("uint2");
							break;
						case HLSLBaseType_Uint3:
							type.typeName = m_tree->AddStringFormat("uint3");
							break;
						case HLSLBaseType_Uint4:
							type.typeName = m_tree->AddStringFormat("uint4");
							break;
						default:
							break;
						}

						type.structuredTypeName = type.typeName;
					}

					//int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;
					int bufferRegister = nextBufferRegister++ + m_options.bufferRegisterOffset;

					const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);

					AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName, true));
				}
			}
			else if (buffer->type.baseType == HLSLBaseType_RWBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedBuffer)
			{
				//if (!buffer->hidden)
				{
					HLSLType type(buffer->type.elementType);
					type.addressSpace = HLSLAddressSpace_Device;


					const char* atomic = "";

					if (buffer->bAtomic)
						atomic = "atomic_";

					type.typeName = type.typeName = m_tree->AddStringFormat("%s%s", atomic, GetTypeName(type));

					//int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;
					int bufferRegister = nextBufferRegister++ + m_options.bufferRegisterOffset;

					

					if (buffer->type.baseType == HLSLBaseType_RWBuffer)
					{
						const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);
						AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName));
					}
					else if (buffer->type.baseType == HLSLBaseType_RasterizerOrderedBuffer)
					{
						const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d), raster_order_group(%d)", bufferRegister, 0);
						AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName));
					}
				}

				m_RWBuffers[m_RWBufferCounter++] = buffer;
			}
			else if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedStructuredBuffer)
			{
				//if (!buffer->hidden)
				{
					HLSLType type;

					if (buffer->type.elementType)
					{
						type.baseType = HLSLBaseType_UserDefined;
						type.addressSpace = HLSLAddressSpace_Device;
						type.typeName = m_tree->AddStringFormat("%s", getElementTypeAsStr(buffer->type));
					}
					else
					{
						type.addressSpace = HLSLAddressSpace_Device;
						type.baseType = buffer->type.elementType;

						const char* atomic = "";

						if (buffer->bAtomic)
							atomic = "atomic_";

						switch (buffer->type.elementType)
						{
						case HLSLBaseType_Float:
							type.typeName = m_tree->AddStringFormat("%sfloat", atomic);
							break;
						case HLSLBaseType_Float2:
							type.typeName = m_tree->AddStringFormat("%sfloat2", atomic);
							break;
						case HLSLBaseType_Float3:
							type.typeName = m_tree->AddStringFormat("%sfloat3", atomic);
							break;
						case HLSLBaseType_Float4:
							type.typeName = m_tree->AddStringFormat("%sfloat4", atomic);
							break;
						case HLSLBaseType_Bool:
							type.typeName = m_tree->AddStringFormat("%sbool", atomic);
							break;
						case HLSLBaseType_Bool2:
							type.typeName = m_tree->AddStringFormat("%sbool2", atomic);
							break;
						case HLSLBaseType_Bool3:
							type.typeName = m_tree->AddStringFormat("%sbool3", atomic);
							break;
						case HLSLBaseType_Bool4:
							type.typeName = m_tree->AddStringFormat("%sbool4", atomic);
							break;
						case HLSLBaseType_Int:
							type.typeName = m_tree->AddStringFormat("%sint", atomic);
							break;
						case HLSLBaseType_Int2:
							type.typeName = m_tree->AddStringFormat("%sint2", atomic);
							break;
						case HLSLBaseType_Int3:
							type.typeName = m_tree->AddStringFormat("%sint3", atomic);
							break;
						case HLSLBaseType_Int4:
							type.typeName = m_tree->AddStringFormat("%sint4", atomic);
							break;
						case HLSLBaseType_Uint:
							type.typeName = m_tree->AddStringFormat("%suint", atomic);
							break;
						case HLSLBaseType_Uint2:
							type.typeName = m_tree->AddStringFormat("%suint2", atomic);
							break;
						case HLSLBaseType_Uint3:
							type.typeName = m_tree->AddStringFormat("%suint3", atomic);
							break;
						case HLSLBaseType_Uint4:
							type.typeName = m_tree->AddStringFormat("%suint4", atomic);
							break;
						default:
							break;
						}
					}

					//int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;
					int bufferRegister = nextBufferRegister++ + m_options.bufferRegisterOffset;

				
					
					if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer)
					{
						const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);
						AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName, true));
					}
					else if (buffer->type.baseType == HLSLBaseType_RasterizerOrderedStructuredBuffer)
					{
						const char * bufferRegisterName = m_tree->AddStringFormat("buffer(%d), raster_order_group(%d)", bufferRegister, 0);
						AddClassArgument(new ClassArgument(buffer->name, type, bufferRegisterName, true));
					}


					

					
				}

				m_RWStructuredBuffers[m_RWStructuredBufferCounter++] = buffer;
			}
		}		
		else if (statement->nodeType == HLSLNodeType_TextureState)
		{
			HLSLTextureState* textureState = (HLSLTextureState *)statement;		

			if(textureState->type.baseType >= HLSLBaseType_Texture1D &&
				textureState->type.baseType <= HLSLBaseType_TextureCubeArray)
			{
				HLSLType type(HLSLBaseType_TextureState);
				type.addressSpace = HLSLAddressSpace_Undefined;
				type.baseType = textureState->type.baseType;
				type.array = textureState->bArray;

				if (type.array)
				{
					type.arrayCount = textureState->arrayIndex[0] == 0 ? DEFAULT_TEXTURE_COUNT : textureState->arrayIndex[0];
				}

				//int textureRegister = ParseRegister(textureState->registerName, nextTextureRegister) + m_options.textureRegisterOffset;;
				
				const char * textureName;


				const char * textureRegisterName;

				if (type.array)
				{
					type.baseType = HLSLBaseType_UserDefined;
					type.addressSpace = HLSLAddressSpace_Constant;
					type.typeName = m_tree->AddStringFormat("Uniforms_%s", textureState->name);

					HLSLBuffer * buffer = m_tree->AddNode<HLSLBuffer>(NULL, 0);
					buffer->field = m_tree->AddNode<HLSLDeclaration>(NULL, 0);

					HLSLDeclaration * declaration = buffer->field;

					HLSLType fieldType(HLSLBaseType_TextureState);
					fieldType.addressSpace = HLSLAddressSpace_Undefined;
					fieldType.baseType = textureState->type.baseType;
					fieldType.array = type.array;
					fieldType.arrayCount = type.arrayCount;
					declaration->name = textureState->name;
					declaration->type = fieldType;

					buffer->field->name = m_tree->AddStringFormat("Textures");
					buffer->fileName = "????";
					buffer->name = textureState->name;


					HLSLStatement* prevStatement = statement;
					prevStatement->hidden = true;

					HLSLStatement* prevsNextStatement = prevStatement->nextStatement;
					prevStatement->nextStatement = buffer;

					buffer->nextStatement = prevsNextStatement;
					statement = buffer;

					int bufferRegister = nextBufferRegister++ + m_options.bufferRegisterOffset;

					textureName = m_tree->AddStringFormat("%s", textureState->name);
					textureRegisterName = m_tree->AddStringFormat("buffer(%d)", bufferRegister);
					AddClassArgument(new ClassArgument(textureName, type, textureRegisterName));
				}
				else
				{
					int textureRegister = nextTextureRegister++ + m_options.textureRegisterOffset;

					textureName = m_tree->AddStringFormat("%s", textureState->name);
					textureRegisterName = m_tree->AddStringFormat("texture(%d)", textureRegister);
					AddClassArgument(new ClassArgument(textureName, type, textureRegisterName));
				}
			}
			else if (textureState->type.baseType >= HLSLBaseType_RasterizerOrderedTexture1D &&
				textureState->type.baseType <= HLSLBaseType_RasterizerOrderedTexture3D)
			{

				HLSLType type(HLSLBaseType_RWTextureState);
				type.addressSpace = HLSLAddressSpace_Undefined;
				type.baseType = textureState->type.baseType;
				type.array = textureState->bArray;
				type.arrayCount = textureState->arrayIndex[0];

				
				int textureRegister = nextTextureRegister++ + m_options.textureRegisterOffset;
				const char * textureName = m_tree->AddStringFormat("%s", textureState->name);

				const char * textureRegisterName = m_tree->AddStringFormat("texture(%d), raster_order_group(%d)", textureRegister, 0);
				AddClassArgument(new ClassArgument(textureName, type, textureRegisterName));
			}

			else if (textureState->type.baseType >= HLSLBaseType_RWTexture1D &&
					 textureState->type.baseType <= HLSLBaseType_RWTexture3D)
			{				

				HLSLType type(HLSLBaseType_RWTextureState);
				type.addressSpace = HLSLAddressSpace_Undefined;
				type.baseType = textureState->type.baseType;
				type.array = textureState->bArray;
				type.arrayCount = textureState->arrayIndex[0];

				
				int textureRegister = nextTextureRegister++ + m_options.textureRegisterOffset;
				const char * textureName = m_tree->AddStringFormat("%s", textureState->name);

				const char * textureRegisterName = m_tree->AddStringFormat("texture(%d)", textureRegister);
				AddClassArgument(new ClassArgument(textureName, type, textureRegisterName));
			}
		}
		//Does Metal support groupshared memory????
		/*
		else if (statement->nodeType == HLSLNodeType_GroupShared)
		{
			HLSLGroupShared* pGroupShared = static_cast<HLSLGroupShared*>(statement);

			OutputDeclaration(pGroupShared->declaration, NULL);
			m_writer.Write(";");

		}
		*/
		else if (statement->nodeType == HLSLNodeType_SamplerState)
		{
			HLSLSamplerState* samplerState = (HLSLSamplerState *)statement;

			HLSLType type(HLSLBaseType_SamplerState);
			type.addressSpace = HLSLAddressSpace_Undefined;

			//int samplerRegister = ParseRegister(samplerState->registerName, nextSamplerRegister);
			int samplerRegister = nextSamplerRegister++;

			const char * samplerName = m_tree->AddStringFormat("%s", samplerState->name);

			type.typeName = samplerName;
			type.baseType = HLSLBaseType_SamplerState;

			type.array = samplerState->type.array;
			type.arraySize = samplerState->type.arraySize;
			type.arrayCount = samplerState->type.arrayCount;

			const char * samplerRegisterName = m_tree->AddStringFormat("sampler(%d)", samplerRegister);
			AddClassArgument(new ClassArgument(samplerName, type, samplerRegisterName));

		}
		else if (statement->nodeType == HLSLNodeType_Function)
		{
			HLSLFunction* function = static_cast<HLSLFunction*>(statement);

			if ((String_Equal(function->name, m_entryName)) && m_target == Target_HullShader)
			{
				m_entryName = function->name = m_tree->AddStringFormat("VS%s", function->name);
			}
			else if (secondaryEntryFunction && m_target == Target_HullShader)
			{
				//assumes every entry function has same name
				if ((String_Equal(function->name, m_secondaryEntryName)))
				{
					m_secondaryEntryName = function->name = m_tree->AddStringFormat("HS%s", function->name);
				}
			}
		}

		statement = statement->nextStatement;
	}

	// @@ IC: instance_id parameter must be a function argument. If we find it inside a struct we must move it to the function arguments
	// and patch all the references to it!

	// Translate semantics.
	HLSLArgument* argument = entryFunction->argument;

	int attributeCounter = 0;

	HLSLArgument* prevArgument = NULL;

	while (argument != NULL)
	{
		if (argument->hidden)
		{
			argument = argument->nextArgument;
			continue;
		}

		if (argument->modifier == HLSLArgumentModifier_Out)
		{
			// Translate output arguments semantics.
			if (argument->type.baseType == HLSLBaseType_UserDefined)
			{
				// Our vertex input is a struct and its fields need to be tagged when we generate that
				HLSLStruct* structure = tree->FindGlobalStruct(argument->type.typeName);
				if (structure == NULL)
				{
					Error("Vertex shader output struct '%s' not found in shader\n", argument->type.typeName);
				}

				HLSLStructField* field = structure->field;
				while (field != NULL)
				{
					if (!field->hidden)
					{
						field->sv_semantic = TranslateOutputSemantic(field->semantic);
					}
					field = field->nextField;
				}
			}
			else
			{
				argument->sv_semantic = TranslateOutputSemantic(argument->semantic);
			}
		}
		else
		{
			// Translate input arguments semantics.
			if (argument->type.baseType == HLSLBaseType_UserDefined || argument->type.baseType == HLSLBaseType_OutputPatch)
			{
				if (argument->type.baseType == HLSLBaseType_OutputPatch)
				{

					argument->type.structuredTypeName = m_tree->AddStringFormat("Domain_%s", argument->type.structuredTypeName);

					//inform maxVertex to attribute & Patch Identifier
					HLSLStatement* statement = root->statement;

					while (statement)
					{
						HLSLAttribute* pAttribute = statement->attributes;
						while (pAttribute != NULL)
						{
							if (pAttribute->attributeType == HLSLAttributeType_Domain)
							{
								pAttribute->maxVertexCount = argument->type.maxPoints;
								strcpy(pAttribute->patchIdentifier, argument->name);
								break;
							}

							pAttribute = pAttribute->nextAttribute;
						}


						statement = statement->nextStatement;
					}



					//create new wrapping struct for domain shader's patch_control_point
					statement = root->statement;

					while (statement)
					{
						if (statement->nodeType == HLSLNodeType_Struct)
						{
							HLSLStruct* structure = (HLSLStruct*)statement;
							String_Equal(argument->type.typeName, structure->name);
							break;
						}

						statement = statement->nextStatement;
					}


					HLSLStruct* structure = m_tree->AddNode<HLSLStruct>("", 0);
					structure->name = argument->type.structuredTypeName;

					structure->field = m_tree->AddNode<HLSLStructField>("", 0);
					structure->field->nodeType = HLSLNodeType_PatchControlPoint;
					structure->field->type.baseType = HLSLBaseType_PatchControlPoint;
					structure->field->type.typeName = m_tree->AddStringFormat("patch_control_point<%s>", argument->type.typeName);

					structure->field->name = "control_points";


					HLSLStatement* prevNextStatement = statement->nextStatement;

					statement->nextStatement = structure;
					structure->nextStatement = prevNextStatement;
				}

				// Our vertex input is a struct and its fields need to be tagged when we generate that
				HLSLStruct* structure = tree->FindGlobalStruct(argument->type.typeName);
				if (structure == NULL)
				{
					Error("Vertex shader input struct '%s' not found in shader\n", argument->type.typeName);
				}

				HLSLStructField* field = structure->field;
				while (field != NULL)
				{
					// Hide vertex shader output position from fragment shader. @@ This is messing up the runtime compiler...
					/*if (target == Target_FragmentShader && is_semantic(field->semantic, "POSITION"))
					{
					field->hidden = true;
					}*/

					if (!field->hidden)
					{
						field->sv_semantic = TranslateInputSemantic(field->semantic);

						/*if (target == Target_VertexShader && is_semantic(field->semantic, "COLOR"))
						{
						field->type.flags |= HLSLTypeFlag_Swizzle_BGRA;
						}*/
					}
					field = field->nextField;
				}
			}
			else
			{
				const char* result = TranslateInputSemantic(argument->semantic);

				if (result == NULL)
				{
					//remove this argument for Main function printing
					if (prevArgument == NULL)
					{
						if (argument->nextArgument)
						{
							argument = argument->nextArgument;
							continue;
						}
						else
						{
							argument = NULL;
							break;
						}
					}
					else
					{
						if (argument->nextArgument == NULL)
							break;

						prevArgument->nextArgument = argument->nextArgument;
						continue;
					}
				}
				else
					argument->sv_semantic = result;
			}
		}

		prevArgument = argument;
		argument = argument->nextArgument;


		if (argument == NULL && prevArgument != NULL)
		{
			statement = root->statement;
			HLSLStatement* lastStatement = NULL;
			while (statement)
			{
				lastStatement = statement;
				statement = statement->nextStatement;
			}

			if (m_tree->NeedsExtension(USE_WaveGetLaneIndex))
			{
				/*
				lastStatement->nextStatement = m_tree->AddNode<HLSLDeclaration>("", 0);

				HLSLDeclaration* pDeclaration = (HLSLDeclaration*)lastStatement->nextStatement;
				pDeclaration->arrayDimension = 0;
				pDeclaration->assignment = NULL;
				pDeclaration->bArray = false;
				pDeclaration->nodeType = HLSLNodeType_Declaration;
				pDeclaration->type.baseType = HLSLBaseType_Uint;
				pDeclaration->name = m_tree->AddString("simd_lane_id");

				lastStatement = lastStatement->nextStatement;
				*/

				prevArgument->nextArgument = m_tree->AddNode<HLSLArgument>("", 0);
				prevArgument = prevArgument->nextArgument;
				prevArgument->name = m_tree->AddString("simd_lane_id");
				prevArgument->type.baseType = HLSLBaseType_Uint;
				prevArgument->semantic = prevArgument->sv_semantic = m_tree->AddString("thread_index_in_simdgroup");
			}

			if (m_tree->NeedsExtension(USE_WaveGetLaneCount))
			{
				/*
				lastStatement->nextStatement = m_tree->AddNode<HLSLDeclaration>("", 0);

				HLSLDeclaration* pDeclaration = (HLSLDeclaration*)lastStatement->nextStatement;
				pDeclaration->arrayDimension = 0;
				pDeclaration->assignment = NULL;
				pDeclaration->bArray = false;
				pDeclaration->nodeType = HLSLNodeType_Declaration;
				pDeclaration->type.baseType = HLSLBaseType_Uint;
				pDeclaration->name = m_tree->AddString("simd_size");

				lastStatement = lastStatement->nextStatement;
				*/

				prevArgument->nextArgument = m_tree->AddNode<HLSLArgument>("", 0);
				prevArgument = prevArgument->nextArgument;
				prevArgument->name = m_tree->AddString("simd_size");
				prevArgument->type.baseType = HLSLBaseType_Uint;
				prevArgument->semantic = prevArgument->sv_semantic = m_tree->AddString("threads_per_simdgroup");
			}
		}

	}

	// Translate return value semantic.
	if (entryFunction->returnType.baseType != HLSLBaseType_Void)
	{
		if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
		{
			// Our vertex input is a struct and its fields need to be tagged when we generate that
			HLSLStruct* structure = tree->FindGlobalStruct(entryFunction->returnType.typeName);
			if (structure == NULL)
			{
				Error("Vertex shader output struct '%s' not found in shader\n", entryFunction->returnType.typeName);
			}

			HLSLStructField* field = structure->field;
			while (field != NULL)
			{
				if (!field->hidden)
				{
					field->sv_semantic = TranslateOutputSemantic(field->semantic);
				}
				field = field->nextField;
			}
		}
		else
		{
			entryFunction->sv_semantic = TranslateOutputSemantic(entryFunction->semantic);

			//Error("MSL only supports COLOR semantic in return \n", entryFunction->returnType.typeName);
		}
	}

	if (secondaryEntryFunction)
	{
		if (secondaryEntryFunction->returnType.baseType != HLSLBaseType_Void)
		{
			if (secondaryEntryFunction->returnType.baseType == HLSLBaseType_UserDefined)
			{
				// Our vertex input is a struct and its fields need to be tagged when we generate that
				HLSLStruct* structure = tree->FindGlobalStruct(secondaryEntryFunction->returnType.typeName);
				if (structure == NULL)
				{
					Error("Vertex shader output struct '%s' not found in shader\n", secondaryEntryFunction->returnType.typeName);
				}

				HLSLStructField* field = structure->field;
				while (field != NULL)
				{
					if (!field->hidden)
					{
						field->sv_semantic = TranslateOutputSemantic(field->semantic);
					}
					field = field->nextField;
				}
			}
			else
			{
				secondaryEntryFunction->sv_semantic = TranslateOutputSemantic(secondaryEntryFunction->semantic);
			}
		}
	}



}

void MSLGenerator::CleanPrepass()
{
	ClassArgument* currentArg = m_firstClassArgument;
	while (currentArg != NULL)
	{
		ClassArgument* nextArg = currentArg->nextArg;
		delete currentArg;
		currentArg = nextArg;
	}
	delete currentArg;
	m_firstClassArgument = NULL;
	m_lastClassArgument = NULL;
}

void MSLGenerator::PrependDeclarations()
{
	// Any special function stubs we need go here
	// That includes special constructors to emulate HLSL not being strict


	m_writer.WriteLine(0, "#include <metal_stdlib>");

	if(m_tree->gExtension[USE_ATOMIC])
		m_writer.WriteLine(0, "#include <metal_atomic>");

	if (m_target == Target_ComputeShader)
		m_writer.WriteLine(0, "#include <metal_compute>");

	m_writer.WriteLine(0, "using namespace metal;");
	//m_writer.WriteLine(0, "#define MSL 1");
	m_writer.WriteLine(0, "");

	// 'mad' should be translated as 'fma'

	if (m_tree->NeedsFunction("mad"))
	{
		m_writer.WriteLine(0, "inline float mad(float a, float b, float c) {");
		m_writer.WriteLine(1, "return a * b + c;");
		m_writer.WriteLine(0, "}");
		m_writer.WriteLine(0, "inline float2 mad(float2 a, float2 b, float2 c) {");
		m_writer.WriteLine(1, "return a * b + c;");
		m_writer.WriteLine(0, "}");
		m_writer.WriteLine(0, "inline float3 mad(float3 a, float3 b, float3 c) {");
		m_writer.WriteLine(1, "return a * b + c;");
		m_writer.WriteLine(0, "}");
		m_writer.WriteLine(0, "inline float4 mad(float4 a, float4 b, float4 c) {");
		m_writer.WriteLine(1, "return a * b + c;");
		m_writer.WriteLine(0, "}");
	}

	/*
	if (m_tree->NeedsFunction("max"))
	{
	m_writer.WriteLine(0, "inline float max(int a, float b) {");
	m_writer.WriteLine(1, "return max((float)a, b);");
	m_writer.WriteLine(0, "}");
	m_writer.WriteLine(0, "inline float max(float a, int b) {");
	m_writer.WriteLine(1, "return max(a, (float)b);");
	m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction("min"))
	{
	m_writer.WriteLine(0, "inline float min(int a, float b) {");
	m_writer.WriteLine(1, "return min((float)a, b);");
	m_writer.WriteLine(0, "}");
	m_writer.WriteLine(0, "inline float min(float a, int b) {");
	m_writer.WriteLine(1, "return min(a, (float)b);");
	m_writer.WriteLine(0, "}");
	}
	*/

	/*
	if (m_tree->NeedsFunction("lerp"))
	{
	m_writer.WriteLine(0, "template<typename T> inline T mix(T a, T b, int x) {");
	m_writer.WriteLine(1, "return mix(a, b, (float)x);");
	m_writer.WriteLine(0, "}");
	m_writer.WriteLine(0, "#define lerp mix");
	}
	*/

	/*
	if (m_tree->NeedsFunction("mul"))
	{
	const char* am = (m_options.flags & Flag_PackMatrixRowMajor) ? "m * a" : "a * m";
	const char* ma = (m_options.flags & Flag_PackMatrixRowMajor) ? "a * m" : "m * a";

	// @@ Add all mul variants? Replace by * ?

	m_writer.WriteLine(0, "inline float2 mul(float2 a, float2x2 m) {");
	m_writer.WriteLine(1, "return %s;", am);
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float3 mul(float3 a, float3x3 m) {");
	m_writer.WriteLine(1, "return %s;", am);
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float4 mul(float4 a, float4x4 m) {");
	m_writer.WriteLine(1, "return %s;", am);
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float2 mul(float2x2 m, float2 a) {");
	m_writer.WriteLine(1, "return %s;", ma);
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float3 mul(float3x3 m, float3 a) {");
	m_writer.WriteLine(1, "return %s;", ma);
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float4 mul(float4x4 m, float4 a) {");
	m_writer.WriteLine(1, "return %s;", ma);
	m_writer.WriteLine(0, "}");

	// TODO: Support PackMatrixRowMajor for float3x4/float4x3
	m_writer.WriteLine(0, "inline float3 mul(float4 a, float3x4 m) {");
	m_writer.WriteLine(1, "return a * m;");
	m_writer.WriteLine(0, "}");
	}
	*/

	// @@ How do we know if these will be needed? We could write these after parsing the whole file and prepend them.
	/*
	m_writer.WriteLine(0, "inline float4 column(float4x4 m, int i) {");
	m_writer.WriteLine(1, "return float4(m[0][i], m[1][i], m[2][i], m[3][i]);");
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float3 column(float3x4 m, int i) {");
	m_writer.WriteLine(1, "return float3(m[0][i], m[1][i], m[2][i]);");
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float2 column(float2x4 m, int i) {");
	m_writer.WriteLine(1, "return float2(m[0][i], m[1][i]);");
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float4 set_column(thread float4x4& m, int i, float4 v) {");
	m_writer.WriteLine(1, "    m[0][i] = v.x; m[1][i] = v.y; m[2][i] = v.z; m[3][i] = v.w; return v;");
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float3 set_column(thread float3x4& m, int i, float3 v) {");
	m_writer.WriteLine(1, "    m[0][i] = v.x; m[1][i] = v.y; m[2][i] = v.z; return v;");
	m_writer.WriteLine(0, "}");

	m_writer.WriteLine(0, "inline float2 set_column(thread float2x4& m, int i, float2 v) {");
	m_writer.WriteLine(1, "    m[0][i] = v.x; m[1][i] = v.y; return v;");
	m_writer.WriteLine(0, "}");
	
	m_writer.WriteLine(0, "inline float3x3 matrix_ctor(float4x4 m) {");
	m_writer.WriteLine(1, "    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);");
	m_writer.WriteLine(0, "}");
	*/

	if (m_tree->gExtension[USE_3X3_CONVERSION])
	{
		m_writer.WriteLine(0, "inline float3x3 matrix_ctor(float4x4 m)");
		m_writer.WriteLine(0, "{");
		m_writer.WriteLine(1, "    return float3x3(m[0].xyz, m[1].xyz, m[2].xyz);");
		m_writer.WriteLine(0, "}");
	}


	if (m_tree->NeedsFunction("clip"))
	{
		m_writer.WriteLine(0, "inline void clip(float x) {");
		m_writer.WriteLine(1, "if (x < 0.0) discard_fragment();");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction("rcp"))
	{
		m_writer.WriteLine(0, "inline float rcp(float x) {");
		m_writer.WriteLine(1, "return 1.0f / x;");
		m_writer.WriteLine(0, "}");
	}

	/*
	if (m_tree->NeedsFunction("ddx")) m_writer.WriteLine(0, "#define ddx dfdx");
	if (m_tree->NeedsFunction("ddy")) m_writer.WriteLine(0, "#define ddy dfdy");
	if (m_tree->NeedsFunction("frac")) m_writer.WriteLine(0, "#define frac fract");
	*/

	//m_writer.WriteLine(0, "#define mad fma");     // @@ This doesn't seem to work.

	if (m_tree->NeedsFunction("tex2D") ||
		m_tree->NeedsFunction("tex2Dlod") ||
		m_tree->NeedsFunction("tex2Dgrad") ||
		m_tree->NeedsFunction("tex2Dbias") ||
		m_tree->NeedsFunction("tex2Dfetch"))
	{
		m_writer.WriteLine(0, "struct Texture2DSampler {");
		m_writer.WriteLine(1, "const thread texture2d<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "Texture2DSampler(thread const texture2d<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");
	}

	if (m_tree->NeedsFunction("tex2D"))
	{
		m_writer.WriteLine(0, "inline float4 tex2D(Texture2DSampler ts, float2 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord);");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction("tex2Dlod"))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dlod(Texture2DSampler ts, float4 texCoordMip) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordMip.xy, level(texCoordMip.w));");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction("tex2Dgrad"))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dgrad(Texture2DSampler ts, float2 texCoord, float2 gradx, float2 grady) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord.xy, gradient2d(gradx, grady));");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction("tex2Dbias"))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dbias(Texture2DSampler ts, float4 texCoordBias) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordBias.xy, bias(texCoordBias.w));");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction("tex2Dfetch"))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dfetch(Texture2DSampler ts, uint2 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.read(texCoord);");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction("tex3D") ||
		m_tree->NeedsFunction("tex3Dlod"))
	{
		m_writer.WriteLine(0, "struct Texture3DSampler {");
		m_writer.WriteLine(1, "const thread texture3d<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "Texture3DSampler(thread const texture3d<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");
	}

	if (m_tree->NeedsFunction("tex3D"))
	{
		m_writer.WriteLine(0, "inline float4 tex3D(Texture3DSampler ts, float3 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord);");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction("tex3Dlod"))
	{
		m_writer.WriteLine(0, "inline float4 tex3Dlod(Texture3DSampler ts, float4 texCoordMip) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordMip.xyz, level(texCoordMip.w));");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction("texCUBE") ||
		m_tree->NeedsFunction("texCUBElod") ||
		m_tree->NeedsFunction("texCUBEbias"))
	{
		m_writer.WriteLine(0, "struct TextureCubeSampler {");
		m_writer.WriteLine(1, "const thread texturecube<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "TextureCubeSampler(thread const texturecube<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");
	}

	if (m_tree->NeedsFunction("texCUBE"))
	{
		m_writer.WriteLine(0, "inline float4 texCUBE(TextureCubeSampler ts, float3 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord);");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction("texCUBElod"))
	{
		m_writer.WriteLine(0, "inline float4 texCUBElod(TextureCubeSampler ts, float4 texCoordMip) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordMip.xyz, level(texCoordMip.w));");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction("texCUBEbias"))
	{
		m_writer.WriteLine(0, "inline float4 texCUBEbias(TextureCubeSampler ts, float4 texCoordBias) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordBias.xyz, bias(texCoordBias.w));");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction("tex2Dcmp"))
	{
		m_writer.WriteLine(0, "struct Texture2DShadowSampler {");
		m_writer.WriteLine(1, "const thread depth2d<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "Texture2DShadowSampler(thread const depth2d<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");

		m_writer.WriteLine(0, "inline float4 tex2Dcmp(Texture2DShadowSampler ts, float4 texCoordCompare) {");
		if (m_options.flags & Flag_ConstShadowSampler)
		{
			// iOS Metal requires that the sampler in sample_compare is a compile-time constant
			m_writer.WriteLine(1, "constexpr sampler shadow_constant_sampler(mip_filter::none, min_filter::linear, mag_filter::linear, address::clamp_to_edge, compare_func::less);");
			m_writer.WriteLine(1, "return ts.t.sample_compare(shadow_constant_sampler, texCoordCompare.xy, texCoordCompare.z);");
		}
		else
		{
			m_writer.WriteLine(1, "return ts.t.sample_compare(ts.s, texCoordCompare.xy, texCoordCompare.z);");
		}
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction("tex2DMSfetch"))
	{
		m_writer.WriteLine(0, "inline float4 tex2DMSfetch(texture2d_ms<float> t, int2 texCoord, int sample) {");
		m_writer.WriteLine(1, "return t.read(uint2(texCoord), sample);");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction("tex2DArray"))
	{
		m_writer.WriteLine(0, "struct Texture2DArraySampler {");
		m_writer.WriteLine(1, "const thread texture2d_array<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "Texture2DArraySampler(thread const texture2d_array<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");

		m_writer.WriteLine(0, "inline float4 tex2DArray(Texture2DArraySampler ts, float3 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord.xy, texCoord.z + 0.5);"); // 0.5 offset needed on nvidia gpus
		m_writer.WriteLine(0, "}");
	}
}

bool MSLGenerator::Generate(HLSLTree* tree, Target target, const char* entryName, const Options& options)
{
	m_firstClassArgument = NULL;
	m_lastClassArgument = NULL;

	m_tree = tree;
	m_target = target;
	//ASSERT(m_target == Target_VertexShader || m_target == Target_FragmentShader);
	m_entryName = entryName;
	m_secondaryEntryName = entryName;
	m_options = options;
	
	if (m_target == Target::Target_VertexShader)
	{
		m_options.bufferRegisterOffset = 1;
	}


	m_writer.Reset();

	// Find entry point function
	HLSLFunction* entryFunction = tree->FindFunction(entryName);
	if (entryFunction == NULL)
	{
		Error("Entry point '%s' doesn't exist\n", entryName);
		return false;
	}


	// Find entry point function
	HLSLFunction* secondaryEntryFunction = NULL;
	if (m_target == Target_HullShader)
	{
		secondaryEntryFunction = tree->FindFunction(entryName, 1);
		if (secondaryEntryFunction == NULL)
		{
			Error("secondary Entry point '%s' doesn't exist\n", entryName);
			return false;
		}
	}

	Prepass(tree, target, entryFunction, secondaryEntryFunction);

	PrependDeclarations();

	// In MSL, uniforms are parameters for the entry point, not globals:
	// to limit code rewriting, we wrap the entire original shader into a class.
	// Uniforms are then passed to the constructor and copied to member variables.
	const char* shaderClassName = NULL;

	switch (target)
	{
	case MSLGenerator::Target_VertexShader:
		shaderClassName = "Vertex_Shader";
		break;
	case MSLGenerator::Target_FragmentShader:
		shaderClassName = "Fragment_Shader";
		break;
	case MSLGenerator::Target_HullShader:
		shaderClassName = "Hull_Shader";
		break;
	case MSLGenerator::Target_DomainShader:
		shaderClassName = "Domain_Shader";
		break;
	case MSLGenerator::Target_GeometryShader:
		shaderClassName = "Geometry_Shader";
		break;
	case MSLGenerator::Target_ComputeShader:
		shaderClassName = "Compute_Shader";
		break;
	default:
		shaderClassName = "???_Shader";
		break;
	}

	m_writer.WriteLine(0, "struct %s", shaderClassName);
	m_writer.WriteLine(0, "{");
	HLSLRoot* root = m_tree->GetRoot();
	OutputStatements(1, root->statement, NULL);

	// Generate constructor
	m_writer.WriteLine(0, "");
	//m_writer.BeginLine(1);

	m_writer.WriteLine(1, "%s(", shaderClassName);
	const ClassArgument* currentArg = m_firstClassArgument;
	const ClassArgument* prevArg = NULL;

	bool bSkip = false;
	bool bPreSkip = false;

	while (currentArg != NULL)
	{
		if (currentArg->type.addressSpace == HLSLAddressSpace_Constant)
			m_writer.Write(0, "constant ");
		else if (currentArg->type.addressSpace == HLSLAddressSpace_Device)
			m_writer.Write(0, "device ");

		if (currentArg->type.baseType == HLSLBaseType_UserDefined)
		{
			if (currentArg->bStructuredBuffer)
				m_writer.Write("%s* %s", currentArg->type.typeName, currentArg->name);
			else
				m_writer.Write("%s & %s", currentArg->type.typeName, currentArg->name);
		}
		else if (currentArg->type.baseType >= HLSLBaseType_FirstNumeric && currentArg->type.baseType <= HLSLBaseType_LastNumeric)
		{
			if (currentArg->bStructuredBuffer)
				m_writer.Write("%s* %s", currentArg->type.typeName, currentArg->name);
			else
				m_writer.Write("%s & %s", currentArg->type.typeName, currentArg->name);
		}
		else if (currentArg->type.baseType >= HLSLBaseType_Texture1D && currentArg->type.baseType <= HLSLBaseType_TextureCubeArray)
		{
			if (currentArg->type.typeName == NULL)
			{
				if (currentArg->type.array)
				{
					m_writer.Write(0, "array<%s, %d> %s", GetTypeName(currentArg->type), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, currentArg->name);
				}
				else
					m_writer.Write(0, "%s %s", GetTypeName(currentArg->type), currentArg->name);
			}
			else
				m_writer.Write(0, "constant %s::%s & %s", shaderClassName, currentArg->type.typeName, currentArg->name);
		}
		else if (currentArg->type.baseType >= HLSLBaseType_RWTexture1D && currentArg->type.baseType <= HLSLBaseType_RWTexture3D)
		{
			if (currentArg->type.typeName == NULL)
			{
				if (currentArg->type.array)
				{
					m_writer.Write(0, "array<%s, %d> %s", GetTypeName(currentArg->type), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, currentArg->name);
				}
				else
					m_writer.Write(0, "%s %s", GetTypeName(currentArg->type), currentArg->name);
			}
			else
				m_writer.Write(0, "constant %s::%s & %s", shaderClassName, currentArg->type.typeName, currentArg->name);
		}
		else if (currentArg->type.baseType == HLSLBaseType_SamplerState || currentArg->type.baseType == HLSLBaseType_Sampler)
		{			
			if (currentArg->type.array)
				m_writer.Write(0, "array_ref<sampler> %s", currentArg->name);
			else
				m_writer.Write(0, "%s %s", GetTypeName(currentArg->type), currentArg->name);			
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIf)
		{
			if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
				currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
				currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
			{
				bSkip = true;
			}
			else
			{
				m_writer.WriteLine(0, "\n#if %s", currentArg->name);
			}
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfDef)
		{
			if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
				currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
				currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
			{
				bSkip = true;
			}
			else
			{
				m_writer.WriteLine(0, "\n#ifdef %s", currentArg->name);
			}			
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfnDef)
		{
			if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
				currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
				currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
			{
				bSkip = true;
			}
			else
			{
				m_writer.WriteLine(0, "\n#ifndef %s", currentArg->name);
			}
			
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElif)
		{
			if (!bSkip)
				m_writer.Write(0, "\n#elif %s", currentArg->name);

		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElse)
		{
			if (!bSkip)
				m_writer.WriteLine(0, "\n#else\n");

			
			
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorEndif)
		{
			if (!bSkip)
				m_writer.Write(0, "\n#endif\n");
			else
				bSkip = false;
			
		}
		else
		{
			m_writer.Write(0, "%s %s", GetTypeName(currentArg->type), currentArg->name);
		}

		bPreSkip = bSkip;

		prevArg = currentArg;
		currentArg = currentArg->nextArg;

		const ClassArgument* traversalArg = currentArg;

		while (traversalArg)
		{
			if (prevArg->type.baseType == HLSLBaseType_PreProcessorIf ||
				prevArg->type.baseType == HLSLBaseType_PreProcessorIfDef ||
				prevArg->type.baseType == HLSLBaseType_PreProcessorIfnDef ||
				prevArg->type.baseType == HLSLBaseType_PreProcessorElif ||
				prevArg->type.baseType == HLSLBaseType_PreProcessorElse ||
				prevArg->type.baseType == HLSLBaseType_PreProcessorEndif)
			{
				//m_writer.Write("\n");
				break;
			}

			if (traversalArg->type.baseType != HLSLBaseType_PreProcessorIf &&
				traversalArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
				traversalArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
				traversalArg->type.baseType != HLSLBaseType_PreProcessorElif &&
				traversalArg->type.baseType != HLSLBaseType_PreProcessorElse &&
				traversalArg->type.baseType != HLSLBaseType_PreProcessorEndif
				)
			{
				m_writer.Write(",");
				break;
			}

			traversalArg = traversalArg->nextArg;
		}

		
	}
	m_writer.Write(")");

	bSkip = false;
	bPreSkip = false;

	currentArg = m_firstClassArgument;
	if (currentArg) m_writer.Write(" :\n");
	while (currentArg != NULL)
	{
		bPreSkip = bSkip;
		if (currentArg->type.baseType == HLSLBaseType_PreProcessorIf ||
			currentArg->type.baseType == HLSLBaseType_PreProcessorIfDef ||
			currentArg->type.baseType == HLSLBaseType_PreProcessorIfnDef ||
			currentArg->type.baseType == HLSLBaseType_PreProcessorElif ||
			currentArg->type.baseType == HLSLBaseType_PreProcessorElse ||
			currentArg->type.baseType == HLSLBaseType_PreProcessorEndif
			)
		{
			if (currentArg->type.baseType == HLSLBaseType_PreProcessorIf)
			{
				if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
				{
					bSkip = true;
				}
				else
				{
					m_writer.WriteLine(0, "\n#if %s", currentArg->name);
				}
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfDef)
			{
				if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
				{
					bSkip = true;
				}
				else
				{
					m_writer.WriteLine(0, "\n#ifdef %s", currentArg->name);
				}
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfnDef)
			{
				if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
				{
					bSkip = true;
				}
				else
				{
					m_writer.WriteLine(0, "\n#ifndef %s", currentArg->name);
				}
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElif)
			{
				if (!bSkip)
					m_writer.Write(0, "\n#elif %s", currentArg->name);
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElse)
			{
				if (!bSkip)
					m_writer.Write(0, "\n#else\n", currentArg->name);
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorEndif)
			{
				if (!bSkip)
					m_writer.Write(0, "\n#endif\n");
				else
					bSkip = false;
			}
			currentArg = currentArg->nextArg;
		}
		else
		{
			m_writer.Write(0, "%s(%s)", currentArg->name, currentArg->name);

			prevArg = currentArg;
			currentArg = currentArg->nextArg;



			const ClassArgument* traversalArg = currentArg;

			while (traversalArg)
			{
				if (prevArg->type.baseType == HLSLBaseType_PreProcessorIf ||
					prevArg->type.baseType == HLSLBaseType_PreProcessorIfDef ||
					prevArg->type.baseType == HLSLBaseType_PreProcessorIfnDef ||
					prevArg->type.baseType == HLSLBaseType_PreProcessorElif ||
					prevArg->type.baseType == HLSLBaseType_PreProcessorElse ||
					prevArg->type.baseType == HLSLBaseType_PreProcessorEndif)
				{
					//m_writer.Write("\n");
					break;
				}

				if (traversalArg->type.baseType != HLSLBaseType_PreProcessorIf &&
					traversalArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
					traversalArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
					traversalArg->type.baseType != HLSLBaseType_PreProcessorElif &&
					traversalArg->type.baseType != HLSLBaseType_PreProcessorElse &&
					traversalArg->type.baseType != HLSLBaseType_PreProcessorEndif
					)
				{
					m_writer.Write(",");
					break;
				}

				traversalArg = traversalArg->nextArg;
			}
		}

		
	}
	m_writer.EndLine(" {}");

	m_writer.WriteLine(0, "};"); // Class


								 // Generate real entry point, the one called by Metal
	m_writer.WriteLine(0, "");

	// If function return value has a non-color output semantic, declare a temporary struct for the output.
	bool wrapReturnType = false;
	if (entryFunction->sv_semantic != NULL && strcmp(entryFunction->sv_semantic, "color(0)") != 0)
	{
		wrapReturnType = true;

		m_writer.WriteLine(0, "struct %s_output { %s tmp [[%s]]; };", entryName, GetTypeName(entryFunction->returnType), entryFunction->sv_semantic);

		m_writer.WriteLine(0, "");
	}


	//print Attributes for main Func
	m_writer.BeginLine(0);
	HLSLStatement* _statement = root->statement;

	while (_statement)
	{
		OutputAttributes(0, _statement->attributes, true);
		_statement = _statement->nextStatement;
	}

	//temporarily numthreads for hull shader
	if (m_target == Target_HullShader)
	{
		m_writer.WriteLine(0, "//************************************************ Notice ************************************************//");
		m_writer.WriteLine(0, "// The user should check threads count in each group because metal is using compute shader as hull shader.//");
		m_writer.WriteLine(0, "// Also, this translator cannot know the number of maxmium threads needed to tessellate.                   //");
		m_writer.WriteLine(0, "// So, the user might need to add additional logic to cover it.                                          //");
		m_writer.WriteLine(0, "//********************************************************************************************************//");

		m_writer.Write(0, "//[numthreads(%d, %d, %d)]", 32, 1, 1);
	}

	m_writer.EndLine(0);

	m_writer.BeginLine(0);

	// @@ Add/Translate function attributes.
	// entryFunction->attributes

	if (m_target == Target_VertexShader)
	{
		m_writer.Write("vertex ");
	}
	else if (m_target == Target_FragmentShader)
	{
		m_writer.Write("fragment ");
	}
	else if (m_target == Target_HullShader)
	{
		m_writer.Write("kernel ");
	}
	else if (m_target == Target_DomainShader)
	{
		m_writer.Write("vertex ");
	}
	else if (m_target == Target_GeometryShader)
	{
		m_writer.Write("geometry ");
	}
	else if (m_target == Target_ComputeShader)
	{
		m_writer.Write("kernel ");
	}
	else
	{
		m_writer.Write("fragment ");
	}



	// Return type.
	if (wrapReturnType)
	{
		m_writer.Write("%s_output", entryName);
	}
	else
	{
		//fragment should return float4
		if (m_target == Target_FragmentShader)
		{
			if (String_Equal(GetTypeName(entryFunction->returnType), "float") ||
				String_Equal(GetTypeName(entryFunction->returnType), "float2") ||
				String_Equal(GetTypeName(entryFunction->returnType), "float3"))
				m_writer.Write("float4");
			else if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
				m_writer.Write("%s::%s", shaderClassName, GetTypeName(entryFunction->returnType));
			else
				m_writer.Write("%s", GetTypeName(entryFunction->returnType));

		}
		//hull should return void
		else if (m_target == Target_HullShader)
		{
			m_writer.Write("void");
		}
		else
		{
			if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
				m_writer.Write("%s::", shaderClassName);

			if (entryFunction->returnType.baseType == HLSLBaseType_PatchControlPoint)
				m_writer.Write("%s::", shaderClassName);

			m_writer.Write("%s", GetTypeName(entryFunction->returnType));
		}
	}


	m_writer.Write(" stageMain(\n");

	int argumentCount = 0;
	HLSLArgument* argument = entryFunction->argument;

	bool bFirstStruct = false;
	const char* firstTypeName = NULL;

	while (argument != NULL)
	{
		if (!argument->hidden)
		{
			if (argument->type.baseType == HLSLBaseType_UserDefined || argument->type.baseType == HLSLBaseType_OutputPatch)
			{
				if (m_target == Target_HullShader)
					m_writer.Write(1,"constant %s::", shaderClassName);
				else
					m_writer.Write(1, "%s::", shaderClassName);
			}

			if (m_target == Target_HullShader)
				m_writer.Write("%s* %s", GetTypeName(argument->type), argument->name);
			else
			{
				m_writer.Write("%s %s", GetTypeName(argument->type), argument->name);
			}

			// @@ IC: We are assuming that the first argument (x) -> first struct (o) is the 'stage_in'. 
			if (bFirstStruct == false && argument->type.baseType == HLSLBaseType_UserDefined || argument->type.baseType == HLSLBaseType_OutputPatch)
			{
				bFirstStruct = true;
				firstTypeName = argument->type.typeName;

				if (m_target == Target_HullShader)
					m_writer.Write(" [[buffer(0)]]");
				else
					m_writer.Write(" [[stage_in]]");

			}
			else if (argument->sv_semantic)
			{
				m_writer.Write(" [[%s]]", argument->sv_semantic);
			}
			argumentCount++;
		}
		else
		{
			argument = argument->nextArgument;
			continue;
		}

		argument = argument->nextArgument;

		if (argument && !argument->hidden)
		{
			m_writer.Write(",\n");
		}
	}

	currentArg = m_firstClassArgument;

	prevArg = NULL;

	bSkip = false;
	bPreSkip = false;
	
	bool bIntent = false;

	while (currentArg != NULL)
	{
		ClassArgument* nextArg = currentArg->nextArg;		

		if (prevArg && prevArg->preprocessorContents)
		{
			if(!bPreSkip && !bSkip)
				m_writer.Write("\n");
		}
		else if (nextArg &&
			nextArg->type.baseType != HLSLBaseType_PreProcessorIf &&
			nextArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
			nextArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
			nextArg->type.baseType != HLSLBaseType_PreProcessorElif &&
			nextArg->type.baseType != HLSLBaseType_PreProcessorElse &&
			nextArg->type.baseType != HLSLBaseType_PreProcessorEndif)
		{
			if (!bSkip)
				m_writer.Write(",\n");
		}
		else if(nextArg &&
			(currentArg->type.baseType != HLSLBaseType_PreProcessorIf &&
			 currentArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
			 currentArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
			 currentArg->type.baseType != HLSLBaseType_PreProcessorElif &&
			 currentArg->type.baseType != HLSLBaseType_PreProcessorElse &&
			 currentArg->type.baseType != HLSLBaseType_PreProcessorEndif))
		{
			if (!bSkip)
				m_writer.Write(",\n");			
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorEndif)
		{
			if (!bSkip)
				m_writer.Write("\n");
		}
		else if (nextArg == NULL &&
			currentArg->type.baseType != HLSLBaseType_PreProcessorIf &&
			currentArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
			currentArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
			currentArg->type.baseType != HLSLBaseType_PreProcessorElif &&
			currentArg->type.baseType != HLSLBaseType_PreProcessorElse &&
			currentArg->type.baseType != HLSLBaseType_PreProcessorEndif)
		{
			if (!bSkip)
				m_writer.Write(",\n");
		}


		bPreSkip = bSkip;

		bIntent = false;

		if (currentArg->type.addressSpace == HLSLAddressSpace_Constant)
		{
			m_writer.Write(1, "constant ");
			bIntent = true;
		}
		else if (currentArg->type.addressSpace == HLSLAddressSpace_Device)
		{
			m_writer.Write(1, "device ");
			bIntent = true;
		}

		if (currentArg->type.baseType == HLSLBaseType_UserDefined || currentArg->type.baseType == HLSLBaseType_OutputPatch)
		{
			if (currentArg->bStructuredBuffer)
				m_writer.Write(bIntent ? 0 : 1, "%s::%s* %s [[%s]]", shaderClassName, currentArg->type.typeName, currentArg->name, currentArg->registerName);
			else
				m_writer.Write(bIntent ? 0 : 1, "%s::%s & %s [[%s]]", shaderClassName, currentArg->type.typeName, currentArg->name, currentArg->registerName);

		}
		else if (currentArg->type.baseType >= HLSLBaseType_FirstNumeric && currentArg->type.baseType <= HLSLBaseType_LastNumeric)
		{
			if (currentArg->bStructuredBuffer)
				m_writer.Write("%s* %s [[%s]]", currentArg->type.typeName, currentArg->name, currentArg->registerName);
			else
				m_writer.Write("%s & %s [[%s]]", currentArg->type.typeName, currentArg->name, currentArg->registerName);
		}
		else if (currentArg->type.baseType >= HLSLBaseType_Texture1D && currentArg->type.baseType <= HLSLBaseType_TextureCubeArray)
		{
			if (currentArg->type.typeName == NULL)
			{
				if (currentArg->type.array)
					m_writer.Write(bIntent ? 0 : 1, "array<%s, %d> %s [[%s]]", GetTypeName(currentArg->type), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, currentArg->name, currentArg->registerName);
				else
					m_writer.Write(bIntent ? 0 : 1, "%s %s [[%s]]", GetTypeName(currentArg->type), currentArg->name, currentArg->registerName);
			}
			else
				m_writer.Write(bIntent ? 0 : 1, "constant %s::%s & %s [[%s]]", shaderClassName, currentArg->type.typeName, currentArg->name, currentArg->registerName);
		}
		else if (currentArg->type.baseType >= HLSLBaseType_RWTexture1D && currentArg->type.baseType <= HLSLBaseType_RWTexture3D)
		{
			if (currentArg->type.typeName == NULL)
			{
				if (currentArg->type.array)
				{
					m_writer.Write(bIntent ? 0 : 1, "array<%s, %d> %s [[%s]]", GetTypeName(currentArg->type), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, currentArg->name, currentArg->registerName);
				}
				else
					m_writer.Write(bIntent ? 0 : 1, "%s %s [[%s]]", GetTypeName(currentArg->type), currentArg->name, currentArg->registerName);
			}
			else
				m_writer.Write(bIntent ? 0 : 1, "constant %s::%s & %s [[%s]]", shaderClassName, currentArg->type.typeName, currentArg->name, currentArg->registerName);
		}
		else if (currentArg->type.baseType == HLSLBaseType_SamplerState || currentArg->type.baseType == HLSLBaseType_Sampler)
		{
			if (currentArg->type.array)
			{
				m_writer.Write(bIntent ? 0 : 1, "array<sampler, ");
				OutputExpression(currentArg->type.arraySize, NULL, NULL, NULL, false);
				m_writer.Write(0, "> %s [[%s]]", currentArg->name, currentArg->registerName);
			}
			else
				m_writer.Write(bIntent ? 0 : 1, "%s %s [[%s]]", GetTypeName(currentArg->type), currentArg->name, currentArg->registerName);
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIf)
		{
			if (currentArg->nextArg)
			{
				if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
				{
					bSkip = true;
				}
				else
				{
					m_writer.Write(0, "\n#if %s", currentArg->name);
				}
			}
			
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfDef)
		{
			if (currentArg->nextArg)
			{
				if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
				{
					bSkip = true;
				}
				else
				{
					m_writer.Write(0, "\n#ifdef %s", currentArg->name);
				}
			}
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfnDef)
		{
			if (currentArg->nextArg)
			{
				if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
					currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
				{
					bSkip = true;
				}
				else
				{
					m_writer.Write(0, "\n#ifndef %s", currentArg->name);
				}
			}			
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElif)
		{
			if(!bSkip)
				m_writer.Write(0, "#elif %s", currentArg->name);
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElse)
		{
			if (!bSkip)
				m_writer.Write(0, "#else");
		}
		else if (currentArg->type.baseType == HLSLBaseType_PreProcessorEndif)
		{
			if (!bSkip)
				m_writer.Write(0, "#endif\n");
			else
				bSkip = false;
		}
		else
		{
			m_writer.Write(1, "%s %s [[%s]]", GetTypeName(currentArg->type), currentArg->name, currentArg->registerName);
		}

		prevArg = currentArg;
		currentArg = currentArg->nextArg;

		
	}



	if (m_target == Target_HullShader)
	{
		//need to add additional arguments (PatchTess, Hullshader Output, threadId)

		//PatchTess
		HLSLStatement* _statement = root->statement;

		while (_statement)
		{
			if (_statement->nodeType == HLSLNodeType_Function)
			{
				HLSLFunction* function = static_cast<HLSLFunction*>(_statement);

				if (function->bPatchconstantfunc)
				{
					m_writer.Write(",\n");
					m_writer.Write(1, "device %s::%s* %s [[%s]]", shaderClassName, GetTypeName(function->returnType), "tessellationFactorBuffer", "buffer(10)" /*Temporary!!!*/);
				}
				else if (String_Equal(function->name, m_secondaryEntryName))
				{
					m_writer.Write(",\n");
					m_writer.Write(1, "device %s::%s* %s [[%s]]", shaderClassName, function->returnType.typeName, "hullOutputBuffer", "buffer(11)" /*Temporary!!!*/);
				}
			}
			_statement = _statement->nextStatement;
		}

		m_writer.Write(",\n");
		m_writer.Write(1, "uint threadId[[thread_position_in_grid]]");
	}

	
	

	m_writer.EndLine(")");
	m_writer.WriteLine(0, "{");

	// Create local variables for each of the parameters we'll need to pass
	// into the entry point function.
	argument = entryFunction->argument;

	while (argument != NULL)
	{
		m_writer.BeginLine(1);

		if (argument->hidden)
		{
			argument = argument->nextArgument;
			continue;
		}

		if (firstTypeName == argument->type.typeName && m_target == Target_HullShader)
		{
			argument = argument->nextArgument;
			continue;
		}


		if ((firstTypeName == argument->type.typeName) && (argument->type.baseType == HLSLBaseType_UserDefined))
			m_writer.Write("%s::", shaderClassName);

		//const char* newDeclarationName = m_tree->AddStringFormat("%s::%s0", shaderClassName, argument->name);
		const char* newArgumentName = m_tree->AddStringFormat("%s0", argument->name);


		if (argument->type.baseType == HLSLBaseType_OutputPatch)
		{
			/*
			//disable array
			HLSLType tempType = argument->type;
			tempType.array = false;

			OutputDeclaration(tempType, newArgumentName, NULL, NULL);
			*/
		}
		else
		{
			OutputDeclaration(argument->type, newArgumentName, NULL, NULL);
			m_writer.EndLine(";");
		}

		//if (argument->modifier != HLSLArgumentModifier_Out)
		if (!((argument->modifier == HLSLArgumentModifier_Out) || (argument->modifier == HLSLArgumentModifier_Inout)))
		{
			// Set the value for the local variable.
			if (argument->type.baseType == HLSLBaseType_UserDefined || argument->type.baseType == HLSLBaseType_OutputPatch)
			{
				HLSLStruct* structDeclaration = tree->FindGlobalStruct(argument->type.typeName);
				ASSERT(structDeclaration != NULL);


				if (argument->type.array)
				{

					if (argument->type.baseType == HLSLBaseType_OutputPatch)
					{
						//m_writer.WriteLine(1, "%s = %s;", newArgumentName, argument->name);
					}
					else
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
									const char* builtInSemantic = GetBuiltInSemantic(field->semantic, HLSLArgumentModifier_In);
									if (builtInSemantic)
									{
										m_writer.WriteLine(1, "%s[%d].%s = %s[%d];", newArgumentName, i, field->name, builtInSemantic, i);
									}
									else
									{
										m_writer.WriteLine(1, "%s[%d].%s = %s[%d];", newArgumentName, i, field->name, field->semantic, i);
									}
								}
								field = field->nextField;
							}
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
							const char* builtInSemantic = GetBuiltInSemantic(field->semantic, HLSLArgumentModifier_In, argument->name, field->name);
							if (builtInSemantic)
							{
								m_writer.WriteLine(1, "%s.%s = %s;", newArgumentName, field->name, builtInSemantic);
							}
							else
							{
								m_writer.WriteLine(1, "%s.%s = %s.%s;", newArgumentName, field->name, argument->name, field->name);
							}
						}
						field = field->nextField;
					}
				}


			}
			else if (argument->semantic != NULL)
			{
				const char* builtInSemantic = GetBuiltInSemantic(argument->semantic, HLSLArgumentModifier_In);
				/*
				if (builtInSemantic)
				{
					m_writer.WriteLine(1, "%s = %s;", newArgumentName, builtInSemantic);
				}
				else
				*/
				{
					m_writer.WriteLine(1, "%s = %s;", newArgumentName, argument->name);
				}
			}
		}

		argument = argument->nextArgument;
	}


	bSkip = false;
	bPreSkip = false;

	// Create the helper class instance and call the entry point from the original shader
	m_writer.BeginLine(1);
	m_writer.Write("%s %s", shaderClassName, entryName);

	currentArg = m_firstClassArgument;
	
	//const char* argumentList[128];
	int arg_counter = 0;
	//bool bDup = false;
	//bool bIgnore = false;
	if (currentArg)
	{
		m_writer.Write("(\n");
		//m_writer.Write(1, "");

		prevArg = NULL;

		while (currentArg != NULL)
		{
			ClassArgument* nextArg = currentArg->nextArg;

			if (prevArg && prevArg->preprocessorContents)
			{
				if (!bPreSkip && !bSkip)
					m_writer.Write("\n");
			}
			else if (nextArg &&
				nextArg->type.baseType != HLSLBaseType_PreProcessorIf &&
				nextArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
				nextArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
				nextArg->type.baseType != HLSLBaseType_PreProcessorElif &&
				nextArg->type.baseType != HLSLBaseType_PreProcessorElse &&
				nextArg->type.baseType != HLSLBaseType_PreProcessorEndif)
			{
				if (!bSkip && (m_firstClassArgument != currentArg))
					m_writer.Write(",\n");
			}
			else if (nextArg &&
				(currentArg->type.baseType != HLSLBaseType_PreProcessorIf &&
					currentArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
					currentArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
					currentArg->type.baseType != HLSLBaseType_PreProcessorElif &&
					currentArg->type.baseType != HLSLBaseType_PreProcessorElse &&
					currentArg->type.baseType != HLSLBaseType_PreProcessorEndif))
			{
				if (!bSkip)
				{
					m_writer.Write(",\n");
				}
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorEndif)
			{
				if (!bSkip)
					m_writer.Write("\n");
			}
			else if (nextArg == NULL &&
				currentArg->type.baseType != HLSLBaseType_PreProcessorIf &&
				currentArg->type.baseType != HLSLBaseType_PreProcessorIfDef &&
				currentArg->type.baseType != HLSLBaseType_PreProcessorIfnDef &&
				currentArg->type.baseType != HLSLBaseType_PreProcessorElif &&
				currentArg->type.baseType != HLSLBaseType_PreProcessorElse &&
				currentArg->type.baseType != HLSLBaseType_PreProcessorEndif)
			{
				if (!bSkip && (m_firstClassArgument != currentArg))
					m_writer.Write(",\n");
			}


			bPreSkip = bSkip;

			bIntent = false;

				
			if (currentArg->type.baseType == HLSLBaseType_PreProcessorIf)
			{
				if (currentArg->nextArg)
				{
					if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
						currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
						currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
					{
						bSkip = true;
					}
					else
					{
						m_writer.Write(0, "\n#if %s", currentArg->name);
					}
				}

			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfDef)
			{
				if (currentArg->nextArg)
				{
					if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
						currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
						currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
					{
						bSkip = true;
					}
					else
					{
						m_writer.Write(0, "\n#ifdef %s", currentArg->name);
					}
				}
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorIfnDef)
			{
				if (currentArg->nextArg)
				{
					if (currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElif ||
						currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorElse ||
						currentArg->nextArg->type.baseType == HLSLBaseType_PreProcessorEndif)
					{
						bSkip = true;
					}
					else
					{
						m_writer.Write(0, "\n#ifndef %s", currentArg->name);
					}
				}
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElif)
			{
				if (!bSkip)
					m_writer.Write(0, "#elif %s", currentArg->name);
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorElse)
			{
				if (!bSkip)
					m_writer.Write(0, "#else");
			}
			else if (currentArg->type.baseType == HLSLBaseType_PreProcessorEndif)
			{
				if (!bSkip)
					m_writer.Write(0, "#endif\n");
				else
					bSkip = false;
			}
			else
			{
				m_writer.Write(1, "%s", currentArg->name);
			}

			prevArg = currentArg;
			currentArg = currentArg->nextArg;
			
		}

		m_writer.Write(")");
	}

	m_writer.EndLine(";");




	if (m_target == Target_HullShader)
	{
		m_writer.BeginLine(2);
		m_writer.Write("%s::%s VS_Out = %s.%s(", shaderClassName, entryFunction->returnType.typeName, entryName, entryFunction->name);

		argument = entryFunction->argument;
		while (argument != NULL)
		{
			if (!argument->hidden)
			{
				const char* newArgumentName = m_tree->AddStringFormat("%s0", argument->name);

				if (argument->type.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
				{
					if (argument->type.baseType == HLSLBaseType_UserDefined)
						m_writer.Write("%s[threadId]", argument->name);
					else
						m_writer.Write("%s", argument->name);
				}
				else
					m_writer.Write("%s", newArgumentName);
			}
			else
			{
				argument = argument->nextArgument;
				continue;
			}

			argument = argument->nextArgument;
			if (argument && !argument->hidden)
			{
				m_writer.Write(", ");
			}
		}

		m_writer.EndLine(");");

		m_writer.BeginLine(2);
		m_writer.Write("%s::%s HS_Out = %s.%s(", shaderClassName, secondaryEntryFunction->returnType.typeName, entryName, secondaryEntryFunction->name);

		argument = secondaryEntryFunction->argument;
		while (argument != NULL)
		{
			if (!argument->hidden)
			{
				const char* newArgumentName = m_tree->AddStringFormat("%s0", argument->name);

				if (argument->type.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
				{
					if (argument->type.baseType == HLSLBaseType_InputPatch && String_Equal(entryFunction->returnType.typeName, argument->type.structuredTypeName))
					{
						m_writer.Write("&VS_Out ");
					}
					else if (String_Equal(argument->semantic, "SV_PrimitiveID") || String_Equal(argument->semantic, "SV_OutputControlPointID"))
					{
						m_writer.Write("0");
					}
					else
						m_writer.Write("%s", argument->name);
				}
				else
					m_writer.Write("%s", newArgumentName);
			}
			else
			{
				argument = argument->nextArgument;
				continue;
			}

			argument = argument->nextArgument;
			if (argument && !argument->hidden)
			{
				m_writer.Write(", ");
			}
		}

		m_writer.EndLine(");");

		//find PatchConstantFunction
		HLSLStatement* _statement = root->statement;
		HLSLFunction* PCF = NULL;

		while (_statement)
		{
			if (_statement->nodeType == HLSLNodeType_Function)
			{
				HLSLFunction* function = static_cast<HLSLFunction*>(_statement);

				if (function->bPatchconstantfunc)
				{
					PCF = function;
					break;
				}
			}
			_statement = _statement->nextStatement;
		}


		if (PCF)
		{
			m_writer.BeginLine(2);
			m_writer.Write("%s::%s Patch_Out = %s.%s(", shaderClassName, PCF->returnType.typeName, entryName, PCF->name);

			argument = PCF->argument;
			while (argument != NULL)
			{
				if (!argument->hidden)
				{
					const char* newArgumentName = m_tree->AddStringFormat("%s0", argument->name);

					if (argument->type.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
					{
						if (argument->type.baseType == HLSLBaseType_InputPatch && String_Equal(entryFunction->returnType.typeName, argument->type.structuredTypeName))
						{
							m_writer.Write("&VS_Out ");
						}
						else if (String_Equal(argument->semantic, "SV_PrimitiveID") || String_Equal(argument->semantic, "SV_OutputControlPointID"))
						{
							m_writer.Write("0");
						}
						else
							m_writer.Write("%s", argument->name);
					}
					else
						m_writer.Write("%s", newArgumentName);
				}
				else
				{
					argument = argument->nextArgument;
					continue;
				}

				argument = argument->nextArgument;
				if (argument && !argument->hidden)
				{
					m_writer.Write(", ");
				}
			}

			m_writer.EndLine(");");
		}

		m_writer.BeginLine(2);
		m_writer.EndLine("hullOutputBuffer[threadId] = HS_Out;");

		m_writer.BeginLine(2);
		m_writer.EndLine("tessellationFactorBuffer[threadId] = Patch_Out;");

	}
	else
	{
		//m_writer.BeginLine(2);

		if (wrapReturnType)
		{
			m_writer.Write(1, "%s_output output; output.tmp = %s.%s(", entryName, entryName, entryName);
		}
		else
		{
			m_writer.Write(1, "return %s.%s(", entryName, entryName);
		}

		argument = entryFunction->argument;
		while (argument != NULL)
		{
			if (!argument->hidden)
			{
				const char* newArgumentName = m_tree->AddStringFormat("%s0", argument->name);

				if (argument->type.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
					m_writer.Write("%s", argument->name);
				else
					m_writer.Write("%s", newArgumentName);
			}
			else
			{
				argument = argument->nextArgument;
				continue;
			}

			argument = argument->nextArgument;
			if (argument && !argument->hidden)
			{
				m_writer.Write(", ");
			}
		}

		m_writer.EndLine(");");

		if (wrapReturnType)
		{
			m_writer.WriteLine(1, "return output;");
		}


	}

	m_writer.WriteLine(0, "}");



	CleanPrepass();
	m_tree = NULL;

	// Any final check goes here, but shouldn't be needed as the Metal compiler is solid

	return !m_error;
}

const char* MSLGenerator::GetBuiltInSemantic(const char* semantic, HLSLArgumentModifier modifier, const char* argument, const char* field)
{
	/*
	if (m_target == Target_VertexShader && modifier == HLSLArgumentModifier_In && (String_Equal(semantic, "SV_InstanceID") || String_Equal(semantic, "INSTANCE_ID")))
	{
		return "instance_id";
	}
	else if (m_target == Target_VertexShader && modifier == HLSLArgumentModifier_In && (String_Equal(semantic, "SV_VertexID") || String_Equal(semantic, "VERTEX_ID")))
	{
		return "vertex_id";
	}
	else
	*/
	if (m_target == Target_FragmentShader && modifier == HLSLArgumentModifier_In && (String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "SV_Position")))
	{
		const char* newArgument = m_tree->AddStringFormat("float4(%s.%s.xyz, 1.0 / %s.%s.w)", argument, field, argument, field);
		return newArgument;
	}

	return NULL;
}

const char* MSLGenerator::GetResult() const
{
	return m_writer.GetResult();
}

void MSLGenerator::OutputStatements(int indent, HLSLStatement* statement, const HLSLFunction* function)
{
	// Main generator loop: called recursively
	while (statement != NULL)
	{
		if (statement->hidden)
		{
			statement = statement->nextStatement;
			continue;
		}

		OutputAttributes(indent, statement->attributes, false);

		if (statement->nodeType == HLSLNodeType_Declaration)
		{
			HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);
			m_writer.BeginLine(indent, declaration->fileName, declaration->line);
			OutputDeclaration(declaration, function);
			m_writer.EndLine(";");
		}
		else if (statement->nodeType == HLSLNodeType_Preprocessor)
		{
			HLSLpreprocessor* preprocessor = static_cast<HLSLpreprocessor*>(statement);

			switch (preprocessor->type)
			{
			case HLSLBaseType_UserMacro:
				//m_writer.WriteLine(0, "%s", preprocessor->contents);
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

						OutputExpression(hlslExp, NULL, NULL, function, false);
						hlslExp = hlslExp->nextExpression;
					}

					m_writer.Write(0, "]");
				}

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
					m_writer.Write(0, "#define %s ", preprocessor->name);
					HLSLExpression* expression = preprocessor->expression;

					while (expression)
					{
						OutputExpression(expression, NULL, NULL, function, true);
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

			if (textureState->type.baseType >= HLSLBaseType_Texture1D &&
				textureState->type.baseType <= HLSLBaseType_TextureCubeArray)
			{
				HLSLType type;
				type.baseType = textureState->type.baseType;


				if (textureState->bArray)
				{
					m_writer.WriteLine(indent, "array<%s, %d> %s;", GetTypeName(type), textureState->arrayIndex[0] == 0 ? DEFAULT_TEXTURE_COUNT : textureState->arrayIndex[0], textureState->name);
				}
				else
				{
					m_writer.BeginLine(indent, textureState->fileName, textureState->line);

					m_writer.Write("%s", GetTypeName(type));
					m_writer.Write(" %s", textureState->name);

					m_writer.EndLine(";");
				}
			}
			else if (textureState->type.baseType >= HLSLBaseType_RasterizerOrderedTexture1D &&
				textureState->type.baseType <= HLSLBaseType_RasterizerOrderedTexture3D)
			{
				HLSLType type;
				type.baseType = textureState->type.baseType;

				if (textureState->bArray)
				{
					//multiple Array???????????
					m_writer.WriteLine(indent, "struct %sData", textureState->name);
					m_writer.WriteLine(indent, "{");

					for (int i = 0; i < (int)textureState->arrayDimension; i++)
					{
						m_writer.WriteLine(indent + 1, "array<%s, %d> textures;", GetTypeName(type), textureState->arrayIndex[i]);
					}
					m_writer.WriteLine(indent, "}");
				}
				else
				{
					m_writer.BeginLine(indent, textureState->fileName, textureState->line);

					m_writer.Write("%s", GetTypeName(type));
					m_writer.Write(" %s", textureState->name);

					m_writer.EndLine(";");
				}
			}
			else if (textureState->type.baseType >= HLSLBaseType_RWTexture1D &&
					 textureState->type.baseType <= HLSLBaseType_RWTexture3D)
			{
				HLSLType type;
				type.baseType = textureState->type.baseType;

				if (textureState->bArray)
				{
					//multiple Array???????????
					m_writer.WriteLine(indent, "struct %sData", textureState->name);
					m_writer.WriteLine(indent, "{");

					for (int i = 0; i < (int)textureState->arrayDimension; i++)
					{
						m_writer.WriteLine(indent + 1, "array<%s, %d> textures;", GetTypeName(type), textureState->arrayIndex[i]);
					}
					m_writer.WriteLine(indent, "}");
				}
				else
				{
					m_writer.BeginLine(indent, textureState->fileName, textureState->line);

					m_writer.Write("%s", GetTypeName(type));
					m_writer.Write(" %s", textureState->name);

					m_writer.EndLine(";");
				}
			}
		}
		//Does Metal support groupshared memory????
		else if (statement->nodeType == HLSLNodeType_GroupShared)
		{
			HLSLGroupShared* pGroupShared = static_cast<HLSLGroupShared*>(statement);

			//m_writer.Write(0, "shared ");
			OutputDeclaration(pGroupShared->declaration, function);
			m_writer.EndLine(";");

		}
		else if (statement->nodeType == HLSLNodeType_SamplerState)
		{
			HLSLSamplerState* samplerState = static_cast<HLSLSamplerState*>(statement);

			m_writer.BeginLine(indent, samplerState->fileName, samplerState->line);

			if (samplerState->bStructured)
			{
				m_writer.WriteLineTagged(indent, samplerState->fileName, samplerState->line, "SamplerState %s {", samplerState->name);
				HLSLSamplerStateExpression* expression = samplerState->expression;

				while (expression != NULL)
				{
					if (!expression->hidden)
					{
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
				if (samplerState->type.array)
				{
					m_writer.Write("array_ref<sampler> %s", samplerState->name);
				}
				else
				{
					m_writer.Write("sampler %s", samplerState->name);
				}
					

				//	m_writer.Write("[");
				//	OutputExpression(samplerState->type.arraySize, NULL, NULL, function, false);
				//	m_writer.Write("]");
				

				m_writer.EndLine(";");
			}
		}
		else if (statement->nodeType == HLSLNodeType_Struct)
		{
			HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
			OutputStruct(indent, structure);
		}
		else if (statement->nodeType == HLSLNodeType_Buffer)
		{
			HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);

			if (buffer->type.baseType == HLSLBaseType_CBuffer || buffer->type.baseType == HLSLBaseType_TBuffer || buffer->type.baseType == HLSLBaseType_ConstantBuffer)
			{
				OutputBuffer(indent, buffer);
			}
			else if (buffer->type.baseType == HLSLBaseType_StructuredBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				HLSLType tempType;
				tempType.baseType = buffer->type.elementType;

				m_writer.WriteLine(indent, "constant %s* %s;", buffer->type.elementType != HLSLBaseType_Unknown ? getElementTypeAsStr(buffer->type) : GetTypeName(tempType), buffer->name);
			}
			else if (buffer->type.baseType == HLSLBaseType_RWBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				m_writer.Write(indent, "device ");
				const char* atomic = "";

				if (buffer->bAtomic)
					atomic = "atomic_";

				switch (buffer->type.elementType)
				{
				case HLSLBaseType_Float:
					m_writer.Write("%sfloat*", atomic);
					break;
				case HLSLBaseType_Float2:
					m_writer.Write("%sfloat2*", atomic);
					break;
				case HLSLBaseType_Float3:
					m_writer.Write("%sfloat3*", atomic);
					break;
				case HLSLBaseType_Float4:
					m_writer.Write("%sfloat4*", atomic);
					break;
				case HLSLBaseType_Bool:
					m_writer.Write("%sbool*", atomic);
					break;
				case HLSLBaseType_Bool2:
					m_writer.Write("%sbool2*", atomic);
					break;
				case HLSLBaseType_Bool3:
					m_writer.Write("%sbool3*", atomic);
					break;
				case HLSLBaseType_Bool4:
					m_writer.Write("bool4*", atomic);
					break;
				case HLSLBaseType_Int:
					m_writer.Write("%sint*", atomic);
					break;
				case HLSLBaseType_Int2:
					m_writer.Write("%sint2*", atomic);
					break;
				case HLSLBaseType_Int3:
					m_writer.Write("%sint3*", atomic);
					break;
				case HLSLBaseType_Int4:
					m_writer.Write("%sint4*", atomic);
					break;
				case HLSLBaseType_Uint:
					m_writer.Write("%suint*", atomic);
					break;
				case HLSLBaseType_Uint2:
					m_writer.Write("%suint2*", atomic);
					break;
				case HLSLBaseType_Uint3:
					m_writer.Write("%suint3*", atomic);
					break;
				case HLSLBaseType_Uint4:
					m_writer.Write("%suint4*", atomic);
					break;
				default:
					break;
				}

				m_writer.Write(" %s", buffer->name);
				m_writer.EndLine(";");
			}
			else if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer)
			{

				HLSLDeclaration* field = buffer->field;

				m_writer.Write(indent, "device ");

				if (buffer->type.elementType)
				{
					m_writer.Write("%s*", getElementTypeAsStr(buffer->type));
				}
				else
				{
					const char* atomic = "";

					if (buffer->bAtomic)
						atomic = "atomic_";

					switch (buffer->type.elementType)
					{
					case HLSLBaseType_Float:
						m_writer.Write("%sfloat*", atomic);
						break;
					case HLSLBaseType_Float2:
						m_writer.Write("%sfloat2*", atomic);
						break;
					case HLSLBaseType_Float3:
						m_writer.Write("%sfloat3*", atomic);
						break;
					case HLSLBaseType_Float4:
						m_writer.Write("%sfloat4*", atomic);
						break;
					case HLSLBaseType_Bool:
						m_writer.Write("%sbool*", atomic);
						break;
					case HLSLBaseType_Bool2:
						m_writer.Write("%sbool2*", atomic);
						break;
					case HLSLBaseType_Bool3:
						m_writer.Write("%sbool3*", atomic);
						break;
					case HLSLBaseType_Bool4:
						m_writer.Write("%sbool4*", atomic);
						break;
					case HLSLBaseType_Int:
						m_writer.Write("%sint*", atomic);
						break;
					case HLSLBaseType_Int2:
						m_writer.Write("%sint2*", atomic);
						break;
					case HLSLBaseType_Int3:
						m_writer.Write("%sint3*", atomic);
						break;
					case HLSLBaseType_Int4:
						m_writer.Write("%sint4*", atomic);
						break;
					case HLSLBaseType_Uint:
						m_writer.Write("%suint*", atomic);
						break;
					case HLSLBaseType_Uint2:
						m_writer.Write("%suint2*", atomic);
						break;
					case HLSLBaseType_Uint3:
						m_writer.Write("%suint3*", atomic);
						break;
					case HLSLBaseType_Uint4:
						m_writer.Write("%suint4*", atomic);
						break;
					default:
						break;
					}
				}				

				m_writer.Write(" %s", buffer->name);
				m_writer.EndLine(";");
			}
			else if (buffer->type.baseType >= HLSLBaseType_ByteAddressBuffer || buffer->type.baseType <= HLSLBaseType_RWByteAddressBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				m_writer.WriteLine(indent, "constant %s* %s;", "uint", buffer->name);
			}
			else if (buffer->field->type.baseType >= HLSLBaseType_Texture1D && buffer->field->type.baseType <= HLSLBaseType_RWTexture3D)
			{
				OutputBuffer(indent, buffer);
			}
			
		}
		else if (statement->nodeType == HLSLNodeType_Function)
		{
			HLSLFunction* function = static_cast<HLSLFunction*>(statement);

			if (!function->forward)
			{
				OutputFunction(indent, function);
			}
		}
		else if (statement->nodeType == HLSLNodeType_ExpressionStatement)
		{
			HLSLExpressionStatement* expressionStatement = static_cast<HLSLExpressionStatement*>(statement);

			/*
			// IC: This works, but it only helps in very few scenarios. We need a more general solution that involves more complex syntax tree transformations.
			if (expressionStatement->expression->nodeType == HLSLNodeType_FunctionCall)
			{
			OutputFunctionCallStatement(indent, (HLSLFunctionCall*)expressionStatement->expression);
			}
			else
			*/
			{
				m_writer.BeginLine(indent, statement->fileName, statement->line);
				OutputExpression(expressionStatement->expression, NULL, NULL, function, true);
				m_writer.EndLine(";");
			}
		}
		else if (statement->nodeType == HLSLNodeType_ReturnStatement)
		{
			HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
			if (returnStatement->expression != NULL)
			{
				//m_writer.BeginLine(indent, returnStatement->fileName, returnStatement->line);
				m_writer.Write(indent, "return ");
				OutputExpression(returnStatement->expression, NULL, NULL, function, true);
				m_writer.EndLine(";");
			}
			else
			{
				//m_writer.WriteLineTagged(indent, returnStatement->fileName, returnStatement->line, "return;");
				m_writer.WriteLine(indent, "return;");
			}
		}
		else if (statement->nodeType == HLSLNodeType_DiscardStatement)
		{
			HLSLDiscardStatement* discardStatement = static_cast<HLSLDiscardStatement*>(statement);
			m_writer.WriteLineTagged(indent, discardStatement->fileName, discardStatement->line, "discard_fragment();");
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
			m_writer.BeginLine(indent, ifStatement->fileName, ifStatement->line);
			m_writer.Write("if (");
			OutputExpression(ifStatement->condition, NULL, NULL, function, true);
			m_writer.Write(")");
			m_writer.EndLine();


			if (ifStatement->statement != NULL)
			{
				if (ifStatement->statement->nodeType == HLSLNodeType_Preprocessor)
				{
					OutputStatements(indent + 1, ifStatement->statement, function);
				}
				else
				{
					m_writer.WriteLine(indent, "{");
					OutputStatements(indent + 1, ifStatement->statement, function);
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
				OutputExpression(ifStatement->elseifStatement[i]->condition, NULL, NULL, function, true);
				m_writer.Write(")");
				m_writer.EndLine();
				m_writer.WriteLine(indent, "{");
				OutputStatements(indent + 1, ifStatement->elseifStatement[i]->statement, function);
				m_writer.WriteLine(indent, "}");
			}

			if (ifStatement->elseStatement != NULL)
			{
				m_writer.WriteLine(indent, "else");
				m_writer.WriteLine(indent, "{");
				OutputStatements(indent + 1, ifStatement->elseStatement, function);
				m_writer.WriteLine(indent, "}");
			}
		}
		else if (statement->nodeType == HLSLNodeType_SwitchStatement)
		{
			HLSLSwitchStatement* switchStatement = static_cast<HLSLSwitchStatement*>(statement);

			m_writer.Write(indent, "switch (");
			OutputExpression(switchStatement->condition, NULL, NULL, function, false);
			m_writer.Write(")\n");

			m_writer.WriteLine(indent, "{");

			//print cases
			for (int i = 0; i< switchStatement->caseCounter; i++)
			{
				m_writer.Write(indent + 1, "case ");


				OutputExpression(switchStatement->caseNumber[i], NULL, NULL, function, false);

				m_writer.Write(":\n");

				m_writer.WriteLine(indent + 1, "{");
				OutputStatements(indent + 2, switchStatement->caseStatement[i], function);
				m_writer.WriteLine(indent + 1, "}");
			}

			//print default

			m_writer.Write(indent + 1, "default:\n");
			m_writer.WriteLine(indent + 1, "{");
			OutputStatements(indent + 2, switchStatement->caseDefault, function);
			m_writer.WriteLine(indent + 1, "}");

			m_writer.WriteLine(indent, "}");
		}
		else if (statement->nodeType == HLSLNodeType_ForStatement)
		{
			HLSLForStatement* forStatement = static_cast<HLSLForStatement*>(statement);
			m_writer.BeginLine(indent, forStatement->fileName, forStatement->line);
			m_writer.Write("for (");
			
			if (forStatement->initialization)
				OutputDeclaration(forStatement->initialization, function);
			else if (forStatement->initializationWithoutDeclaration)
				OutputExpression(forStatement->initializationWithoutDeclaration, NULL, NULL, function, true);



			m_writer.Write("; ");
			OutputExpression(forStatement->condition, NULL, NULL, function, true);
			m_writer.Write("; ");
			OutputExpression(forStatement->increment, NULL, NULL, function, true);
			m_writer.Write(")");
			m_writer.EndLine();

			m_writer.WriteLine(indent, "{");
			OutputStatements(indent + 1, forStatement->statement, function);
			m_writer.WriteLine(indent, "}");
		}
		else if (statement->nodeType == HLSLNodeType_WhileStatement)
		{
			HLSLWhileStatement* whileStatement = static_cast<HLSLWhileStatement*>(statement);

			m_writer.BeginLine(indent, whileStatement->fileName, whileStatement->line);
			m_writer.Write("while (");

			OutputExpression(whileStatement->condition, NULL, NULL, function, true);
			m_writer.Write(") {");
			m_writer.EndLine();
			OutputStatements(indent + 1, whileStatement->statement, function);
			m_writer.WriteLine(indent, "}");
		}
		else if (statement->nodeType == HLSLNodeType_BlockStatement)
		{
			HLSLBlockStatement* blockStatement = static_cast<HLSLBlockStatement*>(statement);
			m_writer.WriteLineTagged(indent, blockStatement->fileName, blockStatement->line, "{");
			OutputStatements(indent + 1, blockStatement->statement, function);
			m_writer.WriteLine(indent, "}");
		}
		else if (statement->nodeType == HLSLNodeType_Technique)
		{
			// Techniques are ignored.
		}
		else if (statement->nodeType == HLSLNodeType_Pipeline)
		{
			// Pipelines are ignored.
		}
		else
		{
			// Unhandled statement type.
			ASSERT(0);
		}

		statement = statement->nextStatement;
	}
}

// Called by OutputStatements

void MSLGenerator::OutputAttributes(int indent, HLSLAttribute* attribute, bool bMain)
{
	// IC: These do not appear to exist in MSL.
	while (attribute != NULL) {
		if (attribute->attributeType == HLSLAttributeType_Unroll)
		{
			// @@ Do any of these work?
			//m_writer.WriteLine(indent, attribute->fileName, attribute->line, "#pragma unroll");
			//m_writer.WriteLine(indent, attribute->fileName, attribute->line, "[[unroll]]");
		}
		else if (attribute->attributeType == HLSLAttributeType_Flatten)
		{
			// @@
		}
		else if (attribute->attributeType == HLSLAttributeType_Branch)
		{
			// @@
		}
		else if (attribute->attributeType == HLSLAttributeType_NumThreads && m_target == Target_ComputeShader && bMain)
		{

			m_writer.Write("//[numthreads(");

			if (attribute->numGroupX != 0)
				m_writer.Write("%d, ", attribute->numGroupX);
			else
				m_writer.Write("%s, ", attribute->numGroupXstr);

			if (attribute->numGroupY != 0)
				m_writer.Write("%d, ", attribute->numGroupY);
			else
				m_writer.Write("%s, ", attribute->numGroupYstr);


			if (attribute->numGroupZ != 0)
				m_writer.Write("%d", attribute->numGroupZ);
			else
				m_writer.Write("%s", attribute->numGroupZstr);

			m_writer.Write(")]");

			//m_writer.Write(indent, "//[numthreads(%d, %d, %d)]", attribute->numGroupX, attribute->numGroupY, attribute->numGroupZ);
		}
		else if (attribute->attributeType == HLSLAttributeType_Domain && m_target == Target_DomainShader && bMain)
		{
			//remove ""
			char tempDomain[16];
			strcpy(tempDomain, &((attribute->domain)[1]));

			tempDomain[strlen(attribute->domain) - 2] = NULL;

			m_writer.Write(indent, "[[%s(%s, %d)]]", attribute->patchIdentifier, tempDomain, attribute->maxVertexCount);
		}

		attribute = attribute->nextAttribute;
	}
}

void MSLGenerator::OutputDeclaration(HLSLDeclaration* declaration, const HLSLFunction* function)
{
	if (IsSamplerType(declaration->type))
	{
		// Declare a texture and a sampler instead
		// We do not handle multiple textures on the same line
		ASSERT(declaration->nextDeclaration == NULL);
		if (declaration->type.baseType == HLSLBaseType_Sampler2D)
			m_writer.Write("thread texture2d<float>& %s_texture; thread sampler& %s_sampler", declaration->name, declaration->name);
		else if (declaration->type.baseType == HLSLBaseType_Sampler3D)
			m_writer.Write("thread texture3d<float>& %s_texture; thread sampler& %s_sampler", declaration->name, declaration->name);
		else if (declaration->type.baseType == HLSLBaseType_SamplerCube)
			m_writer.Write("thread texturecube<float>& %s_texture; thread sampler& %s_sampler", declaration->name, declaration->name);
		else if (declaration->type.baseType == HLSLBaseType_Sampler2DShadow)
			m_writer.Write("thread depth2d<float>& %s_texture; thread sampler& %s_sampler", declaration->name, declaration->name);
		else if (declaration->type.baseType == HLSLBaseType_Sampler2DMS)
			m_writer.Write("thread texture2d_ms<float>& %s_texture", declaration->name);
		else if (declaration->type.baseType == HLSLBaseType_Sampler2DArray)
			m_writer.Write("thread texture2d_array<float>& %s_texture; thread sampler& %s_sampler", declaration->name, declaration->name);
		else
			m_writer.Write("<unhandled texture type>");
	}
	else
	{
		OutputDeclaration(declaration->type, declaration->name, declaration->assignment, function);
		declaration = declaration->nextDeclaration;
		while (declaration != NULL)
		{
			m_writer.Write(",");
			OutputDeclarationBody(declaration->type, declaration->name, declaration->assignment, function);
			declaration = declaration->nextDeclaration;
		};
	}
}

void MSLGenerator::OutputStruct(int indent, HLSLStruct* structure)
{
	m_writer.WriteLineTagged(indent, structure->fileName, structure->line, "struct %s", structure->name);
	m_writer.WriteLine(indent, "{");

	HLSLStructField* field = structure->field;
	while (field != NULL)
	{
		if (!field->hidden)
		{
			m_writer.BeginLine(indent + 1, field->fileName, field->line);

			if (field->atomic)
				m_writer.Write("atomic_");

			if (String_Equal(field->semantic, "SV_TessFactor") || String_Equal(field->semantic, "SV_InsideTessFactor"))
			{
				m_writer.Write("array<");
				m_writer.Write("%s, ", "half");
				//m_writer.Write("%d", OutputExpression(field));
				OutputExpression(field->type.arraySize, NULL, NULL, NULL, true);

				m_writer.Write(">");

				// Then name
				m_writer.Write(" %s", field->name);
			}
			else
			{
				if (field->sv_semantic && strncmp(field->sv_semantic, "color", 5) == 0)
				{
					field->type.baseType = HLSLBaseType_Float4;
					OutputDeclaration(field->type, field->name, NULL, NULL);
				}
				else
				{
					if (field->preProcessor)
					{
						if (field->preProcessor->expression)
							OutputExpression(field->preProcessor->expression, NULL, NULL, NULL, false);
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

						m_writer.EndLine();
						field = field->nextField;
						continue;
					}
					else
						OutputDeclaration(field->type, field->name, NULL, NULL);
				}
				
			}

			if (field->sv_semantic)
			{
				m_writer.Write(" [[%s]]", field->sv_semantic);
			}

			m_writer.Write(";");
			m_writer.EndLine();
		}
		field = field->nextField;
	}
	m_writer.WriteLine(indent, "};");
}

void MSLGenerator::OutputBuffer(int indent, HLSLBuffer* buffer)
{
	HLSLDeclaration* field = buffer->field;

	//m_writer.BeginLine(indent, buffer->fileName, buffer->line);
	m_writer.WriteLine(indent, "struct Uniforms_%s", buffer->name);
	m_writer.WriteLine(indent, "{");



	if (buffer->nodeType == HLSLNodeType_Buffer)
	{
		HLSLBuffer* cBuffer = (HLSLBuffer*)buffer;

		if (cBuffer->type.elementType)
		{
			HLSLStruct* pStruct = m_tree->FindGlobalStruct(cBuffer->userDefinedElementTypeStr);
			HLSLStructField* field = pStruct->field;

			while (field)
			{
				if (!field->hidden)
				{
					m_writer.BeginLine(indent + 1, field->fileName, field->line);
					OutputDeclaration(field->type, field->name, NULL, NULL);
					m_writer.Write(";");
					m_writer.EndLine();
				}
				field = field->nextField;
			}
		}

	}

	while (field != NULL)
	{
		if (!field->hidden)
		{
			m_writer.BeginLine(indent + 1, field->fileName, field->line);
			OutputDeclaration(field->type, field->name, field->assignment, NULL, false, false, /*alignment=*/0);
			m_writer.EndLine(";");
		}
		field = (HLSLDeclaration*)field->nextStatement;
	}
	m_writer.WriteLine(indent, "};");

	m_writer.WriteLine(indent, "constant Uniforms_%s & %s;", buffer->name, buffer->name);
}

void MSLGenerator::OutputFunction(int indent, const HLSLFunction* function)
{
	const char* functionName = function->name;
	const char* returnTypeName = GetTypeName(function->returnType);

	m_writer.BeginLine(indent, function->fileName, function->line);
	m_writer.Write("%s %s(", returnTypeName, functionName);

	OutputArguments(function->argument, function);

	m_writer.EndLine(")");
	m_writer.WriteLine(indent, "{");
	OutputStatements(indent + 1, function->statement, function);
	m_writer.WriteLine(indent, "};");
}


// @@ We could be a lot smarter removing parenthesis based on the operator precedence of the parent expression.
static bool NeedsParenthesis(HLSLExpression* expression, HLSLExpression* parentExpression) {

	// For now we just omit the parenthesis if there's no parent expression.
	if (parentExpression == NULL)
	{
		return false;
	}

	// One more special case that's pretty common.
	if (parentExpression->nodeType == HLSLNodeType_MemberAccess)
	{
		if (expression->nodeType == HLSLNodeType_IdentifierExpression ||
			expression->nodeType == HLSLNodeType_ArrayAccess ||
			expression->nodeType == HLSLNodeType_MemberAccess)
		{
			return false;
		}
	}

	return true;
}

static bool GetCanImplicitCast(const HLSLType& srcType, const HLSLType& dstType)
{
	return srcType.baseType == dstType.baseType;
}

static const HLSLType* commonScalarType(const HLSLType& lhs, const HLSLType& rhs)
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

void MSLGenerator::OutputExpression(HLSLExpression* expression, const HLSLType* dstType, HLSLExpression* parentExpression, const HLSLFunction* function, bool needsEndParen)
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


	if (expression->childExpression)
	{
		m_writer.Write("{");
		OutputExpressionList(expression->childExpression, function);
		m_writer.Write("}");
	}
	else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
	{
		HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
		const char* name = identifierExpression->name;
		// For texture declaration, generate a struct instead
		if (identifierExpression->global && IsSamplerType(identifierExpression->expressionType))
		{
			if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2D)
				m_writer.Write("Texture2DSampler(%s_texture, %s_sampler)", name, name);
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler3D)
				m_writer.Write("Texture3DSampler(%s_texture, %s_sampler)", name, name);
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_SamplerCube)
				m_writer.Write("TextureCubeSampler(%s_texture, %s_sampler)", name, name);
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DShadow)
				m_writer.Write("Texture2DShadowSampler(%s_texture, %s_sampler)", name, name);
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DMS)
				m_writer.Write("%s_texture", name);
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DArray)
				m_writer.Write("Texture2DArraySampler(%s_texture, %s_sampler)", name, name);
			else
				m_writer.Write("<unhandled texture type>");
		}	
		//add buffer's name
		else if (function != NULL && !matchFunctionArgumentsIdentifiers(function->argument, identifierExpression->name) && m_tree->FindBuffertMember(identifierExpression->name))
		{
			m_writer.Write("%s.%s", m_tree->FindBuffertMember(identifierExpression->name), identifierExpression->name);
		}
		else if (expression->expressionType.baseType == HLSLBaseType_OutputPatch)
		{
			m_writer.Write("%s.control_points", identifierExpression->name);
		}
		else if (expression->expressionType.baseType == HLSLBaseType_ByteAddressBuffer || expression->expressionType.baseType == HLSLBaseType_RWByteAddressBuffer)
		{
			HLSLFunctionCall* funct = (HLSLFunctionCall*)expression;

			if (funct != NULL && funct->functionExpression)
			{
				OutputExpression(funct->functionExpression, dstType, parentExpression, function, needsEndParen);
			}

		
		}
		else
		{
			if (identifierExpression->global)
			{
				HLSLBuffer * buffer;
				HLSLDeclaration * declaration = m_tree->FindGlobalDeclaration(identifierExpression->name, &buffer);

				if (declaration && declaration->buffer)
				{
					ASSERT(buffer == declaration->buffer);
					m_writer.Write("%s.", declaration->buffer->name);
				}
			}


			// if it is one of PushConstantBuffer's data's Name
			for (int index = 0; index < m_PushConstantBufferCounter; index++)
			{
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


			m_writer.Write("%s", name);
		}
	}
	else if (expression->nodeType == HLSLNodeType_CastingExpression)
	{
		HLSLCastingExpression* castingExpression = static_cast<HLSLCastingExpression*>(expression);
		OutputCast(castingExpression->type);
		m_writer.Write("(");
		OutputExpression(castingExpression->expression, NULL, castingExpression, function, false);
		m_writer.Write(")");
	}
	else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
	{
		HLSLConstructorExpression* constructorExpression = static_cast<HLSLConstructorExpression*>(expression);
		m_writer.Write("%s(", GetTypeName(constructorExpression->type));
		OutputExpressionList(constructorExpression->argument, function);
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
			ASSERT(-1);
		}
	}
	else if (expression->nodeType == HLSLNodeType_UnaryExpression)
	{
		HLSLUnaryExpression* unaryExpression = static_cast<HLSLUnaryExpression*>(expression);
		const char* op = "?";
		bool pre = true;
		switch (unaryExpression->unaryOp)
		{
		case HLSLUnaryOp_Negative:      op = "-";  break;
		case HLSLUnaryOp_Positive:      op = "+";  break;
		case HLSLUnaryOp_Not:           op = "!";  break;
		case HLSLUnaryOp_BitNot:        op = "~";  break;
		case HLSLUnaryOp_PreIncrement:  op = "++"; break;
		case HLSLUnaryOp_PreDecrement:  op = "--"; break;
		case HLSLUnaryOp_PostIncrement: op = "++"; pre = false; break;
		case HLSLUnaryOp_PostDecrement: op = "--"; pre = false; break;
		}
		bool addParenthesis = NeedsParenthesis(unaryExpression->expression, expression);
		if (addParenthesis) m_writer.Write("(");
		if (pre)
		{
			m_writer.Write("%s", op);
			OutputExpression(unaryExpression->expression, NULL, unaryExpression, function, needsEndParen);
		}
		else
		{
			OutputExpression(unaryExpression->expression, NULL, unaryExpression, function, needsEndParen);
			m_writer.Write("%s", op);
		}
		if (addParenthesis) m_writer.Write(")");
	}
	else if (expression->nodeType == HLSLNodeType_BinaryExpression)
	{
		HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(expression);

		//bool addParenthesis = NeedsParenthesis(expression, parentExpression);
		//if (addParenthesis) m_writer.Write("(");

		if (needsEndParen)
			m_writer.Write("(");

		bool rewrite_assign = false;
		if (binaryExpression->binaryOp == HLSLBinaryOp_Assign && binaryExpression->expression1->nodeType == HLSLNodeType_ArrayAccess)
		{
			HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(binaryExpression->expression1);
			if (!arrayAccess->array->expressionType.array && IsMatrixType(arrayAccess->array->expressionType.baseType))
			{
				rewrite_assign = true;

				m_writer.Write("set_column(");
				OutputExpression(arrayAccess->array, NULL, NULL, function, needsEndParen);
				m_writer.Write(", ");
				OutputExpression(arrayAccess->index, NULL, NULL, function, needsEndParen);
				m_writer.Write(", ");
				OutputExpression(binaryExpression->expression2, NULL, NULL, function, needsEndParen);
				m_writer.Write(")");
			}
		}

		if (!rewrite_assign)
		{

			const char* op = "?";
			const HLSLType* dstType1 = NULL;
			const HLSLType* dstType2 = NULL;

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
			case HLSLBinaryOp_And:          op = " && "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_Or:           op = " || "; dstType1 = dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_BitAnd:       op = " & "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_BitOr:		op = " | "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_BitXor:		op = " ^ "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_BitAndAssign: op = " &= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_BitOrAssign:  op = " |= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_BitXorAssign: op = " ^= "; dstType2 = &binaryExpression->expressionType; break;
			case HLSLBinaryOp_LeftShift:    op = " << "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_RightShift:   op = " >> "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			case HLSLBinaryOp_Modular:      op = " % "; dstType1 = dstType2 = commonScalarType(binaryExpression->expression1->expressionType, binaryExpression->expression2->expressionType); break;
			default:
				ASSERT(-1);
			}

			// Exception Handling for imageStore, this might make errors
			// Need to change better form
			HLSLArrayAccess* ArrayAcess = static_cast<HLSLArrayAccess*>(binaryExpression->expression1);

			//if (binaryExpression->binaryOp == HLSLBinaryOp_Assign && ArrayAcess->array != NULL && ArrayAcess->array->nodeType == HLSLNodeType_TextureStateExpression)
			if (binaryExpression->binaryOp == HLSLBinaryOp_Assign && binaryExpression->expression1->expressionType.baseType >= HLSLBaseType_RWTexture1D && binaryExpression->expression1->expressionType.baseType <= HLSLBaseType_RWTexture3D)
			{
				//HLSLTextureStateExpression* rwTextureStateExpression = static_cast<HLSLTextureStateExpression*>(ArrayAcess->array);

				HLSLTextureStateExpression* textureStateExpression = static_cast<HLSLTextureStateExpression*>(binaryExpression->expression1);

				if (textureStateExpression->expressionType.baseType >= HLSLBaseType_RWTexture1D && textureStateExpression->expressionType.baseType <= HLSLBaseType_RWTexture3D)
				{
					m_writer.Write("%s", textureStateExpression->name);

					if (textureStateExpression->bArray)
					{
						for (int i = 0; i < (int)textureStateExpression->arrayDimension; i++)
						{
							if (textureStateExpression->arrayExpression)
							{
								m_writer.Write("[");
								OutputExpressionList(textureStateExpression->arrayExpression, function);
								m_writer.Write("]");
							}
							else if (textureStateExpression->arrayIndex[i] > 0)
								m_writer.Write("[%u]", textureStateExpression->arrayIndex[i]);
							else
								m_writer.Write("[%d]", DEFAULT_TEXTURE_COUNT);
						}
					}

					m_writer.Write(".write(");

					OutputExpression(binaryExpression->expression2, NULL, binaryExpression, function, true);

					switch (textureStateExpression->expressionType.baseType)
					{
					case HLSLBaseType_RWTexture1D:
						m_writer.Write(", uint(");
						break;
					case HLSLBaseType_RWTexture1DArray:
						m_writer.Write(", uint2(");
						break;
					case HLSLBaseType_RWTexture2D:
						m_writer.Write(", uint2(");
						break;
					case HLSLBaseType_RWTexture2DArray:
						m_writer.Write(", uint3(");
						break;
					case HLSLBaseType_RWTexture3D:
						m_writer.Write(", uint3(");
						break;
					default:
						break;
					}

					//OutputExpression(ArrayAcess->index, NULL, NULL, function);

					if (textureStateExpression->indexExpression)
					{
						OutputExpression(textureStateExpression->indexExpression, NULL, NULL, function, true);
					}

					
					switch (textureStateExpression->expressionType.baseType)
					{
					case HLSLBaseType_RWTexture1D:
						m_writer.Write(")");
						break;
					case HLSLBaseType_RWTexture1DArray:
						m_writer.Write(").x, uint2(");
						OutputExpression(textureStateExpression->indexExpression, NULL, NULL, function, true);
						m_writer.Write(").y");
						break;
					case HLSLBaseType_RWTexture2D:
						m_writer.Write(")");
						break;
					case HLSLBaseType_RWTexture2DArray:
						m_writer.Write(").xy, uint3(");
						OutputExpression(textureStateExpression->indexExpression, NULL, NULL, function, true);
						m_writer.Write(").z");
						break;
					case HLSLBaseType_RWTexture3D:
						m_writer.Write(")");
						break;
					default:
						break;
					}


					m_writer.Write(")");
				}
				else
				{
					OutputExpression(binaryExpression->expression1, dstType1, binaryExpression, function, true);
					m_writer.Write("%s", op);
					OutputExpression(binaryExpression->expression2, dstType2, binaryExpression, function, true);
				}
			}
			else
			{
				OutputExpression(binaryExpression->expression1, dstType1, binaryExpression, function, needsEndParen);

				m_writer.Write("%s", op);
				OutputExpression(binaryExpression->expression2, dstType2, binaryExpression, function, true);
			}
		}

		//if (addParenthesis) m_writer.Write(")");

		if (needsEndParen)
			m_writer.Write(")");
	}
	else if (expression->nodeType == HLSLNodeType_ConditionalExpression)
	{
		HLSLConditionalExpression* conditionalExpression = static_cast<HLSLConditionalExpression*>(expression);
		// @@ Remove parenthesis.
		m_writer.Write("((");
		OutputExpression(conditionalExpression->condition, NULL, NULL, function, true);
		m_writer.Write(")?(");
		OutputExpression(conditionalExpression->trueExpression, NULL, NULL, function, true);
		m_writer.Write("):(");
		OutputExpression(conditionalExpression->falseExpression, NULL, NULL, function, true);
		m_writer.Write("))");
	}
	else if (expression->nodeType == HLSLNodeType_MemberAccess)
	{
		HLSLMemberAccess* memberAccess = static_cast<HLSLMemberAccess*>(expression);
		//bool addParenthesis = NeedsParenthesis(memberAccess->object, expression);
		bool addParenthesis;
		if (!needsEndParen)
			addParenthesis = false;
		else 
			addParenthesis = true;
		//compare the length of swizzling

		if (memberAccess->swizzle)
		{
			int parentLength = 0;
			switch (memberAccess->object->expressionType.baseType)
			{
			case HLSLBaseType_Float:
			case HLSLBaseType_Half:
			case HLSLBaseType_Bool:
			case HLSLBaseType_Int:
			case HLSLBaseType_Uint:
				parentLength = 1;
				break;

			case HLSLBaseType_Float2:
			case HLSLBaseType_Half2:
			case HLSLBaseType_Bool2:
			case HLSLBaseType_Int2:
			case HLSLBaseType_Uint2:
				parentLength = 2;
				break;

			case HLSLBaseType_Float3:
			case HLSLBaseType_Half3:
			case HLSLBaseType_Bool3:
			case HLSLBaseType_Int3:
			case HLSLBaseType_Uint3:
				parentLength = 3;
				break;

			case HLSLBaseType_Float4:
			case HLSLBaseType_Half4:
			case HLSLBaseType_Bool4:
			case HLSLBaseType_Int4:
			case HLSLBaseType_Uint4:
				parentLength = 4;
				break;
			default:
				break;
			}

			int swizzleLength = (int)strlen(memberAccess->field);

			if (parentLength < swizzleLength)
				m_writer.Write("%s", GetTypeName(expression->expressionType));
		}

		if (addParenthesis)
		{
			m_writer.Write("(");
		}
		OutputExpression(memberAccess->object, NULL, NULL, function, true);
		if (addParenthesis)
		{
			m_writer.Write(")");
		}


		m_writer.Write(".%s", memberAccess->field);
	}
	else if (expression->nodeType == HLSLNodeType_ArrayAccess)
	{
		HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);

		OutputExpression(arrayAccess->array, NULL, expression, function, true);
		m_writer.Write("[");
		OutputExpression(arrayAccess->index, NULL, NULL, function, true);
		m_writer.Write("]");

		/*
		if (arrayAccess->array->expressionType.array || !IsMatrixType(arrayAccess->array->expressionType.baseType))
		{
		OutputExpression(arrayAccess->array, expression);
		m_writer.Write("[");
		OutputExpression(arrayAccess->index, NULL);
		m_writer.Write("]");
		}
		else
		{

		// @@ This doesn't work for l-values!

		m_writer.Write("column(");
		OutputExpression(arrayAccess->array, NULL);
		m_writer.Write(", ");
		OutputExpression(arrayAccess->index, NULL);
		m_writer.Write(")");

		}
		*/
	}
	else if (expression->nodeType == HLSLNodeType_FunctionCall)
	{
		HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(expression);
		const char* name = functionCall->function->name;


		if (String_Equal(name, "mul"))
		{
			HLSLExpression* argument[2];
			if (GetFunctionArguments(functionCall, argument, 2) != 2)
			{
				Error("mul expects 2 arguments");
				return;
			}

			const HLSLType& type0 = functionCall->function->argument->type;
			const HLSLType& type1 = functionCall->function->argument->nextArgument->type;

			const char* prefix = "";
			const char* infix = "*";

			if (m_options.flags & Flag_PackMatrixRowMajor)
			{
				m_writer.Write("%s((", prefix);
				OutputExpression(argument[1], NULL, NULL, function, true);
				m_writer.Write(")%s(", infix);
				OutputExpression(argument[0], NULL, NULL, function, true);
				m_writer.Write("))");
			}
			else
			{
				m_writer.Write("%s((", prefix);
				OutputExpression(argument[0], NULL, NULL, function, true);
				m_writer.Write(")%s(", infix);
				OutputExpression(argument[1], NULL, NULL, function, true);
				m_writer.Write("))");
			}
		}
		else if (String_Equal(name, "Sample") || String_Equal(name, "SampleLevel") || String_Equal(name, "SampleBias") ||
			String_Equal(name, "GatherRed") || String_Equal(name, "SampleGrad"))
		{
			if (functionCall->pTextureStateExpression)
			{
				const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;

				//m_writer.Write(" %s", pTextureStateExpression->name);


				if (pTextureStateExpression->bArray)
				{
					for (int i = 0; i < (int)pTextureStateExpression->arrayDimension; i++)
					{
						if (pTextureStateExpression->arrayExpression)
						{
							m_writer.Write(".Textures[");
							OutputExpressionList(pTextureStateExpression->arrayExpression, function);
							m_writer.Write("]");
						}
						else if (pTextureStateExpression->arrayIndex[i] > 0)
						{
							m_writer.Write(".Textures[%u]", pTextureStateExpression->arrayIndex[i]);
						}
						else
						{
							m_writer.Write(".Textures[%d]", DEFAULT_TEXTURE_COUNT);
						}
					}
				}
			}

			if (String_Equal(name, "Sample"))
				m_writer.Write(".%s(", "sample");
			else if (String_Equal(name, "SampleLevel"))
				m_writer.Write(".%s(", "sample");
			else if (String_Equal(name, "SampleBias"))
				m_writer.Write(".%s(", "sample");
			else if (String_Equal(name, "GatherRed"))
				m_writer.Write(".%s(", "sample");
			else if (String_Equal(name, "SampleGrad"))
				m_writer.Write(".%s(", "gather");

			HLSLExpression* firstExpression = functionCall->argument;

			//should be sampler
			OutputExpression(firstExpression, NULL, NULL, function, true);

			m_writer.Write(", ");

			HLSLExpression* SecondExpression = firstExpression->nextExpression;
			OutputExpression(SecondExpression, NULL, NULL, function, true);

			if (functionCall->pTextureStateExpression->expressionType.baseType == HLSLBaseType_Texture1DArray ||
				functionCall->pTextureStateExpression->expressionType.baseType == HLSLBaseType_Texture2DArray ||
				functionCall->pTextureStateExpression->expressionType.baseType == HLSLBaseType_Texture2DMSArray ||
				functionCall->pTextureStateExpression->expressionType.baseType == HLSLBaseType_TextureCubeArray)
			{
				switch (functionCall->pTextureStateExpression->expressionType.baseType)
				{
				case HLSLBaseType_Texture1DArray:
					m_writer.Write(".x, ");
					break;
				case HLSLBaseType_Texture2DArray:
					m_writer.Write(".xy, ");
					break;
				case HLSLBaseType_Texture2DMSArray:
					m_writer.Write(".xy, ");
					break;
				case HLSLBaseType_TextureCubeArray:
					m_writer.Write(".xyz, ");
					break;
				default:
					break;
				}

				///Print again
				OutputExpression(SecondExpression, NULL, NULL, function, true);

				switch (functionCall->pTextureStateExpression->expressionType.baseType)
				{
				case HLSLBaseType_Texture1DArray:
					m_writer.Write(".y");
					break;
				case HLSLBaseType_Texture2DArray:
					m_writer.Write(".z");
					break;
				case HLSLBaseType_Texture2DMSArray:
					m_writer.Write(".z");
					break;
				case HLSLBaseType_TextureCubeArray:
					m_writer.Write(".w");
					break;
				default:
					break;
				}
			}
			
			//This is for SampleLevel
			HLSLExpression* ThirdExpression = SecondExpression->nextExpression;
			if (ThirdExpression)
			{
				m_writer.Write(", level(");
				OutputExpression(ThirdExpression, NULL, NULL, function, true);
				m_writer.Write(")");
			}

			m_writer.Write(")");
		}
		else if (String_Equal(name, "Load"))
		{
			//m_writer.Write(" %s.", functionCall->pTextureStateExpression->name);

			//for Texture
			if (functionCall->pTextureStateExpression)
			{
				switch (functionCall->pTextureStateExpression->expressionType.baseType)
				{
				case HLSLBaseType_Texture1D:
					m_writer.Write(".read(");
					break;
				case HLSLBaseType_Texture1DArray:
					m_writer.Write(".read(uint2(");
					break;
				case HLSLBaseType_Texture2D:
					m_writer.Write(".read(");
					break;
				case HLSLBaseType_Texture2DArray:
					m_writer.Write(".read(uint3(");
					break;
				case HLSLBaseType_Texture3D:
					m_writer.Write(".read(");
					break;
				case HLSLBaseType_Texture2DMS:
					m_writer.Write(".read(");
					break;
				case HLSLBaseType_Texture2DMSArray:
					m_writer.Write(".read(uint3(");
					break;
				case HLSLBaseType_TextureCube:
					m_writer.Write("<metal use uint face for choosing the face of cubemap>");
					break;
				case HLSLBaseType_TextureCubeArray:
					m_writer.Write("<metal use uint face for choosing the face of cubemap>");
					break;
				default:
					break;
				}

				OutputExpressionList(functionCall->argument, function);

				//HLSL -> MSL
				switch (functionCall->pTextureStateExpression->expressionType.baseType)
				{
				case HLSLBaseType_Texture1D:
					m_writer.Write(".x");
					break;
				case HLSLBaseType_Texture1DArray:
					m_writer.Write(").x, uint2(");
					OutputExpressionList(functionCall->argument, function);
					m_writer.Write(").y");
					break;
				case HLSLBaseType_Texture2D:
					m_writer.Write(".xy");
					break;
				case HLSLBaseType_Texture2DArray:
					m_writer.Write(").xy, uint3(");
					OutputExpressionList(functionCall->argument, function);
					m_writer.Write(").z");
					break;
				case HLSLBaseType_Texture3D:
					m_writer.Write(".xyz");
					break;
				case HLSLBaseType_Texture2DMS:
					//m_writer.Write(".xy");
					break;
				case HLSLBaseType_Texture2DMSArray:
					//m_writer.Write(").xy, uint3(");
					//OutputExpressionList(functionCall->argument, function);
					//m_writer.Write(").z");
					break;
				case HLSLBaseType_TextureCube:
					m_writer.Write("");
					break;
				case HLSLBaseType_TextureCubeArray:
					m_writer.Write("");
					break;
				default:
					break;
				}

				m_writer.Write(")");
			}
			else
			{
				//for buffers
				m_writer.Write("[");
				OutputExpressionList(functionCall->argument, function);
				m_writer.Write("]");
			}
		}
		else if (String_Equal(name, "Store"))
		{
			if (functionCall->pBuffer)
			{

				m_writer.Write("%s", functionCall->pBuffer->name);

				m_writer.Write("[");

				OutputExpression(functionCall->argument, NULL, NULL, function, false);

				m_writer.Write("]");

				m_writer.Write(" = ");

				OutputExpression(functionCall->argument->nextExpression, NULL, NULL, function, false);
			}
		}
		else if (String_Equal(name, "InterlockedAdd") ||
			String_Equal(name, "InterlockedAnd") ||
			String_Equal(name, "InterlockedCompareExchange") ||
			String_Equal(name, "InterlockedExchange") ||
			String_Equal(name, "InterlockedMax") ||
			String_Equal(name, "InterlockedMin") ||
			String_Equal(name, "InterlockedOr") ||
			String_Equal(name, "InterlockedXor"))
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

			if (numArguments == 3)
			{
				OutputExpression(expression->nextExpression->nextExpression, NULL, NULL, function, true);
				m_writer.Write(" = ");
			}

			/*
			HLSLType* expectedType = NULL;
			if (argument != NULL)
			expectedType = &argument->nextArgument->nextArgument->type;
			*/

			//OutputExpression(expression->nextExpression->nextExpression, expectedType);
			//m_writer.Write(" = ");


			if (String_Equal(name, "InterlockedAdd"))
			{
				m_writer.Write("atomic_fetch_add_explicit(&");
			}
			else if (String_Equal(name, "InterlockedAnd"))
			{
				m_writer.Write("atomic_fetch_and_explicit(&");
			}
			else if (String_Equal(name, "InterlockedCompareExchange"))
			{
				m_writer.Write("atomic_compare_exchange_weak_explicit(&");
			}
			else if (String_Equal(name, "InterlockedExchange"))
			{
				m_writer.Write("atomic_exchange_explicit(&");
			}
			else if (String_Equal(name, "InterlockedMax"))
			{
				m_writer.Write("atomic_fetch_max_explicit(&");
			}
			else if (String_Equal(name, "InterlockedMin"))
			{
				m_writer.Write("atomic_fetch_min_explicit(&");
			}
			else if (String_Equal(name, "InterlockedOr"))
			{
				m_writer.Write("atomic_fetch_or_explicit(&");
			}
			else if (String_Equal(name, "InterlockedXor"))
			{
				m_writer.Write("atomic_fetch_xor_explicit(&");
			}

			int numExpressions2 = 0;

			while (expression != NULL)
			{

				if (numExpressions2 == 2)
				{
					//m_writer.Write(")");
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

				OutputExpression(expression, NULL, NULL, function, true);
				expression = expression->nextExpression;

				numExpressions2++;
			}

			m_writer.Write(", memory_order_relaxed)");

		}
		else if (String_Equal(name, "sincos"))
		{
			HLSLExpression* expression = functionCall->argument;
			HLSLExpression* angleArgument = expression;
			HLSLExpression* sinArgument = expression->nextExpression;
			HLSLExpression* cosArgument = sinArgument->nextExpression;

			OutputExpression(sinArgument, NULL, NULL, functionCall->function, true);

			m_writer.Write(" = ");
			m_writer.Write("sincos(");

			OutputExpression(angleArgument, NULL, NULL, functionCall->function, true);

			m_writer.Write(", ");

			OutputExpression(cosArgument, NULL, NULL, functionCall->function, true);

			m_writer.Write(")");
		}
		//!!!! need to update it later
		else if (String_Equal(name, "GroupMemoryBarrierWithGroupSync"))
		{
			m_writer.Write("simdgroup_barrier(mem_flags::mem_threadgroup)");
		}
		else if (String_Equal(name, "GroupMemoryBarrier"))
		{
			m_writer.Write("threadgroup_barrier(mem_flags::mem_threadgroup)");
		}
		else if (String_Equal(name, "DeviceMemoryBarrierWithGroupSync"))
		{
			m_writer.Write("simdgroup_barrier(mem_flags::mem_device)");
		}
		else if (String_Equal(name, "DeviceMemoryBarrier"))
		{
			m_writer.Write("threadgroup_barrier(mem_flags::mem_device)");
		}
		else if (String_Equal(name, "AllMemoryBarrierWithGroupSync"))
		{
			m_writer.Write("threadgroup_barrier(mem_flags::mem_device)");
		}
		else if (String_Equal(name, "AllMemoryBarrier"))
		{
			m_writer.Write("threadgroup_barrier(mem_flags::mem_device)");
		}
		else
			OutputFunctionCall(functionCall);
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

	

		if (textureStateExpression->indexExpression)
		{						
			m_writer.Write("%s", textureStateExpression->name);
			m_writer.Write(".read(");
			OutputExpression(textureStateExpression->indexExpression, NULL, NULL, function, true);
			m_writer.Write(")");
		}

		if (textureStateExpression->functionExpression)
		{
			if (textureStateExpression->functionExpression->nodeType == HLSLNodeType_FunctionCall)
			{
				HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(textureStateExpression->functionExpression);
				const char* name = functionCall->function->name;

				if (String_Equal(name, "GetDimensions"))
				{
					HLSLExpression* exp = functionCall->argument;

					int argCount = 0;

					while (exp)
					{
						if (argCount > 0)
						{
							m_writer.Write(";\n");
							m_writer.Write(2, "");
						}

						OutputExpression(exp, NULL, NULL, function, true);

						m_writer.Write(" = ");

						const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;

						m_writer.Write("%s", pTextureStateExpression->name);

						if (pTextureStateExpression->bArray)
						{
							for (int i = 0; i < (int)pTextureStateExpression->arrayDimension; i++)
							{
								if (pTextureStateExpression->arrayExpression)
								{
									m_writer.Write("[");
									OutputExpressionList(pTextureStateExpression->arrayExpression, function);
									m_writer.Write("]");
								}
								else if (pTextureStateExpression->arrayIndex[i] > 0)
									m_writer.Write("[%u]", pTextureStateExpression->arrayIndex[i]);
								else
									m_writer.Write("[]");
							}
						}


						if (argCount == 0)
							m_writer.Write(".get_width(");
						else if (argCount == 1)
							m_writer.Write(".get_height(");
						else if (argCount == 2)
						{
							if (textureStateExpression->type == HLSLBaseType_Sampler2DMS)
							{
								m_writer.Write(".get_num_samples(");
							}
							else
							{
								m_writer.Write(".get_depth(");
							}

							

							// it also handle functions below later
							// https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx-graphics-hlsl-to-getdimensions
							//m_writer.Write(".get_array_size(");
							//m_writer.Write(".get_num_mip_levels(");
						}

						m_writer.Write(")");

						exp = exp->nextExpression;
						argCount++;
					}



				}
				else
				{
					m_writer.Write("%s", textureStateExpression->name);
					OutputExpression(textureStateExpression->functionExpression, NULL, NULL, function, false);
				}
			}
			else
			{
				m_writer.Write("%s", textureStateExpression->name);
				OutputExpression(textureStateExpression->functionExpression, NULL, NULL, function, false);
			}
			
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

bool MSLGenerator::matchFunctionArgumentsIdentifiers(HLSLArgument* argument, const char* name)
{
	while (argument)
	{
		if (String_Equal(argument->name, name))
			return true;
		argument = argument->nextArgument;
	}

	return false;
}

void MSLGenerator::OutputCast(const HLSLType& type)
{

	if (type.baseType == HLSLBaseType_Float3x3)
	{
		m_writer.Write("matrix_ctor");
	}
	else
	{
		m_writer.Write("(");
		OutputDeclarationType(type);
		m_writer.Write(")");
	}
}

// Called by the various Output functions
void MSLGenerator::OutputArguments(HLSLArgument* argument, const HLSLFunction* function)
{
	int numArgs = 0;

	bool bcount = true;

	HLSLArgument* prevArg = NULL;

	while (argument != NULL)
	{
		if (argument->hidden)
		{
			argument = argument->nextArgument;
			continue;
		}

		if (prevArg == NULL)
		{

		}
		else if (prevArg->preprocessor == NULL && numArgs > 0)
		{
			m_writer.Write(", ");
		}

		bool isRef = false;
		bool isConst = false;
		if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
		{
			isRef = true;

		}

		/*
		if (argument->modifier == HLSLArgumentModifier_In || argument->modifier == HLSLArgumentModifier_Const)
		{
			isConst = true;
		}
		*/

		if (argument->modifier == HLSLArgumentModifier_Const)
		{
			isConst = true;
		}
	

		if (argument->preprocessor)
		{
			HLSLpreprocessor* pre = (HLSLpreprocessor*)argument->preprocessor;
			m_writer.Write("\n");
			if (pre->type == HLSLBaseType_PreProcessorIf)
			{
				OutputStatements(0, argument->preprocessor, function);
				bcount = true;
			}
			else if (pre->type == HLSLBaseType_PreProcessorIfDef)
			{
				OutputStatements(0, argument->preprocessor, function);
				bcount = true;
			}
			else if (pre->type == HLSLBaseType_PreProcessorIfnDef)
			{
				OutputStatements(0, argument->preprocessor, function);
				bcount = true;
			}
			else if (pre->type == HLSLBaseType_PreProcessorElse)
			{
				OutputStatements(0, argument->preprocessor, function);
				bcount = false;
			}
			else if (pre->type == HLSLBaseType_PreProcessorEndif)
			{
				OutputStatements(0, argument->preprocessor, function);
				bcount = true;
			}
			else
			{
				bcount = false;
			}
		}
		

		if (argument->preprocessor)
		{

		}
		else
		{
			m_writer.Write(0, "");
			OutputDeclaration(argument->type, argument->name, argument->defaultValue, function, isRef, isConst);

			if (bcount)
				++numArgs;
		}

		prevArg = argument;
		argument = argument->nextArgument;

		/*
		{
			if (numArgs > 0)
			{
				m_writer.Write(", ");
			}

			OutputDeclaration(argument->type, argument->name, argument->defaultValue, function, isRef, isConst);
		}

		argument = argument->nextArgument;
		++numArgs;
		*/
	}
}

void MSLGenerator::OutputDeclaration(const HLSLType& type, const char* name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef, bool isConst, int alignment)
{
	OutputDeclarationType(type, isRef, isConst, alignment);
	OutputDeclarationBody(type, name, assignment, function, isRef);
}

void MSLGenerator::OutputDeclarationType(const HLSLType& type, bool isRef, bool isConst, int alignment)
{
	const char* typeName = GetTypeName(type);
	if (isRef)
	{
		m_writer.Write("thread ");
	}
	if (isConst || type.flags & HLSLTypeFlag_Const)
	{
		m_writer.Write("const ");

		/*
		if (type.flags & HLSLTypeFlag_Static)
		{
		m_writer.Write("static constant constexpr ");
		}
		*/
	}
	if (IsSamplerType(type))
	{
		if (type.baseType == HLSLBaseType_Sampler2D)
			typeName = "Texture2DSampler";
		else if (type.baseType == HLSLBaseType_Sampler3D)
			typeName = "Texture3DSampler";
		else if (type.baseType == HLSLBaseType_SamplerCube)
			typeName = "TextureCubeSampler";
		else if (type.baseType == HLSLBaseType_Sampler2DShadow)
			typeName = "Texture2DShadowSampler";
		else if (type.baseType == HLSLBaseType_Sampler2DMS)
			typeName = "Texture2DMSSampler";
		else if (type.baseType == HLSLBaseType_Sampler2DArray)
			typeName = "Texture2DArraySampler";
		else
			typeName = "<unhandled texture type>";
	}
	else if (alignment != 0)
	{
		m_writer.Write("alignas(%d) ", alignment);
	}

	if (type.array)
	{
		if (type.baseType >= HLSLBaseType_Texture1D && type.baseType <= HLSLBaseType_RWTexture3D)
		{
			m_writer.Write("array<");
			m_writer.Write("%s, %d", GetTypeName(type), type.arrayCount);
			m_writer.Write(">");
		}
		else
			m_writer.Write("%s", typeName);
	}
	else
	{
		m_writer.Write("%s", typeName);

	}

	// Interpolation modifiers.
	if (type.flags & HLSLTypeFlag_NoInterpolation)
	{
		m_writer.Write(" [[flat]]");
	}
	else
	{
		if (type.flags & HLSLTypeFlag_NoPerspective)
		{
			if (type.flags & HLSLTypeFlag_Centroid)
			{
				m_writer.Write(" [[centroid_no_perspective]]");
			}
			else if (type.flags & HLSLTypeFlag_Sample)
			{
				m_writer.Write(" [[sample_no_perspective]]");
			}
			else
			{
				m_writer.Write(" [[center_no_perspective]]");
			}
		}
		else
		{
			if (type.flags & HLSLTypeFlag_Centroid)
			{
				m_writer.Write(" [[centroid_perspective]]");
			}
			else if (type.flags & HLSLTypeFlag_Sample)
			{
				m_writer.Write(" [[sample_perspective]]");
			}
			else
			{
				// Default.
				//m_writer.Write(" [[center_perspective]]");
			}
		}
	}
}

void MSLGenerator::OutputDeclarationBody(const HLSLType& type, const char* name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef)
{
	if (isRef)
	{
		// Arrays of refs are illegal in C++ and hence MSL, need to "link" the & to the var name
		m_writer.Write("(&");
	}

	// Then name
	m_writer.Write(" %s", name);

	if (isRef)
	{
		m_writer.Write(")");
	}

	// Add brackets for arrays
	if (type.array)
	{
		if (type.baseType >= HLSLBaseType_Texture1D && type.baseType <= HLSLBaseType_RWTexture3D)
		{

		}
		else if (type.baseType == HLSLBaseType_OutputPatch)
		{
			
		}
		else
		{
			m_writer.Write("[");
			if (type.arraySize != NULL)
			{
				OutputExpression(type.arraySize, NULL, NULL, function, true);
			}
			m_writer.Write("]");
		}


	}

	// Semantics and registers unhandled for now

	// Assignment handling
	if (assignment != NULL)
	{
		m_writer.Write(" = ");
		if (type.array)
		{
			m_writer.Write("{ ");
			OutputExpressionList(assignment, function);
			m_writer.Write(" }");
		}
		else if (assignment->nextExpression)
		{
			m_writer.Write("{ ");
			OutputExpressionList(assignment, function);
			m_writer.Write(" }");
		}
		else
		{
			OutputExpression(assignment, NULL, NULL, function, true);
		}
	}
}

void MSLGenerator::OutputExpressionList(HLSLExpression* expression, const HLSLFunction* function)
{
	int numExpressions = 0;
	while (expression != NULL)
	{
		if (numExpressions > 0)
		{
			m_writer.Write(", ");
		}
		OutputExpression(expression, NULL, NULL, function, true);
		expression = expression->nextExpression;
		++numExpressions;
	}
}

inline bool isAddressable(HLSLExpression* expression)
{
	if (expression->nodeType == HLSLNodeType_IdentifierExpression)
	{
		return true;
	}
	if (expression->nodeType == HLSLNodeType_ArrayAccess)
	{
		return true;
	}
	if (expression->nodeType == HLSLNodeType_MemberAccess)
	{
		HLSLMemberAccess* memberAccess = (HLSLMemberAccess*)expression;
		return !memberAccess->swizzle;
	}
	return false;
}

void MSLGenerator::OutputFunctionCallStatement(int indent, HLSLFunctionCall* functionCall)
{
	int argumentIndex = 0;
	HLSLArgument* argument = functionCall->function->argument;
	HLSLExpression* expression = functionCall->argument;
	while (argument != NULL)
	{
		if (!isAddressable(expression))
		{
			if (argument->modifier == HLSLArgumentModifier_Out)
			{
				m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
				OutputDeclarationType(argument->type);
				m_writer.Write("tmp%d;", argumentIndex);
				m_writer.EndLine();
			}
			else if (argument->modifier == HLSLArgumentModifier_Inout)
			{
				m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
				OutputDeclarationType(argument->type);
				m_writer.Write("tmp%d = ", argumentIndex);
				OutputExpression(expression, NULL, NULL, functionCall->function, true);
				m_writer.EndLine(";");
			}
		}
		argument = argument->nextArgument;
		expression = expression->nextExpression;
		argumentIndex++;
	}

	m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
	const char* name = functionCall->function->name;
	m_writer.Write("%s(", name);
	//OutputExpressionList(functionCall->argument);

	// Output expression list with temporary substitution.
	argumentIndex = 0;
	argument = functionCall->function->argument;
	expression = functionCall->argument;
	while (expression != NULL)
	{
		if (!isAddressable(expression) && (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout))
		{
			m_writer.Write("tmp%d", argumentIndex);
		}
		else
		{
			OutputExpression(expression, NULL, NULL, functionCall->function, true);
		}

		argument = argument->nextArgument;
		expression = expression->nextExpression;
		argumentIndex++;
		if (expression)
		{
			m_writer.Write(", ");
		}
	}
	m_writer.EndLine(");");

	argumentIndex = 0;
	argument = functionCall->function->argument;
	expression = functionCall->argument;
	while (expression != NULL)
	{
		if (!isAddressable(expression) && (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout))
		{
			m_writer.BeginLine(indent, functionCall->fileName, functionCall->line);
			OutputExpression(expression, NULL, NULL, functionCall->function, true);
			m_writer.Write(" = tmp%d", argumentIndex);
			m_writer.EndLine(";");
		}

		argument = argument->nextArgument;
		expression = expression->nextExpression;
		argumentIndex++;
	}
}

void MSLGenerator::OutputFunctionCall(HLSLFunctionCall* functionCall)
{
	// @@ All these shenanigans only work if the function call is a statement...

	const char* name = functionCall->function->name;

	if (String_Equal(name, "lerp"))
		name = "mix";
	else if (String_Equal(name, "ddx"))
		name = "dfdx";
	else if (String_Equal(name, "ddy"))
		name = "dfdy";
	else if (String_Equal(name, "frac"))
		name = "fract";
	else if (String_Equal(name, "countbits"))
	{
		name = "popcount";
	}

	else if (String_Equal(name, "QuadReadAcrossDiagonal"))
	{
		name = "quad_shuffle";

		m_writer.Write("%s(", name);
		OutputExpressionList(functionCall->argument, functionCall->function);
		m_writer.Write(", 3");
		m_writer.Write(")");
		return;
	}

	else if (String_Equal(name, "QuadReadLaneAt"))
	{
		name = "quad_broadcast";
	}


	else if (String_Equal(name, "QuadReadAcrossX"))
	{
		name = "quad_shuffle";

		m_writer.Write("%s(", name);
		OutputExpressionList(functionCall->argument, functionCall->function);
		m_writer.Write(", 1");
		m_writer.Write(")");
		return;
	}

	else if (String_Equal(name, "QuadReadAcrossY"))
	{
		name = "quad_shuffle";

		m_writer.Write("%s(", name);
		OutputExpressionList(functionCall->argument, functionCall->function);
		m_writer.Write(", 2");
		m_writer.Write(")");
		return;
	}

	else if (String_Equal(name, "WaveActiveAllEqual"))
	{
		name = "<unknown function>";
	}

	else if (String_Equal(name, "WaveActiveBitAnd"))
	{
		name = "simd_and";
	}

	else if (String_Equal(name, "WaveActiveBitOr"))
	{
		name = "simd_or";
	}

	else if (String_Equal(name, "WaveActiveBitXor"))
	{
		name = "simd_xor";
	}

	else if (String_Equal(name, "WaveActiveCountBits"))
	{
		name = "<unknown function>";
	}

	else if (String_Equal(name, "WaveActiveMax"))
	{
		name = "simd_max";
	}
	else if (String_Equal(name, "WaveActiveMin"))
	{
		name = "simd_min";
	}
	else if (String_Equal(name, "WaveActiveProduct"))
	{
		name = "simd_product";
	}
	else if (String_Equal(name, "WaveActiveSum"))
	{
		name = "simd_sum";
	}
	else if (String_Equal(name, "WaveActiveAllTrue"))
	{
		name = "simd_all";
	}
	else if (String_Equal(name, "WaveActiveAnyTrue"))
	{
		name = "simd_any";
	}
	else if (String_Equal(name, "WaveActiveBallot"))
	{
		name = "simd_ballot";
	}

	else if (String_Equal(name, "WaveIsFirstLane"))
	{
		name = "simd_is_first";
	}

	else if (String_Equal(name, "WaveGetLaneIndex"))
	{
		m_writer.Write("simd_lane_id");
		return;
	}
	else if (String_Equal(name, "WaveGetLaneCount"))
	{
		m_writer.Write("simd_size");
		return;
	}

	else if (String_Equal(name, "WavePrefixCountBits"))
	{
		name = "<unknown function>";
	}

	else if (String_Equal(name, "WavePrefixProduct"))
	{
		name = "simd_prefix_exclusive_product";
	}

	else if (String_Equal(name, "WavePrefixSum"))
	{
		name = "simd_prefix_exclusive_sum";
	}

	else if (String_Equal(name, "WaveReadLaneFirst"))
	{
		name = "simd_broadcast_first";
	}

	else if (String_Equal(name, "WaveReadLaneAt"))
	{
		name = "simd_broadcast";
	}
	else if (String_Equal(name, "asfloat"))
		name = "as_type<float>";
	else if (String_Equal(name, "asuint"))
		name = "as_type<uint>";
	else if (String_Equal(name, "asint"))
		name = "as_type<int>";


	m_writer.Write("%s(", name);
	OutputExpressionList(functionCall->argument, functionCall->function);
	m_writer.Write(")");
}

const char* MSLGenerator::TranslateInputSemantic(const char * semantic)
{
	if (semantic == NULL)
		return NULL;

	unsigned int length, index;
	ParseSemantic(semantic, &length, &index);

	if (m_target == MSLGenerator::Target_VertexShader || m_target == MSLGenerator::Target_DomainShader)
	{
		if (String_Equal(semantic, "INSTANCE_ID")) return "instance_id";
		if (String_Equal(semantic, "SV_InstanceID")) return "instance_id";
		if (String_Equal(semantic, "VERTEX_ID")) return "vertex_id";
		if (String_Equal(semantic, "SV_VertexID")) return "vertex_id";

		if (m_target == Target_DomainShader && (String_Equal(semantic, "SV_DomainLocation")))
			return "position_in_patch";

		return m_tree->AddStringFormat("attribute(%d)", attributeCounter++);


		if (m_options.attributeCallback)
		{
			char name[64];
			ASSERT(length < sizeof(name));

			strncpy(name, semantic, length);
			name[length] = 0;

			int attribute = m_options.attributeCallback(name, index);

			if (attribute >= 0)
			{
				return m_tree->AddStringFormat("attribute(%d)", attribute);
			}
		}
	}
	else if (m_target == MSLGenerator::Target_HullShader)
	{
		if (String_Equal(semantic, "SV_PrimitiveID"))
			return "????";

		return m_tree->AddStringFormat("attribute(%d)", attributeCounter++);
	}
	else if (m_target == MSLGenerator::Target_FragmentShader)
	{
		//if (String_Equal(semantic, "POSITION")) return "position";
		if (String_Equal(semantic, "SV_Position") || String_Equal(semantic, "SV_POSITION")) return "position";
		if (String_Equal(semantic, "SV_SampleIndex")) return "sample_id";
		if (String_Equal(semantic, "VFACE")) return "front_facing";
		if (String_Equal(semantic, "TARGET_INDEX")) return "render_target_array_index";
	}
	else if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_DispatchThreadID")))
		return "thread_position_in_grid";
	else  if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_GroupID")))
		return "threadgroup_position_in_grid";
	else  if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_GroupIndex")))
		return "thread_index_in_threadgroup";
	else  if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_GroupThreadID")))
		return "thread_position_in_threadgroup";
	

	else
	{

	}

	return NULL;
}


const char* MSLGenerator::TranslateOutputSemantic(const char * semantic)
{
	if (semantic == NULL)
		return NULL;

	unsigned int length, index;
	ParseSemantic(semantic, &length, &index);

	if (m_target == MSLGenerator::Target_VertexShader || m_target == Target_DomainShader)
	{
		//if (String_Equal(semantic, "POSITION")) return "position";
		if (String_Equal(semantic, "SV_Position") || String_Equal(semantic, "SV_POSITION")) return "position";
		if (String_Equal(semantic, "PSIZE")) return "point_size";
		if (String_Equal(semantic, "POINT_SIZE")) return "point_size";
		if (String_Equal(semantic, "TARGET_INDEX")) return "render_target_array_index";
	}
	else if (m_target == MSLGenerator::Target_HullShader)
	{
		if (String_Equal(semantic, "SV_Position") || String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "POSITION")) return "position";


		if (String_Equal(semantic, "SV_PrimitiveID"))
			return "????";
	}
	else if (m_target == MSLGenerator::Target_FragmentShader)
	{
		if (m_options.flags & MSLGenerator::Flag_NoIndexAttribute)
		{
			// No dual-source blending on iOS, and no index() attribute
			if (String_Equal(semantic, "COLOR0_1")) return NULL;
		}
		else
		{
			// @@ IC: Hardcoded for this specific case, extend ParseSemantic?
			if (String_Equal(semantic, "COLOR0_1")) return "color(0), index(1)";
		}

		if (strncmp(semantic, "SV_Target", length) == 0)
		{
			return m_tree->AddStringFormat("color(%d)", index);
		}
		if (strncmp(semantic, "COLOR", length) == 0)
		{
			return m_tree->AddStringFormat("color(%d)", index);
		}

		if (String_Equal(semantic, "DEPTH")) return "depth(any)";
		if (String_Equal(semantic, "DEPTH_GT")) return "depth(greater)";
		if (String_Equal(semantic, "DEPTH_LT")) return "depth(less)";
		if (String_Equal(semantic, "SAMPLE_MASK")) return "sample_mask";
	}

	return NULL;
}

void MSLGenerator::OutPushConstantIdentifierTextureStateExpression(int size, int counter, const HLSLTextureStateExpression* pTextureStateExpression, bool* bWritten)
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
