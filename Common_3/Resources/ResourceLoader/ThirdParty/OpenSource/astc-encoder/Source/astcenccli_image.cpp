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
 * @brief Functions for creating in-memory ASTC image structures.
 */

#include <cassert>
#include <cstring>

#include "astcenccli_internal.h"

/* See header for documentation. */
astcenc_image *alloc_image(
	unsigned int bitness,
	unsigned int dim_x,
	unsigned int dim_y,
	unsigned int dim_z
) {
	astcenc_image *img = tf_new(astcenc_image);
	img->dim_x = dim_x;
	img->dim_y = dim_y;
	img->dim_z = dim_z;

	void** data = (void**)tf_malloc(sizeof(void*) * dim_z);
	img->data = data;

	if (bitness == 8)
	{
		img->data_type = ASTCENC_TYPE_U8;
		for (unsigned int z = 0; z < dim_z; z++)
		{
			data[z] = (uint8_t*)tf_malloc(dim_x * dim_y * 4);
		}
	}
	else if (bitness == 16)
	{
		img->data_type = ASTCENC_TYPE_F16;
		for (unsigned int z = 0; z < dim_z; z++)
		{
			data[z] = (uint16_t*)tf_malloc(dim_x * dim_y * 4);
		}
	}
	else // if (bitness == 32)
	{
		assert(bitness == 32);
		img->data_type = ASTCENC_TYPE_F32;
		for (unsigned int z = 0; z < dim_z; z++)
		{
			data[z] = (float*)tf_malloc(sizeof(float) * dim_x * dim_y * 4);
		}
	}

	return img;
}

/* See header for documentation. */
void free_image(astcenc_image * img)
{
	if (img == nullptr)
	{
		return;
	}

	for (unsigned int z = 0; z < img->dim_z; z++)
	{
		tf_free(reinterpret_cast<char*>(img->data[z]));
	}

	tf_free(img->data);
	tf_free(img);
}

void free_cimage(const astc_compressed_image& img)
{
	if (img.data)
	{
		tf_free(img.data);
	}
}

/* See header for documentation. */
int determine_image_components(const astcenc_image * img)
{
	unsigned int dim_x = img->dim_x;
	unsigned int dim_y = img->dim_y;
	unsigned int dim_z = img->dim_z;

	// Scan through the image data to determine how many color components the image has
	bool is_luma = true;
	bool has_alpha = false;

	if (img->data_type == ASTCENC_TYPE_U8)
	{
		for (unsigned int z = 0; z < dim_z; z++)
		{
			uint8_t* data8 = static_cast<uint8_t*>(img->data[z]);

			for (unsigned int y = 0; y < dim_y; y++)
			{
				for (unsigned int x = 0; x < dim_x; x++)
				{
					int r = data8[(4 * dim_x * y) + (4 * x    )];
					int g = data8[(4 * dim_x * y) + (4 * x + 1)];
					int b = data8[(4 * dim_x * y) + (4 * x + 2)];
					int a = data8[(4 * dim_x * y) + (4 * x + 3)];

					is_luma = is_luma && (r == g) && (r == b);
					has_alpha = has_alpha || (a != 0xFF);
				}
			}
		}
	}
	else if (img->data_type == ASTCENC_TYPE_F16)
	{
		for (unsigned int z = 0; z < dim_z; z++)
		{
			uint16_t* data16 = static_cast<uint16_t*>(img->data[z]);

			for (unsigned int y = 0; y < dim_y; y++)
			{
				for (unsigned int x = 0; x < dim_x; x++)
				{
					int r = data16[(4 * dim_x * y) + (4 * x    )];
					int g = data16[(4 * dim_x * y) + (4 * x + 1)];
					int b = data16[(4 * dim_x * y) + (4 * x + 2)];
					int a = data16[(4 * dim_x * y) + (4 * x + 3)];

					is_luma = is_luma && (r == g) && (r == b);
					has_alpha = has_alpha || ((a ^ 0xC3FF) != 0xFFFF);
					// a ^ 0xC3FF returns FFFF if and only if the input is 1.0
				}
			}
		}
	}
	else // if (img->data_type == ASTCENC_TYPE_F32)
	{
		assert(img->data_type == ASTCENC_TYPE_F32);

		for (unsigned int z = 0; z < dim_z; z++)
		{
			float* data32 = static_cast<float*>(img->data[z]);

			for (unsigned int y = 0; y < dim_y; y++)
			{
				for (unsigned int x = 0; x < dim_x; x++)
				{
					float r = data32[(4 * dim_x * y) + (4 * x    )];
					float g = data32[(4 * dim_x * y) + (4 * x + 1)];
					float b = data32[(4 * dim_x * y) + (4 * x + 2)];
					float a = data32[(4 * dim_x * y) + (4 * x + 3)];

					is_luma = is_luma && (r == g) && (r == b);
					has_alpha = has_alpha || (a != 1.0f);
				}
			}
		}
	}

	int image_components = 1 + (is_luma == 0 ? 2 : 0) + (has_alpha ? 1 : 0);
	return image_components;
}

/* See header for documentation. */
astcenc_image* astc_img_from_floatx4_array(
	const float* data,
	unsigned int dim_x,
	unsigned int dim_y,
	bool y_flip
) {
	astcenc_image* img = alloc_image(16, dim_x, dim_y, 1);

	for (unsigned int y = 0; y < dim_y; y++)
	{
		uint16_t* data16 = static_cast<uint16_t*>(img->data[0]);
		unsigned int y_src = y_flip ? (dim_y - y - 1) : y;
		const float* src = data + 4 * dim_x * y_src;

		for (unsigned int x = 0; x < dim_x; x++)
		{
			vint4 colorf16 = float_to_float16(vfloat4(
				src[4 * x    ],
				src[4 * x + 1],
				src[4 * x + 2],
				src[4 * x + 3]
			));

			data16[(4 * dim_x * y) + (4 * x    )] = static_cast<uint16_t>(colorf16.lane<0>());
			data16[(4 * dim_x * y) + (4 * x + 1)] = static_cast<uint16_t>(colorf16.lane<1>());
			data16[(4 * dim_x * y) + (4 * x + 2)] = static_cast<uint16_t>(colorf16.lane<2>());
			data16[(4 * dim_x * y) + (4 * x + 3)] = static_cast<uint16_t>(colorf16.lane<3>());
		}
	}

	return img;
}

/* See header for documentation. */
astcenc_image* astc_img_from_unorm8x4_array(
	const uint8_t* data,
	unsigned int dim_x,
	unsigned int dim_y,
	bool y_flip
) {
	astcenc_image* img = alloc_image(8, dim_x, dim_y, 1);

	for (unsigned int y = 0; y < dim_y; y++)
	{
		uint8_t* data8 = static_cast<uint8_t*>(img->data[0]);
		unsigned int y_src = y_flip ? (dim_y - y - 1) : y;
		const uint8_t* src = data + 4 * dim_x * y_src;

		for (unsigned int x = 0; x < dim_x; x++)
		{
			data8[(4 * dim_x * y) + (4 * x    )] = src[4 * x    ];
			data8[(4 * dim_x * y) + (4 * x + 1)] = src[4 * x + 1];
			data8[(4 * dim_x * y) + (4 * x + 2)] = src[4 * x + 2];
			data8[(4 * dim_x * y) + (4 * x + 3)] = src[4 * x + 3];
		}
	}

	return img;
}

// initialize a flattened array of float values from an ASTC codec image
// The returned array is allocated with new[] and must be deleted with delete[].
/* See header for documentation. */
float* floatx4_array_from_astc_img(
	const astcenc_image* img,
	bool y_flip
) {
	unsigned int dim_x = img->dim_x;
	unsigned int dim_y = img->dim_y;
	float *buf = (float*)tf_malloc(sizeof(float)  * 4 * dim_x * dim_y);

	if (img->data_type == ASTCENC_TYPE_U8)
	{
		uint8_t* data8 = static_cast<uint8_t*>(img->data[0]);
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			float* dst = buf + y * dim_x * 4;

			for (unsigned int x = 0; x < dim_x; x++)
			{
				dst[4 * x    ] = data8[(4 * dim_x * ymod) + (4 * x    )] * (1.0f / 255.0f);
				dst[4 * x + 1] = data8[(4 * dim_x * ymod) + (4 * x + 1)] * (1.0f / 255.0f);
				dst[4 * x + 2] = data8[(4 * dim_x * ymod) + (4 * x + 2)] * (1.0f / 255.0f);
				dst[4 * x + 3] = data8[(4 * dim_x * ymod) + (4 * x + 3)] * (1.0f / 255.0f);
			}
		}
	}
	else if (img->data_type == ASTCENC_TYPE_F16)
	{
		uint16_t* data16 = static_cast<uint16_t*>(img->data[0]);
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			float *dst = buf + y * dim_x * 4;

			for (unsigned int x = 0; x < dim_x; x++)
			{
				vint4 colori(
					data16[(4 * dim_x * ymod) + (4 * x    )],
					data16[(4 * dim_x * ymod) + (4 * x + 1)],
					data16[(4 * dim_x * ymod) + (4 * x + 2)],
					data16[(4 * dim_x * ymod) + (4 * x + 3)]
				);

				vfloat4 color = float16_to_float(colori);
				store(color, dst + 4 * x);
			}
		}
	}
	else // if (img->data_type == ASTCENC_TYPE_F32)
	{
		assert(img->data_type == ASTCENC_TYPE_F32);
		float* data32 = static_cast<float*>(img->data[0]);
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			float *dst = buf + y * dim_x * 4;

			for (unsigned int x = 0; x < dim_x; x++)
			{
				dst[4 * x    ] = data32[(4 * dim_x * ymod) + (4 * x    )];
				dst[4 * x + 1] = data32[(4 * dim_x * ymod) + (4 * x + 1)];
				dst[4 * x + 2] = data32[(4 * dim_x * ymod) + (4 * x + 2)];
				dst[4 * x + 3] = data32[(4 * dim_x * ymod) + (4 * x + 3)];
			}
		}
	}

	return buf;
}

/* See header for documentation. */
uint8_t* unorm8x4_array_from_astc_img(
	const astcenc_image* img,
	bool y_flip
) {
	unsigned int dim_x = img->dim_x;
	unsigned int dim_y = img->dim_y;
	uint8_t* buf = (uint8_t*)tf_malloc(4 * dim_x * dim_y);

	if (img->data_type == ASTCENC_TYPE_U8)
	{
		uint8_t* data8 = static_cast<uint8_t*>(img->data[0]);
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			uint8_t* dst = buf + y * dim_x * 4;

			for (unsigned int x = 0; x < dim_x; x++)
			{
				dst[4 * x    ] = data8[(4 * dim_x * ymod) + (4 * x    )];
				dst[4 * x + 1] = data8[(4 * dim_x * ymod) + (4 * x + 1)];
				dst[4 * x + 2] = data8[(4 * dim_x * ymod) + (4 * x + 2)];
				dst[4 * x + 3] = data8[(4 * dim_x * ymod) + (4 * x + 3)];
			}
		}
	}
	else if (img->data_type == ASTCENC_TYPE_F16)
	{
		uint16_t* data16 = static_cast<uint16_t*>(img->data[0]);
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			uint8_t* dst = buf + y * dim_x * 4;

			for (unsigned int x = 0; x < dim_x; x++)
			{
				vint4 colori(
					data16[(4 * dim_x * ymod) + (4 * x    )],
					data16[(4 * dim_x * ymod) + (4 * x + 1)],
					data16[(4 * dim_x * ymod) + (4 * x + 2)],
					data16[(4 * dim_x * ymod) + (4 * x + 3)]
				);

				vfloat4 color = float16_to_float(colori);
				color = clamp(0.0f, 1.0f, color) * 255.0f;

				colori = float_to_int_rtn(color);
				pack_low_bytes(colori);
				store_nbytes(colori, dst + 4 * x);
			}
		}
	}
	else // if (img->data_type == ASTCENC_TYPE_F32)
	{
		assert(img->data_type == ASTCENC_TYPE_F32);
		float* data32 = static_cast<float*>(img->data[0]);
		for (unsigned int y = 0; y < dim_y; y++)
		{
			unsigned int ymod = y_flip ? dim_y - y - 1 : y;
			uint8_t* dst = buf + y * dim_x * 4;

			for (unsigned int x = 0; x < dim_x; x++)
			{
				dst[4 * x    ] = static_cast<uint8_t>(astc::flt2int_rtn(astc::clamp1f(data32[(4 * dim_x * ymod) + (4 * x    )]) * 255.0f));
				dst[4 * x + 1] = static_cast<uint8_t>(astc::flt2int_rtn(astc::clamp1f(data32[(4 * dim_x * ymod) + (4 * x + 1)]) * 255.0f));
				dst[4 * x + 2] = static_cast<uint8_t>(astc::flt2int_rtn(astc::clamp1f(data32[(4 * dim_x * ymod) + (4 * x + 2)]) * 255.0f));
				dst[4 * x + 3] = static_cast<uint8_t>(astc::flt2int_rtn(astc::clamp1f(data32[(4 * dim_x * ymod) + (4 * x + 3)]) * 255.0f));
			}
		}
	}

	return buf;
}
