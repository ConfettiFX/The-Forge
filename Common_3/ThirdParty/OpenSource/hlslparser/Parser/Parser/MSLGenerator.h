#ifndef MSL_GENERATOR_H
#define MSL_GENERATOR_H

#include "CodeWriter.h"
#include "HLSLTree.h"


class  HLSLTree;
struct HLSLFunction;
struct HLSLStruct;
    
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

        int (*attributeCallback)(const char* name, unsigned int index);

        Options()
        {
            flags = 0;
            bufferRegisterOffset = 0;
			textureRegisterOffset = 0;
            attributeCallback = NULL;
        }
    };

    MSLGenerator();
	~MSLGenerator()
	{
		/*
		if (m_InputStructure)
		{
			HLSLStructField* field = m_InputStructure->field;

			while (field)
			{
				HLSLStructField* prevField = field;

				if (field->nextField)
				{					
					field = field->nextField;
				}

				delete prevField;
				prevField = NULL;
			}

			delete m_InputStructure;
			m_InputStructure = NULL;
		}
		*/
	}

    bool Generate(HLSLTree* tree, Target target, const char* entryName, const Options& options = Options());
    const char* GetResult() const;

private:
    
    // @@ Rename class argument. Add buffers & textures.
    struct ClassArgument
    {
        const char* name;
        HLSLType type;
        //const char* typeName;     // @@ Do we need more than the type name?
        const char* registerName;
		bool bStructuredBuffer;	

		const char* preprocessorContents;

        ClassArgument * nextArg;
        
        ClassArgument(const char* name, HLSLType type, const char * registerName, bool bStructuredBuffer = false) :
            name(name), type(type), registerName(registerName), bStructuredBuffer(bStructuredBuffer)
		{
			nextArg = NULL;
			preprocessorContents = NULL;
		}

		ClassArgument(const char* name, const char* preprocessorContents, HLSLType type, const char * registerName = NULL, bool bStructuredBuffer = false) :
			name(name), preprocessorContents(preprocessorContents), type(type), registerName(registerName), bStructuredBuffer(bStructuredBuffer)
		{
			nextArg = NULL;
		}


		/*
		ClassArgument(const char* name, HLSLBaseType baseType, const char * registerName) :
			name(name), baseType(baseType), registerName(registerName)
		{



			nextArg = NULL;
		}
		*/
    };

    void AddClassArgument(ClassArgument * arg);

    void Prepass(HLSLTree* tree, Target target, HLSLFunction* entryFunction, HLSLFunction* secondaryEntryFunction);
    void CleanPrepass();
    
    void PrependDeclarations();
    
    void OutputStatements(int indent, HLSLStatement* statement, const HLSLFunction* function);
    void OutputAttributes(int indent, HLSLAttribute* attribute, bool bMain);
    void OutputDeclaration(HLSLDeclaration* declaration, const HLSLFunction* function);
    void OutputStruct(int indent, HLSLStruct* structure);
    void OutputBuffer(int indent, HLSLBuffer* buffer);
    void OutputFunction(int indent, const HLSLFunction* function);
    void OutputExpression(HLSLExpression* expression, const HLSLType* dstType, HLSLExpression* parentExpression, const HLSLFunction* function, bool needsEndParen);
    void OutputCast(const HLSLType& type);
    
    void OutputArguments(HLSLArgument* argument, const HLSLFunction* function);
    void OutputDeclaration(const HLSLType& type, const char* name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef = false, bool isConst = false, int alignment = 0);
    void OutputDeclarationType(const HLSLType& type, bool isConst = false, bool isRef = false, int alignment = 0);
    void OutputDeclarationBody(const HLSLType& type, const char* name, HLSLExpression* assignment, const HLSLFunction* function, bool isRef = false);
    void OutputExpressionList(HLSLExpression* expression, const HLSLFunction* function);
    void OutputFunctionCallStatement(int indent, HLSLFunctionCall* functionCall);
    void OutputFunctionCall(HLSLFunctionCall* functionCall);

    const char* TranslateInputSemantic(const char* semantic);
    const char* TranslateOutputSemantic(const char* semantic);

    void Error(const char* format, ...);

	void OutPushConstantIdentifierTextureStateExpression(int size, int counter, const HLSLTextureStateExpression* pTextureStateExpression, bool* bWritten);
	//void OutPushConstantIdentifierRWTextureStateExpression(int size, int counter, const HLSLRWTextureStateExpression* pRWTextureStateExpression, bool* bWritten);

	bool matchFunctionArgumentsIdentifiers(HLSLArgument* argument, const char* name);


	const char* GetBuiltInSemantic(const char* semantic, HLSLArgumentModifier modifier, const char* argument = NULL, const char* field = NULL);

private:

    CodeWriter      m_writer;

    HLSLTree*       m_tree;
    const char*     m_entryName;
	const char*     m_secondaryEntryName;
    Target          m_target;
    Options         m_options;

    bool            m_error;

    ClassArgument * m_firstClassArgument;
    ClassArgument * m_lastClassArgument;

	//HLSLStruct*		m_InputStructure = NULL;


	unsigned int	attributeCounter;


	HLSLBuffer*  m_RWBuffers[64];
	int			   m_RWBufferCounter;

	HLSLBuffer*  m_RWStructuredBuffers[64];
	int					m_RWStructuredBufferCounter;

	HLSLBuffer*  m_PushConstantBuffers[64];
	int					m_PushConstantBufferCounter;

	HLSLStruct* m_StructBuffers[64];
	int			m_StructBuffersCounter;



};



#endif
