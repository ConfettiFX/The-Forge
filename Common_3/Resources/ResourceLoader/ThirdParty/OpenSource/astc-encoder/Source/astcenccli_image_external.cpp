// SPDX-License-Identifier: Apache-2.0
// ----------------------------------------------------------------------------
// Copyright 2011-2022 Arm Limited
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy
// of the License at:
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations
// under the License.
// ----------------------------------------------------------------------------

/**
 * @brief Functions for building the implementation of stb_image and tinyexr.
 */

#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <vector>

#include "astcenccli_internal.h"

// Configure the STB image imagewrite library build.
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_NO_GIF
#define STBI_NO_PIC
#define STBI_NO_PNM
#define STBI_NO_PNG
#define STBI_NO_PSD

// Configure the TinyEXR library build.
#define TINYEXR_IMPLEMENTATION

// TheForge: added this define so that we can link astc-encoder against the AssetPipeline
#define TINYEXR_USE_MINIZ_FROM_THE_FORGE 1

// Configure the Wuffs library build.
#define WUFFS_IMPLEMENTATION
#define WUFFS_CONFIG__MODULES
#define WUFFS_CONFIG__MODULE__ADLER32
#define WUFFS_CONFIG__MODULE__BASE
#define WUFFS_CONFIG__MODULE__CRC32
#define WUFFS_CONFIG__MODULE__DEFLATE
#define WUFFS_CONFIG__MODULE__PNG
#define WUFFS_CONFIG__MODULE__ZLIB
#include "wuffs-v0.3.c"

// For both libraries force asserts (which can be triggered by corrupt input
// images) to be handled at runtime in release builds to avoid security issues.
#define STBI_ASSERT(x) astcenc_runtime_assert(x)
#define TEXR_ASSERT(x) astcenc_runtime_assert(x)

/**
 * @brief Trap image load failures and convert into a runtime error.
 */
static void astcenc_runtime_assert(bool condition)
{
    if (!condition)
    {
        printf("ERROR: Corrupt input image\n");
        exit(1);
    }
}

#include "stb_image.h"
#include "stb_image_write.h"
#include "tinyexr.h"

/**
 * @brief Load an image using Wuffs to provide the loader.
 *
 * @param      filename          The name of the file to load.
 * @param      y_flip            Should the image be vertically flipped?
 * @param[out] is_hdr            Is this an HDR image load?
 * @param[out] component_count   The number of components in the data.
 *
 * @return The loaded image data in a canonical 4 channel format, or @c nullptr on error.
 */
astcenc_image* load_png_with_wuffs(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	is_hdr = false;
	component_count = 4;

	std::ifstream file(filename, std::ios::binary | std::ios::ate);
	if (!file)
	{
		printf("ERROR: Failed to load image %s (can't fopen)\n", filename);
		return nullptr;
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<uint8_t> buffer(size);
	file.read((char*)buffer.data(), size);

	wuffs_png__decoder *dec = wuffs_png__decoder__alloc();
	if (!dec)
	{
		return nullptr;
	}

	wuffs_base__image_config ic;
	wuffs_base__io_buffer src = wuffs_base__ptr_u8__reader(buffer.data(), size, true);
	wuffs_base__status status = wuffs_png__decoder__decode_image_config(dec, &ic, &src);
	if (status.repr)
	{
		return nullptr;
	}

	uint32_t dim_x = wuffs_base__pixel_config__width(&ic.pixcfg);
	uint32_t dim_y = wuffs_base__pixel_config__height(&ic.pixcfg);
	size_t num_pixels = dim_x * dim_y;
	if (num_pixels > (SIZE_MAX / 4))
	{
		return nullptr;
	}

	// Override the image's native pixel format to be RGBA_NONPREMUL
	wuffs_base__pixel_config__set(
	    &ic.pixcfg,
	    WUFFS_BASE__PIXEL_FORMAT__RGBA_NONPREMUL,
	    WUFFS_BASE__PIXEL_SUBSAMPLING__NONE,
	    dim_x, dim_y);

	// Configure the work buffer
	size_t workbuf_len = wuffs_png__decoder__workbuf_len(dec).max_incl;
	if (workbuf_len > SIZE_MAX)
	{
		return nullptr;
	}

	wuffs_base__slice_u8 workbuf_slice = wuffs_base__make_slice_u8((uint8_t*)malloc(workbuf_len), workbuf_len);
	if (!workbuf_slice.ptr)
	{
		return nullptr;
	}

	wuffs_base__slice_u8 pixbuf_slice = wuffs_base__make_slice_u8((uint8_t*)malloc(num_pixels * 4), num_pixels * 4);
	if (!pixbuf_slice.ptr)
	{
		return nullptr;
	}

	wuffs_base__pixel_buffer pb;
	status = wuffs_base__pixel_buffer__set_from_slice(&pb, &ic.pixcfg, pixbuf_slice);
	if (status.repr)
	{
		return nullptr;
	}

	// Decode the pixels
	status = wuffs_png__decoder__decode_frame(dec, &pb, &src, WUFFS_BASE__PIXEL_BLEND__SRC, workbuf_slice, NULL);
	if (status.repr)
	{
		return nullptr;
	}

	astcenc_image* img = astc_img_from_unorm8x4_array(pixbuf_slice.ptr, dim_x, dim_y, y_flip);

	free(pixbuf_slice.ptr);
	free(workbuf_slice.ptr);
	free(dec);

	return img;
}
