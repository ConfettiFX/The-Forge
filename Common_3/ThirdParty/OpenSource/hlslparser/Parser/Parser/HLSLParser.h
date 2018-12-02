//=============================================================================
//
// Render/HLSLParser.h
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#ifndef HLSL_PARSER_H
#define HLSL_PARSER_H

//#include "Engine/StringPool.h"
//#include "Engine/Array.h"
#include "Engine.h"

#include "HLSLTokenizer.h"
#include "HLSLTree.h"

#define MAX_INCLUDE_FILE 16

enum Target
{
	Target_VertexShader,
	Target_FragmentShader,
	Target_HullShader,
	Target_DomainShader,
	Target_GeometryShader,
	Target_ComputeShader,
};

enum Language
{
	Language_HLSL,
	Language_GLSL,	
	Language_MSL,
	Language_LegacyHLSL,	
	Language_ORBIS,
	Language_SWITCH,
};


struct EffectState;

struct PrepropStackData
{
	bool branchProp;

	//now three languagues
	bool passed[6];

	PrepropStackData()
	{
		branchProp = false;
		passed[0] = false;
		passed[1] = false;
		passed[2] = false;
		passed[3] = false;
		passed[4] = false;
		passed[5] = false;
	}
};

class HLSLParser
{

public:

    HLSLParser(Allocator* allocator, const char* fileName[], const char* buffer, size_t length, const char* entryName, Target target, Language language, const char* bufferForInlcuded[], int includedCounter);

	~HLSLParser()
	{
		delete mainTokenizer;
	}

    bool Parse(HLSLTree* tree/*, PreprocessorPackage packageArray[]*/);

	HLSLTokenizer*   currentTokenizer;

	HLSLTokenizer*   mainTokenizer;

	HLSLTokenizer*	pTokenizerForIncluded[MAX_INCLUDE_FILE];
	int				pTokenizerForIncludedCount;

private:

	bool Check(int token);
    bool Accept(int token);
    bool Expect(int token);

    /**
     * Special form of Accept for accepting a word that is not actually a token
     * but will be treated like one. This is useful for HLSL keywords that are
     * only tokens in specific contexts (like in/inout in parameter lists).
     */
    bool Accept(const char* token);
    bool Expect(const char* token);

    bool AcceptIdentifier(const char*& identifier);
	int AcceptMacroIdentifier(const char*& identifier);
    bool ExpectIdentifier(const char*& identifier);
	bool ExpectIdentifierForDefine(const char*& identifier);
    bool AcceptFloat(float& value);
	bool AcceptHalf( float& value );
    bool AcceptInt(int& value);
	bool AcceptUint(unsigned int& value);
    bool AcceptType(bool allowVoid, HLSLBaseType& type, const char*& typeName, int* typeFlags);
    bool ExpectType(bool allowVoid, HLSLBaseType& type, const char*& typeName, int* typeFlags);
    bool AcceptBinaryOperator(int priority, HLSLBinaryOp& binaryOp);
    bool AcceptUnaryOperator(bool pre, HLSLUnaryOp& unaryOp);
    bool AcceptAssign(HLSLBinaryOp& binaryOp);
    bool AcceptTypeModifier(int & typeFlags);
    bool AcceptInterpolationModifier(int& flags);

    /**
     * Handles a declaration like: "float2 name[5]". If allowUnsizedArray is true, it is
     * is acceptable for the declaration to not specify the bounds of the array (i.e. name[]).
     */
    bool AcceptDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name);
    bool ExpectDeclaration(bool allowUnsizedArray, HLSLType& type, const char*& name);

    bool ParseTopLevel(HLSLStatement*& statement);
    bool ParseBlock(HLSLStatement*& firstStatement, const HLSLType& returnType);
	bool ParseSwitchBlocks(HLSLStatement*& firstStatement, const HLSLType& returnType);
    bool ParseStatementOrBlock(HLSLStatement*& firstStatement, const HLSLType& returnType, bool bSwitchStatement);
    bool ParseStatement(HLSLStatement*& statement, const HLSLType& returnType);
    bool ParseDeclaration(HLSLDeclaration*& declaration);
    bool ParseFieldDeclaration(HLSLStructField*& field);
	//bool ParseSamplerStateFieldDeclaration(HLSLSamplerStateField*& field);
    //bool ParseBufferFieldDeclaration(HLSLBufferField*& field);
	bool ParseSamplerStateExpression(HLSLSamplerStateExpression*& expression);
    bool ParseExpression(HLSLExpression*& expression);
    bool ParseBinaryExpression(int priority, HLSLExpression*& expression);
    bool ParseTerminalExpression(HLSLExpression*& expression, bool& needsEndParen, bool bPreprocessor);
    bool ParseExpressionList(int endToken, bool allowEmptyEnd, HLSLExpression*& firstExpression, int& numExpressions);
    bool ParseArgumentList(HLSLArgument*& firstArgument, int& numArguments);
	bool ParseMacroFunctionArgumentList(HLSLArgument*& firstArgument, int& numArguments, HLSLBaseType &baseType);
    bool ParseDeclarationAssignment(HLSLDeclaration* declaration);
    bool ParsePartialConstructor(HLSLExpression*& expression, HLSLBaseType type, const char* typeName);

    bool ParseStateName(bool isSamplerState, bool isPipelineState, const char*& name, const EffectState *& state);
    bool ParseColorMask(int& mask);
    bool ParseStateValue(const EffectState * state, HLSLStateAssignment* stateAssignment);
    bool ParseStateAssignment(HLSLStateAssignment*& stateAssignment, bool isSamplerState, bool isPipelineState);
    bool ParseSamplerState(HLSLExpression*& expression);
    bool ParseTechnique(HLSLStatement*& statement);
    bool ParsePass(HLSLPass*& pass);
    bool ParsePipeline(HLSLStatement*& pipeline);
    bool ParseStage(HLSLStatement*& stage);

    bool ParseAttributeList(HLSLAttribute*& attribute);
    bool ParseAttributeBlock(HLSLAttribute*& attribute);

    bool CheckForUnexpectedEndOfStream(int endToken);

	bool AcceptBufferType(HLSLBuffer* pBuffer);

	bool GetBufferElementType(HLSLBuffer* pBuffer, bool bAllowVoid, int* pTypeFlag, bool optional);
	bool GetRegisterAssignment(HLSLBuffer* pBuffer, const char* errorMsg_bufferType);
	bool GetBufferBody(HLSLBuffer* pBuffer);
	void GetBufferArray(HLSLBuffer* pBuffer);
	
	bool GetRegisterAssignment(HLSLTextureState* pTextureState, const char* errorMsg_bufferType);
	void GetTextureArray(HLSLTextureState* pTextureState);		
	bool GetTextureElementType(HLSLTextureState* pTextureState, bool bAllowVoid, int* pTypeFlag, bool optional);
	bool AcceptTextureType(HLSLTextureState* pTextureState);

	
    HLSLStruct* FindUserDefinedType(const char* name) const;



	

	const HLSLpreprocessor* FindPreprocessorDefinedType(const char* name) const;
	const HLSLTextureState* FindTextureStateDefinedType(const char* name) const;
	const HLSLTextureState* FindTextureStateDefinedTypeWithAddress(const char* name) const;

	//const HLSLRWTextureState* FindRWTextureStateDefinedType(const char* name) const;
	const HLSLSamplerState* FindSamplerStateDefinedType(const char* name) const;

	HLSLBuffer* FindBuffer(const char* name) const;

	HLSLBuffer* FindConstantBuffer(const char* name) const;

	//HLSLConstantBuffer* FindCBufferDefinedType(const char* name) const;
	//HLSLRWBuffer* FindRWBuffer(const char* name) const;
	//HLSLRWStructuredBuffer* FindRWStructuredBuffer(const char* name) const;
	//HLSLStructuredBuffer* FindStructuredBuffer(const char* name) const;

	//void FindClosestTextureIdentifier(const HLSLTextureState* pTextureState, char* functionCaller, HLSLFunctionCall* functionCall, int i, const char* pIdentifierName);
	//void FindClosestTextureIdentifier(const HLSLRWTextureState* pTextureState, char* functionCaller, HLSLFunctionCall* functionCall, int i, const char* pIdentifierName);
	
    void BeginScope();
    void EndScope();

    void DeclareVariable(const char* name, const HLSLType& type);

    /** Returned pointer is only valid until Declare or Begin/EndScope is called. */
    const HLSLType* FindVariable(const char* name, bool& global) const;

    HLSLFunction* FindFunction(const char* name);
    const HLSLFunction* FindFunction(const HLSLFunction* fun) const;

    bool GetIsFunction(const char* name) const;
    
    /** Finds the overloaded function that matches the specified call. */
    const HLSLFunction* MatchFunctionCall(const HLSLFunctionCall* functionCall, const char* name);

    /** Gets the type of the named field on the specified object type (fieldName can also specify a swizzle. ) */
    bool GetMemberType(HLSLType& objectType, HLSLMemberAccess * memberAccess);

    bool CheckTypeCast(const HLSLType& srcType, const HLSLType& dstType);

    const char* GetFileName();
    int GetLineNumber() const;

	
	bool GenerateMacroFunctions(HLSLFunction* originalFunction, HLSLBaseType baseType);
	bool IsEmptyDefineIdentifier(HLSLStatement* &statement);


	HLSLpreprocessor* HandleBranchPreprofessor(const char* fileName, int line, bool *doesNotExpectSemicolon, bool *pValid);

	HLSLBaseType GetBaseTypeFromElement(const char* element);


    struct Variable
    {
        const char*     name;
        HLSLType        type;
    };

    
    Array<HLSLStruct*>      m_userTypes;

	Array<HLSLBuffer*>		m_Buffers;

	/*
	Array<HLSLConstantBuffer*>    m_cBuffers;
	Array<HLSLRWBuffer*>			  m_rwBuffer;
	Array<HLSLRWStructuredBuffer*>    m_rwStructuredBuffer;
	Array<HLSLStructuredBuffer*>	  m_structuredBuffer;
	*/

	Array<HLSLSamplerState*>      m_samplerStates;
	
	Array<HLSLTextureState*>      m_textureStates;
	//Array<HLSLRWTextureState*>    m_rwtextureStates;
	

	Array<HLSLTextureStateExpression*>      m_textureStateExpressions;
	//Array<HLSLRWTextureStateExpression*>      m_rwtextureStateExpressions;

	Array<HLSLpreprocessor*>    m_preProcessors;
    Array<Variable>         m_variables;
    Array<HLSLFunction*>    m_functions;
    int                     m_numGlobals;

    HLSLTree*               m_tree;
	const char*				m_entryName;

	Target					m_target;
	Language				m_language;


	Array<PrepropStackData*> m_PrepropStack;
	int						m_CurrentPrePropStack;



	int						m_BranchCurrentStack;
	int						m_BranchValidStack;
	bool					m_bEmbrace;

	const HLSLpreprocessor*	currentPreprocessor;

};



#endif