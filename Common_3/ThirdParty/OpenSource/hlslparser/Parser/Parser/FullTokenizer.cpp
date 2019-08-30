#include "FullTokenizer.h"

#include "Engine.h"

#pragma warning(disable:4996)

// The constructor does all the work. It loads the file and parses all the tokens.
FullTokenizer::FullTokenizer(StringLibrary * pStringLibrary, const char* fileName, const char* buffer, size_t length)
{
	m_pStringLibrary = pStringLibrary;

	m_tokens.clear();
	m_fileNames.clear();

	m_currToken = -1;
	m_error = false;

	errorBuffer[0] = '\0';

	HLSLTokenizer tokenizer(fileName, buffer, length);

	bool isDone = false;
	eastl::string lastFileName;
	int lastFileIndex = -1;
	int currId = 0;

	while (!isDone)
	{
		FullToken fullToken;
		eastl::string fileName = tokenizer.GetFileName();

		if (fileName != lastFileName)
		{
			int fileIndex = -1;
			for (int i = 0; i < m_fileNames.size(); i++)
			{
				if (m_fileNames[i] == fileName)
				{
					fileIndex = i;
					break;
				}
			}

			if (fileIndex < 0)
			{
				fileIndex = (int)m_fileNames.size();
				m_fileNames.push_back(fileName);
			}

			lastFileName = fileName;
			lastFileIndex = fileIndex;
		}
		ASSERT_PARSER(lastFileIndex >= 0);
		fullToken.m_fileNameIndex = lastFileIndex;
		fullToken.m_fileLine = tokenizer.GetLineNumber();

		eastl::string identifier = tokenizer.GetIdentifier();

		fullToken.m_token = (HLSLToken)tokenizer.GetToken();
		m_pStringLibrary->InsertDirect(identifier);
		fullToken.m_identifier = identifier;

		fullToken.m_float = tokenizer.GetFloat();
		fullToken.m_int = tokenizer.GetInt();
		fullToken.m_uint = tokenizer.GetuInt();
		fullToken.m_id = currId;

		currId++;

		//char buf[HLSLTokenizer::s_maxIdentifier];
		//tokenizer.GetTokenName(buf);

		if (fullToken.m_token == HLSLToken_EndOfStream)
		{
			isDone = true;
		}

		m_tokens.push_back(fullToken);

		tokenizer.Next();
	}

	int lastLine = m_tokens.size() > 0 ? m_tokens.back().m_fileLine : 0;
	m_eofToken = FullToken();
	m_eofToken.m_fileLine = lastLine;

	m_currToken = 0;

	// tokenizer destructor happens here
}

void FullTokenizer::Error(const char* format, ...)
{
	FullToken token = GetFullToken();

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
	strcat(errorBuffer, m_fileNames[token.m_fileNameIndex].c_str());
	strcat(errorBuffer, "\n");

	strcat(errorBuffer, "error) Line (");
	strcat(errorBuffer, _itoa(token.m_fileLine, it, 10));
	strcat(errorBuffer, ")\t: ");
	strcat(errorBuffer, buffer);
	strcat(errorBuffer, "\n");

	std::cout << errorBuffer << std::endl;

	Log_Error("%s\n", buffer);
}

const FullToken & FullTokenizer::GetFullToken() const
{
	// not sure if we should assert if we go past the end of the tokens, for now just return
	// a base, empty token
	if (m_currToken >= m_tokens.size() || m_error)
	{
		return m_eofToken;
	}

	ASSERT_PARSER(m_currToken >= 0);

	return m_tokens[m_currToken];
}

const FullToken & FullTokenizer::GetFullPrevToken(int index) const
{
	// clamp at 0?
	int prevToken = (m_currToken >= index) ? m_currToken - index : 0;

	// not sure if we should assert if we go past the end of the tokens, for now just return
	// a base, empty token
	if (prevToken >= m_tokens.size() || m_error)
	{
		return m_eofToken;
	}

	ASSERT_PARSER(prevToken >= 0);

	return m_tokens[prevToken];
}

// Advances to the next token in the stream.
void FullTokenizer::Next()
{
	m_currToken++;
}

//static bool s_debugTokenizer = true;

// Returns the current token in the stream.
int FullTokenizer::GetToken() const
{
	const FullToken & fullToken = GetFullToken();
	return fullToken.m_token;
}

// Returns the number of the current token.
float FullTokenizer::GetFloat() const
{
	const FullToken & fullToken = GetFullToken();
	return fullToken.m_float;
}

int FullTokenizer::GetInt() const
{
	const FullToken & fullToken = GetFullToken();
	return fullToken.m_int;
}

unsigned int FullTokenizer::GetuInt() const
{
	const FullToken & fullToken = GetFullToken();
	return fullToken.m_uint;
}

// Returns the identifier for the current token.
const char* FullTokenizer::GetIdentifier() const
{
	const FullToken & fullToken = GetFullToken();
	return fullToken.m_identifier.c_str();
}

const char* FullTokenizer::GetPrevIdentifier(int index) const
{
	const FullToken & fullToken = GetFullPrevToken(index);
	return fullToken.m_identifier.c_str();
}

int FullTokenizer::GetHistoryCounter() const
{
	return m_currToken;
}

// Returns the line number where the current token began.
int FullTokenizer::GetLineNumber() const
{
	const FullToken & fullToken = GetFullToken();
	return fullToken.m_fileLine;
}

// Returns the file name where the current token began.
const char* FullTokenizer::GetFileName() const
{
	const FullToken & fullToken = GetFullToken();
	const eastl::string & fileName = m_fileNames[fullToken.m_fileNameIndex];
	return fileName.c_str();
}

// Gets a human readable text description of the current token.
void FullTokenizer::GetTokenName(char buffer[s_maxIdentifier]) const
{
	const FullToken & fullToken = GetFullToken();
	
	HLSLTokenizer::GetTokenName(fullToken.m_token, buffer);
/*
	int len = (int)fullToken.m_tokenName.length();
	ASSERT_PARSER(len < s_maxIdentifier);

	strcpy_s(buffer, s_maxIdentifier, fullToken.m_tokenName.c_str());
	*/
}

eastl::string FullTokenizer::GetReadableName(const FullToken & fullToken) const
{
	char buffer[HLSLTokenizer::s_maxIdentifier];

	if (fullToken.m_token == HLSLToken_FloatLiteral || fullToken.m_token == HLSLToken_HalfLiteral)
	{
		sprintf(buffer, "%f", fullToken.m_float);
	}
	else if (fullToken.m_token == HLSLToken_IntLiteral)
	{
		sprintf(buffer, "%d", fullToken.m_int);
	}
	else if (fullToken.m_token == HLSLToken_UintLiteral)
	{
		sprintf(buffer, "%u", fullToken.m_uint);
	}
	else if (fullToken.m_token == HLSLToken_Identifier)
	{
		strcpy(buffer, fullToken.m_identifier.c_str());
	}
	else
	{
		HLSLTokenizer::GetTokenName(fullToken.m_token, buffer);
	}

	return buffer;
}

// Gets a human readable text description of the specified token.
void FullTokenizer::GetTokenName(int token, char buffer[s_maxIdentifier])
{
	HLSLTokenizer::GetTokenName(token, buffer);
}

void FullTokenizer::Undo()
{
	m_currToken = (m_currToken >= 1) ? m_currToken - 1 : 0;
}

void FullTokenizer::DumpTokensToFile(const char fileName[]) const
{
	FILE * fout = fopen(fileName,"w");
	ASSERT_PARSER(fout != nullptr);

	for (int i = 0; i < m_tokens.size(); i++)
	{
		const FullToken & token = m_tokens[i];

		eastl::string readableName = GetReadableName(token);

		fprintf(fout,"%6d: %-20s (%2d - %6d)\n",i,readableName.c_str(),token.m_fileNameIndex,token.m_fileLine);
	}

	fprintf(fout,"\n");

	for (int i = 0; i < m_fileNames.size(); i++)
	{
		fprintf(fout,"%2d: %s\n", i, m_fileNames[i].c_str());
	}

	fprintf(fout, "\n");

	fclose(fout);
	fout = nullptr;
}

