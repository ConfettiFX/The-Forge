#ifndef FULL_TOKENIZER_H
#define FULL_TOKENIZER_H

#include <fstream>
#include <sstream>
#include <iostream>

#include "HLSLTokenizer.h"

#include "../../../EASTL/vector.h"
#include "../../../EASTL/string.h"

#include "StringLibrary.h"

// uncomment this line if you want tinystl::strings in your tokens for debuggin


struct FullToken
{
	// Token is initialized to reasonable defaults, so that if the parser asks for an invalid token,
	// we return a reasonable value. It should never happen, but we have this fallback just in case.
	FullToken()
	{
		m_float = 0.0f;
		m_int = 0;
		m_uint = 0;
		m_fileNameIndex = 0;
		m_fileLine = 0;

		m_id = -1;
		
		// the default token is EOF
		m_token = HLSLToken_EndOfStream;
	}

	HLSLToken m_token;
	float m_float;
	int m_int;
	unsigned int m_uint;
	int m_fileNameIndex;
	int m_fileLine;

	eastl::string m_identifier;

	// ID is the only new field. It starts at 0 and increments for each token for debugging purposes.
	int m_id;

};

class FullTokenizer
{

public:

	/// Maximum string length of an identifier.
	static const int s_maxIdentifier = 255 + 1;

	static const int s_maxHistoryIdentifier = 1024;

	// The constructor does all the work. It loads the file and parses all the tokens.
	FullTokenizer(StringLibrary * pStringLibrary, const char* fileName, const char* buffer, size_t length);

	const FullToken & GetFullToken() const;
	const FullToken & GetFullPrevToken(int index) const;

	// Reports an error using printf style formatting. The current line number
	// is included. Only the first error reported will be output.
	// Also, that's why it is not const.
	void Error(const char* format, ...);

	// Advances to the next token in the stream.
	void Next();

	// Returns the current token in the stream.
	int GetToken() const;

	// Returns the number of the current token.
	float GetFloat() const;
	int   GetInt() const;
	unsigned int GetuInt() const;

	// Returns the identifier for the current token.
	const char* GetIdentifier() const;

	const char* GetPrevIdentifier(int index) const;

	int GetHistoryCounter() const;

	// Returns the line number where the current token began.
	int GetLineNumber() const;

	// Returns the file name where the current token began.
	const char* GetFileName() const;

	eastl::string GetReadableName(const FullToken & fullToken) const;

	// Gets a human readable text description of the current token.
	void GetTokenName(char buffer[s_maxIdentifier]) const;

	// Gets a human readable text description of the specified token.
	static void GetTokenName(int token, char buffer[s_maxIdentifier]);

	void Undo();

	void DumpTokensToFile(const char fileName[]) const;

	eastl::vector < eastl::string > GetAllFileNames() const
	{
		return m_fileNames;
	}

private:
	eastl::vector < FullToken > m_tokens;
	eastl::vector < eastl::string > m_fileNames;

	int m_currToken;

	bool m_error;

	FullToken m_eofToken;

	StringLibrary * m_pStringLibrary;

public:
	char errorBuffer[1024];
};


#endif


