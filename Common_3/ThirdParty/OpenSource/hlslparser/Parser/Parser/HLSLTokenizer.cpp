#include "Engine.h"

#include "HLSLTokenizer.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

static bool GetIsSymbol(char c)
{
    switch (c)
    {
    case ';':
    case ':':
    case '(': case ')':
    case '[': case ']':
    case '{': case '}':
    case '-': case '+':
    case '*': case '/':
	//case '%':
    case '?':
    case '!':
    case ',':
    case '=':
    case '.':
    case '<': case '>':
    case '|': case '&': case '^': case '~':
        return true;
    }
    return false;
}

/** Returns true if the character is a valid token separator at the end of a number type token */
static bool GetIsNumberSeparator(char c)
{
    return c == 0 || isspace(c) || GetIsSymbol(c);
}

HLSLTokenizer::HLSLTokenizer(const char* fileName, const char* buffer, size_t length)
{
    m_buffer            = buffer;
    m_bufferEnd         = buffer + length;
    m_fileName          = fileName;
    m_lineNumber        = 1;
    m_tokenLineNumber   = 1;
    m_error             = false;
	m_historyCounter = 0;
	
    Next();
}

void HLSLTokenizer::Undo()
{
	m_buffer = m_prevBuffer;
	m_token = m_PrevToken;
}

void HLSLTokenizer::Next()
{
	m_prevBuffer = m_buffer;
	m_PrevToken = m_token;

	while( SkipWhitespace() || SkipComment() || ScanLineDirective() || SkipPragmaDirective() /*|| StorePreprocessor()*/)
    {

    }

    if (m_error)
    {
        m_token = HLSLToken_EndOfStream;
        return;
    }

    m_tokenLineNumber = m_lineNumber;

	if (m_buffer >= m_bufferEnd || *m_buffer == '\0')
    {
        m_token = HLSLToken_EndOfStream;
        return;
    }

    const char* start = m_buffer;
    // +=, -=, *=, /=, ==, <=, >=, <<, >>
    if (m_buffer[0] == '+' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_PlusEqual;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '-' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_MinusEqual;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '*' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_TimesEqual;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '/' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_DivideEqual;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '=' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_EqualEqual;
        m_buffer += 2;
        return;
    }

	else if (m_buffer[0] == '&' && m_buffer[1] == '=')
	{
		m_token = HLSLToken_AndEqual;
		m_buffer += 2;
		return;
	}
	else if (m_buffer[0] == '|' && m_buffer[1] == '=')
	{
		m_token = HLSLToken_BarEqual;
		m_buffer += 2;
		return;
	}
	else if (m_buffer[0] == '^' && m_buffer[1] == '=')
	{
		m_token = HLSLToken_XorEqual;
		m_buffer += 2;
		return;
	}

	

    else if (m_buffer[0] == '!' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_NotEqual;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '<' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_LessEqual;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '>' && m_buffer[1] == '=')
    {
        m_token = HLSLToken_GreaterEqual;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '&' && m_buffer[1] == '&')
    {
        m_token = HLSLToken_AndAnd;
        m_buffer += 2;
        return;
    }
    else if (m_buffer[0] == '|' && m_buffer[1] == '|')
    {
        m_token = HLSLToken_BarBar;
        m_buffer += 2;
        return;
    }
	else if (m_buffer[0] == '<' && m_buffer[1] == '<')
	{
		m_token = HLSLToken_LeftShift;
		m_buffer += 2;
		return;
	}
	else if (m_buffer[0] == '>' && m_buffer[1] == '>')
	{
		m_token = HLSLToken_RightShift;
		m_buffer += 2;
		return;
	}
	else if (m_buffer[0] == '%')
	{
		m_token = HLSLToken_Modular;
		m_buffer += 1;
		return;
	}

    // ++, --
    if ((m_buffer[0] == '-' || m_buffer[0] == '+') && (m_buffer[1] == m_buffer[0]))
    {
        m_token = (m_buffer[0] == '+') ? HLSLToken_PlusPlus : HLSLToken_MinusMinus;
        m_buffer += 2;
        return;
    }

    // Check for the start of a number.
    if (ScanNumber())
    {
		size_t length = 1;
		memcpy(m_identifier, "", length);
		m_identifier[length] = 0;

        return;
    }
    
    if (GetIsSymbol(m_buffer[0]))
    {
        m_token = static_cast<unsigned char>(m_buffer[0]);
        ++m_buffer;
        return;
    }

    // Must be an identifier or a reserved word.
    while (m_buffer < m_bufferEnd && m_buffer[0] != 0 && !GetIsSymbol(m_buffer[0]) && !isspace(m_buffer[0]))
    {
        ++m_buffer;
    }	

    size_t length = m_buffer - start;
    memcpy(m_identifier, start, length);
    m_identifier[length] = 0;

	if (m_historyCounter < s_maxHistoryIdentifier)
	{
		strcpy(m_identifierHistory[m_historyCounter], m_identifier);
		m_identifierHistoryAddress[m_historyCounter++] = start;
	}
	else
	{
		// if it is full, shift
		for (int i = 0; i < s_maxIdentifier - 1; i++)
		{
			strcpy(m_identifierHistory[i], m_identifierHistory[i + 1]);
			m_identifierHistoryAddress[i] = m_identifierHistoryAddress[i + 1];
		}

		strcpy(m_identifierHistory[s_maxIdentifier - 1], m_identifier);
		m_identifierHistoryAddress[s_maxIdentifier - 1] = start;
	}



    const int numReservedWords = sizeof(_reservedWords) / sizeof(const char*);
    for (int i = 0; i < numReservedWords; ++i)
    {
        if (String_Equal(_reservedWords[i], m_identifier))
        {
			if (String_Equal("else", m_identifier))
			{
				//else if
				if (m_buffer[0] == ' ' && m_buffer[1] == 'i' && m_buffer[2] == 'f')
				{
					m_buffer += 3;
					m_token = 256 + i - 1;
					return;
				}
			}	
			
			m_token = 256 + i;
			return;
           
        }
    }

    m_token = HLSLToken_Identifier;

}

bool HLSLTokenizer::GetRestofWholeline(char* strBuffer)
{
	bool result = false;
	int counter = 0;
	while (m_buffer < m_bufferEnd)
	{
		result = true;
		strBuffer[counter] = m_buffer[0];

		if (m_buffer[0] == '\n')
		{		
			strBuffer[counter] = 0;
			++m_lineNumber;
			++m_buffer;
			break;
		}
		++m_buffer;
		counter++;
	}
	return result;
}

bool HLSLTokenizer::GetRestofWholelineWOSpace(char* strBuffer)
{
	bool result = false;
	int counter = 0;
	while (m_buffer < m_bufferEnd)
	{
		result = true;

		if (m_buffer[0] == ' ' || m_buffer[0] == '\t')
		{
			//skip
			counter--;
		}
		else
			strBuffer[counter] = m_buffer[0];

		if (m_buffer[0] == '\n')
		{
			strBuffer[counter] = 0;
			++m_lineNumber;
			++m_buffer;
			break;
		}
		++m_buffer;
		counter++;
	}
	return result;
}


bool HLSLTokenizer::SkipWhitespace()
{
    bool result = false;
    while (m_buffer < m_bufferEnd && isspace(m_buffer[0]))
    {
        result = true;
        if (m_buffer[0] == '\n')
        {
            ++m_lineNumber;
        }
        ++m_buffer;
    }
    return result;
}

bool HLSLTokenizer::SkipComment()
{
    bool result = false;
    if (m_buffer[0] == '/')
    {
        if (m_buffer[1] == '/' && strncmp(m_buffer + 2, " USERMACRO", 10) != 0 )
        {
            // Single line comment.
            result = true;
            m_buffer += 2;
            while (m_buffer < m_bufferEnd)
            {
                if (*(m_buffer++) == '\n')
                {
                    ++m_lineNumber;
                    break;
                }
            }
        }
        else if (m_buffer[1] == '*')
        {
            // Multi-line comment.
            result = true;
            m_buffer += 2;
            while (m_buffer < m_bufferEnd)
            {
                if (m_buffer[0] == '\n')
                {
                    ++m_lineNumber;
                }
                if (m_buffer[0] == '*' && m_buffer[1] == '/')
                {
                    break;
                }
                ++m_buffer;
            }
            if (m_buffer < m_bufferEnd)
            {
                m_buffer += 2;
            }
        }
    }
    return result;
}

bool HLSLTokenizer::SkipPragmaDirective()
{
	bool result = false;
	if( m_bufferEnd - m_buffer > 7 && *m_buffer == '#' )
	{
		const char* ptr = m_buffer + 1;
		while( isspace( *ptr ) )
			ptr++;

		if( strncmp( ptr, "pragma", 6 ) == 0 && isspace( ptr[ 6 ] ) )
		{
			m_buffer = ptr + 6;
			result = true;
			while( m_buffer < m_bufferEnd )
			{
				if( *( m_buffer++ ) == '\n' )
				{
					++m_lineNumber;
					break;
				}
			}
		}
	}
	return result;
}

bool HLSLTokenizer::ScanNumber()
{
	m_fValue = 0.0;
	m_uiValue = 0;
	m_iValue = 0;

    // Don't treat the + or - as part of the number.
    if (m_buffer[0] == '+' || m_buffer[0] == '-')
    {
        return false;
    }

    // Parse hex literals.
	if (m_bufferEnd - m_buffer > 2 && m_buffer[0] == '0' && m_buffer[1] == 'x')
    {
		char*   hEnd = NULL;
		//temporarily parse to Uint
		char uint[256];
		uint[255] = NULL;

		uint[0] = 'r';
		uint[1] = 'e';
		uint[2] = 'a';
		uint[3] = 'd';
		uint[4] = '1';
		uint[5] = '6';
		uint[6] = ' ';

		strncpy(&uint[7], &m_buffer[2], 127);

		int endCounter = 0;
		for (int i = 7; i < 127; i++)
		{
			if (!((uint[i] >= '0' &&  uint[i] <= '9') || (uint[i] >= 'A' &&  uint[i] <= 'F') || (uint[i] >= 'a' &&  uint[i] <= 'f')) )
			{
				if(uint[i] == 'u')
					endCounter++;
				else if(uint[i] == 'U' && uint[i + 1] == 'L')
					endCounter += 2;
				else if(uint[i] == 'U')
					endCounter++;

				uint[i] = 0;
				break;
			}

			endCounter++;
		}
		
		unsigned uiValue;
		sscanf(uint, "%*s %8x", &uiValue);

		memset(uint, NULL, 256);
				
		m_buffer += (2 + endCounter);
		m_uiValue = uiValue;
		m_token = HLSLToken_UintLiteral;

		return true;
		

		/*        
        int     iValue = strtol(m_buffer+2, &hEnd, 16);
        if (GetIsNumberSeparator(hEnd[0]))
        {
            m_buffer = hEnd;
            m_token  = HLSLToken_IntLiteral;
            m_iValue = iValue;
            return true;
        }
		*/
    }

    char* fEnd = NULL;
    double fValue = String_ToDouble(m_buffer, &fEnd);
	
	char dString[256] = {};

	for (int i = 0; &m_buffer[i] != fEnd; i++)
	{
		dString[i] = m_buffer[i];
	}
	

    if (fEnd == m_buffer)
    {
        return false;
    }

    char*  iEnd = NULL;
    int    iValue = String_ToInteger(m_buffer, &iEnd);

	char uintStr[256];
	uintStr[127] = NULL;

	strncpy(uintStr, m_buffer, 128);

	for (int i = 0; i < 127; i++)
	{
		if (!(uintStr[i] >= '0' &&  uintStr[i] <= '9'))
		{
			uintStr[i] = 0;
			break;
		}

	}

	

	unsigned int uiValue = String_ToUInteger(uintStr, &iEnd);

	//  If the character has e, it is double
	if (strstr(dString, "e") != NULL && fEnd > iEnd && GetIsNumberSeparator(fEnd[0]))
	{
		m_buffer = fEnd;
		m_token = HLSLToken_FloatLiteral;
		m_fValue = static_cast<float>(fValue);
		return true;
	}
	else
	{
		// If the character after the number is an f then the f is treated as part
		// of the number (to handle 1.0f syntax).
		if ((fEnd[0] == 'f' || fEnd[0] == 'F' || fEnd[0] == 'h' || fEnd[0] == 'H') && fEnd < m_bufferEnd)
		{
			++fEnd;
		}

		if (fEnd > iEnd && GetIsNumberSeparator(fEnd[0]))
		{
			m_buffer = fEnd;
			m_token = (fEnd[-1] == 'f' || fEnd[-1] == 'F')? HLSLToken_FloatLiteral : HLSLToken_HalfLiteral;
			m_fValue = static_cast<float>(fValue);
			return true;
		}
	}

    

	if ((iEnd[0] == 'u' || fEnd[0] == 'i') && iEnd < m_bufferEnd)
	{
		++iEnd;
	}
	else if ((iEnd[0] == 'U' && iEnd[1] == 'L') && iEnd < m_bufferEnd)
	{
		iEnd += 2;
	}
	else if (iEnd[0] == 'U' && iEnd < m_bufferEnd)
	{
		++iEnd;
	}

    if (iEnd > m_buffer && GetIsNumberSeparator(iEnd[0]))
    {
        m_buffer = iEnd;
        
		if (uiValue == (unsigned int)iValue)
			m_token = HLSLToken_IntLiteral;
		else
			m_token = HLSLToken_UintLiteral;
				
		//m_token = iEnd[-1] == 'u' ? HLSLToken_UintLiteral : HLSLToken_IntLiteral;
		m_uiValue = uiValue;
		m_iValue = iValue;
        return true;
    }

    return false;
}

bool HLSLTokenizer::ScanLineDirective()
{
    
    if (m_bufferEnd - m_buffer > 5 && strncmp(m_buffer, "#line", 5) == 0 && isspace(m_buffer[5]))
    {

        m_buffer += 5;
        
        while (m_buffer < m_bufferEnd && isspace(m_buffer[0]))
        {
            if (m_buffer[0] == '\n')
            {
                Error("Syntax error: expected line number after #line");
                return false;
            }
            ++m_buffer;
        }

        char* iEnd = NULL;
        int lineNumber = String_ToInteger(m_buffer, &iEnd);

        if (!isspace(*iEnd))
        {
            Error("Syntax error: expected line number after #line");
            return false;
        }

        m_buffer = iEnd;
        while (m_buffer < m_bufferEnd && isspace(m_buffer[0]))
        {
            char c = m_buffer[0];
            ++m_buffer;
            if (c == '\n')
            {
                m_lineNumber = lineNumber;
                return true;
            }
        }

        if (m_buffer >= m_bufferEnd)
        {
            m_lineNumber = lineNumber;
            return true;
        }

        if (m_buffer[0] != '"')
        {
            Error("Syntax error: expected '\"' after line number near #line");
            return false;
        }
            
        ++m_buffer;
        
        int i = 0;
        while (i + 1 < s_maxIdentifier && m_buffer < m_bufferEnd && m_buffer[0] != '"')
        {
            if (m_buffer[0] == '\n')
            {
                Error("Syntax error: expected '\"' before end of line near #line");
                return false;
            }

            m_lineDirectiveFileName[i] = *m_buffer;
            ++m_buffer;
            ++i;
        }
        
        m_lineDirectiveFileName[i] = 0;
        
        if (m_buffer >= m_bufferEnd)
        {
            Error("Syntax error: expected '\"' before end of file near #line");
            return false;
        }

        if (i + 1 >= s_maxIdentifier)
        {
            Error("Syntax error: file name too long near #line");
            return false;
        }

        // Skip the closing quote
        ++m_buffer;
        
        while (m_buffer < m_bufferEnd && m_buffer[0] != '\n')
        {
            if (!isspace(m_buffer[0]))
            {
                Error("Syntax error: unexpected input after file name near #line");
                return false;
            }
            ++m_buffer;
        }

        // Skip new line
        ++m_buffer;

        m_lineNumber = lineNumber;
        m_fileName = m_lineDirectiveFileName;

        return true;
    }

    return false;
}

const char* HLSLTokenizer::GetBufferAddress() const
{
	return m_buffer;
}

const char* HLSLTokenizer::GetIndentifierHistoryAddress(int index) const
{
	return m_identifierHistoryAddress[index];
}

int HLSLTokenizer::GetToken() const
{
    return m_token;
}

float HLSLTokenizer::GetFloat() const
{
    return m_fValue;
}

int HLSLTokenizer::GetInt() const
{
    return m_iValue;
}

unsigned int HLSLTokenizer::GetuInt() const
{
	return m_uiValue;
}

const char* HLSLTokenizer::GetPrevIdentifier(int index) const
{
	return m_identifierHistory[index];
}

int HLSLTokenizer::GetHistoryCounter() const
{
	return m_historyCounter;
}

const char* HLSLTokenizer::GetIdentifier() const
{
    return m_identifier;
}

int HLSLTokenizer::GetLineNumber() const
{
    return m_tokenLineNumber;
}

const char* HLSLTokenizer::GetFileName() const
{
    return m_fileName;
}

void HLSLTokenizer::Error(const char* format, ...)
{
    // It's not always convenient to stop executing when an error occurs,
    // so just track once we've hit an error and stop reporting them until
    // we successfully bail out of execution.
    if (m_error)
    {
        return;
    }
    m_error = true;

	char buffer[1024];

    va_list args;
    va_start(args, format);
    int result = vsnprintf(buffer, sizeof(buffer) - 1, format, args);
    va_end(args);

	char it[64];

	strcpy(errorBuffer, "error) FileName \t: ");
	strcat(errorBuffer, m_fileName);
	strcat(errorBuffer, "\n");

	strcat(errorBuffer, "error) Line (");
	strcat(errorBuffer, _itoa(m_lineNumber, it, 10));
	strcat(errorBuffer, ")\t: ");
	strcat(errorBuffer, buffer);
	strcat(errorBuffer, "\n");

	std::cout << errorBuffer << std::endl;

    Log_Error("%s\n", buffer);

	

} 

void HLSLTokenizer::GetTokenName(char buffer[s_maxIdentifier]) const
{
    if (m_token == HLSLToken_FloatLiteral || m_token == HLSLToken_HalfLiteral )
    {
        sprintf(buffer, "%f", m_fValue);
    }
    else if (m_token == HLSLToken_IntLiteral)
    {
        sprintf(buffer, "%d", m_iValue);
    }
	else if (m_token == HLSLToken_UintLiteral)
	{
		sprintf(buffer, "%u", m_uiValue);
	}
    else if (m_token == HLSLToken_Identifier)
    {
        strcpy(buffer, m_identifier);
    }
    else
    {
        GetTokenName(m_token, buffer);
    }
}

void HLSLTokenizer::GetTokenName(int token, char buffer[s_maxIdentifier])
{
    if (token < 256)
    {
        buffer[0] = (char)token;
        buffer[1] = 0;
    }
    else if (token < HLSLToken_LessEqual)
    {
        strcpy(buffer, _reservedWords[token - 256]);
    }
    else
    {
        switch (token)
        {
        case HLSLToken_PlusPlus:
            strcpy(buffer, "++");
            break;
        case HLSLToken_MinusMinus:
            strcpy(buffer, "--");
            break;
        case HLSLToken_PlusEqual:
            strcpy(buffer, "+=");
            break;
        case HLSLToken_MinusEqual:
            strcpy(buffer, "-=");
            break;
        case HLSLToken_TimesEqual:
            strcpy(buffer, "*=");
            break;
        case HLSLToken_DivideEqual:
            strcpy(buffer, "/=");
            break;

		case HLSLToken_AndEqual:
			strcpy(buffer, "&=");
			break;
		case HLSLToken_BarEqual:
			strcpy(buffer, "|=");
			break;
		case HLSLToken_XorEqual:
			strcpy(buffer, "^=");
			break;

		case HLSLToken_HalfLiteral:
			strcpy( buffer, "half" );
			break;
        case HLSLToken_FloatLiteral:
            strcpy(buffer, "float");
            break;
        case HLSLToken_IntLiteral:
            strcpy(buffer, "int");
            break;
		case HLSLToken_UintLiteral:
			strcpy(buffer, "uint");
			break;
        case HLSLToken_Identifier:
            strcpy(buffer, "identifier");
            break;
        case HLSLToken_EndOfStream:
            strcpy(buffer, "<eof>");
			break;
        default:
            strcpy(buffer, "unknown");
            break;
        }
    }

}


