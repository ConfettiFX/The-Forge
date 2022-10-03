// SPDX-License-Identifier: Apache-2.0
// ----------------------------------------------------------------------------
// Copyright 2011-2021 Arm Limited
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
 * @brief Functions and data declarations.
 */

#ifndef ASTCENCCLI_INTERNAL_INCLUDED
#define ASTCENCCLI_INTERNAL_INCLUDED

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "astcenc.h"
#include "astcenc_mathlib.h"

/**
 * @brief The payload stored in a compressed ASTC image.
 */
struct astc_compressed_image
{
	/** @brief The block width in texels. */
	unsigned int block_x;

	/** @brief The block height in texels. */
	unsigned int block_y;

	/** @brief The block depth in texels. */
	unsigned int block_z;

	/** @brief The image width in texels. */
	unsigned int dim_x;

	/** @brief The image height in texels. */
	unsigned int dim_y;

	/** @brief The image depth in texels. */
	unsigned int dim_z;

	/** @brief The binary data payload. */
	uint8_t* data;

	/** @brief The binary data length in bytes. */
	size_t data_len;
};

/**
 * @brief Config options that have been read from command line.
 */
struct cli_config_options
{
	/** @brief The number of threads to use for processing. */
	unsigned int thread_count;

	/** @brief The number of repeats to execute for benchmarking. */
	unsigned int repeat_count;

	/** @brief The number of image slices to load for a 3D image. */
	unsigned int array_size;

	/** @brief @c true if running in silent mode with minimal output. */
	bool silentmode;

	/** @brief @c true if the images should be y-flipped. */
	bool y_flip;

	/** @brief @c true if diagnostic images should be stored. */
	bool diagnostic_images;

	/** @brief The low exposure fstop for error computation. */
	int low_fstop;

	/** @brief The high exposure fstop for error computation. */
	int high_fstop;

	/** @brief The  pre-encode swizzle. */
	astcenc_swizzle swz_encode;

	/** @brief The  post-decode swizzle. */
	astcenc_swizzle swz_decode;
};

/**
 * @brief Load uncompressed image.
 *
 * @param filename               The file path on disk.
 * @param y_flip                 Should this image be Y flipped?
 * @param[out] is_hdr            Is the loaded image HDR?
 * @param[out] component_count   The number of components in the loaded image.
 *
 * @return The astc image file, or nullptr on error.
 */
astcenc_image* load_ncimage(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count);

/**
 * @brief Load uncompressed PNG image.
 *
 * @param filename               The file path on disk.
 * @param y_flip                 Should this image be Y flipped?
 * @param[out] is_hdr            Is the loaded image HDR?
 * @param[out] component_count   The number of components in the loaded image.
 *
 * @return The astc image file, or nullptr on error.
 */
astcenc_image* load_png_with_wuffs(
	const char* filename,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count);

/**
 * @brief Save an uncompressed image.
 *
 * @param img        The source data for the image.
 * @param filename   The name of the file to save.
 * @param y_flip     Should the image be vertically flipped?
 *
 * @return @c true if the image saved OK, @c false on error.
 */
bool store_ncimage(
	const astcenc_image* img,
	const char* filename,
	int y_flip);

/**
 * @brief Check if the output file type requires a specific bitness.
 *
 * @param filename The file name, containing hte extension to check.
 *
 * @return Valid values are:
 *     * -1 - error - unknown file type.
 *     *  0 - no enforced bitness.
 *     *  8 - enforced 8-bit UNORM.
 *     * 16 - enforced 16-bit FP16.
 */
int get_output_filename_enforced_bitness(
	const char* filename);

/**
 * @brief Allocate a new image in a canonical format.
 *
 * Allocated images must be freed with a @c free_image() call.
 *
 * @param bitness   The number of bits per component (8, 16, or 32).
 * @param dim_x     The width of the image, in texels.
 * @param dim_y     The height of the image, in texels.
 * @param dim_z     The depth of the image, in texels.
 *
 * @return The allocated image, or @c nullptr on error.
 */
astcenc_image* alloc_image(
	unsigned int bitness,
	unsigned int dim_x,
	unsigned int dim_y,
	unsigned int dim_z);

/**
 * @brief Free an image.
 *
 * @param img   The image to free.
 */
void free_image(
	astcenc_image* img);

void free_cimage(const astc_compressed_image& img);

/**
 * @brief Determine the number of active components in an image.
 *
 * @param img   The image to analyze.
 *
 * @return The number of active components in the image.
 */
int determine_image_components(
	const astcenc_image* img);

/**
 * @brief Load a compressed .astc image.
 *
 * @param filename   The file to load.
 * @param img        The image to populate with loaded data.
 *
 * @return Non-zero on error, zero on success.
 */
int load_cimage(
	const char* filename,
	astc_compressed_image& img);

/**
 * @brief Store a compressed .astc image.
 *
 * @param img        The image to store.
 * @param filename   The file to save.
 *
 * @return Non-zero on error, zero on success.
 */
int store_cimage(
	const astc_compressed_image& img,
	const char* filename);

/**
 * @brief Load a compressed .ktx image.
 *
 * @param filename   The file to load.
 * @param is_srgb    Is this an sRGB encoded file?
 * @param img        The image to populate with loaded data.
 *
 * @return Non-zero on error, zero on success.
 */
bool load_ktx_compressed_image(
	const char* filename,
	bool& is_srgb,
	astc_compressed_image& img) ;

/**
 * @brief Store a compressed .ktx image.
 *
 * @param img        The image to store.
 * @param filename   The file to store.
 * @param is_srgb    Is this an sRGB encoded file?
 *
 * @return Non-zero on error, zero on success.
 */
bool store_ktx_compressed_image(
	const astc_compressed_image& img,
	const char* filename,
	bool is_srgb);

/**
 * @brief Create an image from a 2D float data array.
 *
 * @param data     The raw input data.
 * @param dim_x    The width of the image, in texels.
 * @param dim_y    The height of the image, in texels.
 * @param y_flip   Should this image be vertically flipped?
 *
 * @return The populated image.
 */
astcenc_image* astc_img_from_floatx4_array(
	const float* data,
	unsigned int dim_x,
	unsigned int dim_y,
	bool y_flip);

/**
 * @brief Create an image from a 2D byte data array.
 *
 * @param data     The raw input data.
 * @param dim_x    The width of the image, in texels.
 * @param dim_y    The height of the image, in texels.
 * @param y_flip   Should this image be vertically flipped?
 *
 * @return The populated image.
 */
astcenc_image* astc_img_from_unorm8x4_array(
	const uint8_t* data,
	unsigned int dim_x,
	unsigned int dim_y,
	bool y_flip);

/**
 * @brief Create a flattened RGBA FLOAT32 data array from an image structure.
 *
 * The returned data array is allocated with @c new[] and must be freed with a @c delete[] call.
 *
 * @param img      The input image.
 * @param y_flip   Should the data in the array be Y flipped?
 *
 * @return The data array.
 */
float* floatx4_array_from_astc_img(
	const astcenc_image* img,
	bool y_flip);

/**
 * @brief Create a flattened RGBA UNORM8 data array from an image structure.
 *
 * The returned data array is allocated with @c new[] and must be freed with a @c delete[] call.
 *
 * @param img      The input image.
 * @param y_flip   Should the data in the array be Y flipped?
 *
 * @return The data array.
 */
uint8_t* unorm8x4_array_from_astc_img(
	const astcenc_image* img,
	bool y_flip);

/* ============================================================================
  Functions for printing build info and help messages
============================================================================ */

/**
 * @brief Print the tool copyright and version header to stdout.
 */
void astcenc_print_header();

/**
 * @brief Print the tool copyright, version, and short-form help to stdout.
 */
void astcenc_print_shorthelp();

/**
 * @brief Print the tool copyright, version, and long-form help to stdout.
 */
void astcenc_print_longhelp();

/**
 * @brief Compute error metrics comparing two images.
 *
 * @param compute_hdr_metrics      True if HDR metrics should be computed.
 * @param compute_normal_metrics   True if normal map metrics should be computed.
 * @param input_components         The number of input color components.
 * @param img1                     The original image.
 * @param img2                     The compressed image.
 * @param fstop_lo                 The low exposure fstop (HDR only).
 * @param fstop_hi                 The high exposure fstop (HDR only).
 */
void compute_error_metrics(
	bool compute_hdr_metrics,
	bool compute_normal_metrics,
	int input_components,
	const astcenc_image* img1,
	const astcenc_image* img2,
	int fstop_lo,
	int fstop_hi);

/**
 * @brief Get the current time.
 *
 * @return The current time in seconds since arbitrary epoch.
 */
double get_time();

/**
 * @brief Get the number of CPU cores.
 *
 * @return The number of online or onlineable CPU cores in the system.
 */
int get_cpu_count();

/**
 * @brief Launch N worker threads and wait for them to complete.
 *
 * All threads run the same thread function, and have the same thread payload, but are given a
 * unique thread ID (0 .. N-1) as a parameter to the run function to allow thread-specific behavior.
 *
|* @param thread_count The number of threads to spawn.
 * @param func         The function to execute. Must have the signature:
 *                     void (int thread_count, int thread_id, void* payload)
 * @param payload      Pointer to an opaque thread payload object.
 */
void launch_threads(
	int thread_count,
	void (*func)(int, int, void*),
	void *payload);

#endif
