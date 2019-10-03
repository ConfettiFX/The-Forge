//=============================================================================
//
// Render/HLSLGenerator.cpp
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#include "Engine.h"

#include "HLSLGenerator.h"
#include "HLSLParser.h"
#include "HLSLTree.h"
#include "StringLibrary.h"

CachedString HLSLGenerator::GetTypeName(const HLSLType& type)
{
    switch (type.baseType)
    {
    case HLSLBaseType_Void:         return MakeCached("void");
    case HLSLBaseType_Float:        return MakeCached("float");
	case HLSLBaseType_Float1x2:        return MakeCached("float1x2");
	case HLSLBaseType_Float1x3:        return MakeCached("float1x3");
	case HLSLBaseType_Float1x4:        return MakeCached("float1x4");
    case HLSLBaseType_Float2:       return MakeCached("float2");
	case HLSLBaseType_Float2x2:        return MakeCached("float2x2");
	case HLSLBaseType_Float2x3:        return MakeCached("float2x3");
	case HLSLBaseType_Float2x4:        return MakeCached("float2x4");
    case HLSLBaseType_Float3:       return MakeCached("float3");
	case HLSLBaseType_Float3x2:        return MakeCached("float3x2");
	case HLSLBaseType_Float3x3:        return MakeCached("float3x3");
	case HLSLBaseType_Float3x4:        return MakeCached("float3x4");
    case HLSLBaseType_Float4:       return MakeCached("float4");
	case HLSLBaseType_Float4x2:        return MakeCached("float4x2");
	case HLSLBaseType_Float4x3:        return MakeCached("float4x3");
	case HLSLBaseType_Float4x4:        return MakeCached("float4x4");

    case HLSLBaseType_Half:         return MakeCached("half");
	case HLSLBaseType_Half1x2:        return MakeCached("half1x2");
	case HLSLBaseType_Half1x3:        return MakeCached("half1x3");
	case HLSLBaseType_Half1x4:        return MakeCached("half1x4");
    case HLSLBaseType_Half2:        return MakeCached("half2");
	case HLSLBaseType_Half2x2:        return MakeCached("half2x2");
	case HLSLBaseType_Half2x3:        return MakeCached("half2x3");
	case HLSLBaseType_Half2x4:        return MakeCached("half2x4");
    case HLSLBaseType_Half3:        return MakeCached("half3");
	case HLSLBaseType_Half3x2:        return MakeCached("half3x2");
	case HLSLBaseType_Half3x3:        return MakeCached("half3x3");
	case HLSLBaseType_Half3x4:        return MakeCached("half3x4");
    case HLSLBaseType_Half4:        return MakeCached("half4");
	case HLSLBaseType_Half4x2:        return MakeCached("half4x2");
	case HLSLBaseType_Half4x3:        return MakeCached("half4x3");
	case HLSLBaseType_Half4x4:        return MakeCached("half4x4");

	case HLSLBaseType_Min16Float:        return MakeCached("min16float");
	case HLSLBaseType_Min16Float1x2:        return MakeCached("min16float1x2");
	case HLSLBaseType_Min16Float1x3:        return MakeCached("min16float1x3");
	case HLSLBaseType_Min16Float1x4:        return MakeCached("min16float1x4");
	case HLSLBaseType_Min16Float2:       return MakeCached("min16float2");
	case HLSLBaseType_Min16Float2x2:        return MakeCached("min16float2x2");
	case HLSLBaseType_Min16Float2x3:        return MakeCached("min16float2x3");
	case HLSLBaseType_Min16Float2x4:        return MakeCached("min16float2x4");
	case HLSLBaseType_Min16Float3:       return MakeCached("min16float3");
	case HLSLBaseType_Min16Float3x2:        return MakeCached("min16float3x2");
	case HLSLBaseType_Min16Float3x3:        return MakeCached("min16float3x3");
	case HLSLBaseType_Min16Float3x4:        return MakeCached("min16float3x4");
	case HLSLBaseType_Min16Float4:       return MakeCached("min16float4");
	case HLSLBaseType_Min16Float4x2:        return MakeCached("min16float4x2");
	case HLSLBaseType_Min16Float4x3:        return MakeCached("min16float4x3");
	case HLSLBaseType_Min16Float4x4:        return MakeCached("min16float4x4");

	case HLSLBaseType_Min10Float:        return MakeCached("min10float");
	case HLSLBaseType_Min10Float1x2:        return MakeCached("min10float1x2");
	case HLSLBaseType_Min10Float1x3:        return MakeCached("min10float1x3");
	case HLSLBaseType_Min10Float1x4:        return MakeCached("min10float1x4");
	case HLSLBaseType_Min10Float2:       return MakeCached("min10float2");
	case HLSLBaseType_Min10Float2x2:        return MakeCached("min10float2x2");
	case HLSLBaseType_Min10Float2x3:        return MakeCached("min10float2x3");
	case HLSLBaseType_Min10Float2x4:        return MakeCached("min10float2x4");
	case HLSLBaseType_Min10Float3:       return MakeCached("min10float3");
	case HLSLBaseType_Min10Float3x2:        return MakeCached("min10float3x2");
	case HLSLBaseType_Min10Float3x3:        return MakeCached("min10float3x3");
	case HLSLBaseType_Min10Float3x4:        return MakeCached("min10float3x4");
	case HLSLBaseType_Min10Float4:       return MakeCached("min10float4");
	case HLSLBaseType_Min10Float4x2:        return MakeCached("min10float4x2");
	case HLSLBaseType_Min10Float4x3:        return MakeCached("min10float4x3");
	case HLSLBaseType_Min10Float4x4:        return MakeCached("min10float4x4");

    case HLSLBaseType_Bool:         return MakeCached("bool");
	case HLSLBaseType_Bool1x2:        return MakeCached("bool1x2");
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

	case HLSLBaseType_InputPatch:     return (type.InputPatchName);
	case HLSLBaseType_OutputPatch:     return (type.OutputPatchName);

	case HLSLBaseType_PointStream:		return MakeCached("PointStream");
	case HLSLBaseType_LineStream:		return MakeCached("LineStream");
	case HLSLBaseType_TriangleStream:	return MakeCached("TriangleStream");

    case HLSLBaseType_Texture:      return MakeCached("texture");

	case HLSLBaseType_Texture1D:      return MakeCached("Texture1D");
	case HLSLBaseType_Texture1DArray:      return MakeCached("Texture1DArray");
	case HLSLBaseType_Texture2D:      return MakeCached("Texture2D");
	case HLSLBaseType_Texture2DArray:      return MakeCached("Texture1DArray");
	case HLSLBaseType_Texture3D:      return MakeCached("Texture3D");
	case HLSLBaseType_Texture2DMS:      return MakeCached("Texture2DMS");
	case HLSLBaseType_Texture2DMSArray:      return MakeCached("Texture2DMSArray");
	case HLSLBaseType_TextureCube:      return MakeCached("TextureCube");
	case HLSLBaseType_TextureCubeArray:      return MakeCached("TextureCubeArray");

	case HLSLBaseType_RWTexture1D:      return MakeCached("RWTexture1D");
	case HLSLBaseType_RWTexture1DArray:      return MakeCached("RWTexture1DArray");
	case HLSLBaseType_RWTexture2D:      return MakeCached("RWTexture2D");
	case HLSLBaseType_RWTexture2DArray:      return MakeCached("RWTexture1DArray");
	case HLSLBaseType_RWTexture3D:      return MakeCached("RWTexture3D");

    case HLSLBaseType_Sampler:      return MakeCached("sampler");
    case HLSLBaseType_Sampler2D:    return MakeCached("sampler2D");
    case HLSLBaseType_Sampler3D:    return MakeCached("sampler3D");
    case HLSLBaseType_SamplerCube:  return MakeCached("samplerCUBE");
    case HLSLBaseType_Sampler2DShadow:  return MakeCached("sampler2DShadow");
    case HLSLBaseType_Sampler2DMS:  return MakeCached("sampler2DMS");
    case HLSLBaseType_Sampler2DArray:    return MakeCached("sampler2DArray");
	case HLSLBaseType_SamplerState:  return MakeCached("SamplerState");
	case HLSLBaseType_SamplerComparisonState:  return MakeCached("SamplerComparisonState");
    case HLSLBaseType_UserDefined:  return type.typeName;

    default: return MakeCached("<unknown type>");
    }
}

HLSLGenerator::HLSLGenerator()
{
    m_tree                          = NULL;
    m_entryName                     = NULL;
    m_legacy                        = false;
    m_target                        = Target_VertexShader;
    m_isInsideBuffer                = false;
	m_stringLibrary					= NULL;

	m_defaultSpaceIndex = 0;

}



// @@ We need a better way of doing semantic replacement:
// - Look at the function being generated.
// - Return semantic, semantics associated to fields of the return structure, or output arguments, or fields of structures associated to output arguments -> output semantic replacement.
// - Semantics associated input arguments or fields of the input arguments -> input semantic replacement.
static CachedString TranslateSemantic(HLSLGenerator & generator, const CachedString & semantic, bool output, HLSLGenerator::Target target)
{
    if (target == HLSLGenerator::Target_VertexShader)
    {
        if (output) 
        {
            if (String_Equal("SV_POSITION", semantic))     return generator.MakeCached("SV_POSITION");
			
        }
        else {
            if (String_Equal("INSTANCE_ID", semantic))  return generator.MakeCached("SV_InstanceID");
        }
    }
    else if (target == HLSLGenerator::Target_PixelShader)
    {
        if (output)
        {
            if (String_Equal("DEPTH", semantic))      return generator.MakeCached("SV_Depth");
            if (String_Equal("COLOR", semantic))      return generator.MakeCached("SV_Target");
            if (String_Equal("COLOR0", semantic))     return generator.MakeCached("SV_Target0");
            if (String_Equal("COLOR0_1", semantic))   return generator.MakeCached("SV_Target1");
            if (String_Equal("COLOR1", semantic))     return generator.MakeCached("SV_Target1");
            if (String_Equal("COLOR2", semantic))     return generator.MakeCached("SV_Target2");
            if (String_Equal("COLOR3", semantic))     return generator.MakeCached("SV_Target3");
        }
        else
        {
            if (String_Equal("VPOS", semantic))       return generator.MakeCached("SV_POSITION");
            if (String_Equal("VFACE", semantic))      return generator.MakeCached("SV_IsFrontFace");    // bool   @@ Should we do type replacement too?
        }
    }
    return CachedString();
}

bool HLSLGenerator::Generate(StringLibrary * stringLibrary, HLSLTree* tree, Target target, const char* entryName, bool legacy)
{
    m_tree      = tree;
    m_entryName = entryName;
    m_target    = target;
    m_legacy    = legacy;
    m_isInsideBuffer = false;

	m_stringLibrary = stringLibrary;

	m_defaultSpaceName = tree->AddStringFormatCached("space%d",m_defaultSpaceIndex);

    m_writer.Reset();

    // @@ Should we generate an entirely new copy of the tree so that we can modify it in place?
    if (!legacy)
    {
        HLSLFunction * function = tree->FindFunction(MakeCached(entryName));

        // Handle return value semantics
        if (function->semantic.IsEmpty()) {
            function->sv_semantic = TranslateSemantic(*this,function->semantic, /*output=*/true, target);
        }
        if (function->returnType.baseType == HLSLBaseType_UserDefined) {
            HLSLStruct * s = tree->FindGlobalStruct(function->returnType.typeName);

			HLSLStructField * sv_fields = NULL;

			HLSLStructField * lastField = NULL;
            HLSLStructField * field = s->field;
            while (field) {
				HLSLStructField * nextField = field->nextField;

                if (field->semantic.IsNotEmpty()) {
					field->hidden = false;
                    field->sv_semantic = TranslateSemantic(*this, field->semantic, /*output=*/true, target);

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
        //HLSLArgument * argument = function->argument;
		const eastl::vector<HLSLArgument*>& argVec = function->args;

		for (int i = 0; i < argVec.size(); i++)
		{
			HLSLArgument * argument = argVec[i];
            bool output = argument->modifier == HLSLArgumentModifier_Out;
            if (argument->semantic.IsNotEmpty())
			{
                argument->sv_semantic = TranslateSemantic(*this, argument->semantic, output, target);
            }

            if (argument->type.baseType == HLSLBaseType_UserDefined) {
                HLSLStruct * s = tree->FindGlobalStruct(argument->type.typeName);

                HLSLStructField * field = s->field;
                while (field) {
                    if (field->semantic.IsNotEmpty()) {
						field->hidden = false;

                        field->sv_semantic = TranslateSemantic(*this, field->semantic, output, target);
                    }

                    field = field->nextField;
                }
            }

        }
    }

	bool hasTex2D = m_tree->GetContainsString("tex2D");
	bool hasTex2Dproj = m_tree->GetContainsString("tex2Dproj");
	bool hasTex2Dlod = m_tree->GetContainsString("tex2Dlod");
	bool hasTex2Dbias = m_tree->GetContainsString("tex2Dbias");
	bool hasTex2Dgrad = m_tree->GetContainsString("tex2Dgrad");
	bool hasTex2Dgather = m_tree->GetContainsString("tex2Dgather");
	bool hasTex2Dsize = m_tree->GetContainsString("tex2Dsize");
	bool hasTex2Dfetch = m_tree->GetContainsString("tex2Dfetch");
	bool hasTex2Dcmp = m_tree->GetContainsString("tex2Dcmp");
	bool hasTex2DMSfetch = m_tree->GetContainsString("tex2DMSfetch");
	bool hasTex2DMSsize = m_tree->GetContainsString("tex2DMSsize");
	bool hasTex3D = m_tree->GetContainsString("tex3D");
	bool hasTex3Dlod = m_tree->GetContainsString("tex3Dlod");
	bool hasTex3Dbias = m_tree->GetContainsString("tex3Dbias");
	bool hasTex3Dsize = m_tree->GetContainsString("tex3Dsize");
	bool hasTexCUBE = m_tree->GetContainsString("texCUBE");
	bool hasTexCUBElod = m_tree->GetContainsString("texCUBElod");
	bool hasTexCUBEbias = m_tree->GetContainsString("texCUBEbias");
	bool hasTexCUBEsize = m_tree->GetContainsString("texCUBEsize");

	int strSize = 128;

    ChooseUniqueName("TextureSampler2D",            m_textureSampler2DStruct, strSize);
    ChooseUniqueName("CreateTextureSampler2D",      m_textureSampler2DCtor, strSize);
    ChooseUniqueName("TextureSampler2DShadow",      m_textureSampler2DShadowStruct, strSize);
    ChooseUniqueName("CreateTextureSampler2DShadow",m_textureSampler2DShadowCtor, strSize);
    ChooseUniqueName("TextureSampler3D",            m_textureSampler3DStruct,   	 strSize);
    ChooseUniqueName("CreateTextureSampler3D",      m_textureSampler3DCtor,     	 strSize);
    ChooseUniqueName("TextureSamplerCube",          m_textureSamplerCubeStruct, 	 strSize);
    ChooseUniqueName("CreateTextureSamplerCube",    m_textureSamplerCubeCtor,   	 strSize);
    ChooseUniqueName("tex2D",                       m_tex2DFunction,            	 strSize);
    ChooseUniqueName("tex2Dproj",                   m_tex2DProjFunction,        	 strSize);
    ChooseUniqueName("tex2Dlod",                    m_tex2DLodFunction,         	 strSize);
    ChooseUniqueName("tex2Dbias",                   m_tex2DBiasFunction,        	 strSize);
    ChooseUniqueName("tex2Dgrad",                   m_tex2DGradFunction,        	 strSize);
    ChooseUniqueName("tex2Dgather",                 m_tex2DGatherFunction,      	 strSize);
    ChooseUniqueName("tex2Dsize",                   m_tex2DSizeFunction,        	 strSize);
    ChooseUniqueName("tex2Dfetch",                  m_tex2DFetchFunction,       	 strSize);
    ChooseUniqueName("tex2Dcmp",                    m_tex2DCmpFunction,         	 strSize);
    ChooseUniqueName("tex2DMSfetch",                m_tex2DMSFetchFunction,     	 strSize);
    ChooseUniqueName("tex2DMSsize",                 m_tex2DMSSizeFunction,      	 strSize);
    ChooseUniqueName("tex3D",                       m_tex3DFunction,            	 strSize);
    ChooseUniqueName("tex3Dlod",                    m_tex3DLodFunction,         	 strSize);
    ChooseUniqueName("tex3Dbias",                   m_tex3DBiasFunction,        	 strSize);
    ChooseUniqueName("tex3Dsize",                   m_tex3DSizeFunction,        	 strSize);
    ChooseUniqueName("texCUBE",                     m_texCubeFunction,          	 strSize);
    ChooseUniqueName("texCUBElod",                  m_texCubeLodFunction,       	 strSize);
    ChooseUniqueName("texCUBEbias",                 m_texCubeBiasFunction,      	 strSize);
    ChooseUniqueName("texCUBEsize",                 m_texCubeSizeFunction,      	 strSize);

	ChooseUniqueName("Sample",						m_Sample,					strSize);
	ChooseUniqueName("SampleLevel",					m_SampleLevel,				strSize);
	ChooseUniqueName("SampleBias",					m_SampleBias,				strSize);

	
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

		if (hasTex2D) 
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float2 texCoord) {", RawStr(m_tex2DFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dproj)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", RawStr(m_tex2DProjFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord.xy / texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dlod)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord, int2 offset=0) {", RawStr(m_tex2DLodFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "return ts.t.SampleLevel(ts.s, texCoord.xy, texCoord.w, offset);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dbias)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", RawStr(m_tex2DBiasFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "return ts.t.SampleBias(ts.s, texCoord.xy, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dgrad)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float2 texCoord, float2 ddx, float2 ddy) {", RawStr(m_tex2DGradFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "return ts.t.SampleGrad(ts.s, texCoord.xy, ddx, ddy);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dgather)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float2 texCoord, int component, int2 offset=0) {", RawStr(m_tex2DGatherFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "if(component == 0) return ts.t.GatherRed(ts.s, texCoord, offset);");
            m_writer.WriteLine(1, "if(component == 1) return ts.t.GatherGreen(ts.s, texCoord, offset);");
            m_writer.WriteLine(1, "if(component == 2) return ts.t.GatherBlue(ts.s, texCoord, offset);");
            m_writer.WriteLine(1, "return ts.t.GatherAlpha(ts.s, texCoord, offset);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dsize)
        {
            m_writer.WriteLine(0, "int2 %s(%s ts) {", RawStr(m_tex2DSizeFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "int2 size; ts.t.GetDimensions(size.x, size.y); return size;");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dfetch)
        {
            m_writer.WriteLine(0, "int2 %s(%s ts, int3 texCoord) {", RawStr(m_tex2DFetchFunction), RawStr(m_textureSampler2DStruct));
            m_writer.WriteLine(1, "return ts.t.Load(texCoord.xyz);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2Dcmp)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", RawStr(m_tex2DCmpFunction), RawStr(m_textureSampler2DShadowStruct));
            m_writer.WriteLine(1, "return ts.t.SampleCmpLevelZero(ts.s, texCoord.xy, texCoord.z);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2DMSfetch)
        {
            m_writer.WriteLine(0, "float4 %s(Texture2DMS<float4> t, int2 texCoord, int sample) {", RawStr(m_tex2DMSFetchFunction));
            m_writer.WriteLine(1, "return t.Load(texCoord, sample);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex2DMSsize)
        {
            m_writer.WriteLine(0, "int3 %s(Texture2DMS<float4> t) {", RawStr(m_tex2DMSSizeFunction));
            m_writer.WriteLine(1, "int3 size; t.GetDimensions(size.x, size.y, size.z); return size;");   // @@ Not tested, does this return the number of samples in the third argument?
            m_writer.WriteLine(0, "}");
        }
        if (hasTex3D)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float3 texCoord) {", RawStr(m_tex3DFunction), RawStr(m_textureSampler3DStruct));
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex3Dlod)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", RawStr(m_tex3DLodFunction), RawStr(m_textureSampler3DStruct));
            m_writer.WriteLine(1, "return ts.t.SampleLevel(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex3Dbias)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", RawStr(m_tex3DBiasFunction), RawStr(m_textureSampler3DStruct));
            m_writer.WriteLine(1, "return ts.t.SampleBias(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTex3Dsize)
        {
            m_writer.WriteLine(0, "int3 %s(%s ts) {", RawStr(m_tex3DSizeFunction), RawStr(m_textureSampler3DStruct));
            m_writer.WriteLine(1, "int3 size; ts.t.GetDimensions(size.x, size.y, size.z); return size;");
            m_writer.WriteLine(0, "}");
        }
        if (hasTexCUBE)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float3 texCoord) {", RawStr(m_texCubeFunction), RawStr(m_textureSamplerCubeStruct));
            m_writer.WriteLine(1, "return ts.t.Sample(ts.s, texCoord);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTexCUBElod)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", RawStr(m_texCubeLodFunction), RawStr(m_textureSamplerCubeStruct));
            m_writer.WriteLine(1, "return ts.t.SampleLevel(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTexCUBEbias)
        {
            m_writer.WriteLine(0, "float4 %s(%s ts, float4 texCoord) {", RawStr(m_texCubeBiasFunction), RawStr(m_textureSamplerCubeStruct));
            m_writer.WriteLine(1, "return ts.t.SampleBias(ts.s, texCoord.xyz, texCoord.w);");
            m_writer.WriteLine(0, "}");
        }
        if (hasTexCUBEsize)
        {
            m_writer.WriteLine(0, "int %s(%s ts) {", RawStr(m_texCubeSizeFunction), RawStr(m_textureSamplerCubeStruct));
            m_writer.WriteLine(1, "int size; ts.t.GetDimensions(size); return size;");   // @@ Not tested, does this return a single value?
            m_writer.WriteLine(0, "}");
        }
    }
	
    HLSLRoot* root = m_tree->GetRoot();
    OutputStatements(0, root->statement);

    m_tree = NULL;
    return true;
}

const char* HLSLGenerator::GetResult() const
{
    return m_writer.GetResult();
}

void HLSLGenerator::OutputExpressionList(const eastl::vector<HLSLExpression*>& expressions, size_t start)
{
    for (size_t i = start; i < expressions.size(); ++i)
    {
        if (i > start)
        {
            m_writer.Write(", ");
        }
        OutputExpression(expressions[i], true);
    }
}

void HLSLGenerator::OutputExpression(HLSLExpression* expression, bool needsEndParen)
{
	if (expression->nodeType == HLSLNodeType_InitListExpression)
	{
		HLSLInitListExpression* initExpression = static_cast<HLSLInitListExpression*>(expression);
		m_writer.Write("{");
		OutputExpressionList(initExpression->initExpressions);
		m_writer.Write("}");
	}
	else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
	{
		HLSLIdentifierExpression* identifierExpression = static_cast<HLSLIdentifierExpression*>(expression);
		HLSLDeclaration* declaration = identifierExpression->pDeclaration;
		ASSERT_PARSER(declaration);
		CachedString name = declaration->name;
		if (!m_legacy && IsSamplerType(identifierExpression->expressionType) && declaration->global)
		{
			// @@ Handle generic sampler type.
            if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2D)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", RawStr(m_textureSampler2DCtor), RawStr(name), RawStr(name));
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler3D)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", RawStr(m_textureSampler3DCtor), RawStr(name), RawStr(name));
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_SamplerCube)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", RawStr(m_textureSamplerCubeCtor), RawStr(name), RawStr(name));
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DShadow)
            {
                m_writer.Write("%s(%s_texture, %s_sampler)", RawStr(m_textureSampler2DShadowCtor), RawStr(name), RawStr(name));
            }
            else if (identifierExpression->expressionType.baseType == HLSLBaseType_Sampler2DMS)
            {
                m_writer.Write("%s", RawStr(name));
            }
        }
		else if (expression->functionExpression)
		{
			//TODO: check and remove
			// if it is buffer type and has function expression
		
			HLSLBuffer* buffer = m_tree->FindBuffer(name);
			if (buffer)
			{
				m_writer.Write("%s", RawStr(buffer->name));

				if (buffer->type.array)
				{
					OutputArrayExpression(buffer->type.arrayDimension, buffer->arrayDimExpression);
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
				m_writer.Write("%s", RawStr(name));
			}

			OutputExpression(expression->functionExpression, needsEndParen);
		}
        else
        {
            m_writer.Write("%s", RawStr(name));
        }
    }
    else if (expression->nodeType == HLSLNodeType_CastingExpression)
    {
        HLSLCastingExpression* castingExpression = static_cast<HLSLCastingExpression*>(expression);
        m_writer.Write("(");
        OutputDeclaration(castingExpression->expressionType, MakeCached(""));
        m_writer.Write(")");
		m_writer.Write("(");
		OutputExpression(castingExpression->expression, false);
		m_writer.Write(")");
	}
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression* constructorExpression = static_cast<HLSLConstructorExpression*>(expression);
        m_writer.Write("%s(", RawStr(GetTypeName(constructorExpression->expressionType)));
        OutputExpressionList(constructorExpression->params);
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
				ASSERT_PARSER(0);
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
            m_writer.Write("%s", (op));
            OutputExpression(unaryExpression->expression, needsEndParen);
        }
        else
        {
            OutputExpression(unaryExpression->expression, needsEndParen);
            m_writer.Write("%s", (op));
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
		case HLSLBinaryOp_Comma:        op = " , "; break;
		default:
			ASSERT_PARSER(0);
        }
        m_writer.Write("%s", (op));
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
		
		m_writer.Write(".%s", RawStr(memberAccess->field));
		
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
		const eastl::vector<HLSLExpression*>& params = functionCall->params;

        CachedString name = functionCall->function->name;
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
		
		if (String_Equal(name, "Sample") || String_Equal(name, "SampleLevel") ||
			String_Equal(name, "SampleCmp") || String_Equal(name, "SampleCmpLevelZero") ||
			String_Equal(name, "SampleBias") || String_Equal(name, "SampleGrad") ||
			String_Equal(name, "GatherRed") || String_Equal(name, "Load") ||
			String_Equal(name, "Store") || String_Equal(name, "GetDimensions"))
		{
			ASSERT_PARSER(params.size() >= 2);
			OutputExpression(params[0], needsEndParen);
			m_writer.Write(".%s(", RawStr(name));
			OutputExpressionList(params, 1);
			m_writer.Write(")");
		}
		else
		{
			m_writer.Write("%s(", RawStr(name));
			OutputExpressionList(functionCall->params);
			m_writer.Write(")");
		}
	}
	else
	{
		m_writer.Write("<unknown expression>");
	}
}

void HLSLGenerator::OutputArguments(const eastl::vector < HLSLArgument* > & arguments)
{
	for (int i = 0; i < arguments.size(); i++)
	{
		if (i > 0)
		{
			m_writer.Write(", ");
		}

		HLSLArgument * argument = arguments[i];

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
		case HLSLArgumentModifier_Const:
			m_writer.Write("const ");
			break;
		case HLSLArgumentModifier_None:
			// no op
			break;
		default:
			ASSERT_PARSER(0);
			break;
		}

		CachedString semantic = argument->sv_semantic.IsNotEmpty() ? argument->sv_semantic : argument->semantic;

		OutputDeclaration(argument->type, argument->name, semantic, CachedString(), argument->defaultValue);
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
				m_writer.Write("[%s(", (attributeName));

				if (attribute->numGroupX != 0)
					m_writer.Write("%d, ", attribute->numGroupX);
				else
					m_writer.Write("%s, ", RawStr(attribute->numGroupXstr));
					
				if(attribute->numGroupY != 0)
					m_writer.Write("%d, ", attribute->numGroupY);
				else
					m_writer.Write("%s, ", RawStr(attribute->numGroupYstr));
					
					
				if(attribute->numGroupZ != 0)
					m_writer.Write("%d", attribute->numGroupZ);
				else
					m_writer.Write("%s", RawStr(attribute->numGroupZstr));

				m_writer.Write(")]");
				m_writer.WriteLine(indent, "");

			}
			else if (String_Equal(attributeName, "maxvertexcount"))
			{
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%d)]", attributeName, attribute->maxVertexCount);
			}
			else if (String_Equal(attributeName, "unroll"))
			{
				if (attribute->unrollIdentifier.IsNotEmpty())
					m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%s)]", attributeName, RawStr(attribute->unrollIdentifier));
				else
				{
					if(attribute->unrollCount == 0)
						m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s]", attributeName);
					else
						m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%d)]", attributeName, attribute->unrollCount);
				}
			}
			else if (String_Equal(attributeName, "domain"))
			{
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%s)]", attributeName, RawStr(attribute->domain));
			}
			else if (String_Equal(attributeName, "partitioning"))
			{
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%s)]", attributeName, RawStr(attribute->partitioning));
			}
			else if (String_Equal(attributeName, "outputtopology"))
			{
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%s)]", attributeName, RawStr(attribute->outputtopology));
			}
			else if (String_Equal(attributeName, "outputcontrolpoints"))
			{
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%d)]", attributeName, attribute->outputcontrolpoints);
			}
			else if (String_Equal(attributeName, "patchconstantfunc"))
			{
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%s)]", attributeName, RawStr(attribute->patchconstantfunc));
			}
			else if (String_Equal(attributeName, "maxtessfactor"))
			{
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s(%f)]", attributeName, attribute->maxTessellationFactor);
			}
			else
				m_writer.WriteLineTagged(indent, RawStr(attribute->fileName), attribute->line, "[%s]", attributeName);
        }

        attribute = attribute->nextAttribute;
    }
}

void HLSLGenerator::WriteRegisterAndSpace(const CachedString & registerName, const CachedString & registerSpaceName)
{
	bool isRegister = false;
	bool isSpace = false;

	if (registerName.IsNotEmpty())
	{
		if (!String_Equal(registerName, "none"))
		{
			isRegister = true;
		}
	}

	if (registerSpaceName.IsNotEmpty())
	{
		if (!String_Equal(registerSpaceName, "none"))
		{
			isSpace = true;
		}
	}

	if (isSpace && isRegister)
	{
		m_writer.Write(" : register(%s, %s)", RawStr(registerName), RawStr(registerSpaceName));
	}
	else if (isRegister)
	{
		m_writer.Write(" : register(%s)", RawStr(registerName));
	}
	else if (isSpace)
	{
		m_writer.Write(" : register(%s)", RawStr(registerSpaceName));
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
		
		if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration* declaration = static_cast<HLSLDeclaration*>(statement);
			
			//PrintPreprocessors(declaration->line);
            m_writer.BeginLine(indent, RawStr(declaration->fileName), declaration->line);
            OutputDeclaration(declaration);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct* structure = static_cast<HLSLStruct*>(statement);
            m_writer.WriteLineTagged(indent, RawStr(structure->fileName), structure->line, "struct %s", RawStr(structure->name));
			m_writer.WriteLine(indent, "{");
            HLSLStructField* field = structure->field;
            while (field != NULL)
            {
                if (!field->hidden)
                {
					{
						m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);
						CachedString semantic = field->sv_semantic.IsNotEmpty() ? field->sv_semantic : field->semantic;
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

			m_writer.BeginLine(indent, RawStr(textureState->fileName), textureState->line);

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
				m_writer.Write("%s", getElementTypeAsStr(m_stringLibrary, textureState->type));
			}


			
			if (textureState->type.baseType == HLSLBaseType_Texture2DMS || textureState->type.baseType == HLSLBaseType_Texture2DMSArray)
			{
				if(textureState->sampleIdentifier.IsNotEmpty())
					m_writer.Write(", %s", RawStr(textureState->sampleIdentifier));
				else
				{
					if (textureState->sampleCount != 0)
					{
						m_writer.Write(", %d", textureState->sampleCount);				
					}
				}
			}

			if (textureState->type.elementType != HLSLBaseType_Unknown || 			
				textureState->type.baseType == HLSLBaseType_Texture2DMS ||
				textureState->type.baseType == HLSLBaseType_Texture2DMSArray
				)
			{
				m_writer.Write(">");
			}
			
			m_writer.Write(" %s", RawStr(textureState->name));

			if (textureState->type.array)
			{
				OutputArrayExpression(textureState->type.arrayDimension, textureState->arrayDimExpression);
			}


			WriteRegisterAndSpace(textureState->registerName, textureState->registerSpaceName);

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
			
			m_writer.BeginLine(indent, RawStr(samplerState->fileName), samplerState->line);

			if (samplerState->type.baseType == HLSLBaseType_SamplerComparisonState)
			{
				m_writer.Write("SamplerComparisonState %s", RawStr(samplerState->name));
			}
			else
			{
				m_writer.Write("SamplerState %s", RawStr(samplerState->name));
			}

			if (samplerState->type.array)
			{
				OutputArrayExpression(samplerState->type.arrayDimension, samplerState->arrayDimExpression);
			}

			WriteRegisterAndSpace(samplerState->registerName, samplerState->registerSpaceName);

			m_writer.EndLine(";");
		}
		else if (statement->nodeType == HLSLNodeType_Buffer)
		{
			HLSLBuffer* buffer = static_cast<HLSLBuffer*>(statement);
			HLSLDeclaration* field = buffer->field;
			
			m_writer.BeginLine(indent, RawStr(buffer->fileName), buffer->line);
			
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
				m_writer.Write(" %s", RawStr(buffer->name));

				WriteRegisterAndSpace(buffer->registerName, buffer->registerSpaceName);

				m_writer.EndLine("");
				m_writer.EndLine("{");

				m_isInsideBuffer = true;

				while (field != NULL)
				{
					if (!field->hidden)
					{
						//PrintPreprocessors(field->line);
						m_writer.BeginLine(indent + 1, RawStr(field->fileName), field->line);
						OutputDeclaration(field->type, field->name, /*semantic=*/CachedString(), /*registerName*/field->registerName, field->assignment);
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
				{
					m_writer.Write("<%s>", getElementTypeAsStr(m_stringLibrary, buffer->type));
				}

				m_writer.Write(" %s", RawStr(buffer->name));

				if (buffer->type.array)
				{
					OutputArrayExpression(buffer->type.arrayDimension, buffer->arrayDimExpression);
				}

				WriteRegisterAndSpace(buffer->registerName, buffer->registerSpaceName);

				m_writer.EndLine(";");
			}		
		}			
        else if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction* function = static_cast<HLSLFunction*>(statement);

            // Use an alternate name for the function which is supposed to be entry point
            // so that we can supply our own function which will be the actual entry point.
            CachedString functionName   = function->name;
            CachedString returnTypeName = GetTypeName(function->returnType);

            m_writer.BeginLine(indent, RawStr(function->fileName), function->line);
            m_writer.Write("%s %s(", RawStr(returnTypeName), RawStr(functionName));

			eastl::vector < HLSLArgument * > arguments = function->args;
			OutputArguments(arguments);

            CachedString semantic = function->sv_semantic.IsNotEmpty() ? function->sv_semantic : function->semantic;
            if (semantic.IsNotEmpty())
            {
                m_writer.WriteLine(indent, ") : %s", RawStr(semantic));
				m_writer.EndLine("{");
            }
            else
            {
                m_writer.WriteLine(indent,")");
				m_writer.EndLine("{");
            }

            OutputStatements(indent + 1, function->statement);
			m_writer.WriteLine(indent, "};");
			m_writer.EndLine("");
        }
        else if (statement->nodeType == HLSLNodeType_ExpressionStatement)
        {
            HLSLExpressionStatement* expressionStatement = static_cast<HLSLExpressionStatement*>(statement);

            m_writer.BeginLine(indent, RawStr(statement->fileName), statement->line);
            OutputExpression(expressionStatement->expression, true);
            m_writer.EndLine(";");
        }
        else if (statement->nodeType == HLSLNodeType_ReturnStatement)
        {
            HLSLReturnStatement* returnStatement = static_cast<HLSLReturnStatement*>(statement);

            if (returnStatement->expression != NULL)
            {				
                m_writer.BeginLine(indent, RawStr(returnStatement->fileName), returnStatement->line);
                m_writer.Write("return ");
                OutputExpression(returnStatement->expression, true);
                m_writer.EndLine(";");
            }
            else
            {
                m_writer.WriteLineTagged(indent, RawStr(returnStatement->fileName), returnStatement->line, "return;");
            }
        }
        else if (statement->nodeType == HLSLNodeType_DiscardStatement)
        {
            HLSLDiscardStatement* discardStatement = static_cast<HLSLDiscardStatement*>(statement);
            m_writer.WriteLineTagged(indent, RawStr(discardStatement->fileName), discardStatement->line, "discard;");
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
            OutputExpression(ifStatement->condition, false);
            m_writer.Write(")");
            m_writer.EndLine();

			if (ifStatement->statement != NULL)
			{
				//if there are contents in brace 
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

			for (int i = 0; i< ifStatement->elseifStatement.size(); i++)
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

			int numCases = (int)switchStatement->caseNumber.size();
			ASSERT_PARSER(numCases == switchStatement->caseStatement.size());

			//print cases
			for (int i = 0; i< numCases; i++)
			{
				// check if the default is here
				if (switchStatement->caseDefault != NULL &&
					switchStatement->caseDefaultIndex == i)
				{
					m_writer.Write(indent + 1, "default:\n");
					m_writer.WriteLine(indent + 1, "{");
					OutputStatements(indent + 2, switchStatement->caseDefault);
					m_writer.WriteLine(indent + 1, "}");
				}


				m_writer.Write(indent + 1, "case ");


				OutputExpression(switchStatement->caseNumber[i], false);

				m_writer.Write(":\n");

				m_writer.WriteLine(indent + 1, "{");
				OutputStatements(indent + 2, switchStatement->caseStatement[i]);
				m_writer.WriteLine(indent + 1, "}");
			}

			//print default
			if (switchStatement->caseDefault != NULL &&
				switchStatement->caseDefaultIndex == numCases)
			{
				m_writer.Write(indent + 1, "default:\n");
				m_writer.WriteLine(indent + 1, "{");
				OutputStatements(indent + 2, switchStatement->caseDefault);
				m_writer.WriteLine(indent + 1, "}");
			}

			m_writer.WriteLine(indent, "}");
		}
        else if (statement->nodeType == HLSLNodeType_ForStatement)
        {
            HLSLForStatement* forStatement = static_cast<HLSLForStatement*>(statement);

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

			m_writer.BeginLine(indent, RawStr(whileStatement->fileName), whileStatement->line);
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
            m_writer.WriteLineTagged(indent, RawStr(blockStatement->fileName), blockStatement->line, "{");
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
			ASSERT_PARSER(0);
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
        if (declaration->registerName.IsNotEmpty())
        {
            sscanf(RawStr(declaration->registerName), "s%d", &reg);
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
                m_writer.Write("%s %s_texture : register(t%d); %s %s_sampler : register(s%d)", textureType, RawStr(declaration->name), reg, samplerType, RawStr(declaration->name), reg);
            }
            else
            {
                m_writer.Write("%s %s_texture; %s %s_sampler", textureType, RawStr(declaration->name), samplerType, RawStr(declaration->name));
            }
        }
        else
        {
            if (reg != -1)
            {
                m_writer.Write("%s %s : register(t%d)", textureType, RawStr(declaration->name), reg);
            }
            else
            {
                m_writer.Write("%s %s", textureType, RawStr(declaration->name));
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
    CachedString typeName = GetTypeName(type);
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
            typeName = MakeCached("Texture2DMS<float4>");
        }
    }

	if (type.baseType == HLSLBaseType_PointStream || type.baseType == HLSLBaseType_LineStream || type.baseType == HLSLBaseType_TriangleStream)
	{
		m_writer.Write("%s<%s> ", RawStr(typeName), RawStr(type.structuredTypeName));
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

	if (type.textureTypeName.IsNotEmpty())
	{
		if (type.typeName.IsNotEmpty())
		{
			m_writer.Write("%s<%s, %s> ", RawStr(typeName), RawStr(type.textureTypeName), RawStr(type.typeName));
		}
		else
		{
			m_writer.Write("%s<%s> ", RawStr(typeName), RawStr(type.textureTypeName));
		}
	}
	else
		m_writer.Write("%s ", RawStr(typeName));
}

void HLSLGenerator::OutputDeclarationBody(const HLSLType& type, const CachedString & name, const CachedString & semantic/*=NULL*/, const CachedString & registerName/*=NULL*/, HLSLExpression * assignment/*=NULL*/)
{
    m_writer.Write("%s", RawStr(name));

    if (type.array)
    {
		if (type.baseType == HLSLBaseType_InputPatch || type.baseType == HLSLBaseType_OutputPatch)
		{
			return;
		}

		// TODO: Fix this assert, happens when semantic="SV_TessFactor"
		//ASSERT_PARSER(semantic == NULL);
		
        m_writer.Write("[");
        if (type.arrayExtent[0])
        {
            m_writer.Write("%d", type.arrayExtent[0]);
        }
        m_writer.Write("]");
    }

    if (semantic.IsNotEmpty()) 
    {
        m_writer.Write(" : %s", RawStr(semantic));
    }

    if (registerName.IsNotEmpty())
    {
		if (!String_Equal(registerName,"none"))
		{
			if (m_isInsideBuffer)
			{
				m_writer.Write(" : packoffset(%s)", RawStr(registerName));
			}
			else 
			{
				m_writer.Write(" : register(%s)", RawStr(registerName));
			}
		}
    }

    if (assignment != NULL)
    {
        m_writer.Write(" = ");
        OutputExpression(assignment, true);
    }
}

void HLSLGenerator::OutputDeclaration(const HLSLType& type, const CachedString & name, const CachedString & semantic/*=NULL*/, const CachedString & registerName/*=NULL*/, HLSLExpression * assignment/*=NULL*/)
{
    OutputDeclarationType(type);
    OutputDeclarationBody(type, name, semantic, registerName, assignment);
}

bool HLSLGenerator::ChooseUniqueName(const char* base, CachedString & dstName, int dstLength) const
{
    // IC: Try without suffix first.
	char dst[1024];
    String_Printf(dst, dstLength, "%s", base);
    if (!m_tree->GetContainsString(base))
    {
		dstName = m_tree->AddStringCached(dst);
        return true;
    }

    for (int i = 1; i < 1024; ++i)
    {
        String_Printf(dst, dstLength, "%s%d", base, i);
        if (!m_tree->GetContainsString(dst))
        {
			dstName = m_tree->AddStringCached(dst);
			return true;
        }
    }
    return false;
}

CachedString HLSLGenerator::MakeCached(const char * str)
{
	CachedString ret = m_tree->AddStringCached(str);
	return ret;
}

void HLSLGenerator::OutputArrayExpression(int arrayDimension, HLSLExpression* (&arrayDimExpression)[MAX_DIM])
{
	for (int i = 0; i < arrayDimension; i++)
	{
		if (arrayDimExpression[i])
		{
			m_writer.Write("[");
			OutputExpression(arrayDimExpression[i], false);
			m_writer.Write("]");
		}
		else
		{
			m_writer.Write("[]");
		}
	}
}
