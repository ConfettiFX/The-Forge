#ifndef MSL_GENERATOR_H
#define MSL_GENERATOR_H

#include "CodeWriter.h"
#include "HLSLTree.h"

#include "../../../EASTL/vector.h"
#include "../../../EASTL/string.h"
#include "../../../EASTL/hash_set.h"
#include "Parser.h"

class  HLSLTree;
struct HLSLFunction;
struct HLSLStruct;

class StringLibrary;

/**
 * This class is used to generate MSL shaders.
 */
class MSLGenerator
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
    
    enum Flags
    {
        Flag_ConstShadowSampler = 1 << 0,
        Flag_PackMatrixRowMajor = 1 << 1,
        Flag_NoIndexAttribute   = 1 << 2,
    };

    struct Options
    {
        unsigned int flags;
        
		unsigned int textureRegisterOffset;
		unsigned int bufferRegisterOffset;

		bool bindingRequired;
		eastl::vector < BindingOverride > bindingOverrides;


        int (*attributeCallback)(const char* name, unsigned int index);

        Options()
        {
            flags = 0;
            bufferRegisterOffset = 0;
			textureRegisterOffset = 0;
            attributeCallback = NULL;

			bindingRequired = false;
			bindingOverrides.clear();
        }
    };

    MSLGenerator();
	~MSLGenerator()
	{
	}

    bool Generate(StringLibrary * stringLibrary, HLSLTree* tree, Target target, const char* entryName, const Options& options = Options());
    const char* GetResult() const;

private:
    
    // @@ Rename class argument. Add buffers & textures.
    struct ClassArgument
    {
        CachedString name;
        HLSLType type;
        CachedString registerName;
		bool bStructuredBuffer;	

        ClassArgument * nextArg;
        
        ClassArgument(CachedString nameParam, HLSLType typeParam, CachedString registerNameParam, bool bStructuredBufferParam = false) :
            name(nameParam), type(typeParam), registerName(registerNameParam), bStructuredBuffer(bStructuredBufferParam)
		{
			nextArg = NULL;
		}

		ClassArgument(CachedString nameParam, const char* preprocessorContents, HLSLType typeParam, CachedString registerNameParam = CachedString(), bool bStructuredBufferParam = false) :
			name(nameParam), type(typeParam), registerName(registerNameParam), bStructuredBuffer(bStructuredBufferParam)
		{
			nextArg = NULL;
		}

    };

    void AddClassArgument(ClassArgument * arg);

    void Prepass(HLSLTree* tree, Target target, HLSLFunction* entryFunction, HLSLFunction* secondaryEntryFunction);
    void CleanPrepass();
	
	int GetBufferRegister(const CachedString & cachedName);
	int GetTextureRegister(const CachedString & cachedName);
	int GetSamplerRegister(const CachedString & cachedName);



	//bool DoesEntryUseName(HLSLFunction* entryFunction, const CachedString & name);
	void GetAllEntryUsedNames(StringLibrary & foundNames,
		eastl::hash_set < const HLSLFunction * > & foundFuncs,
		HLSLFunction* entryFunction);

    void HideUnusedStatementDeclarations(HLSLFunction * entryFunction);

    void PrependDeclarations();
    
    void OutputStatements(int indent, HLSLStatement* statement, const HLSLFunction* function);
    void OutputAttributes(int indent, HLSLAttribute* attribute, bool bMain);
    void OutputDeclaration(HLSLDeclaration* declaration, const HLSLFunction* function);
    void OutputStruct(int indent, HLSLStruct* structure);
    void OutputBuffer(int indent, HLSLBuffer* buffer);
    void OutputFunction(int indent, const HLSLFunction* function);
    void OutputExpression(HLSLExpression* expression, const HLSLType* dstType, HLSLExpression* parentExpression, const HLSLFunction* function, bool needsEndParen);
    void OutputCast(const HLSLType& type);
    
    //void OutputArguments(HLSLArgument* argument, const HLSLFunction* function);
	void OutputArguments(const eastl::vector<HLSLArgument*>& arguments, const HLSLFunction* function);
	void OutputDeclaration(const HLSLType& type, const CachedString & name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef = false, bool isConst = false, int alignment = 0);
    void OutputDeclarationType(const HLSLType& type, bool isConst = false, bool isRef = false, int alignment = 0);
    void OutputDeclarationBody(const HLSLType& type, const CachedString & name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef = false);
	void OutputExpressionListConstructor(const eastl::vector<HLSLExpression*>& expressions, const HLSLFunction* function, HLSLBaseType expectedScalarType);
	void OutputExpressionList(const eastl::vector<HLSLExpression*>& expressionVec, const HLSLFunction* function, size_t start = 0);
    void OutputFunctionCall(HLSLFunctionCall* functionCall);

	CachedString GetTypeName(const HLSLType& type);

    CachedString TranslateInputSemantic(const CachedString & semantic, int incr);
	CachedString TranslateOutputSemantic(const CachedString & semantic);


	void Error(const char* format, ...);

	bool matchFunctionArgumentsIdentifiersVec(const eastl::vector < HLSLArgument* > & arguments, const CachedString & name);


	CachedString GetBuiltInSemantic(const CachedString & semantic, HLSLArgumentModifier modifier, const CachedString & argument = CachedString(), const CachedString & field = CachedString());


	CachedString MakeCached(const char * str);

private:

    CodeWriter      m_writer;

    HLSLTree*       m_tree;
    CachedString     m_entryName;
	CachedString     m_secondaryEntryName;
    Target          m_target;
    Options         m_options;

    bool            m_error;

    ClassArgument * m_firstClassArgument;
    ClassArgument * m_lastClassArgument;

	CachedString m_texIndexFuncName;

	int	attributeCounter;

	eastl::vector < HLSLBuffer* > m_RWBuffers;
	eastl::vector < HLSLBuffer* > m_RWStructuredBuffers;
	eastl::vector < HLSLBuffer* > m_PushConstantBuffers;
	eastl::vector < HLSLStruct* > m_StructBuffers;

	StringLibrary * m_stringLibrary;

	int m_nextTextureRegister;
	int m_nextSamplerRegister;
	int m_nextBufferRegister; // options start at 1

};



#endif
