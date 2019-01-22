//=============================================================================
//
// Render/HLSLGenerator.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

//#include "Engine/String.h"
//#include "Engine/Log.h"
#include "Engine.h"

#include "HLSLGenerator.h"
#include "HLSLParser.h"
#include "HLSLTree.h"


static const char* GetTypeName(const HLSLType& type)
{
    switch (type.baseType)
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

    case HLSLBaseType_Half:         return "half";
	case HLSLBaseType_Half1x2:        return "half1x2";
	case HLSLBaseType_Half1x3:        return "half1x3";
	case HLSLBaseType_Half1x4:        return "half1x4";
    case HLSLBaseType_Half2:        return "half2";
	case HLSLBaseType_Half2x2:        return "half2x2";
	case HLSLBaseType_Half2x3:        return "half2x3";
	case HLSLBaseType_Half2x4:        return "half2x4";
    case HLSLBaseType_Half3:        return "half3";
	case HLSLBaseType_Half3x2:        return "half3x2";
	case HLSLBaseType_Half3x3:        return "half3x3";
	case HLSLBaseType_Half3x4:        return "half3x4";
    case HLSLBaseType_Half4:        return "half4";
	case HLSLBaseType_Half4x2:        return "half4x2";
	case HLSLBaseType_Half4x3:        return "half4x3";
	case HLSLBaseType_Half4x4:        return "half4x4";



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

	case HLSLBaseType_InputPatch:     return type.InputPatchName;
	case HLSLBaseType_OutputPatch:     return type.OutputPatchName;

	case HLSLBaseType_PointStream:		return "PointStream";
	case HLSLBaseType_LineStream:		return "LineStream";
	case HLSLBaseType_TriangleStream:	return "TriangleStream";

    case HLSLBaseType_Texture:      return "texture";

	case HLSLBaseType_Texture1D:      return "Texture1D";
	case HLSLBaseType_Texture1DArray:      return "Texture1DArray";
	case HLSLBaseType_Texture2D:      return "Texture2D";
	case HLSLBaseType_Texture2DArray:      return "Texture1DArray";
	case HLSLBaseType_Texture3D:      return "Texture3D";
	case HLSLBaseType_Texture2DMS:      return "Texture2DMS";
	case HLSLBaseType_Texture2DMSArray:      return "Texture2DMSArray";
	case HLSLBaseType_TextureCube:      return "TextureCube";
	case HLSLBaseType_TextureCubeArray:      return "TextureCubeArray";

	case HLSLBaseType_RWTexture1D:      return "RWTexture1D";
	case HLSLBaseType_RWTexture1DArray:      return "RWTexture1DArray";
	case HLSLBaseType_RWTexture2D:      return "RWTexture2D";
	case HLSLBaseType_RWTexture2DArray:      return "RWTexture1DArray";
	case HLSLBaseType_RWTexture3D:      return "RWTexture3D";

    case HLSLBaseType_Sampler:      return "sampler";
    case HLSLBaseType_Sampler2D:    return "sampler2D";
    case HLSLBaseType_Sampler3D:    return "sampler3D";
    case HLSLBaseType_SamplerCube:  return "samplerCUBE";
    case HLSLBaseType_Sampler2DShadow:  return "sampler2DShadow";
    case HLSLBaseType_Sampler2DMS:  return "sampler2DMS";
    case HLSLBaseType_Sampler2DArray:    return "sampler2DArray";
	case HLSLBaseType_SamplerState:  return "SamplerState";
    case HLSLBaseType_UserDefined:  return type.typeName;	

    default: return "<unknown type>";
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

HLSLGenerator::HLSLGenerator()
{
    m_tree                          = NULL;
    m_entryName                     = NULL;
    m_legacy                        = false;
    m_target                        = Target_VertexShader;
    m_isInsideBuffer                = false;
    m_textureSampler2DStruct[0]     = 0;
    m_textureSampler2DCtor[0]       = 0;
    m_textureSampler3DStruct[0]     = 0;
    m_textureSampler3DCtor[0]       = 0;
    m_textureSamplerCubeStruct[0]   = 0;
    m_textureSamplerCubeCtor[0]     = 0;
    m_tex2DFunction[0]              = 0;
    m_tex2DProjFunction[0]          = 0;
    m_tex2DLodFunction[0]           = 0;
    m_tex2DBiasFunction[0]          = 0;
    m_tex2DGradFunction[0]          = 0;
    m_tex2DGatherFunction[0]        = 0;
    m_tex2DSizeFunction[0]          = 0;
    m_tex2DFetchFunction[0]         = 0;
    m_tex2DCmpFunction[0]           = 0;
    m_tex2DMSFetchFunction[0]       = 0;
    m_tex3DFunction[0]              = 0;
    m_tex3DLodFunction[0]           = 0;
    m_tex3DBiasFunction[0]          = 0;
    m_texCubeFunction[0]            = 0;
    m_texCubeLodFunction[0]         = 0;
    m_texCubeBiasFunction[0]        = 0;

	m_Sample[0]						= 0;
	m_SampleLevel[0]				= 0;
	m_SampleBias[0]					= 0;
}


// @@ We need a better way of doing semantic replacement:
// - Look at the function being generated.
// - Return semantic, semantics associated to fields of the return structure, or output arguments, or fields of structures associated to output arguments -> output semantic replacement.
// - Semantics associated input arguments or fields of the input arguments -> input semantic replacement.
static const char * TranslateSemantic(const char* semantic, bool output, HLSLGenerator::Target target)
{
    if (target == HLSLGenerator::Target_VertexShader)
    {
        if (output) 
        {
            if (String_Equal("SV_POSITION", semantic))     return "SV_POSITION";
			
        }
        else {
            if (String_Equal("INSTANCE_ID", semantic))  return "SV_InstanceID";
        }
    }
    else if (target == HLSLGenerator::Target_PixelShader)
    {
        if (output)
        {
            if (String_Equal("DEPTH", semantic))      return "SV_Depth";
            if (String_Equal("COLOR", semantic))      return "SV_Target";
            if (String_Equal("COLOR0", semantic))     return "SV_Target0";
            if (String_Equal("COLOR0_1", semantic))   return "SV_Target1";
            if (String_Equal("COLOR1", semantic))     return "SV_Target1";
            if (String_Equal("COLOR2", semantic))     return "SV_Target2";
            if (String_Equal("COLOR3", semantic))     return "SV_Target3";
        }
        else
        {
            if (String_Equal("VPOS", semantic))       return "SV_POSITION";
            if (String_Equal("VFACE", semantic))      return "SV_IsFrontFace";    // bool   @@ Should we do type replacement too?
        }
    }
    return NULL;
}

/*
void HLSLGenerator::PrintPreprocessors(int currentLine)
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
*/

bool HLSLGenerator::Generate(HLSLTree* tree, Target target, const char* entryName, bool legacy)
{
    m_tree      = tree;
    m_entryName = entryName;
    m_target    = target;
    m_legacy    = legacy;
    m_isInsideBuffer = false;

	//memcpy_s(m_preprocessorPackage, sizeof(HLSLPreprocessorPackage) * 128, preprocessors, sizeof(HLSLPreprocessorPackage) * 128);

	/*
	for (int i = 0; i < 128; i++)
	{
		strcpy(m_preprocessorPackage[i].m_storedPreprocessors, preprocessorPackage[i].m_storedPreprocessors);
		m_preprocessorPackage[i].m_line = preprocessorPackage[i].m_line;		
	}
	*/

    m_writer.Reset();

    // @@ Should we generate an entirely new copy of the tree so that we can modify it in place?
    if (!legacy)
    {
        HLSLFunction * function = tree->FindFunction(entryName);

        // Handle return value semantics
        if (function->semantic != NULL) {
            function->sv_semantic = TranslateSemantic(function->semantic, /*output=*/true, target);
        }
        if (function->returnType.baseType == HLSLBaseType_UserDefined) {
            HLSLStruct * s = tree->FindGlobalStruct(function->returnType.typeName);

			HLSLStructField * sv_fields = NULL;

			HLSLStructField * lastField = NULL;
            HLSLStructField * field = s->field;
            while (field) {
				HLSLStructField * nextField = field->nextField;

                if (field->semantic) {
					field->hidden = false;
                    field->sv_semantic = TranslateSemantic(field->semantic, /*output=*/true, target);

					/*
					// Fields with SV semantics are stored at the end to avoid linkage problems.
					if (field->sv_semantic != NULL) {
						// Unlink from last.
						if (lastField != NULL) lastField->nextField = nextField;
						else s->field = nextField;

						// Add to sv_fields.
						field->nextField = sv_fields;
						sv_fields = field;
					}
					*/
                }

				field = field->nextField;

				//if (field != sv_fields) lastField = field;
                //field = nextField;
            }

			/*
			// Append SV fields at the end.
			if (sv_fields != NULL) {
				if (lastField == NULL) {
					s->field = sv_fields;
				}
				else {
					ASSERT(lastField->nextField == NULL);
					lastField->nextField = sv_fields;
				}
			}
			*/
        }

        // Handle argument semantics.
        // @@ It would be nice to flag arguments that are used by the program and skip or hide the unused ones.
        HLSLArgument * argument = function->argument;
        while (argument) {
            bool output = argument->modifier == HLSLArgumentModifier_Out;
            if (argument->semantic) {
                argument->sv_semantic = TranslateSemantic(argument->semantic, output, target); 
            }

            if (argument->type.baseType == HLSLBaseType_UserDefined) {
                HLSLStruct * s = tree->FindGlobalStruct(argument->type.typeName);

                HLSLStructField * field = s->field;
                while (field) {
                    if (field->semantic) {
						field->hidden = false;

						/*
						if (target == Target_PixelShader && !output && String_EqualNoCase(field->semantic, "POSITION")) {
							ASSERT(String_EqualNoCase(field->sv_semantic, "SV_POSITION"));
							field->hidden = true;
						}
						*/

                        field->sv_semantic = TranslateSemantic(field->semantic, output, target);
                    }

                    field = field->nextField;
                }
            }

            argument = argument->nextArgument;
        }
    }

    ChooseUniqueName("TextureSampler2D",            m_textureSampler2DStruct,   sizeof(m_textureSampler2DStruct));
    ChooseUniqueName("CreateTextureSampler2D",      m_textureSampler2DCtor,     sizeof(m_textureSampler2DCtor));
    ChooseUniqueName("TextureSampler2DShadow",      m_textureSampler2DShadowStruct, sizeof(m_textureSampler2DShadowStruct));
    ChooseUniqueName("CreateTextureSampler2DShadow",m_textureSampler2DShadowCtor,   sizeof(m_textureSampler2DShadowCtor));
    ChooseUniqueName("TextureSampler3D",            m_textureSampler3DStruct,   sizeof(m_textureSampler3DStruct));
    ChooseUniqueName("CreateTextureSampler3D",      m_textureSampler3DCtor,     sizeof(m_textureSampler3DCtor));
    ChooseUniqueName("TextureSamplerCube",          m_textureSamplerCubeStruct, sizeof(m_textureSamplerCubeStruct));
    ChooseUniqueName("CreateTextureSamplerCube",    m_textureSamplerCubeCtor,   sizeof(m_textureSamplerCubeCtor));
    ChooseUniqueName("tex2D",                       m_tex2DFunction,            sizeof(m_tex2DFunction));
    ChooseUniqueName("tex2Dproj",                   m_tex2DProjFunction,        sizeof(m_tex2DProjFunction));
    ChooseUniqueName("tex2Dlod",                    m_tex2DLodFunction,         sizeof(m_tex2DLodFunction));
    ChooseUniqueName("tex2Dbias",                   m_tex2DBiasFunction,        sizeof(m_tex2DBiasFunction));
    ChooseUniqueName("tex2Dgrad",                   m_tex2DGradFunction,        sizeof(m_tex2DGradFunction));
    ChooseUniqueName("tex2Dgather",                 m_tex2DGatherFunction,      sizeof(m_tex2DGatherFunction));
    ChooseUniqueName("tex2Dsize",                   m_tex2DSizeFunction,        sizeof(m_tex2DSizeFunction));
    ChooseUniqueName("tex2Dfetch",                  m_tex2DFetchFunction,       sizeof(m_tex2DFetchFunction));
    ChooseUniqueName("tex2Dcmp",                    m_tex2DCmpFunction,         sizeof(m_tex2DCmpFunction));
    ChooseUniqueName("tex2DMSfetch",                m_tex2DMSFetchFunction,     sizeof(m_tex2DMSFetchFunction));
    ChooseUniqueName("tex2DMSsize",                 m_tex2DMSSizeFunction,      sizeof(m_tex2DMSSizeFunction));
    ChooseUniqueName("tex3D",                       m_tex3DFunction,            sizeof(m_tex3DFunction));
    ChooseUniqueName("tex3Dlod",                    m_tex3DLodFunction,         sizeof(m_tex3DLodFunction));
    ChooseUniqueName("tex3Dbias",                   m_tex3DBiasFunction,        sizeof(m_tex3DBiasFunction));
    ChooseUniqueName("tex3Dsize",                   m_tex3DSizeFunction,        sizeof(m_tex3DSizeFunction));
    ChooseUniqueName("texCUBE",                     m_texCubeFunction,          sizeof(m_texCubeFunction));
    ChooseUniqueName("texCUBElod",                  m_texCubeLodFunction,       sizeof(m_texCubeLodFunction));
    ChooseUniqueName("texCUBEbias",                 m_texCubeBiasFunction,      sizeof(m_texCubeBiasFunction));
    ChooseUniqueName("texCUBEsize",                 m_texCubeSizeFunction,      sizeof(m_texCubeSizeFunction));

	ChooseUniqueName("Sample",						m_Sample,					sizeof(m_Sample));
	ChooseUniqueName("SampleLevel",					m_SampleLevel,				sizeof(m_SampleLevel));
	ChooseUniqueName("SampleBias",					m_SampleBias,				sizeof(m_SampleBias));

	

    if (!m_legacy)
    {
		//write preprocessors

		//m_parser

        // @@ Only emit code for sampler types that are actually used?
		/*
        m_writer.WriteLine(0, "struct %s {", m_textureSampler2DStruct);
        m_writer.WriteLine(1, "Texture2D    t;");
        m_writer.WriteLine(1, "SamplerState s;");
        m_writer.WriteLine(0, "};");

        m_writer.WriteLine(0, "struct %s {", m_textureSampler2DShadowStruct);
        m_writer.WriteLine(1, "Texture2D                t;");
        m_writer.WriteLine(1, "SamplerComparisonState   s;");
        m_writer.WriteLine(0, "};");

        m_writer.WriteLine(0, "struct %s {", m_textureSampler3DStruct);
        m_writer.WriteLine(1, "Texture3D    t;");
        m_writer.WriteLine(1, "SamplerState s;");
        m_writer.WriteLine(0, "};");

        m_writer.WriteLine(0, "struct %s {", m_textureSamplerCubeStruct);
        m_writer.WriteLine(1, "TextureCube  t;");
        m_writer.WriteLine(1, "SamplerState s;");
        m_writer.WriteLine(0, "};");

        m_writer.WriteLine(0, "%s %s(Texture2D t, SamplerState s) {", m_textureSampler2DStruct, m_textureSampler2DCtor);
        m_writer.WriteLine(1, "%s ts;", m_textureSampler2DStruct);
        m_writer.WriteLine(1, "ts.t = t; ts.s = s;");
        m_writer.WriteLine(1, "return ts;");
        m_writer.WriteLine(0, "}");

        m_writer.WriteLine(0, "%s %s(Texture2D t, SamplerComparisonState s) {", m_textureSampler2DShadowStruct, m_textureSampler2DShadowCtor);
        m_writer.WriteLine(1, "%s ts;", m_textureSampler2DShadowStruct);
        m_writer.WriteLine(1, "ts.t = t; ts.s = s;");
        m_writer.WriteLine(1, "return ts;");
        m_writer.WriteLine(0, "}");

        m_writer.WriteLine(0, "%s %s(Texture3D t, SamplerState s) {", m_textureSampler3DStruct, m_textureSampler3DCtor);
        m_writer.WriteLine(1, "%s ts;", m_textureSampler3DStruct);
        m_writer.WriteLine(1, "ts.t = t; ts.s = s;");
        m_writer.WriteLine(1, "return ts;");
        m_writer.WriteLine(0, "}");

        m_writer.WriteLine(0, "%s %s(TextureCube t, SamplerState s) {", m_textureSamplerCubeStruct, m_textureSamplerCubeCtor);
        m_writer.WriteLine(1, "%s ts;", m_textureSamplerCubeStruct);
        m_writer.WriteLine(1, "ts.t = t; ts.s = s;");
        m_writer.WriteLine(1, "return ts;");
        m_writer.WriteLine(0, "}");
        */
        if (m_tree->GetContainsString("tex2D")) 
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float2 texCoord) {", m_tex2DFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dproj"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", m_tex2DProjFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord.xy / texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dlod"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord, int2 offset=0) {", m_tex2DLodFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "return ts.t.SampleLevel(ts.s, texCoord.xy, texCoord.w, offset);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dbias"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", m_tex2DBiasFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "return ts.t.SampleBias(ts.s, texCoord.xy, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dgrad"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float2 texCoord, float2 ddx, float2 ddy) {", m_tex2DGradFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "return ts.t.SampleGrad(ts.s, texCoord.xy, ddx, ddy);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dgather"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float2 texCoord, int component, int2 offset=0) {", m_tex2DGatherFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "if(component == 0) return ts.t.GatherRed(ts.s, texCoord, offset);");
            m_writer.WriteLine(1, "if(component == 1) return ts.t.GatherGreen(ts.s, texCoord, offset);");
            m_writer.WriteLine(1, "if(component == 2) return ts.t.GatherBlue(ts.s, texCoord, offset);");
            m_writer.WriteLine(1, "return ts.t.GatherAlpha(ts.s, texCoord, offset);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dsize"))
        {
            m_writer.WriteLine(0, "int2 %s(%s ts) {", m_tex2DSizeFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "int2 size; ts.t.GetDimensions(size.x, size.y); return size;");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dfetch"))
        {
            m_writer.WriteLine(0, "int2 %s(%s ts, int3 texCoord) {", m_tex2DFetchFunction, m_textureSampler2DStruct);
            m_writer.WriteLine(1, "return ts.t.Load(texCoord.xyz);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2Dcmp"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", m_tex2DCmpFunction, m_textureSampler2DShadowStruct);
            m_writer.WriteLine(1, "return ts.t.SampleCmpLevelZero(ts.s, texCoord.xy, texCoord.z);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2DMSfetch"))
        {
            m_writer.WriteLine(0, "float4 %s(Texture2DMS<float4> t, int2 texCoord, int sample) {", m_tex2DMSFetchFunction);
            m_writer.WriteLine(1, "return t.Load(texCoord, sample);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex2DMSsize"))
        {
            m_writer.WriteLine(0, "int3 %s(Texture2DMS<float4> t) {", m_tex2DMSSizeFunction);
            m_writer.WriteLine(1, "int3 size; t.GetDimensions(size.x, size.y, size.z); return size;");   // @@ Not tested, does this return the number of samples in the third argument?
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex3D"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float3 texCoord) {", m_tex3DFunction, m_textureSampler3DStruct);
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex3Dlod"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", m_tex3DLodFunction, m_textureSampler3DStruct);
            m_writer.WriteLine(1, "return ts.t.SampleLevel(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex3Dbias"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", m_tex3DBiasFunction, m_textureSampler3DStruct);
            m_writer.WriteLine(1, "return ts.t.SampleBias(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("tex3Dsize"))
        {
            m_writer.WriteLine(0, "int3 %s(%s ts) {", m_tex3DSizeFunction, m_textureSampler3DStruct);
            m_writer.WriteLine(1, "int3 size; ts.t.GetDimensions(size.x, size.y, size.z); return size;");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("texCUBE"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float3 texCoord) {", m_texCubeFunction, m_textureSamplerCubeStruct);
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("texCUBElod"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", m_texCubeLodFunction, m_textureSamplerCubeStruct);
            m_writer.WriteLine(1, "return ts.t.SampleLevel(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("texCUBEbias"))
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", m_texCubeBiasFunction, m_textureSamplerCubeStruct);
            m_writer.WriteLine(1, "return ts.t.SampleBias(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (m_tree->GetContainsString("texCUBEsize"))
        {
            m_writer.WriteLine(0, "int %s(%s ts) {", m_texCubeSizeFunction, m_textureSamplerCubeStruct);
            m_writer.WriteLine(1, "int size; ts.t.GetDimensions(size); return size;");   // @@ Not tested, does this return a single value?
            m_writer.WriteLine(0, "}");
        }
    }
	
	//m_writer.WriteLine(0, "#define DIRECT3D12 1");	

    HLSLRoot* root = m_tree->GetRoot();
    OutputStatements(0, root->statement);

    m_tree = NULL;
    return true;
}

const char* HLSLGenerator::GetResult() const
{
    return m_writer.GetResult();
}

void HLSLGenerator::OutputExpressionList(HLSLExpression* expression)
{
    int numExpressions = 0;
    while (expression != NULL)
    {
        if (numExpressions > 0)
        {
            m_writer.Write(", ");
        }
        OutputExpression(expression, true);
        expression = expression->nextExpression;
        ++numExpressions;
    }
}

void HLSLGenerator::OutputExpression(HLSLExpression* expression, bool needsEndParen)
{
	if (expression->childExpression)
	{
		m_writer.Write("{");
			OutputExpressionList(expression->childExpression);
		m_writer.Write("}");
	}
    else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
        const char* name = identifierExpression->name;
        if (!m_legacy && IsSamplerType(identifierExpression->expressionType) && identifierExpression->global)
        {
            // @@ Handle generic sampler type.

            if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2D)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", m_textureSampler2DCtor, name, name);
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler3D)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", m_textureSampler3DCtor, name, name);
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_SamplerCube)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", m_textureSamplerCubeCtor, name, name);
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DShadow)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", m_textureSampler2DShadowCtor, name, name);
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DMS)
            {
                m_writer.Write("%s", name);
            }
        }		
		else if (expression->functionExpression)
		{
			// if it is buffer type and has function expression
		
			HLSLBuffer* buffer = m_tree->FindBuffer(name);
			HLSLTextureStateExpression* tse = m_tree->FindTextureStateExpression(name);
			if (buffer)
			{
				m_writer.Write("%s", buffer->name);

				if (buffer->bArray)
				{
					for (int i = 0; i < (int)buffer->arrayDimension; i++)
					{
						if (!String_Equal(buffer->arrayIdentifier[i], ""))
							m_writer.Write("[%s]", buffer->arrayIdentifier);
						else if (buffer->arrayIndex[i] > 0)
							m_writer.Write("[%u]", buffer->arrayIndex[i]);
						else
							m_writer.Write("[]");
					}
				}
			}
			/*
			else if (tse)
			{
				m_writer.Write("%s", tse->name);

				if (tse->bArray)
				{
					for (int i = 0; i < (int)tse->arrayDimension; i++)
					{
						if (!String_Equal(tse->arrayIdentifier[i], ""))
							m_writer.Write("[%s]", tse->arrayIdentifier);
						else if (tse->arrayIndex[i] > 0)
							m_writer.Write("[%u]", tse->arrayIndex[i]);
						else
							m_writer.Write("[]");
					}
				}
			}
			*/
			else
			{
				m_writer.Write("%s", name);
			}

			

			OutputExpression(expression->functionExpression, needsEndParen);
		}
        else
        {
            m_writer.Write("%s", name);
        }
    }
    else if (expression->nodeType == HLSLNodeType_CastingExpression)
    {
        HLSLCastingExpression* castingExpression = static_cast<HLSLCastingExpression*>(expression);
        m_writer.Write("(");
        OutputDeclaration(castingExpression->type, "");
        m_writer.Write(")");
        OutputExpression(castingExpression->expression, false);
    }
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression* constructorExpression = static_cast<HLSLConstructorExpression*>(expression);
        m_writer.Write("%s(", GetTypeName(constructorExpression->type));
        OutputExpressionList(constructorExpression->argument);
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
		case HLSLBaseType_Uint:
			m_writer.Write("%u", literalExpression->uiValue);
			break;
        case HLSLBaseType_Int:
            m_writer.Write("%d", literalExpression->iValue);
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
        switch (unaryExpression->unaryOp)
        {
        case HLSLUnaryOp_Negative:      op = "-";  break;
        case HLSLUnaryOp_Positive:      op = "+";  break;
        case HLSLUnaryOp_Not:           op = "!";  break;
        case HLSLUnaryOp_PreIncrement:  op = "++"; break;
        case HLSLUnaryOp_PreDecrement:  op = "--"; break;
        case HLSLUnaryOp_PostIncrement: op = "++"; pre = false; break;
        case HLSLUnaryOp_PostDecrement: op = "--"; pre = false; break;
        case HLSLUnaryOp_BitNot:        op = "~";  break;
        }
        //m_writer.Write("(");
        if (pre)
        {
            m_writer.Write("%s", op);
            OutputExpression(unaryExpression->expression, needsEndParen);
        }
        else
        {
            OutputExpression(unaryExpression->expression, needsEndParen);
            m_writer.Write("%s", op);
        }
       // m_writer.Write(")");
    }
    else if (expression->nodeType == HLSLNodeType_BinaryExpression)
    {
        HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(expression);
       

		if (needsEndParen)
			m_writer.Write("(");

        OutputExpression(binaryExpression->expression1, needsEndParen);
        const char* op = "?";
        switch (binaryExpression->binaryOp)
        {
        case HLSLBinaryOp_Add:          op = " + "; break;
        case HLSLBinaryOp_Sub:          op = " - "; break;
        case HLSLBinaryOp_Mul:          op = " * "; break;
        case HLSLBinaryOp_Div:          op = " / "; break;
        case HLSLBinaryOp_Less:         op = " < "; break;
        case HLSLBinaryOp_Greater:      op = " > "; break;
        case HLSLBinaryOp_LessEqual:    op = " <= "; break;
        case HLSLBinaryOp_GreaterEqual: op = " >= "; break;
        case HLSLBinaryOp_Equal:        op = " == "; break;
        case HLSLBinaryOp_NotEqual:     op = " != "; break;
        case HLSLBinaryOp_Assign:       op = " = "; break;
        case HLSLBinaryOp_AddAssign:    op = " += "; break;
        case HLSLBinaryOp_SubAssign:    op = " -= "; break;
        case HLSLBinaryOp_MulAssign:    op = " *= "; break;
        case HLSLBinaryOp_DivAssign:    op = " /= "; break;
        case HLSLBinaryOp_And:          op = " && "; break;
        case HLSLBinaryOp_Or:           op = " || "; break;
		case HLSLBinaryOp_BitAnd:       op = " & "; break;
        case HLSLBinaryOp_BitOr:        op = " | "; break;
        case HLSLBinaryOp_BitXor:       op = " ^ "; break;
		case HLSLBinaryOp_LeftShift:    op = " << "; break;
		case HLSLBinaryOp_RightShift:   op = " >> "; break;
		case HLSLBinaryOp_Modular:      op = " % "; break;
		case HLSLBinaryOp_BitAndAssign: op = " &= "; break;
		case HLSLBinaryOp_BitOrAssign:  op = " |= "; break;
		case HLSLBinaryOp_BitXorAssign: op = " ^= "; break;
        default:
            ASSERT(0);
        }
        m_writer.Write("%s", op);
        OutputExpression(binaryExpression->expression2, true);
       
		if (needsEndParen)
			m_writer.Write(")");

    }
    else if (expression->nodeType == HLSLNodeType_ConditionalExpression)
    {
        HLSLConditionalExpression* conditionalExpression = static_cast<HLSLConditionalExpression*>(expression);
        m_writer.Write("((");
        OutputExpression(conditionalExpression->condition, needsEndParen);
        m_writer.Write(")?(");
        OutputExpression(conditionalExpression->trueExpression, needsEndParen);
        m_writer.Write("):(");
        OutputExpression(conditionalExpression->falseExpression, needsEndParen);
        m_writer.Write("))");
    }
    else if (expression->nodeType == HLSLNodeType_MemberAccess)
    {
        HLSLMemberAccess* memberAccess = static_cast<HLSLMemberAccess*>(expression);
        //m_writer.Write("(");
        
		OutputExpression(memberAccess->object, true);
		
		//m_writer.Write(").%s", memberAccess->field);
		
		m_writer.Write(".%s", memberAccess->field);
		
		if ((memberAccess->object->expressionType.baseType == HLSLBaseType_PointStream) ||
			(memberAccess->object->expressionType.baseType == HLSLBaseType_LineStream) ||
			(memberAccess->object->expressionType.baseType == HLSLBaseType_TriangleStream))
		{
			m_writer.Write("(");
			OutputExpression(memberAccess->functionExpression, needsEndParen);
			m_writer.Write(")");
		}
    }
    else if (expression->nodeType == HLSLNodeType_ArrayAccess)
    {
        HLSLArrayAccess* arrayAccess = static_cast<HLSLArrayAccess*>(expression);
        OutputExpression(arrayAccess->array, needsEndParen);
        m_writer.Write("[");
        OutputExpression(arrayAccess->index, needsEndParen);
        m_writer.Write("]");
    }
    else if (expression->nodeType == HLSLNodeType_FunctionCall)
    {
        HLSLFunctionCall* functionCall = static_cast<HLSLFunctionCall*>(expression);
        const char* name = functionCall->function->name;
        if (!m_legacy)
        {
            if (String_Equal(name, "tex2D"))
            {
                name = m_tex2DFunction;
            }
            else if (String_Equal(name, "tex2Dproj"))
            {
                name = m_tex2DProjFunction;
            }
            else if (String_Equal(name, "tex2Dlod"))
            {
                name = m_tex2DLodFunction;
            }
            else if (String_Equal(name, "tex2Dbias"))
            {
                name = m_tex2DBiasFunction;
            }
            else if (String_Equal(name, "tex2Dgrad"))
            {
                name = m_tex2DGradFunction;
            }
            else if (String_Equal(name, "tex2Dgather"))
            {
                name = m_tex2DGatherFunction;
            }
            else if (String_Equal(name, "tex2Dsize"))
            {
                name = m_tex2DSizeFunction;
            }
            else if (String_Equal(name, "tex2Dfetch"))
            {
                name = m_tex2DFetchFunction;
            }
            else if (String_Equal(name, "tex2Dcmp"))
            {
                name = m_tex2DCmpFunction;
            }
            else if (String_Equal(name, "tex2DMSfetch"))
            {
                name = m_tex2DMSFetchFunction;
            }
            else if (String_Equal(name, "tex2DMSsize"))
            {
                name = m_tex2DMSSizeFunction;
            }
            else if (String_Equal(name, "tex3D"))
            {
                name = m_tex3DFunction;
            }
            else if (String_Equal(name, "tex3Dlod"))
            {
                name = m_tex3DLodFunction;
            }
            else if (String_Equal(name, "tex3Dbias"))
            {
                name = m_tex3DBiasFunction;
            }
            else if (String_Equal(name, "tex3Dsize"))
            {
                name = m_tex3DSizeFunction;
            }
            else if (String_Equal(name, "texCUBE"))
            {
                name = m_texCubeFunction;
            }
            else if (String_Equal(name, "texCUBElod"))
            {
                name = m_texCubeLodFunction;
            }
            else if (String_Equal(name, "texCUBEbias"))
            {
                name = m_texCubeBiasFunction;
            }
            else if (String_Equal(name, "texCUBEsize"))
            {
                name = m_texCubeSizeFunction;
            }	
        }
		
		if (String_Equal(name, "Sample") || String_Equal(name, "SampleLevel") || String_Equal(name, "SampleCmp") || String_Equal(name, "SampleCmpLevelZero") || String_Equal(name, "SampleBias") || String_Equal(name, "SampleGrad") || String_Equal(name, "GatherRed"))
		{		

			if (functionCall->pTextureStateExpression)
			{
				const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;				

			}

			m_writer.Write(".%s(", name);
			OutputExpressionList(functionCall->argument);
			m_writer.Write(")");
		}
		else if(String_Equal(name, "Load") || String_Equal(name, "Store") || String_Equal(name, "GetDimensions"))
		{
			if (functionCall->pTextureStateExpression)
			{
				const HLSLTextureStateExpression* pTextureStateExpression = functionCall->pTextureStateExpression;

				//m_writer.Write(" %s", pTextureStateExpression->name);

				if (pTextureStateExpression->bArray)
				{
					for (int i = 0; i < (int)pTextureStateExpression->arrayDimension; i++)
					{
						//if (!String_Equal(pTextureStateExpression->arrayIdentifier[i], ""))
						//	m_writer.Write("[%s]", pTextureStateExpression->arrayIdentifier);
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

				if (pTextureStateExpression->indexExpression)
				{
					m_writer.Write("[");
					OutputExpressionList(pTextureStateExpression->indexExpression);
					m_writer.Write("]");
				}
			}

			

			m_writer.Write(".%s(", name);
			OutputExpressionList(functionCall->argument);
			m_writer.Write(")");
		}
		else		
		{
			m_writer.Write("%s(", name);
			OutputExpressionList(functionCall->argument);
			m_writer.Write(")");
		}

       
    }
	else if(expression->nodeType == HLSLNodeType_PreprocessorExpression)
	{
		HLSLPreprocessorExpression* preprecessorExpression = static_cast<HLSLPreprocessorExpression*>(expression);
		
		switch (preprecessorExpression->type)
		{
		case HLSLBaseType_PreProcessorIf: 
			m_writer.WriteLine(0, "#if %s", preprecessorExpression->contents);
			break;
		case HLSLBaseType_PreProcessorElif:
			m_writer.WriteLine(0, "#elif %s", preprecessorExpression->contents);
			break;
		case HLSLBaseType_PreProcessorElse:
			m_writer.WriteLine(0, "#else");
			break;
		case HLSLBaseType_PreProcessorEndif:
			m_writer.WriteLine(0, "#endif");
			break;
		case HLSLBaseType_PreProcessorIfDef:
			m_writer.WriteLine(0, "#ifdef %s", preprecessorExpression->contents);
			break;
		case HLSLBaseType_PreProcessorIfnDef:
			m_writer.WriteLine(0, "#ifndef %s", preprecessorExpression->contents);
			break;
		case HLSLBaseType_PreProcessorUndef:
			m_writer.WriteLine(0, "#undef %s", preprecessorExpression->contents);
			break;
		case HLSLBaseType_PreProcessorInclude:
			m_writer.WriteLine(0, "#include %s", preprecessorExpression->contents);
			break;
		case HLSLBaseType_PreProcessorLine:
			
			break;
		case HLSLBaseType_PreProcessorPragma:
			
			break;
		default:
			break;
		}

		m_writer.Write(0, "%s", preprecessorExpression->name);		
	}
	else if (expression->nodeType == HLSLNodeType_SamplerStateExpression)
	{
		HLSLSamplerStateExpression* samplerStateExpression = static_cast<HLSLSamplerStateExpression*>(expression);

		m_writer.Write("%s", samplerStateExpression->name);

	}
	else if (expression->nodeType == HLSLNodeType_TextureStateExpression)
	{
		HLSLTextureStateExpression* textureStateExpression = static_cast<HLSLTextureStateExpression*>(expression);		
				

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
				else if (!String_Equal(textureStateExpression->arrayIdentifier[i], ""))
					m_writer.Write("[%s]", textureStateExpression->arrayIdentifier);
				else if (textureStateExpression->arrayIndex[i] > 0)
					m_writer.Write("[%u]", textureStateExpression->arrayIndex[i]);
				else
					m_writer.Write("[]");
			}
		}

		if (textureStateExpression->indexExpression)
		{
			m_writer.Write("[");
			OutputExpressionList(textureStateExpression->indexExpression);
			m_writer.Write("]");
		}
		
		if (textureStateExpression->functionExpression)
		{
			OutputExpression(textureStateExpression->functionExpression, false);
		}
				
	}
    else
    {
        m_writer.Write("<unknown expression>");
    }
}

void HLSLGenerator::OutputArguments(HLSLArgument* argument)
{
    int numArgs = 0;

	bool bcount = true;

	HLSLArgument* prevArg = NULL;

    while (argument != NULL)
    {
		if (prevArg == NULL)
		{

		}
		else if (prevArg->preprocessor == NULL && numArgs > 0)
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
        case HLSLArgumentModifier_Uniform:
            m_writer.Write("uniform ");
            break;
		case HLSLArgumentModifier_Point:
			m_writer.Write("point ");
			break;
		case HLSLArgumentModifier_Line:
			m_writer.Write("line ");
			break;
		case HLSLArgumentModifier_Triangle:
			m_writer.Write("triangle ");
			break;
		case HLSLArgumentModifier_Lineadj:
			m_writer.Write("lineadj ");
			break;
		case HLSLArgumentModifier_Triangleadj:
			m_writer.Write("triangleadj ");
			break;

        }

        const char * semantic = argument->sv_semantic ? argument->sv_semantic : argument->semantic;

		if (argument->preprocessor)
		{

		}
		else
		{
			OutputDeclaration(argument->type, argument->name, semantic, /*registerName=*/NULL, argument->defaultValue);

			if(bcount)
			++numArgs;
		}

		prevArg = argument;
		argument = argument->nextArgument;
		
    }
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
	if (attributeType == HLSLAttributeType_MaxtessFactor) return "maxtessfactor";

	if (attributeType == HLSLAttributeType_EarlyDepthStencil) return "earlydepthstencil";

    return NULL;
}

void HLSLGenerator::OutputAttributes(int indent, HLSLAttribute* attribute)
{
    while (attribute != NULL)
    {
        const char * attributeName = GetAttributeName(attribute->attributeType);
    
        if (attributeName != NULL)
        {
			if (String_Equal(attributeName, "numthreads"))
			{
				m_writer.Write("[%s(", attributeName);

				if (attribute->numGroupX != 0)
					m_writer.Write("%d, ", attribute->numGroupX);
				else
					m_writer.Write("%s, ", attribute->numGroupXstr);
					
				if(attribute->numGroupY != 0)
					m_writer.Write("%d, ", attribute->numGroupY);
				else
					m_writer.Write("%s, ", attribute->numGroupYstr);
					
					
				if(attribute->numGroupZ != 0)
					m_writer.Write("%d", attribute->numGroupZ);
				else
					m_writer.Write("%s", attribute->numGroupZstr);

				m_writer.Write(")]");
				m_writer.WriteLine(indent, "");

			}
			else if (String_Equal(attributeName, "maxvertexcount"))
			{
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%d)]", attributeName, attribute->maxVertexCount);
			}
			else if (String_Equal(attributeName, "unroll"))
			{
				if (attribute->unrollIdentifier)
					m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%s)]", attributeName, attribute->unrollIdentifier);
				else
				{
					if(attribute->unrollCount == 0)
						m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s]", attributeName);
					else
						m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%d)]", attributeName, attribute->unrollCount);
				}
			}
			else if (String_Equal(attributeName, "domain"))
			{
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%s)]", attributeName, attribute->domain);
			}
			else if (String_Equal(attributeName, "partitioning"))
			{
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%s)]", attributeName, attribute->partitioning);
			}
			else if (String_Equal(attributeName, "outputtopology"))
			{
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%s)]", attributeName, attribute->outputtopology);
			}
			else if (String_Equal(attributeName, "outputcontrolpoints"))
			{
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%d)]", attributeName, attribute->outputcontrolpoints);
			}
			else if (String_Equal(attributeName, "patchconstantfunc"))
			{
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%s)]", attributeName, attribute->patchconstantfunc);
			}
			else if (String_Equal(attributeName, "maxtessfactor"))
			{
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s(%f)]", attributeName, attribute->maxTessellationFactor);
			}
			else
				m_writer.WriteLineTagged(indent, attribute->fileName, attribute->line, "[%s]", attributeName);
        }

		

        attribute = attribute->nextAttribute;
    }
}

void HLSLGenerator::OutputStatements(int indent, HLSLStatement* statement)
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
			case HLSLBaseType_Empty:
				m_writer.WriteLine(0, "%s", preprocessor->name);
				break;
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

					m_writer.WriteLine(0, "]");		
				}
				break;
			case HLSLBaseType_PreProcessorDefine:				
				m_writer.WriteLine(0, "#define %s", preprocessor->name);
				break;
			case HLSLBaseType_PreProcessorIf:
				m_writer.WriteLine(0, "#if %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorElif:
				m_writer.WriteLine(0, "#elif %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorElse:
				m_writer.WriteLine(0, "#else");
				break;
			case HLSLBaseType_PreProcessorEndif:
				m_writer.WriteLine(0, "#endif");
				break;
			case HLSLBaseType_PreProcessorIfDef:
				m_writer.WriteLine(0, "#ifdef %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorIfnDef:
				m_writer.WriteLine(0, "#ifndef %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorUndef: 
				m_writer.WriteLine(0, "#undef %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorInclude: 
				m_writer.WriteLine(0, "#include %s", preprocessor->contents);
				break;
			case HLSLBaseType_PreProcessorLine: 
				break;
			case HLSLBaseType_PreProcessorPragma:
				break;
			default:

				if(preprocessor->expression)
				{
					m_writer.Write("#define %s ", preprocessor->name);
					HLSLExpression* expression = preprocessor->expression;

					while (expression)
					{
						OutputExpression(expression, true);
						expression = expression->nextExpression;
					}
				}

				m_writer.EndLine("");

				break;
			}

			//m_writer.EndLine("");

		}		
        else if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);
			
			//PrintPreprocessors(declaration->line);
            m_writer.BeginLine(indent, declaration->fileName, declaration->line);
            OutputDeclaration(declaration);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
            m_writer.WriteLineTagged(indent, structure->fileName, structure->line, "struct %s", structure->name);
			m_writer.WriteLine(indent, "{");
            HLSLStructField* field = structure->field;
            while (field != NULL)
            {
                if (!field->hidden)
                {
					if (field->preProcessor)
					{
						if(field->preProcessor->expression)
							OutputExpression(field->preProcessor->expression, true);
						else
						{
							switch (field->preProcessor->type)
							{
							case HLSLBaseType_PreProcessorIf: 
								m_writer.WriteLine(0, "#if %s", field->preProcessor->contents);
								break;
							case HLSLBaseType_PreProcessorElif:
								m_writer.WriteLine(0, "#elif %s", field->preProcessor->contents);
								break;
							case HLSLBaseType_PreProcessorElse:
								m_writer.WriteLine(0, "#else");
								break;
							case HLSLBaseType_PreProcessorEndif:
								m_writer.WriteLine(0, "#endif");
								break;
							case HLSLBaseType_PreProcessorIfDef:
								m_writer.WriteLine(0, "#ifdef %s", field->preProcessor->contents);
								break;
							case HLSLBaseType_PreProcessorIfnDef:
								m_writer.WriteLine(0, "#ifndef %s", field->preProcessor->contents);
								break;
							case HLSLBaseType_PreProcessorUndef:
								m_writer.WriteLine(0, "#undef %s", field->preProcessor->contents);
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
						m_writer.BeginLine(indent + 1, field->fileName, field->line);
						const char * semantic = field->sv_semantic ? field->sv_semantic : field->semantic;
						OutputDeclaration(field->type, field->name, semantic);
						m_writer.Write(";");
						m_writer.EndLine();
					}                   
                }
                field = field->nextField;
            }
            m_writer.WriteLine(indent, "};");
        }
		else if (statement->nodeType == HLSLNodeType_TextureState)
		{
			HLSLTextureState* textureState = static_cast<HLSLTextureState*>(statement);

			//PrintPreprocessors(textureState->line);
			m_writer.BeginLine(indent, textureState->fileName, textureState->line);

			switch (textureState->type.baseType)
			{
				case HLSLBaseType_Texture1D :
					m_writer.Write("Texture1D");
					break;
				case HLSLBaseType_Texture1DArray :
					m_writer.Write("Texture1DArray");
					break;
				case HLSLBaseType_Texture2D :
					m_writer.Write("Texture2D");
					break;
				case HLSLBaseType_Texture2DArray :
					m_writer.Write("Texture2DArray");
					break;
				case HLSLBaseType_Texture3D :
					m_writer.Write("Texture3D");
					break;
				case HLSLBaseType_Texture2DMS :
					m_writer.Write("Texture2DMS");
					break;
				case HLSLBaseType_Texture2DMSArray :
					m_writer.Write("Texture2DMSArray");
					break;
				case HLSLBaseType_TextureCube :
					m_writer.Write("TextureCube");
					break;
				case HLSLBaseType_TextureCubeArray:
					m_writer.Write("TextureCubeArray");
					break;
				case HLSLBaseType_RWTexture1D:
					m_writer.Write("RWTexture1D");
					break;
				case HLSLBaseType_RWTexture1DArray:
					m_writer.Write("RWTexture1DArray");
					break;
				case HLSLBaseType_RWTexture2D:
					m_writer.Write("RWTexture2D");
					break;
				case HLSLBaseType_RWTexture2DArray:
					m_writer.Write("RWTexture2DArray");
					break;
				case HLSLBaseType_RWTexture3D:
					m_writer.Write("RWTexture3D");
					break;
				case HLSLBaseType_RasterizerOrderedTexture1D:
					m_writer.Write("RasterizerOrderedTexture1D");
					break;
				case HLSLBaseType_RasterizerOrderedTexture1DArray:
					m_writer.Write("RasterizerOrderedTexture1DArray");
					break;
				case HLSLBaseType_RasterizerOrderedTexture2D:
					m_writer.Write("RasterizerOrderedTexture2D");
					break;
				case HLSLBaseType_RasterizerOrderedTexture2DArray:
					m_writer.Write("RasterizerOrderedTexture2DArray");
					break;
				case HLSLBaseType_RasterizerOrderedTexture3D:
					m_writer.Write("RasterizerOrderedTexture3D");
					break;
				default:
					break;
			}
			
			if (textureState->type.elementType != HLSLBaseType_Unknown)
			{
				m_writer.Write("<");
				m_writer.Write("%s", getElementTypeAsStr(textureState->type));
			}


			
			if (textureState->type.baseType == HLSLBaseType_Texture2DMS || textureState->type.baseType == HLSLBaseType_Texture2DMSArray)
			{
				if(textureState->sampleIdentifier)
					m_writer.Write(", %s", textureState->sampleIdentifier);
				else
					m_writer.Write(", %d", textureState->sampleCount);				
			}


			if (textureState->type.elementType != HLSLBaseType_Unknown || 			
				textureState->type.baseType == HLSLBaseType_Texture2DMS ||
				textureState->type.baseType == HLSLBaseType_Texture2DMSArray
				)
			{
				m_writer.Write(">");
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

			if (textureState->registerName != NULL)
			{
				if (textureState->registerSpaceName != NULL)
				{
					m_writer.Write(" : register(%s", textureState->registerName);
					m_writer.Write(", %s)", textureState->registerSpaceName);
				}
				else
					m_writer.Write(" : register(%s)", textureState->registerName);
			}

			m_writer.EndLine(";");
		}	
		else if (statement->nodeType == HLSLNodeType_GroupShared)
		{
			HLSLGroupShared* pGroupShared = static_cast<HLSLGroupShared*>(statement);

			m_writer.Write(0, "groupshared ");
			OutputDeclaration(pGroupShared->declaration);			
			m_writer.EndLine(";");

		}
		else if (statement->nodeType == HLSLNodeType_SamplerState)
		{
			HLSLSamplerState* samplerState = static_cast<HLSLSamplerState*>(statement);
			
			m_writer.BeginLine(indent, samplerState->fileName, samplerState->line);

			if (samplerState->bStructured)
			{
				if (samplerState->IsComparisionState)
				{
					m_writer.WriteLineTagged(indent, samplerState->fileName, samplerState->line, "SamplerComparisonState %s {", samplerState->name);
				}
				else
				{
					m_writer.WriteLineTagged(indent, samplerState->fileName, samplerState->line, "SamplerState %s {", samplerState->name);
				}
				
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
				if (samplerState->IsComparisionState)
				{
					m_writer.Write("SamplerComparisonState %s", samplerState->name);
				}
				else
				{
					m_writer.Write("SamplerState %s", samplerState->name);
				}

			

				if (samplerState->type.array)
				{
					m_writer.Write("[");
					OutputExpression(samplerState->type.arraySize, false);
					m_writer.Write("]");
				}

				if (samplerState->registerName != NULL)
				{
					if (samplerState->registerSpaceName != NULL)
					{
						m_writer.Write(" : register(%s", samplerState->registerName);
						m_writer.Write(", %s)", samplerState->registerSpaceName);
					}
					else
						m_writer.Write(" : register(%s)", samplerState->registerName);
				}


				m_writer.EndLine(";");
			}
		}
		else if (statement->nodeType == HLSLNodeType_Buffer)
		{

			HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
			HLSLDeclaration* field = buffer->field;
			
			m_writer.BeginLine(indent, buffer->fileName, buffer->line);
			
			switch (buffer->type.baseType)
			{
				case HLSLBaseType_CBuffer:
					m_writer.Write("cbuffer");
					break;
				case HLSLBaseType_TBuffer:
					m_writer.Write("tbuffer");
					break;
				case HLSLBaseType_ConstantBuffer:									
					m_writer.Write("ConstantBuffer");
					break;
				case HLSLBaseType_StructuredBuffer:
					m_writer.Write("StructuredBuffer");
					break;
				case HLSLBaseType_PureBuffer:
					m_writer.Write("Buffer");
					break;
				case HLSLBaseType_RWBuffer:
					m_writer.Write("RWBuffer");
					break;
				case HLSLBaseType_RWStructuredBuffer:
					m_writer.Write("RWStructuredBuffer");
					break;
				case HLSLBaseType_ByteAddressBuffer:
					m_writer.Write("ByteAddressBuffer");
					break;
				case HLSLBaseType_RWByteAddressBuffer:
					m_writer.Write("RWByteAddressBuffer");
					break;
				default:
					break;
			}


			//handle cbuffer
			if (buffer->type.baseType == HLSLBaseType_CBuffer)
			{
				m_writer.Write(" %s", buffer->name);

				if (buffer->registerName != NULL)
				{
					if (buffer->registerSpaceName != NULL)
					{
						m_writer.Write(" : register(%s", buffer->registerName);
						m_writer.Write(", %s)", buffer->registerSpaceName);
					}
					else
						m_writer.Write(" : register(%s)", buffer->registerName);
				}

				m_writer.EndLine("");
				m_writer.EndLine("{");

				m_isInsideBuffer = true;

				while (field != NULL)
				{
					if (!field->hidden)
					{
						//PrintPreprocessors(field->line);
						m_writer.BeginLine(indent + 1, field->fileName, field->line);
						OutputDeclaration(field->type, field->name, /*semantic=*/NULL, /*registerName*/field->registerName, field->assignment);
						m_writer.Write(";");
						m_writer.EndLine();
					}
					field = (HLSLDeclaration*)field->nextStatement;
				}

				m_isInsideBuffer = false;
				m_writer.WriteLine(indent, "};");
				m_writer.EndLine("");
			}
			else
			{
				if(buffer->type.elementType != HLSLBaseType_Unknown && 
					buffer->type.baseType != HLSLBaseType_ByteAddressBuffer &&
					buffer->type.baseType != HLSLBaseType_RWByteAddressBuffer)
				m_writer.Write("<%s>", getElementTypeAsStr(buffer->type));

				m_writer.Write(" %s", buffer->name);

				if (buffer->bArray)
				{
					for (int i = 0; i < (int)buffer->arrayDimension; i++)
					{
						if (!String_Equal(buffer->arrayIdentifier[i], ""))
							m_writer.Write("[%s]", buffer->arrayIdentifier);
						else if (buffer->arrayIndex[i] > 0)
							m_writer.Write("[%u]", buffer->arrayIndex[i]);
						else
							m_writer.Write("[]");
					}
				}

				if (buffer->registerName != NULL)
				{
					if (buffer->registerSpaceName != NULL)
					{
						m_writer.Write(" : register(%s", buffer->registerName);
						m_writer.Write(", %s)", buffer->registerSpaceName);
					}
					else
						m_writer.Write(" : register(%s)", buffer->registerName);
				}

				m_writer.EndLine(";");
			}		
		}			
        else if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction* function = static_cast<HLSLFunction*>(statement);

            // Use an alternate name for the function which is supposed to be entry point
            // so that we can supply our own function which will be the actual entry point.
            const char* functionName   = function->name;
            const char* returnTypeName = GetTypeName(function->returnType);

			//PrintPreprocessors(function->line);
            m_writer.BeginLine(indent, function->fileName, function->line);
            m_writer.Write("%s %s(", returnTypeName, functionName);

            OutputArguments(function->argument);

            const char * semantic = function->sv_semantic ? function->sv_semantic : function->semantic;
            if (semantic != NULL)
            {
                m_writer.WriteLine(indent, ") : %s", semantic);
				m_writer.EndLine("{");
            }
            else
            {
                m_writer.WriteLine(indent,")");
				m_writer.EndLine("{");
            }

            //m_writer.EndLine();

            OutputStatements(indent + 1, function->statement);
            //m_writer.WriteLine(indent, "};");
			m_writer.WriteLine(indent, "};");
			m_writer.EndLine("");
        }
        else if (statement->nodeType == HLSLNodeType_ExpressionStatement)
        {
            HLSLExpressionStatement* expressionStatement = static_cast<HLSLExpressionStatement*>(statement);

			//PrintPreprocessors(statement->line);
            m_writer.BeginLine(indent, statement->fileName, statement->line);
            OutputExpression(expressionStatement->expression, true);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_ReturnStatement)
        {
            HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);
			//PrintPreprocessors(returnStatement->line);

            if (returnStatement->expression != NULL)
            {				
                m_writer.BeginLine(indent, returnStatement->fileName, returnStatement->line);
                m_writer.Write("return ");
                OutputExpression(returnStatement->expression, true);
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
			//PrintPreprocessors(discardStatement->line);
            m_writer.WriteLineTagged(indent, discardStatement->fileName, discardStatement->line, "discard;");
        }
        else if (statement->nodeType == HLSLNodeType_BreakStatement)
        {
            HLSLBreakStatement* breakStatement = static_cast<HLSLBreakStatement*>(statement);
			//PrintPreprocessors(breakStatement->line);
            m_writer.WriteLineTagged(indent, breakStatement->fileName, breakStatement->line, "break;");
        }
        else if (statement->nodeType == HLSLNodeType_ContinueStatement)
        {
            HLSLContinueStatement* continueStatement = static_cast<HLSLContinueStatement*>(statement);
			//PrintPreprocessors(continueStatement->line);
            m_writer.WriteLineTagged(indent, continueStatement->fileName, continueStatement->line, "continue;");
        }
        else if (statement->nodeType == HLSLNodeType_IfStatement)
        {
            HLSLIfStatement* ifStatement = static_cast<HLSLIfStatement*>(statement);

            m_writer.BeginLine(indent, ifStatement->fileName, ifStatement->line);
            m_writer.Write("if (");
            OutputExpression(ifStatement->condition, true);
            m_writer.Write(")");
            m_writer.EndLine();


			if (ifStatement->statement != NULL)
			{
				//if there are contents in brace 

				if (ifStatement->statement->nodeType == HLSLNodeType_Preprocessor)
				{
					OutputStatements(indent + 1, ifStatement->statement);
				}
				else
				{
					m_writer.WriteLine(indent, "{");
					OutputStatements(indent + 1, ifStatement->statement);
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
				OutputExpression(ifStatement->elseifStatement[i]->condition, true);
				m_writer.Write(")");
				m_writer.EndLine();
				m_writer.WriteLine(indent, "{");
				OutputStatements(indent + 1, ifStatement->elseifStatement[i]->statement);
				m_writer.WriteLine(indent, "}");
			}

            if (ifStatement->elseStatement != NULL)
            {
                m_writer.WriteLine(indent, "else");
				m_writer.WriteLine(indent, "{");
                OutputStatements(indent + 1, ifStatement->elseStatement);
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

			//PrintPreprocessors(forStatement->line);
            //m_writer.BeginLine(indent, forStatement->fileName, forStatement->line);
            m_writer.Write(indent, "for (");

			if(forStatement->initialization)
				OutputDeclaration(forStatement->initialization);
			else if(forStatement->initializationWithoutDeclaration)
				OutputExpression(forStatement->initializationWithoutDeclaration, true);

            m_writer.Write("; ");
            OutputExpression(forStatement->condition, true);
            m_writer.Write("; ");
            OutputExpression(forStatement->increment, true);
            m_writer.Write(")");
            m_writer.EndLine();

			m_writer.WriteLine(indent, "{");
            OutputStatements(indent + 1, forStatement->statement);
            m_writer.WriteLine(indent, "}");
        }		
		else if (statement->nodeType == HLSLNodeType_WhileStatement)
		{
			HLSLWhileStatement* whileStatement = static_cast<HLSLWhileStatement*>(statement);

			//PrintPreprocessors(whileStatement->line);
			m_writer.BeginLine(indent, whileStatement->fileName, whileStatement->line);
			m_writer.Write("while (");
			
			OutputExpression(whileStatement->condition, true);
			m_writer.Write(") {");
			m_writer.EndLine();
			OutputStatements(indent + 1, whileStatement->statement);
			m_writer.WriteLine(indent, "}");
		}
        else if (statement->nodeType == HLSLNodeType_BlockStatement)
        {
            HLSLBlockStatement* blockStatement = static_cast<HLSLBlockStatement*>(statement);
			//PrintPreprocessors(blockStatement->line);
            m_writer.WriteLineTagged(indent, blockStatement->fileName, blockStatement->line, "{");
            OutputStatements(indent + 1, blockStatement->statement);
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
            // Unhanded statement type.
            ASSERT(0);
        }

        statement = statement->nextStatement;
    }
}

void HLSLGenerator::OutputDeclaration(HLSLDeclaration* declaration)
{
    bool isSamplerType = IsSamplerType(declaration->type);

    if (!m_legacy && isSamplerType)
    {
        int reg = -1;
        if (declaration->registerName != NULL)
        {
            sscanf(declaration->registerName, "s%d", &reg);
        }

        const char* textureType = NULL;
        const char* samplerType = "SamplerState";
        // @@ Handle generic sampler type.

        if (declaration->type.baseType == HLSLBaseType_Sampler2D)
        {
            textureType = "Texture2D";
        }
        else if (declaration->type.baseType == HLSLBaseType_Sampler3D)
        {
            textureType = "Texture3D";
        }
        else if (declaration->type.baseType == HLSLBaseType_SamplerCube)
        {
            textureType = "TextureCube";
        }
        else if (declaration->type.baseType == HLSLBaseType_Sampler2DShadow)
        {
            textureType = "Texture2D";
            samplerType = "SamplerComparisonState";
        }
        else if (declaration->type.baseType == HLSLBaseType_Sampler2DMS)
        {
            textureType = "Texture2DMS<float4>";  // @@ Is template argument required?
            samplerType = NULL;
        }

        if (samplerType != NULL)
        {
            if (reg != -1)
            {
                m_writer.Write("%s %s_texture : register(t%d); %s %s_sampler : register(s%d)", textureType, declaration->name, reg, samplerType, declaration->name, reg);
            }
            else
            {
                m_writer.Write("%s %s_texture; %s %s_sampler", textureType, declaration->name, samplerType, declaration->name);
            }
        }
        else
        {
            if (reg != -1)
            {
                m_writer.Write("%s %s : register(t%d)", textureType, declaration->name, reg);
            }
            else
            {
                m_writer.Write("%s %s", textureType, declaration->name);
            }
        }
        return;
    }

    OutputDeclarationType(declaration->type);
    OutputDeclarationBody(declaration->type, declaration->name, declaration->semantic, declaration->registerName, declaration->assignment);
    declaration = declaration->nextDeclaration;

    while(declaration != NULL) {
        m_writer.Write(", ");
        OutputDeclarationBody(declaration->type, declaration->name, declaration->semantic, declaration->registerName, declaration->assignment);
        declaration = declaration->nextDeclaration;
    };
}

void HLSLGenerator::OutputDeclarationType(const HLSLType& type)
{
    const char* typeName = GetTypeName(type);
    if (!m_legacy)
    {
        if (type.baseType == HLSLBaseType_Sampler2D)
        {
            typeName = m_textureSampler2DStruct;
        }
        else if (type.baseType == HLSLBaseType_Sampler3D)
        {
            typeName = m_textureSampler3DStruct;
        }
        else if (type.baseType == HLSLBaseType_SamplerCube)
        {
            typeName = m_textureSamplerCubeStruct;
        }
        else if (type.baseType == HLSLBaseType_Sampler2DShadow)
        {
            typeName = m_textureSampler2DShadowStruct;
        }
        else if (type.baseType == HLSLBaseType_Sampler2DMS)
        {
            typeName = "Texture2DMS<float4>";
        }
    }

	if (type.baseType == HLSLBaseType_PointStream || type.baseType == HLSLBaseType_LineStream || type.baseType == HLSLBaseType_TriangleStream)
	{
		m_writer.Write("%s<%s> ", typeName, type.structuredTypeName);
		return;
	}

    if (type.flags & HLSLTypeFlag_Const)
    {
        m_writer.Write("const ");
    }
    if (type.flags & HLSLTypeFlag_Static)
    {
        m_writer.Write("static ");
    }

    // Interpolation modifiers.
    if (type.flags & HLSLTypeFlag_Centroid)
    {
        m_writer.Write("centroid ");
    }
    if (type.flags & HLSLTypeFlag_Linear)
    {
        m_writer.Write("linear ");
    }
    if (type.flags & HLSLTypeFlag_NoInterpolation)
    {
        m_writer.Write("nointerpolation ");
    }
    if (type.flags & HLSLTypeFlag_NoPerspective)
    {
        m_writer.Write("noperspective ");
    }
    if (type.flags & HLSLTypeFlag_Sample)   // @@ Only in shader model >= 4.1
    {
        m_writer.Write("sample ");
    }

	if (type.textureTypeName)
	{
		if (type.typeName)
		{
			m_writer.Write("%s<%s, %s> ", typeName, type.textureTypeName, type.typeName);
		}
		else
		{
			m_writer.Write("%s<%s> ", typeName, type.textureTypeName);
		}
	}
	else
		m_writer.Write("%s ", typeName);
}

void HLSLGenerator::OutputDeclarationBody(const HLSLType& type, const char* name, const char* semantic/*=NULL*/, const char* registerName/*=NULL*/, HLSLExpression * assignment/*=NULL*/)
{
    m_writer.Write("%s", name);

    if (type.array)
    {
		if (type.baseType == HLSLBaseType_InputPatch || type.baseType == HLSLBaseType_OutputPatch)
		{
			return;
		}

        ASSERT(semantic == NULL);
		
        m_writer.Write("[");
        if (type.arraySize != NULL)
        {
            OutputExpression(type.arraySize, true);
        }
        m_writer.Write("]");
    }

    if (semantic != NULL) 
    {
        m_writer.Write(" : %s", semantic);
    }

    if (registerName != NULL)
    {
        if (m_isInsideBuffer)
        {
            m_writer.Write(" : packoffset(%s)", registerName);
        }
        else 
        {
            m_writer.Write(" : register(%s)", registerName);
        }
    }

    if (assignment != NULL && !IsSamplerType(type))
    {
        m_writer.Write(" = ");
        if (type.array)
        {
            m_writer.Write("{ ");
            OutputExpressionList(assignment);
            m_writer.Write(" }");
        }
		else if(assignment->nextExpression)
		{
			m_writer.Write("{ ");
			OutputExpressionList(assignment);
			m_writer.Write(" }");
		}
        else 
        {
            OutputExpression(assignment, true);
        }
    }
}

void HLSLGenerator::OutputDeclaration(const HLSLType& type, const char* name, const char* semantic/*=NULL*/, const char* registerName/*=NULL*/, HLSLExpression * assignment/*=NULL*/)
{
    OutputDeclarationType(type);
    OutputDeclarationBody(type, name, semantic, registerName, assignment);
}

bool HLSLGenerator::ChooseUniqueName(const char* base, char* dst, int dstLength) const
{
    // IC: Try without suffix first.
    String_Printf(dst, dstLength, "%s", base);
    if (!m_tree->GetContainsString(base))
    {
        return true;
    }

    for (int i = 1; i < 1024; ++i)
    {
        String_Printf(dst, dstLength, "%s%d", base, i);
        if (!m_tree->GetContainsString(dst))
        {
            return true;
        }
    }
    return false;
}

