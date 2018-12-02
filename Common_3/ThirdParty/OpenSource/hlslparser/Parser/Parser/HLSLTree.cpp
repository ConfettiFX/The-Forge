//#include "Engine/Assert.h"
#include "Engine.h"

#include "HLSLTree.h"

HLSLTree::HLSLTree(Allocator* allocator) :
    m_allocator(allocator), m_stringPool(allocator), m_defineStringPool(allocator)
{
    m_firstPage         = m_allocator->New<NodePage>();
    m_firstPage->next   = NULL;

    m_currentPage       = m_firstPage;
    m_currentPageOffset = 0;

    m_root              = AddNode<HLSLRoot>(NULL, 1);


	for (int i = 0; i < MAX_GLOBAL_EXTENSION; i++)
	{
		gExtension[i] = false;
	}
}

HLSLTree::~HLSLTree()
{
    NodePage* page = m_firstPage;
    while (page != NULL)
    {
        NodePage* next = page->next;
        m_allocator->Delete(page);
        page = next;
    }
}

void HLSLTree::AllocatePage()
{
    NodePage* newPage    = m_allocator->New<NodePage>();
    newPage->next        = NULL;
    m_currentPage->next  = newPage;
    m_currentPageOffset  = 0;
    m_currentPage        = newPage;
}

const char* HLSLTree::AddString(const char* string)
{   
    return m_stringPool.AddString(string);
}

const char* HLSLTree::AddDefineString(const char* string)
{
	return m_defineStringPool.AddString(string);
}

const char* HLSLTree::AddStringFormat(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    const char * string = m_stringPool.AddStringFormatList(format, args);
    va_end(args);
    return string;
}

bool HLSLTree::GetContainsString(const char* string) const
{
    return m_stringPool.GetContainsString(string);
}

HLSLRoot* HLSLTree::GetRoot() const
{
    return m_root;
}

void* HLSLTree::AllocateMemory(size_t size)
{
    if (m_currentPageOffset + size > s_nodePageSize)
    {
        AllocatePage();
    }
    void* buffer = m_currentPage->buffer + m_currentPageOffset;
    m_currentPageOffset += size;
    return buffer;
}

// @@ This doesn't do any parameter matching. Simply returns the first function with that name.
HLSLFunction * HLSLTree::FindFunction(const char * name, int index)
{
    HLSLStatement * statement = m_root->statement;

	int count = 0;

    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Function)
        {
            HLSLFunction * function = (HLSLFunction *)statement;
            if (String_Equal(name, function->name))
            {
				if(index == count)
					return function;

				count++;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLDeclaration * HLSLTree::FindGlobalDeclaration(const char * name, HLSLBuffer ** buffer_out/*=NULL*/)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration * declaration = (HLSLDeclaration *)statement;
            if (String_Equal(name, declaration->name))
            {
                if (buffer_out) *buffer_out = NULL;
                return declaration;
            }
        }
        else if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer* buffer = (HLSLBuffer*)statement;

            HLSLDeclaration* field = buffer->field;
            while (field != NULL)
            {
                ASSERT(field->nodeType == HLSLNodeType_Declaration);
                if (String_Equal(name, field->name))
                {
                    if (buffer_out) *buffer_out = buffer;
                    return field;
                }
                field = (HLSLDeclaration*)field->nextStatement;
            }
        }

        statement = statement->nextStatement;
    }

    if (buffer_out) *buffer_out = NULL;
    return NULL;
}

HLSLStruct * HLSLTree::FindGlobalStruct(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Struct)
        {
            HLSLStruct * declaration = (HLSLStruct *)statement;
            if (String_Equal(name, declaration->name))
            {
                return declaration;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

const char* HLSLTree::FindGlobalStructMember(const char *memberName)
{
	HLSLStatement * statement = m_root->statement;
	while (statement != NULL)
	{
		if (statement->nodeType == HLSLNodeType_Struct)
		{
			HLSLStruct * declaration = (HLSLStruct *)statement;

			HLSLStructField* field = declaration->field;

			while (field != NULL)
			{
				if (String_Equal(memberName, field->name))
				{
					return declaration->name;
				}

				field = field->nextField;
			}
		}

		statement = statement->nextStatement;
	}

	return NULL;
}


const char* HLSLTree::FindBuffertMember(const char *memberName)
{
	HLSLStatement * statement = m_root->statement;
	while (statement != NULL)
	{
		if (statement->nodeType == HLSLNodeType_Buffer)
		{
			HLSLBuffer * buffer = (HLSLBuffer *)statement;

			HLSLDeclaration* field = buffer->field;
			
			while (field != NULL)
			{
				if (String_Equal(memberName, field->name))
				{
					return buffer->name;
				}

				field = (HLSLDeclaration*)field->nextStatement;
			}
		}

		statement = statement->nextStatement;
	}

	return NULL;
}


HLSLTechnique * HLSLTree::FindTechnique(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Technique)
        {
            HLSLTechnique * technique = (HLSLTechnique *)statement;
            if (String_Equal(name, technique->name))
            {
                return technique;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLPipeline * HLSLTree::FindFirstPipeline()
{
    return FindNextPipeline(NULL);
}

HLSLPipeline * HLSLTree::FindNextPipeline(HLSLPipeline * current)
{
    HLSLStatement * statement = current ? current : m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Pipeline)
        {
            return (HLSLPipeline *)statement;
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLPipeline * HLSLTree::FindPipeline(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Pipeline)
        {
            HLSLPipeline * pipeline = (HLSLPipeline *)statement;
            if (String_Equal(name, pipeline->name))
            {
                return pipeline;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLBuffer * HLSLTree::FindBuffer(const char * name)
{
    HLSLStatement * statement = m_root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer * buffer = (HLSLBuffer *)statement;
            if (String_Equal(name, buffer->name))
            {
                return buffer;
            }
        }

        statement = statement->nextStatement;
    }

    return NULL;
}

HLSLTextureStateExpression * HLSLTree::FindTextureStateExpression(const char * name)
{
	HLSLStatement * statement = m_root->statement;
	while (statement != NULL)
	{
		if (statement->nodeType == HLSLNodeType_TextureStateExpression)
		{
			HLSLTextureStateExpression * textureExpression = (HLSLTextureStateExpression *)statement;
			if (String_Equal(name, textureExpression->name))
			{
				return textureExpression;
			}
		}

		statement = statement->nextStatement;
	}

	return NULL;
}





bool HLSLTree::GetExpressionValue(HLSLExpression * expression, int & value)
{
    ASSERT (expression != NULL);

    // Expression must be constant.
    if ((expression->expressionType.flags & HLSLTypeFlag_Const) == 0) 
    {
        return false;
    }

    // We are expecting an integer scalar. @@ Add support for type conversion from other scalar types.
    if (expression->expressionType.baseType != HLSLBaseType_Int &&
        expression->expressionType.baseType != HLSLBaseType_Bool)
    {
        return false;
    }

    if (expression->expressionType.array) 
    {
        return false;
    }

    if (expression->nodeType == HLSLNodeType_BinaryExpression) 
    {
        HLSLBinaryExpression * binaryExpression = (HLSLBinaryExpression *)expression;

        int value1, value2;
        if (!GetExpressionValue(binaryExpression->expression1, value1) ||
            !GetExpressionValue(binaryExpression->expression2, value2))
        {
            return false;
        }

        switch(binaryExpression->binaryOp)
        {
            case HLSLBinaryOp_And:
                value = value1 && value2;
                return true;
            case HLSLBinaryOp_Or:
                value = value1 || value2;
                return true;
            case HLSLBinaryOp_Add:
                value = value1 + value2;
                return true;
            case HLSLBinaryOp_Sub:
                value = value1 - value2;
                return true;
            case HLSLBinaryOp_Mul:
                value = value1 * value2;
                return true;
            case HLSLBinaryOp_Div:
                value = value1 / value2;
                return true;
            case HLSLBinaryOp_Less:
                value = value1 < value2;
                return true;
            case HLSLBinaryOp_Greater:
                value = value1 > value2;
                return true;
            case HLSLBinaryOp_LessEqual:
                value = value1 <= value2;
                return true;
            case HLSLBinaryOp_GreaterEqual:
                value = value1 >= value2;
                return true;
            case HLSLBinaryOp_Equal:
                value = value1 == value2;
                return true;
            case HLSLBinaryOp_NotEqual:
                value = value1 != value2;
                return true;
            case HLSLBinaryOp_BitAnd:
                value = value1 & value2;
                return true;
            case HLSLBinaryOp_BitOr:
                value = value1 | value2;
                return true;
            case HLSLBinaryOp_BitXor:
                value = value1 ^ value2;
                return true;
            case HLSLBinaryOp_Assign:
            case HLSLBinaryOp_AddAssign:
            case HLSLBinaryOp_SubAssign:
            case HLSLBinaryOp_MulAssign:
            case HLSLBinaryOp_DivAssign:
                // IC: These are not valid on non-constant expressions and should fail earlier when querying expression value.
                return false;
        }
    }
    else if (expression->nodeType == HLSLNodeType_UnaryExpression) 
    {
        HLSLUnaryExpression * unaryExpression = (HLSLUnaryExpression *)expression;

        if (!GetExpressionValue(unaryExpression->expression, value))
        {
            return false;
        }

        switch(unaryExpression->unaryOp)
        {
            case HLSLUnaryOp_Negative:
                value = -value;
                return true;
            case HLSLUnaryOp_Positive:
                // nop.
                return true;
            case HLSLUnaryOp_Not:
                value = !value;
                return true;
            case HLSLUnaryOp_BitNot:
                value = ~value;
                return true;
            case HLSLUnaryOp_PostDecrement:
            case HLSLUnaryOp_PostIncrement:
            case HLSLUnaryOp_PreDecrement:
            case HLSLUnaryOp_PreIncrement:
                // IC: These are not valid on non-constant expressions and should fail earlier when querying expression value.
                return false;
        }
    }
    else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression * identifier = (HLSLIdentifierExpression *)expression;

        HLSLDeclaration * declaration = FindGlobalDeclaration(identifier->name);
        if (declaration == NULL) 
        {
            return false;
        }
        if ((declaration->type.flags & HLSLTypeFlag_Const) == 0)
        {
            return false;
        }

        return GetExpressionValue(declaration->assignment, value);
    }
    else if (expression->nodeType == HLSLNodeType_LiteralExpression) 
    {
        HLSLLiteralExpression * literal = (HLSLLiteralExpression *)expression;
   
        if (literal->expressionType.baseType == HLSLBaseType_Int) value = literal->iValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Bool) value = (int)literal->bValue;
        else return false;
        
        return true;
    }

    return false;
}

bool HLSLTree::NeedsFunction(const char* name)
{
    // Early out
    if (!GetContainsString(name))
        return false;

    struct NeedsFunctionVisitor: HLSLTreeVisitor
    {
        const char* name;
        bool result;

        virtual void VisitTopLevelStatement(HLSLStatement * node)
        {
            if (!node->hidden)
                HLSLTreeVisitor::VisitTopLevelStatement(node);
        }

        virtual void VisitFunctionCall(HLSLFunctionCall * node)
        {
            result = result || String_Equal(name, node->function->name);

            HLSLTreeVisitor::VisitFunctionCall(node);
        }
    };

    NeedsFunctionVisitor visitor;
    visitor.name = name;
    visitor.result = false;

    visitor.VisitRoot(m_root);

    return visitor.result;
}

int GetVectorDimension(HLSLType & type)
{
    if (type.baseType >= HLSLBaseType_FirstNumeric &&
        type.baseType <= HLSLBaseType_LastNumeric)
    {
        if (type.baseType == HLSLBaseType_Float || type.baseType == HLSLBaseType_Half) return 1;
        if (type.baseType == HLSLBaseType_Float2 || type.baseType == HLSLBaseType_Half2) return 2;
        if (type.baseType == HLSLBaseType_Float3 || type.baseType == HLSLBaseType_Half3) return 3;
        if (type.baseType == HLSLBaseType_Float4 || type.baseType == HLSLBaseType_Half4) return 4;

    }
    return 0;
}

// Returns dimension, 0 if invalid.
int HLSLTree::GetExpressionValue(HLSLExpression * expression, float values[4])
{
    ASSERT (expression != NULL);

    // Expression must be constant.
    if ((expression->expressionType.flags & HLSLTypeFlag_Const) == 0) 
    {
        return 0;
    }

    if (expression->expressionType.baseType == HLSLBaseType_Int ||
        expression->expressionType.baseType == HLSLBaseType_Bool)
    {
        int int_value;
        if (GetExpressionValue(expression, int_value)) {
            for (int i = 0; i < 4; i++) values[i] = (float)int_value;   // @@ Warn if conversion is not exact.
            return 1;
        }

        return 0;
    }
    if (expression->expressionType.baseType >= HLSLBaseType_FirstInteger && expression->expressionType.baseType <= HLSLBaseType_LastInteger)
    {
        // @@ Add support for uints?
        // @@ Add support for int vectors?
        return 0;
    }
    if (expression->expressionType.baseType > HLSLBaseType_LastNumeric)
    {
        return 0;
    }

    // @@ Not supported yet, but we may need it?
    if (expression->expressionType.array) 
    {
        return false;
    }

    if (expression->nodeType == HLSLNodeType_BinaryExpression) 
    {
        HLSLBinaryExpression * binaryExpression = (HLSLBinaryExpression *)expression;
        int dim = GetVectorDimension(binaryExpression->expressionType);

        float values1[4], values2[4];
        int dim1 = GetExpressionValue(binaryExpression->expression1, values1);
        int dim2 = GetExpressionValue(binaryExpression->expression2, values2);

        if (dim1 == 0 || dim2 == 0)
        {
            return 0;
        }

        if (dim1 != dim2)
        {
            // Brodacast scalar to vector size.
            if (dim1 == 1)
            {
                for (int i = 1; i < dim2; i++) values1[i] = values1[0];
                dim1 = dim2;
            }
            else if (dim2 == 1)
            {
                for (int i = 1; i < dim1; i++) values2[i] = values2[0];
                dim2 = dim1;
            }
            else
            {
                return 0;
            }
        }
        ASSERT(dim == dim1);

        switch(binaryExpression->binaryOp)
        {
            case HLSLBinaryOp_Add:
                for (int i = 0; i < dim; i++) values[i] = values1[i] + values2[i];
                return dim;
            case HLSLBinaryOp_Sub:
                for (int i = 0; i < dim; i++) values[i] = values1[i] - values2[i];
                return dim;
            case HLSLBinaryOp_Mul:
                for (int i = 0; i < dim; i++) values[i] = values1[i] * values2[i];
                return dim;
            case HLSLBinaryOp_Div:
                for (int i = 0; i < dim; i++) values[i] = values1[i] / values2[i];
                return dim;
            default:
                return 0;
        }
    }
    else if (expression->nodeType == HLSLNodeType_UnaryExpression) 
    {
        HLSLUnaryExpression * unaryExpression = (HLSLUnaryExpression *)expression;
        int dim = GetVectorDimension(unaryExpression->expressionType);

        int dim1 = GetExpressionValue(unaryExpression->expression, values);
        if (dim1 == 0)
        {
            return 0;
        }
        ASSERT(dim == dim1);

        switch(unaryExpression->unaryOp)
        {
            case HLSLUnaryOp_Negative:
                for (int i = 0; i < dim; i++) values[i] = -values[i];
                return dim;
            case HLSLUnaryOp_Positive:
                // nop.
                return dim;
            default:
                return 0;
        }
    }
    else if (expression->nodeType == HLSLNodeType_ConstructorExpression)
    {
        HLSLConstructorExpression * constructor = (HLSLConstructorExpression *)expression;

        int dim = GetVectorDimension(constructor->expressionType);

        int idx = 0;
        HLSLExpression * arg = constructor->argument;
        while (arg != NULL)
        {
            float tmp[4];
            int n = GetExpressionValue(arg, tmp);
            for (int i = 0; i < n; i++) values[idx + i] = tmp[i];
            idx += n;

            arg = arg->nextExpression;
        }
        ASSERT(dim == idx);

        return dim;
    }
    else if (expression->nodeType == HLSLNodeType_IdentifierExpression)
    {
        HLSLIdentifierExpression * identifier = (HLSLIdentifierExpression *)expression;

        HLSLDeclaration * declaration = FindGlobalDeclaration(identifier->name);
        if (declaration == NULL) 
        {
            return 0;
        }
        if ((declaration->type.flags & HLSLTypeFlag_Const) == 0)
        {
            return 0;
        }

        return GetExpressionValue(declaration->assignment, values);
    }
    else if (expression->nodeType == HLSLNodeType_LiteralExpression)
    {
        HLSLLiteralExpression * literal = (HLSLLiteralExpression *)expression;

        if (literal->expressionType.baseType == HLSLBaseType_Float) values[0] = literal->fValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Half) values[0] = literal->fValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Bool) values[0] = literal->bValue;
        else if (literal->expressionType.baseType == HLSLBaseType_Int) values[0] = (float)literal->iValue;  // @@ Warn if conversion is not exact.
        else return 0;

        return 1;
    }

    return 0;
}




void HLSLTreeVisitor::VisitType(HLSLType & type)
{
}

void HLSLTreeVisitor::VisitRoot(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        VisitTopLevelStatement(statement);
        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitTopLevelStatement(HLSLStatement * node)
{
    if (node->nodeType == HLSLNodeType_Declaration) {
        VisitDeclaration((HLSLDeclaration *)node);
    }
    else if (node->nodeType == HLSLNodeType_Struct) {
        VisitStruct((HLSLStruct *)node);
    }
    else if (node->nodeType == HLSLNodeType_Buffer) {
        VisitBuffer((HLSLBuffer *)node);
    }
    else if (node->nodeType == HLSLNodeType_Function) {
        VisitFunction((HLSLFunction *)node);
    }
    else if (node->nodeType == HLSLNodeType_Technique) {
        VisitTechnique((HLSLTechnique *)node);
    }
    else {
        ASSERT(0);
    }
}

void HLSLTreeVisitor::VisitStatements(HLSLStatement * statement)
{
    while (statement != NULL) {
        VisitStatement(statement);
        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitStatement(HLSLStatement * node)
{
    // Function statements
    if (node->nodeType == HLSLNodeType_Declaration) {
        VisitDeclaration((HLSLDeclaration *)node);
    }
    else if (node->nodeType == HLSLNodeType_ExpressionStatement) {
        VisitExpressionStatement((HLSLExpressionStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ReturnStatement) {
        VisitReturnStatement((HLSLReturnStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_DiscardStatement) {
        VisitDiscardStatement((HLSLDiscardStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_BreakStatement) {
        VisitBreakStatement((HLSLBreakStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ContinueStatement) {
        VisitContinueStatement((HLSLContinueStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_IfStatement) {
        VisitIfStatement((HLSLIfStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_ForStatement) {
        VisitForStatement((HLSLForStatement *)node);
    }
    else if (node->nodeType == HLSLNodeType_BlockStatement) {
        VisitBlockStatement((HLSLBlockStatement *)node);
    }
    else {
        ASSERT(0);
    }
}

void HLSLTreeVisitor::VisitDeclaration(HLSLDeclaration * node)
{
    VisitType(node->type);
    /*do {
        VisitExpression(node->assignment);
        node = node->nextDeclaration;
    } while (node);*/
    if (node->assignment != NULL) {
        VisitExpression(node->assignment);
    }
    if (node->nextDeclaration != NULL) {
        VisitDeclaration(node->nextDeclaration);
    }
}

void HLSLTreeVisitor::VisitStruct(HLSLStruct * node)
{
    HLSLStructField * field = node->field;
    while (field != NULL) {
        VisitStructField(field);
        field = field->nextField;
    }
}

void HLSLTreeVisitor::VisitStructField(HLSLStructField * node)
{
    VisitType(node->type);
}

void HLSLTreeVisitor::VisitBuffer(HLSLBuffer * node)
{
    HLSLDeclaration * field = node->field;
    while (field != NULL) {
        ASSERT(field->nodeType == HLSLNodeType_Declaration);
        VisitDeclaration(field);
        ASSERT(field->nextDeclaration == NULL);
        field = (HLSLDeclaration *)field->nextStatement;
    }
}

/*void HLSLTreeVisitor::VisitBufferField(HLSLBufferField * node)
{
    VisitType(node->type);
}*/

void HLSLTreeVisitor::VisitFunction(HLSLFunction * node)
{
    VisitType(node->returnType);

    HLSLArgument * argument = node->argument;
    while (argument != NULL) {
        VisitArgument(argument);
        argument = argument->nextArgument;
    }

    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitArgument(HLSLArgument * node)
{
    VisitType(node->type);
    if (node->defaultValue != NULL) {
        VisitExpression(node->defaultValue);
    }
}

void HLSLTreeVisitor::VisitExpressionStatement(HLSLExpressionStatement * node)
{
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitExpression(HLSLExpression * node)
{
    VisitType(node->expressionType);

    if (node->nodeType == HLSLNodeType_UnaryExpression) {
        VisitUnaryExpression((HLSLUnaryExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_BinaryExpression) {
        VisitBinaryExpression((HLSLBinaryExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_ConditionalExpression) {
        VisitConditionalExpression((HLSLConditionalExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_CastingExpression) {
        VisitCastingExpression((HLSLCastingExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_LiteralExpression) {
        VisitLiteralExpression((HLSLLiteralExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_IdentifierExpression) {
        VisitIdentifierExpression((HLSLIdentifierExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_ConstructorExpression) {
        VisitConstructorExpression((HLSLConstructorExpression *)node);
    }
    else if (node->nodeType == HLSLNodeType_MemberAccess) {
        VisitMemberAccess((HLSLMemberAccess *)node);
    }
    else if (node->nodeType == HLSLNodeType_ArrayAccess) {
        VisitArrayAccess((HLSLArrayAccess *)node);
    }
    else if (node->nodeType == HLSLNodeType_FunctionCall) {
        VisitFunctionCall((HLSLFunctionCall *)node);
    }
    // Acoget-TODO: This was missing. Did adding it break anything?
    else if (node->nodeType == HLSLNodeType_SamplerState) {
        VisitSamplerState((HLSLSamplerState *)node);
    }
    else {
        ASSERT(0);
    }
}

void HLSLTreeVisitor::VisitReturnStatement(HLSLReturnStatement * node)
{
	if(node->expression)
		VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitDiscardStatement(HLSLDiscardStatement * node) {}
void HLSLTreeVisitor::VisitBreakStatement(HLSLBreakStatement * node) {}
void HLSLTreeVisitor::VisitContinueStatement(HLSLContinueStatement * node) {}

void HLSLTreeVisitor::VisitIfStatement(HLSLIfStatement * node)
{
    VisitExpression(node->condition);
    VisitStatements(node->statement);
    if (node->elseStatement) {
        VisitStatements(node->elseStatement);
    }
}

void HLSLTreeVisitor::VisitForStatement(HLSLForStatement * node)
{
    if (node->initialization) {
        VisitDeclaration(node->initialization);
    }
    if (node->condition) {
        VisitExpression(node->condition);
    }
    if (node->increment) {
        VisitExpression(node->increment);
    }
    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitBlockStatement(HLSLBlockStatement * node)
{
    VisitStatements(node->statement);
}

void HLSLTreeVisitor::VisitUnaryExpression(HLSLUnaryExpression * node)
{
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitBinaryExpression(HLSLBinaryExpression * node)
{
    VisitExpression(node->expression1);
    VisitExpression(node->expression2);
}

void HLSLTreeVisitor::VisitConditionalExpression(HLSLConditionalExpression * node)
{
    VisitExpression(node->condition);
    VisitExpression(node->falseExpression);
    VisitExpression(node->trueExpression);
}

void HLSLTreeVisitor::VisitCastingExpression(HLSLCastingExpression * node)
{
    VisitType(node->type);
    VisitExpression(node->expression);
}

void HLSLTreeVisitor::VisitLiteralExpression(HLSLLiteralExpression * node) {}
void HLSLTreeVisitor::VisitIdentifierExpression(HLSLIdentifierExpression * node) {}

void HLSLTreeVisitor::VisitConstructorExpression(HLSLConstructorExpression * node)
{
    HLSLExpression * argument = node->argument;
    while (argument != NULL) {
        VisitExpression(argument);
        argument = argument->nextExpression;
    }
}

void HLSLTreeVisitor::VisitMemberAccess(HLSLMemberAccess * node)
{
    VisitExpression(node->object);
}

void HLSLTreeVisitor::VisitArrayAccess(HLSLArrayAccess * node)
{
    VisitExpression(node->array);
    VisitExpression(node->index);
}

void HLSLTreeVisitor::VisitFunctionCall(HLSLFunctionCall * node)
{
    HLSLExpression * argument = node->argument;
    while (argument != NULL) {
        VisitExpression(argument);
        argument = argument->nextExpression;
    }
}

void HLSLTreeVisitor::VisitStateAssignment(HLSLStateAssignment * node) {}

void HLSLTreeVisitor::VisitSamplerState(HLSLSamplerState * node)
{
    HLSLStateAssignment * stateAssignment = node->stateAssignments;
    while (stateAssignment != NULL) {
        VisitStateAssignment(stateAssignment);
        stateAssignment = stateAssignment->nextStateAssignment;
    }
}

void HLSLTreeVisitor::VisitPass(HLSLPass * node)
{
    HLSLStateAssignment * stateAssignment = node->stateAssignments;
    while (stateAssignment != NULL) {
        VisitStateAssignment(stateAssignment);
        stateAssignment = stateAssignment->nextStateAssignment;
    }
}

void HLSLTreeVisitor::VisitTechnique(HLSLTechnique * node)
{
    HLSLPass * pass = node->passes;
    while (pass != NULL) {
        VisitPass(pass);
        pass = pass->nextPass;
    }
}

void HLSLTreeVisitor::VisitFunctions(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        if (statement->nodeType == HLSLNodeType_Function) {
            VisitFunction((HLSLFunction *)statement);
        }

        statement = statement->nextStatement;
    }
}

void HLSLTreeVisitor::VisitParameters(HLSLRoot * root)
{
    HLSLStatement * statement = root->statement;
    while (statement != NULL) {
        if (statement->nodeType == HLSLNodeType_Declaration) {
            VisitDeclaration((HLSLDeclaration *)statement);
        }

        statement = statement->nextStatement;
    }
}


class ResetHiddenFlagVisitor : public HLSLTreeVisitor
{
public:
    virtual void VisitTopLevelStatement(HLSLStatement * statement)
    {
        statement->hidden = true;

        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            VisitBuffer((HLSLBuffer*)statement);
        }
    }

    // Hide buffer fields.
    virtual void VisitDeclaration(HLSLDeclaration * node)
    {
        node->hidden = true;
    }

    virtual void VisitArgument(HLSLArgument * node)
    {
        node->hidden = false;   // Arguments are visible by default.
    }
};

class MarkVisibleStatementsVisitor : public HLSLTreeVisitor
{
public:
    HLSLTree * tree;
    MarkVisibleStatementsVisitor(HLSLTree * tree) : tree(tree) {}

    virtual void VisitFunction(HLSLFunction * node)
    {
        node->hidden = false;
        HLSLTreeVisitor::VisitFunction(node);

        if (node->forward)
            VisitFunction(node->forward);
    }

    virtual void VisitFunctionCall(HLSLFunctionCall * node)
    {
        HLSLTreeVisitor::VisitFunctionCall(node);

        if (node->function->hidden)
        {
            VisitFunction(const_cast<HLSLFunction*>(node->function));
        }
    }

    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node)
    {
        HLSLTreeVisitor::VisitIdentifierExpression(node);

        if (node->global)
        {
            HLSLDeclaration * declaration = tree->FindGlobalDeclaration(node->name);
            if (declaration != NULL && declaration->hidden)
            {
                declaration->hidden = false;
                VisitDeclaration(declaration);
            }
        }
    }

    virtual void VisitType(HLSLType & type)
    {
        if (type.baseType == HLSLBaseType_UserDefined)
        {
            HLSLStruct * globalStruct = tree->FindGlobalStruct(type.typeName);
            if (globalStruct != NULL)
            {
                globalStruct->hidden = false;
                VisitStruct(globalStruct);
            }
        }
    }

};


void PruneTree(HLSLTree* tree, const char* entryName0, const char* entryName1/*=NULL*/)
{
    HLSLRoot* root = tree->GetRoot();

    // Reset all flags.
    ResetHiddenFlagVisitor reset;
    reset.VisitRoot(root);

    // Mark all the statements necessary for these entrypoints.
    HLSLFunction* entry = tree->FindFunction(entryName0);
    if (entry != NULL)
    {
        MarkVisibleStatementsVisitor mark(tree);
        mark.VisitFunction(entry);
    }

    if (entryName1 != NULL)
    {
        entry = tree->FindFunction(entryName1);
        if (entry != NULL)
        {
            MarkVisibleStatementsVisitor mark(tree);
            mark.VisitFunction(entry);
        }
    }

    // Mark buffers visible, if any of their fields is visible.
    HLSLStatement * statement = root->statement;
    while (statement != NULL)
    {
        if (statement->nodeType == HLSLNodeType_Buffer)
        {
            HLSLBuffer* buffer = (HLSLBuffer*)statement;

            HLSLDeclaration* field = buffer->field;
            while (field != NULL)
            {
                ASSERT(field->nodeType == HLSLNodeType_Declaration);
                if (!field->hidden)
                {
                    buffer->hidden = false;
                    break;
                }
                field = (HLSLDeclaration*)field->nextStatement;
            }
        }

        statement = statement->nextStatement;
    }
}


void SortTree(HLSLTree * tree)
{
    // Stable sort so that statements are in this order:
    // structs, declarations, functions, techniques.
	// but their relative order is preserved.

    HLSLRoot* root = tree->GetRoot();

    HLSLStatement* structs = NULL;
    HLSLStatement* lastStruct = NULL;
    HLSLStatement* constDeclarations = NULL;
    HLSLStatement* lastConstDeclaration = NULL;
    HLSLStatement* declarations = NULL;
    HLSLStatement* lastDeclaration = NULL;
    HLSLStatement* functions = NULL;
    HLSLStatement* lastFunction = NULL;
    HLSLStatement* other = NULL;
    HLSLStatement* lastOther = NULL;

    HLSLStatement* statement = root->statement;
    while (statement != NULL) {
        HLSLStatement* nextStatement = statement->nextStatement;
        statement->nextStatement = NULL;

        if (statement->nodeType == HLSLNodeType_Struct) {
            if (structs == NULL) structs = statement;
            if (lastStruct != NULL) lastStruct->nextStatement = statement;
            lastStruct = statement;
        }
        else if (statement->nodeType == HLSLNodeType_Declaration || statement->nodeType == HLSLNodeType_Buffer) {
            if (statement->nodeType == HLSLNodeType_Declaration && (((HLSLDeclaration *)statement)->type.flags & HLSLTypeFlag_Const)) {
                if (constDeclarations == NULL) constDeclarations = statement;
                if (lastConstDeclaration != NULL) lastConstDeclaration->nextStatement = statement;
                lastConstDeclaration = statement;
            }
            else {
                if (declarations == NULL) declarations = statement;
                if (lastDeclaration != NULL) lastDeclaration->nextStatement = statement;
                lastDeclaration = statement;
            }
        }
        else if (statement->nodeType == HLSLNodeType_Function) {
            if (functions == NULL) functions = statement;
            if (lastFunction != NULL) lastFunction->nextStatement = statement;
            lastFunction = statement;
        }
        else {
            if (other == NULL) other = statement;
            if (lastOther != NULL) lastOther->nextStatement = statement;
            lastOther = statement;
        }

        statement = nextStatement;
    }

    // Chain all the statements in the order that we want.
    HLSLStatement * firstStatement = structs;
    HLSLStatement * lastStatement = lastStruct;

    if (constDeclarations != NULL) {
        if (firstStatement == NULL) firstStatement = constDeclarations;
        else lastStatement->nextStatement = constDeclarations;
        lastStatement = lastConstDeclaration;
    }

    if (declarations != NULL) {
        if (firstStatement == NULL) firstStatement = declarations;
        else lastStatement->nextStatement = declarations;
        lastStatement = lastDeclaration;
    }

    if (functions != NULL) {
        if (firstStatement == NULL) firstStatement = functions;
        else lastStatement->nextStatement = functions;
        lastStatement = lastFunction;
    }

    if (other != NULL) {
        if (firstStatement == NULL) firstStatement = other;
        else lastStatement->nextStatement = other;
        lastStatement = lastOther;
    }

    root->statement = firstStatement;
}





// First and last can be the same.
void AddStatements(HLSLRoot * root, HLSLStatement * before, HLSLStatement * first, HLSLStatement * last)
{
    if (before == NULL) {
        last->nextStatement = root->statement;
        root->statement = first;
    }
    else {
        last->nextStatement = before->nextStatement;
        before->nextStatement = first;
    }
}

void AddSingleStatement(HLSLRoot * root, HLSLStatement * before, HLSLStatement * statement)
{
    AddStatements(root, before, statement, statement);
}



// @@ This is very game-specific. Should be moved to pipeline_parser or somewhere else.
void GroupParameters(HLSLTree * tree)
{
    // Sort parameters based on semantic and group them in cbuffers.

    HLSLRoot* root = tree->GetRoot();

    HLSLDeclaration * firstPerItemDeclaration = NULL;
    HLSLDeclaration * lastPerItemDeclaration = NULL;

    HLSLDeclaration * instanceDataDeclaration = NULL;

    HLSLDeclaration * firstPerPassDeclaration = NULL;
    HLSLDeclaration * lastPerPassDeclaration = NULL;

    HLSLDeclaration * firstPerItemSampler = NULL;
    HLSLDeclaration * lastPerItemSampler = NULL;

    HLSLDeclaration * firstPerPassSampler = NULL;
    HLSLDeclaration * lastPerPassSampler = NULL;

    HLSLStatement * statementBeforeBuffers = NULL;
    
    HLSLStatement* previousStatement = NULL;
    HLSLStatement* statement = root->statement;
    while (statement != NULL)
    {
        HLSLStatement* nextStatement = statement->nextStatement;

        if (statement->nodeType == HLSLNodeType_Struct) // Do not remove this, or it will mess the else clause below.
        {   
            statementBeforeBuffers = statement;
        }
        else if (statement->nodeType == HLSLNodeType_Declaration)
        {
            HLSLDeclaration* declaration = (HLSLDeclaration*)statement;

            // We insert buffers after the last const declaration.
            if ((declaration->type.flags & HLSLTypeFlag_Const) != 0)
            {
                statementBeforeBuffers = statement;
            }

            // Do not move samplers or static/const parameters.
            if ((declaration->type.flags & (HLSLTypeFlag_Static|HLSLTypeFlag_Const)) == 0)
            {
                // Unlink statement.
                statement->nextStatement = NULL;
                if (previousStatement != NULL) previousStatement->nextStatement = nextStatement;
                else root->statement = nextStatement;

                while(declaration != NULL)
                {
                    HLSLDeclaration* nextDeclaration = declaration->nextDeclaration;

                    if (declaration->semantic != NULL && String_EqualNoCase(declaration->semantic, "PER_INSTANCED_ITEM"))
                    {
                        ASSERT(instanceDataDeclaration == NULL);
                        instanceDataDeclaration = declaration;
                    }
                    else
                    {
                        // Select group based on type and semantic.
                        HLSLDeclaration ** first, ** last;
                        if (declaration->semantic == NULL || String_EqualNoCase(declaration->semantic, "PER_ITEM") || String_EqualNoCase(declaration->semantic, "PER_MATERIAL"))
                        {
                            if (IsSamplerType(declaration->type))
                            {
                                first = &firstPerItemSampler;
                                last = &lastPerItemSampler;
                            }
                            else
                            {
                                first = &firstPerItemDeclaration;
                                last = &lastPerItemDeclaration;
                            }
                        }
                        else
                        {
                            if (IsSamplerType(declaration->type))
                            {
                                first = &firstPerPassSampler;
                                last = &lastPerPassSampler;
                            }
                            else
                            {
                                first = &firstPerPassDeclaration;
                                last = &lastPerPassDeclaration;
                            }
                        }

                        // Add declaration to new list.
                        if (*first == NULL) *first = declaration;
                        else (*last)->nextStatement = declaration;
                        *last = declaration;
                    }

                    // Unlink from declaration list.
                    declaration->nextDeclaration = NULL;

                    // Reset attributes.
                    declaration->registerName = NULL;
                    //declaration->semantic = NULL;         // @@ Don't do this!

                    declaration = nextDeclaration;
                }
            }
        }
        /*else
        {
            if (statementBeforeBuffers == NULL) {
                // This is the location where we will insert our buffers.
                statementBeforeBuffers = previousStatement;
            }
        }*/

        if (statement->nextStatement == nextStatement) {
            previousStatement = statement;
        }
        statement = nextStatement;
    }


    // Add instance data declaration at the end of the per_item buffer.
    if (instanceDataDeclaration != NULL)
    {
        if (firstPerItemDeclaration == NULL) firstPerItemDeclaration = instanceDataDeclaration;
        else lastPerItemDeclaration->nextStatement = instanceDataDeclaration;
    }


    // Add samplers.
    if (firstPerItemSampler != NULL) {
        AddStatements(root, statementBeforeBuffers, firstPerItemSampler, lastPerItemSampler);
        statementBeforeBuffers = lastPerItemSampler;
    }
    if (firstPerPassSampler != NULL) {
        AddStatements(root, statementBeforeBuffers, firstPerPassSampler, lastPerPassSampler);
        statementBeforeBuffers = lastPerPassSampler;
    }


    // @@ We are assuming per_item and per_pass buffers don't already exist. @@ We should assert on that.

    if (firstPerItemDeclaration != NULL)
    {
        // Create buffer statement.
        HLSLBuffer * perItemBuffer = tree->AddNode<HLSLBuffer>(firstPerItemDeclaration->fileName, firstPerItemDeclaration->line-1);
        perItemBuffer->name = tree->AddString("per_item");
        perItemBuffer->registerName = tree->AddString("b0");
        perItemBuffer->field = firstPerItemDeclaration;
        
        // Set declaration buffer pointers.
        HLSLDeclaration * field = perItemBuffer->field;
        while (field != NULL)
        {
            field->buffer = perItemBuffer;
            field = (HLSLDeclaration *)field->nextStatement;
        }

        // Add buffer to statements.
        AddSingleStatement(root, statementBeforeBuffers, perItemBuffer);
        statementBeforeBuffers = perItemBuffer;
    }

    if (firstPerPassDeclaration != NULL)
    {
        // Create buffer statement.
        HLSLBuffer * perPassBuffer = tree->AddNode<HLSLBuffer>(firstPerPassDeclaration->fileName, firstPerPassDeclaration->line-1);
        perPassBuffer->name = tree->AddString("per_pass");
        perPassBuffer->registerName = tree->AddString("b1");
        perPassBuffer->field = firstPerPassDeclaration;

        // Set declaration buffer pointers.
        HLSLDeclaration * field = perPassBuffer->field;
        while (field != NULL)
        {
            field->buffer = perPassBuffer;
            field = (HLSLDeclaration *)field->nextStatement;
        }
        
        // Add buffer to statements.
        AddSingleStatement(root, statementBeforeBuffers, perPassBuffer);
    }
}


class FindArgumentVisitor : public HLSLTreeVisitor
{
public:
    bool found;
    const char * name;

	FindArgumentVisitor()
	{
		found = false;
		name  = NULL;
	}

    bool FindArgument(const char * name, HLSLFunction * function)
    {
        this->found = false;
        this->name = name;
        VisitStatements(function->statement);
        return found;
    }

    virtual void VisitStatements(HLSLStatement * statement) override
    {
        while (statement != NULL && !found)
        {
            VisitStatement(statement);
            statement = statement->nextStatement;
        }
    }

    virtual void VisitIdentifierExpression(HLSLIdentifierExpression * node) override
    {
        if (node->name == name)
        {
            found = true;
        }
    }
};


void HideUnusedArguments(HLSLFunction * function)
{
    FindArgumentVisitor visitor;
 
    // For each argument.
    HLSLArgument * arg = function->argument;
    while (arg != NULL)
    {
        if (!visitor.FindArgument(arg->name, function))
        {
            arg->hidden = true;
        }

        arg = arg->nextArgument;
    }
}

bool EmulateAlphaTest(HLSLTree* tree, const char* entryName, float alphaRef/*=0.5*/)
{
    // Find all return statements of this entry point.
    HLSLFunction* entry = tree->FindFunction(entryName);
    if (entry != NULL)
    {
        HLSLStatement ** ptr = &entry->statement;
        HLSLStatement * statement = entry->statement;
        while (statement != NULL)
        {
            if (statement->nodeType == HLSLNodeType_ReturnStatement)
            {
                HLSLReturnStatement * returnStatement = (HLSLReturnStatement *)statement;
                HLSLBaseType returnType = returnStatement->expression->expressionType.baseType;
                
                // Build statement: "if (%s.a < 0.5) discard;"

                HLSLDiscardStatement * discard = tree->AddNode<HLSLDiscardStatement>(statement->fileName, statement->line);
                
                HLSLExpression * alpha = NULL;
                if (returnType == HLSLBaseType_Float4 || returnType == HLSLBaseType_Half4)
                {
                    // @@ If return expression is a constructor, grab 4th argument.
                    // That's not as easy, since we support 'float4(float3, float)' or 'float4(float, float3)', extracting
                    // the latter is not that easy.
                    /*if (returnStatement->expression->nodeType == HLSLNodeType_ConstructorExpression) {
                        HLSLConstructorExpression * constructor = (HLSLConstructorExpression *)returnStatement->expression;
                        //constructor->
                    }
                    */
                    
                    if (alpha == NULL) {
                        HLSLMemberAccess * access = tree->AddNode<HLSLMemberAccess>(statement->fileName, statement->line);
                        access->expressionType = HLSLType(HLSLBaseType_Float);
                        access->object = returnStatement->expression;     // @@ Is reference OK? Or should we clone expression?
                        access->field = tree->AddString("a");
                        access->swizzle = true;
                        
                        alpha = access;
                    }
                }
                else if (returnType == HLSLBaseType_Float || returnType == HLSLBaseType_Half)
                {
                    alpha = returnStatement->expression;     // @@ Is reference OK? Or should we clone expression?
                }
                else {
                    return false;
                }
                
                HLSLLiteralExpression * threshold = tree->AddNode<HLSLLiteralExpression>(statement->fileName, statement->line);
                threshold->expressionType = HLSLType(HLSLBaseType_Float);
                threshold->fValue = alphaRef;
                threshold->type = HLSLBaseType_Float;
                
                HLSLBinaryExpression * condition = tree->AddNode<HLSLBinaryExpression>(statement->fileName, statement->line);
                condition->expressionType = HLSLType(HLSLBaseType_Bool);
                condition->binaryOp = HLSLBinaryOp_Less;
                condition->expression1 = alpha;
                condition->expression2 = threshold;

                // Insert statement.
                HLSLIfStatement * st = tree->AddNode<HLSLIfStatement>(statement->fileName, statement->line);
                st->condition = condition;
                st->statement = discard;
                st->nextStatement = statement;
                *ptr = st;
            }
        
            ptr = &statement->nextStatement;
            statement = statement->nextStatement;
        }
    }

    return true;
}

