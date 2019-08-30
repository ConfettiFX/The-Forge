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

#include "Engine.h"

#include "HLSLTokenizer.h"
#include "HLSLTree.h"
#include "FullTokenizer.h"

#include "StringLibrary.h"

#include "Parser.h"

struct EffectState;


class HLSLParser
{

public:
	/** This structure stores a HLSLFunction-like declaration for an intrinsic function */
	struct Intrinsic
	{
		void AllocArgs(int num);

		~Intrinsic();

		CachedString MakeCached(StringLibrary & stringLibrary, const char * name);

		explicit Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType);
		explicit Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1);
		explicit Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2);
		explicit Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3);
		explicit Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4);
		explicit Intrinsic(StringLibrary & stringLibrary, const char* name, HLSLBaseType returnType, HLSLBaseType arg1, HLSLBaseType arg2, HLSLBaseType arg3, HLSLBaseType arg4, HLSLBaseType arg5);

		HLSLFunction    fullFunction;
		HLSLArgument    argument[5];

		const char *    rawName;

	};

	struct IntrinsicHelper
	{
		IntrinsicHelper();

		~IntrinsicHelper();
		void BuildIntrinsics();

		StringLibrary m_intrinsicStringLibrary;
		eastl::vector < Intrinsic * > m_intrinsics;
	};

    HLSLParser(StringLibrary * stringLibrary, IntrinsicHelper * intrinsicHelper, FullTokenizer * pTokenizer, const char* entryName, Parser::Target target, Parser::Language language, const char debugTokenFileName[]);

	~HLSLParser()
	{
		m_pFullTokenizer = NULL;

		m_intrinsicHelper = NULL;
		m_pStringLibrary = NULL;
	}

    bool Parse(HLSLTree* tree);

	FullTokenizer * m_pFullTokenizer;
	StringLibrary * m_pStringLibrary;

	const char * GetCstr(const CachedString & currStr) const;

	void RevertParentStackToSize(int dstSize)
	{
		m_parentStackIdentifiers.resize(dstSize);
	}

	int GetParentStackSize() const
	{
		return (int)m_parentStackIdentifiers.size();
	}

	static bool GetBinaryOpResultType(HLSLBinaryOp binaryOp, const HLSLType& type1, const HLSLType& type2, HLSLType& result);


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

    bool AcceptFloat(float& value);
	bool AcceptHalf( float& value );
	bool AcceptMin16Float(float& value);
	bool AcceptMin10Float(float& value);
    bool AcceptInt(int& value);
	bool AcceptUint(unsigned int& value);
    bool AcceptType(bool allowVoid, HLSLBaseType& type, CachedString & typeName, int* typeFlags);
    bool ExpectType(bool allowVoid, HLSLBaseType& type, CachedString & typeName, int* typeFlags);
    bool AcceptBinaryOperator(int priority, HLSLBinaryOp& binaryOp);
    bool AcceptUnaryOperator(bool pre, HLSLUnaryOp& unaryOp);
    bool AcceptAssign(HLSLBinaryOp& binaryOp);
    bool AcceptTypeModifier(int & typeFlags);
    bool AcceptInterpolationModifier(int& flags);

	bool ApplyMemberAccessToNode(HLSLExpression * & expression);

	bool AcceptIdentifier(CachedString & identifier);
	bool ExpectIdentifier(CachedString & identifier);



    /**
     * Handles a declaration like: "float2 name[5]". If allowUnsizedArray is true, it is
     * is acceptable for the declaration to not specify the bounds of the array (i.e. name[]).
     */
    bool AcceptDeclaration(bool allowUnsizedArray, HLSLType& type, CachedString & name);
    bool ExpectDeclaration(bool allowUnsizedArray, HLSLType& type, CachedString & name);

    bool ParseTopLevel(HLSLStatement*& statement);
    bool ParseBlock(HLSLStatement*& firstStatement, const HLSLType& returnType);
	bool ParseSwitchBlocks(HLSLStatement*& firstStatement, const HLSLType& returnType);
    bool ParseStatementOrBlock(HLSLStatement*& firstStatement, const HLSLType& returnType, bool bSwitchStatement);
    bool ParseStatement(HLSLStatement*& statement, const HLSLType& returnType);
    bool ParseDeclaration(HLSLDeclaration*& declaration);
    bool ParseFieldDeclaration(HLSLStructField*& field);
	bool ParseSamplerStateExpression(HLSLSamplerStateExpression*& expression);
    bool ParseExpression(HLSLExpression*& expression, bool allowCommaOperator, int binaryPriority);
    bool ParseBinaryExpression(int priority, HLSLExpression*& expression);
    bool ParseTerminalExpression(HLSLExpression*& expression, bool& needsEndParen, bool bPreprocessor);
    bool ParseExpressionList(int endToken, bool allowEmptyEnd, HLSLExpression*& firstExpression, int& numExpressions);
	bool ParseArgumentVec(eastl::vector< HLSLArgument* > & argVec);
    bool ParseDeclarationAssignment(HLSLDeclaration* declaration);
    bool ParsePartialConstructor(HLSLExpression*& expression, HLSLBaseType type, const CachedString & typeName);

    bool ParseStateName(bool isSamplerState, bool isPipelineState, CachedString & name, const EffectState *& state);
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

	bool GetRegisterAssignment(HLSLStatement* pStatement, const char* errorMsg);

	bool AcceptBufferType(HLSLBuffer* pBuffer);

	bool GetBufferElementType(HLSLBuffer* pBuffer, bool bAllowVoid, int* pTypeFlag, bool optional);
	bool GetBufferBody(HLSLBuffer* pBuffer);
	void GetBufferArray(HLSLBuffer* pBuffer);
	
	void GetTextureArray(HLSLTextureState* pTextureState);		
	bool GetTextureElementType(HLSLTextureState* pTextureState, bool bAllowVoid, int* pTypeFlag, bool optional);
	bool AcceptTextureType(HLSLTextureState* pTextureState);

    HLSLStruct* FindUserDefinedType(const CachedString & name) const;

	const HLSLTextureState* FindTextureStateDefinedType(const CachedString & name) const;
	const HLSLTextureState* FindTextureStateDefinedTypeWithAddress(const CachedString & name) const;

	const HLSLSamplerState* FindSamplerStateDefinedType(const CachedString & name) const;
	HLSLBuffer* FindBuffer(const CachedString & name) const;

	HLSLBuffer* FindConstantBuffer(const CachedString & name) const;

	CachedString FindParentTextureOrBuffer(const HLSLTextureState* & foundTexture, HLSLBuffer * & foundBuffer) const;


    void BeginScope();
    void EndScope();

    void DeclareVariable(const CachedString & name, const HLSLType& type);

    /** Returned pointer is only valid until Declare or Begin/EndScope is called. */
    const HLSLType* FindVariable(const CachedString & name, bool& global) const;

    HLSLFunction* FindFunction(const CachedString & name);
    const HLSLFunction* FindFunction(const HLSLFunction* fun) const;

    bool GetIsFunction(const CachedString & name) const;
    
    /** Finds the overloaded function that matches the specified call. */
    const HLSLFunction* MatchFunctionCall(const HLSLFunctionCall* functionCall, const CachedString & name);

    /** Gets the type of the named field on the specified object type (fieldName can also specify a swizzle. ) */
    bool GetMemberType(HLSLType& objectType, HLSLMemberAccess * memberAccess);

    bool CheckTypeCast(const HLSLType& srcType, const HLSLType& dstType);

    const char* GetFileName();
    int GetLineNumber() const;

	CachedString GetTypeName(const HLSLType& type);


	HLSLBaseType GetBaseTypeFromElement(const char* element);

    struct Variable
    {
        CachedString     name;
        HLSLType        type;
    };
	    
    eastl::vector<HLSLStruct*>					m_userTypes;
	eastl::vector<HLSLBuffer*>					m_Buffers;
	eastl::vector<HLSLSamplerState*>				m_samplerStates;
	eastl::vector<HLSLTextureState*>				m_textureStates;
	eastl::vector<HLSLTextureStateExpression*>	m_textureStateExpressions;
	eastl::vector<Variable>						m_variables;
	eastl::vector<HLSLFunction*>					m_functions;
    int												m_numGlobals;

	IntrinsicHelper *		m_intrinsicHelper;

	// This requires some explanation. During the parsing steps, in nodes do not care about their parents,
	// and while parsing each node we only know about the nodes below us, or above us.
	// This causes a problem when dealing with certain nodes that could either be a buffer or a texture.
	// The example that caused this bug was:
	//
	//    TextureCube srcTexture : register(t1, space1);
	//    ...
	//    float dim[2];
	//    srcTexture.GetDimensions(dim[0], dim[1]);
	//
	// When GetDimensions() is parsed, it does not know if its parent is a texture or a buffer. If it's a texture,
	// we need to point to the last expression:
	//
	//     functionCall->pTextureStateExpression = m_textureStateExpressions[m_textureStateExpressions.size() - 1];
	//
	// and if it is a buffer we need to point to it.
	//
	//     functionCall->pBuffer = FindBuffer(prevIdentifier);
	//
	// The original code would look backwards in the history to estimate the identifier, but that would have problems
	// with nested function calls. So the new solution is to keep a special stack of nodes for the current parsed tree.
	// Then if we need to find out if our parent is a Texture/Buffer/something else, we can just go backwards through
	// this stack. The stack is only for the current statement as well.
	//
	// Also, note that this stack does not handle every node type and path in the parser. It has only been tested
	// for a few cases, namely resolving if a parent is a Texture or a Buffer.
	eastl::vector < CachedString > m_parentStackIdentifiers;

	eastl::deque< size_t > m_variableScopes;
	eastl::deque< size_t > m_textureStateScopes;
	eastl::deque< size_t > m_textureStateExpressionScopes;

	HLSLTree*               m_tree;
	eastl::string			m_entryNameStr;
	CachedString 			m_entryName;

	Parser::Target			m_target;
	Parser::Language		m_language;

};

#endif