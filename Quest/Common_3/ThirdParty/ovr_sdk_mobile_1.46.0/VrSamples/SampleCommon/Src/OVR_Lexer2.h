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
#pragma once

#include <stdint.h>
#include <vector>

namespace OVRFW {

//==============================================================
// ovrLexer
//
// This was originally intended to get rid of sscanf usage which
// has security issues (buffer overflows) and cannot parse strings containing
// spaces without using less-than-ideal workarounds. Supports keeping quoted
// strings as single tokens, UTF-8 encoded text, but not much else.
//
// If a punctuation string is passed in, each character in the string
// is treated as a punctuation mark. Punctuation marks will always be
// lexed to a single punctuation per token, independent of whitespace.
//
// If the / and * characters are passed as punctuation, then the lexer
// will also treat // and /* */ as C-style comments.
//==============================================================
class ovrLexer {
   public:
    enum ovrResult {
        LEX_RESULT_OK,
        LEX_RESULT_ERROR, // ran out of buffer space
        LEX_RESULT_EOF, // tried to read past the end of the buffer
        LEX_RESULT_UNKNOWN_ESCAPE, // unrecognized escape code
        LEX_RESULT_UNEXPECTED_TOKEN, // did get the expected token
        LEX_RESULT_VALUE_OUT_OF_RANGE, // numeric value was out of range for the given data size
    };

    // length of source must be specified
    ovrLexer(const char* source, const size_t sourceLength, char const* punctuation);
    ovrLexer(std::vector<uint8_t> const& source, char const* punctuation);
    // expects a 0-terminated string as input and length is determined as lenght of a string.
    ovrLexer(const char* source);
    ovrLexer(const char* source, char const* punctuation);
    ovrLexer(const ovrLexer& other);
    ovrLexer(ovrLexer&& other);

    ~ovrLexer();

    ovrLexer& operator=(const ovrLexer& other);
    ovrLexer& operator=(ovrLexer&& other);

    ovrResult NextToken(char* token, size_t const maxTokenSize);
    ovrResult PeekToken(char* token, size_t const maxTokenSize);
    ovrResult ExpectToken(char const* expectedToken, char* token, size_t const maxTokenSize);
    ovrResult ExpectPunctuation(char const* punc, char* token, size_t const maxTokenSize);

    ovrResult ParseInt(int& value, int const defaultVal);
    ovrResult ParseUnsignedInt(unsigned int& value, unsigned int const defaultVal);
    ovrResult ParseLongLong(long long& value, long long const defaultVal);
    ovrResult ParseUnsignedLongLong(unsigned long long& value, unsigned long long const defaultVal);
    ovrResult ParseFloat(float& value, float const defaultVal);
    ovrResult ParseDouble(double& value, double const defaultVal);
    ovrResult ParsePointer(unsigned char*& ptr, unsigned char* defaultVal);
    ovrResult ParseToEndOfLine(char* buffer, size_t const maxBufferSize);

    ovrResult GetError() const {
        return Error;
    }

   private:
    static bool FindChar(char const* buffer, uint32_t const ch);
    static bool IsWhitespace(uint32_t const ch);
    static bool IsQuote(uint32_t const ch);
    static ovrResult SkipWhitespace(char const*& p, char const* source, size_t const sourceLength);
    static bool IsPunctuation(char const* punctuation, uint32_t const ch);
    static void CopyResult(char const* buffer, char* token, size_t const maxTokenSize);
    static uint32_t TranslateEscapeCode(uint32_t const inCh);

    uint32_t PeekNextChar();
    ovrResult SkipToEndOfLine();

    void EmitCodePoint(
        uint32_t const ch,
        char* buffer,
        ptrdiff_t& bufferOfs,
        size_t const bufferSize,
        char* token,
        size_t const maxTokenSize) const;

   private:
    const char* Source;
    size_t SourceLength;
    const char* p; // pointer to current position
    ovrResult Error;
    char* Punctuation; // UTF-8 string holding characters to lex as punctuation (may be empty)
};

} // namespace OVRFW
