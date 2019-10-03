//=============================================================================
//
// Render/GLSLGenerator.h
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#ifndef GLSL_GENERATOR_H
#define GLSL_GENERATOR_H

#include "CodeWriter.h"
#include "HLSLTree.h"
#include "Parser.h"

class StringLibrary;

class GLSLGenerator
{

public:
    enum Target
    {
        Target_VertexShader,
        Target_FragmentShader,
		Target_HullShader,
		Target_DomainShader,
		Target_GeometryShader,
		Target_ComputeShader,
    };

    enum Version
    {
        Version_110, // OpenGL 2.0
        Version_140, // OpenGL 3.1
        Version_150, // OpenGL 3.2
		Version_450, // OpenGL 4.5
        Version_100_ES, // OpenGL ES 2.0
        Version_300_ES, // OpenGL ES 3.0
    };

    enum Flags
    {
        Flag_FlipPositionOutput = 1 << 0,
        Flag_EmulateConstantBuffer = 1 << 1,
        Flag_PackMatrixRowMajor = 1 << 2,
        Flag_LowerMatrixMultiplication = 1 << 3,
    };

    struct Options
    {
        unsigned int flags;
        const char* constantBufferPrefix;
		eastl::vector < BindingShift >shiftVec;

        Options()
        {
            flags = 0;
            constantBufferPrefix = "";
        }
    };

    GLSLGenerator();
    
    bool Generate(StringLibrary * stringLibrary, HLSLTree* tree, Target target, Version versiom, const char* entryName, const Options& options = Options());
    const char* GetResult() const;

private:

    enum AttributeModifier
    {
        AttributeModifier_In,
        AttributeModifier_Out,
		AttributeModifier_Inout,
    };

	void OutputExpressionList(const eastl::vector<HLSLExpression*>& expressions, size_t i = 0);
	void OutputExpression(HLSLExpression* expression, const HLSLType* dstType = NULL, bool allowCast = true);
	void OutputExpressionForBufferArray(HLSLExpression* expression, const HLSLType* dstType = NULL);

	void OutputIdentifier(const CachedString & name);
	void OutputIdentifierExpression(HLSLIdentifierExpression* pIdentExpr);
	void OutputArguments(const eastl::vector < HLSLArgument* > & arguments);


	void OutputAttributes(int indent, HLSLAttribute* attribute);
    /**
     * If the statements are part of a function, then returnType can be used to specify the type
     * that a return statement is expected to produce so that correct casts will be generated.
     */
    void OutputStatements(int indent, HLSLStatement* statement, const HLSLType* returnType = NULL);

    void OutputAttribute(const HLSLType& type, const CachedString & semantic, AttributeModifier modifier, int *counter);
    void OutputAttributes(HLSLFunction* entryFunction);
    void OutputEntryCaller(HLSLFunction* entryFunction);
    void OutputDeclaration(HLSLDeclaration* declaration);
	void OutputDeclarationType( const HLSLType& type );
	void OutputDeclarationBody( const HLSLType& type, const CachedString & name);
    void OutputDeclaration(const HLSLType& type, const CachedString & name);
    void OutputCast(const HLSLType& type);

    void OutputSetOutAttribute(const char* semantic, const CachedString & resultName);

    void LayoutBuffer(HLSLBuffer* buffer, unsigned int& offset);
    void LayoutBuffer(const HLSLType& type, unsigned int& offset);
    void LayoutBufferElement(const HLSLType& type, unsigned int& offset);
    void LayoutBufferAlign(const HLSLType& type, unsigned int& offset);

    HLSLBuffer* GetBufferAccessExpression(HLSLExpression* expression);
    void OutputBufferAccessExpression(HLSLBuffer* buffer, HLSLExpression* expression, const HLSLType& type, unsigned int postOffset);
    unsigned int OutputBufferAccessIndex(HLSLExpression* expression, unsigned int postOffset);

	void OutputArrayExpression(int arrayDimension, HLSLExpression* (&arrayDimExpression)[MAX_DIM]);

    void OutputBuffer(int indent, HLSLBuffer* buffer);

    HLSLFunction* FindFunction(HLSLRoot* root, const CachedString & name);
    HLSLStruct* FindStruct(HLSLRoot* root, const CachedString & name);

    void Error(const char* format, ...);

    /** GLSL contains some reserved words that don't exist in HLSL. This function will
     * sanitize those names. */
	CachedString GetSafeIdentifierName(const CachedString & name) const;

    /** Generates a name of the format "base+n" where n is an integer such that the name
     * isn't used in the syntax tree. */
	bool ChooseUniqueName(const char* base, CachedString & dstName) const;

    CachedString GetBuiltInSemantic(const CachedString & semantic, AttributeModifier modifier, int* outputIndex = 0);
	CachedString GetBuiltInSemantic(const CachedString & semantic, AttributeModifier modifier, const HLSLType& type, int* outputIndex = 0);
	CachedString GetAttribQualifier(AttributeModifier modifier);

	CachedString MakeCached(const char * str);

private:
    static const int    s_numReservedWords = 7;
    static const char*  s_reservedWord[s_numReservedWords];

    CodeWriter          m_writer;

    HLSLTree*           m_tree;
    CachedString         m_entryName;
    Target              m_target;
    Version             m_version;
    bool                m_versionLegacy;
    Options             m_options;

    bool                m_outputPosition;
	eastl::vector < HLSLBaseType > m_outputTypes;

    CachedString        m_outAttribPrefix;
    CachedString        m_inAttribPrefix;

    CachedString		m_matrixRowFunction;
    CachedString		m_matrixCtorFunction;
    CachedString		m_matrixMulFunction;
    CachedString		m_clipFunction;
	CachedString		m_tex2DlodFunction;
	CachedString		m_tex2DbiasFunction;
    CachedString		m_tex2DgradFunction;
    CachedString		m_tex3DlodFunction;
    CachedString		m_texCUBEbiasFunction;
	CachedString		m_texCUBElodFunction;
	CachedString		m_textureLodOffsetFunction;
	CachedString		m_scalarSwizzle2Function;
    CachedString		m_scalarSwizzle3Function;
    CachedString		m_scalarSwizzle4Function;
    CachedString		m_sinCosFunction;
	CachedString		m_bvecTernary;
	CachedString		m_f16tof32Function;
	CachedString		m_f32tof16Function;
	CachedString		m_getDimensions;
	CachedString		m_mulMatFunction;

    bool                m_error;

	CachedString		m_reservedWord[s_numReservedWords];

	eastl::vector < CachedString> m_StructuredBufferNames;
	eastl::vector < HLSLBuffer*> m_PushConstantBuffers;

	CachedString		m_outputGeometryType;

	CachedString		m_domain;
	CachedString		m_partitioning;
	CachedString		m_outputtopology;
	CachedString		m_patchconstantfunc;

	CachedString		m_geoInputdataType;
	CachedString		m_geoOutputdataType;

	StringLibrary * m_stringLibrary;
};


#endif