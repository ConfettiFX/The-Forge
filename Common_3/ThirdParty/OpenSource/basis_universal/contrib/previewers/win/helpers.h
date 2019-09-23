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
 * Software decodes BC1 format data.
 * 
 * \param[in] src BC1 source blocks (the number of blocks being determined by the image dimensions)
 * \param[in] imgW width of the decoded image
 * \param[in] imgH height of the decoded image
 * \return handle to a bitmap (ownership passed to the caller)
 */
HBITMAP dxtToBitmap(const uint8_t* src, uint32_t const imgW, uint32_t const imgH, bool const flip = false);
