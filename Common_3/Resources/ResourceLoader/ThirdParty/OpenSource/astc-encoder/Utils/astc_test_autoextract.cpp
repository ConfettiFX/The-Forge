// SPDX-License-Identifier: Apache-2.0
// ----------------------------------------------------------------------------
// Copyright 2021 Arm Limited
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

// Overview
// ========
//
// This is a utility tool to automatically generate single tile test vectors
// out of a larger test image. This tool takes three input images:
//
//    - the uncompressed referenced,
//    - the known-good compressed reference,
//    - a new compressed image.
//
// The two compressed images are compared block-by-block, and if any block
// differences are found the worst block is extracted from the uncompressed
// reference and written back to disk as a single tile output image.
//
// Limitations
// ===========
//
// This tool only currently supports 2D LDR images.
//
// Build
// =====
//
// g++ astc_test_autoextract.cpp -I../Source -o astc_test_autoextract

#include <stdio.h>
#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/**
 * @brief Compute the array offset in a 2D image
 */
int pix(int x_pix, int y_idx, int x_idx, int chans, int p_idx)
{
	return ((y_idx * x_pix) + x_idx) * chans + p_idx;
}

int main(int argc, char **argv)
{

	// Parse command line
	if (argc < 6)
	{
		printf("Usage: astc_test_extract <blocksize> <ref> <good> <bad> <out>\n");
		return 1;
	}

	int blockdim_x, blockdim_y;
	if (sscanf(argv[1], "%dx%d", &blockdim_x, &blockdim_y) < 2)
	{
		printf("blocksize must be of form WxH; e.g. 8x4\n");
		return 1;
	}

	// Load the original reference image
	int ref_dim_x, ref_dim_y, ref_ncomp;
	uint8_t* data_ref = (uint8_t*)stbi_load(argv[2], &ref_dim_x, &ref_dim_y, &ref_ncomp, 4);
	if (!data_ref)
	{
		printf("Failed to load reference image.\n");
		return 1;
	}

	// Load the good test image
	int good_dim_x, good_dim_y, good_ncomp;
	uint8_t* data_good = (uint8_t*)stbi_load(argv[3], &good_dim_x, &good_dim_y, &good_ncomp, 4);
	if (!data_good)
	{
		printf("Failed to load good test image.\n");
		return 1;
	}

	// Load the bad test image
	int bad_dim_x, bad_dim_y, bad_ncomp;
	uint8_t* data_bad = (uint8_t*)stbi_load(argv[4], &bad_dim_x, &bad_dim_y, &bad_ncomp, 4);
	if (!data_bad)
	{
		printf("Failed to load bad test image.\n");
		return 1;
	}

	if (ref_dim_x != good_dim_x || ref_dim_x != bad_dim_x ||
		ref_dim_y != good_dim_y || ref_dim_y != bad_dim_y)
	{
		printf("Failed as images are different resolutions.\n");
		return 1;
	}


	int x_blocks = (ref_dim_x + blockdim_x - 1) / blockdim_x;
	int y_blocks = (ref_dim_y + blockdim_y - 1) / blockdim_y;

	int *errorsums = (int*)malloc(x_blocks * y_blocks * 4);
	for (int i = 0; i < x_blocks * y_blocks; i++)
	{
		errorsums[i] = 0;
	}

	// Diff the two test images to find blocks that differ
	for (int y = 0; y < ref_dim_y; y++)
	{
		for (int x = 0; x < ref_dim_x; x++)
		{
			int x_block = x / blockdim_x;
			int y_block = y / blockdim_y;

			int r_gd = data_good[pix(ref_dim_x, y, x, 4, 0)];
			int g_gd = data_good[pix(ref_dim_x, y, x, 4, 1)];
			int b_gd = data_good[pix(ref_dim_x, y, x, 4, 2)];
			int a_gd = data_good[pix(ref_dim_x, y, x, 4, 3)];

			int r_bd = data_bad[pix(ref_dim_x, y, x, 4, 0)];
			int g_bd = data_bad[pix(ref_dim_x, y, x, 4, 1)];
			int b_bd = data_bad[pix(ref_dim_x, y, x, 4, 2)];
			int a_bd = data_bad[pix(ref_dim_x, y, x, 4, 3)];

			int r_diff = (r_gd - r_bd) * (r_gd - r_bd);
			int g_diff = (g_gd - g_bd) * (g_gd - g_bd);
			int b_diff = (b_gd - b_bd) * (b_gd - b_bd);
			int a_diff = (a_gd - a_bd) * (a_gd - a_bd);

			int diff = r_diff + g_diff + b_diff + a_diff;
			errorsums[pix(x_blocks, y_block, x_block, 1, 0)] += diff;
		}
	}

	// Diff the two test images to find blocks that differ
	float worst_error = 0.0f;
	int worst_x_block = 0;
	int worst_y_block = 0;
	for (int y = 0; y < y_blocks; y++)
	{
		for (int x = 0; x < x_blocks; x++)
		{
			float error = errorsums[pix(x_blocks, y, x, 1, 0)];
			if (error > worst_error)
			{
				worst_error = error;
				worst_x_block = x;
				worst_y_block = y;
			}
		}
	}

	if (worst_error == 0.0f)
	{
		printf("No block errors found\n");
	}
	else
	{
		int start_y = worst_y_block * blockdim_y;
		int start_x = worst_x_block * blockdim_x;

		int end_y = (worst_y_block + 1) * blockdim_y;
		int end_x = (worst_x_block + 1) * blockdim_x;

		if (end_x > ref_dim_x)
		{
			end_x = ref_dim_x;
		}

		if (end_y > ref_dim_y)
		{
			end_y = ref_dim_y;
		}

		int outblk_x = end_x - start_x;
		int outblk_y = end_y - start_y;

		printf("Block errors found at ~(%u, %u) px\n", start_x, start_y);

		// Write out the worst bad block (from original reference)
		uint8_t* data_out = &(data_ref[pix(ref_dim_x, start_y, start_x, 4, 0)]);
		stbi_write_png(argv[5], outblk_x, outblk_y, 4, data_out, 4 * ref_dim_x);
	}

	free(errorsums);
	stbi_image_free(data_ref);
	stbi_image_free(data_good);
	stbi_image_free(data_bad);
	return 0;
}
