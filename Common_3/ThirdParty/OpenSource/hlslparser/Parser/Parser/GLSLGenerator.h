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


struct GLSLPreprocessorPackage
{
	char	m_storedPreprocessors[256];
	int		m_line = -100;
};

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

        Options()
        {
            flags = 0;
            constantBufferPrefix = "";
        }
    };

    GLSLGenerator();
    
    bool Generate(HLSLTree* tree, Target target, Version versiom, const char* entryName, const Options& options = Options());
    const char* GetResult() const;

private:

    enum AttributeModifier
    {
        AttributeModifier_In,
        AttributeModifier_Out,
		AttributeModifier_Inout,
    };

    void OutputExpressionList(HLSLExpression* expression, HLSLArgument* argument = NULL);
    void OutputExpression(HLSLExpression* expression, const HLSLType* dstType = NULL);

	void OutputExpressionForBufferArray(HLSLExpression* expression, const HLSLType* dstType = NULL);

    void OutputIdentifier(const char* name);
    void OutputArguments(HLSLArgument* argument);
    

	void OutputAttributes(int indent, HLSLAttribute* attribute);
    /**
     * If the statements are part of a function, then returnType can be used to specify the type
     * that a return statement is expected to produce so that correct casts will be generated.
     */
    void OutputStatements(int indent, HLSLStatement* statement, const HLSLType* returnType = NULL);

    void OutputAttribute(const HLSLType& type, const char* semantic, AttributeModifier modifier, int *counter);
    void OutputAttributes(HLSLFunction* entryFunction);
    void OutputEntryCaller(HLSLFunction* entryFunction);
    void OutputDeclaration(HLSLDeclaration* declaration);
	void OutputDeclarationType( const HLSLType& type );
	void OutputDeclarationBody( const HLSLType& type, const char* name );
    void OutputDeclaration(const HLSLType& type, const char* name);
    void OutputCast(const HLSLType& type);

    void OutputSetOutAttribute(const char* semantic, const char* resultName);

    void LayoutBuffer(HLSLBuffer* buffer, unsigned int& offset);
    void LayoutBuffer(const HLSLType& type, unsigned int& offset);
    void LayoutBufferElement(const HLSLType& type, unsigned int& offset);
    void LayoutBufferAlign(const HLSLType& type, unsigned int& offset);

    HLSLBuffer* GetBufferAccessExpression(HLSLExpression* expression);
    void OutputBufferAccessExpression(HLSLBuffer* buffer, HLSLExpression* expression, const HLSLType& type, unsigned int postOffset);
    unsigned int OutputBufferAccessIndex(HLSLExpression* expression, unsigned int postOffset);

    void OutputBuffer(int indent, HLSLBuffer* buffer);

	void OutPushConstantIdentifierTextureStateExpression(int size, int counter, const HLSLTextureStateExpression* pTextureStateExpression, bool* bWritten);
	//void OutPushConstantIdentifierRWTextureStateExpression(int size, int counter, const HLSLRWTextureStateExpression* prwTextureStateExpression, bool* bWritten);

    HLSLFunction* FindFunction(HLSLRoot* root, const char* name);
    HLSLStruct* FindStruct(HLSLRoot* root, const char* name);

    void Error(const char* format, ...);

    /** GLSL contains some reserved words that don't exist in HLSL. This function will
     * sanitize those names. */
    const char* GetSafeIdentifierName(const char* name) const;

    /** Generates a name of the format "base+n" where n is an integer such that the name
     * isn't used in the syntax tree. */
    bool ChooseUniqueName(const char* base, char* dst, int dstLength) const;

    const char* GetBuiltInSemantic(const char* semantic, AttributeModifier modifier, int* outputIndex = 0);
	const char* GetBuiltInSemantic(const char* semantic, AttributeModifier modifier, const HLSLType& type, int* outputIndex = 0);
    const char* GetAttribQualifier(AttributeModifier modifier);

	void GetRegisterNumbering(const char* registerName, char* dst);
	int GetLayoutSetNumbering(const char* registerSpaceName);

	void PrintPreprocessors(int currentLine);


	bool HandleTextureStateBinaryExpression(HLSLExpression* bie, HLSLTextureStateExpression* tse)
	{
		HLSLBinaryExpression* binaryExpression = static_cast<HLSLBinaryExpression*>(bie);

		bool result = false;

		//recursive
		if (binaryExpression->expression1->nodeType == HLSLNodeType_BinaryExpression)
		{
			result = HandleTextureStateBinaryExpression(binaryExpression->expression1, tse);
		}

		if (result)
			return result;

		if (binaryExpression->expression2->nodeType == HLSLNodeType_BinaryExpression)
		{
			result = HandleTextureStateBinaryExpression(binaryExpression->expression2, tse);
		}

		if (result)
			return result;


		if (binaryExpression->expression1->nodeType == HLSLNodeType_FunctionCall)
		{
			HLSLFunctionCall* fc = static_cast<HLSLFunctionCall*>(binaryExpression->expression1);
			fc->pTextureStateExpression = tse;

			OutputExpression(binaryExpression->expression1);
		}
		else if (binaryExpression->expression1->nodeType == HLSLNodeType_MemberAccess)
		{
			HLSLMemberAccess* ma = static_cast<HLSLMemberAccess*>(binaryExpression->expression1);
			HLSLFunctionCall* fc = static_cast<HLSLFunctionCall*>(ma->object);
			fc->pTextureStateExpression = tse;

			OutputExpression(binaryExpression->expression1);
		}
		else if (binaryExpression->expression2->nodeType == HLSLNodeType_FunctionCall)
		{
			HLSLFunctionCall* fc = static_cast<HLSLFunctionCall*>(binaryExpression->expression2);
			fc->pTextureStateExpression = tse;

			OutputExpression(binaryExpression->expression2);
		}
		else if (binaryExpression->expression2->nodeType == HLSLNodeType_MemberAccess)
		{
			HLSLMemberAccess* ma = static_cast<HLSLMemberAccess*>(binaryExpression->expression2);
			HLSLFunctionCall* fc = static_cast<HLSLFunctionCall*>(ma->object);
			fc->pTextureStateExpression = tse;

			OutputExpression(binaryExpression->expression2);
		}
		else
		{
			//error
			return false;
		}

		return true;
	}


	bool IsRWTexture(HLSLBaseType type)
	{
		switch (type)
		{
			case HLSLBaseType_RWTexture1D:
			case HLSLBaseType_RWTexture1DArray:
			case HLSLBaseType_RWTexture2D:
			case HLSLBaseType_RWTexture2DArray:
			case HLSLBaseType_RWTexture3D:
				return true;
			default:
				return false;
		}			
	}

	bool IsRasterizerOrderedTexture(HLSLBaseType type)
	{
		switch (type)
		{
		case HLSLBaseType_RasterizerOrderedTexture1D:
		case HLSLBaseType_RasterizerOrderedTexture1DArray:
		case HLSLBaseType_RasterizerOrderedTexture2D:
		case HLSLBaseType_RasterizerOrderedTexture2DArray:
		case HLSLBaseType_RasterizerOrderedTexture3D:
			return true;
		default:
			return false;
		}
	}

	bool IsTexture(HLSLBaseType type)
	{
		switch (type)
		{
		case HLSLBaseType_Texture1D:
		case HLSLBaseType_Texture1DArray:
		case HLSLBaseType_Texture2D:
		case HLSLBaseType_Texture2DArray:
		case HLSLBaseType_Texture3D:
		case HLSLBaseType_Texture2DMS:
		case HLSLBaseType_Texture2DMSArray:
		case HLSLBaseType_TextureCube:
		case HLSLBaseType_TextureCubeArray:
			return true;
		default:
			return false;
		}
	}

	


private:

    static const int    s_numReservedWords = 7;
    static const char*  s_reservedWord[s_numReservedWords];

    CodeWriter          m_writer;

    HLSLTree*           m_tree;
    const char*         m_entryName;
    Target              m_target;
    Version             m_version;
    bool                m_versionLegacy;
    Options             m_options;

    bool                m_outputPosition;
    int                 m_outputTargets;

	HLSLBaseType		m_outputTypes[64];

    const char*         m_outAttribPrefix;
    const char*         m_inAttribPrefix;

    char                m_matrixRowFunction[64];
    char                m_matrixCtorFunction[64];
    char                m_matrixMulFunction[64];
    char                m_clipFunction[64];
    char                m_tex2DlodFunction[64];
    char                m_tex2DbiasFunction[64];
    char                m_tex2DgradFunction[64];
    char                m_tex3DlodFunction[64];
    char                m_texCUBEbiasFunction[64];
	char                m_texCUBElodFunction[ 64 ];
    char                m_scalarSwizzle2Function[64];
    char                m_scalarSwizzle3Function[64];
    char                m_scalarSwizzle4Function[64];
    char                m_sinCosFunction[64];
	char                m_bvecTernary[ 64 ];
	char				m_f16tof32Function[64];
	char				m_f32tof16Function[64];

    bool                m_error;

    char                m_reservedWord[s_numReservedWords][64];


	char				m_StructuredBufferNames[128][64];
	int					m_StructuredBufferCounter;

	HLSLBuffer*			m_PushConstantBuffers[64];
	int					m_PushConstantBufferCounter;

	GLSLPreprocessorPackage m_preprocessorPackage[128];
	char				m_outputGeometryType[16];


	const char*			m_domain;
	const char*			m_partitioning;
	const char*			m_outputtopology;
	const char*			m_patchconstantfunc;

	const char*			m_geoInputdataType;
	const char*			m_geoOutputdataType;
};


#endif