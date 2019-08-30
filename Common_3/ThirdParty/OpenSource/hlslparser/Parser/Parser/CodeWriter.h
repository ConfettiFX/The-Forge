//=============================================================================
//
// Render/CodeWriter.h
//
// Created by Max McGuire (max@unknownworlds.com)
// Copyright (c) 2013, Unknown Worlds Entertainment, Inc.
//
//=============================================================================

#ifndef CODE_WRITER_H
#define CODE_WRITER_H

#include "Engine.h"
#include "../../../EASTL/string.h"

#if defined(__GNUC__)
#define M4_PRINTF_ATTR(string_index, first_to_check) __attribute__((format(printf, string_index, first_to_check)))
#else
#define M4_PRINTF_ATTR(string_index, first_to_check)
#endif

/**
 * This class is used for outputting code. It handles indentation and inserting #line markers
 * to match the desired output line numbers.
 */
class CodeWriter
{

public:
    CodeWriter(bool writeFileNames = true);

    void BeginLine(int indent, const char* fileName = NULL, int lineNumber = -1);
    M4_PRINTF_ATTR(2, 3) void Write(const char* format, ...);
    void EndLine(const char* text = NULL);

	M4_PRINTF_ATTR(3, 4) void Write(int indent, const char* format, ...);

    M4_PRINTF_ATTR(3, 4) void WriteLine(int indent, const char* format, ...);

    M4_PRINTF_ATTR(5, 6) void WriteLineTagged(int indent, const char* fileName, int lineNumber, const char* format, ...);

    const char* GetResult() const;
    void Reset();

	int				m_previousLine = 0;
	int             m_currentLine;

private:

    eastl::string     m_buffer;
    
    const char*     m_currentFileName;
    int             m_spacesPerIndent;
    bool            m_writeLines;
    bool            m_writeFileNames;

};

#endif