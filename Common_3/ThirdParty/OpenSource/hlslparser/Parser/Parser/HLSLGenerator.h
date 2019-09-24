//=============================================================================
//
// Render/HLSLGenerator.h
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#ifndef HLSL_GENERATOR_H
#define HLSL_GENERATOR_H

#include "CodeWriter.h"
#include "HLSLTree.h"

#pragma once


class  HLSLTree;
struct HLSLFunction;
struct HLSLStruct;

class StringLibrary;

/**
 * This class is used to generate HLSL which is compatible with the D3D9
 * compiler (i.e. no cbuffers).
 */
class HLSLGenerator
{

public:

    enum Target
    {
        Target_VertexShader,
        Target_PixelShader,
		Target_HullShader,
		Target_DomainShader,
		Target_GeometryShader,
		Target_ComputeShader,
    };

    HLSLGenerator();
    
    bool Generate(StringLibrary * stringLibrary, HLSLTree* tree, Target target, const char* entryName, bool legacy);
    const char* GetResult() const;

private:

    void OutputExpressionList(const eastl::vector<HLSLExpression*>& expressions, size_t start = 0);
    void OutputExpression(HLSLExpression* expression, bool needsEndParen);
	void OutputArguments(const eastl::vector<HLSLArgument*>& arguments);
	void OutputAttributes(int indent, HLSLAttribute* attribute);
    void OutputStatements(int indent, HLSLStatement* statement);
    void OutputDeclaration(HLSLDeclaration* declaration);
    void OutputDeclaration(const HLSLType& type, const CachedString & name, const CachedString & semantic = CachedString(), const CachedString & registerName = CachedString(), HLSLExpression* defaultValue = NULL);
    void OutputDeclarationType(const HLSLType& type);
    void OutputDeclarationBody(const HLSLType& type, const CachedString & name, const CachedString & semantic = CachedString(), const CachedString & registerName = CachedString(), HLSLExpression * assignment = NULL);
	void OutputArrayExpression(int arrayDimension, HLSLExpression* (&arrayDimExpression)[MAX_DIM]);

    /** Generates a name of the format "base+n" where n is an integer such that the name
     * isn't used in the syntax tree. */
    bool ChooseUniqueName(const char* base, CachedString & dstName, int dstLength) const;
	CachedString GetTypeName(const HLSLType& type);

	void WriteRegisterAndSpace(const CachedString & registerName, const CachedString & spaceName);

public:
	CachedString MakeCached(const char * str);

private:

    CodeWriter      m_writer;

    HLSLTree* m_tree;
    const char*     m_entryName;
    Target          m_target;
    bool            m_legacy;
    bool            m_isInsideBuffer;

	int				m_defaultSpaceIndex;
	CachedString	m_defaultSpaceName;

    CachedString    m_textureSampler2DStruct;
    CachedString    m_textureSampler2DCtor;
    CachedString    m_textureSampler2DShadowStruct;
    CachedString    m_textureSampler2DShadowCtor;
    CachedString    m_textureSampler3DStruct;
    CachedString    m_textureSampler3DCtor;
    CachedString    m_textureSamplerCubeStruct;
    CachedString    m_textureSamplerCubeCtor;
    CachedString    m_tex2DFunction;
    CachedString    m_tex2DProjFunction;
    CachedString    m_tex2DLodFunction;
    CachedString    m_tex2DBiasFunction;
    CachedString    m_tex2DGradFunction;
    CachedString    m_tex2DGatherFunction;
    CachedString    m_tex2DSizeFunction;
    CachedString    m_tex2DFetchFunction;
    CachedString    m_tex2DCmpFunction;
    CachedString    m_tex2DMSFetchFunction;
    CachedString    m_tex2DMSSizeFunction;
    CachedString    m_tex3DFunction;
    CachedString    m_tex3DLodFunction;
    CachedString    m_tex3DBiasFunction;
    CachedString    m_tex3DSizeFunction;
    CachedString    m_texCubeFunction;
    CachedString    m_texCubeLodFunction;
    CachedString    m_texCubeBiasFunction;
    CachedString    m_texCubeSizeFunction;

	CachedString	m_Sample;
	CachedString	m_SampleLevel;
	CachedString	m_SampleBias;
	
	StringLibrary * m_stringLibrary;
};


#endif