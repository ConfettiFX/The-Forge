#pragma once

#include <Windows.h>

#include <cstdint>

/**
 * Write a formatted string to the connected debugger (e.g. DebugView).
 * 
 * \param[in] fmt content to write in \c printf format (followed by optional arguments)
 */
void dprintf(char* const fmt, ...);

/**
 * Converts raw RGBA data to a Windows BGRA bitmap.
 * 
 * \param[in] src raw RGBA data 
 * \param[in] imgW width of the decoded image
 * \param[in] imgH height of the decoded image
 * \return handle to a bitmap (ownership passed to the caller)
 */
HBITMAP rgbToBitmap(const uint32_t* src, uint32_t const imgW, uint32_t const imgH, bool const flip = false);
