/**********************************************************************************

Filename    :   OVR_Lexer.h
Content     :   A light-weight lexer
Created     :   May 1, 2015
Authors     :   Jonathan E. Wright

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

This source code is licensed under the BSD-style license found in the
LICENSE file in the Oculus360Photos/ directory. An additional grant
of patent rights can be found in the PATENTS file in the same directory.

************************************************************************************/

#include "OVR_Lexer2.h"

#include "OVR_Std.h"
#include "OVR_UTF8Util.h"
#include <cstdlib> // for strto* functions
#include <errno.h>
#include <utility>
#include <assert.h>

namespace OVRFW {

//==============================
// ovrLexer::ovrLexer
ovrLexer::ovrLexer(const char* source, const size_t sourceLength, char const* punctuation)
    : Source(source),
      SourceLength(sourceLength),
      p(Source),
      Error(LEX_RESULT_OK),
      Punctuation(NULL) {
    size_t len = punctuation == NULL ? 0 : OVR::OVR_strlen(punctuation);
    if (len == 0) {
        Punctuation = new char[16];
        Punctuation[0] = '\0';
    } else {
        Punctuation = new char[len + 1];
        OVR::OVR_strcpy(Punctuation, len + 1, punctuation);
    }
}

//==============================
// ovrLexer::ovrLexer
ovrLexer::ovrLexer(const char* source) : ovrLexer(source, OVR::OVR_strlen(source), NULL) {}

//==============================
// ovrLexer::ovrLexer
ovrLexer::ovrLexer(const char* source, char const* punctuation)
    : ovrLexer(source, OVR::OVR_strlen(source), punctuation) {}

//==============================
// ovrLexer::ovrLexer
ovrLexer::ovrLexer(std::vector<uint8_t> const& source, char const* punctuation)
    : ovrLexer(
          (const char*)(static_cast<uint8_t const*>(source.data())),
          source.size(),
          punctuation) {}

//==============================
// ovrLexer::ovrLexer
ovrLexer::ovrLexer(const ovrLexer& other) {
    operator=(other);
}

//==============================
// ovrLexer::ovrLexer
ovrLexer::ovrLexer(ovrLexer&& other) {
    operator=(std::move(other));
}

//==============================
// ovrLexer::~ovrLexer
ovrLexer::~ovrLexer() {
    assert(Error == LEX_RESULT_OK || Error == LEX_RESULT_EOF);
    delete Punctuation;
    Punctuation = NULL;
}

//==============================
// ovrLexer::operator=
ovrLexer& ovrLexer::operator=(const ovrLexer& other) {
    // self assignment
    if (this == &other) {
        return *this;
    }

    Source = other.Source;
    SourceLength = other.SourceLength;
    p = other.p;
    Error = other.Error;

    size_t len = other.Punctuation == NULL ? 0 : OVR::OVR_strlen(other.Punctuation);
    if (len == 0) {
        Punctuation = new char[16];
        Punctuation[0] = '\0';
    } else {
        Punctuation = new char[len + 1];
        OVR::OVR_strcpy(Punctuation, len + 1, other.Punctuation);
    }

    return *this;
}

//==============================
// ovrLexer::operator=
ovrLexer& ovrLexer::operator=(ovrLexer&& other) {
    // self assignment
    if (this == &other) {
        return *this;
    }

    Source = other.Source;
    SourceLength = other.SourceLength;
    p = other.p;
    Error = other.Error;
    Punctuation = other.Punctuation;

    other.Source = nullptr;
    other.SourceLength = 0;
    other.p = nullptr;
    other.Punctuation = nullptr;

    return *this;
}

//==============================
// ovrLexer::FindChar
bool ovrLexer::FindChar(char const* buffer, uint32_t const ch) {
    const char* curPtr = buffer;
    for (;;) {
        uint32_t const curChar = UTF8Util::DecodeNextChar(&curPtr);
        if (curChar == '\0') {
            return false;
        } else if (curChar == ch) {
            return true;
        }
    }
}

//==============================
// ovrLexer::IsWhitespace
bool ovrLexer::IsWhitespace(uint32_t const ch) {
    return (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
}

//==============================
// ovrLexer::IsQuote
bool ovrLexer::IsQuote(uint32_t const ch) {
    bool result = ch == '\"';
    assert(
        result == false || ch <= 255); // quotes are assumed elsewhere to be single-byte characters!
    return result;
}

//==============================
// ovrLexer::SkipWhitespace
ovrLexer::ovrResult
ovrLexer::SkipWhitespace(char const*& p, char const* source, size_t const sourceLength) {
    const char* cur = p; // copy p because we only want to advance if it is whitespace
    uint32_t ch;
    for (;;) {
        if (p >= source + sourceLength) {
            return LEX_RESULT_EOF;
        }

        ch = UTF8Util::DecodeNextChar(&cur);

        if (ch == '\0' || !IsWhitespace(ch)) {
            return LEX_RESULT_OK;
        }
        p = cur;
    }
}

//==============================
// ovrLexer::IsPunctuation
bool ovrLexer::IsPunctuation(char const* punctuation, uint32_t const ch) {
    const char* p = punctuation;
    uint32_t curPunc = UTF8Util::DecodeNextChar(&p);
    while (curPunc != '\0') {
        if (curPunc == ch) {
            return true;
        }
        curPunc = UTF8Util::DecodeNextChar(&p);
    }
    return false;
}

//==============================
// ovrLexer::CopyResult
void ovrLexer::CopyResult(char const* buffer, char* token, size_t const maxTokenSize) {
    // NOTE: if any multi-byte characters are ever treated as quotes, this code must change
    if (IsQuote(*buffer)) {
        size_t len = UTF8Util::GetLength(buffer);
        const uint32_t lastChar = UTF8Util::GetCharAt(len - 1, buffer);
        if (IsQuote(lastChar)) {
            // The first char and last char are single-byte quotes, we can now just step past the
            // first and not copy the last.
            char const* start = buffer + 1;
            len = OVR::OVR_strlen(start); // We do not care about UTF length here since we know the
                                          // quotes are a single bytes
            OVR::OVR_strncpy(token, maxTokenSize, start, len - 1);
            return;
        }
    }

    OVR::OVR_strcpy(token, maxTokenSize, buffer);
}

//==============================
// ovrLexer::PeekNextChar
uint32_t ovrLexer::PeekNextChar() {
    if (p >= Source + SourceLength) {
        return '\0';
    }

    // save state
    ovrResult error = Error;
    const char* tp = p;

    uint32_t ch = UTF8Util::DecodeNextChar(&p);

    // restore state
    Error = error;
    p = tp;

    return ch;
}

//==============================
// ovrLexer::SkipToEndOfLine
ovrLexer::ovrResult ovrLexer::SkipToEndOfLine() {
    uint32_t ch;
    do {
        if (p >= Source + SourceLength) {
            return LEX_RESULT_EOF;
        }

        ch = UTF8Util::DecodeNextChar(&p);
    } while (ch != '\n' && ch != '\0');

    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::EmitCodePoint
// helper function to ensure we never emit a character beyond the end of the buffer and that if we
// try to we always 0-terminate at the end of the buffer.
void ovrLexer::EmitCodePoint(
    uint32_t const ch,
    char* buffer,
    ptrdiff_t& bufferOfs,
    size_t const bufferSize,
    char* token,
    size_t const maxTokenSize) const {
    if (static_cast<size_t>(bufferOfs) < bufferSize) {
        UTF8Util::EncodeChar(buffer, &bufferOfs, ch);
    } else {
        // not enough room to emit, so stuff a null character at the end
        bufferOfs = bufferSize - 1;
        UTF8Util::EncodeChar(buffer, &bufferOfs, '\0');
    }
}

//==============================
// ovrLexer::TranslateEscapeCode
uint32_t ovrLexer::TranslateEscapeCode(uint32_t const inCh) {
    switch (inCh) {
        case 'n':
            return '\n';
        case 'r':
            return '\r';
        case 't':
            return '\t';
        case '"':
            return '\"';
        case '\'':
            return '\'';
        default:
            return '\0';
    }
}

//==============================
// ovrLexer::NextToken
ovrLexer::ovrResult ovrLexer::NextToken(char* token, size_t const maxTokenSize) {
    if (token == NULL || maxTokenSize <= 0) {
        assert(token != NULL && maxTokenSize > 0);
        return LEX_RESULT_ERROR;
    }

    token[0] = '\0';

    size_t const BUFF_SIZE = 8192;
    char buffer[BUFF_SIZE];

    SkipWhitespace(p, Source, SourceLength);

    bool inQuotes = false;
    bool inComment = false;
    bool isPunc = false;

    char const* lastp = p;

    ptrdiff_t bufferOfs = 0;
    for (;;) {
        if (p > Source + SourceLength) {
            EmitCodePoint('\0', buffer, bufferOfs, BUFF_SIZE, token, maxTokenSize);
            CopyResult(buffer, token, maxTokenSize);
            return LEX_RESULT_EOF;
        }

        lastp = p;
        uint32_t ch = UTF8Util::DecodeNextChar(&p);

        // exit if we just read whitespace or a null byte
        if (ch == '\0' || (!inQuotes && !inComment && IsWhitespace(ch))) {
            break;
        }

        isPunc = IsPunctuation(Punctuation, ch);
        if (inComment) {
            if (ch == '*' && PeekNextChar() == '/') {
                inComment = false;
                // consume the '/' character
                ch = UTF8Util::DecodeNextChar(&p);
                // skip any whitespace that may follow the comment
                ovrResult res = SkipWhitespace(p, Source, SourceLength);
                if (res != LEX_RESULT_OK) {
                    return res;
                }
            }
            continue;
        } else if (inQuotes && ch == '\\') {
            ch = TranslateEscapeCode(PeekNextChar());
            if (ch == '\0') {
                return LEX_RESULT_UNKNOWN_ESCAPE;
            }
            UTF8Util::DecodeNextChar(&p); // consume the escape code
        } else if (!inQuotes && !inComment && isPunc) {
            if (ch == '/' && PeekNextChar() == '*') {
                inComment = true;
                continue;
            } else if (ch == '/' && PeekNextChar() == '/') {
                SkipToEndOfLine();
                // skip any whitespace that may start the next line
                ovrResult res = SkipWhitespace(p, Source, SourceLength);
                if (res != LEX_RESULT_OK) {
                    return res;
                }
                continue;
            } else if (bufferOfs > 0) {
                // we're already in a token, undo the read of the punctuation and exit
                p = lastp;
                break;
            } else {
                // if this is the first character of a token, just emit the punctuation
                EmitCodePoint(ch, buffer, bufferOfs, BUFF_SIZE, token, maxTokenSize);
                break;
            }
        } else if (IsQuote(ch)) {
            if (inQuotes) // if we were in quotes, end the token at the closing quote
            {
                break;
            }
            // otherwise set the quote flag and skip emission of the quote character
            inQuotes = true;
            continue;
        }

        int encodeSize = UTF8Util::GetEncodeCharSize(ch);
        if (static_cast<size_t>(bufferOfs + encodeSize) >= BUFF_SIZE - 1 ||
            static_cast<size_t>(bufferOfs + encodeSize + 1) >= maxTokenSize) {
            // truncation
            UTF8Util::EncodeChar(buffer, &bufferOfs, '\0');
            CopyResult(buffer, token, maxTokenSize);
            return LEX_RESULT_ERROR;
        }
        UTF8Util::EncodeChar(buffer, &bufferOfs, ch);
    }

    // always emit a null byte
    EmitCodePoint('\0', buffer, bufferOfs, BUFF_SIZE, token, maxTokenSize);
    CopyResult(buffer, token, maxTokenSize);
    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::PeekToken
ovrLexer::ovrResult ovrLexer::PeekToken(char* token, size_t const maxTokenSize) {
    // save state
    ovrResult error = Error;
    const char* tp = p;

    ovrResult res = NextToken(token, maxTokenSize);

    // restore state
    Error = error;
    p = tp;

    return res;
}

//==============================
// ovrLexer::ParseInt
ovrLexer::ovrResult ovrLexer::ParseInt(int& value, int const defaultVal) {
    char token[128];
    ovrResult r = NextToken(token, sizeof(token));
    if (r != LEX_RESULT_OK) {
        value = defaultVal;
        return r;
    }

    errno = 0;
    char* endptr = nullptr;
    value = strtol(token, &endptr, 10);

    // Did we overflow?
    if (errno == ERANGE) {
        value = defaultVal;
        return LEX_RESULT_VALUE_OUT_OF_RANGE;
    }

    // Did we fail to parse any characters at all?
    if (endptr == token) {
        value = defaultVal;
        return LEX_RESULT_EOF;
    }

    // Did we hit a non-digit character before the end of the string?
    if (*endptr != '\0') {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    // any other unexpected error
    if (errno != 0) {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::ParseUnsignedInt
ovrLexer::ovrResult ovrLexer::ParseUnsignedInt(unsigned int& value, unsigned int const defaultVal) {
    char token[128];
    ovrResult r = NextToken(token, sizeof(token));
    if (r != LEX_RESULT_OK) {
        value = defaultVal;
        return r;
    }

    errno = 0;
    char* endptr = nullptr;
    value = strtoul(token, &endptr, 10);

    // Did we overflow?
    if (errno == ERANGE) {
        value = defaultVal;
        return LEX_RESULT_VALUE_OUT_OF_RANGE;
    }

    // Did we fail to parse any characters at all?
    if (endptr == token) {
        value = defaultVal;
        return LEX_RESULT_EOF;
    }

    // Did we hit a non-digit character before the end of the string?
    if (*endptr != '\0') {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    // any other unexpected error
    if (errno != 0) {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::ParseLongLong
ovrLexer::ovrResult ovrLexer::ParseLongLong(long long& value, long long const defaultVal) {
    char token[128];
    ovrResult r = NextToken(token, sizeof(token));
    if (r != LEX_RESULT_OK) {
        value = defaultVal;
        return r;
    }

    errno = 0;
    char* endptr = nullptr;
    value = strtoll(token, &endptr, 10);

    // Did we overflow?
    if (errno == ERANGE) {
        value = defaultVal;
        return LEX_RESULT_VALUE_OUT_OF_RANGE;
    }

    // Did we fail to parse any characters at all?
    if (endptr == token) {
        value = defaultVal;
        return LEX_RESULT_EOF;
    }

    // Did we hit a non-digit character before the end of the string?
    if (*endptr != '\0') {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    // any other unexpected error
    if (errno != 0) {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::ParseUnsignedLongLong
ovrLexer::ovrResult ovrLexer::ParseUnsignedLongLong(
    unsigned long long& value,
    unsigned long long const defaultVal) {
    char token[128];
    ovrResult r = NextToken(token, sizeof(token));
    if (r != LEX_RESULT_OK) {
        value = defaultVal;
        return r;
    }

    errno = 0;
    char* endptr = nullptr;
    value = strtoull(token, &endptr, 10);

    // Did we overflow?
    if (errno == ERANGE) {
        value = defaultVal;
        return LEX_RESULT_VALUE_OUT_OF_RANGE;
    }

    // Did we fail to parse any characters at all?
    if (endptr == token) {
        value = defaultVal;
        return LEX_RESULT_EOF;
    }

    // Did we hit a non-digit character before the end of the string?
    if (*endptr != '\0') {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    // any other unexpected error
    if (errno != 0) {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::ParseFloat
ovrLexer::ovrResult ovrLexer::ParseFloat(float& value, float const defaultVal) {
    char token[128];
    ovrResult r = NextToken(token, sizeof(token));
    if (r != LEX_RESULT_OK) {
        value = defaultVal;
        return r;
    }

    errno = 0;
    char* endptr = nullptr;
    value = strtof(token, &endptr);

    // Did we overflow?
    if (errno == ERANGE) {
        value = defaultVal;
        return LEX_RESULT_VALUE_OUT_OF_RANGE;
    }

    // Did we fail to parse any characters at all?
    if (endptr == token) {
        value = defaultVal;
        return LEX_RESULT_EOF;
    }

    // Did we hit a non-digit character before the end of the string?
    if (*endptr != '\0' && *endptr != 'f') {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    // any other unexpected error
    if (errno != 0) {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::ParseDouble
ovrLexer::ovrResult ovrLexer::ParseDouble(double& value, double const defaultVal) {
    char token[128];
    ovrResult r = NextToken(token, sizeof(token));
    if (r != LEX_RESULT_OK) {
        value = defaultVal;
        return r;
    }

    errno = 0;
    char* endptr = nullptr;
    value = strtod(token, &endptr);

    // Did we overflow?
    if (errno == ERANGE) {
        value = defaultVal;
        return LEX_RESULT_VALUE_OUT_OF_RANGE;
    }

    // Did we fail to parse any characters at all?
    if (endptr == token) {
        value = defaultVal;
        return LEX_RESULT_EOF;
    }

    // Did we hit a non-digit character before the end of the string?
    if (*endptr != '\0') {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    // any other unexpected error
    if (errno != 0) {
        value = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::ParsePointer
ovrLexer::ovrResult ovrLexer::ParsePointer(unsigned char*& ptr, unsigned char* defaultVal) {
    char token[128];
    ovrResult r = NextToken(token, sizeof(token));
    if (r != LEX_RESULT_OK) {
        ptr = defaultVal;
        return r;
    }
    const int result = sscanf(token, "%p", &ptr);
    if (result != 1) {
        ptr = defaultVal;
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }

    return LEX_RESULT_OK;
}

ovrLexer::ovrResult ovrLexer::ParseToEndOfLine(char* buffer, size_t const maxBufferSize) {
    size_t remainingLen = SourceLength - (p - Source);
    if (maxBufferSize > remainingLen) {
        memcpy(buffer, p, remainingLen);
        buffer[remainingLen] = '\0';
        return LEX_RESULT_OK;
    }

    return LEX_RESULT_ERROR;
}

//==============================
// ovrLexer::ExpectToken
ovrLexer::ovrResult
ovrLexer::ExpectToken(char const* expectedToken, char* token, size_t const maxTokenSize) {
    ovrResult res = NextToken(token, maxTokenSize);
    if (res != LEX_RESULT_OK) {
        return res;
    }

    if (OVR::OVR_strcmp(token, expectedToken) != 0) {
        return LEX_RESULT_UNEXPECTED_TOKEN;
    }
    return LEX_RESULT_OK;
}

//==============================
// ovrLexer::ExpectPunctuation
ovrLexer::ovrResult
ovrLexer::ExpectPunctuation(char const* punc, char* token, size_t const maxTokenSize) {
    ovrResult res = NextToken(token, maxTokenSize);
    if (res != LEX_RESULT_OK) {
        return res;
    }
    size_t tokenLen = OVR::OVR_strlen(token);
    if (tokenLen == 1) {
        int len = static_cast<int>(OVR::OVR_strlen(punc));
        for (int i = 0; i < len; ++i) {
            if (punc[i] == token[0]) {
                return LEX_RESULT_OK;
            }
        }
    }
    return LEX_RESULT_UNEXPECTED_TOKEN;
}

#if 0 // enable for unit tests at static initialization time

#include "OVR_LogUtils.h"

class ovrLexerUnitTest {
public:
	ovrLexerUnitTest()
	{
		TestLex( "Single-token." );
		TestLex( "This is a test of simple parsing." );
		TestLex( "  Test leading whitespace." );
		TestLex( "Test trailing whitespace.   " );
		TestLex( "Test token truncation 012345678901234567890123456789ABCDEFGHIJ." );
		TestLex( "Test \"quoted strings\"!" );
		TestLex( "Test quoted strings with token truncation \"012345678901234567890123456789ABCDEFGHIJ\"!" );
		TestEOF( "This is a test of EOF" );
	}

	void TestLexInternal( ovrLexer & lex, char const * text )
	{
		char out[8192];
		char token[32];

		out[0] = '\0';
		while( lex.NextToken( token, sizeof( token ) ) == ovrLexer::LEX_RESULT_OK )
		{
			OVR::OVR_strcat( out, sizeof( out ), token );
			OVR::OVR_strcat( out, sizeof( out ), " " );
		}
		LOG( "%s", out );
	}

	void TestLex( char const * text )
	{
		ovrLexer lex( text );
		TestLexInternal( lex, text );
	}

	void TestEOF( char const * text )
	{
		ovrLexer lex( text );
		TestLexInternal( lex, text );

		char token[32];
		if ( lex.NextToken( token, sizeof( token ) ) != ovrLexer::LEX_RESULT_EOF )
		{
			LOG( "Did not get EOF!" );
		}
		if ( lex.NextToken( token, sizeof( token ) ) == ovrLexer::LEX_RESULT_EOF )
		{
			LOG( "Got expected EOF" );
		}
	}

};

static ovrLexerUnitTest unitTest;

#endif

} // namespace OVRFW
