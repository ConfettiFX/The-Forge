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
 * @brief Functions for codec library front-end.
 */

#include "astcenc.h"
#include "astcenccli_internal.h"

#include <cassert>
#include <cstring>
#include <functional>
#include <string>
#include <sstream>
#include <vector>
#include <memory>

/* ============================================================================
	Data structure definitions
============================================================================ */

typedef unsigned int astcenc_operation;

struct mode_entry
{
	const char* opt;
	astcenc_operation operation;
	astcenc_profile decode_mode;
};

/* ============================================================================
	Constants and literals
============================================================================ */

/** @brief Stage bit indicating we need to load a compressed image. */
static const unsigned int ASTCENC_STAGE_LD_COMP    = 1 << 0;

/** @brief Stage bit indicating we need to store a compressed image. */
static const unsigned int ASTCENC_STAGE_ST_COMP    = 1 << 1;

/** @brief Stage bit indicating we need to load an uncompressed image. */
static const unsigned int ASTCENC_STAGE_LD_NCOMP   = 1 << 2;

/** @brief Stage bit indicating we need to store an uncompressed image. */
static const unsigned int ASTCENC_STAGE_ST_NCOMP   = 1 << 3;

/** @brief Stage bit indicating we need compress an image. */
static const unsigned int ASTCENC_STAGE_COMPRESS   = 1 << 4;

/** @brief Stage bit indicating we need to decompress an image. */
static const unsigned int ASTCENC_STAGE_DECOMPRESS = 1 << 5;

/** @brief Stage bit indicating we need to compare an image with the original input. */
static const unsigned int ASTCENC_STAGE_COMPARE    = 1 << 6;

/** @brief Operation indicating an unknown request (should never happen). */
static const astcenc_operation ASTCENC_OP_UNKNOWN  = 0;

/** @brief Operation indicating the user wants to print long-form help text and version info. */
static const astcenc_operation ASTCENC_OP_HELP     = 1 << 7;

/** @brief Operation indicating the user wants to print short-form help text and version info. */
static const astcenc_operation ASTCENC_OP_VERSION  = 1 << 8;

/** @brief Operation indicating the user wants to compress and store an image. */
static const astcenc_operation ASTCENC_OP_COMPRESS =
                               ASTCENC_STAGE_LD_NCOMP |
                               ASTCENC_STAGE_COMPRESS |
                               ASTCENC_STAGE_ST_COMP;

/** @brief Operation indicating the user wants to decompress and store an image. */
static const astcenc_operation ASTCENC_OP_DECOMPRESS =
                               ASTCENC_STAGE_LD_COMP |
                               ASTCENC_STAGE_DECOMPRESS |
                               ASTCENC_STAGE_ST_NCOMP;

/** @brief Operation indicating the user wants to test a compression setting on an image. */
static const astcenc_operation ASTCENC_OP_TEST =
                               ASTCENC_STAGE_LD_NCOMP |
                               ASTCENC_STAGE_COMPRESS |
                               ASTCENC_STAGE_DECOMPRESS |
                               ASTCENC_STAGE_COMPARE |
                               ASTCENC_STAGE_ST_NCOMP;

/**
 * @brief Image preprocesing tasks prior to encoding.
 */
enum astcenc_preprocess
{
	/** @brief No image preprocessing. */
	ASTCENC_PP_NONE = 0,
	/** @brief Normal vector unit-length normalization. */
	ASTCENC_PP_NORMALIZE,
	/** @brief Color data alpha premultiplication. */
	ASTCENC_PP_PREMULTIPLY
};

/** @brief Decode table for command line operation modes. */
static const mode_entry modes[] {
	{"-cl",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_LDR},
	{"-dl",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_LDR},
	{"-tl",      ASTCENC_OP_TEST,       ASTCENC_PRF_LDR},
	{"-cs",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_LDR_SRGB},
	{"-ds",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_LDR_SRGB},
	{"-ts",      ASTCENC_OP_TEST,       ASTCENC_PRF_LDR_SRGB},
	{"-ch",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_HDR_RGB_LDR_A},
	{"-dh",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_HDR_RGB_LDR_A},
	{"-th",      ASTCENC_OP_TEST,       ASTCENC_PRF_HDR_RGB_LDR_A},
	{"-cH",      ASTCENC_OP_COMPRESS,   ASTCENC_PRF_HDR},
	{"-dH",      ASTCENC_OP_DECOMPRESS, ASTCENC_PRF_HDR},
	{"-tH",      ASTCENC_OP_TEST,       ASTCENC_PRF_HDR},
	{"-h",       ASTCENC_OP_HELP,       ASTCENC_PRF_HDR},
	{"-help",    ASTCENC_OP_HELP,       ASTCENC_PRF_HDR},
	{"-v",       ASTCENC_OP_VERSION,    ASTCENC_PRF_HDR},
	{"-version", ASTCENC_OP_VERSION,    ASTCENC_PRF_HDR}
};

/**
 * @brief Compression workload definition for worker threads.
 */
struct compression_workload
{
	astcenc_context* context;
	astcenc_image* image;
	astcenc_swizzle swizzle;
	uint8_t* data_out;
	size_t data_len;
	astcenc_error error;
};

/**
 * @brief Decompression workload definition for worker threads.
 */
struct decompression_workload
{
	astcenc_context* context;
	uint8_t* data;
	size_t data_len;
	astcenc_image* image_out;
	astcenc_swizzle swizzle;
	astcenc_error error;
};

/**
 * @brief Test if a string argument is a well formed float.
 */
static bool is_float(
	std::string target
) {
	float test;
	std::istringstream stream(target);

	// Leading whitespace is an error
	stream >> std::noskipws >> test;

	// Ensure entire no remaining string in addition to parse failure
	return stream.eof() && !stream.fail();
}

/**
 * @brief Test if a string ends with a given suffix.
 */
static bool ends_with(
	const std::string& str,
	const std::string& suffix
) {
	return (str.size() >= suffix.size()) &&
	       (0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix));
}

/**
 * @brief Runner callback function for a compression worker thread.
 *
 * @param thread_count   The number of threads in the worker pool.
 * @param thread_id      The index of this thread in the worker pool.
 * @param payload        The parameters for this thread.
 */
static void compression_workload_runner(
	int thread_count,
	int thread_id,
	void* payload
) {
	(void)thread_count;

	compression_workload* work = static_cast<compression_workload*>(payload);
	astcenc_error error = astcenc_compress_image(
	                       work->context, work->image, &work->swizzle,
	                       work->data_out, work->data_len, thread_id);

	// This is a racy update, so which error gets returned is a random, but it
	// will reliably report an error if an error occurs
	if (error != ASTCENC_SUCCESS)
	{
		work->error = error;
	}
}

/**
 * @brief Runner callback function for a decompression worker thread.
 *
 * @param thread_count   The number of threads in the worker pool.
 * @param thread_id      The index of this thread in the worker pool.
 * @param payload        The parameters for this thread.
 */
static void decompression_workload_runner(
	int thread_count,
	int thread_id,
	void* payload
) {
	(void)thread_count;

	decompression_workload* work = static_cast<decompression_workload*>(payload);
	astcenc_error error = astcenc_decompress_image(
	                       work->context, work->data, work->data_len,
	                       work->image_out, &work->swizzle, thread_id);

	// This is a racy update, so which error gets returned is a random, but it
	// will reliably report an error if an error occurs
	if (error != ASTCENC_SUCCESS)
	{
		work->error = error;
	}
}

/**
 * @brief Utility to generate a slice file name from a pattern.
 *
 * Convert "foo/bar.png" in to "foo/bar_<slice>.png"
 *
 * @param basename The base pattern; must contain a file extension.
 * @param index    The slice index.
 * @param error    Set to true on success, false on error (no extension found).
 *
 * @return The slice file name.
 */
static std::string get_slice_filename(
	const std::string& basename,
	unsigned int index,
	bool& error
) {
	size_t sep = basename.find_last_of('.');
	if (sep == std::string::npos)
	{
		error = true;
		return "";
	}

	std::string base = basename.substr(0, sep);
	std::string ext = basename.substr(sep);
	std::string name = base + "_" + std::to_string(index) + ext;
	error = false;
	return name;
}

/**
 * @brief Load a non-astc image file from memory.
 *
 * @param filename            The file to load, or a pattern for array loads.
 * @param dim_z               The number of slices to load.
 * @param y_flip              Should this image be Y flipped?
 * @param[out] is_hdr         Is the loaded image HDR?
 * @param[out] component_count The number of components in the loaded image.
 *
 * @return The astc image file, or nullptr on error.
 */
static astcenc_image* load_uncomp_file(
	const char* filename,
	unsigned int dim_z,
	bool y_flip,
	bool& is_hdr,
	unsigned int& component_count
) {
	astcenc_image *image = nullptr;

	// For a 2D image just load the image directly
	if (dim_z == 1)
	{
		image = load_ncimage(filename, y_flip, is_hdr, component_count);
	}
	else
	{
		bool slice_is_hdr;
		unsigned int slice_component_count;
		astcenc_image* slice = nullptr;
		std::vector<astcenc_image*> slices;

		// For a 3D image load an array of slices
		for (unsigned int image_index = 0; image_index < dim_z; image_index++)
		{
			bool error;
			std::string slice_name = get_slice_filename(filename, image_index, error);
			if (error)
			{
				printf("ERROR: Image pattern does not contain file extension: %s\n", filename);
				break;
			}

			slice = load_ncimage(slice_name.c_str(), y_flip,
			                     slice_is_hdr, slice_component_count);
			if (!slice)
			{
				break;
			}

			slices.push_back(slice);

			// Check it is not a 3D image
			if (slice->dim_z != 1)
			{
				printf("ERROR: Image arrays do not support 3D sources: %s\n", slice_name.c_str());
				break;
			}

			// Check slices are consistent with each other
			if (image_index != 0)
			{
				if ((is_hdr != slice_is_hdr) || (component_count != slice_component_count))
				{
					printf("ERROR: Image array[0] and [%d] are different formats\n", image_index);
					break;
				}

				if ((slices[0]->dim_x != slice->dim_x) ||
				    (slices[0]->dim_y != slice->dim_y) ||
				    (slices[0]->dim_z != slice->dim_z))
				{
					printf("ERROR: Image array[0] and [%d] are different dimensions\n", image_index);
					break;
				}
			}
			else
			{
				is_hdr = slice_is_hdr;
				component_count = slice_component_count;
			}
		}

		// If all slices loaded correctly then repack them into a single image
		if (slices.size() == dim_z)
		{
			unsigned int dim_x = slices[0]->dim_x;
			unsigned int dim_y = slices[0]->dim_y;
			int bitness = is_hdr ? 16 : 8;
			int slice_size = dim_x * dim_y;

			image = alloc_image(bitness, dim_x, dim_y, dim_z);

			// Combine 2D source images into one 3D image
			for (unsigned int z = 0; z < dim_z; z++)
			{
				if (image->data_type == ASTCENC_TYPE_U8)
				{
					uint8_t* data8 = static_cast<uint8_t*>(image->data[z]);
					uint8_t* data8src = static_cast<uint8_t*>(slices[z]->data[0]);
					size_t copy_size = slice_size * 4 * sizeof(uint8_t);
					memcpy(data8, data8src, copy_size);
				}
				else if (image->data_type == ASTCENC_TYPE_F16)
				{
					uint16_t* data16 = static_cast<uint16_t*>(image->data[z]);
					uint16_t* data16src = static_cast<uint16_t*>(slices[z]->data[0]);
					size_t copy_size = slice_size * 4 * sizeof(uint16_t);
					memcpy(data16, data16src, copy_size);
				}
				else // if (image->data_type == ASTCENC_TYPE_F32)
				{
					assert(image->data_type == ASTCENC_TYPE_F32);
					float* data32 = static_cast<float*>(image->data[z]);
					float* data32src = static_cast<float*>(slices[z]->data[0]);
					size_t copy_size = slice_size * 4 * sizeof(float);
					memcpy(data32, data32src, copy_size);
				}
			}
		}

		for (auto &i : slices)
		{
			free_image(i);
		}
	}

	return image;
}

/**
 * @brief Parse the command line.
 *
 * @param      argc        Command line argument count.
 * @param[in]  argv        Command line argument vector.
 * @param[out] operation   Codec operation mode.
 * @param[out] profile     Codec color profile.
 *
 * @return 0 if everything is okay, 1 if there is some error
 */
static int parse_commandline_options(
	int argc,
	char **argv,
	astcenc_operation& operation,
	astcenc_profile& profile
) {
	assert(argc >= 2); (void)argc;

	profile = ASTCENC_PRF_LDR;
	operation = ASTCENC_OP_UNKNOWN;

	int modes_count = sizeof(modes) / sizeof(modes[0]);
	for (int i = 0; i < modes_count; i++)
	{
		if (!strcmp(modes[i].opt, argv[1]))
		{
			operation = modes[i].operation;
			profile = modes[i].decode_mode;
			break;
		}
	}

	if (operation == ASTCENC_OP_UNKNOWN)
	{
		printf("ERROR: Unrecognized operation '%s'\n", argv[1]);
		return 1;
	}

	return 0;
}

/**
 * @brief Initialize the astcenc_config
 *
 * @param      argc         Command line argument count.
 * @param[in]  argv         Command line argument vector.
 * @param      operation    Codec operation mode.
 * @param[out] profile      Codec color profile.
 * @param      comp_image   Compressed image if a decompress operation.
 * @param[out] preprocess   Image preprocess operation.
 * @param[out] config       Codec configuration.
 *
 * @return 0 if everything is okay, 1 if there is some error
 */
static int init_astcenc_config(
	int argc,
	char **argv,
	astcenc_profile profile,
	astcenc_operation operation,
	astc_compressed_image& comp_image,
	astcenc_preprocess& preprocess,
	astcenc_config& config
) {
	unsigned int block_x = 0;
	unsigned int block_y = 0;
	unsigned int block_z = 1;

	// For decode the block size is set by the incoming image.
	if (operation == ASTCENC_OP_DECOMPRESS)
	{
		block_x = comp_image.block_x;
		block_y = comp_image.block_y;
		block_z = comp_image.block_z;
	}

	float quality = 0.0f;
	preprocess = ASTCENC_PP_NONE;

	// parse the command line's encoding options.
	int argidx = 4;
	if (operation & ASTCENC_STAGE_COMPRESS)
	{
		// Read and decode block size
		if (argc < 5)
		{
			printf("ERROR: Block size must be specified\n");
			return 1;
		}

		int cnt2D, cnt3D;
		int dimensions = sscanf(argv[4], "%ux%u%nx%u%n",
		                        &block_x, &block_y, &cnt2D, &block_z, &cnt3D);
		// Character after the last match should be a NUL
		if (!(((dimensions == 2) && !argv[4][cnt2D]) || ((dimensions == 3) && !argv[4][cnt3D])))
		{
			printf("ERROR: Block size '%s' is invalid\n", argv[4]);
			return 1;
		}

		// Read and decode search quality
		if (argc < 6)
		{
			printf("ERROR: Search quality level must be specified\n");
			return 1;
		}

		if (!strcmp(argv[5], "-fastest"))
		{
			quality = ASTCENC_PRE_FASTEST;
		}
		else if (!strcmp(argv[5], "-fast"))
		{
			quality = ASTCENC_PRE_FAST;
		}
		else if (!strcmp(argv[5], "-medium"))
		{
			quality = ASTCENC_PRE_MEDIUM;
		}
		else if (!strcmp(argv[5], "-thorough"))
		{
			quality = ASTCENC_PRE_THOROUGH;
		}
		else if (!strcmp(argv[5], "-exhaustive"))
		{
			quality = ASTCENC_PRE_EXHAUSTIVE;
		}
		else if (is_float(argv[5]))
		{
			quality = static_cast<float>(atof(argv[5]));
		}
		else
		{
			printf("ERROR: Search quality/preset '%s' is invalid\n", argv[5]);
			return 1;
		}

		argidx = 6;
	}

	unsigned int flags = 0;

	// Gather the flags that we need
	while (argidx < argc)
	{
		if (!strcmp(argv[argidx], "-a"))
		{
			// Skip over the data value for now
			argidx++;
			flags |= ASTCENC_FLG_USE_ALPHA_WEIGHT;
		}
		else if (!strcmp(argv[argidx], "-mask"))
		{
			flags |= ASTCENC_FLG_MAP_MASK;
		}
		else if (!strcmp(argv[argidx], "-normal"))
		{
			flags |= ASTCENC_FLG_MAP_NORMAL;
		}
		else if (!strcmp(argv[argidx], "-rgbm"))
		{
			// Skip over the data value for now
			argidx++;
			flags |= ASTCENC_FLG_MAP_RGBM;
		}
		else if (!strcmp(argv[argidx], "-perceptual"))
		{
			flags |= ASTCENC_FLG_USE_PERCEPTUAL;
		}
		else if (!strcmp(argv[argidx], "-pp-normalize"))
		{
			if (preprocess != ASTCENC_PP_NONE)
			{
				printf("ERROR: Only a single image preprocess can be used\n");
				return 1;
			}
			preprocess = ASTCENC_PP_NORMALIZE;
		}
		else if (!strcmp(argv[argidx], "-pp-premultiply"))
		{
			if (preprocess != ASTCENC_PP_NONE)
			{
				printf("ERROR: Only a single image preprocess can be used\n");
				return 1;
			}
			preprocess = ASTCENC_PP_PREMULTIPLY;
		}
		argidx ++;
	}

#if defined(ASTCENC_DECOMPRESS_ONLY)
	flags |= ASTCENC_FLG_DECOMPRESS_ONLY;
#else
	// Decompression can skip some memory allocation, but need full tables
	if (operation == ASTCENC_OP_DECOMPRESS)
	{
		flags |= ASTCENC_FLG_DECOMPRESS_ONLY;
	}
	// Compression and test passes can skip some decimation initialization
	// as we know we are decompressing images that were compressed using the
	// same settings and heuristics ...
	else
	{
		flags |= ASTCENC_FLG_SELF_DECOMPRESS_ONLY;
	}
#endif

	astcenc_error status = astcenc_config_init(profile, block_x, block_y, block_z,
	                                           quality, flags, &config);
	if (status == ASTCENC_ERR_BAD_BLOCK_SIZE)
	{
		printf("ERROR: Block size '%s' is invalid\n", argv[4]);
		return 1;
	}
	else if (status == ASTCENC_ERR_BAD_CPU_ISA)
	{
		printf("ERROR: Required SIMD ISA support missing on this CPU\n");
		return 1;
	}
	else if (status == ASTCENC_ERR_BAD_CPU_FLOAT)
	{
		printf("ERROR: astcenc must not be compiled with -ffast-math\n");
		return 1;
	}
	else if (status != ASTCENC_SUCCESS)
	{
		printf("ERROR: Init config failed with %s\n", astcenc_get_error_string(status));
		return 1;
	}

	return 0;
}

/**
 * @brief Edit the astcenc_config
 *
 * @param         argc         Command line argument count.
 * @param[in]     argv         Command line argument vector.
 * @param         operation    Codec operation.
 * @param[out]    cli_config   Command line config.
 * @param[in,out] config       Codec configuration.
 *
 * @return 0 if everything is OK, 1 if there is some error
 */
static int edit_astcenc_config(
	int argc,
	char **argv,
	const astcenc_operation operation,
	cli_config_options& cli_config,
	astcenc_config& config
) {

	int argidx = (operation & ASTCENC_STAGE_COMPRESS) ? 6 : 4;

	while (argidx < argc)
	{
		if (!strcmp(argv[argidx], "-silent"))
		{
			argidx++;
			cli_config.silentmode = 1;
		}
		else if (!strcmp(argv[argidx], "-cw"))
		{
			argidx += 5;
			if (argidx > argc)
			{
				printf("ERROR: -cw switch with less than 4 arguments\n");
				return 1;
			}

			config.cw_r_weight = static_cast<float>(atof(argv[argidx - 4]));
			config.cw_g_weight = static_cast<float>(atof(argv[argidx - 3]));
			config.cw_b_weight = static_cast<float>(atof(argv[argidx - 2]));
			config.cw_a_weight = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-a"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -a switch with no argument\n");
				return 1;
			}

			config.a_scale_radius = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-esw"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -esw switch with no argument\n");
				return 1;
			}

			if (strlen(argv[argidx - 1]) != 4)
			{
				printf("ERROR: -esw pattern does not contain 4 characters\n");
				return 1;
			}

			astcenc_swz swizzle_components[4];
			for (int i = 0; i < 4; i++)
			{
				switch (argv[argidx - 1][i])
				{
				case 'r':
					swizzle_components[i] = ASTCENC_SWZ_R;
					break;
				case 'g':
					swizzle_components[i] = ASTCENC_SWZ_G;
					break;
				case 'b':
					swizzle_components[i] = ASTCENC_SWZ_B;
					break;
				case 'a':
					swizzle_components[i] = ASTCENC_SWZ_A;
					break;
				case '0':
					swizzle_components[i] = ASTCENC_SWZ_0;
					break;
				case '1':
					swizzle_components[i] = ASTCENC_SWZ_1;
					break;
				default:
					printf("ERROR: -esw component '%c' is not valid\n", argv[argidx - 1][i]);
					return 1;
				}
			}

			cli_config.swz_encode.r = swizzle_components[0];
			cli_config.swz_encode.g = swizzle_components[1];
			cli_config.swz_encode.b = swizzle_components[2];
			cli_config.swz_encode.a = swizzle_components[3];
		}
		else if (!strcmp(argv[argidx], "-ssw"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -ssw switch with no argument\n");
				return 1;
			}

			size_t char_count = strlen(argv[argidx - 1]);
			if (char_count == 0)
			{
				printf("ERROR: -ssw pattern contains no characters\n");
				return 1;
			}

			if (char_count > 4)
			{
				printf("ERROR: -ssw pattern contains more than 4 characters\n");
				return 1;
			}

			bool found_r = false;
			bool found_g = false;
			bool found_b = false;
			bool found_a = false;

			for (size_t i = 0; i < char_count; i++)
			{
				switch (argv[argidx - 1][i])
				{
				case 'r':
					found_r = true;
					break;
				case 'g':
					found_g = true;
					break;
				case 'b':
					found_b = true;
					break;
				case 'a':
					found_a = true;
					break;
				default:
					printf("ERROR: -ssw component '%c' is not valid\n", argv[argidx - 1][i]);
					return 1;
				}
			}

			config.cw_r_weight = found_r ? 1.0f : 0.0f;
			config.cw_g_weight = found_g ? 1.0f : 0.0f;
			config.cw_b_weight = found_b ? 1.0f : 0.0f;
			config.cw_a_weight = found_a ? 1.0f : 0.0f;
		}
		else if (!strcmp(argv[argidx], "-dsw"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -dsw switch with no argument\n");
				return 1;
			}

			if (strlen(argv[argidx - 1]) != 4)
			{
				printf("ERROR: -dsw switch does not contain 4 characters\n");
				return 1;
			}

			astcenc_swz swizzle_components[4];
			for (int i = 0; i < 4; i++)
			{
				switch (argv[argidx - 1][i])
				{
				case 'r':
					swizzle_components[i] = ASTCENC_SWZ_R;
					break;
				case 'g':
					swizzle_components[i] = ASTCENC_SWZ_G;
					break;
				case 'b':
					swizzle_components[i] = ASTCENC_SWZ_B;
					break;
				case 'a':
					swizzle_components[i] = ASTCENC_SWZ_A;
					break;
				case '0':
					swizzle_components[i] = ASTCENC_SWZ_0;
					break;
				case '1':
					swizzle_components[i] = ASTCENC_SWZ_1;
					break;
				case 'z':
					swizzle_components[i] =  ASTCENC_SWZ_Z;
					break;
				default:
					printf("ERROR: ERROR: -dsw component '%c' is not valid\n", argv[argidx - 1][i]);
					return 1;
				}
			}

			cli_config.swz_decode.r = swizzle_components[0];
			cli_config.swz_decode.g = swizzle_components[1];
			cli_config.swz_decode.b = swizzle_components[2];
			cli_config.swz_decode.a = swizzle_components[3];
		}
		// presets begin here
		else if (!strcmp(argv[argidx], "-mask"))
		{
			argidx++;
		}
		else if (!strcmp(argv[argidx], "-normal"))
		{
			argidx++;

			cli_config.swz_encode.r = ASTCENC_SWZ_R;
			cli_config.swz_encode.g = ASTCENC_SWZ_R;
			cli_config.swz_encode.b = ASTCENC_SWZ_R;
			cli_config.swz_encode.a = ASTCENC_SWZ_G;

			cli_config.swz_decode.r = ASTCENC_SWZ_R;
			cli_config.swz_decode.g = ASTCENC_SWZ_A;
			cli_config.swz_decode.b = ASTCENC_SWZ_Z;
			cli_config.swz_decode.a = ASTCENC_SWZ_1;
		}
		else if (!strcmp(argv[argidx], "-rgbm"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -rgbm switch with no argument\n");
				return 1;
			}

			config.rgbm_m_scale = static_cast<float>(atof(argv[argidx - 1]));
			config.cw_a_weight = 2.0f * config.rgbm_m_scale;
		}
		else if (!strcmp(argv[argidx], "-perceptual"))
		{
			argidx++;
		}
		else if (!strcmp(argv[argidx], "-pp-normalize"))
		{
			argidx++;
		}
		else if (!strcmp(argv[argidx], "-pp-premultiply"))
		{
			argidx++;
		}
		else if (!strcmp(argv[argidx], "-blockmodelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -blockmodelimit switch with no argument\n");
				return 1;
			}

			config.tune_block_mode_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-partitioncountlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -partitioncountlimit switch with no argument\n");
				return 1;
			}

			config.tune_partition_count_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-partitionindexlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -partitionindexlimit switch with no argument\n");
				return 1;
			}

			config.tune_partition_index_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-dblimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -dblimit switch with no argument\n");
				return 1;
			}

			if ((config.profile == ASTCENC_PRF_LDR) || (config.profile == ASTCENC_PRF_LDR_SRGB))
			{
				config.tune_db_limit = static_cast<float>(atof(argv[argidx - 1]));
			}
		}
		else if (!strcmp(argv[argidx], "-2partitionlimitfactor"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -2partitionlimitfactor switch with no argument\n");
				return 1;
			}

			config.tune_2_partition_early_out_limit_factor = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-3partitionlimitfactor"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -3partitionlimitfactor switch with no argument\n");
				return 1;
			}

			config.tune_3_partition_early_out_limit_factor = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-2planelimitcorrelation"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -2planelimitcorrelation switch with no argument\n");
				return 1;
			}

			config.tune_2_plane_early_out_limit_correlation = static_cast<float>(atof(argv[argidx - 1]));
		}
		else if (!strcmp(argv[argidx], "-lowweightmodelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -lowweightmodelimit switch with no argument\n");
				return 1;
			}

			config.tune_low_weight_count_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-refinementlimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -refinementlimit switch with no argument\n");
				return 1;
			}

			config.tune_refinement_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-candidatelimit"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -candidatelimit switch with no argument\n");
				return 1;
			}

			config.tune_candidate_limit = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-j"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -j switch with no argument\n");
				return 1;
			}

			cli_config.thread_count = atoi(argv[argidx - 1]);
		}
		else if (!strcmp(argv[argidx], "-repeats"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -repeats switch with no argument\n");
				return 1;
			}

			cli_config.repeat_count = atoi(argv[argidx - 1]);
			if (cli_config.repeat_count <= 0)
			{
				printf("ERROR: -repeats value must be at least one\n");
				return 1;
			}
		}
		else if (!strcmp(argv[argidx], "-yflip"))
		{
			argidx++;
			cli_config.y_flip = 1;
		}
		else if (!strcmp(argv[argidx], "-mpsnr"))
		{
			argidx += 3;
			if (argidx > argc)
			{
				printf("ERROR: -mpsnr switch with less than 2 arguments\n");
				return 1;
			}

			cli_config.low_fstop = atoi(argv[argidx - 2]);
			cli_config.high_fstop = atoi(argv[argidx - 1]);
			if (cli_config.high_fstop < cli_config.low_fstop)
			{
				printf("ERROR: -mpsnr switch <low> is greater than the <high>\n");
				return 1;
			}
		}
		// Option: Encode a 3D image from a sequence of 2D images.
		else if (!strcmp(argv[argidx], "-zdim"))
		{
			// Only supports compressing
			if (!(operation & ASTCENC_STAGE_COMPRESS))
			{
				printf("ERROR: -zdim switch is only valid for compression\n");
				return 1;
			}

			// Image depth must be specified.
			if (argidx + 2 > argc)
			{
				printf("ERROR: -zdim switch with no argument\n");
				return 1;
			}
			argidx++;

			// Read array size (image depth).
			if (!sscanf(argv[argidx], "%u", &cli_config.array_size) || cli_config.array_size == 0)
			{
				printf("ERROR: -zdim size '%s' is invalid\n", argv[argidx]);
				return 1;
			}

			if ((cli_config.array_size > 1) && (config.block_z == 1))
			{
				printf("ERROR: -zdim with 3D input data for a 2D output format\n");
				return 1;
			}
			argidx++;
		}
#if defined(ASTCENC_DIAGNOSTICS)
		else if (!strcmp(argv[argidx], "-dtrace"))
		{
			argidx += 2;
			if (argidx > argc)
			{
				printf("ERROR: -dtrace switch with no argument\n");
				return 1;
			}

			config.trace_file_path = argv[argidx - 1];
		}
#endif
		else if (!strcmp(argv[argidx], "-dimage"))
		{
			argidx += 1;
			cli_config.diagnostic_images = true;
		}
		else // check others as well
		{
			printf("ERROR: Argument '%s' not recognized\n", argv[argidx]);
			return 1;
		}
	}

	if (cli_config.thread_count <= 0)
	{
		cli_config.thread_count = get_cpu_count();
	}

#if defined(ASTCENC_DIAGNOSTICS)
	// Force single threaded for diagnostic builds
	cli_config.thread_count = 1;

	if (!config.trace_file_path)
	{
		printf("ERROR: Diagnostics builds must set -dtrace\n");
		return 1;
	}
#endif

	return 0;
}

/**
 * @brief Print the config settings in a human readable form.
 *
 * @param[in] cli_config   Command line config.
 * @param[in] config       Codec configuration.
 */
static void print_astcenc_config(
	const cli_config_options& cli_config,
	const astcenc_config& config
) {
	// Print all encoding settings unless specifically told otherwise
	if (!cli_config.silentmode)
	{
		printf("Compressor settings\n");
		printf("===================\n\n");

		switch (config.profile)
		{
		case ASTCENC_PRF_LDR:
			printf("    Color profile:              LDR linear\n");
			break;
		case ASTCENC_PRF_LDR_SRGB:
			printf("    Color profile:              LDR sRGB\n");
			break;
		case ASTCENC_PRF_HDR_RGB_LDR_A:
			printf("    Color profile:              HDR RGB + LDR A\n");
			break;
		case ASTCENC_PRF_HDR:
			printf("    Color profile:              HDR RGBA\n");
			break;
		}

		if (config.block_z == 1)
		{
			printf("    Block size:                 %ux%u\n", config.block_x, config.block_y);
		}
		else
		{
			printf("    Block size:                 %ux%ux%u\n", config.block_x, config.block_y, config.block_z);
		}

		printf("    Bitrate:                    %3.2f bpp\n", 128.0 / (config.block_x * config.block_y * config.block_z));
		printf("    RGB alpha scale weight:     %d\n", (config.flags & ASTCENC_FLG_USE_ALPHA_WEIGHT));
		if ((config.flags & ASTCENC_FLG_USE_ALPHA_WEIGHT))
		{
			printf("    Radius RGB alpha scale:     %u texels\n", config.a_scale_radius);
		}

		printf("    R component weight:         %g\n", static_cast<double>(config.cw_r_weight));
		printf("    G component weight:         %g\n", static_cast<double>(config.cw_g_weight));
		printf("    B component weight:         %g\n", static_cast<double>(config.cw_b_weight));
		printf("    A component weight:         %g\n", static_cast<double>(config.cw_a_weight));
		printf("    Partition cutoff:           %u partitions\n", config.tune_partition_count_limit);
		printf("    Partition index cutoff:     %u partition ids\n", config.tune_partition_index_limit);
		printf("    PSNR cutoff:                %g dB\n", static_cast<double>(config.tune_db_limit));
		printf("    3 partition cutoff:         %g\n", static_cast<double>(config.tune_2_partition_early_out_limit_factor));
		printf("    4 partition cutoff:         %g\n", static_cast<double>(config.tune_3_partition_early_out_limit_factor));
		printf("    2 plane correlation cutoff: %g\n", static_cast<double>(config.tune_2_plane_early_out_limit_correlation));
		printf("    Block mode centile cutoff:  %g%%\n", static_cast<double>(config.tune_block_mode_limit));
		printf("    Candidate cutoff:           %u candidates\n", config.tune_candidate_limit);
		printf("    Refinement cutoff:          %u iterations\n", config.tune_refinement_limit);
		printf("    Compressor thread count:    %d\n", cli_config.thread_count);
		printf("\n");
	}
}

/**
 * @brief Get the value of a single pixel in an image.
 *
 * Note, this implementation is not particularly optimal as it puts format
 * checks in the inner-most loop. For the CLI preprocess passes this is deemed
 * acceptable as these are not performance critical paths.
 *
 * @param[in] img   The output image.
 * @param     x     The pixel x coordinate.
 * @param     y     The pixel y coordinate.
 * @param     z     The pixel z coordinate.
 *
 * @return      pixel   The pixel color value to write.
 */
static vfloat4 image_get_pixel(
	const astcenc_image& img,
	unsigned int x,
	unsigned int y,
	unsigned int z
) {
	// We should never escape bounds
	assert(x < img.dim_x);
	assert(y < img.dim_y);
	assert(z < img.dim_z);

	if (img.data_type == ASTCENC_TYPE_U8)
	{
		uint8_t* data = static_cast<uint8_t*>(img.data[z]);

		float r = data[(4 * img.dim_x * y) + (4 * x    )] / 255.0f;
		float g = data[(4 * img.dim_x * y) + (4 * x + 1)] / 255.0f;
		float b = data[(4 * img.dim_x * y) + (4 * x + 2)] / 255.0f;
		float a = data[(4 * img.dim_x * y) + (4 * x + 3)] / 255.0f;

		return vfloat4(r, g, b, a);
	}
	else if (img.data_type == ASTCENC_TYPE_F16)
	{
		uint16_t* data = static_cast<uint16_t*>(img.data[z]);

		vint4 colori(
			data[(4 * img.dim_x * y) + (4 * x    )],
			data[(4 * img.dim_x * y) + (4 * x + 1)],
			data[(4 * img.dim_x * y) + (4 * x + 2)],
			data[(4 * img.dim_x * y) + (4 * x + 3)]
		);

		return float16_to_float(colori);
	}
	else // if (img.data_type == ASTCENC_TYPE_F32)
	{
		assert(img.data_type == ASTCENC_TYPE_F32);
		float* data = static_cast<float*>(img.data[z]);

		return vfloat4(
			data[(4 * img.dim_x * y) + (4 * x    )],
			data[(4 * img.dim_x * y) + (4 * x + 1)],
			data[(4 * img.dim_x * y) + (4 * x + 2)],
			data[(4 * img.dim_x * y) + (4 * x + 3)]
		);
	}
}

/**
 * @brief Set the value of a single pixel in an image.
 *
 * @param[out] img     The output image; must use F32 texture components.
 * @param      x       The pixel x coordinate.
 * @param      y       The pixel y coordinate.
 * @param      z       The pixel z coordinate.
 * @param      pixel   The pixel color value to write.
 */
static void image_set_pixel(
	astcenc_image& img,
	unsigned int x,
	unsigned int y,
	unsigned int z,
	vfloat4 pixel
) {
	// We should never escape bounds
	assert(x < img.dim_x);
	assert(y < img.dim_y);
	assert(z < img.dim_z);
	assert(img.data_type == ASTCENC_TYPE_F32);

	float* data = static_cast<float*>(img.data[z]);

	data[(4 * img.dim_x * y) + (4 * x    )] = pixel.lane<0>();
	data[(4 * img.dim_x * y) + (4 * x + 1)] = pixel.lane<1>();
	data[(4 * img.dim_x * y) + (4 * x + 2)] = pixel.lane<2>();
	data[(4 * img.dim_x * y) + (4 * x + 3)] = pixel.lane<3>();
}

/**
 * @brief Set the value of a single pixel in an image.
 *
 * @param[out] img     The output image; must use F32 texture components.
 * @param      x       The pixel x coordinate.
 * @param      y       The pixel y coordinate.
 * @param      pixel   The pixel color value to write.
 */
static void image_set_pixel_u8(
	astcenc_image& img,
	size_t x,
	size_t y,
	vint4 pixel
) {
	// We should never escape bounds
	assert(x < img.dim_x);
	assert(y < img.dim_y);
	assert(img.data_type == ASTCENC_TYPE_U8);

	uint8_t* data = static_cast<uint8_t*>(img.data[0]);
	pixel = pack_low_bytes(pixel);
	store_nbytes(pixel, data + (4 * img.dim_x * y) + (4 * x    ));
}

/**
 * @brief Create a copy of @c input with forced unit-length normal vectors.
 *
 * It is assumed that all normal vectors are stored in the RGB components, and
 * stored in a packed unsigned range of [0,1] which must be unpacked prior
 * normalization. Data must then be repacked into this form for handing over to
 * the core codec.
 *
 * @param[in]  input    The input image.
 * @param[out] output   The output image, must use F32 components.
 */
static void image_preprocess_normalize(
	const astcenc_image& input,
	astcenc_image& output
) {
	for (unsigned int z = 0; z < input.dim_z; z++)
	{
		for (unsigned int y = 0; y < input.dim_y; y++)
		{
			for (unsigned int x = 0; x < input.dim_x; x++)
			{
				vfloat4 pixel = image_get_pixel(input, x, y, z);

				// Stash alpha component and zero
				float a = pixel.lane<3>();
				pixel.set_lane<3>(0.0f);

				// Decode [0,1] normals to [-1,1]
				pixel.set_lane<0>((pixel.lane<0>() * 2.0f) - 1.0f);
				pixel.set_lane<1>((pixel.lane<1>() * 2.0f) - 1.0f);
				pixel.set_lane<2>((pixel.lane<2>() * 2.0f) - 1.0f);

				// Normalize pixel and restore alpha
				pixel = normalize(pixel);
				pixel.set_lane<3>(a);

				// Encode [-1,1] normals to [0,1]
				pixel.set_lane<0>((pixel.lane<0>() + 1.0f) / 2.0f);
				pixel.set_lane<1>((pixel.lane<1>() + 1.0f) / 2.0f);
				pixel.set_lane<2>((pixel.lane<2>() + 1.0f) / 2.0f);

				image_set_pixel(output, x, y, z, pixel);
			}
		}
	}
}

/**
 * @brief Linearize an sRGB value.
 *
 * @return The linearized value.
 */
static float srgb_to_linear(
	float a
) {
	if (a <= 0.04045f)
	{
		return a * (1.0f / 12.92f);
	}

	return powf((a + 0.055f) * (1.0f / 1.055f), 2.4f);
}

/**
 * @brief sRGB gamma-encode a linear value.
 *
 * @return The gamma encoded value.
 */
static float linear_to_srgb(
	float a
) {
	if (a <= 0.0031308f)
	{
		return a * 12.92f;
	}

	return 1.055f * powf(a, 1.0f / 2.4f) - 0.055f;
}

/**
 * @brief Create a copy of @c input with premultiplied color data.
 *
 * If we are compressing sRGB data we linearize the data prior to
 * premultiplication and re-gamma-encode afterwards.
 *
 * @param[in]  input     The input image.
 * @param[out] output    The output image, must use F32 components.
 * @param      profile   The encoding profile.
 */
static void image_preprocess_premultiply(
	const astcenc_image& input,
	astcenc_image& output,
	astcenc_profile profile
) {
	for (unsigned int z = 0; z < input.dim_z; z++)
	{
		for (unsigned int y = 0; y < input.dim_y; y++)
		{
			for (unsigned int x = 0; x < input.dim_x; x++)
			{
				vfloat4 pixel = image_get_pixel(input, x, y, z);

				// Linearize sRGB
				if (profile == ASTCENC_PRF_LDR_SRGB)
				{
					pixel.set_lane<0>(srgb_to_linear(pixel.lane<0>()));
					pixel.set_lane<1>(srgb_to_linear(pixel.lane<1>()));
					pixel.set_lane<2>(srgb_to_linear(pixel.lane<2>()));
				}

				// Premultiply pixel in linear-space
				pixel.set_lane<0>(pixel.lane<0>() * pixel.lane<3>());
				pixel.set_lane<1>(pixel.lane<1>() * pixel.lane<3>());
				pixel.set_lane<2>(pixel.lane<2>() * pixel.lane<3>());

				// Gamma-encode sRGB
				if (profile == ASTCENC_PRF_LDR_SRGB)
				{
					pixel.set_lane<0>(linear_to_srgb(pixel.lane<0>()));
					pixel.set_lane<1>(linear_to_srgb(pixel.lane<1>()));
					pixel.set_lane<2>(linear_to_srgb(pixel.lane<2>()));
				}

				image_set_pixel(output, x, y, z, pixel);
			}
		}
	}
}

/**
 * @brief Populate a single diagnostic image showing aspects of the encoding.
 *
 * @param context      The context to use.
 * @param image        The compressed image to analyze.
 * @param diag_image   The output visualization image to populate.
 * @param texel_func   The per-texel callback used to determine output color.
 */
static void print_diagnostic_image(
	astcenc_context* context,
	const astc_compressed_image& image,
	astcenc_image& diag_image,
	std::function<vint4(astcenc_block_info&, size_t, size_t)> texel_func
) {
	size_t block_cols = (image.dim_x + image.block_x - 1) / image.block_x;
	size_t block_rows = (image.dim_y + image.block_y - 1) / image.block_y;

	uint8_t* data = image.data;
	for (size_t block_y = 0; block_y < block_rows; block_y++)
	{
		for (size_t block_x = 0; block_x < block_cols; block_x++)
		{
			astcenc_block_info block_info;
			astcenc_get_block_info(context, data, &block_info);
			data += 16;

			size_t start_row = block_y * image.block_y;
			size_t start_col = block_x * image.block_x;

			size_t end_row = astc::min(start_row + image.block_y, static_cast<size_t>(image.dim_y));
			size_t end_col = astc::min(start_col + image.block_x, static_cast<size_t>(image.dim_x));

			for (size_t texel_y = start_row; texel_y < end_row; texel_y++)
			{
				for (size_t texel_x = start_col; texel_x < end_col; texel_x++)
				{
					vint4 color = texel_func(block_info, texel_x - start_col, texel_y - start_row);
					image_set_pixel_u8(diag_image, texel_x, texel_y, color);
				}
			}
		}
	}
}

/**
 * @brief Print a set of diagnostic images showing aspects of the encoding.
 *
 * @param context       The context to use.
 * @param image         The compressed image to analyze.
 * @param output_file   The output file name to use as a stem for new names.
 */
static void print_diagnostic_images(
	astcenc_context* context,
	const astc_compressed_image& image,
	const std::string& output_file
) {
	if (image.dim_z != 1)
	{
		return;
	}

	// Try to find a file extension we know about
	size_t index = output_file.find_last_of(".");
	std::string stem = output_file;
	if (index != std::string::npos)
	{
		stem = stem.substr(0, index);
	}

	auto diag_image = alloc_image(8, image.dim_x, image.dim_y, image.dim_z);

	// ---- ---- ---- ---- Partitioning ---- ---- ---- ----
	auto partition_func = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		const vint4 colors[] {
			vint4(  0,   0,   0, 255),
			vint4(255,   0,   0, 255),
			vint4(  0, 255,   0, 255),
			vint4(  0,   0, 255, 255),
			vint4(255, 255, 255, 255)
		};

		size_t texel_index = texel_y * info.block_x + texel_x;

		int partition { 0 };
		if (!info.is_constant_block)
		{
			partition = info.partition_assignment[texel_index] + 1;
		}

		return colors[partition];
	};

	print_diagnostic_image(context, image, *diag_image, partition_func);
	std::string fname = stem + "_diag_partitioning.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Weight planes  ---- ---- ---- ----
	auto texel_func1 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		const vint4 colors[] {
			vint4(  0,   0,   0, 255),
			vint4(255,   0,   0, 255),
			vint4(  0, 255,   0, 255),
			vint4(  0,   0, 255, 255),
			vint4(255, 255, 255, 255)
		};

		int component { 0 };
		if (info.is_dual_plane_block)
		{
			component = info.dual_plane_component + 1;
		}

		return colors[component];
	};

	print_diagnostic_image(context, image, *diag_image, texel_func1);
	fname = stem + "_diag_weight_plane2.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Weight density  ---- ---- ---- ----
	auto texel_func2 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		float density = 0.0f;
		if (!info.is_constant_block)
		{
			float texel_count = static_cast<float>(info.block_x * info.block_y);
			float weight_count = static_cast<float>(info.weight_x * info.weight_y);
			density = weight_count / texel_count;
		}

		int densityi = static_cast<int>(255.0f * density);
		return vint4(densityi, densityi, densityi, 255);
	};

	print_diagnostic_image(context, image, *diag_image, texel_func2);
	fname = stem + "_diag_weight_density.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Weight quant  ---- ---- ---- ----
	auto texel_func3 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		int quant { 0 };
		if (!info.is_constant_block)
		{
			quant = info.weight_level_count - 1;
		}

		return vint4(quant, quant, quant, 255);
	};

	print_diagnostic_image(context, image, *diag_image, texel_func3);
	fname = stem + "_diag_weight_quant.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Color quant  ---- ---- ---- ----
	auto texel_func4 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		int quant { 0 };
		if (!info.is_constant_block)
		{
			quant = info.color_level_count - 1;
		}

		return vint4(quant, quant, quant, 255);
	};

	print_diagnostic_image(context, image, *diag_image, texel_func4);
	fname = stem + "_diag_color_quant.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Color endpoint mode: Index ---- ---- ---- ----
	auto texel_func5 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		size_t texel_index = texel_y * info.block_x + texel_x;

		int cem { 255 };
		if (!info.is_constant_block)
		{
			uint8_t partition = info.partition_assignment[texel_index];
			cem = info.color_endpoint_modes[partition] * 16;
		}

		return vint4(cem, cem, cem, 255);
	};

	print_diagnostic_image(context, image, *diag_image, texel_func5);
	fname = stem + "_diag_cem_index.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Color endpoint mode: Components ---- ---- ---- ----
	auto texel_func6 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		const vint4 colors[] {
			vint4(  0,   0,   0, 255),
			vint4(255,   0,   0, 255),
			vint4(  0, 255,   0, 255),
			vint4(  0,   0, 255, 255),
			vint4(255, 255, 255, 255)
		};

		size_t texel_index = texel_y * info.block_x + texel_x;

		int components { 0 };
		if (!info.is_constant_block)
		{
			uint8_t partition = info.partition_assignment[texel_index];
			uint8_t cem = info.color_endpoint_modes[partition];

			switch (cem)
			{
				case 0:
				case 1:
				case 2:
				case 3:
					components = 1;
					break;
				case 4:
				case 5:
					components = 2;
					break;
				case 6:
				case 7:
				case 8:
				case 9:
				case 11:
					components = 3;
					break;
				default:
					components = 4;
					break;
			}
		}

		return colors[components];
	};

	print_diagnostic_image(context, image, *diag_image, texel_func6);
	fname = stem + "_diag_cem_components.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Color endpoint mode: Style ---- ---- ---- ----
	auto texel_func7 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		const vint4 colors[] {
			vint4(  0,   0,   0, 255),
			vint4(255,   0,   0, 255),
			vint4(  0, 255,   0, 255),
			vint4(  0,   0, 255, 255),
		};

		size_t texel_index = texel_y * info.block_x + texel_x;

		int style { 0 };
		if (!info.is_constant_block)
		{
			uint8_t partition = info.partition_assignment[texel_index];
			uint8_t cem = info.color_endpoint_modes[partition];

			switch (cem)
			{
				// Direct - two absolute endpoints
				case 0:
				case 1:
				case 2:
				case 3:
				case 4:
				case 8:
				case 11:
				case 12:
				case 14:
				case 15:
					style = 1;
					break;
				// Offset - one absolute plus delta
				case 5:
				case 9:
				case 13:
					style = 2;
					break;
				// Scale - one absolute plus scale
				case 6:
				case 7:
				case 10:
					style = 3;
					break;
				// Shouldn't happen ...
				default:
					style = 0;
					break;
			}
		}

		return colors[style];
	};

	print_diagnostic_image(context, image, *diag_image, texel_func7);
	fname = stem + "_diag_cem_style.png";
	store_ncimage(diag_image, fname.c_str(), false);

	// ---- ---- ---- ---- Color endpoint mode: Style ---- ---- ---- ----
	auto texel_func8 = [](astcenc_block_info& info, size_t texel_x, size_t texel_y) {
		(void)texel_x;
		(void)texel_y;

		size_t texel_index = texel_y * info.block_x + texel_x;

		int style { 0 };
		if (!info.is_constant_block)
		{
			uint8_t partition = info.partition_assignment[texel_index];
			uint8_t cem = info.color_endpoint_modes[partition];

			switch (cem)
			{
				// LDR blocks
				case 0:
				case 1:
				case 4:
				case 5:
				case 6:
				case 8:
				case 9:
				case 10:
				case 12:
				case 13:
					style = 128;
					break;
				// HDR blocks
				default:
					style = 155;
					break;
			}
		}

		return vint4(style, style, style, 255);
	};

	print_diagnostic_image(context, image, *diag_image, texel_func8);
	fname = stem + "_diag_cem_hdr.png";
	store_ncimage(diag_image, fname.c_str(), false);

	free_image(diag_image);
}

/**
 * @brief The main entry point.
 *
 * @param argc   The number of arguments.
 * @param argv   The vector of arguments.
 *
 * @return 0 on success, non-zero otherwise.
 */
int main(
	int argc,
	char **argv
) {
	double start_time = get_time();

	if (argc < 2)
	{
		astcenc_print_shorthelp();
		return 0;
	}

	astcenc_operation operation;
	astcenc_profile profile;
	int error = parse_commandline_options(argc, argv, operation, profile);
	if (error)
	{
		return 1;
	}

	switch (operation)
	{
	case ASTCENC_OP_HELP:
		astcenc_print_longhelp();
		return 0;
	case ASTCENC_OP_VERSION:
		astcenc_print_header();
		return 0;
	default:
		break;
	}

	std::string input_filename = argc >= 3 ? argv[2] : "";
	std::string output_filename = argc >= 4 ? argv[3] : "";

	if (input_filename.empty())
	{
		printf("ERROR: Input file not specified\n");
		return 1;
	}

	if (output_filename.empty())
	{
		printf("ERROR: Output file not specified\n");
		return 1;
	}

	// TODO: Handle RAII resources so they get freed when out of scope
	// Load the compressed input file if needed

	// This has to come first, as the block size is in the file header
	astc_compressed_image image_comp {};
	if (operation & ASTCENC_STAGE_LD_COMP)
	{
		if (ends_with(input_filename, ".astc"))
		{
			error = load_cimage(input_filename.c_str(), image_comp);
			if (error)
			{
				return 1;
			}
		}
		else if (ends_with(input_filename, ".ktx"))
		{
			bool is_srgb;
			error = load_ktx_compressed_image(input_filename.c_str(), is_srgb, image_comp);
			if (error)
			{
				return 1;
			}

			if (is_srgb && (profile != ASTCENC_PRF_LDR_SRGB))
			{
				printf("WARNING: Input file is sRGB, but decompressing as linear\n");
			}

			if (!is_srgb && (profile == ASTCENC_PRF_LDR_SRGB))
			{
				printf("WARNING: Input file is linear, but decompressing as sRGB\n");
			}
		}
		else
		{
			printf("ERROR: Unknown compressed input file type\n");
			return 1;
		}
	}

	astcenc_config config {};
	astcenc_preprocess preprocess;
	error = init_astcenc_config(argc, argv, profile, operation, image_comp, preprocess, config);
	if (error)
	{
		return 1;
	}

	// Initialize cli_config_options with default values
	cli_config_options cli_config { 0, 1, 1, false, false, false, -10, 10,
		{ ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A },
		{ ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A } };

	error = edit_astcenc_config(argc, argv, operation, cli_config, config);
	if (error)
	{
		return 1;
	}

	astcenc_image* image_uncomp_in = nullptr ;
	unsigned int image_uncomp_in_component_count = 0;
	bool image_uncomp_in_is_hdr = false;
	astcenc_image* image_decomp_out = nullptr;

	// TODO: Handle RAII resources so they get freed when out of scope
	astcenc_error    codec_status;
	astcenc_context* codec_context;


	// Preflight - check we have valid extensions for storing a file
	if (operation & ASTCENC_STAGE_ST_NCOMP)
	{
		int bitness = get_output_filename_enforced_bitness(output_filename.c_str());
		if (bitness < 0)
		{
			const char *eptr = strrchr(output_filename.c_str(), '.');
			eptr = eptr ? eptr : "";
			printf("ERROR: Unknown uncompressed output file type '%s'\n", eptr);
			return 1;
		}
	}

	if (operation & ASTCENC_STAGE_ST_COMP)
	{
#if defined(_WIN32)
		bool is_null = output_filename == "NUL" || output_filename == "nul";
#else
		bool is_null = output_filename == "/dev/null";
#endif

		if (!(is_null || ends_with(output_filename, ".astc") || ends_with(output_filename, ".ktx")))
		{
			const char *eptr = strrchr(output_filename.c_str(), '.');
			eptr = eptr ? eptr : "";
			printf("ERROR: Unknown compressed output file type '%s'\n", eptr);
			return 1;
		}
	}

	codec_status = astcenc_context_alloc(&config, cli_config.thread_count, &codec_context);
	if (codec_status != ASTCENC_SUCCESS)
	{
		printf("ERROR: Codec context alloc failed: %s\n", astcenc_get_error_string(codec_status));
		return 1;
	}

	// Load the uncompressed input file if needed
	if (operation & ASTCENC_STAGE_LD_NCOMP)
	{
		image_uncomp_in = load_uncomp_file(
		    input_filename.c_str(), cli_config.array_size, cli_config.y_flip,
		    image_uncomp_in_is_hdr, image_uncomp_in_component_count);
		if (!image_uncomp_in)
		{
			printf ("ERROR: Failed to load uncompressed image file\n");
			return 1;
		}


		if (preprocess != ASTCENC_PP_NONE)
		{
			// Allocate a float image so we can avoid additional quantization,
			// as e.g. premultiplication can result in fractional color values
			astcenc_image* image_pp = alloc_image(32,
			                                      image_uncomp_in->dim_x,
			                                      image_uncomp_in->dim_y,
			                                      image_uncomp_in->dim_z);
			if (!image_pp)
			{
				printf ("ERROR: Failed to allocate preprocessed image\n");
				return 1;
			}

			if (preprocess == ASTCENC_PP_NORMALIZE)
			{
				image_preprocess_normalize(*image_uncomp_in, *image_pp);
			}

			if (preprocess == ASTCENC_PP_PREMULTIPLY)
			{
				image_preprocess_premultiply(*image_uncomp_in, *image_pp,
				                             config.profile);
			}

			// Delete the original as we no longer need it
			free_image(image_uncomp_in);
			image_uncomp_in = image_pp;
		}

		if (!cli_config.silentmode)
		{
			printf("Source image\n");
			printf("============\n\n");
			printf("    Source:                     %s\n", input_filename.c_str());
			printf("    Color profile:              %s\n", image_uncomp_in_is_hdr ? "HDR" : "LDR");
			if (image_uncomp_in->dim_z > 1)
			{
				printf("    Dimensions:                 3D, %ux%ux%u\n",
				       image_uncomp_in->dim_x, image_uncomp_in->dim_y, image_uncomp_in->dim_z);
			}
			else
			{
				printf("    Dimensions:                 2D, %ux%u\n",
				       image_uncomp_in->dim_x, image_uncomp_in->dim_y);
			}
			printf("    Components:                 %d\n\n", image_uncomp_in_component_count);
		}
	}

	double image_size = 0.0;
	if (image_uncomp_in)
	{
		image_size = static_cast<double>(image_uncomp_in->dim_x) *
		             static_cast<double>(image_uncomp_in->dim_y) *
		             static_cast<double>(image_uncomp_in->dim_z);
	}
	else
	{
		image_size = static_cast<double>(image_comp.dim_x) *
		             static_cast<double>(image_comp.dim_y) *
		             static_cast<double>(image_comp.dim_z);
	}

	// Compress an image
	double best_compression_time = 100000.0;
	double total_compression_time = 0.0;
	if (operation & ASTCENC_STAGE_COMPRESS)
	{
		print_astcenc_config(cli_config, config);

		unsigned int blocks_x = (image_uncomp_in->dim_x + config.block_x - 1) / config.block_x;
		unsigned int blocks_y = (image_uncomp_in->dim_y + config.block_y - 1) / config.block_y;
		unsigned int blocks_z = (image_uncomp_in->dim_z + config.block_z - 1) / config.block_z;
		size_t buffer_size = blocks_x * blocks_y * blocks_z * 16;
		uint8_t* buffer = new uint8_t[buffer_size];

		compression_workload work;
		work.context = codec_context;
		work.image = image_uncomp_in;
		work.swizzle = cli_config.swz_encode;
		work.data_out = buffer;
		work.data_len = buffer_size;
		work.error = ASTCENC_SUCCESS;

		// Only launch worker threads for multi-threaded use - it makes basic
		// single-threaded profiling and debugging a little less convoluted
		double start_compression_time = get_time();
		for (unsigned int i = 0; i < cli_config.repeat_count; i++)
		{
			double start_iter_time = get_time();
			if (cli_config.thread_count > 1)
			{
				launch_threads(cli_config.thread_count, compression_workload_runner, &work);
			}
			else
			{
				work.error = astcenc_compress_image(
					work.context, work.image, &work.swizzle,
					work.data_out, work.data_len, 0);
			}

			astcenc_compress_reset(codec_context);

			double iter_time = get_time() - start_iter_time;
			best_compression_time = astc::min(iter_time, best_compression_time);
		}
		total_compression_time = get_time() - start_compression_time;

		if (work.error != ASTCENC_SUCCESS)
		{
			printf("ERROR: Codec compress failed: %s\n", astcenc_get_error_string(work.error));
			return 1;
		}

		image_comp.block_x = config.block_x;
		image_comp.block_y = config.block_y;
		image_comp.block_z = config.block_z;
		image_comp.dim_x = image_uncomp_in->dim_x;
		image_comp.dim_y = image_uncomp_in->dim_y;
		image_comp.dim_z = image_uncomp_in->dim_z;
		image_comp.data = buffer;
		image_comp.data_len = buffer_size;
	}

	// Decompress an image
	double best_decompression_time = 100000.0;
	double total_decompression_time = 0.0;
	if (operation & ASTCENC_STAGE_DECOMPRESS)
	{
		int out_bitness = get_output_filename_enforced_bitness(output_filename.c_str());
		if (out_bitness == 0)
		{
			bool is_hdr = (config.profile == ASTCENC_PRF_HDR) || (config.profile == ASTCENC_PRF_HDR_RGB_LDR_A);
			out_bitness = is_hdr ? 16 : 8;
		}

		image_decomp_out = alloc_image(
		    out_bitness, image_comp.dim_x, image_comp.dim_y, image_comp.dim_z);

		decompression_workload work;
		work.context = codec_context;
		work.data = image_comp.data;
		work.data_len = image_comp.data_len;
		work.image_out = image_decomp_out;
		work.swizzle = cli_config.swz_decode;
		work.error = ASTCENC_SUCCESS;

		// Only launch worker threads for multi-threaded use - it makes basic
		// single-threaded profiling and debugging a little less convoluted
		double start_decompression_time = get_time();
		for (unsigned int i = 0; i < cli_config.repeat_count; i++)
		{
			double start_iter_time = get_time();
			if (cli_config.thread_count > 1)
			{
				launch_threads(cli_config.thread_count, decompression_workload_runner, &work);
			}
			else
			{
				work.error = astcenc_decompress_image(
				    work.context, work.data, work.data_len,
				    work.image_out, &work.swizzle, 0);
			}

			astcenc_decompress_reset(codec_context);

			double iter_time = get_time() - start_iter_time;
			best_decompression_time = astc::min(iter_time, best_decompression_time);
		}
		total_decompression_time = get_time() - start_decompression_time;

		if (work.error != ASTCENC_SUCCESS)
		{
			printf("ERROR: Codec decompress failed: %s\n", astcenc_get_error_string(codec_status));
			return 1;
		}
	}

#if defined(_WIN32)
	bool is_null = output_filename == "NUL" || output_filename == "nul";
#else
	bool is_null = output_filename == "/dev/null";
#endif

   // Print metrics in comparison mode
	if (operation & ASTCENC_STAGE_COMPARE)
	{
		bool is_normal_map = config.flags & ASTCENC_FLG_MAP_NORMAL;

		compute_error_metrics(
		    image_uncomp_in_is_hdr, is_normal_map, image_uncomp_in_component_count,
		    image_uncomp_in, image_decomp_out, cli_config.low_fstop, cli_config.high_fstop);
	}

	// Store compressed image
	if (operation & ASTCENC_STAGE_ST_COMP)
	{
		if (ends_with(output_filename, ".astc"))
		{
			error = store_cimage(image_comp, output_filename.c_str());
			if (error)
			{
				printf ("ERROR: Failed to store compressed image\n");
				return 1;
			}
		}
		else if (ends_with(output_filename, ".ktx"))
		{
			bool srgb = profile == ASTCENC_PRF_LDR_SRGB;
			error = store_ktx_compressed_image(image_comp, output_filename.c_str(), srgb);
			if (error)
			{
				printf ("ERROR: Failed to store compressed image\n");
				return 1;
			}
		}
		else
		{
			if (!is_null)
			{
				printf("ERROR: Unknown compressed output file type\n");
				return 1;
			}
		}
	}

	// Store decompressed image
	if (operation & ASTCENC_STAGE_ST_NCOMP)
	{
		if (!is_null)
		{
			bool store_result = store_ncimage(image_decomp_out, output_filename.c_str(),
			                                  cli_config.y_flip);
			if (!store_result)
			{
				printf("ERROR: Failed to write output image %s\n", output_filename.c_str());
				return 1;
			}
		}
	}

	// Store diagnostic images
	if (cli_config.diagnostic_images && !is_null)
	{
		print_diagnostic_images(codec_context, image_comp, output_filename);
	}

	free_image(image_uncomp_in);
	free_image(image_decomp_out);
	astcenc_context_free(codec_context);

	delete[] image_comp.data;

	if ((operation & ASTCENC_STAGE_COMPARE) || (!cli_config.silentmode))
	{
		double end_time = get_time();

		double repeats = static_cast<double>(cli_config.repeat_count);
		double avg_compression_time = total_compression_time / repeats;
		double avg_decompression_time = total_decompression_time / repeats;
		double total_time = (end_time - start_time) - ((repeats - 1.0) * avg_compression_time)  - ((repeats - 1.0) * avg_decompression_time);

		printf("Performance metrics\n");
		printf("===================\n\n");
		printf("    Total time:                %8.4f s\n", total_time);

		if (operation & ASTCENC_STAGE_COMPRESS)
		{
			double compression_rate = image_size / (best_compression_time * 1000000.0);

			printf("    Coding time:               %8.4f s\n", best_compression_time);
			printf("    Coding rate:               %8.4f MT/s\n", compression_rate);
		}

		if (operation & ASTCENC_STAGE_DECOMPRESS)
		{
			double decompression_rate = image_size / (best_decompression_time * 1000000.0);
			printf("    Decoding time:             %8.4f s\n", best_decompression_time);
			printf("    Decoding rate:             %8.4f MT/s\n", decompression_rate);
		}
	}

	return 0;
}
