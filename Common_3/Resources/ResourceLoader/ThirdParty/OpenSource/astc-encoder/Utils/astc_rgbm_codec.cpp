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

// This is a utility tool to encode HDR into RGBM, or decode RGBM into HDR.

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "astcenc_mathlib.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define MODE_ENCODE 0
#define MODE_DECODE 1

int main(int argc, char **argv)
{
	// Parse command line
	if (argc != 6)
	{
		printf("Usage: astc_rgbm_codec [-ch|-dh] <M> <low_clamp> <source> <dest>\n");
		exit(1);
	}

	int opmode;
	if (strcmp(argv[1], "-ch") == 0)
	{
		opmode = MODE_ENCODE;
	}
	else if (strcmp(argv[1], "-dh") == 0)
	{
		opmode = MODE_DECODE;
	}
	else
	{
		printf("ERROR: Bad operation mode\n");
		exit(1);
	}

 	float rgbm_multiplier = atof(argv[2]);
 	float low_clamp = atof(argv[3]);

	const char* src_file = argv[4];
	const char* dst_file = argv[5];

	// Convert an HDR input file into an RGBM encoded LDR file
	if (opmode == MODE_ENCODE)
	{
		// Load the input image
		int dim_x;
		int dim_y;
		const float* data_in = stbi_loadf(src_file, &dim_x, &dim_y, nullptr, 4);
		if (!data_in)
		{
			printf("ERROR: Failed to load input image.\n");
			exit(1);
		}

		// Allocate the output image
		uint8_t* data_out = (uint8_t*)malloc(4 * dim_y * dim_x);
		if (!data_out)
		{
			printf("ERROR: Failed to allow output image.\n");
			exit(1);
		}

		// For each pixel apply RGBM encoding
		for (int y = 0; y < dim_y; y++)
		{
			const float* row_in = data_in + (4 * dim_x * y);
			uint8_t* row_out = data_out + (4 * dim_x * y);

			for (int x = 0; x < dim_x; x++)
			{
				const float* pixel_in = row_in + 4 * x;
				uint8_t* pixel_out = row_out + 4 * x;

				float r_in = pixel_in[0] / rgbm_multiplier;
				float g_in = pixel_in[1] / rgbm_multiplier;
				float b_in = pixel_in[2] / rgbm_multiplier;

				float max_rgb = astc::max(r_in, g_in, b_in);

				// Ensure we always round up to next largest M
				float m_scale = astc::min(1.0f, ceil(max_rgb * 255.0f) / 255.0f);

				// But keep well above zero to avoid clamps in the compressor
				m_scale = astc::max(m_scale, low_clamp / 255.0f);

				float r_scale = astc::min(1.0f, r_in / m_scale);
				float g_scale = astc::min(1.0f, g_in / m_scale);
				float b_scale = astc::min(1.0f, b_in / m_scale);

				pixel_out[0] = (uint8_t)(r_scale * 255.0f);
				pixel_out[1] = (uint8_t)(g_scale * 255.0f);
				pixel_out[2] = (uint8_t)(b_scale * 255.0f);
				pixel_out[3] = (uint8_t)(m_scale * 255.0f);
			}
		}

		// Write out the result
		stbi_write_png(dst_file, dim_x, dim_y, 4, data_out, 4 * dim_x);
	}
	// Convert an RGBM encoded LDR file into an HDR file
	else
	{
		// Load the input image
		int dim_x;
		int dim_y;
		const uint8_t* data_in = stbi_load(src_file, &dim_x, &dim_y, nullptr, 4);
		if (!data_in)
		{
			printf("ERROR: Failed to load input image.\n");
			exit(1);
		}

		// Allocate the output image
		float* data_out = (float*)malloc(4 * dim_y * dim_x * sizeof(float));
		if (!data_out)
		{
			printf("ERROR: Failed to allow output image.\n");
			exit(1);
		}

		// For each pixel apply RGBM decoding
		for (int y = 0; y < dim_y; y++)
		{
			const uint8_t* row_in = data_in + (4 * dim_x * y);
			float* row_out = data_out + (4 * dim_x * y);

			for (int x = 0; x < dim_x; x++)
			{
				const uint8_t* pixel_in = row_in + 4 * x;
				float* pixel_out = row_out + 4 * x;

				float r_scale = ((float)pixel_in[0]) / 255.0f;
				float g_scale = ((float)pixel_in[1]) / 255.0f;
				float b_scale = ((float)pixel_in[2]) / 255.0f;

				float m_scale = ((float)pixel_in[3]) / 255.0f;

				pixel_out[0] = r_scale * (m_scale * rgbm_multiplier);
				pixel_out[1] = g_scale * (m_scale * rgbm_multiplier);
				pixel_out[2] = b_scale * (m_scale * rgbm_multiplier);
				pixel_out[3] = 1.0f;
			}
		}

		// Write out the result
		stbi_write_hdr(dst_file, dim_x, dim_y, 4, data_out);
	}

	return 0;
}
