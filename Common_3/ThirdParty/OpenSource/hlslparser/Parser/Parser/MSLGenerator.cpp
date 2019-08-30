//=============================================================================
//
// Render/MSLGenerator.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#include "Engine.h"

#include "MSLGenerator.h"
#include "HLSLParser.h"
#include "HLSLTree.h"

#include <string.h>
#include "StringLibrary.h"

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


CachedString MSLGenerator::GetTypeName(const HLSLType& type)
{
	HLSLBaseType baseType = type.baseType;

	if (type.baseType == HLSLBaseType_Unknown)
		baseType = type.elementType;

	switch (baseType)
	{
	case HLSLBaseType_Void:				return MakeCached("void");
	case HLSLBaseType_Float:			return MakeCached("float");
	case HLSLBaseType_Float1x2:			return MakeCached("float1x2");
	case HLSLBaseType_Float1x3:			return MakeCached("float1x3");
	case HLSLBaseType_Float1x4:			return MakeCached("float1x4");
	case HLSLBaseType_Float2:			return MakeCached("float2");
	case HLSLBaseType_Float2x2:			return MakeCached("float2x2");
	case HLSLBaseType_Float2x3:			return MakeCached("float2x3");
	case HLSLBaseType_Float2x4:			return MakeCached("float2x4");
	case HLSLBaseType_Float3:			return MakeCached("float3");
	case HLSLBaseType_Float3x2:			return MakeCached("float3x2");
	case HLSLBaseType_Float3x3:			return MakeCached("float3x3");
	case HLSLBaseType_Float3x4:			return MakeCached("float3x4");
	case HLSLBaseType_Float4:			return MakeCached("float4");
	case HLSLBaseType_Float4x2:			return MakeCached("float4x2");
	case HLSLBaseType_Float4x3:			return MakeCached("float4x3");
	case HLSLBaseType_Float4x4:			return MakeCached("float4x4");

	case HLSLBaseType_Half:				return MakeCached("half");
	case HLSLBaseType_Half1x2:			return MakeCached("half1x2");
	case HLSLBaseType_Half1x3:			return MakeCached("half1x3");
	case HLSLBaseType_Half1x4:			return MakeCached("half1x4");
	case HLSLBaseType_Half2:			return MakeCached("half2");
	case HLSLBaseType_Half2x2:			return MakeCached("half2x2");
	case HLSLBaseType_Half2x3:			return MakeCached("half2x3");
	case HLSLBaseType_Half2x4:			return MakeCached("half2x4");
	case HLSLBaseType_Half3:			return MakeCached("half3");
	case HLSLBaseType_Half3x2:			return MakeCached("half3x2");
	case HLSLBaseType_Half3x3:			return MakeCached("half3x3");
	case HLSLBaseType_Half3x4:			return MakeCached("half3x4");
	case HLSLBaseType_Half4:			return MakeCached("half4");
	case HLSLBaseType_Half4x2:			return MakeCached("half4x2");
	case HLSLBaseType_Half4x3:			return MakeCached("half4x3");
	case HLSLBaseType_Half4x4:			return MakeCached("half4x4");

	case HLSLBaseType_Min16Float:       return MakeCached("half");
	case HLSLBaseType_Min16Float1x2:    return MakeCached("half1x2");
	case HLSLBaseType_Min16Float1x3:    return MakeCached("half1x3");
	case HLSLBaseType_Min16Float1x4:    return MakeCached("half1x4");
	case HLSLBaseType_Min16Float2:      return MakeCached("half2");
	case HLSLBaseType_Min16Float2x2:    return MakeCached("half2x2");
	case HLSLBaseType_Min16Float2x3:    return MakeCached("half2x3");
	case HLSLBaseType_Min16Float2x4:    return MakeCached("half2x4");
	case HLSLBaseType_Min16Float3:      return MakeCached("half3");
	case HLSLBaseType_Min16Float3x2:    return MakeCached("half3x2");
	case HLSLBaseType_Min16Float3x3:    return MakeCached("half3x3");
	case HLSLBaseType_Min16Float3x4:    return MakeCached("half3x4");
	case HLSLBaseType_Min16Float4:      return MakeCached("half4");
	case HLSLBaseType_Min16Float4x2:    return MakeCached("half4x2");
	case HLSLBaseType_Min16Float4x3:    return MakeCached("half4x3");
	case HLSLBaseType_Min16Float4x4:    return MakeCached("half4x4");

	case HLSLBaseType_Min10Float:       return MakeCached("half");
	case HLSLBaseType_Min10Float1x2:    return MakeCached("half1x2");
	case HLSLBaseType_Min10Float1x3:    return MakeCached("half1x3");
	case HLSLBaseType_Min10Float1x4:    return MakeCached("half1x4");
	case HLSLBaseType_Min10Float2:      return MakeCached("half2");
	case HLSLBaseType_Min10Float2x2:    return MakeCached("half2x2");
	case HLSLBaseType_Min10Float2x3:    return MakeCached("half2x3");
	case HLSLBaseType_Min10Float2x4:    return MakeCached("half2x4");
	case HLSLBaseType_Min10Float3:      return MakeCached("half3");
	case HLSLBaseType_Min10Float3x2:    return MakeCached("half3x2");
	case HLSLBaseType_Min10Float3x3:    return MakeCached("half3x3");
	case HLSLBaseType_Min10Float3x4:    return MakeCached("half3x4");
	case HLSLBaseType_Min10Float4:      return MakeCached("half4");
	case HLSLBaseType_Min10Float4x2:    return MakeCached("half4x2");
	case HLSLBaseType_Min10Float4x3:    return MakeCached("half4x3");
	case HLSLBaseType_Min10Float4x4:    return MakeCached("half4x4");

	case HLSLBaseType_Bool:				return MakeCached("bool");
	case HLSLBaseType_Bool1x2:			return MakeCached("bool1x2");
	case HLSLBaseType_Bool1x3:        return MakeCached("bool1x3");
	case HLSLBaseType_Bool1x4:        return MakeCached("bool1x4");
	case HLSLBaseType_Bool2:        return MakeCached("bool2");
	case HLSLBaseType_Bool2x2:        return MakeCached("bool2x2");
	case HLSLBaseType_Bool2x3:        return MakeCached("bool2x3");
	case HLSLBaseType_Bool2x4:        return MakeCached("bool2x4");
	case HLSLBaseType_Bool3:        return MakeCached("bool3");
	case HLSLBaseType_Bool3x2:        return MakeCached("bool3x2");
	case HLSLBaseType_Bool3x3:        return MakeCached("bool3x3");
	case HLSLBaseType_Bool3x4:        return MakeCached("bool3x4");
	case HLSLBaseType_Bool4:        return MakeCached("bool4");
	case HLSLBaseType_Bool4x2:        return MakeCached("bool4x2");
	case HLSLBaseType_Bool4x3:        return MakeCached("bool4x3");
	case HLSLBaseType_Bool4x4:        return MakeCached("bool4x4");

	case HLSLBaseType_Int:          return MakeCached("int");
	case HLSLBaseType_Int1x2:        return MakeCached("int1x2");
	case HLSLBaseType_Int1x3:        return MakeCached("int1x3");
	case HLSLBaseType_Int1x4:        return MakeCached("int1x4");
	case HLSLBaseType_Int2:        return MakeCached("int2");
	case HLSLBaseType_Int2x2:        return MakeCached("int2x2");
	case HLSLBaseType_Int2x3:        return MakeCached("int2x3");
	case HLSLBaseType_Int2x4:        return MakeCached("int2x4");
	case HLSLBaseType_Int3:        return MakeCached("int3");
	case HLSLBaseType_Int3x2:        return MakeCached("int3x2");
	case HLSLBaseType_Int3x3:        return MakeCached("int3x3");
	case HLSLBaseType_Int3x4:        return MakeCached("int3x4");
	case HLSLBaseType_Int4:        return MakeCached("int4");
	case HLSLBaseType_Int4x2:        return MakeCached("int4x2");
	case HLSLBaseType_Int4x3:        return MakeCached("int4x3");
	case HLSLBaseType_Int4x4:        return MakeCached("int4x4");

	case HLSLBaseType_Uint:          return MakeCached("uint");
	case HLSLBaseType_Uint1x2:        return MakeCached("uint1x2");
	case HLSLBaseType_Uint1x3:        return MakeCached("uint1x3");
	case HLSLBaseType_Uint1x4:        return MakeCached("uint1x4");
	case HLSLBaseType_Uint2:        return MakeCached("uint2");
	case HLSLBaseType_Uint2x2:        return MakeCached("uint2x2");
	case HLSLBaseType_Uint2x3:        return MakeCached("uint2x3");
	case HLSLBaseType_Uint2x4:        return MakeCached("uint2x4");
	case HLSLBaseType_Uint3:        return MakeCached("uint3");
	case HLSLBaseType_Uint3x2:        return MakeCached("uint3x2");
	case HLSLBaseType_Uint3x3:        return MakeCached("uint3x3");
	case HLSLBaseType_Uint3x4:        return MakeCached("uint3x4");
	case HLSLBaseType_Uint4:        return MakeCached("uint4");
	case HLSLBaseType_Uint4x2:        return MakeCached("uint4x2");
	case HLSLBaseType_Uint4x3:        return MakeCached("uint4x3");
	case HLSLBaseType_Uint4x4:        return MakeCached("uint4x4");

	case HLSLBaseType_InputPatch:

		return type.typeName;

	case HLSLBaseType_OutputPatch:

		return type.structuredTypeName;

	case HLSLBaseType_PatchControlPoint:

		return type.typeName;

	case HLSLBaseType_Texture:          return MakeCached("texture");


	case HLSLBaseType_Texture1D:      return MakeCached("texture1d<float>");
	case HLSLBaseType_Texture1DArray:      return MakeCached("texture1d_array<float>");
	case HLSLBaseType_Texture2D:      return MakeCached("texture2d<float>");
	case HLSLBaseType_Texture2DArray:      return MakeCached("texture2d_array<float>");
	case HLSLBaseType_Texture3D:      return MakeCached("texture3d<float>");
	case HLSLBaseType_Texture2DMS:      return MakeCached("texture2d_ms<float>");
	case HLSLBaseType_Texture2DMSArray:      return MakeCached("texture2d_ms_array<float>");
	case HLSLBaseType_TextureCube:      return MakeCached("texturecube<float>");
	case HLSLBaseType_TextureCubeArray:      return MakeCached("texturecube_array<float>");

	case HLSLBaseType_RasterizerOrderedTexture1D:      return MakeCached("texture1d<float, access::read_write>");
	case HLSLBaseType_RasterizerOrderedTexture1DArray:      return MakeCached("texture1d_array<float, access::read_write>");
	case HLSLBaseType_RasterizerOrderedTexture2D:      return MakeCached("texture2d<float, access::read_write>");
	case HLSLBaseType_RasterizerOrderedTexture2DArray:      return MakeCached("texture2d_array<float, access::read_write>");
	case HLSLBaseType_RasterizerOrderedTexture3D:      return MakeCached("texture3d<float, access::read_write>");

	case HLSLBaseType_RWTexture1D:      return MakeCached("texture1d<float, access::read_write>");
	case HLSLBaseType_RWTexture1DArray:      return MakeCached("texture1d_array<float, access::read_write>");
	case HLSLBaseType_RWTexture2D:      return MakeCached("texture2d<float, access::read_write>");
	case HLSLBaseType_RWTexture2DArray:      return MakeCached("texture2d_array<float, access::read_write>");
	case HLSLBaseType_RWTexture3D:      return MakeCached("texture3d<float, access::read_write>");

	case HLSLBaseType_DepthTexture2D:      return MakeCached("depth2d<float>");
	case HLSLBaseType_DepthTexture2DArray:      return MakeCached("depth2d_array<float>");
	case HLSLBaseType_DepthTexture2DMS:      return MakeCached("depth2d_ms<float>");
	case HLSLBaseType_DepthTexture2DMSArray:      return MakeCached("depth2d_ms_array<float>");
	case HLSLBaseType_DepthTextureCube:      return MakeCached("depthcube<float>");
	case HLSLBaseType_DepthTextureCubeArray:      return MakeCached("depthcube_array<float>");


	case HLSLBaseType_Sampler:          return MakeCached("sampler");
		// ACoget-TODO: How to detect non-float textures, if relevant?
	case HLSLBaseType_Sampler2D:        return MakeCached("texture2d<float>");
	case HLSLBaseType_Sampler3D:        return MakeCached("texture3d<float>");
	case HLSLBaseType_SamplerCube:      return MakeCached("texturecube<float>");
	case HLSLBaseType_Sampler2DShadow:  return MakeCached("depth2d<float>");
	case HLSLBaseType_Sampler2DMS:      return MakeCached("texture2d_ms<float>");
	case HLSLBaseType_Sampler2DArray:   return MakeCached("texture2d_array<float>");
	case HLSLBaseType_SamplerState:		return MakeCached("sampler");
	case HLSLBaseType_UserDefined:      return type.typeName;
	default:
		ASSERT_PARSER(0);
		return MakeCached("<unknown type>");
	}
}

static int GetFunctionArguments(HLSLFunctionCall* functionCall, HLSLExpression* expression[], int maxArguments)
{
	HLSLExpression* argument = functionCall->callArgument;
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
	m_target = Target_VertexShader;
	m_error = false;

	m_firstClassArgument = NULL;
	m_lastClassArgument = NULL;

	attributeCounter = 0;

	m_RWBuffers.clear();
	m_RWStructuredBuffers.clear();
	m_PushConstantBuffers.clear();
	m_StructBuffers.clear();

	m_stringLibrary = NULL;
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


class FindGlobalVisitor : public HLSLTreeVisitor
{
public:
//	bool found;
//	CachedString name;

	HLSLTree * m_tree = NULL;

	bool debugMe = false;

	StringLibrary m_foundNames;
	//StringLibrary m_foundFuncs;

	eastl::hash_set < const HLSLFunction * > m_foundFuncs;



	//eastl::vector < HLSLFunction * > m_searchedFunctions;



	FindGlobalVisitor()
	{
		//found = false;
		//name.Reset();// = NULL;
	}
#if 0
	bool FindArgument(const CachedString & name, const HLSLFunction * function, HLSLTree * tree)
	{
		this->m_tree = tree;
		this->found = false;
		this->name = name;
		VisitStatements(function->statement);
		return found;
	}
#endif

	void AddString(const eastl::string& name)
	{
		m_foundNames.InsertDirect(name);
	}

	void AddString(const CachedString & name)
	{
		////TODO: remove????
		//if (String_Equal(name,"localNextPositionRest"))
		//{
		//	int test = 0;
		//	test++;
		//}
		m_foundNames.InsertDirect(name.m_string);
	}

	virtual void VisitStatements(HLSLStatement * statement) override
	{
		// if we found the argument, then just early out
		while (statement != NULL)
		{
			VisitStatement(statement);
			statement = statement->nextStatement;
		}
	}

	virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node) override
	{
		if (debugMe)
		{
			printf("%s\n", RawStr(node->name));
		}

		AddString(node->name);

		// also, check if this node is part of a cbuffer
		CachedString foundBuffer = m_tree->FindBuffertMember(node->name);
		AddString(foundBuffer);
	}

	void VisitSamplerIdentifier(const CachedString & samplerName)
	{
		if (debugMe)
		{
			printf("%s\n", RawStr(samplerName));
		}

		AddString(samplerName);
	}

	void VisitArrayAccess(HLSLArrayAccess * node)
	{
		if (debugMe)
		{
			printf("%s\n", RawStr(node->identifier));
		}
		AddString(node->identifier);

		HLSLTreeVisitor::VisitArrayAccess(node);
	}

	void VisitTextureIdentifier(const CachedString & texName)
	{
		if (debugMe)
		{
			printf("%s\n", RawStr(texName));
		}

		AddString(texName);
	}

	void VisitFunctionCall(HLSLFunctionCall * node)
	{
		if (node->function->statement != NULL)
		{
			CachedString funcName = node->function->name;

			// have we already searched this function?
			bool found = false;
			{
				eastl::hash_set<const HLSLFunction * >::iterator iter = m_foundFuncs.find(node->function);
				if (iter != m_foundFuncs.end())
				{
					found = true;
				}
			}

			if (debugMe)
			{
				printf("    Begin: %s\n", RawStr(funcName));
			}

			if (!found)
			{
				FindGlobalVisitor visitor;
				visitor.debugMe = debugMe;

				visitor.m_tree = m_tree;
				visitor.VisitStatements(node->function->statement);

				// add the globals we found
				{
					eastl::vector < eastl::string > allStrings = visitor.m_foundNames.GetFlatStrings();
			
					for (int i = 0; i < allStrings.size(); i++)
					{
						AddString(allStrings[i]);
					}
				}

				// and add the funcs that we found
				{
					for (eastl::hash_set<const HLSLFunction *>::iterator iter = visitor.m_foundFuncs.begin();
						iter != visitor.m_foundFuncs.end();
						iter++)
					{
						m_foundFuncs.insert(*iter);
					}
				}

				m_foundFuncs.insert(node->function);
			}

			if (debugMe)
			{
				printf("    End: %s\n", RawStr(funcName));
			}

		}

		HLSLTreeVisitor::VisitFunctionCall(node);
	}

};

/*
bool MSLGenerator::DoesEntryUseName(HLSLFunction* entryFunction, const CachedString & name)
{
	FindGlobalVisitor visitor;
	visitor.name = name;
	visitor.found = false;

	bool found = visitor.FindArgument(name, entryFunction,m_tree);
	return found;
}
*/

void MSLGenerator::GetAllEntryUsedNames(StringLibrary & foundNames,
	eastl::hash_set < const HLSLFunction * > & foundFuncs,
	HLSLFunction* entryFunction)
{
	FindGlobalVisitor visitor;
	visitor.m_tree = m_tree;
	visitor.m_foundFuncs.insert(entryFunction);
	visitor.VisitStatements(entryFunction->statement);

	foundNames = visitor.m_foundNames;
	foundFuncs = visitor.m_foundFuncs;
}

void MSLGenerator::Prepass(HLSLTree* tree, Target target, HLSLFunction* entryFunction, HLSLFunction* secondaryEntryFunction)
{
	// Hide unused arguments. @@ It would be good to do this in the other generators too.
	HLSLRoot* root = tree->GetRoot();
	HLSLStatement* statement = root->statement;

	HLSLType samplerType(HLSLBaseType_Sampler);

	m_nextTextureRegister = 0;
	m_nextSamplerRegister = 0;
	m_nextBufferRegister = 0; // options start at 1

	int mainFuncCounter = 0;

#if 0
	// test
	if (1)
	{
		FindGlobalVisitor visitor;
		visitor.debugMe = true;

		visitor.m_tree = m_tree;
		//HLSLFunction* tempEntryFunction = tree->FindFunction(MakeCached("QuatFromUnitVectors"));

		visitor.VisitStatements(entryFunction->statement);
		int temp = 0;
		temp++;
	}
#endif

	StringLibrary usedNames;
	{
		eastl::hash_set < const HLSLFunction * > foundFuncs;
		GetAllEntryUsedNames(usedNames, foundFuncs, entryFunction);
	}

	bool keepUnused = false;
	while (statement != NULL)
	{
		if (statement->nodeType == HLSLNodeType_Declaration)
		{
			HLSLDeclaration* declaration = (HLSLDeclaration*)statement;

			if (!declaration->hidden && IsSamplerType(declaration->type))
			{
				bool isUsed = usedNames.HasString(declaration->cachedName.m_string);
				if (keepUnused || isUsed)
				{
					//int textureRegister = ParseRegister(declaration->registerName, nextTextureRegister) + m_options.textureRegisterOffset;
					//int textureRegister = nextTextureRegister++ + m_options.textureRegisterOffset;
					//int textureRegister = nextTextureRegister + m_options.textureRegisterOffset;
					//nextTextureRegister++;
					int textureRegister = GetTextureRegister(declaration->cachedName);

					CachedString textureName = m_tree->AddStringFormatCached("%s_texture", RawStr(declaration->cachedName));
					CachedString textureRegisterName = m_tree->AddStringFormatCached("texture(%d)", textureRegister);
					AddClassArgument(new ClassArgument(textureName, declaration->type, textureRegisterName));

					if (declaration->type.baseType != HLSLBaseType_Sampler2DMS)
					{
						//int samplerRegister = nextSamplerRegister++;
						// not sure if this is right
						int samplerRegister = GetTextureRegister(declaration->cachedName);

						CachedString samplerName = m_tree->AddStringFormatCached("%s_sampler", RawStr(declaration->cachedName));
						CachedString samplerRegisterName = m_tree->AddStringFormatCached("sampler(%d)", samplerRegister);
						AddClassArgument(new ClassArgument(samplerName, samplerType, samplerRegisterName));
					}
				}
			}
		}
		else if (statement->nodeType == HLSLNodeType_Buffer)
		{
			HLSLBuffer * buffer = (HLSLBuffer *)statement;

			if (buffer->type.baseType == HLSLBaseType_CBuffer ||
				buffer->type.baseType == HLSLBaseType_TBuffer)
			{

				bool isUsed = usedNames.HasString(buffer->cachedName.m_string);
				if (keepUnused || isUsed)
				{
					HLSLType type(HLSLBaseType_UserDefined);
					type.addressSpace = HLSLAddressSpace_Constant;


					type.typeName = m_tree->AddStringFormatCached("Uniforms_%s", RawStr(buffer->cachedName));

					//int bufferRegister = nextBufferRegister + m_options.bufferRegisterOffset;
					//nextBufferRegister++;
					int bufferRegister = GetTextureRegister(buffer->cachedName);


					CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d)", bufferRegister);

					AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName));
				}
			}
			if (buffer->type.baseType == HLSLBaseType_ConstantBuffer)
			{
				bool isUsed = usedNames.HasString(buffer->cachedName.m_string);
				if (keepUnused || isUsed)
				{
					HLSLType type(HLSLBaseType_UserDefined);
					type.addressSpace = HLSLAddressSpace_Constant;


					if (buffer->bPushConstant)
					{
						m_PushConstantBuffers.push_back(buffer);
					}

					type.typeName = m_tree->AddStringFormatCached("Uniforms_%s", RawStr(buffer->cachedName));

					//int bufferRegister = nextBufferRegister + m_options.bufferRegisterOffset;
					//nextBufferRegister++;
					int bufferRegister = GetTextureRegister(buffer->cachedName);

					CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d)", bufferRegister);

					AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName));
				}
			}
			else if (buffer->type.baseType == HLSLBaseType_ByteAddressBuffer || buffer->type.baseType == HLSLBaseType_RWByteAddressBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedByteAddressBuffer)
			{
				bool isUsed = usedNames.HasString(buffer->cachedName.m_string);
				if (keepUnused || isUsed)
				{
					HLSLType type(HLSLBaseType_Uint);
					type.addressSpace = HLSLAddressSpace_Constant;
					type.typeName = m_tree->AddStringFormatCached("uint");
					type.structuredTypeName = type.typeName;

					//int bufferRegister = ParseRegister(buffer->registerName, nextBufferRegister) + m_options.bufferRegisterOffset;
					//int bufferRegister = nextBufferRegister + m_options.bufferRegisterOffset;
					//nextBufferRegister++;
					int bufferRegister = GetTextureRegister(buffer->cachedName);

					if (buffer->type.baseType == HLSLBaseType_RasterizerOrderedByteAddressBuffer)
					{
						CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d), raster_order_group(%d)", bufferRegister, 0);
						AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName, true));
					}
					else
					{
						CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d)", bufferRegister);
						AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName, true));
					}
				}
			}
			else if (buffer->type.baseType == HLSLBaseType_StructuredBuffer || buffer->type.baseType == HLSLBaseType_PureBuffer)
			{

				bool isUsed = usedNames.HasString(buffer->cachedName.m_string);
				if (keepUnused || isUsed)
				{
					HLSLType type(HLSLBaseType_UserDefined);
					type.addressSpace = HLSLAddressSpace_Constant;

					if (buffer->type.elementType)
					{
						type.baseType = buffer->type.elementType;
						type.elementType = buffer->type.elementType;
						type.structuredTypeName = type.typeName = m_tree->AddStringFormatCached("%s", getElementTypeAsStr(m_stringLibrary,buffer->type));
					}
					else
					{
						type.baseType = buffer->type.elementType;
						type.elementType = buffer->type.elementType;
						switch (buffer->type.elementType)
						{
						case HLSLBaseType_Float:
							type.typeName = m_tree->AddStringFormatCached("float");
							break;
						case HLSLBaseType_Float2:
							type.typeName = m_tree->AddStringFormatCached("float2");
							break;
						case HLSLBaseType_Float3:
							type.typeName = m_tree->AddStringFormatCached("float3");
							break;
						case HLSLBaseType_Float4:
							type.typeName = m_tree->AddStringFormatCached("float4");
							break;

						case HLSLBaseType_Half:
							type.typeName = m_tree->AddStringFormatCached("half");
							break;
						case HLSLBaseType_Half2:
							type.typeName = m_tree->AddStringFormatCached("half2");
							break;
						case HLSLBaseType_Half3:
							type.typeName = m_tree->AddStringFormatCached("half3");
							break;
						case HLSLBaseType_Half4:
							type.typeName = m_tree->AddStringFormatCached("half4");
							break;

						case HLSLBaseType_Min16Float:
							type.typeName = m_tree->AddStringFormatCached("half");
							break;
						case HLSLBaseType_Min16Float2:
							type.typeName = m_tree->AddStringFormatCached("half2");
							break;
						case HLSLBaseType_Min16Float3:
							type.typeName = m_tree->AddStringFormatCached("half3");
							break;
						case HLSLBaseType_Min16Float4:
							type.typeName = m_tree->AddStringFormatCached("half4");
							break;

						case HLSLBaseType_Min10Float:
							type.typeName = m_tree->AddStringFormatCached("half");
							break;
						case HLSLBaseType_Min10Float2:
							type.typeName = m_tree->AddStringFormatCached("half2");
							break;
						case HLSLBaseType_Min10Float3:
							type.typeName = m_tree->AddStringFormatCached("half3");
							break;
						case HLSLBaseType_Min10Float4:
							type.typeName = m_tree->AddStringFormatCached("half4");
							break;

						case HLSLBaseType_Bool:
							type.typeName = m_tree->AddStringFormatCached("bool");
							break;
						case HLSLBaseType_Bool2:
							type.typeName = m_tree->AddStringFormatCached("bool2");
							break;
						case HLSLBaseType_Bool3:
							type.typeName = m_tree->AddStringFormatCached("bool3");
							break;
						case HLSLBaseType_Bool4:
							type.typeName = m_tree->AddStringFormatCached("bool4");
							break;

						case HLSLBaseType_Int:
							type.typeName = m_tree->AddStringFormatCached("int");
							break;
						case HLSLBaseType_Int2:
							type.typeName = m_tree->AddStringFormatCached("int2");
							break;
						case HLSLBaseType_Int3:
							type.typeName = m_tree->AddStringFormatCached("int3");
							break;
						case HLSLBaseType_Int4:
							type.typeName = m_tree->AddStringFormatCached("int4");
							break;

						case HLSLBaseType_Uint:
							type.typeName = m_tree->AddStringFormatCached("uint");
							break;
						case HLSLBaseType_Uint2:
							type.typeName = m_tree->AddStringFormatCached("uint2");
							break;
						case HLSLBaseType_Uint3:
							type.typeName = m_tree->AddStringFormatCached("uint3");
							break;
						case HLSLBaseType_Uint4:
							type.typeName = m_tree->AddStringFormatCached("uint4");
							break;
						default:
							break;
						}

						type.structuredTypeName = type.typeName;
					}

					//int bufferRegister = nextBufferRegister + m_options.bufferRegisterOffset;
					//nextBufferRegister++;
					int bufferRegister = GetTextureRegister(buffer->cachedName);

					CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d)", bufferRegister);

					AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName, true));
				}
			}
			else if (buffer->type.baseType == HLSLBaseType_RWBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedBuffer)
			{
				bool isUsed = usedNames.HasString(buffer->cachedName.m_string);
				if (keepUnused || isUsed)
				{
					HLSLType type(buffer->type.elementType);
					type.addressSpace = HLSLAddressSpace_Device;


					const char* atomic = "";

					if (buffer->bAtomic)
						atomic = "atomic_";

					type.typeName = m_tree->AddStringFormatCached("%s%s", atomic, RawStr(GetTypeName(type)));

					//int bufferRegister = nextBufferRegister + m_options.bufferRegisterOffset;
					//nextBufferRegister++;
					int bufferRegister = GetTextureRegister(buffer->cachedName);

					if (buffer->type.baseType == HLSLBaseType_RWBuffer)
					{
						CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d)", bufferRegister);
						AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName));
					}
					else if (buffer->type.baseType == HLSLBaseType_RasterizerOrderedBuffer)
					{
						CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d), raster_order_group(%d)", bufferRegister, 0);
						AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName));
					}
				}

				m_RWBuffers.push_back(buffer);
			}
			else if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer || buffer->type.baseType == HLSLBaseType_RasterizerOrderedStructuredBuffer)
			{
				bool isUsed = usedNames.HasString(buffer->cachedName.m_string);
				if (keepUnused || isUsed)
				{
					HLSLType type;

					if (buffer->type.elementType == HLSLBaseType_Unknown)
					{
						type.baseType = HLSLBaseType_UserDefined;
						type.addressSpace = HLSLAddressSpace_Device;
						type.typeName = m_tree->AddStringFormatCached("%s", getElementTypeAsStr(m_stringLibrary, buffer->type));
					}
					else
					{
						type.addressSpace = HLSLAddressSpace_Device;
						type.baseType = buffer->type.elementType;
						type.elementType = buffer->type.elementType;

						const char* atomic = "";

						if (buffer->bAtomic)
							atomic = "atomic_";

						switch (buffer->type.elementType)
						{
						case HLSLBaseType_Float:
							type.typeName = m_tree->AddStringFormatCached("%sfloat", atomic);
							break;
						case HLSLBaseType_Float2:
							type.typeName = m_tree->AddStringFormatCached("%sfloat2", atomic);
							break;
						case HLSLBaseType_Float3:
							type.typeName = m_tree->AddStringFormatCached("%sfloat3", atomic);
							break;
						case HLSLBaseType_Float4:
							type.typeName = m_tree->AddStringFormatCached("%sfloat4", atomic);
							break;

						case HLSLBaseType_Half:
							type.typeName = m_tree->AddStringFormatCached("%shalf", atomic);
							break;
						case HLSLBaseType_Half2:
							type.typeName = m_tree->AddStringFormatCached("%shalf2", atomic);
							break;
						case HLSLBaseType_Half3:
							type.typeName = m_tree->AddStringFormatCached("%shalf3", atomic);
							break;
						case HLSLBaseType_Half4:
							type.typeName = m_tree->AddStringFormatCached("%shalf4", atomic);
							break;

						case HLSLBaseType_Min16Float:
							type.typeName = m_tree->AddStringFormatCached("%shalf", atomic);
							break;
						case HLSLBaseType_Min16Float2:
							type.typeName = m_tree->AddStringFormatCached("%shalf2", atomic);
							break;
						case HLSLBaseType_Min16Float3:
							type.typeName = m_tree->AddStringFormatCached("%shalf3", atomic);
							break;
						case HLSLBaseType_Min16Float4:
							type.typeName = m_tree->AddStringFormatCached("%shalf4", atomic);
							break;

						case HLSLBaseType_Min10Float:
							type.typeName = m_tree->AddStringFormatCached("%shalf", atomic);
							break;
						case HLSLBaseType_Min10Float2:
							type.typeName = m_tree->AddStringFormatCached("%shalf2", atomic);
							break;
						case HLSLBaseType_Min10Float3:
							type.typeName = m_tree->AddStringFormatCached("%shalf3", atomic);
							break;
						case HLSLBaseType_Min10Float4:
							type.typeName = m_tree->AddStringFormatCached("%shalf4", atomic);
							break;

						case HLSLBaseType_Bool:
							type.typeName = m_tree->AddStringFormatCached("%sbool", atomic);
							break;
						case HLSLBaseType_Bool2:
							type.typeName = m_tree->AddStringFormatCached("%sbool2", atomic);
							break;
						case HLSLBaseType_Bool3:
							type.typeName = m_tree->AddStringFormatCached("%sbool3", atomic);
							break;
						case HLSLBaseType_Bool4:
							type.typeName = m_tree->AddStringFormatCached("%sbool4", atomic);
							break;
						case HLSLBaseType_Int:
							type.typeName = m_tree->AddStringFormatCached("%sint", atomic);
							break;
						case HLSLBaseType_Int2:
							type.typeName = m_tree->AddStringFormatCached("%sint2", atomic);
							break;
						case HLSLBaseType_Int3:
							type.typeName = m_tree->AddStringFormatCached("%sint3", atomic);
							break;
						case HLSLBaseType_Int4:
							type.typeName = m_tree->AddStringFormatCached("%sint4", atomic);
							break;
						case HLSLBaseType_Uint:
							type.typeName = m_tree->AddStringFormatCached("%suint", atomic);
							break;
						case HLSLBaseType_Uint2:
							type.typeName = m_tree->AddStringFormatCached("%suint2", atomic);
							break;
						case HLSLBaseType_Uint3:
							type.typeName = m_tree->AddStringFormatCached("%suint3", atomic);
							break;
						case HLSLBaseType_Uint4:
							type.typeName = m_tree->AddStringFormatCached("%suint4", atomic);
							break;
						default:
							break;
						}
					}

					//int bufferRegister = nextBufferRegister + m_options.bufferRegisterOffset;
					//nextBufferRegister++;
					int bufferRegister = GetTextureRegister(buffer->cachedName);

					if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer)
					{
						CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d)", bufferRegister);
						AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName, true));
					}
					else if (buffer->type.baseType == HLSLBaseType_RasterizerOrderedStructuredBuffer)
					{
						CachedString bufferRegisterName = m_tree->AddStringFormatCached("buffer(%d), raster_order_group(%d)", bufferRegister, 0);
						AddClassArgument(new ClassArgument(buffer->cachedName, type, bufferRegisterName, true));
					}
					
				}

				m_RWStructuredBuffers.push_back(buffer);
			}
		}		
		else if (statement->nodeType == HLSLNodeType_TextureState)
		{
			HLSLTextureState* textureState = (HLSLTextureState *)statement;		

			bool isUsed = usedNames.HasString(textureState->cachedName.m_string);
			if (keepUnused || isUsed)
			{
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

					CachedString textureName;
					CachedString textureRegisterName;

					if (type.array)
					{
						type.baseType = HLSLBaseType_UserDefined;
						type.addressSpace = HLSLAddressSpace_Constant;
						type.typeName = m_tree->AddStringFormatCached("Uniforms_%s", RawStr(textureState->cachedName));

						HLSLBuffer * buffer = m_tree->AddNode<HLSLBuffer>(NULL, 0);
						buffer->field = m_tree->AddNode<HLSLDeclaration>(NULL, 0);

						HLSLDeclaration * declaration = buffer->field;

						HLSLType fieldType(HLSLBaseType_TextureState);
						fieldType.addressSpace = HLSLAddressSpace_Undefined;
						fieldType.baseType = textureState->type.baseType;
						fieldType.array = type.array;
						fieldType.arrayCount = type.arrayCount;
						declaration->cachedName = textureState->cachedName;
						declaration->type = fieldType;

						buffer->field->cachedName = m_tree->AddStringFormatCached("Textures");
						buffer->fileName = MakeCached("????");
						buffer->cachedName = textureState->cachedName;


						HLSLStatement* prevStatement = statement;
						prevStatement->hidden = true;

						HLSLStatement* prevsNextStatement = prevStatement->nextStatement;
						prevStatement->nextStatement = buffer;

						buffer->nextStatement = prevsNextStatement;
						statement = buffer;

						//int bufferRegister = nextBufferRegister + m_options.bufferRegisterOffset;
						//nextBufferRegister++;
						int bufferRegister = GetTextureRegister(buffer->cachedName);

						textureName = m_tree->AddStringFormatCached("%s", RawStr(textureState->cachedName));
						textureRegisterName = m_tree->AddStringFormatCached("buffer(%d)", bufferRegister);
						AddClassArgument(new ClassArgument(textureName, type, textureRegisterName));
					}
					else
					{
						//int textureRegister = nextTextureRegister + m_options.textureRegisterOffset;
						//nextTextureRegister++;
						int textureRegister = GetTextureRegister(textureState->cachedName);

						textureName = m_tree->AddStringFormatCached("%s", RawStr(textureState->cachedName));
						textureRegisterName = m_tree->AddStringFormatCached("texture(%d)", textureRegister);
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

					//int textureRegister = nextTextureRegister + m_options.textureRegisterOffset;
					//nextTextureRegister++;
					int textureRegister = GetTextureRegister(textureState->cachedName);

					CachedString textureName = m_tree->AddStringFormatCached("%s", RawStr(textureState->cachedName));

					CachedString textureRegisterName = m_tree->AddStringFormatCached("texture(%d), raster_order_group(%d)", textureRegister, 0);
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

					//int textureRegister = nextTextureRegister + m_options.textureRegisterOffset;
					//nextTextureRegister++;
					int textureRegister = GetTextureRegister(textureState->cachedName);
					CachedString textureName = m_tree->AddStringFormatCached("%s", RawStr(textureState->cachedName));

					CachedString textureRegisterName = m_tree->AddStringFormatCached("texture(%d)", textureRegister);
					AddClassArgument(new ClassArgument(textureName, type, textureRegisterName));
				}
			}
		}
		else if (statement->nodeType == HLSLNodeType_SamplerState)
		{
			HLSLSamplerState* samplerState = (HLSLSamplerState *)statement;

			bool isUsed = usedNames.HasString(samplerState->cachedName.m_string);
			if (keepUnused || isUsed)
			{
				HLSLType type(HLSLBaseType_SamplerState);
				type.addressSpace = HLSLAddressSpace_Undefined;

				//int samplerRegister = nextSamplerRegister++;
				int samplerRegister = GetSamplerRegister(samplerState->cachedName);

				CachedString samplerName = m_tree->AddStringFormatCached("%s", RawStr(samplerState->cachedName));

				type.typeName = samplerName;
				type.baseType = HLSLBaseType_SamplerState;

				type.array = samplerState->type.array;
				type.arraySize = samplerState->type.arraySize;
				type.arrayCount = samplerState->type.arrayCount;

				CachedString samplerRegisterName = m_tree->AddStringFormatCached("sampler(%d)", samplerRegister);
				AddClassArgument(new ClassArgument(samplerName, type, samplerRegisterName));

			}
			else if (statement->nodeType == HLSLNodeType_Function)
			{
				HLSLFunction* function = static_cast<HLSLFunction*>(statement);

				if ((String_Equal(function->name, m_entryName)) && m_target == Target_HullShader)
				{
					m_entryName = function->name = m_tree->AddStringFormatCached("VS%s", function->name);
				}
				else if (secondaryEntryFunction && m_target == Target_HullShader)
				{
					//assumes every entry function has same name
					if ((String_Equal(function->name, m_secondaryEntryName)))
					{
						m_secondaryEntryName = function->name = m_tree->AddStringFormatCached("HS%s", function->name);
					}
				}
			}
		}

		statement = statement->nextStatement;
	}

	// @@ IC: instance_id parameter must be a function argument. If we find it inside a struct we must move it to the function arguments
	// and patch all the references to it!
	{
		// Translate semantics.
		int attributeCounter = 0;

		eastl::vector < HLSLArgument * > srcArgumentVec = entryFunction->GetArguments();
		eastl::vector < HLSLArgument * > dstArgumentVec = {};

		int prevIndex = -1;
		for (int i = 0; i < srcArgumentVec.size(); i++)
		{
			HLSLArgument * argument = srcArgumentVec[i];
			if (argument->hidden)
			{
				// no change
				dstArgumentVec.push_back(argument);
				continue;
			}

			if (argument->modifier == HLSLArgumentModifier_Out)
			{
				// Translate output arguments semantics.
				if (argument->argType.baseType == HLSLBaseType_UserDefined)
				{
					// Our vertex input is a struct and its fields need to be tagged when we generate that
					HLSLStruct* structure = tree->FindGlobalStruct(argument->argType.typeName);
					if (structure == NULL)
					{
						Error("Vertex shader output struct %s' not found in shader\n", argument->argType.typeName);
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
				if (argument->argType.baseType == HLSLBaseType_UserDefined || argument->argType.baseType == HLSLBaseType_OutputPatch)
				{
					if (argument->argType.baseType == HLSLBaseType_OutputPatch)
					{

						argument->argType.structuredTypeName = m_tree->AddStringFormatCached("Domain_%s", RawStr(argument->argType.structuredTypeName));

						//inform maxVertex to attribute & Patch Identifier
						HLSLStatement* statement = root->statement;

						while (statement)
						{
							HLSLAttribute* pAttribute = statement->attributes;
							while (pAttribute != NULL)
							{
								if (pAttribute->attributeType == HLSLAttributeType_Domain)
								{
									pAttribute->maxVertexCount = argument->argType.maxPoints;
									pAttribute->patchIdentifier = argument->name;
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
								String_Equal(argument->argType.typeName, structure->name);
								break;
							}

							statement = statement->nextStatement;
						}

						HLSLStruct* structure = m_tree->AddNode<HLSLStruct>("", 0);
						structure->name = argument->argType.structuredTypeName;

						structure->field = m_tree->AddNode<HLSLStructField>("", 0);
						structure->field->nodeType = HLSLNodeType_PatchControlPoint;
						structure->field->type.baseType = HLSLBaseType_PatchControlPoint;
						structure->field->type.typeName = m_tree->AddStringFormatCached("patch_control_point<%s>", RawStr(argument->argType.typeName));

						structure->field->name = MakeCached("control_points");

						HLSLStatement* prevNextStatement = statement->nextStatement;

						statement->nextStatement = structure;
						structure->nextStatement = prevNextStatement;
					}

					// Our vertex input is a struct and its fields need to be tagged when we generate that
					HLSLStruct* structure = tree->FindGlobalStruct(argument->argType.typeName);
					if (structure == NULL)
					{
						Error("struct '%s' not found in shader\n", argument->argType.typeName);
					}

					HLSLStructField* field = structure->field;
					while (field != NULL)
					{
						if (!field->hidden)
						{
							int count = 1;
							field->type.array && m_tree->GetExpressionValue(field->type.arraySize, count);
							field->sv_semantic = TranslateInputSemantic(field->semantic, count);
						}
						field = field->nextField;
					}
				}
				else
				{
					int count = 1;
					argument->argType.array && m_tree->GetExpressionValue(argument->argType.arraySize, count);
					CachedString result = TranslateInputSemantic(argument->semantic, count);

					if (result.IsEmpty())
					{
						//remove this argument for Main function printing
						if (prevIndex < 0)
						{
							continue;
						}
						else
						{
							continue;
						}
					}
					else
					{
						argument->sv_semantic = result;
					}
				}
			}

			prevIndex = i;

			// add this one to the dstArgumentVec
			dstArgumentVec.push_back(argument);

			// if this is the last one, and we have a previous argument
			//if (argument == NULL && prevArgument != NULL)
			if (i == srcArgumentVec.size()-1 && prevIndex >= 0)
			{
				statement = root->statement;
				HLSLStatement* lastStatement = NULL;
				while (statement)
				{
					lastStatement = statement;
					statement = statement->nextStatement;
				}

				// create new arguments
				if (m_tree->NeedsExtension(USE_WaveGetLaneIndex))
				{
					HLSLArgument * simdArgument = m_tree->AddNode<HLSLArgument>("", 0);

					simdArgument->name = m_tree->AddStringCached("simd_lane_id");
					simdArgument->argType.baseType = HLSLBaseType_Uint;
					simdArgument->semantic = simdArgument->sv_semantic = m_tree->AddStringCached("thread_index_in_simdgroup");
					dstArgumentVec.push_back(simdArgument);
				}

				if (m_tree->NeedsExtension(USE_WaveGetLaneCount))
				{
					HLSLArgument * waveArgument = m_tree->AddNode<HLSLArgument>("", 0);
					waveArgument->name = m_tree->AddStringCached("simd_size");
					waveArgument->argType.baseType = HLSLBaseType_Uint;
					waveArgument->semantic = waveArgument->sv_semantic = m_tree->AddStringCached("threads_per_simdgroup");
					dstArgumentVec.push_back(waveArgument);
				}
			}

		}

		// set the vector
		entryFunction->argumentVec.resize(dstArgumentVec.size());
		for (int i = 0; i < dstArgumentVec.size(); i++)
		{
			entryFunction->argumentVec[i] = dstArgumentVec[i];
		}

		//entryFunction->firstArgument = (dstArgumentVec.size() > 0) ? dstArgumentVec[0] : NULL;
		for (int i = 0; i < dstArgumentVec.size(); i++)
		{
			HLSLArgument * currArg = dstArgumentVec[i];

			HLSLArgument * nextArg = NULL;
			if (i < dstArgumentVec.size()-1)
			{
				nextArg = dstArgumentVec[i+1];
			}

			//currArg->nextArgument = nextArg;
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

int MSLGenerator::GetBufferRegister(const CachedString & cachedName)
{
	if (m_options.bindingRequired)
	{
		for (int i = 0; i < m_options.bindingOverrides.size(); i++)
		{
			const BindingOverride & binding = m_options.bindingOverrides[i];
			if (String_Equal(binding.m_attrName.c_str(),RawStr(cachedName)))
			{
				// assume set is 0
				return binding.m_binding;
			}
		}

		ASSERT_PARSER(0);
	}
	else
	{
		int bufferRegister = m_nextBufferRegister + m_options.bufferRegisterOffset;
		m_nextBufferRegister++;
		return bufferRegister;
	}

	return -1;
}

int MSLGenerator::GetTextureRegister(const CachedString & cachedName)
{
	if (m_options.bindingRequired)
	{
		for (int i = 0; i < m_options.bindingOverrides.size(); i++)
		{
			const BindingOverride & binding = m_options.bindingOverrides[i];
			if (String_Equal(binding.m_attrName.c_str(), RawStr(cachedName)))
			{
				// assume set is 0
				return binding.m_binding;
			}
		}

		ASSERT_PARSER(0);
	}
	else
	{
		int textureRegister = m_nextTextureRegister + m_options.textureRegisterOffset;
		m_nextTextureRegister++;
		return textureRegister;
	}

	return -1;
}

int MSLGenerator::GetSamplerRegister(const CachedString & cachedName)
{
	if (m_options.bindingRequired)
	{
		for (int i = 0; i < m_options.bindingOverrides.size(); i++)
		{
			const BindingOverride & binding = m_options.bindingOverrides[i];
			if (String_Equal(binding.m_attrName.c_str(), RawStr(cachedName)))
			{
				// assume set is 0
				return binding.m_binding;
			}
		}

		ASSERT_PARSER(0);
	}
	else
	{
		int samplerRegister = m_nextSamplerRegister;// + m_options.samplerRegisterOffset; // I guess there is no sampler offset
		m_nextSamplerRegister++;
		return samplerRegister;
	}

	return -1;
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

static void WriteTextureIndexVariation(const eastl::string & funcName, HLSLBaseType texType, HLSLBaseType indexType)
{
	ASSERT_PARSER(HLSLBaseType_Texture <= texType && texType <= HLSLBaseType_RWTexture3D);
	ASSERT_PARSER(HLSLBaseType_FirstNumeric <= indexType && indexType <= HLSLBaseType_LastNumeric);

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
	m_writer.WriteLine(0, "");

	// 'mad' should be translated as 'fma'

	if (m_tree->NeedsFunction(MakeCached("mad")))
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


	if (m_tree->NeedsFunction(MakeCached("clip")))
	{
		m_writer.WriteLine(0, "inline void clip(float x) {");
		m_writer.WriteLine(1, "if (x < 0.0) discard_fragment();");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction(MakeCached("rcp")))
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

	if (m_tree->NeedsFunction(MakeCached("tex2D")) ||
		m_tree->NeedsFunction(MakeCached("tex2Dlod")) ||
		m_tree->NeedsFunction(MakeCached("tex2Dgrad")) ||
		m_tree->NeedsFunction(MakeCached("tex2Dbias")) ||
		m_tree->NeedsFunction(MakeCached("tex2Dfetch")))
	{
		m_writer.WriteLine(0, "struct Texture2DSampler {");
		m_writer.WriteLine(1, "const thread texture2d<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "Texture2DSampler(thread const texture2d<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");
	}

	if (m_tree->NeedsFunction(MakeCached("tex2D")))
	{
		m_writer.WriteLine(0, "inline float4 tex2D(Texture2DSampler ts, float2 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord);");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction(MakeCached("tex2Dlod")))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dlod(Texture2DSampler ts, float4 texCoordMip) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordMip.xy, level(texCoordMip.w));");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction(MakeCached("tex2Dgrad")))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dgrad(Texture2DSampler ts, float2 texCoord, float2 gradx, float2 grady) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord.xy, gradient2d(gradx, grady));");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction(MakeCached("tex2Dbias")))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dbias(Texture2DSampler ts, float4 texCoordBias) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordBias.xy, bias(texCoordBias.w));");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction(MakeCached("tex2Dfetch")))
	{
		m_writer.WriteLine(0, "inline float4 tex2Dfetch(Texture2DSampler ts, uint2 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.read(texCoord);");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction(MakeCached("tex3D")) ||
		m_tree->NeedsFunction(MakeCached("tex3Dlod")))
	{
		m_writer.WriteLine(0, "struct Texture3DSampler {");
		m_writer.WriteLine(1, "const thread texture3d<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "Texture3DSampler(thread const texture3d<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");
	}

	if (m_tree->NeedsFunction(MakeCached("tex3D")))
	{
		m_writer.WriteLine(0, "inline float4 tex3D(Texture3DSampler ts, float3 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord);");
		m_writer.WriteLine(0, "}");
	}
	if (m_tree->NeedsFunction(MakeCached("tex3Dlod")))
	{
		m_writer.WriteLine(0, "inline float4 tex3Dlod(Texture3DSampler ts, float4 texCoordMip) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordMip.xyz, level(texCoordMip.w));");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction(MakeCached("texCUBE")) ||
		m_tree->NeedsFunction(MakeCached("texCUBElod")) ||
		m_tree->NeedsFunction(MakeCached("texCUBEbias")))
	{
		m_writer.WriteLine(0, "struct TextureCubeSampler {");
		m_writer.WriteLine(1, "const thread texturecube<float>& t;");
		m_writer.WriteLine(1, "const thread sampler& s;");
		m_writer.WriteLine(1, "TextureCubeSampler(thread const texturecube<float>& t, thread const sampler& s) : t(t), s(s) {};");
		m_writer.WriteLine(0, "};");
	}

	if (m_tree->NeedsFunction(MakeCached("texCUBE")))
	{
		m_writer.WriteLine(0, "inline float4 texCUBE(TextureCubeSampler ts, float3 texCoord) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoord);");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction(MakeCached("texCUBElod")))
	{
		m_writer.WriteLine(0, "inline float4 texCUBElod(TextureCubeSampler ts, float4 texCoordMip) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordMip.xyz, level(texCoordMip.w));");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction(MakeCached("texCUBEbias")))
	{
		m_writer.WriteLine(0, "inline float4 texCUBEbias(TextureCubeSampler ts, float4 texCoordBias) {");
		m_writer.WriteLine(1, "return ts.t.sample(ts.s, texCoordBias.xyz, bias(texCoordBias.w));");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction(MakeCached("tex2Dcmp")))
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

	if (m_tree->NeedsFunction(MakeCached("tex2DMSfetch")))
	{
		m_writer.WriteLine(0, "inline float4 tex2DMSfetch(texture2d_ms<float> t, int2 texCoord, int sample) {");
		m_writer.WriteLine(1, "return t.read(uint2(texCoord), sample);");
		m_writer.WriteLine(0, "}");
	}

	if (m_tree->NeedsFunction(MakeCached("tex2DArray")))
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

	// find variations of index expressions
	{
		m_texIndexFuncName = MakeCached("TexIndexHelper");

		eastl::vector < HLSLBaseType > textureType;
		eastl::vector < HLSLBaseType > paramType;
		m_tree->FindTextureLoadOverloads(textureType,paramType);

		ASSERT_PARSER(textureType.size() == paramType.size());
		size_t numVariations = textureType.size();
		for (size_t variationIter = 0; variationIter < numVariations; variationIter++)
		{
			WriteTextureIndexVariation(m_texIndexFuncName.m_string,textureType[variationIter],paramType[variationIter]);
		}
	}

}

bool MSLGenerator::Generate(StringLibrary * stringLibrary, HLSLTree* tree, Target target, const char* entryName, const Options& options)
{
	m_firstClassArgument = NULL;
	m_lastClassArgument = NULL;
	m_stringLibrary = stringLibrary;

	m_tree = tree;
	m_target = target;
	m_entryName = MakeCached(entryName);
	m_secondaryEntryName = MakeCached(entryName);
	m_options = options;
	
	m_nextTextureRegister = 0;
	m_nextSamplerRegister = 0;
	m_nextBufferRegister = 0; // options start at 1


	if (m_target == Target::Target_VertexShader)
	{
		m_options.bufferRegisterOffset = 1;
	}

	m_writer.Reset();

	// Find entry point function
	HLSLFunction* entryFunction = tree->FindFunction(MakeCached(entryName));
	if (entryFunction == NULL)
	{
		Error("Entry point '%s' doesn't exist\n", entryName);
		return false;
	}

	// Find entry point function
	HLSLFunction* secondaryEntryFunction = NULL;
	if (m_target == Target_HullShader)
	{
		secondaryEntryFunction = tree->FindFunction(MakeCached(entryName), 1);
		if (secondaryEntryFunction == NULL)
		{
			Error("secondary Entry point '%s' doesn't exist\n", entryName);
			return false;
		}
	}
	/*
	// find/hide unused arguments
	{
		FindGlobalVisitor visitor;
		eastl::vector < HLSLArgument * > argVec = entryFunction->GetArguments();
		for (int i = 0; i < argVec.size(); i++)
		{
			HLSLArgument * arg = argVec[i];
			if (!visitor.FindArgument(arg->name, entryFunction))
			{
				printf("Hiding: %s\n", arg->name.m_string.c_str());
				arg->hidden = true;
			}
		}
	}
	*/


	Prepass(tree, target, entryFunction, secondaryEntryFunction);

	HideUnusedStatementDeclarations(entryFunction);

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
				m_writer.Write("%s* %s", RawStr(currentArg->type.typeName), RawStr(currentArg->name));
			else
				m_writer.Write("%s & %s", RawStr(currentArg->type.typeName), RawStr(currentArg->name));
		}
		else if (currentArg->type.baseType >= HLSLBaseType_FirstNumeric && currentArg->type.baseType <= HLSLBaseType_LastNumeric)
		{
			if (currentArg->bStructuredBuffer)
				m_writer.Write("%s* %s", RawStr(currentArg->type.typeName), RawStr(currentArg->name));
			else
				m_writer.Write("%s & %s", RawStr(currentArg->type.typeName), RawStr(currentArg->name));
		}
		else if (currentArg->type.baseType >= HLSLBaseType_Texture1D && currentArg->type.baseType <= HLSLBaseType_TextureCubeArray)
		{
			if (currentArg->type.typeName.IsEmpty())
			{
				if (currentArg->type.array)
				{
					m_writer.Write(0, "array<%s, %d> %s", RawStr(GetTypeName(currentArg->type)), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, RawStr(currentArg->name));
				}
				else
					m_writer.Write(0, "%s %s", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name));
			}
			else
				m_writer.Write(0, "constant %s::%s & %s", CHECK_CSTR(shaderClassName), RawStr(currentArg->type.typeName), RawStr(currentArg->name));
		}
		else if (currentArg->type.baseType >= HLSLBaseType_RWTexture1D && currentArg->type.baseType <= HLSLBaseType_RWTexture3D)
		{
			if (currentArg->type.typeName.IsEmpty())
			{
				if (currentArg->type.array)
				{
					m_writer.Write(0, "array<%s, %d> %s", RawStr(GetTypeName(currentArg->type)), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, RawStr(currentArg->name));
				}
				else
					m_writer.Write(0, "%s %s", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name));
			}
			else
				m_writer.Write(0, "constant %s::%s & %s", CHECK_CSTR(shaderClassName), RawStr(currentArg->type.typeName), RawStr(currentArg->name));
		}
		else if (currentArg->type.baseType == HLSLBaseType_SamplerState || currentArg->type.baseType == HLSLBaseType_Sampler)
		{			
			if (currentArg->type.array)
				m_writer.Write(0, "array_ref<sampler> %s", RawStr(currentArg->name));
			else
				m_writer.Write(0, "%s %s", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name));
		}
		else
		{
			m_writer.Write(0, "%s %s", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name));
		}

		bPreSkip = bSkip;

		prevArg = currentArg;
		currentArg = currentArg->nextArg;

		const ClassArgument* traversalArg = currentArg;

		while (traversalArg)
		{
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
		{
			m_writer.Write(0, "%s(%s)", RawStr(currentArg->name), RawStr(currentArg->name));

			prevArg = currentArg;
			currentArg = currentArg->nextArg;

			const ClassArgument* traversalArg = currentArg;

			while (traversalArg)
			{
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

	// create a separate struct for the different versions
	eastl::vector < HLSLArgument * > stageParams;
	eastl::vector < HLSLArgument * > resParams;

	// mainParams are the ones we will actually use in the main function, the extraParams
	// are the ones we are packing into a special little structure
	{
		eastl::vector < HLSLArgument * > argumentVec = entryFunction->GetArguments();

		for (int i = 0; i < argumentVec.size(); i++)
		{
			HLSLArgument * argument = argumentVec[i];

			if (argument->argType.baseType == HLSLBaseType_UserDefined)
			{
				stageParams.push_back(argument);
			}
			else
			{
				HLSLBaseType baseType = argument->argType.baseType;

				// check the type, if it's a scalar/vector, then we need to check the semantic. if it's
				// a texture/sampler/buffer it must be a mainParam
				bool isStandardType = false;
				if (IsMatrixType(baseType) || isScalarType(baseType) || isVectorType(baseType))
				{
					isStandardType = true;
				}

				bool isAttr = false;

				if (isStandardType)
				{
					if (argument->sv_semantic.IsNotEmpty())
					{
						// check the semantic
						eastl::string semantic = argument->sv_semantic.m_string;

						eastl::string attrBase = "attribute(";
						if (semantic.length() >= attrBase.length() + 2)
						{
							eastl::string subStr = semantic.substr(0,attrBase.length());

							isAttr = String_Equal(subStr.c_str(),attrBase.c_str());
						}
					}
				}

				if (isAttr)
				{
					stageParams.push_back(argument);
				}
				else
				{
					resParams.push_back(argument);
				}
			}
		}
	}

	if (stageParams.size() > 0)
	{
		m_writer.WriteLine(0,"");
		m_writer.WriteLine(0,"struct %s_input", entryName);
		m_writer.WriteLine(0, "{");

		for (int i = 0; i < stageParams.size(); i++)
		{
			HLSLArgument * argument = stageParams[i];

			if (argument->argType.baseType == HLSLBaseType_UserDefined)
			{
				// Our vertex input is a struct and its fields need to be tagged when we generate that
				HLSLStruct* structure = tree->FindGlobalStruct(argument->argType.typeName);
				if (structure == NULL)
				{
					Error("struct '%s' not found in shader\n", argument->argType.typeName);
				}

				HLSLStructField* field = structure->field;
				while (field != NULL)
				{
					if (!field->hidden && field->semantic.IsNotEmpty())
					{
						if (field->type.array)
						{
							int count = 1;
							m_tree->GetExpressionValue(field->type.arraySize, count);
							int base;
							bool isAttr = sscanf(RawStr(field->sv_semantic), "attribute(%d)", &base) == 1;
							for (int i = 0; i < count; ++i)
							{
								if (isAttr)
									m_writer.WriteLine(1, "%s %s_%d [[attribute(%d)]];", RawStr(GetTypeName(field->type)), RawStr(field->semantic), i, base+i);
								else
									m_writer.WriteLine(1, "%s %s_%d;", RawStr(GetTypeName(field->type)), RawStr(field->semantic), i);
							}
						}
						else
						{
							if (field->sv_semantic.IsNotEmpty())
								m_writer.WriteLine(1, "%s %s [[%s]];", RawStr(GetTypeName(field->type)), RawStr(field->semantic), RawStr(field->sv_semantic));
							else
								m_writer.WriteLine(1, "%s %s;", RawStr(GetTypeName(field->type)), RawStr(field->semantic));
						}
					}
					field = field->nextField;
				}
			}
			else
			{
				if (argument->sv_semantic.IsNotEmpty())
					m_writer.WriteLine(1,"%s %s [[%s]];", RawStr(GetTypeName(argument->argType)), RawStr(argument->semantic), RawStr(argument->sv_semantic));
				else
					m_writer.WriteLine(1,"%s %s;", RawStr(GetTypeName(argument->argType)), RawStr(argument->semantic));
			}

			//m_writer.WriteLine(1,"%s %s [[%s]];",
			//	RawStr(GetTypeName(argument->argType)), RawStr(argument->name),
			//	RawStr(argument->sv_semantic));
		}
		m_writer.WriteLine(0, "};");
		m_writer.WriteLine(0, "");
	}

	// If function return value has a non-color output semantic, declare a temporary struct for the output.
	bool wrapReturnType = false;
	if (entryFunction->sv_semantic.IsNotEmpty() && String_Equal(entryFunction->sv_semantic, "color(0)") != 0)
	{
		wrapReturnType = true;

		m_writer.WriteLine(0, "struct %s_output { %s tmp [[%s]]; };", entryName, RawStr(GetTypeName(entryFunction->returnType)), RawStr(entryFunction->sv_semantic));

		m_writer.WriteLine(0, "");
	}
	else if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
	{
		// Our vertex input is a struct and its fields need to be tagged when we generate that
		HLSLStruct* structure = tree->FindGlobalStruct(entryFunction->returnType.typeName);
		if (structure == NULL)
		{
			Error("struct '%s' not found in shader\n", entryFunction->returnType.typeName);
		}

		m_writer.WriteLine(0, "struct %s_output", entryName);
		m_writer.WriteLine(0, "{");
		//int attrCounter = 0;
		HLSLStructField* field = structure->field;
		while (field != NULL)
		{
			if (!field->hidden && field->semantic.IsNotEmpty())
			{
				if (field->type.array)
				{
					int count = 1;
					m_tree->GetExpressionValue(field->type.arraySize, count);
					for (int i = 0; i < count; ++i)
					{
						if (field->sv_semantic.IsNotEmpty())
							m_writer.WriteLine(1, "%s %s_%d [[%s]];", RawStr(GetTypeName(field->type)), RawStr(field->semantic), i, RawStr(field->sv_semantic));
						else
							m_writer.WriteLine(1, "%s %s_%d;", RawStr(GetTypeName(field->type)), RawStr(field->semantic), i);
					}
				}
				else
				{
					if (field->sv_semantic.IsNotEmpty())
						m_writer.WriteLine(1, "%s %s [[%s]];", RawStr(GetTypeName(field->type)), RawStr(field->semantic), RawStr(field->sv_semantic));
					else
						m_writer.WriteLine(1, "%s %s;", RawStr(GetTypeName(field->type)), RawStr(field->semantic));
				}
			}
			field = field->nextField;
		}
		m_writer.WriteLine(0, "};");
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
		m_writer.Write("%s_output", CHECK_CSTR(entryName));
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
				m_writer.Write("%s_output", CHECK_CSTR(entryName));
			else
				m_writer.Write("%s", RawStr(GetTypeName(entryFunction->returnType)));

		}
		//hull should return void
		else if (m_target == Target_HullShader)
		{
			m_writer.Write("void");
		}
		else if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
		{
			m_writer.Write("%s_output", CHECK_CSTR(entryName));
		}
		else
		{
			if (entryFunction->returnType.baseType == HLSLBaseType_PatchControlPoint)
				m_writer.Write("%s::", CHECK_CSTR(shaderClassName));

			m_writer.Write("%s", RawStr(GetTypeName(entryFunction->returnType)));
		}
	}

	m_writer.Write(" stageMain(\n");

	//int argumentCount = 0;

	bool bFirstStruct = false;
	//CachedString firstTypeName;

	eastl::vector < HLSLArgument * > argumentVec = entryFunction->GetArguments();

	int numWritten = 0;

	if (stageParams.size() >= 1)
	{
		m_writer.Write("\t%s_input inputData [[stage_in]]", entryName);
	}

	//for (int i = 0; i < argumentVec.size(); i++)
	for (int i = 0; i < resParams.size(); i++)
	{
		HLSLArgument * argument = resParams[i];

		if (!argument->hidden)
		{
			if (numWritten >= 1)
			{
				m_writer.Write(",\n");
			}

			if (argument->argType.baseType == HLSLBaseType_UserDefined || argument->argType.baseType == HLSLBaseType_OutputPatch)
			{
				if (m_target == Target_HullShader)
					m_writer.Write(1,"constant %s::", CHECK_CSTR(shaderClassName));
				else
					m_writer.Write(1, "%s::", CHECK_CSTR(shaderClassName));
			}

			if (m_target == Target_HullShader)
				m_writer.Write("%s* %s", RawStr(GetTypeName(argument->argType)), RawStr(argument->name));
			else
			{
				m_writer.Write("%s %s", RawStr(GetTypeName(argument->argType)), RawStr(argument->name));
			}

			if (argument->argType.baseType == HLSLBaseType_UserDefined || argument->argType.baseType == HLSLBaseType_OutputPatch)
			{
				if (m_target == Target_HullShader)
					m_writer.Write(" [[buffer(0)]]"); // not sure about this
				else
					m_writer.Write(" [[stage_in]]");

			}
			else if (argument->sv_semantic.IsNotEmpty())
			{
				m_writer.Write(" [[%s]]", RawStr(argument->sv_semantic));
			}

#if 0
			// @@ IC: We are assuming that the first argument (x) -> first struct (o) is the 'stage_in'. 
			// this really doesn't seem robust, disabling for now, enabling it later if we need to
			if (bFirstStruct == false && argument->argType.baseType == HLSLBaseType_UserDefined || argument->argType.baseType == HLSLBaseType_OutputPatch)
			{
				bFirstStruct = true;
				firstTypeName = argument->argType.typeName;

				if (m_target == Target_HullShader)
					m_writer.Write(" [[buffer(0)]]");
				else
					m_writer.Write(" [[stage_in]]");

			}
			else if (argument->sv_semantic.IsNotEmpty())
			{
				m_writer.Write(" [[%s]]", RawStr(argument->sv_semantic));
			}
#endif
		}
		else
		{
			continue;
		}

		numWritten++;
	}

	currentArg = m_firstClassArgument;

	prevArg = NULL;

	bSkip = false;
	bPreSkip = false;
	
	bool bIntent = false;

	while (currentArg != NULL)
	{
		ClassArgument* nextArg = currentArg->nextArg;		

		if (nextArg)
		{
			if (!bSkip)
				m_writer.Write(",\n");
		}
		else if(nextArg)
		{
			if (!bSkip)
				m_writer.Write(",\n");			
		}
		else if (nextArg == NULL)
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
				m_writer.Write(bIntent ? 0 : 1, "%s::%s* %s [[%s]]", CHECK_CSTR(shaderClassName), RawStr(currentArg->type.typeName), RawStr(currentArg->name), RawStr(currentArg->registerName));
			else
				m_writer.Write(bIntent ? 0 : 1, "%s::%s & %s [[%s]]", CHECK_CSTR(shaderClassName), RawStr(currentArg->type.typeName), RawStr(currentArg->name), RawStr(currentArg->registerName));

		}
		else if (currentArg->type.baseType >= HLSLBaseType_FirstNumeric && currentArg->type.baseType <= HLSLBaseType_LastNumeric)
		{
			if (currentArg->bStructuredBuffer)
				m_writer.Write("%s* %s [[%s]]", RawStr(currentArg->type.typeName), RawStr(currentArg->name), RawStr(currentArg->registerName));
			else
				m_writer.Write("%s & %s [[%s]]", RawStr(currentArg->type.typeName), RawStr(currentArg->name), RawStr(currentArg->registerName));
		}
		else if (currentArg->type.baseType >= HLSLBaseType_Texture1D && currentArg->type.baseType <= HLSLBaseType_TextureCubeArray)
		{
			if (currentArg->type.typeName.IsEmpty())
			{
				if (currentArg->type.array)
					m_writer.Write(bIntent ? 0 : 1, "array<%s, %d> %s [[%s]]", RawStr(GetTypeName(currentArg->type)), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, RawStr(currentArg->name), RawStr(currentArg->registerName));
				else
					m_writer.Write(bIntent ? 0 : 1, "%s %s [[%s]]", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name), RawStr(currentArg->registerName));
			}
			else
			{
				if (currentArg->type.baseType == HLSLBaseType_UserDefined)
				{
					m_writer.Write(bIntent ? 0 : 1, "constant %s::%s & %s [[%s]]", CHECK_CSTR(shaderClassName), RawStr(currentArg->type.typeName), RawStr(currentArg->name), RawStr(currentArg->registerName));
				}
				else
				{
					m_writer.Write(bIntent ? 0 : 1, "constant %s & %s [[%s]]", RawStr(currentArg->type.typeName), RawStr(currentArg->name), RawStr(currentArg->registerName));
				}
			}
		}
		else if (currentArg->type.baseType >= HLSLBaseType_RWTexture1D && currentArg->type.baseType <= HLSLBaseType_RWTexture3D)
		{
			if (currentArg->type.typeName.IsEmpty())
			{
				if (currentArg->type.array)
				{
					m_writer.Write(bIntent ? 0 : 1, "array<%s, %d> %s [[%s]]", RawStr(GetTypeName(currentArg->type)), currentArg->type.arrayCount == 0 ? DEFAULT_TEXTURE_COUNT : currentArg->type.arrayCount, RawStr(currentArg->name), RawStr(currentArg->registerName));
				}
				else
					m_writer.Write(bIntent ? 0 : 1, "%s %s [[%s]]", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name), RawStr(currentArg->registerName));
			}
			else
				m_writer.Write(bIntent ? 0 : 1, "constant %s::%s & %s [[%s]]", CHECK_CSTR(shaderClassName), RawStr(currentArg->type.typeName), RawStr(currentArg->name), RawStr(currentArg->registerName));
		}
		else if (currentArg->type.baseType == HLSLBaseType_SamplerState || currentArg->type.baseType == HLSLBaseType_Sampler)
		{
			if (currentArg->type.array)
			{
				m_writer.Write(bIntent ? 0 : 1, "array<sampler, ");
				OutputExpression(currentArg->type.arraySize, NULL, NULL, NULL, false);
				m_writer.Write(0, "> %s [[%s]]", RawStr(currentArg->name), RawStr(currentArg->registerName));
			}
			else
				m_writer.Write(bIntent ? 0 : 1, "%s %s [[%s]]", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name), RawStr(currentArg->registerName));
		}
		else
		{
			m_writer.Write(1, "%s %s [[%s]]", RawStr(GetTypeName(currentArg->type)), RawStr(currentArg->name), RawStr(currentArg->registerName));
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
					m_writer.Write(1, "device %s::%s* %s [[%s]]", CHECK_CSTR(shaderClassName), RawStr(GetTypeName(function->returnType)), CHECK_CSTR("tessellationFactorBuffer"), CHECK_CSTR("buffer(10)") /*Temporary!!!*/);
				}
				else if (String_Equal(function->name, m_secondaryEntryName))
				{
					m_writer.Write(",\n");
					m_writer.Write(1, "device %s::%s* %s [[%s]]", CHECK_CSTR(shaderClassName), RawStr(function->returnType.typeName), CHECK_CSTR("hullOutputBuffer"), CHECK_CSTR("buffer(11)") /*Temporary!!!*/);
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
	//argumentVec = entryFunction->GetArguments();

	{
		for (int i = 0; i < resParams.size(); i++)
		{
			HLSLArgument * argument = resParams[i];
			m_writer.BeginLine(1);

			if (argument->hidden)
			{
				continue;
			}

			/*
			if (firstTypeName == argument->argType.typeName && m_target == Target_HullShader)
			{
				continue;
			}
			*/

			// I guess we'll say that all user defined types need this?
			if (argument->argType.baseType == HLSLBaseType_UserDefined)
			{
				m_writer.Write("%s::", CHECK_CSTR(shaderClassName));
			}

			CachedString newArgumentName = m_tree->AddStringFormatCached("%s0", RawStr(argument->name));


			if (argument->argType.baseType == HLSLBaseType_OutputPatch)
			{
			}
			else
			{
				OutputDeclaration(argument->argType, newArgumentName, NULL, NULL);
				m_writer.EndLine(";");
			}

			if (!((argument->modifier == HLSLArgumentModifier_Out) || (argument->modifier == HLSLArgumentModifier_Inout)))
			{
				// Set the value for the local variable.
				if (argument->argType.baseType == HLSLBaseType_UserDefined || argument->argType.baseType == HLSLBaseType_OutputPatch)
				{
					HLSLStruct* structDeclaration = tree->FindGlobalStruct(argument->argType.typeName);
					ASSERT_PARSER(structDeclaration != NULL);

					if (argument->argType.array)
					{

						if (argument->argType.baseType == HLSLBaseType_OutputPatch)
						{
							//m_writer.WriteLine(1, "%s = %s;", newArgumentName, argument->name);
						}
						else
						{
							int arraySize = 0;
							m_tree->GetExpressionValue(argument->argType.arraySize, arraySize);

							for (int i = 0; i < arraySize; i++)
							{
								HLSLStructField* field = structDeclaration->field;

								while (field != NULL)
								{
									if (field->semantic.IsNotEmpty())
									{
										CachedString builtInSemantic = GetBuiltInSemantic(field->semantic, HLSLArgumentModifier_In);
										if (builtInSemantic.IsNotEmpty())
										{
											m_writer.WriteLine(1, "%s[%d].%s = %s[%d];", RawStr(newArgumentName), i, RawStr(field->name), RawStr(builtInSemantic), i);
										}
										else
										{
											m_writer.WriteLine(1, "%s[%d].%s = %s[%d];", RawStr(newArgumentName), i, RawStr(field->name), RawStr(field->semantic), i);
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
							if (field->semantic.IsNotEmpty())
							{
								CachedString builtInSemantic = GetBuiltInSemantic(field->semantic, HLSLArgumentModifier_In, argument->name, field->name);
								if (builtInSemantic.IsNotEmpty())
								{
									m_writer.WriteLine(1, "%s.%s = %s;", RawStr(newArgumentName), RawStr(field->name), RawStr(builtInSemantic));
								}
								else
								{
									if (field->type.array)
									{
										int count = 1;
										m_tree->GetExpressionValue(field->type.arraySize, count);
										for (int i=0; i < count; ++i)
										{
											m_writer.WriteLine(1, "%s.%s[%d] = %s.%s%d;", RawStr(newArgumentName), RawStr(field->name), i, RawStr(argument->name), RawStr(field->name), i);
										}
									}
									else
										m_writer.WriteLine(1, "%s.%s = %s.%s;", RawStr(newArgumentName), RawStr(field->name), RawStr(argument->name), RawStr(field->name));
								}
							}
							field = field->nextField;
						}
					}


				}
				else if (argument->semantic.IsNotEmpty())
				{
					//CachedString builtInSemantic = GetBuiltInSemantic(argument->semantic, HLSLArgumentModifier_In);
					// should this next comment be back in?
					/*
					if (builtInSemantic)
					{
						m_writer.WriteLine(1, "%s = %s;", newArgumentName, builtInSemantic);
					}
					else
					*/
					{
						m_writer.WriteLine(1, "%s = %s;", RawStr(newArgumentName), RawStr(argument->name));
					}
				}
			}

			//argument = argument->nextArgument;
		}
	}

	// grab extra params so we can pass them in
	{
		for (int paramIter = 0; paramIter < stageParams.size(); paramIter++)
		{
			HLSLArgument * argument = stageParams[paramIter];

			if (argument->hidden)
			{
				continue;
			}

			if (argument->argType.baseType == HLSLBaseType_UserDefined)
			{
				m_writer.WriteLine(1, "%s::%s %s0;", CHECK_CSTR(shaderClassName), RawStr(GetTypeName(argument->argType)), RawStr(argument->name));
				// Our vertex input is a struct and its fields need to be tagged when we generate that
				HLSLStruct* structure = tree->FindGlobalStruct(argument->argType.typeName);
				if (structure == NULL)
				{
					Error("struct '%s' not found in shader\n", argument->argType.typeName);
				}

				HLSLStructField* field = structure->field;
				while (field != NULL)
				{
					if (!field->hidden && field->semantic.IsNotEmpty())
					{
						if (field->type.array)
						{
							int count = 1;
							m_tree->GetExpressionValue(field->type.arraySize, count);
							int base;
							bool isAttr = sscanf(RawStr(field->sv_semantic), "attribute(%d)", &base) == 1;
							for (int i = 0; i < count; ++i)
							{
								m_writer.WriteLine(1, "%s0.%s[%d] = inputData.%s_%d;", RawStr(argument->name), RawStr(field->name), i, RawStr(field->semantic), i);
							}
						}
						else
						{
							m_writer.WriteLine(1, "%s0.%s = inputData.%s;", RawStr(argument->name), RawStr(field->name), RawStr(field->semantic));
						}
					}
					field = field->nextField;
				}
			}
			else
			{
				m_writer.WriteLine(1, "%s %s0 = inputData.%s;",
					RawStr(GetTypeName(argument->argType)),
					RawStr(argument->name),
					RawStr(argument->semantic));
			}
		}
	}

	bSkip = false;
	bPreSkip = false;

	// Create the helper class instance and call the entry point from the original shader
	m_writer.BeginLine(1);
	m_writer.Write("%s %s", CHECK_CSTR(shaderClassName), CHECK_CSTR(entryName));

	currentArg = m_firstClassArgument;
	
	int arg_counter = 0;
	if (currentArg)
	{
		m_writer.Write("(\n");

		prevArg = NULL;

		while (currentArg != NULL)
		{
			ClassArgument* nextArg = currentArg->nextArg;

			if (nextArg)
			{
				if (!bSkip && (m_firstClassArgument != currentArg))
					m_writer.Write(",\n");
			}
			else if (nextArg)
			{
				if (!bSkip)
				{
					m_writer.Write(",\n");
				}
			}
			else if (nextArg == NULL)
			{
				if (!bSkip && (m_firstClassArgument != currentArg))
					m_writer.Write(",\n");
			}


			bPreSkip = bSkip;

			bIntent = false;

				
			{
				m_writer.Write(1, "%s", RawStr(currentArg->name));
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
		m_writer.Write("%s::%s VS_Out = %s.%s(", CHECK_CSTR(shaderClassName), RawStr(entryFunction->returnType.typeName), CHECK_CSTR(entryName), RawStr(entryFunction->name));

		eastl::vector < HLSLArgument * > argumentVec = entryFunction->GetArguments();

		int numWritten = 0;
		for (int i = 0; i < argumentVec.size(); i++)
		{
			HLSLArgument * argument = argumentVec[i];
			if (!argument->hidden)
			{
				if (numWritten >= 1)
				{
					m_writer.Write(", ");
				}
				CachedString newArgumentName = m_tree->AddStringFormatCached("%s0", RawStr(argument->name));

				if (argument->argType.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
				{
					if (argument->argType.baseType == HLSLBaseType_UserDefined)
						m_writer.Write("%s[threadId]", RawStr(argument->name));
					else
						m_writer.Write("%s", RawStr(argument->name));
				}
				else
					m_writer.Write("%s", RawStr(newArgumentName));
			}
			else
			{
				continue;
			}
			numWritten++;
		}

		m_writer.EndLine(");");

		m_writer.BeginLine(2);
		m_writer.Write("%s::%s HS_Out = %s.%s(", CHECK_CSTR(shaderClassName), RawStr(secondaryEntryFunction->returnType.typeName), CHECK_CSTR(entryName), RawStr(secondaryEntryFunction->name));

		argumentVec = secondaryEntryFunction->GetArguments();
		numWritten = 0;
		for (int i = 0; i < argumentVec.size(); i++)
		{
			HLSLArgument * argument = argumentVec[i];
			if (!argument->hidden)
			{
				if (numWritten >= 1)
				{
					m_writer.Write(", ");
				}

				CachedString newArgumentName = m_tree->AddStringFormatCached("%s0", RawStr(argument->name));

				if (argument->argType.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
				{
					if (argument->argType.baseType == HLSLBaseType_InputPatch && String_Equal(entryFunction->returnType.typeName, argument->argType.structuredTypeName))
					{
						m_writer.Write("&VS_Out ");
					}
					else if (String_Equal(argument->semantic, "SV_PrimitiveID") || String_Equal(argument->semantic, "SV_OutputControlPointID"))
					{
						m_writer.Write("0");
					}
					else
						m_writer.Write("%s", RawStr(argument->name));
				}
				else
					m_writer.Write("%s", RawStr(newArgumentName));
			}
			else
			{
				continue;
			}

			numWritten++;
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
			m_writer.Write("%s::%s Patch_Out = %s.%s(", CHECK_CSTR(shaderClassName), RawStr(PCF->returnType.typeName), CHECK_CSTR(entryName), RawStr(PCF->name));

			eastl::vector < HLSLArgument * > argumentVec = PCF->GetArguments();
			int numWritten = 0;
			for (int i = 0; i < argumentVec.size(); i++)
			{
				HLSLArgument * argument = argumentVec[i];

				if (!argument->hidden)
				{
					if (numWritten >= 1)
					{
						m_writer.Write(", ");
					}

					CachedString newArgumentName = m_tree->AddStringFormatCached("%s0", RawStr(argument->name));

					if (argument->argType.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
					{
						if (argument->argType.baseType == HLSLBaseType_InputPatch && String_Equal(entryFunction->returnType.typeName, argument->argType.structuredTypeName))
						{
							m_writer.Write("&VS_Out ");
						}
						else if (String_Equal(argument->semantic, "SV_PrimitiveID") || String_Equal(argument->semantic, "SV_OutputControlPointID"))
						{
							m_writer.Write("0");
						}
						else
							m_writer.Write("%s", RawStr(argument->name));
					}
					else
						m_writer.Write("%s", RawStr(newArgumentName));
				}
				else
				{
					continue;
				}

				numWritten++;
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
		if (wrapReturnType)
		{
			m_writer.Write(1, "%s_output output; output.tmp = %s.%s(", CHECK_CSTR(entryName), CHECK_CSTR(entryName), CHECK_CSTR(entryName));
		}
		else if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
		{
			m_writer.Write(1, "%s::%s result = %s.%s(", CHECK_CSTR(shaderClassName), RawStr(entryFunction->returnType.typeName), CHECK_CSTR(entryName), CHECK_CSTR(entryName));
		}
		else
		{
			m_writer.Write(1, "return %s.%s(", CHECK_CSTR(entryName), CHECK_CSTR(entryName));
		}

		eastl::vector < HLSLArgument * > argumentVec = entryFunction->GetArguments();
		int numWritten = 0;
		for (int i = 0; i < argumentVec.size(); i++)
		{
			HLSLArgument * argument = argumentVec[i];

			if (!argument->hidden)
			{
				if (numWritten >= 1)
				{
					m_writer.Write(", ");
				}
				CachedString newArgumentName = m_tree->AddStringFormatCached("%s0", RawStr(argument->name));

				if (argument->argType.baseType == HLSLBaseType_OutputPatch || m_target == Target_HullShader)
					m_writer.Write("%s", RawStr(argument->name));
				else
					m_writer.Write("%s", RawStr(newArgumentName));
			}
			else
			{
				continue;
			}

			numWritten++;
		}

		m_writer.EndLine(");");

		if (wrapReturnType)
		{
			m_writer.WriteLine(1, "return output;");
		}
		else if (entryFunction->returnType.baseType == HLSLBaseType_UserDefined)
		{
			// Our vertex input is a struct and its fields need to be tagged when we generate that
			HLSLStruct* structure = tree->FindGlobalStruct(entryFunction->returnType.typeName);
			if (structure == NULL)
			{
				Error("struct '%s' not found in shader\n", entryFunction->returnType.typeName);
			}

			m_writer.WriteLine(1, "%s_output output;", CHECK_CSTR(entryName));
			HLSLStructField* field = structure->field;
			while (field != NULL)
			{
				if (!field->hidden && field->semantic.IsNotEmpty())
				{
					if (field->type.array)
					{
						int count = 1;
						m_tree->GetExpressionValue(field->type.arraySize, count);
						for (int i = 0; i < count; ++i)
						{
							m_writer.WriteLine(1, "output.%s_%d = result.%s[%d];", RawStr(field->semantic), i, RawStr(field->name), i);
						}
					}
					else
					{
						m_writer.WriteLine(1, "output.%s = result.%s;",  RawStr(field->semantic), RawStr(field->name));
					}
				}
				field = field->nextField;
			}
			m_writer.WriteLine(1, "return output;");
		}
	}

	m_writer.WriteLine(0, "}");



	CleanPrepass();
	m_tree = NULL;

	// Any final check goes here, but shouldn't be needed as the Metal compiler is solid

	return !m_error;
}

CachedString MSLGenerator::GetBuiltInSemantic(const CachedString & semantic, HLSLArgumentModifier modifier, const CachedString & argument, const CachedString & field)
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
		CachedString newArgument = m_tree->AddStringFormatCached("float4(%s.%s.xyz, 1.0 / %s.%s.w)", RawStr(argument), RawStr(field), RawStr(argument), RawStr(field));
		return newArgument;
	}

	return CachedString();
}

CachedString MSLGenerator::MakeCached(const char * str)
{
	CachedString ret = m_tree->AddStringCached(str);
	return ret;
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
			m_writer.BeginLine(indent, RawStr(declaration->fileName), declaration->line);
			OutputDeclaration(declaration, function);
			m_writer.EndLine(";");
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
					m_writer.WriteLine(indent, "array<%s, %d> %s;", RawStr(GetTypeName(type)), textureState->arrayIndex[0] == 0 ? DEFAULT_TEXTURE_COUNT : textureState->arrayIndex[0], RawStr(textureState->cachedName));
				}
				else
				{
					m_writer.BeginLine(indent, RawStr(textureState->fileName), textureState->line);

					m_writer.Write("%s", RawStr(GetTypeName(type)));
					m_writer.Write(" %s", RawStr(textureState->cachedName));

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
					m_writer.WriteLine(indent, "struct %sData", RawStr(textureState->cachedName));
					m_writer.WriteLine(indent, "{");

					for (int i = 0; i < (int)textureState->arrayDimension; i++)
					{
						m_writer.WriteLine(indent + 1, "array<%s, %d> textures;", RawStr(GetTypeName(type)), textureState->arrayIndex[i]);
					}
					m_writer.WriteLine(indent, "}");
				}
				else
				{
					m_writer.BeginLine(indent, RawStr(textureState->fileName), textureState->line);

					m_writer.Write("%s", RawStr(GetTypeName(type)));
					m_writer.Write(" %s", RawStr(textureState->cachedName));

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
					m_writer.WriteLine(indent, "struct %sData", RawStr(textureState->cachedName));
					m_writer.WriteLine(indent, "{");

					for (int i = 0; i < (int)textureState->arrayDimension; i++)
					{
						m_writer.WriteLine(indent + 1, "array<%s, %d> textures;", RawStr(GetTypeName(type)), textureState->arrayIndex[i]);
					}
					m_writer.WriteLine(indent, "}");
				}
				else
				{
					m_writer.BeginLine(indent, RawStr(textureState->fileName), textureState->line);

					m_writer.Write("%s", RawStr(GetTypeName(type)));
					m_writer.Write(" %s", RawStr(textureState->cachedName));

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

			m_writer.BeginLine(indent, RawStr(samplerState->fileName), samplerState->line);

			if (samplerState->bStructured)
			{
				m_writer.WriteLineTagged(indent, RawStr(samplerState->fileName), samplerState->line, "SamplerState %s {", RawStr(samplerState->cachedName));
				HLSLSamplerStateExpression* expression = samplerState->expression;

				while (expression != NULL)
				{
					if (!expression->hidden)
					{
						m_writer.BeginLine(indent + 1, RawStr(expression->fileName), expression->line);

						m_writer.Write("%s = %s", RawStr(expression->lvalue), RawStr(expression->rvalue));
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
					m_writer.Write("array_ref<sampler> %s", RawStr(samplerState->cachedName));
				}
				else
				{
					m_writer.Write("sampler %s", RawStr(samplerState->cachedName));
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

				m_writer.WriteLine(indent, "constant %s* %s;", buffer->type.elementType != HLSLBaseType_Unknown ? getElementTypeAsStr(m_stringLibrary, buffer->type) : RawStr(GetTypeName(tempType)), RawStr(buffer->cachedName));
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
					m_writer.Write("%sfloat*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Float2:
					m_writer.Write("%sfloat2*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Float3:
					m_writer.Write("%sfloat3*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Float4:
					m_writer.Write("%sfloat4*", CHECK_CSTR(atomic));
					break;

				case HLSLBaseType_Half:
					m_writer.Write("%shalf*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Half2:
					m_writer.Write("%shalf2*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Half3:
					m_writer.Write("%shalf3*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Half4:
					m_writer.Write("%shalf4*", CHECK_CSTR(atomic));
					break;

				case HLSLBaseType_Min16Float:
					m_writer.Write("%shalf*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Min16Float2:
					m_writer.Write("%shalf2*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Min16Float3:
					m_writer.Write("%shalf3*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Min16Float4:
					m_writer.Write("%shalf4*", CHECK_CSTR(atomic));
					break;

				case HLSLBaseType_Min10Float:
					m_writer.Write("%shalf*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Min10Float2:
					m_writer.Write("%shalf2*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Min10Float3:
					m_writer.Write("%shalf3*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Min10Float4:
					m_writer.Write("%shalf4*", CHECK_CSTR(atomic));
					break;

				case HLSLBaseType_Bool:
					m_writer.Write("%sbool*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Bool2:
					m_writer.Write("%sbool2*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Bool3:
					m_writer.Write("%sbool3*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Bool4:
					m_writer.Write("bool4*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Int:
					m_writer.Write("%sint*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Int2:
					m_writer.Write("%sint2*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Int3:
					m_writer.Write("%sint3*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Int4:
					m_writer.Write("%sint4*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Uint:
					m_writer.Write("%suint*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Uint2:
					m_writer.Write("%suint2*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Uint3:
					m_writer.Write("%suint3*", CHECK_CSTR(atomic));
					break;
				case HLSLBaseType_Uint4:
					m_writer.Write("%suint4*", CHECK_CSTR(atomic));
					break;
				default:
					break;
				}

				m_writer.Write(" %s", RawStr(buffer->cachedName));
				m_writer.EndLine(";");
			}
			else if (buffer->type.baseType == HLSLBaseType_RWStructuredBuffer)
			{

				HLSLDeclaration* field = buffer->field;

				m_writer.Write(indent, CHECK_CSTR("device "));

				if (buffer->type.elementType)
				{
					m_writer.Write("%s*", CHECK_CSTR(getElementTypeAsStr(m_stringLibrary, buffer->type)));
				}
				else
				{
					const char* atomic = "";

					if (buffer->bAtomic)
						atomic = "atomic_";

					switch (buffer->type.elementType)
					{
					case HLSLBaseType_Float:
						m_writer.Write("%sfloat*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Float2:
						m_writer.Write("%sfloat2*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Float3:
						m_writer.Write("%sfloat3*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Float4:
						m_writer.Write("%sfloat4*", CHECK_CSTR(atomic));
						break;

					case HLSLBaseType_Half:
						m_writer.Write("%shalf*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Half2:
						m_writer.Write("%shalf2*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Half3:
						m_writer.Write("%shalf3*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Half4:
						m_writer.Write("%shalf4*", CHECK_CSTR(atomic));
						break;

					case HLSLBaseType_Min16Float:
						m_writer.Write("%shalf*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Min16Float2:
						m_writer.Write("%shalf2*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Min16Float3:
						m_writer.Write("%shalf3*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Min16Float4:
						m_writer.Write("%shalf4*", CHECK_CSTR(atomic));
						break;

					case HLSLBaseType_Min10Float:
						m_writer.Write("%shalf*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Min10Float2:
						m_writer.Write("%shalf2*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Min10Float3:
						m_writer.Write("%shalf3*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Min10Float4:
						m_writer.Write("%shalf4*", CHECK_CSTR(atomic));
						break;

					case HLSLBaseType_Bool:
						m_writer.Write("%sbool*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Bool2:
						m_writer.Write("%sbool2*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Bool3:
						m_writer.Write("%sbool3*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Bool4:
						m_writer.Write("%sbool4*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Int:
						m_writer.Write("%sint*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Int2:
						m_writer.Write("%sint2*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Int3:
						m_writer.Write("%sint3*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Int4:
						m_writer.Write("%sint4*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Uint:
						m_writer.Write("%suint*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Uint2:
						m_writer.Write("%suint2*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Uint3:
						m_writer.Write("%suint3*", CHECK_CSTR(atomic));
						break;
					case HLSLBaseType_Uint4:
						m_writer.Write("%suint4*", CHECK_CSTR(atomic));
						break;
					default:
						break;
					}
				}				

				m_writer.Write(" %s", RawStr(buffer->cachedName));
				m_writer.EndLine(";");
			}
			else if (buffer->type.baseType >= HLSLBaseType_ByteAddressBuffer || buffer->type.baseType <= HLSLBaseType_RWByteAddressBuffer)
			{
				HLSLDeclaration* field = buffer->field;

				m_writer.WriteLine(indent, "constant %s* %s;", CHECK_CSTR("uint"), RawStr(buffer->cachedName));
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

			{
				m_writer.BeginLine(indent, RawStr(statement->fileName), statement->line);
				OutputExpression(expressionStatement->expression, NULL, NULL, function, true);
				m_writer.EndLine(";");
			}
		}
		else if (statement->nodeType == HLSLNodeType_ReturnStatement)
		{
			HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
			if (returnStatement->expression != NULL)
			{
				m_writer.Write(indent, "return ");
				OutputExpression(returnStatement->expression, NULL, NULL, function, true);
				m_writer.EndLine(";");
			}
			else
			{
				m_writer.WriteLine(indent, "return;");
			}
		}
		else if (statement->nodeType == HLSLNodeType_DiscardStatement)
		{
			HLSLDiscardStatement* discardStatement = static_cast<HLSLDiscardStatement*>(statement);
			m_writer.WriteLineTagged(indent, RawStr(discardStatement->fileName), discardStatement->line, "discard_fragment();");
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
			m_writer.BeginLine(indent, RawStr(ifStatement->fileName), ifStatement->line);
			m_writer.Write("if (");
			OutputExpression(ifStatement->condition, NULL, NULL, function, true);
			m_writer.Write(")");
			m_writer.EndLine();


			if (ifStatement->statement != NULL)
			{
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

			for (int i = 0; i< ifStatement->elseifStatement.size(); i++)
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
			int numCases = (int)switchStatement->caseNumber.size();
			ASSERT_PARSER(numCases == switchStatement->caseStatement.size());

			for (int i = 0; i< numCases; i++)
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
			m_writer.BeginLine(indent, RawStr(forStatement->fileName), forStatement->line);
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

			m_writer.BeginLine(indent, RawStr(whileStatement->fileName), whileStatement->line);
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
			m_writer.WriteLineTagged(indent, RawStr(blockStatement->fileName), blockStatement->line, "{");
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
			ASSERT_PARSER(0);
		}

		statement = statement->nextStatement;
	}
}

void MSLGenerator::HideUnusedStatementDeclarations(HLSLFunction * entryFunction)
{
	StringLibrary usedNames;
	eastl::hash_set < const HLSLFunction * > foundFuncs;
	GetAllEntryUsedNames(usedNames, foundFuncs, entryFunction);


	HLSLRoot* root = m_tree->GetRoot();
	//OutputStatements(1, root->statement, NULL);

	HLSLStatement * statement = root->statement;
	while (statement != NULL)
	{
		if (statement->nodeType == HLSLNodeType_Declaration)
		{
			HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);
			// no op?
		}
		else if (statement->nodeType == HLSLNodeType_TextureState)
		{
			HLSLTextureState* textureState = static_cast<HLSLTextureState*>(statement);
			bool isFound = usedNames.HasString(textureState->cachedName.m_string);
			if (!isFound)
			{
				statement->hidden = true;
			}
		}
		//Does Metal support groupshared memory????
		else if (statement->nodeType == HLSLNodeType_GroupShared)
		{
			HLSLGroupShared* pGroupShared = static_cast<HLSLGroupShared*>(statement);
			bool isFound = usedNames.HasString(pGroupShared->cachedName.m_string);
			if (!isFound)
			{
				statement->hidden = true;
			}
		}
		else if (statement->nodeType == HLSLNodeType_SamplerState)
		{
			HLSLSamplerState* samplerState = static_cast<HLSLSamplerState*>(statement);
			bool isFound = usedNames.HasString(samplerState->cachedName.m_string);
			if (!isFound)
			{
				statement->hidden = true;
			}
		}
		else if (statement->nodeType == HLSLNodeType_Struct)
		{
			HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
		}
		else if (statement->nodeType == HLSLNodeType_Buffer)
		{
			HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
			bool isFound = usedNames.HasString(buffer->cachedName.m_string);
			if (!isFound)
			{
				statement->hidden = true;
			}
		}
		else if (statement->nodeType == HLSLNodeType_Function)
		{
			HLSLFunction* function = static_cast<HLSLFunction*>(statement);
			eastl::hash_set < const HLSLFunction * >::iterator iter = foundFuncs.find(function);
			if (iter != foundFuncs.end())
			{
				// all good
			}
			else
			{
				function->hidden = true;
			}
		}
		else if (statement->nodeType == HLSLNodeType_ExpressionStatement)
		{
			HLSLExpressionStatement* expressionStatement = static_cast<HLSLExpressionStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_ReturnStatement)
		{
			HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_DiscardStatement)
		{
			HLSLDiscardStatement* discardStatement = static_cast<HLSLDiscardStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_BreakStatement)
		{
			HLSLBreakStatement* breakStatement = static_cast<HLSLBreakStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_ContinueStatement)
		{
			HLSLContinueStatement* continueStatement = static_cast<HLSLContinueStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_IfStatement)
		{
			HLSLIfStatement* ifStatement = static_cast<HLSLIfStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_SwitchStatement)
		{
			HLSLSwitchStatement* switchStatement = static_cast<HLSLSwitchStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_ForStatement)
		{
			HLSLForStatement* forStatement = static_cast<HLSLForStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_WhileStatement)
		{
			HLSLWhileStatement* whileStatement = static_cast<HLSLWhileStatement*>(statement);
			// no op
		}
		else if (statement->nodeType == HLSLNodeType_BlockStatement)
		{
			HLSLBlockStatement* blockStatement = static_cast<HLSLBlockStatement*>(statement);
			// no op
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
			ASSERT_PARSER(0);
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
				m_writer.Write("%s, ", RawStr(attribute->numGroupXstr));

			if (attribute->numGroupY != 0)
				m_writer.Write("%d, ", attribute->numGroupY);
			else
				m_writer.Write("%s, ", RawStr(attribute->numGroupYstr));


			if (attribute->numGroupZ != 0)
				m_writer.Write("%d", attribute->numGroupZ);
			else
				m_writer.Write("%s", RawStr(attribute->numGroupZstr));

			m_writer.Write(")]");

			//m_writer.Write(indent, "//[numthreads(%d, %d, %d)]", attribute->numGroupX, attribute->numGroupY, attribute->numGroupZ);
		}
		else if (attribute->attributeType == HLSLAttributeType_Domain && m_target == Target_DomainShader && bMain)
		{
			//remove ""
			char tempDomain[16];
			strcpy(tempDomain, &((RawStr(attribute->domain))[1]));

			tempDomain[strlen(RawStr(attribute->domain)) - 2] = NULL;

			m_writer.Write(indent, "[[%s(%s, %d)]]", RawStr(attribute->patchIdentifier), CHECK_CSTR(tempDomain), attribute->maxVertexCount);
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
		ASSERT_PARSER(declaration->nextDeclaration == NULL);
		if (declaration->type.baseType == HLSLBaseType_Sampler2D)
			m_writer.Write("thread texture2d<float>& %s_texture; thread sampler& %s_sampler", RawStr(declaration->cachedName), RawStr(declaration->cachedName));
		else if (declaration->type.baseType == HLSLBaseType_Sampler3D)
			m_writer.Write("thread texture3d<float>& %s_texture; thread sampler& %s_sampler", RawStr(declaration->cachedName), RawStr(declaration->cachedName));
		else if (declaration->type.baseType == HLSLBaseType_SamplerCube)
			m_writer.Write("thread texturecube<float>& %s_texture; thread sampler& %s_sampler", RawStr(declaration->cachedName), RawStr(declaration->cachedName));
		else if (declaration->type.baseType == HLSLBaseType_Sampler2DShadow)
			m_writer.Write("thread depth2d<float>& %s_texture; thread sampler& %s_sampler", RawStr(declaration->cachedName), RawStr(declaration->cachedName));
		else if (declaration->type.baseType == HLSLBaseType_Sampler2DMS)
			m_writer.Write("thread texture2d_ms<float>& %s_texture", RawStr(declaration->cachedName));
		else if (declaration->type.baseType == HLSLBaseType_Sampler2DArray)
			m_writer.Write("thread texture2d_array<float>& %s_texture; thread sampler& %s_sampler", RawStr(declaration->cachedName), RawStr(declaration->cachedName));
		else
			m_writer.Write("<unhandled texture type>");
	}
	else
	{
		OutputDeclaration(declaration->type, declaration->cachedName, declaration->assignment, function);
		declaration = declaration->nextDeclaration;
		while (declaration != NULL)
		{
			m_writer.Write(",");
			OutputDeclarationBody(declaration->type, declaration->cachedName, declaration->assignment, function);
			declaration = declaration->nextDeclaration;
		};
	}
}

void MSLGenerator::OutputStruct(int indent, HLSLStruct* structure)
{
	m_writer.WriteLineTagged(indent, RawStr(structure->fileName), structure->line, "struct %s", RawStr(structure->name));
	m_writer.WriteLine(indent, "{");

	HLSLStructField* field = structure->field;
	while (field != NULL)
	{
		if (!field->hidden)
		{
			m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);

			if (field->atomic)
				m_writer.Write("atomic_");

			if (String_Equal(field->semantic, "SV_TessFactor") || String_Equal(field->semantic, "SV_InsideTessFactor"))
			{
				m_writer.Write("array<");
				m_writer.Write("%s, ", CHECK_CSTR("half"));
				//m_writer.Write("%d", OutputExpression(field));
				OutputExpression(field->type.arraySize, NULL, NULL, NULL, true);

				m_writer.Write(">");

				// Then name
				m_writer.Write(" %s", RawStr(field->name));
			}
			else
			{
				if (field->sv_semantic.IsNotEmpty() && strncmp(RawStr(field->sv_semantic), "color", 5) == 0)
				{
					field->type.baseType = HLSLBaseType_Float4;
					OutputDeclaration(field->type, field->name, NULL, NULL);
				}
				else
				{
					OutputDeclaration(field->type, field->name, NULL, NULL);
				}
				
			}

			//if (field->sv_semantic.IsNotEmpty())
			//{
			//	m_writer.Write(" [[%s]]", RawStr(field->sv_semantic));
			//}

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
	m_writer.WriteLine(indent, "struct Uniforms_%s", RawStr(buffer->cachedName));
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
					m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);
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
			m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);
			OutputDeclaration(field->type, field->cachedName, field->assignment, NULL, false, false, /*alignment=*/0);
			m_writer.EndLine(";");
		}
		field = (HLSLDeclaration*)field->nextStatement;
	}
	m_writer.WriteLine(indent, "};");

	m_writer.WriteLine(indent, "constant Uniforms_%s & %s;", RawStr(buffer->cachedName), RawStr(buffer->cachedName));
}

void MSLGenerator::OutputFunction(int indent, const HLSLFunction* function)
{
	CachedString functionName = function->name;
	CachedString returnTypeName = GetTypeName(function->returnType);

	m_writer.BeginLine(indent, RawStr(function->fileName), function->line);
	m_writer.Write("%s %s(", RawStr(returnTypeName), RawStr(functionName));

	eastl::vector < HLSLArgument * > arguments = function->GetArguments();
	OutputArgumentsVec(arguments, function);

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
		CachedString name = identifierExpression->name;

		// For texture declaration, generate a struct instead
		if (identifierExpression->global && IsSamplerType(identifierExpression->expressionType))
		{
			if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2D)
				m_writer.Write("Texture2DSampler(%s_texture, %s_sampler)", RawStr(name), RawStr(name));
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler3D)
				m_writer.Write("Texture3DSampler(%s_texture, %s_sampler)", RawStr(name), RawStr(name));
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_SamplerCube)
				m_writer.Write("TextureCubeSampler(%s_texture, %s_sampler)", RawStr(name), RawStr(name));
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DShadow)
				m_writer.Write("Texture2DShadowSampler(%s_texture, %s_sampler)", RawStr(name), RawStr(name));
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DMS)
				m_writer.Write("%s_texture", RawStr(name));
			else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DArray)
				m_writer.Write("Texture2DArraySampler(%s_texture, %s_sampler)", RawStr(name), RawStr(name));
			else
				m_writer.Write("<unhandled texture type>");
		}	
		//add buffer's name
		else if (function != NULL && !matchFunctionArgumentsIdentifiersVec(function->GetArguments(), identifierExpression->name) && m_tree->FindBuffertMember(identifierExpression->name).IsNotEmpty())
		{
			m_writer.Write("%s.%s", RawStr(m_tree->FindBuffertMember(identifierExpression->name)), RawStr(identifierExpression->name));
		}
		else if (expression->expressionType.baseType == HLSLBaseType_OutputPatch)
		{
			m_writer.Write("%s.control_points", RawStr(identifierExpression->name));
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
					ASSERT_PARSER(buffer == declaration->buffer);
					m_writer.Write("%s.", RawStr(declaration->buffer->cachedName));
				}
			}


			// if it is one of PushConstantBuffer's data's Name
			for (int index = 0; index < m_PushConstantBuffers.size(); index++)
			{
				HLSLBuffer* buffer = static_cast<HLSLBuffer*>(m_PushConstantBuffers[index]);
				HLSLDeclaration* field = buffer->field;

				while (field != NULL)
				{
					if (!field->hidden)
					{
						if (String_Equal(field->cachedName, name))
						{
							m_writer.Write("%s.%s", RawStr(buffer->cachedName), RawStr(name));
							return;
						}
					}
					field = (HLSLDeclaration*)field->nextStatement;
				}
			}

			m_writer.Write("%s", RawStr(name));
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
		m_writer.Write("%s(", RawStr(GetTypeName(constructorExpression->type)));
		if (HLSLBaseType_Float <= expression->expressionType.baseType &&
			expression->expressionType.baseType <= HLSLBaseType_LastNumeric)
		{
			// use the special version with explicit casts
			HLSLBaseType expectedScalarType = GetScalarBaseType(expression->expressionType.baseType);

			// In hlsl, something like this is legal:
			// float2 val;
			// uint3 ret = uint3(val,0.0);
			//
			// That's because hlsl can implicitly convert val from a float2 to a uint2, and then apply it to the
			// uint3 contstructor. But in metal, that code causes an illegal cast. So to fix it, we will
			// get the base type of the constructor (uint in this case), and for each parameter, do
			// a cast if necessary.
			OutputExpressionListConstructor(constructorExpression->argument, function, expectedScalarType);
		}
		else
		{
			OutputExpressionList(constructorExpression->argument, function);
		}
		
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
			ASSERT_PARSER(-1);
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
			m_writer.Write("%s", CHECK_CSTR(op));
			OutputExpression(unaryExpression->expression, NULL, unaryExpression, function, needsEndParen);
		}
		else
		{
			OutputExpression(unaryExpression->expression, NULL, unaryExpression, function, needsEndParen);
			m_writer.Write("%s", CHECK_CSTR(op));
		}
		if (addParenthesis) m_writer.Write(")");
	}
	else if (expression->nodeType == HLSLNodeType_BinaryExpression)
	{
		HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(expression);

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
				ASSERT_PARSER(-1);
			}

			// Exception Handling for imageStore, this might make errors
			// Need to change better form
			HLSLArrayAccess* ArrayAcess = static_cast<HLSLArrayAccess*>(binaryExpression->expression1);

			if (binaryExpression->binaryOp == HLSLBinaryOp_Assign && binaryExpression->expression1->expressionType.baseType >= HLSLBaseType_RWTexture1D && binaryExpression->expression1->expressionType.baseType <= HLSLBaseType_RWTexture3D)
			{
				HLSLTextureStateExpression* textureStateExpression = static_cast<HLSLTextureStateExpression*>(binaryExpression->expression1);

				if (textureStateExpression->expressionType.baseType >= HLSLBaseType_RWTexture1D && textureStateExpression->expressionType.baseType <= HLSLBaseType_RWTexture3D)
				{
					m_writer.Write("%s", RawStr(textureStateExpression->name));

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
					m_writer.Write("%s", CHECK_CSTR(op));
					OutputExpression(binaryExpression->expression2, dstType2, binaryExpression, function, true);
				}
			}
			else
			{
				OutputExpression(binaryExpression->expression1, dstType1, binaryExpression, function, needsEndParen);

				m_writer.Write("%s", CHECK_CSTR(op));
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
		bool addParenthesis;
		if (!needsEndParen)
			addParenthesis = false;
		else 
			addParenthesis = true;

		//compare the length of swizzling
		if (memberAccess->swizzle)
		{
			HLSLBaseType baseType = memberAccess->object->expressionType.baseType;
			if (memberAccess->object->nodeType == HLSLNodeType_ArrayAccess)
			{
				HLSLArrayAccess * arrayAccess = (HLSLArrayAccess*)memberAccess->object;
				baseType = arrayAccess->expressionType.elementType;
			}

			int parentLength = 0;
			switch (baseType)
			{
			case HLSLBaseType_Float:
			case HLSLBaseType_Half:
			case HLSLBaseType_Min16Float:
			case HLSLBaseType_Min10Float:
			case HLSLBaseType_Bool:
			case HLSLBaseType_Int:
			case HLSLBaseType_Uint:
				parentLength = 1;
				break;

			case HLSLBaseType_Float2:
			case HLSLBaseType_Half2:
			case HLSLBaseType_Min16Float2:
			case HLSLBaseType_Min10Float2:
			case HLSLBaseType_Bool2:
			case HLSLBaseType_Int2:
			case HLSLBaseType_Uint2:
				parentLength = 2;
				break;

			case HLSLBaseType_Float3:
			case HLSLBaseType_Half3:
			case HLSLBaseType_Min16Float3:
			case HLSLBaseType_Min10Float3:
			case HLSLBaseType_Bool3:
			case HLSLBaseType_Int3:
			case HLSLBaseType_Uint3:
				parentLength = 3;
				break;

			case HLSLBaseType_Float4:
			case HLSLBaseType_Half4:
			case HLSLBaseType_Min16Float4:
			case HLSLBaseType_Min10Float4:
			case HLSLBaseType_Bool4:
			case HLSLBaseType_Int4:
			case HLSLBaseType_Uint4:
				parentLength = 4;
				break;
			default:
				break;
			}

			int swizzleLength = (int)strlen(RawStr(memberAccess->field));

			// this cast seems incorrect. for example, if the swizzle is .w, you would still
			// need a 4 channel vector even though strlen(w) = 1

			//if (parentLength < swizzleLength)
			//	m_writer.Write("%s", RawStr(GetTypeName(expression->expressionType)));
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


		m_writer.Write(".%s", RawStr(memberAccess->field));
	}
	else if (expression->nodeType == HLSLNodeType_ArrayAccess)
	{
		HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);

		OutputExpression(arrayAccess->array, NULL, expression, function, true);
		m_writer.Write("[");
		OutputExpression(arrayAccess->index, NULL, NULL, function, true);
		m_writer.Write("]");

	}
	else if (expression->nodeType == HLSLNodeType_FunctionCall)
	{
		HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(expression);
		CachedString name = functionCall->function->name;


		if (String_Equal(name, "mul"))
		{
			HLSLExpression* argument[2];
			if (GetFunctionArguments(functionCall, argument, 2) != 2)
			{
				Error("mul expects 2 arguments");
				return;
			}

			eastl::vector < HLSLArgument * > argumentVec = functionCall->function->GetArguments();
			ASSERT_PARSER(argumentVec.size() >= 2);

			
			const HLSLType& type0 = argumentVec[0]->argType;
			const HLSLType& type1 = argumentVec[1]->argType;

			const char* prefix = "";
			const char* infix = "*";

			if (m_options.flags & Flag_PackMatrixRowMajor)
			{
				m_writer.Write("%s((", CHECK_CSTR(prefix));
				OutputExpression(argument[1], NULL, NULL, function, true);
				m_writer.Write(")%s(", CHECK_CSTR(infix));
				OutputExpression(argument[0], NULL, NULL, function, true);
				m_writer.Write("))");
			}
			else
			{
				m_writer.Write("%s((", CHECK_CSTR(prefix));
				OutputExpression(argument[0], NULL, NULL, function, true);
				m_writer.Write(")%s(", CHECK_CSTR(infix));
				OutputExpression(argument[1], NULL, NULL, function, true);
				m_writer.Write("))");
			}
		}
		else if (String_Equal(name, "max") || String_Equal(name, "min"))
		{
			HLSLExpression* argument[2];
			if (GetFunctionArguments(functionCall, argument, 2) != 2)
			{
				Error("mul expects 2 arguments");
				return;
			}

			eastl::vector < HLSLArgument * > argumentVec = functionCall->function->GetArguments();
			ASSERT_PARSER(argumentVec.size() >= 2);

			const HLSLType& retType = functionCall->function->returnType;

			CachedString typeName = GetTypeName(retType);

			m_writer.Write("%s((%s)",RawStr(name),RawStr(typeName));
			OutputExpression(argument[0],NULL,NULL,functionCall->function,false);
			m_writer.Write(",(%s)",RawStr(typeName));
			OutputExpression(argument[1], NULL, NULL, functionCall->function, false);
			m_writer.Write(")");
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
				m_writer.Write(".%s(", CHECK_CSTR("sample"));
			else if (String_Equal(name, "SampleLevel"))
				m_writer.Write(".%s(", CHECK_CSTR("sample"));
			else if (String_Equal(name, "SampleBias"))
				m_writer.Write(".%s(", CHECK_CSTR("sample"));
			else if (String_Equal(name, "GatherRed"))
				m_writer.Write(".%s(", CHECK_CSTR("sample"));
			else if (String_Equal(name, "SampleGrad"))
				m_writer.Write(".%s(", CHECK_CSTR("gather"));

			eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();

			ASSERT_PARSER(argVec.size() >= 2);

			HLSLExpression* firstExpression = argVec[0];//functionCall->callArgument;

			//should be sampler
			OutputExpression(firstExpression, NULL, NULL, function, true);

			m_writer.Write(", ");

			HLSLExpression* SecondExpression = argVec[1];//firstExpression->nextExpression;
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
			if (argVec.size() >= 3)
			{
				HLSLExpression* ThirdExpression = argVec[2];//SecondExpression->nextExpression;
				m_writer.Write(", level(");
				OutputExpression(ThirdExpression, NULL, NULL, function, true);
				m_writer.Write(")");
			}

			if (argVec.size() >= 4)
			{
				// 4 params, but it isn't handled yet. TODO.
			}

			m_writer.Write(")");
		}
		else if (String_Equal(name, "Load"))
		{
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

				{
					eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
					OutputExpressionListVec(argVec, function);
				}

				//HLSL -> MSL
				switch (functionCall->pTextureStateExpression->expressionType.baseType)
				{
				case HLSLBaseType_Texture1D:
					m_writer.Write(".x");
					break;
				case HLSLBaseType_Texture1DArray:
					m_writer.Write(").x, uint2(");
					{
						eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
						OutputExpressionListVec(argVec, function);
					}
					m_writer.Write(").y");
					break;
				case HLSLBaseType_Texture2D:
					m_writer.Write(".xy");
					break;
				case HLSLBaseType_Texture2DArray:
					m_writer.Write(").xy, uint3(");
					{
						eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
						OutputExpressionListVec(argVec, function);
					}
					m_writer.Write(").z");
					break;
				case HLSLBaseType_Texture3D:
					m_writer.Write(".xyz");
					break;
				case HLSLBaseType_Texture2DMS:
					break;
				case HLSLBaseType_Texture2DMSArray:
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
				{
					eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
					OutputExpressionListVec(argVec, function);
				}
				m_writer.Write("]");
			}
		}
		else if (String_Equal(name, "Store"))
		{
			if (functionCall->pBuffer)
			{

				m_writer.Write("%s", RawStr(functionCall->pBuffer->cachedName));

				m_writer.Write("[");

				eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
				ASSERT_PARSER(argVec.size() == 2);
				OutputExpression(argVec[0], NULL, NULL, function, false);

				m_writer.Write("]");

				m_writer.Write(" = ");

				OutputExpression(argVec[1], NULL, NULL, function, false);
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
			eastl::vector < HLSLExpression* > expressionVec = functionCall->GetArguments();
			eastl::vector < HLSLArgument* > argumentVec = functionCall->function->GetArguments();

			//check the number of arguements
			if (expressionVec.size() == 3)
			{
				//OutputExpression(expression->nextExpression->nextExpression, NULL, NULL, function, true);
				OutputExpression(expressionVec[2], NULL, NULL, function, true);
				m_writer.Write(" = ");
			}

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

			int numExp = (int)expressionVec.size() <= 2 ? (int)expressionVec.size() : 2;
			for (int i = 0; i < numExp; i++)
			{
				if (i > 0)
				{
					m_writer.Write(", ");
				}

				OutputExpression(expressionVec[i], NULL, NULL, function, true);
			}

			m_writer.Write(", memory_order_relaxed)");

		}
		else if (String_Equal(name, "sincos"))
		{
			eastl::vector < HLSLExpression* > expressionVec = functionCall->GetArguments();

			ASSERT_PARSER(expressionVec.size() == 3);

			HLSLExpression* angleArgument = expressionVec[0];
			HLSLExpression* sinArgument = expressionVec[1];
			HLSLExpression* cosArgument = expressionVec[2];

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
	else if (expression->nodeType == HLSLNodeType_SamplerStateExpression)
	{
		HLSLSamplerStateExpression* samplerStateExpression = static_cast<HLSLSamplerStateExpression*>(expression);

		m_writer.Write("%s", RawStr(samplerStateExpression->name));
	}
	else if (expression->nodeType == HLSLNodeType_TextureStateExpression)
	{
		HLSLTextureStateExpression* textureStateExpression = static_cast<HLSLTextureStateExpression*>(expression);

	

		if (textureStateExpression->indexExpression)
		{						
			HLSLBaseType indexType = textureStateExpression->indexExpression->expressionType.baseType;
			HLSLBaseType scalarType = GetScalarBaseType(indexType);

			int vecLen = (indexType - scalarType) + 1;

			m_writer.Write("%s", RawStr(textureStateExpression->name));
			m_writer.Write(".read(");
			m_writer.Write("(");
			OutputExpression(textureStateExpression->indexExpression, NULL, NULL, function, true);
			m_writer.Write(").xy");

			m_writer.Write(")");
		}
		else if (textureStateExpression->functionExpression)
		{
			if (textureStateExpression->functionExpression->nodeType == HLSLNodeType_FunctionCall)
			{
				HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(textureStateExpression->functionExpression);
				CachedString name = functionCall->function->name;

				if (String_Equal(name, "GetDimensions"))
				{
					eastl::vector < HLSLExpression* > expressionVec = functionCall->GetArguments();
					
					for (int i = 0; i < expressionVec.size(); i++)
					{
						HLSLExpression* exp = expressionVec[i];
						if (i > 0)
						{
							m_writer.Write(";\n");
							m_writer.Write(2, "");
						}

						OutputExpression(exp, NULL, NULL, function, true);

						m_writer.Write(" = ");

						const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;

						if (pTextureStateExpression == NULL)
						{
							m_writer.Write("INVALID pTextureStateExpression");
						}
						else
						{
							m_writer.Write("%s", RawStr(pTextureStateExpression->name));

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
						}

						if (i == 0)
							m_writer.Write(".get_width(");
						else if (i == 1)
							m_writer.Write(".get_height(");
						else if (i == 2)
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
					}
				}
				else
				{
					m_writer.Write("%s", RawStr(textureStateExpression->name));
					OutputExpression(textureStateExpression->functionExpression, NULL, NULL, function, false);
				}
			}
			else
			{
				m_writer.Write("%s", RawStr(textureStateExpression->name));
				OutputExpression(textureStateExpression->functionExpression, NULL, NULL, function, false);
			}
		}
		else
		{
			m_writer.Write("%s", FetchCstr(m_stringLibrary, textureStateExpression->name));
		}

		//TODO output texture variable access;
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

bool MSLGenerator::matchFunctionArgumentsIdentifiersVec(const eastl::vector < HLSLArgument* > & arguments, const CachedString & name)
{
	for (int i = 0; i < arguments.size(); i++)
	{
		HLSLArgument * argument = arguments[i];
		if (String_Equal(argument->name, name))
			return true;

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
void MSLGenerator::OutputArgumentsVec(eastl::vector < HLSLArgument* > & arguments, const HLSLFunction* function)
{
	int numWritten = 0;

	for (int i = 0; i < arguments.size(); i++)
	{
		HLSLArgument * argument = arguments[i];
		if (argument->hidden)
		{
			//argument = argument->nextArgument;
			continue;
		}

		if (numWritten > 0)
		{
			m_writer.Write(", ");
		}

		bool isRef = false;
		bool isConst = false;
		if (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout)
		{
			isRef = true;
		}

		if (argument->modifier == HLSLArgumentModifier_Const)
		{
			isConst = true;
		}

		m_writer.Write(0, "");
		OutputDeclaration(argument->argType, argument->name, argument->defaultValue, function, isRef, isConst);

		numWritten++;
	}
}

void MSLGenerator::OutputDeclaration(const HLSLType& type, const CachedString & name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef, bool isConst, int alignment)
{
	OutputDeclarationType(type, isRef, isConst, alignment);
	OutputDeclarationBody(type, name, assignment, function, isRef);
}

void MSLGenerator::OutputDeclarationType(const HLSLType& type, bool isRef, bool isConst, int alignment)
{
	CachedString typeName = GetTypeName(type);
	if (isRef)
	{
		m_writer.Write("thread ");
	}
	if (isConst || type.flags & HLSLTypeFlag_Const)
	{
		m_writer.Write("const ");

	}
	if (IsSamplerType(type))
	{
		if (type.baseType == HLSLBaseType_Sampler2D)
			typeName = MakeCached("Texture2DSampler");
		else if (type.baseType == HLSLBaseType_Sampler3D)
			typeName = MakeCached("Texture3DSampler");
		else if (type.baseType == HLSLBaseType_SamplerCube)
			typeName = MakeCached("TextureCubeSampler");
		else if (type.baseType == HLSLBaseType_Sampler2DShadow)
			typeName = MakeCached("Texture2DShadowSampler");
		else if (type.baseType == HLSLBaseType_Sampler2DMS)
			typeName = MakeCached("Texture2DMSSampler");
		else if (type.baseType == HLSLBaseType_Sampler2DArray)
			typeName = MakeCached("Texture2DArraySampler");
		else
			typeName = MakeCached("<unhandled texture type>");
	}
	else if (alignment != 0)
	{
		m_writer.Write("alignas(%d) ", alignment);
	}

	if (type.array)
	{
		m_writer.Write("array<%s, ", RawStr(GetTypeName(type)));
			OutputExpression(type.arraySize, NULL, NULL, NULL, true);
		m_writer.Write(">");
	}
	else
	{
		m_writer.Write("%s", RawStr(typeName));

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

void MSLGenerator::OutputDeclarationBody(const HLSLType& type, const CachedString & name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef)
{
	if (isRef)
	{
		// Arrays of refs are illegal in C++ and hence MSL, need to "link" the & to the var name
		m_writer.Write("(&");
	}

	// Then name
	m_writer.Write(" %s", RawStr(name));

	if (isRef)
	{
		m_writer.Write(")");
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
			OutputExpression(assignment, &type, NULL, function, true);
		}
	}
}

void MSLGenerator::OutputExpressionListVec(eastl::vector < HLSLExpression* > expressionVec, const HLSLFunction* function)
{
	for (int i = 0; i < expressionVec.size(); i++)
	{
		HLSLExpression * expression = expressionVec[i];
		ASSERT_PARSER(expression != NULL);
		if (i > 0)
		{
			m_writer.Write(", ");
		}
		OutputExpression(expression, NULL, NULL, function, true);
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

void MSLGenerator::OutputExpressionListConstructor(HLSLExpression* expression, const HLSLFunction* function, HLSLBaseType expectedScalarType)
{
	int numExpressions = 0;
	while (expression != NULL)
	{
		if (numExpressions > 0)
		{
			m_writer.Write(", ");
		}

		HLSLBaseType actualScalarType = GetScalarBaseType(expression->expressionType.baseType);

		if (expectedScalarType != actualScalarType)
		{
			int offset = expression->expressionType.baseType - actualScalarType;
			HLSLBaseType dstType = (HLSLBaseType)(expectedScalarType + offset);

			HLSLType currType;
			currType.baseType = dstType;
			OutputCast(currType);
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

	eastl::vector < HLSLArgument * > argumentVec = functionCall->function->GetArguments();
	eastl::vector < HLSLExpression * > expressionVec = functionCall->GetArguments();

	int numArguments = (int)argumentVec.size();
	ASSERT_PARSER(expressionVec.size() == argumentVec.size());

	for (int i = 0; i < numArguments; i++)
	{
		HLSLArgument* argument = argumentVec[i];
		HLSLExpression* expression = expressionVec[i];

		if (!isAddressable(expression))
		{
			if (argument->modifier == HLSLArgumentModifier_Out)
			{
				m_writer.BeginLine(indent, RawStr(functionCall->fileName), functionCall->line);
				OutputDeclarationType(argument->argType);
				m_writer.Write("tmp%d;", argumentIndex);
				m_writer.EndLine();
			}
			else if (argument->modifier == HLSLArgumentModifier_Inout)
			{
				m_writer.BeginLine(indent, RawStr(functionCall->fileName), functionCall->line);
				OutputDeclarationType(argument->argType);
				m_writer.Write("tmp%d = ", argumentIndex);
				OutputExpression(expression, NULL, NULL, functionCall->function, true);
				m_writer.EndLine(";");
			}
		}
		argumentIndex++;
	}

	m_writer.BeginLine(indent, RawStr(functionCall->fileName), functionCall->line);
	CachedString name = functionCall->function->name;
	m_writer.Write("%s(", RawStr(name));

	argumentIndex = 0;
	
	for (int i = 0; i < expressionVec.size(); i++)
	{
		HLSLArgument* argument = argumentVec[i];
		HLSLExpression* expression = expressionVec[i];

		if (!isAddressable(expression) && (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout))
		{
			m_writer.Write("tmp%d", argumentIndex);
		}
		else
		{
			OutputExpression(expression, NULL, NULL, functionCall->function, true);
		}

		argumentIndex++;
		if (expression)
		{
			m_writer.Write(", ");
		}
	}
	m_writer.EndLine(");");

	argumentIndex = 0;
	for (int i = 0; i < expressionVec.size(); i++)
	{
		HLSLArgument* argument = argumentVec[i];
		HLSLExpression* expression = expressionVec[i];

		if (!isAddressable(expression) && (argument->modifier == HLSLArgumentModifier_Out || argument->modifier == HLSLArgumentModifier_Inout))
		{
			m_writer.BeginLine(indent, RawStr(functionCall->fileName), functionCall->line);
			OutputExpression(expression, NULL, NULL, functionCall->function, true);
			m_writer.Write(" = tmp%d", argumentIndex);
			m_writer.EndLine(";");
		}

		argumentIndex++;
	}
}

void MSLGenerator::OutputFunctionCall(HLSLFunctionCall* functionCall)
{
	// @@ All these shenanigans only work if the function call is a statement...
	CachedString name = functionCall->function->name;

	if (String_Equal(name, "lerp"))
		name = MakeCached("mix");
	else if (String_Equal(name, "ddx"))
		name = MakeCached("dfdx");
	else if (String_Equal(name, "ddy"))
		name = MakeCached("dfdy");
	else if (String_Equal(name, "frac"))
		name = MakeCached("fract");
	else if (String_Equal(name, "countbits"))
	{
		name = MakeCached("popcount");
	}

	else if (String_Equal(name, "QuadReadAcrossDiagonal"))
	{
		name = MakeCached("quad_shuffle");

		m_writer.Write("%s(", RawStr(name));

		{
			eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
			OutputExpressionListVec(argVec, functionCall->function);
		}
		m_writer.Write(", 3");
		m_writer.Write(")");
		return;
	}

	else if (String_Equal(name, "QuadReadLaneAt"))
	{
		name = MakeCached("quad_broadcast");
	}


	else if (String_Equal(name, "QuadReadAcrossX"))
	{
		name = MakeCached("quad_shuffle");

		m_writer.Write("%s(", RawStr(name));
		{
			eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
			OutputExpressionListVec(argVec, functionCall->function);
		}
		m_writer.Write(", 1");
		m_writer.Write(")");
		return;
	}

	else if (String_Equal(name, "QuadReadAcrossY"))
	{
		name = MakeCached("quad_shuffle");

		m_writer.Write("%s(", RawStr(name));
		{
			eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
			OutputExpressionListVec(argVec, functionCall->function);
		}
		m_writer.Write(", 2");
		m_writer.Write(")");
		return;
	}

	else if (String_Equal(name, "WaveActiveAllEqual"))
	{
		name = MakeCached("<unknown function>");
	}

	else if (String_Equal(name, "WaveActiveBitAnd"))
	{
		name = MakeCached("simd_and");
	}

	else if (String_Equal(name, "WaveActiveBitOr"))
	{
		name = MakeCached("simd_or");
	}

	else if (String_Equal(name, "WaveActiveBitXor"))
	{
		name = MakeCached("simd_xor");
	}

	else if (String_Equal(name, "WaveActiveCountBits"))
	{
		name = MakeCached("<unknown function>");
	}

	else if (String_Equal(name, "WaveActiveMax"))
	{
		name = MakeCached("simd_max");
	}
	else if (String_Equal(name, "WaveActiveMin"))
	{
		name = MakeCached("simd_min");
	}
	else if (String_Equal(name, "WaveActiveProduct"))
	{
		name = MakeCached("simd_product");
	}
	else if (String_Equal(name, "WaveActiveSum"))
	{
		name = MakeCached("simd_sum");
	}
	else if (String_Equal(name, "WaveActiveAllTrue"))
	{
		name = MakeCached("simd_all");
	}
	else if (String_Equal(name, "WaveActiveAnyTrue"))
	{
		name = MakeCached("simd_any");
	}
	else if (String_Equal(name, "WaveActiveBallot"))
	{
		name = MakeCached("simd_ballot");
	}

	else if (String_Equal(name, "WaveIsFirstLane"))
	{
		name = MakeCached("simd_is_first");
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
		name = MakeCached("<unknown function>");
	}

	else if (String_Equal(name, "WavePrefixProduct"))
	{
		name = MakeCached("simd_prefix_exclusive_product");
	}

	else if (String_Equal(name, "WavePrefixSum"))
	{
		name = MakeCached("simd_prefix_exclusive_sum");
	}

	else if (String_Equal(name, "WaveReadLaneFirst"))
	{
		name = MakeCached("simd_broadcast_first");
	}

	else if (String_Equal(name, "WaveReadLaneAt"))
	{
		name = MakeCached("simd_broadcast");
	}
	else if (String_Equal(name, "asfloat"))
		name = MakeCached("as_type<float>");
	else if (String_Equal(name, "asuint"))
		name = MakeCached("as_type<uint>");
	else if (String_Equal(name, "asint"))
		name = MakeCached("as_type<int>");


	m_writer.Write("%s(", RawStr(name));
	{
		eastl::vector < HLSLExpression * > argVec = functionCall->GetArguments();
		OutputExpressionListVec(argVec, functionCall->function);
	}
	m_writer.Write(")");
}

CachedString MSLGenerator::TranslateInputSemantic(const CachedString & semantic, int count)
{
	if (semantic.IsEmpty())
		return CachedString();

	unsigned int length, index;
	ParseSemantic(RawStr(semantic), &length, &index);

	if (m_target == MSLGenerator::Target_VertexShader || m_target == MSLGenerator::Target_DomainShader)
	{
		if (String_Equal(semantic, "INSTANCE_ID")) return MakeCached("instance_id");
		if (String_Equal(semantic, "SV_InstanceID")) return MakeCached("instance_id");
		if (String_Equal(semantic, "VERTEX_ID")) return MakeCached("vertex_id");
		if (String_Equal(semantic, "SV_VertexID")) return MakeCached("vertex_id");

		if (m_target == Target_DomainShader && (String_Equal(semantic, "SV_DomainLocation")))
			return MakeCached("position_in_patch");

		CachedString attrStr = m_tree->AddStringFormatCached("attribute(%d)", attributeCounter);
		attributeCounter += count;
		return attrStr;
#if 0

		if (m_options.attributeCallback)
		{
			char name[64];
			ASSERT_PARSER(length < sizeof(name));

			strncpy(name, RawStr(semantic), length);
			name[length] = 0;

			int attribute = m_options.attributeCallback(name, index);

			if (attribute >= 0)
			{
				return m_tree->AddStringFormatCached("attribute(%d)", attribute);
			}
		}
#endif

	}
	else if (m_target == MSLGenerator::Target_HullShader)
	{
		if (String_Equal(semantic, "SV_PrimitiveID"))
			return MakeCached("????");

		CachedString attrStr = m_tree->AddStringFormatCached("attribute(%d)", attributeCounter);
		attributeCounter += count;
		return attrStr;
	}
	else if (m_target == MSLGenerator::Target_FragmentShader)
	{
		if (String_Equal(semantic, "SV_Position") || String_Equal(semantic, "SV_POSITION")) return MakeCached("position");
		if (String_Equal(semantic, "SV_SampleIndex")) return MakeCached("sample_id");
		if (String_Equal(semantic, "VFACE")) return MakeCached("front_facing");
		if (String_Equal(semantic, "TARGET_INDEX")) return MakeCached("render_target_array_index");
	}
	else if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_DispatchThreadID")))
		return MakeCached("thread_position_in_grid");
	else  if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_GroupID")))
		return MakeCached("threadgroup_position_in_grid");
	else  if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_GroupIndex")))
		return MakeCached("thread_index_in_threadgroup");
	else  if (m_target == Target_ComputeShader && (String_Equal(semantic, "SV_GroupThreadID")))
		return MakeCached("thread_position_in_threadgroup");
	

	else
	{

	}

	return CachedString();
}


CachedString MSLGenerator::TranslateOutputSemantic(const CachedString & semantic)
{
	if (semantic.IsEmpty())
		return CachedString();

	unsigned int length, index;
	ParseSemantic(RawStr(semantic), &length, &index);

	if (m_target == MSLGenerator::Target_VertexShader || m_target == Target_DomainShader)
	{
		if (String_Equal(semantic, "SV_Position") || String_Equal(semantic, "SV_POSITION")) return MakeCached("position");
		if (String_Equal(semantic, "PSIZE")) return MakeCached("point_size");
		if (String_Equal(semantic, "POINT_SIZE")) return MakeCached("point_size");
		if (String_Equal(semantic, "TARGET_INDEX")) return MakeCached("render_target_array_index");
	}
	else if (m_target == MSLGenerator::Target_HullShader)
	{
		if (String_Equal(semantic, "SV_Position") || String_Equal(semantic, "SV_POSITION") || String_Equal(semantic, "POSITION")) return MakeCached("position");


		if (String_Equal(semantic, "SV_PrimitiveID"))
			return MakeCached("????");
	}
	else if (m_target == MSLGenerator::Target_FragmentShader)
	{
		if (m_options.flags & MSLGenerator::Flag_NoIndexAttribute)
		{
			// No dual-source blending on iOS, and no index() attribute
			if (String_Equal(semantic, "COLOR0_1")) return CachedString();
		}
		else
		{
			// @@ IC: Hardcoded for this specific case, extend ParseSemantic?
			if (String_Equal(semantic, "COLOR0_1")) return MakeCached("color(0), index(1)");
		}

		if (strncmp(RawStr(semantic), "SV_Target", length) == 0)
		{
			return m_tree->AddStringFormatCached("color(%d)", index);
		}
		if (strncmp(RawStr(semantic), "COLOR", length) == 0)
		{
			return m_tree->AddStringFormatCached("color(%d)", index);
		}

		if (String_Equal(semantic, "DEPTH")) return MakeCached("depth(any)");
		if (String_Equal(semantic, "DEPTH_GT")) return MakeCached("depth(greater)");
		if (String_Equal(semantic, "DEPTH_LT")) return MakeCached("depth(less)");
		if (String_Equal(semantic, "SAMPLE_MASK")) return MakeCached("sample_mask");
	}

	return CachedString();
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
				if (String_Equal(field->cachedName, pTextureStateExpression->arrayIdentifier[counter]))
				{
					*bWritten = true;
					m_writer.Write("%s.%s", RawStr(buffer->cachedName), RawStr(pTextureStateExpression->arrayIdentifier[counter]));
					break;
				}
			}
			field = (HLSLDeclaration*)field->nextStatement;
		}

		if (*bWritten)
			break;
	}
}
