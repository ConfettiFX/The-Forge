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
 * @brief Functions for loading/storing uncompressed and compressed images.
 */

#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "astcenccli_internal.h"

#include "stb_image.h"
#include "stb_image_write.h"
#include "tinyexr.h"

/* ============================================================================
  Image load and store through the stb_iamge and tinyexr libraries
============================================================================ */

/**
 * @brief Load a .exr image using TinyExr to provide the loader.
 *
 * @param      filename          The name of the file to load.
 * @param      y_flip            Should the image be vertically flipped?
 * @param[out] is_hdr            Is this an HDR image load? Always @c true for this function.
 * @param[out] component_count   The number of components in the data.
 *
 * @return The loaded image data in a canonical 4 channel format.
 */
static astcenc_image* load_image_with_tinyexr(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	int dim_x, dim_y;
	float* image;
	const char* err;

	int load_res = LoadEXR(&image, &dim_x, &dim_y, filename, &err);
	if (load_res != TINYEXR_SUCCESS)
	{
		printf("ERROR: Failed to load image %s (%s)\n", filename, err);
		free(reinterpret_cast<void*>(const_cast<char*>(err)));
		return nullptr;
	}

	astcenc_image* res_img = astc_img_from_floatx4_array(image, dim_x, dim_y, y_flip);
	free(image);

	is_hdr = true;
	component_count = 4;
	return res_img;
}

/**
 * @brief Load an image using STBImage to provide the loader.
 *
 * @param      filename          The name of the file to load.
 * @param      y_flip            Should the image be vertically flipped?
 * @param[out] is_hdr            Is this an HDR image load?
 * @param[out] component_count   The number of components in the data.
 *
 * @return The loaded image data in a canonical 4 channel format, or @c nullptr on error.
 */
static astcenc_image* load_image_with_stb(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	int dim_x, dim_y;

	if (stbi_is_hdr(filename))
	{
		float* data = stbi_loadf(filename, &dim_x, &dim_y, nullptr, STBI_rgb_alpha);
		if (data)
		{
			astcenc_image* img = astc_img_from_floatx4_array(data, dim_x, dim_y, y_flip);
			stbi_image_free(data);
			is_hdr = true;
			component_count = 4;
			return img;
		}
	}
	else
	{
		uint8_t* data = stbi_load(filename, &dim_x, &dim_y, nullptr, STBI_rgb_alpha);
		if (data)
		{
			astcenc_image* img = astc_img_from_unorm8x4_array(data, dim_x, dim_y, y_flip);
			stbi_image_free(data);
			is_hdr = false;
			component_count = 4;
			return img;
		}
	}

	printf("ERROR: Failed to load image %s (%s)\n", filename, stbi_failure_reason());
	return nullptr;
}

/**
 * @brief Save an EXR image using TinyExr to provide the store routine.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
static bool store_exr_image_with_tinyexr(
	const astcenc_image* img,
	const char* filename,
	int y_flip
) {
	float *buf = floatx4_array_from_astc_img(img, y_flip);
	int res = SaveEXR(buf, img->dim_x, img->dim_y, 4, 1, filename, nullptr);
	delete[] buf;
	return res >= 0;
}

/**
 * @brief Save a PNG image using STBImageWrite to provide the store routine.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
static bool store_png_image_with_stb(
	const astcenc_image* img,
	const char* filename,
	int y_flip
) {
	assert(img->data_type == ASTCENC_TYPE_U8);
	uint8_t* buf = reinterpret_cast<uint8_t*>(img->data[0]);

	stbi_flip_vertically_on_write(y_flip);
	int res = stbi_write_png(filename, img->dim_x, img->dim_y, 4, buf, img->dim_x * 4);
	return res != 0;
}

/**
 * @brief Save a TGA image using STBImageWrite to provide the store routine.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
static bool store_tga_image_with_stb(
	const astcenc_image* img,
	const char* filename,
	int y_flip
) {
	assert(img->data_type == ASTCENC_TYPE_U8);
	uint8_t* buf = reinterpret_cast<uint8_t*>(img->data[0]);

	stbi_flip_vertically_on_write(y_flip);
	int res = stbi_write_tga(filename, img->dim_x, img->dim_y, 4, buf);
	return res != 0;
}

/**
 * @brief Save a BMP image using STBImageWrite to provide the store routine.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
static bool store_bmp_image_with_stb(
	const astcenc_image* img,
	const char* filename,
	int y_flip
) {
	assert(img->data_type == ASTCENC_TYPE_U8);
	uint8_t* buf = reinterpret_cast<uint8_t*>(img->data[0]);

	stbi_flip_vertically_on_write(y_flip);
	int res = stbi_write_bmp(filename, img->dim_x, img->dim_y, 4, buf);
	return res != 0;
}

/**
 * @brief Save a HDR image using STBImageWrite to provide the store routine.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
static bool store_hdr_image_with_stb(
	const astcenc_image* img,
	const char* filename,
	int y_flip
) {
	float* buf = floatx4_array_from_astc_img(img, y_flip);
	int res = stbi_write_hdr(filename, img->dim_x, img->dim_y, 4, buf);
	delete[] buf;
	return res != 0;
}

/* ============================================================================
Native Load and store of KTX and DDS file formats.

Unlike "regular" 2D image formats, which are mostly supported through stb_image
and tinyexr, these formats are supported directly; this involves a relatively
large number of pixel formats.

The following restrictions apply to loading of these file formats:

    * Only uncompressed data supported
    * Only first mipmap in mipmap pyramid supported
    * KTX: Cube-map arrays are not supported
============================================================================ */
enum scanline_transfer
{
	R8_TO_RGBA8,
	RG8_TO_RGBA8,
	RGB8_TO_RGBA8,
	RGBA8_TO_RGBA8,
	BGR8_TO_RGBA8,
	BGRA8_TO_RGBA8,
	L8_TO_RGBA8,
	LA8_TO_RGBA8,

	RGBX8_TO_RGBA8,
	BGRX8_TO_RGBA8,

	R16_TO_RGBA16F,
	RG16_TO_RGBA16F,
	RGB16_TO_RGBA16F,
	RGBA16_TO_RGBA16F,
	BGR16_TO_RGBA16F,
	BGRA16_TO_RGBA16F,
	L16_TO_RGBA16F,
	LA16_TO_RGBA16F,

	R16F_TO_RGBA16F,
	RG16F_TO_RGBA16F,
	RGB16F_TO_RGBA16F,
	RGBA16F_TO_RGBA16F,
	BGR16F_TO_RGBA16F,
	BGRA16F_TO_RGBA16F,
	L16F_TO_RGBA16F,
	LA16F_TO_RGBA16F,

	R32F_TO_RGBA16F,
	RG32F_TO_RGBA16F,
	RGB32F_TO_RGBA16F,
	RGBA32F_TO_RGBA16F,
	BGR32F_TO_RGBA16F,
	BGRA32F_TO_RGBA16F,
	L32F_TO_RGBA16F,
	LA32F_TO_RGBA16F
};

/**
 * @brief Copy a scanline from a source file and expand to a canonical format.
 *
 * Outputs are always 4 component RGBA, stored as U8 (LDR) or FP16 (HDR).
 *
 * @param[out] dst           The start of the line to store to.
 * @param      src           The start of the line to load.
 * @param      pixel_count   The number of pixels in the scanline.
 * @param      method        The conversion function.
 */
static void copy_scanline(
	void* dst,
	const void* src,
	int pixel_count,
	scanline_transfer method
) {

#define id(x) (x)
#define u16_sf16(x) float_to_float16(x * (1.0f/65535.0f))
#define f32_sf16(x) float_to_float16(x)

#define COPY_R(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++) \
		{ \
			d[4 * i    ] = convfunc(s[i]); \
			d[4 * i + 1] = 0;              \
			d[4 * i + 2] = 0;              \
			d[4 * i + 3] = oneval;         \
		} \
	} while (0); \
	break

#define COPY_RG(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++) \
		{ \
			d[4 * i    ] = convfunc(s[2 * i    ]); \
			d[4 * i + 1] = convfunc(s[2 * i + 1]); \
			d[4 * i + 2] = 0;                      \
			d[4 * i + 3] = oneval;                 \
		} \
	} while (0); \
	break

#define COPY_RGB(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++) \
		{ \
			d[4 * i    ] = convfunc(s[3 * i    ]); \
			d[4 * i + 1] = convfunc(s[3 * i + 1]); \
			d[4 * i + 2] = convfunc(s[3 * i + 2]); \
			d[4 * i + 3] = oneval;                 \
		} \
	} while (0); \
	break

#define COPY_BGR(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++)\
		{ \
			d[4 * i    ] = convfunc(s[3 * i + 2]); \
			d[4 * i + 1] = convfunc(s[3 * i + 1]); \
			d[4 * i + 2] = convfunc(s[3 * i    ]); \
			d[4 * i + 3] = oneval;                 \
		} \
	} while (0); \
	break

#define COPY_RGBX(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++)\
		{ \
			d[4 * i    ] = convfunc(s[4 * i    ]); \
			d[4 * i + 1] = convfunc(s[4 * i + 1]); \
			d[4 * i + 2] = convfunc(s[4 * i + 2]); \
			d[4 * i + 3] = oneval;                 \
		} \
	} while (0); \
	break

#define COPY_BGRX(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++)\
		{ \
			d[4 * i    ] = convfunc(s[4 * i + 2]); \
			d[4 * i + 1] = convfunc(s[4 * i + 1]); \
			d[4 * i + 2] = convfunc(s[4 * i    ]); \
			d[4 * i + 3] = oneval;                 \
		} \
	} while (0); \
	break

#define COPY_RGBA(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++) \
		{ \
			d[4 * i    ] = convfunc(s[4 * i    ]); \
			d[4 * i + 1] = convfunc(s[4 * i + 1]); \
			d[4 * i + 2] = convfunc(s[4 * i + 2]); \
			d[4 * i + 3] = convfunc(s[4 * i + 3]); \
		} \
	} while (0); \
	break

#define COPY_BGRA(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++) \
		{ \
			d[4 * i    ] = convfunc(s[4 * i + 2]); \
			d[4 * i + 1] = convfunc(s[4 * i + 1]); \
			d[4 * i + 2] = convfunc(s[4 * i    ]); \
			d[4 * i + 3] = convfunc(s[4 * i + 3]); \
		} \
	} while (0); \
	break

#define COPY_L(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++) \
		{ \
			d[4 * i    ] = convfunc(s[i]); \
			d[4 * i + 1] = convfunc(s[i]); \
			d[4 * i + 2] = convfunc(s[i]); \
			d[4 * i + 3] = oneval;         \
		} \
	} while (0); \
	break

#define COPY_LA(dsttype, srctype, convfunc, oneval) \
	do { \
		const srctype* s = reinterpret_cast<const srctype*>(src); \
		dsttype* d = reinterpret_cast<dsttype*>(dst); \
		for (int i = 0; i < pixel_count; i++) \
		{ \
			d[4 * i    ] = convfunc(s[2 * i    ]); \
			d[4 * i + 1] = convfunc(s[2 * i    ]); \
			d[4 * i + 2] = convfunc(s[2 * i    ]); \
			d[4 * i + 3] = convfunc(s[2 * i + 1]); \
		} \
	} while (0); \
	break

	switch (method)
	{
	case R8_TO_RGBA8:
		COPY_R(uint8_t, uint8_t, id, 0xFF);
	case RG8_TO_RGBA8:
		COPY_RG(uint8_t, uint8_t, id, 0xFF);
	case RGB8_TO_RGBA8:
		COPY_RGB(uint8_t, uint8_t, id, 0xFF);
	case RGBA8_TO_RGBA8:
		COPY_RGBA(uint8_t, uint8_t, id, 0xFF);
	case BGR8_TO_RGBA8:
		COPY_BGR(uint8_t, uint8_t, id, 0xFF);
	case BGRA8_TO_RGBA8:
		COPY_BGRA(uint8_t, uint8_t, id, 0xFF);
	case RGBX8_TO_RGBA8:
		COPY_RGBX(uint8_t, uint8_t, id, 0xFF);
	case BGRX8_TO_RGBA8:
		COPY_BGRX(uint8_t, uint8_t, id, 0xFF);
	case L8_TO_RGBA8:
		COPY_L(uint8_t, uint8_t, id, 0xFF);
	case LA8_TO_RGBA8:
		COPY_LA(uint8_t, uint8_t, id, 0xFF);

	case R16F_TO_RGBA16F:
		COPY_R(uint16_t, uint16_t, id, 0x3C00);
	case RG16F_TO_RGBA16F:
		COPY_RG(uint16_t, uint16_t, id, 0x3C00);
	case RGB16F_TO_RGBA16F:
		COPY_RGB(uint16_t, uint16_t, id, 0x3C00);
	case RGBA16F_TO_RGBA16F:
		COPY_RGBA(uint16_t, uint16_t, id, 0x3C00);
	case BGR16F_TO_RGBA16F:
		COPY_BGR(uint16_t, uint16_t, id, 0x3C00);
	case BGRA16F_TO_RGBA16F:
		COPY_BGRA(uint16_t, uint16_t, id, 0x3C00);
	case L16F_TO_RGBA16F:
		COPY_L(uint16_t, uint16_t, id, 0x3C00);
	case LA16F_TO_RGBA16F:
		COPY_LA(uint16_t, uint16_t, id, 0x3C00);

	case R16_TO_RGBA16F:
		COPY_R(uint16_t, uint16_t, u16_sf16, 0x3C00);
	case RG16_TO_RGBA16F:
		COPY_RG(uint16_t, uint16_t, u16_sf16, 0x3C00);
	case RGB16_TO_RGBA16F:
		COPY_RGB(uint16_t, uint16_t, u16_sf16, 0x3C00);
	case RGBA16_TO_RGBA16F:
		COPY_RGBA(uint16_t, uint16_t, u16_sf16, 0x3C00);
	case BGR16_TO_RGBA16F:
		COPY_BGR(uint16_t, uint16_t, u16_sf16, 0x3C00);
	case BGRA16_TO_RGBA16F:
		COPY_BGRA(uint16_t, uint16_t, u16_sf16, 0x3C00);
	case L16_TO_RGBA16F:
		COPY_L(uint16_t, uint16_t, u16_sf16, 0x3C00);
	case LA16_TO_RGBA16F:
		COPY_LA(uint16_t, uint16_t, u16_sf16, 0x3C00);

	case R32F_TO_RGBA16F:
		COPY_R(uint16_t, float, f32_sf16, 0x3C00);
	case RG32F_TO_RGBA16F:
		COPY_RG(uint16_t, float, f32_sf16, 0x3C00);
	case RGB32F_TO_RGBA16F:
		COPY_RGB(uint16_t, float, f32_sf16, 0x3C00);
	case RGBA32F_TO_RGBA16F:
		COPY_RGBA(uint16_t, float, f32_sf16, 0x3C00);
	case BGR32F_TO_RGBA16F:
		COPY_BGR(uint16_t, float, f32_sf16, 0x3C00);
	case BGRA32F_TO_RGBA16F:
		COPY_BGRA(uint16_t, float, f32_sf16, 0x3C00);
	case L32F_TO_RGBA16F:
		COPY_L(uint16_t, float, f32_sf16, 0x3C00);
	case LA32F_TO_RGBA16F:
		COPY_LA(uint16_t, float, f32_sf16, 0x3C00);
	}
}

/**
 * @brief Swap endianness of N two byte values.
 *
 * @param[in,out] dataptr      The data to convert.
 * @param         byte_count   The number of bytes to convert.
 */
static void switch_endianness2(
	void* dataptr,
	int byte_count
) {
	uint8_t* data = reinterpret_cast<uint8_t*>(dataptr);
	for (int i = 0; i < byte_count / 2; i++)
	{
		uint8_t d0 = data[0];
		uint8_t d1 = data[1];
		data[0] = d1;
		data[1] = d0;
		data += 2;
	}
}

/**
 * @brief Swap endianness of N four byte values.
 *
 * @param[in,out] dataptr      The data to convert.
 * @param         byte_count   The number of bytes to convert.
 */
static void switch_endianness4(
	void* dataptr,
	int byte_count
) {
	uint8_t* data = reinterpret_cast<uint8_t*>(dataptr);
	for (int i = 0; i < byte_count / 4; i++)
	{
		uint8_t d0 = data[0];
		uint8_t d1 = data[1];
		uint8_t d2 = data[2];
		uint8_t d3 = data[3];
		data[0] = d3;
		data[1] = d2;
		data[2] = d1;
		data[3] = d0;
		data += 4;
	}
}

/**
 * @brief Swap endianness of a u32 value.
 *
 * @param v   The data to convert.
 *
 * @return The converted value.
 */
static uint32_t u32_byterev(uint32_t v)
{
	return (v >> 24) | ((v >> 8) & 0xFF00) | ((v << 8) & 0xFF0000) | (v << 24);
}

/*
 Notes about KTX:

 After the header and the key/value data area, the actual image data follows.
 Each image starts with a 4-byte "imageSize" value indicating the number of bytes of image data follow.
 (For cube-maps, this value appears only after first image; the remaining 5 images are all of equal size.)
 If the size of an image is not a multiple of 4, then it is padded to the next multiple of 4.
 Note that this padding is NOT included in the "imageSize" field.
 In a cubemap, the padding appears after each face note that in a 2D/3D texture, padding does
 NOT appear between the lines/planes of the texture!

 In a KTX file, there may be multiple images; they are organized as follows:

 For each mipmap_level in numberOfMipmapLevels
 	UInt32 imageSize;
 	For each array_element in numberOfArrayElements
 	* for each face in numberOfFaces
 		* for each z_slice in pixelDepth
 			* for each row or row_of_blocks in pixelHeight
 				* for each pixel or block_of_pixels in pixelWidth
 					Byte data[format-specific-number-of-bytes]
 				* end
 			* end
 		*end
 		Byte cubePadding[0-3]
 	*end
 	Byte mipPadding[3 - ((imageSize+ 3) % 4)]
 *end

 In the ASTC codec, we will, for the time being only harvest the first image,
 and we will support only a limited set of formats:

 gl_type: UNSIGNED_BYTE UNSIGNED_SHORT HALF_FLOAT FLOAT UNSIGNED_INT_8_8_8_8 UNSIGNED_INT_8_8_8_8_REV
 gl_format: RED, RG. RGB, RGBA BGR, BGRA
 gl_internal_format: used for upload to OpenGL; we can ignore it on uncompressed-load, but
 	need to provide a reasonable value on store: RGB8 RGBA8 RGB16F RGBA16F
 gl_base_internal_format: same as gl_format unless texture is compressed (well, BGR is turned into RGB)
 	RED, RG, RGB, RGBA
*/

// Khronos enums
#define GL_RED                                      0x1903
#define GL_RG                                       0x8227
#define GL_RGB                                      0x1907
#define GL_RGBA                                     0x1908
#define GL_BGR                                      0x80E0
#define GL_BGRA                                     0x80E1
#define GL_LUMINANCE                                0x1909
#define GL_LUMINANCE_ALPHA                          0x190A

#define GL_R8                                       0x8229
#define GL_RG8                                      0x822B
#define GL_RGB8                                     0x8051
#define GL_RGBA8                                    0x8058

#define GL_R16F                                     0x822D
#define GL_RG16F                                    0x822F
#define GL_RGB16F                                   0x881B
#define GL_RGBA16F                                  0x881A

#define GL_UNSIGNED_BYTE                            0x1401
#define GL_UNSIGNED_SHORT                           0x1403
#define GL_HALF_FLOAT                               0x140B
#define GL_FLOAT                                    0x1406

#define GL_COMPRESSED_RGBA_ASTC_4x4                 0x93B0
#define GL_COMPRESSED_RGBA_ASTC_5x4                 0x93B1
#define GL_COMPRESSED_RGBA_ASTC_5x5                 0x93B2
#define GL_COMPRESSED_RGBA_ASTC_6x5                 0x93B3
#define GL_COMPRESSED_RGBA_ASTC_6x6                 0x93B4
#define GL_COMPRESSED_RGBA_ASTC_8x5                 0x93B5
#define GL_COMPRESSED_RGBA_ASTC_8x6                 0x93B6
#define GL_COMPRESSED_RGBA_ASTC_8x8                 0x93B7
#define GL_COMPRESSED_RGBA_ASTC_10x5                0x93B8
#define GL_COMPRESSED_RGBA_ASTC_10x6                0x93B9
#define GL_COMPRESSED_RGBA_ASTC_10x8                0x93BA
#define GL_COMPRESSED_RGBA_ASTC_10x10               0x93BB
#define GL_COMPRESSED_RGBA_ASTC_12x10               0x93BC
#define GL_COMPRESSED_RGBA_ASTC_12x12               0x93BD

#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4         0x93D0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4         0x93D1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5         0x93D2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5         0x93D3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6         0x93D4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5         0x93D5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6         0x93D6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8         0x93D7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5        0x93D8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6        0x93D9
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8        0x93DA
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10       0x93DB
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10       0x93DC
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12       0x93DD

#define GL_COMPRESSED_RGBA_ASTC_3x3x3_OES           0x93C0
#define GL_COMPRESSED_RGBA_ASTC_4x3x3_OES           0x93C1
#define GL_COMPRESSED_RGBA_ASTC_4x4x3_OES           0x93C2
#define GL_COMPRESSED_RGBA_ASTC_4x4x4_OES           0x93C3
#define GL_COMPRESSED_RGBA_ASTC_5x4x4_OES           0x93C4
#define GL_COMPRESSED_RGBA_ASTC_5x5x4_OES           0x93C5
#define GL_COMPRESSED_RGBA_ASTC_5x5x5_OES           0x93C6
#define GL_COMPRESSED_RGBA_ASTC_6x5x5_OES           0x93C7
#define GL_COMPRESSED_RGBA_ASTC_6x6x5_OES           0x93C8
#define GL_COMPRESSED_RGBA_ASTC_6x6x6_OES           0x93C9

#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES   0x93E0
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES   0x93E1
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES   0x93E2
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES   0x93E3
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES   0x93E4
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES   0x93E5
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES   0x93E6
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES   0x93E7
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES   0x93E8
#define GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES   0x93E9

struct format_entry
{
	unsigned int x;
	unsigned int y;
	unsigned int z;
	bool is_srgb;
	unsigned int format;
};

static const std::array<format_entry, 48> ASTC_FORMATS =
{{
	// 2D Linear RGB
	{ 4,  4,  1, false, GL_COMPRESSED_RGBA_ASTC_4x4},
	{ 5,  4,  1, false, GL_COMPRESSED_RGBA_ASTC_5x4},
	{ 5,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_5x5},
	{ 6,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_6x5},
	{ 6,  6,  1, false, GL_COMPRESSED_RGBA_ASTC_6x6},
	{ 8,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_8x5},
	{ 8,  6,  1, false, GL_COMPRESSED_RGBA_ASTC_8x6},
	{ 8,  8,  1, false, GL_COMPRESSED_RGBA_ASTC_8x8},
	{10,  5,  1, false, GL_COMPRESSED_RGBA_ASTC_10x5},
	{10,  6,  1, false, GL_COMPRESSED_RGBA_ASTC_10x6},
	{10,  8,  1, false, GL_COMPRESSED_RGBA_ASTC_10x8},
	{10, 10,  1, false, GL_COMPRESSED_RGBA_ASTC_10x10},
	{12, 10,  1, false, GL_COMPRESSED_RGBA_ASTC_12x10},
	{12, 12,  1, false, GL_COMPRESSED_RGBA_ASTC_12x12},
	// 2D SRGB
	{ 4,  4,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4},
	{ 5,  4,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4},
	{ 5,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5},
	{ 6,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5},
	{ 6,  6,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6},
	{ 8,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x5},
	{ 8,  6,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x6},
	{ 8,  8,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_8x8},
	{10,  5,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x5},
	{10,  6,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x6},
	{10,  8,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x8},
	{10, 10,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_10x10},
	{12, 10,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x10},
	{12, 12,  1,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_12x12},
	// 3D Linear RGB
	{ 3,  3,  3, false, GL_COMPRESSED_RGBA_ASTC_3x3x3_OES},
	{ 4,  3,  3, false, GL_COMPRESSED_RGBA_ASTC_4x3x3_OES},
	{ 4,  4,  3, false, GL_COMPRESSED_RGBA_ASTC_4x4x3_OES},
	{ 4,  4,  4, false, GL_COMPRESSED_RGBA_ASTC_4x4x4_OES},
	{ 5,  4,  4, false, GL_COMPRESSED_RGBA_ASTC_5x4x4_OES},
	{ 5,  5,  4, false, GL_COMPRESSED_RGBA_ASTC_5x5x4_OES},
	{ 5,  5,  5, false, GL_COMPRESSED_RGBA_ASTC_5x5x5_OES},
	{ 6,  5,  5, false, GL_COMPRESSED_RGBA_ASTC_6x5x5_OES},
	{ 6,  6,  5, false, GL_COMPRESSED_RGBA_ASTC_6x6x5_OES},
	{ 6,  6,  6, false, GL_COMPRESSED_RGBA_ASTC_6x6x6_OES},
	// 3D SRGB
	{ 3,  3,  3,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_3x3x3_OES},
	{ 4,  3,  3,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x3x3_OES},
	{ 4,  4,  3,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x3_OES},
	{ 4,  4,  4,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_4x4x4_OES},
	{ 5,  4,  4,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x4x4_OES},
	{ 5,  5,  4,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x4_OES},
	{ 5,  5,  5,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_5x5x5_OES},
	{ 6,  5,  5,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x5x5_OES},
	{ 6,  6,  5,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x5_OES},
	{ 6,  6,  6,  true, GL_COMPRESSED_SRGB8_ALPHA8_ASTC_6x6x6_OES}
}};

static const format_entry* get_format(
	unsigned int format
) {
	for (auto& it : ASTC_FORMATS)
	{
		if (it.format == format)
		{
			return &it;
		}
	}
	return nullptr;
}

static unsigned int get_format(
	unsigned int x,
	unsigned int y,
	unsigned int z,
	bool is_srgb
) {
	for (auto& it : ASTC_FORMATS)
	{
		if ((it.x == x) && (it.y == y) && (it.z == z)  && (it.is_srgb == is_srgb))
		{
			return it.format;
		}
	}
	return 0;
}

struct ktx_header
{
	uint8_t magic[12];
	uint32_t endianness;				// should be 0x04030201; if it is instead 0x01020304, then the endianness of everything must be switched.
	uint32_t gl_type;					// 0 for compressed textures, otherwise value from table 3.2 (page 162) of OpenGL 4.0 spec
	uint32_t gl_type_size;				// size of data elements to do endianness swap on (1=endian-neutral data)
	uint32_t gl_format;					// 0 for compressed textures, otherwise value from table 3.3 (page 163) of OpenGL spec
	uint32_t gl_internal_format;		// sized-internal-format, corresponding to table 3.12 to 3.14 (pages 182-185) of OpenGL spec
	uint32_t gl_base_internal_format;	// unsized-internal-format: corresponding to table 3.11 (page 179) of OpenGL spec
	uint32_t pixel_width;				// texture dimensions; not rounded up to block size for compressed.
	uint32_t pixel_height;				// must be 0 for 1D textures.
	uint32_t pixel_depth;				// must be 0 for 1D, 2D and cubemap textures.
	uint32_t number_of_array_elements;	// 0 if not a texture array
	uint32_t number_of_faces;			// 6 for cubemaps, 1 for non-cubemaps
	uint32_t number_of_mipmap_levels;	// 0 or 1 for non-mipmapped textures; 0 indicates that auto-mipmap-gen should be done at load time.
	uint32_t bytes_of_key_value_data;	// size in bytes of the key-and-value area immediately following the header.
};

// Magic 12-byte sequence that must appear at the beginning of every KTX file.
static uint8_t ktx_magic[12] {
	0xAB, 0x4B, 0x54, 0x58, 0x20, 0x31, 0x31, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
};

static void ktx_header_switch_endianness(ktx_header * kt)
{
	#define REV(x) kt->x = u32_byterev(kt->x)
	REV(endianness);
	REV(gl_type);
	REV(gl_type_size);
	REV(gl_format);
	REV(gl_internal_format);
	REV(gl_base_internal_format);
	REV(pixel_width);
	REV(pixel_height);
	REV(pixel_depth);
	REV(number_of_array_elements);
	REV(number_of_faces);
	REV(number_of_mipmap_levels);
	REV(bytes_of_key_value_data);
	#undef REV
}

/**
 * @brief Load an uncompressed KTX image using the local custom loader.
 *
 * @param      filename          The name of the file to load.
 * @param      y_flip            Should the image be vertically flipped?
 * @param[out] is_hdr            Is this an HDR image load?
 * @param[out] component_count   The number of components in the data.
 *
 * @return The loaded image data in a canonical 4 channel format, or @c nullptr on error.
 */
static astcenc_image* load_ktx_uncompressed_image(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	FILE *f = fopen(filename, "rb");
	if (!f)
	{
		printf("Failed to open file %s\n", filename);
		return nullptr;
	}

	ktx_header hdr;
	size_t header_bytes_read = fread(&hdr, 1, sizeof(hdr), f);

	if (header_bytes_read != sizeof(hdr))
	{
		printf("Failed to read header of KTX file %s\n", filename);
		fclose(f);
		return nullptr;
	}

	if (memcmp(hdr.magic, ktx_magic, 12) != 0 || (hdr.endianness != 0x04030201 && hdr.endianness != 0x01020304))
	{
		printf("File %s does not have a valid KTX header\n", filename);
		fclose(f);
		return nullptr;
	}

	int switch_endianness = 0;
	if (hdr.endianness == 0x01020304)
	{
		ktx_header_switch_endianness(&hdr);
		switch_endianness = 1;
	}

	if (hdr.gl_type == 0 || hdr.gl_format == 0)
	{
		//printf("File %s appears to be compressed, not supported as input\n", filename);
		fclose(f);
		return nullptr;
	}

	// the formats we support are:

	// Cartesian product of gl_type=(UNSIGNED_BYTE, UNSIGNED_SHORT, HALF_FLOAT, FLOAT) x gl_format=(RED, RG, RGB, RGBA, BGR, BGRA)

	int components;
	switch (hdr.gl_format)
	{
	case GL_RED:
		components = 1;
		break;
	case GL_RG:
		components = 2;
		break;
	case GL_RGB:
		components = 3;
		break;
	case GL_RGBA:
		components = 4;
		break;
	case GL_BGR:
		components = 3;
		break;
	case GL_BGRA:
		components = 4;
		break;
	case GL_LUMINANCE:
		components = 1;
		break;
	case GL_LUMINANCE_ALPHA:
		components = 2;
		break;
	default:
		printf("KTX file %s has unsupported GL type\n", filename);
		fclose(f);
		return nullptr;
	}

	// Although these are set up later, use default initializer to remove warnings
	int bitness = 8;              // Internal precision after conversion
	int bytes_per_component = 1;  // Bytes per component in the KTX file
	scanline_transfer copy_method = R8_TO_RGBA8;

	switch (hdr.gl_type)
	{
	case GL_UNSIGNED_BYTE:
		{
			bitness = 8;
			bytes_per_component = 1;
			switch (hdr.gl_format)
			{
			case GL_RED:
				copy_method = R8_TO_RGBA8;
				break;
			case GL_RG:
				copy_method = RG8_TO_RGBA8;
				break;
			case GL_RGB:
				copy_method = RGB8_TO_RGBA8;
				break;
			case GL_RGBA:
				copy_method = RGBA8_TO_RGBA8;
				break;
			case GL_BGR:
				copy_method = BGR8_TO_RGBA8;
				break;
			case GL_BGRA:
				copy_method = BGRA8_TO_RGBA8;
				break;
			case GL_LUMINANCE:
				copy_method = L8_TO_RGBA8;
				break;
			case GL_LUMINANCE_ALPHA:
				copy_method = LA8_TO_RGBA8;
				break;
			}
			break;
		}
	case GL_UNSIGNED_SHORT:
		{
			bitness = 16;
			bytes_per_component = 2;
			switch (hdr.gl_format)
			{
			case GL_RED:
				copy_method = R16_TO_RGBA16F;
				break;
			case GL_RG:
				copy_method = RG16_TO_RGBA16F;
				break;
			case GL_RGB:
				copy_method = RGB16_TO_RGBA16F;
				break;
			case GL_RGBA:
				copy_method = RGBA16_TO_RGBA16F;
				break;
			case GL_BGR:
				copy_method = BGR16_TO_RGBA16F;
				break;
			case GL_BGRA:
				copy_method = BGRA16_TO_RGBA16F;
				break;
			case GL_LUMINANCE:
				copy_method = L16_TO_RGBA16F;
				break;
			case GL_LUMINANCE_ALPHA:
				copy_method = LA16_TO_RGBA16F;
				break;
			}
			break;
		}
	case GL_HALF_FLOAT:
		{
			bitness = 16;
			bytes_per_component = 2;
			switch (hdr.gl_format)
			{
			case GL_RED:
				copy_method = R16F_TO_RGBA16F;
				break;
			case GL_RG:
				copy_method = RG16F_TO_RGBA16F;
				break;
			case GL_RGB:
				copy_method = RGB16F_TO_RGBA16F;
				break;
			case GL_RGBA:
				copy_method = RGBA16F_TO_RGBA16F;
				break;
			case GL_BGR:
				copy_method = BGR16F_TO_RGBA16F;
				break;
			case GL_BGRA:
				copy_method = BGRA16F_TO_RGBA16F;
				break;
			case GL_LUMINANCE:
				copy_method = L16F_TO_RGBA16F;
				break;
			case GL_LUMINANCE_ALPHA:
				copy_method = LA16F_TO_RGBA16F;
				break;
			}
			break;
		}
	case GL_FLOAT:
		{
			bitness = 16;
			bytes_per_component = 4;
			switch (hdr.gl_format)
			{
			case GL_RED:
				copy_method = R32F_TO_RGBA16F;
				break;
			case GL_RG:
				copy_method = RG32F_TO_RGBA16F;
				break;
			case GL_RGB:
				copy_method = RGB32F_TO_RGBA16F;
				break;
			case GL_RGBA:
				copy_method = RGBA32F_TO_RGBA16F;
				break;
			case GL_BGR:
				copy_method = BGR32F_TO_RGBA16F;
				break;
			case GL_BGRA:
				copy_method = BGRA32F_TO_RGBA16F;
				break;
			case GL_LUMINANCE:
				copy_method = L32F_TO_RGBA16F;
				break;
			case GL_LUMINANCE_ALPHA:
				copy_method = LA32F_TO_RGBA16F;
				break;
			}
			break;
		}
	default:
		printf("KTX file %s has unsupported GL format\n", filename);
		fclose(f);
		return nullptr;
	}

	if (hdr.number_of_mipmap_levels > 1)
	{
		printf("WARNING: KTX file %s has %d mipmap levels; only the first one will be encoded.\n", filename, hdr.number_of_mipmap_levels);
	}

	if (hdr.number_of_array_elements > 1)
	{
		printf("WARNING: KTX file %s contains a texture array with %d layers; only the first one will be encoded.\n", filename, hdr.number_of_array_elements);
	}

	if (hdr.number_of_faces > 1)
	{
		printf("WARNING: KTX file %s contains a cubemap with 6 faces; only the first one will be encoded.\n", filename);
	}


	unsigned int dim_x = hdr.pixel_width;
	unsigned int dim_y = astc::max(hdr.pixel_height, 1u);
	unsigned int dim_z = astc::max(hdr.pixel_depth, 1u);

	// ignore the key/value data
	fseek(f, hdr.bytes_of_key_value_data, SEEK_CUR);

	uint32_t specified_bytes_of_surface = 0;
	size_t sb_read = fread(&specified_bytes_of_surface, 1, 4, f);
	if (sb_read != 4)
	{
		printf("Failed to read header of KTX file %s\n", filename);
		fclose(f);
		return nullptr;
	}

	if (switch_endianness)
	{
		specified_bytes_of_surface = u32_byterev(specified_bytes_of_surface);
	}

	// read the surface
	uint32_t xstride = bytes_per_component * components * dim_x;
	uint32_t ystride = xstride * dim_y;
	uint32_t computed_bytes_of_surface = dim_z * ystride;
	if (computed_bytes_of_surface != specified_bytes_of_surface)
	{
		fclose(f);
		printf("%s: KTX file inconsistency: computed surface size is %d bytes, but specified size is %d bytes\n", filename, computed_bytes_of_surface, specified_bytes_of_surface);
		return nullptr;
	}

	uint8_t *buf = new uint8_t[specified_bytes_of_surface];
	size_t bytes_read = fread(buf, 1, specified_bytes_of_surface, f);
	fclose(f);
	if (bytes_read != specified_bytes_of_surface)
	{
		delete[] buf;
		printf("Failed to read file %s\n", filename);
		return nullptr;
	}

	// perform an endianness swap on the surface if needed.
	if (switch_endianness)
	{
		if (hdr.gl_type_size == 2)
		{
			switch_endianness2(buf, specified_bytes_of_surface);
		}

		if (hdr.gl_type_size == 4)
		{
			switch_endianness4(buf, specified_bytes_of_surface);
		}
	}

	// Transfer data from the surface to our own image data structure
	astcenc_image *astc_img = alloc_image(bitness, dim_x, dim_y, dim_z);

	for (unsigned int z = 0; z < dim_z; z++)
	{
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			unsigned int ydst = ymod;
			void *dst;

			if (astc_img->data_type == ASTCENC_TYPE_U8)
			{
				uint8_t* data8 = static_cast<uint8_t*>(astc_img->data[z]);
				dst = static_cast<void*>(&data8[4 * dim_x * ydst]);
			}
			else // if (astc_img->data_type == ASTCENC_TYPE_F16)
			{
				assert(astc_img->data_type == ASTCENC_TYPE_F16);
				uint16_t* data16 = static_cast<uint16_t*>(astc_img->data[z]);
				dst = static_cast<void*>(&data16[4 * dim_x * ydst]);
			}

			uint8_t *src = buf + (z * ystride) + (y * xstride);
			copy_scanline(dst, src, dim_x, copy_method);
		}
	}

	delete[] buf;
	is_hdr = bitness >= 16;
	component_count = components;
	return astc_img;
}

/**
 * @brief Load a KTX compressed image using the local custom loader.
 *
 * @param      filename          The name of the file to load.
 * @param[out] is_srgb           @c true if this is an sRGB image, @c false otherwise.
 * @param[out] img               The output image to populate.
 *
 * @return @c true on error, @c false otherwise.
 */
bool load_ktx_compressed_image(
	const char* filename,
	bool& is_srgb,
	astc_compressed_image& img
) {
	FILE *f = fopen(filename, "rb");
	if (!f)
	{
		printf("Failed to open file %s\n", filename);
		return true;
	}

	ktx_header hdr;
	size_t actual = fread(&hdr, 1, sizeof(hdr), f);
	if (actual != sizeof(hdr))
	{
		printf("Failed to read header from %s\n", filename);
		fclose(f);
		return true;
	}

	if (memcmp(hdr.magic, ktx_magic, 12) != 0 ||
	    (hdr.endianness != 0x04030201 && hdr.endianness != 0x01020304))
	{
		printf("File %s does not have a valid KTX header\n", filename);
		fclose(f);
		return true;
	}

	bool switch_endianness = false;
	if (hdr.endianness == 0x01020304)
	{
		switch_endianness = true;
		ktx_header_switch_endianness(&hdr);
	}

	if (hdr.gl_type != 0 || hdr.gl_format != 0 || hdr.gl_type_size != 1 ||
	    hdr.gl_base_internal_format != GL_RGBA)
	{
		printf("File %s is not a compressed ASTC file\n", filename);
		fclose(f);
		return true;
	}

	const format_entry* fmt = get_format(hdr.gl_internal_format);
	if (!fmt)
	{
		printf("File %s is not a compressed ASTC file\n", filename);
		fclose(f);
		return true;
	}

	// Skip over any key-value pairs
	int seekerr;
	seekerr = fseek(f, hdr.bytes_of_key_value_data, SEEK_CUR);
	if (seekerr)
	{
		printf("Failed to skip key-value pairs in %s\n", filename);
		fclose(f);
		return true;
	}

	// Read the length of the data and endianess convert
	unsigned int data_len;
	actual = fread(&data_len, 1, sizeof(data_len), f);
	if (actual != sizeof(data_len))
	{
		printf("Failed to read mip 0 size from %s\n", filename);
		fclose(f);
		return true;
	}

	if (switch_endianness)
	{
		data_len = u32_byterev(data_len);
	}

	// Read the data
	unsigned char* data = new unsigned char[data_len];
	actual = fread(data, 1, data_len, f);
	if (actual != data_len)
	{
		printf("Failed to read mip 0 data from %s\n", filename);
		fclose(f);
		delete[] data;
		return true;
	}

	img.block_x = fmt->x;
	img.block_y = fmt->y;
	img.block_z = fmt->z == 0 ? 1 : fmt->z;

	img.dim_x = hdr.pixel_width;
	img.dim_y = hdr.pixel_height;
	img.dim_z = hdr.pixel_depth == 0 ? 1 : hdr.pixel_depth;

	img.data_len = data_len;
	img.data = data;

	is_srgb = fmt->is_srgb;

	fclose(f);
	return false;
}

/**
 * @brief Store a KTX compressed image using a local store routine.
 *
 * @param img        The image data to store.
 * @param filename   The name of the file to save.
 * @param is_srgb    @c true if this is an sRGB image, @c false if linear.
 *
 * @return @c true on error, @c false otherwise.
 */
bool store_ktx_compressed_image(
	const astc_compressed_image& img,
	const char* filename,
	bool is_srgb
) {
	unsigned int fmt = get_format(img.block_x, img.block_y, img.block_z, is_srgb);

	ktx_header hdr;
	memcpy(hdr.magic, ktx_magic, 12);
	hdr.endianness = 0x04030201;
	hdr.gl_type = 0;
	hdr.gl_type_size = 1;
	hdr.gl_format = 0;
	hdr.gl_internal_format = fmt;
	hdr.gl_base_internal_format = GL_RGBA;
	hdr.pixel_width = img.dim_x;
	hdr.pixel_height = img.dim_y;
	hdr.pixel_depth = (img.dim_z == 1) ? 0 : img.dim_z;
	hdr.number_of_array_elements = 0;
	hdr.number_of_faces = 1;
	hdr.number_of_mipmap_levels = 1;
	hdr.bytes_of_key_value_data = 0;

	size_t expected = sizeof(ktx_header) + 4 + img.data_len;
	size_t actual = 0;

	FILE *wf = fopen(filename, "wb");
	if (!wf)
	{
		return true;
	}

	actual += fwrite(&hdr, 1, sizeof(ktx_header), wf);
	actual += fwrite(&img.data_len, 1, 4, wf);
	actual += fwrite(img.data, 1, img.data_len, wf);
	fclose(wf);

	if (actual != expected)
	{
		return true;
	}

	return false;
}

/**
 * @brief Save a KTX uncompressed image using a local store routine.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
static bool store_ktx_uncompressed_image(
	const astcenc_image* img,
	const char* filename,
	int y_flip
) {
	unsigned int dim_x = img->dim_x;
	unsigned int dim_y = img->dim_y;
	unsigned int dim_z = img->dim_z;

	int bitness = img->data_type == ASTCENC_TYPE_U8 ? 8 : 16;
	int image_components = determine_image_components(img);

	ktx_header hdr;

	static const int gl_format_of_components[4] {
		GL_RED, GL_RG, GL_RGB, GL_RGBA
	};

	static const int gl_sized_format_of_components_ldr[4] {
		GL_R8, GL_RG8, GL_RGB8, GL_RGBA8
	};

	static const int gl_sized_format_of_components_hdr[4] {
		GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F
	};

	memcpy(hdr.magic, ktx_magic, 12);
	hdr.endianness = 0x04030201;
	hdr.gl_type = (bitness == 16) ? GL_HALF_FLOAT : GL_UNSIGNED_BYTE;
	hdr.gl_type_size = bitness / 8;
	hdr.gl_format = gl_format_of_components[image_components - 1];
	if (bitness == 16)
	{
		hdr.gl_internal_format = gl_sized_format_of_components_hdr[image_components - 1];
	}
	else
	{
		hdr.gl_internal_format = gl_sized_format_of_components_ldr[image_components - 1];
	}
	hdr.gl_base_internal_format = hdr.gl_format;
	hdr.pixel_width = dim_x;
	hdr.pixel_height = dim_y;
	hdr.pixel_depth = (dim_z == 1) ? 0 : dim_z;
	hdr.number_of_array_elements = 0;
	hdr.number_of_faces = 1;
	hdr.number_of_mipmap_levels = 1;
	hdr.bytes_of_key_value_data = 0;

	// Collect image data to write
	uint8_t ***row_pointers8 = nullptr;
	uint16_t ***row_pointers16 = nullptr;
	if (bitness == 8)
	{
		row_pointers8 = new uint8_t **[dim_z];
		row_pointers8[0] = new uint8_t *[dim_y * dim_z];
		row_pointers8[0][0] = new uint8_t[dim_x * dim_y * dim_z * image_components + 3];

		for (unsigned int z = 1; z < dim_z; z++)
		{
			row_pointers8[z] = row_pointers8[0] + dim_y * z;
			row_pointers8[z][0] = row_pointers8[0][0] + dim_y * dim_x * image_components * z;
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			for (unsigned int y = 1; y < dim_y; y++)
			{
				row_pointers8[z][y] = row_pointers8[z][0] + dim_x * image_components * y;
			}
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			uint8_t* data8 = static_cast<uint8_t*>(img->data[z]);
			for (unsigned int y = 0; y < dim_y; y++)
			{
				int ym = y_flip ? dim_y - y - 1 : y;
				switch (image_components)
				{
				case 1:		// single-component, treated as Luminance
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][x] = data8[(4 * dim_x * ym) + (4 * x    )];
					}
					break;
				case 2:		// two-component, treated as Luminance-Alpha
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][2 * x    ] = data8[(4 * dim_x * ym) + (4 * x    )];
						row_pointers8[z][y][2 * x + 1] = data8[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				case 3:		// three-component, treated a
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][3 * x    ] = data8[(4 * dim_x * ym) + (4 * x    )];
						row_pointers8[z][y][3 * x + 1] = data8[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers8[z][y][3 * x + 2] = data8[(4 * dim_x * ym) + (4 * x + 2)];
					}
					break;
				case 4:		// four-component, treated as RGBA
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][4 * x    ] = data8[(4 * dim_x * ym) + (4 * x    )];
						row_pointers8[z][y][4 * x + 1] = data8[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers8[z][y][4 * x + 2] = data8[(4 * dim_x * ym) + (4 * x + 2)];
						row_pointers8[z][y][4 * x + 3] = data8[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				}
			}
		}
	}
	else						// if bitness == 16
	{
		row_pointers16 = new uint16_t **[dim_z];
		row_pointers16[0] = new uint16_t *[dim_y * dim_z];
		row_pointers16[0][0] = new uint16_t[dim_x * dim_y * dim_z * image_components + 1];

		for (unsigned int z = 1; z < dim_z; z++)
		{
			row_pointers16[z] = row_pointers16[0] + dim_y * z;
			row_pointers16[z][0] = row_pointers16[0][0] + dim_y * dim_x * image_components * z;
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			for (unsigned int y = 1; y < dim_y; y++)
			{
				row_pointers16[z][y] = row_pointers16[z][0] + dim_x * image_components * y;
			}
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			uint16_t* data16 = static_cast<uint16_t*>(img->data[z]);
			for (unsigned int y = 0; y < dim_y; y++)
			{
				int ym = y_flip ? dim_y - y - 1 : y;
				switch (image_components)
				{
				case 1:		// single-component, treated as Luminance
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][x] = data16[(4 * dim_x * ym) + (4 * x    )];
					}
					break;
				case 2:		// two-component, treated as Luminance-Alpha
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][2 * x    ] = data16[(4 * dim_x * ym) + (4 * x    )];
						row_pointers16[z][y][2 * x + 1] = data16[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				case 3:		// three-component, treated as RGB
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][3 * x    ] = data16[(4 * dim_x * ym) + (4 * x    )];
						row_pointers16[z][y][3 * x + 1] = data16[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers16[z][y][3 * x + 2] = data16[(4 * dim_x * ym) + (4 * x + 2)];
					}
					break;
				case 4:		// four-component, treated as RGBA
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][4 * x    ] = data16[(4 * dim_x * ym) + (4 * x    )];
						row_pointers16[z][y][4 * x + 1] = data16[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers16[z][y][4 * x + 2] = data16[(4 * dim_x * ym) + (4 * x + 2)];
						row_pointers16[z][y][4 * x + 3] = data16[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				}
			}
		}
	}

	bool retval { true };
	uint32_t image_bytes = dim_x * dim_y * dim_z * image_components * (bitness / 8);
	uint32_t image_write_bytes = (image_bytes + 3) & ~3;

	FILE *wf = fopen(filename, "wb");
	if (wf)
	{
		void* dataptr = (bitness == 16) ?
			reinterpret_cast<void*>(row_pointers16[0][0]) :
			reinterpret_cast<void*>(row_pointers8[0][0]);

		size_t expected_bytes_written = sizeof(ktx_header) + image_write_bytes + 4;
		size_t hdr_bytes_written = fwrite(&hdr, 1, sizeof(ktx_header), wf);
		size_t bytecount_bytes_written = fwrite(&image_bytes, 1, 4, wf);
		size_t data_bytes_written = fwrite(dataptr, 1, image_write_bytes, wf);
		fclose(wf);
		if (hdr_bytes_written + bytecount_bytes_written + data_bytes_written != expected_bytes_written)
		{
			retval = false;
		}
	}
	else
	{
		retval = false;
	}

	if (row_pointers8)
	{
		delete[] row_pointers8[0][0];
		delete[] row_pointers8[0];
		delete[] row_pointers8;
	}

	if (row_pointers16)
	{
		delete[] row_pointers16[0][0];
		delete[] row_pointers16[0];
		delete[] row_pointers16;
	}

	return retval;
}

/*
	Loader for DDS files.

	Note that after the header, data are densely packed with no padding;
	in the case of multiple surfaces, they appear one after another in
	the file, again with no padding.

	This code is NOT endian-neutral.
*/
struct dds_pixelformat
{
	uint32_t size;				// structure size, set to 32.
	/*
	   flags bits are a combination of the following: 0x1 : Texture contains alpha data 0x2 : ---- (older files: texture contains alpha data, for Alpha-only texture) 0x4 : The fourcc field is valid,
	   indicating a compressed or DX10 texture format 0x40 : texture contains uncompressed RGB data 0x200 : ---- (YUV in older files) 0x20000 : Texture contains Luminance data (can be combined with
	   0x1 for Lum-Alpha) */
	uint32_t flags;
	uint32_t fourcc;			// "DX10" to indicate a DX10 format, "DXTn" for the DXT formats
	uint32_t rgbbitcount;		// number of bits per texel; up to 32 for non-DX10 formats.
	uint32_t rbitmask;			// bitmap indicating position of red/luminance color component
	uint32_t gbitmask;			// bitmap indicating position of green color component
	uint32_t bbitmask;			// bitmap indicating position of blue color component
	uint32_t abitmask;			// bitmap indicating position of alpha color component
};

struct dds_header
{
	uint32_t size;				// header size; must be exactly 124.
	/*
	   flag field is an OR or the following bits, that indicate fields containing valid data:
		1: caps/caps2/caps3/caps4 (set in all DDS files, ignore on read)
		2: height (set in all DDS files, ignore on read)
		4: width (set in all DDS files, ignore on read)
		8: pitch (for uncompressed texture)
		0x1000: the pixel format field (set in all DDS files, ignore on read)
		0x20000: mipmap count (for mipmapped textures with >1 level)
		0x80000: pitch (for compressed texture)
		0x800000: depth (for 3d textures)
	*/
	uint32_t flags;
	uint32_t height;
	uint32_t width;
	uint32_t pitch_or_linear_size;	// scanline pitch for uncompressed; total size in bytes for compressed
	uint32_t depth;
	uint32_t mipmapcount;
	// unused, set to 0
	uint32_t reserved1[11];
	dds_pixelformat ddspf;
	/*
	   caps field is an OR of the following values:
		8 : should be set for a file that contains more than 1 surface (ignore on read)
		0x400000 : should be set for a mipmapped texture
		0x1000 : should be set if the surface is a texture at all (all DDS files, ignore on read)
	*/
	uint32_t caps;
	/*
	   caps2 field is an OR of the following values:
		0x200 : texture is cubemap
		0x400 : +X face of cubemap is present
		0x800 : -X face of cubemap is present
		0x1000 : +Y face of cubemap is present
		0x2000 : -Y face of cubemap is present
		0x4000 : +Z face of cubemap is present
		0x8000 : -Z face of cubemap is present
		0x200000 : texture is a 3d texture.
	*/
	uint32_t caps2;
	// unused, set to 0
	uint32_t caps3;
	// unused, set to 0
	uint32_t caps4;
	// unused, set to 0
	uint32_t reserved2;
};

struct dds_header_dx10
{
	uint32_t dxgi_format;
	uint32_t resource_dimension;	// 2=1d-texture, 3=2d-texture or cubemap, 4=3d-texture
	uint32_t misc_flag;			// 4 if cubemap, else 0
	uint32_t array_size;		// size of array in case of a texture array; set to 1 for a non-array
	uint32_t reserved;			// set to 0.
};

#define DDS_MAGIC 0x20534444
#define DX10_MAGIC 0x30315844

/**
 * @brief Load an uncompressed DDS image using the local custom loader.
 *
 * @param      filename          The name of the file to load.
 * @param      y_flip            Should the image be vertically flipped?
 * @param[out] is_hdr            Is this an HDR image load?
 * @param[out] component_count   The number of components in the data.
 *
 * @return The loaded image data in a canonical 4 channel format, or @c nullptr on error.
 */
static astcenc_image* load_dds_uncompressed_image(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	FILE *f = fopen(filename, "rb");
	if (!f)
	{
		printf("Failed to open file %s\n", filename);
		return nullptr;
	}

	uint8_t magic[4];

	dds_header hdr;
	size_t magic_bytes_read = fread(magic, 1, 4, f);
	size_t header_bytes_read = fread(&hdr, 1, sizeof(hdr), f);
	if (magic_bytes_read != 4 || header_bytes_read != sizeof(hdr))
	{
		printf("Failed to read header of DDS file %s\n", filename);
		fclose(f);
		return nullptr;
	}

	uint32_t magicx = magic[0] | (magic[1] << 8) | (magic[2] << 16) | (magic[3] << 24);

	if (magicx != DDS_MAGIC || hdr.size != 124)
	{
		printf("File %s does not have a valid DDS header\n", filename);
		fclose(f);
		return nullptr;
	}

	int use_dx10_header = 0;
	if (hdr.ddspf.flags & 4)
	{
		if (hdr.ddspf.fourcc == DX10_MAGIC)
		{
			use_dx10_header = 1;
		}
		else
		{
			printf("DDS file %s is compressed, not supported\n", filename);
			fclose(f);
			return nullptr;
		}
	}

	dds_header_dx10 dx10_header;
	if (use_dx10_header)
	{
		size_t dx10_header_bytes_read = fread(&dx10_header, 1, sizeof(dx10_header), f);
		if (dx10_header_bytes_read != sizeof(dx10_header))
		{
			printf("Failed to read header of DDS file %s\n", filename);
			fclose(f);
			return nullptr;
		}
	}

	unsigned int dim_x = hdr.width;
	unsigned int dim_y = hdr.height;
	unsigned int dim_z = (hdr.flags & 0x800000) ? hdr.depth : 1;

	// The bitcount that we will use internally in the codec
	int bitness = 0;

	// The bytes per component in the DDS file itself
	int bytes_per_component = 0;
	int components = 0;
	scanline_transfer copy_method = R8_TO_RGBA8;

	// figure out the format actually used in the DDS file.
	if (use_dx10_header)
	{
		// DX10 header present; use the DXGI format.
		#define DXGI_FORMAT_R32G32B32A32_FLOAT   2
		#define DXGI_FORMAT_R32G32B32_FLOAT      6
		#define DXGI_FORMAT_R16G16B16A16_FLOAT  10
		#define DXGI_FORMAT_R16G16B16A16_UNORM  11
		#define DXGI_FORMAT_R32G32_FLOAT        16
		#define DXGI_FORMAT_R8G8B8A8_UNORM      28
		#define DXGI_FORMAT_R16G16_FLOAT    34
		#define DXGI_FORMAT_R16G16_UNORM    35
		#define DXGI_FORMAT_R32_FLOAT       41
		#define DXGI_FORMAT_R8G8_UNORM      49
		#define DXGI_FORMAT_R16_FLOAT       54
		#define DXGI_FORMAT_R16_UNORM       56
		#define DXGI_FORMAT_R8_UNORM        61
		#define DXGI_FORMAT_B8G8R8A8_UNORM  86
		#define DXGI_FORMAT_B8G8R8X8_UNORM  87

		struct dxgi_params
		{
			int bitness;
			int bytes_per_component;
			int components;
			scanline_transfer copy_method;
			uint32_t dxgi_format_number;
		};

		static const dxgi_params format_params[] {
			{16, 4, 4, RGBA32F_TO_RGBA16F, DXGI_FORMAT_R32G32B32A32_FLOAT},
			{16, 4, 3, RGB32F_TO_RGBA16F, DXGI_FORMAT_R32G32B32_FLOAT},
			{16, 2, 4, RGBA16F_TO_RGBA16F, DXGI_FORMAT_R16G16B16A16_FLOAT},
			{16, 2, 4, RGBA16_TO_RGBA16F, DXGI_FORMAT_R16G16B16A16_UNORM},
			{16, 4, 2, RG32F_TO_RGBA16F, DXGI_FORMAT_R32G32_FLOAT},
			{8, 1, 4, RGBA8_TO_RGBA8, DXGI_FORMAT_R8G8B8A8_UNORM},
			{16, 2, 2, RG16F_TO_RGBA16F, DXGI_FORMAT_R16G16_FLOAT},
			{16, 2, 2, RG16_TO_RGBA16F, DXGI_FORMAT_R16G16_UNORM},
			{16, 4, 1, R32F_TO_RGBA16F, DXGI_FORMAT_R32_FLOAT},
			{8, 1, 2, RG8_TO_RGBA8, DXGI_FORMAT_R8G8_UNORM},
			{16, 2, 1, R16F_TO_RGBA16F, DXGI_FORMAT_R16_FLOAT},
			{16, 2, 1, R16_TO_RGBA16F, DXGI_FORMAT_R16_UNORM},
			{8, 1, 1, R8_TO_RGBA8, DXGI_FORMAT_R8_UNORM},
			{8, 1, 4, BGRA8_TO_RGBA8, DXGI_FORMAT_B8G8R8A8_UNORM},
			{8, 1, 4, BGRX8_TO_RGBA8, DXGI_FORMAT_B8G8R8X8_UNORM},
		};

		int dxgi_modes_supported = sizeof(format_params) / sizeof(format_params[0]);
		int did_select_format = 0;
		for (int i = 0; i < dxgi_modes_supported; i++)
		{
			if (dx10_header.dxgi_format == format_params[i].dxgi_format_number)
			{
				bitness = format_params[i].bitness;
				bytes_per_component = format_params[i].bytes_per_component;
				components = format_params[i].components;
				copy_method = format_params[i].copy_method;
				did_select_format = 1;
				break;
			}
		}

		if (!did_select_format)
		{
			printf("DDS file %s: DXGI format not supported by codec\n", filename);
			fclose(f);
			return nullptr;
		}
	}
	else
	{
		// No DX10 header present. Then try to match the bitcount and bitmask against
		// a set of prepared patterns.
		uint32_t flags = hdr.ddspf.flags;
		uint32_t bitcount = hdr.ddspf.rgbbitcount;
		uint32_t rmask = hdr.ddspf.rbitmask;
		uint32_t gmask = hdr.ddspf.gbitmask;
		uint32_t bmask = hdr.ddspf.bbitmask;
		uint32_t amask = hdr.ddspf.abitmask;

		// RGBA-unorm8
		if ((flags & 0x41) == 0x41 && bitcount == 32 && rmask == 0xFF && gmask == 0xFF00 && bmask == 0xFF0000 && amask == 0xFF000000)
		{
			bytes_per_component = 1;
			components = 4;
			copy_method = RGBA8_TO_RGBA8;
		}
		// BGRA-unorm8
		else if ((flags & 0x41) == 0x41 && bitcount == 32 && rmask == 0xFF0000 && gmask == 0xFF00 && bmask == 0xFF && amask == 0xFF000000)
		{
			bytes_per_component = 1;
			components = 4;
			copy_method = BGRA8_TO_RGBA8;
		}
		// RGBX-unorm8
		else if ((flags & 0x40) && bitcount == 32 && rmask == 0xFF && gmask == 0xFF00 && bmask == 0xFF0000)
		{
			bytes_per_component = 1;
			components = 4;
			copy_method = RGBX8_TO_RGBA8;
		}
		// BGRX-unorm8
		else if ((flags & 0x40) && bitcount == 32 && rmask == 0xFF0000 && gmask == 0xFF00 && bmask == 0xFF)
		{
			bytes_per_component = 1;
			components = 4;
			copy_method = BGRX8_TO_RGBA8;
		}
		// RGB-unorm8
		else if ((flags & 0x40) && bitcount == 24 && rmask == 0xFF && gmask == 0xFF00 && bmask == 0xFF0000)
		{
			bytes_per_component = 1;
			components = 3;
			copy_method = RGB8_TO_RGBA8;
		}
		// BGR-unorm8
		else if ((flags & 0x40) && bitcount == 24 && rmask == 0xFF0000 && gmask == 0xFF00 && bmask == 0xFF)
		{
			bytes_per_component = 1;
			components = 3;
			copy_method = BGR8_TO_RGBA8;
		}
		// RG-unorm16
		else if ((flags & 0x40) && bitcount == 16 && rmask == 0xFFFF && gmask == 0xFFFF0000)
		{
			bytes_per_component = 2;
			components = 2;
			copy_method = RG16_TO_RGBA16F;
		}
		// A8L8
		else if ((flags & 0x20001) == 0x20001 && bitcount == 16 && rmask == 0xFF && amask == 0xFF00)
		{
			bytes_per_component = 1;
			components = 2;
			copy_method = LA8_TO_RGBA8;
		}
		// L8
		else if ((flags & 0x20000) && bitcount == 8 && rmask == 0xFF)
		{
			bytes_per_component = 1;
			components = 1;
			copy_method = L8_TO_RGBA8;
		}
		// L16
		else if ((flags & 0x20000) && bitcount == 16 && rmask == 0xFFFF)
		{
			bytes_per_component = 2;
			components = 1;
			copy_method = L16_TO_RGBA16F;
		}
		else
		{
			printf("DDS file %s: Non-DXGI format not supported by codec\n", filename);
			fclose(f);
			return nullptr;
		}

		bitness = bytes_per_component * 8;
	}

	// then, load the actual file.
	uint32_t xstride = bytes_per_component * components * dim_x;
	uint32_t ystride = xstride * dim_y;
	uint32_t bytes_of_surface = ystride * dim_z;

	uint8_t *buf = new uint8_t[bytes_of_surface];
	size_t bytes_read = fread(buf, 1, bytes_of_surface, f);
	fclose(f);
	if (bytes_read != bytes_of_surface)
	{
		delete[] buf;
		printf("Failed to read file %s\n", filename);
		return nullptr;
	}

	// then transfer data from the surface to our own image-data-structure.
	astcenc_image *astc_img = alloc_image(bitness, dim_x, dim_y, dim_z);

	for (unsigned int z = 0; z < dim_z; z++)
	{
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			unsigned int ydst = ymod;
			void* dst;

			if (astc_img->data_type == ASTCENC_TYPE_U8)
			{
				uint8_t* data8 = static_cast<uint8_t*>(astc_img->data[z]);
				dst = static_cast<void*>(&data8[4 * dim_x * ydst]);
			}
			else // if (astc_img->data_type == ASTCENC_TYPE_F16)
			{
				assert(astc_img->data_type == ASTCENC_TYPE_F16);
				uint16_t* data16 = static_cast<uint16_t*>(astc_img->data[z]);
				dst = static_cast<void*>(&data16[4 * dim_x * ydst]);
			}

			uint8_t *src = buf + (z * ystride) + (y * xstride);
			copy_scanline(dst, src, dim_x, copy_method);
		}
	}

	delete[] buf;
	is_hdr = bitness >= 16;
	component_count = components;
	return astc_img;
}

/**
 * @brief Save a DDS uncompressed image using a local store routine.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
static bool store_dds_uncompressed_image(
	const astcenc_image* img,
	const char* filename,
	int y_flip
) {
	unsigned int dim_x = img->dim_x;
	unsigned int dim_y = img->dim_y;
	unsigned int dim_z = img->dim_z;

	int bitness = img->data_type == ASTCENC_TYPE_U8 ? 8 : 16;
	int image_components = (bitness == 16) ? 4 : determine_image_components(img);

	// DDS-pixel-format structures to use when storing LDR image with 1,2,3 or 4 components.
	static const dds_pixelformat format_of_image_components[4] =
	{
		{32, 0x20000, 0, 8, 0xFF, 0, 0, 0},	// luminance
		{32, 0x20001, 0, 16, 0xFF, 0, 0, 0xFF00},	// L8A8
		{32, 0x40, 0, 24, 0xFF, 0xFF00, 0xFF0000, 0},	// RGB8
		{32, 0x41, 0, 32, 0xFF, 0xFF00, 0xFF0000, 0xFF000000}	// RGBA8
	};

	// DDS-pixel-format structures to use when storing HDR image.
	static const dds_pixelformat dxt10_diverter =
	{
		32, 4, DX10_MAGIC, 0, 0, 0, 0, 0
	};

	// Header handling; will write:
	// * DDS magic value
	// * DDS header
	// * DDS DX10 header, if the file is floating-point
	// * pixel data

	// Main header data
	dds_header hdr;
	hdr.size = 124;
	hdr.flags = 0x100F | (dim_z > 1 ? 0x800000 : 0);
	hdr.height = dim_y;
	hdr.width = dim_x;
	hdr.pitch_or_linear_size = image_components * (bitness / 8) * dim_x;
	hdr.depth = dim_z;
	hdr.mipmapcount = 1;
	for (unsigned int i = 0; i < 11; i++)
	{
		hdr.reserved1[i] = 0;
	}
	hdr.caps = 0x1000;
	hdr.caps2 = (dim_z > 1) ? 0x200000 : 0;
	hdr.caps3 = 0;
	hdr.caps4 = 0;

	// Pixel-format data
	if (bitness == 8)
	{
		hdr.ddspf = format_of_image_components[image_components - 1];
	}
	else
	{
		hdr.ddspf = dxt10_diverter;
	}

	// DX10 data
	dds_header_dx10 dx10;
	dx10.dxgi_format = DXGI_FORMAT_R16G16B16A16_FLOAT;
	dx10.resource_dimension = (dim_z > 1) ? 4 : 3;
	dx10.misc_flag = 0;
	dx10.array_size = 1;
	dx10.reserved = 0;

	// Collect image data to write
	uint8_t ***row_pointers8 = nullptr;
	uint16_t ***row_pointers16 = nullptr;

	if (bitness == 8)
	{
		row_pointers8 = new uint8_t **[dim_z];
		row_pointers8[0] = new uint8_t *[dim_y * dim_z];
		row_pointers8[0][0] = new uint8_t[dim_x * dim_y * dim_z * image_components];

		for (unsigned int z = 1; z < dim_z; z++)
		{
			row_pointers8[z] = row_pointers8[0] + dim_y * z;
			row_pointers8[z][0] = row_pointers8[0][0] + dim_y * dim_z * image_components * z;
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			for (unsigned int y = 1; y < dim_y; y++)
			{
				row_pointers8[z][y] = row_pointers8[z][0] + dim_x * image_components * y;
			}
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			uint8_t* data8 = static_cast<uint8_t*>(img->data[z]);

			for (unsigned int y = 0; y < dim_y; y++)
			{
				int ym = y_flip ? dim_y - y - 1 : y;
				switch (image_components)
				{
				case 1:		// single-component, treated as Luminance
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][x] = data8[(4 * dim_x * ym) + (4 * x    )];
					}
					break;
				case 2:		// two-component, treated as Luminance-Alpha
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][2 * x    ] = data8[(4 * dim_x * ym) + (4 * x    )];
						row_pointers8[z][y][2 * x + 1] = data8[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				case 3:		// three-component, treated as RGB
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][3 * x    ] = data8[(4 * dim_x * ym) + (4 * x    )];
						row_pointers8[z][y][3 * x + 1] = data8[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers8[z][y][3 * x + 2] = data8[(4 * dim_x * ym) + (4 * x + 2)];
					}
					break;
				case 4:		// four-component, treated as RGBA
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers8[z][y][4 * x    ] = data8[(4 * dim_x * ym) + (4 * x    )];
						row_pointers8[z][y][4 * x + 1] = data8[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers8[z][y][4 * x + 2] = data8[(4 * dim_x * ym) + (4 * x + 2)];
						row_pointers8[z][y][4 * x + 3] = data8[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				}
			}
		}
	}
	else						// if bitness == 16
	{
		row_pointers16 = new uint16_t **[dim_z];
		row_pointers16[0] = new uint16_t *[dim_y * dim_z];
		row_pointers16[0][0] = new uint16_t[dim_x * dim_y * dim_z * image_components];

		for (unsigned int z = 1; z < dim_z; z++)
		{
			row_pointers16[z] = row_pointers16[0] + dim_y * z;
			row_pointers16[z][0] = row_pointers16[0][0] + dim_y * dim_x * image_components * z;
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			for (unsigned int y = 1; y < dim_y; y++)
			{
				row_pointers16[z][y] = row_pointers16[z][0] + dim_x * image_components * y;
			}
		}

		for (unsigned int z = 0; z < dim_z; z++)
		{
			uint16_t* data16 = static_cast<uint16_t*>(img->data[z]);

			for (unsigned int y = 0; y < dim_y; y++)
			{
				int ym = y_flip ? dim_y - y - 1: y;
				switch (image_components)
				{
				case 1:		// single-component, treated as Luminance
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][x] = data16[(4 * dim_x * ym) + (4 * x    )];
					}
					break;
				case 2:		// two-component, treated as Luminance-Alpha
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][2 * x    ] = data16[(4 * dim_x * ym) + (4 * x    )];
						row_pointers16[z][y][2 * x + 1] = data16[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				case 3:		// three-component, treated as RGB
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][3 * x    ] = data16[(4 * dim_x * ym) + (4 * x    )];
						row_pointers16[z][y][3 * x + 1] = data16[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers16[z][y][3 * x + 2] = data16[(4 * dim_x * ym) + (4 * x + 2)];
					}
					break;
				case 4:		// four-component, treated as RGBA
					for (unsigned int x = 0; x < dim_x; x++)
					{
						row_pointers16[z][y][4 * x    ] = data16[(4 * dim_x * ym) + (4 * x    )];
						row_pointers16[z][y][4 * x + 1] = data16[(4 * dim_x * ym) + (4 * x + 1)];
						row_pointers16[z][y][4 * x + 2] = data16[(4 * dim_x * ym) + (4 * x + 2)];
						row_pointers16[z][y][4 * x + 3] = data16[(4 * dim_x * ym) + (4 * x + 3)];
					}
					break;
				}
			}
		}
	}

	bool retval { true };
	uint32_t image_bytes = dim_x * dim_y * dim_z * image_components * (bitness / 8);

	uint32_t dds_magic = DDS_MAGIC;

	FILE *wf = fopen(filename, "wb");
	if (wf)
	{
		void *dataptr = (bitness == 16) ?
			reinterpret_cast<void*>(row_pointers16[0][0]) :
			reinterpret_cast<void*>(row_pointers8[0][0]);

		size_t expected_bytes_written = 4 + sizeof(dds_header) + (bitness > 8 ? sizeof(dds_header_dx10) : 0) + image_bytes;

		size_t magic_bytes_written = fwrite(&dds_magic, 1, 4, wf);
		size_t hdr_bytes_written = fwrite(&hdr, 1, sizeof(dds_header), wf);

		size_t dx10_bytes_written;
		if (bitness > 8)
		{
			dx10_bytes_written = fwrite(&dx10, 1, sizeof(dx10), wf);
		}
		else
		{
			dx10_bytes_written = 0;
		}

		size_t data_bytes_written = fwrite(dataptr, 1, image_bytes, wf);

		fclose(wf);
		if (magic_bytes_written + hdr_bytes_written + dx10_bytes_written + data_bytes_written != expected_bytes_written)
		{
			retval = false;
		}
	}
	else
	{
		retval = false;
	}

	if (row_pointers8)
	{
		delete[] row_pointers8[0][0];
		delete[] row_pointers8[0];
		delete[] row_pointers8;
	}

	if (row_pointers16)
	{
		delete[] row_pointers16[0][0];
		delete[] row_pointers16[0];
		delete[] row_pointers16;
	}

	return retval;
}

/**
 * @brief Supported uncompressed image load functions, and their associated file extensions.
 */
static const struct
{
	const char* ending1;
	const char* ending2;
	astcenc_image* (*loader_func)(const char*, bool, bool&, unsigned int&);
} loader_descs[] {
	// LDR formats
	{".png",   ".PNG",  load_png_with_wuffs},
	// HDR formats
	{".exr",   ".EXR",  load_image_with_tinyexr },
	// Container formats
	{".ktx",   ".KTX",  load_ktx_uncompressed_image },
	{".dds",   ".DDS",  load_dds_uncompressed_image },
	// Generic catch all; this one must be last in the list
	{ nullptr, nullptr, load_image_with_stb }
};

static const int loader_descr_count = sizeof(loader_descs) / sizeof(loader_descs[0]);

/**
 * @brief Supported uncompressed image store functions, and their associated file extensions.
 */
static const struct
{
	const char *ending1;
	const char *ending2;
	int enforced_bitness;
	bool (*storer_func)(const astcenc_image *output_image, const char *filename, int y_flip);
} storer_descs[] {
	// LDR formats
	{".bmp", ".BMP",  8, store_bmp_image_with_stb},
	{".png", ".PNG",  8, store_png_image_with_stb},
	{".tga", ".TGA",  8, store_tga_image_with_stb},
	// HDR formats
	{".exr", ".EXR", 16, store_exr_image_with_tinyexr},
	{".hdr", ".HDR", 16, store_hdr_image_with_stb},
	// Container formats
	{".dds", ".DDS",  0, store_dds_uncompressed_image},
	{".ktx", ".KTX",  0, store_ktx_uncompressed_image}
};

static const int storer_descr_count = sizeof(storer_descs) / sizeof(storer_descs[0]);

/* See header for documentation. */
int get_output_filename_enforced_bitness(
	const char* filename
) {
	const char *eptr = strrchr(filename, '.');
	if (!eptr)
	{
		return 0;
	}

	for (int i = 0; i < storer_descr_count; i++)
	{
		if (strcmp(eptr, storer_descs[i].ending1) == 0
		 || strcmp(eptr, storer_descs[i].ending2) == 0)
		{
			return storer_descs[i].enforced_bitness;
		}
	}

	return -1;
}

/* See header for documentation. */
astcenc_image* load_ncimage(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	// Get the file extension
	const char* eptr = strrchr(filename, '.');
	if (!eptr)
	{
		eptr = filename;
	}

	// Scan through descriptors until a matching loader is found
	for (unsigned int i = 0; i < loader_descr_count; i++)
	{
		if (loader_descs[i].ending1 == nullptr
			|| strcmp(eptr, loader_descs[i].ending1) == 0
			|| strcmp(eptr, loader_descs[i].ending2) == 0)
		{
			return loader_descs[i].loader_func(filename, y_flip, is_hdr, component_count);
		}
	}

	// Should never reach here - stb_image provides a generic handler
	return nullptr;
}

/* See header for documentation. */
bool store_ncimage(
	const astcenc_image* output_image,
	const char* filename,
	int y_flip
) {
	const char* eptr = strrchr(filename, '.');
	if (!eptr)
	{
		eptr = ".ktx"; // use KTX file format if we don't have an ending.
	}

	for (int i = 0; i < storer_descr_count; i++)
	{
		if (strcmp(eptr, storer_descs[i].ending1) == 0
		 || strcmp(eptr, storer_descs[i].ending2) == 0)
		{
			return storer_descs[i].storer_func(output_image, filename, y_flip);
		}
	}

	// Should never reach here - get_output_filename_enforced_bitness should
	// have acted as a preflight check
	return false;
}

/* ============================================================================
	ASTC compressed file loading
============================================================================ */
struct astc_header
{
	uint8_t magic[4];
	uint8_t block_x;
	uint8_t block_y;
	uint8_t block_z;
	uint8_t dim_x[3];			// dims = dim[0] + (dim[1] << 8) + (dim[2] << 16)
	uint8_t dim_y[3];			// Sizes are given in texels;
	uint8_t dim_z[3];			// block count is inferred
};

static const uint32_t ASTC_MAGIC_ID = 0x5CA1AB13;

static unsigned int unpack_bytes(
	uint8_t a,
	uint8_t b,
	uint8_t c,
	uint8_t d
) {
	return (static_cast<unsigned int>(a)      ) +
	       (static_cast<unsigned int>(b) <<  8) +
	       (static_cast<unsigned int>(c) << 16) +
	       (static_cast<unsigned int>(d) << 24);
}

/* See header for documentation. */
int load_cimage(
	const char* filename,
	astc_compressed_image& img
) {
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file)
	{
		printf("ERROR: File open failed '%s'\n", filename);
		return 1;
	}

	astc_header hdr;
	file.read(reinterpret_cast<char*>(&hdr), sizeof(astc_header));
	if (!file)
	{
		printf("ERROR: File read failed '%s'\n", filename);
		return 1;
	}

	unsigned int magicval = unpack_bytes(hdr.magic[0], hdr.magic[1], hdr.magic[2], hdr.magic[3]);
	if (magicval != ASTC_MAGIC_ID)
	{
		printf("ERROR: File not recognized '%s'\n", filename);
		return 1;
	}

	// Ensure these are not zero to avoid div by zero
	unsigned int block_x = astc::max(static_cast<unsigned int>(hdr.block_x), 1u);
	unsigned int block_y = astc::max(static_cast<unsigned int>(hdr.block_y), 1u);
	unsigned int block_z = astc::max(static_cast<unsigned int>(hdr.block_z), 1u);

	unsigned int dim_x = unpack_bytes(hdr.dim_x[0], hdr.dim_x[1], hdr.dim_x[2], 0);
	unsigned int dim_y = unpack_bytes(hdr.dim_y[0], hdr.dim_y[1], hdr.dim_y[2], 0);
	unsigned int dim_z = unpack_bytes(hdr.dim_z[0], hdr.dim_z[1], hdr.dim_z[2], 0);

	if (dim_x == 0 || dim_y == 0 || dim_z == 0)
	{
		printf("ERROR: File corrupt '%s'\n", filename);
		return 1;
	}

	unsigned int xblocks = (dim_x + block_x - 1) / block_x;
	unsigned int yblocks = (dim_y + block_y - 1) / block_y;
	unsigned int zblocks = (dim_z + block_z - 1) / block_z;

	size_t data_size = xblocks * yblocks * zblocks * 16;
	uint8_t *buffer = new uint8_t[data_size];

	file.read(reinterpret_cast<char*>(buffer), data_size);
	if (!file)
	{
		printf("ERROR: File read failed '%s'\n", filename);
		return 1;
	}

	img.data = buffer;
	img.data_len = data_size;
	img.block_x = block_x;
	img.block_y = block_y;
	img.block_z = block_z;
	img.dim_x = dim_x;
	img.dim_y = dim_y;
	img.dim_z = dim_z;
	return 0;
}

/* See header for documentation. */
int store_cimage(
	const astc_compressed_image& img,
	const char* filename
) {
	astc_header hdr;
	hdr.magic[0] =  ASTC_MAGIC_ID        & 0xFF;
	hdr.magic[1] = (ASTC_MAGIC_ID >>  8) & 0xFF;
	hdr.magic[2] = (ASTC_MAGIC_ID >> 16) & 0xFF;
	hdr.magic[3] = (ASTC_MAGIC_ID >> 24) & 0xFF;

	hdr.block_x = static_cast<uint8_t>(img.block_x);
	hdr.block_y = static_cast<uint8_t>(img.block_y);
	hdr.block_z = static_cast<uint8_t>(img.block_z);

	hdr.dim_x[0] =  img.dim_x        & 0xFF;
	hdr.dim_x[1] = (img.dim_x >>  8) & 0xFF;
	hdr.dim_x[2] = (img.dim_x >> 16) & 0xFF;

	hdr.dim_y[0] =  img.dim_y       & 0xFF;
	hdr.dim_y[1] = (img.dim_y >>  8) & 0xFF;
	hdr.dim_y[2] = (img.dim_y >> 16) & 0xFF;

	hdr.dim_z[0] =  img.dim_z        & 0xFF;
	hdr.dim_z[1] = (img.dim_z >>  8) & 0xFF;
	hdr.dim_z[2] = (img.dim_z >> 16) & 0xFF;

	std::ofstream file(filename, std::ios::out | std::ios::binary);
	if (!file)
	{
		printf("ERROR: File open failed '%s'\n", filename);
		return 1;
	}

	file.write(reinterpret_cast<char*>(&hdr), sizeof(astc_header));
	file.write(reinterpret_cast<char*>(img.data), img.data_len);
	return 0;
}
