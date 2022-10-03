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
 * @brief Functions for computing image error metrics.
 */

#include <cassert>
#include <cstdio>

#include "astcenccli_internal.h"

/**
 * @brief An accumulator for errors.
 */
class error_accum4
{
public:
	/** @brief The running sum. */
	double sum_r { 0.0 };
	double sum_g { 0.0 };
	double sum_b { 0.0 };
	double sum_a { 0.0 };
};

/**
 * @brief Incremental addition operator for error accumulators.
 *
 * @param val The accumulator to increment
 * @param inc The increment to apply
 *
 * @return The updated accumulator
 */
static error_accum4& operator+=(
	error_accum4 &val,
	vfloat4 inc
) {
	val.sum_r += static_cast<double>(inc.lane<0>());
	val.sum_g += static_cast<double>(inc.lane<1>());
	val.sum_b += static_cast<double>(inc.lane<2>());
	val.sum_a += static_cast<double>(inc.lane<3>());
	return val;
}

/**
 * @brief mPSNR tone-mapping operator for HDR images.
 *
 * @param val     The color value to tone map
 * @param fstop   The exposure fstop; should be in range [-125, 125]
 *
 * @return The mapped color value in [0.0f, 255.0f] range
 */
static float mpsnr_operator(
	float val,
	int fstop
) {
	if32 p;
	p.u = 0x3f800000 + (fstop << 23);  // 0x3f800000 is 1.0f
	val *= p.f;
	val = powf(val, (1.0f / 2.2f));
	val *= 255.0f;

	return astc::clamp(val, 0.0f, 255.0f);
}

/**
 * @brief mPSNR difference between two values.
 *
 * Differences are given as "val1 - val2".
 *
 * @param val1       The first color value
 * @param val2       The second color value
 * @param fstop_lo   The low exposure fstop; should be in range [-125, 125]
 * @param fstop_hi   The high exposure fstop; should be in range [-125, 125]
 *
 * @return The summed mPSNR difference across all active fstop levels
 */
static float mpsnr_sumdiff(
	float val1,
	float val2,
	int fstop_lo,
	int fstop_hi
) {
	float summa = 0.0f;
	for (int i = fstop_lo; i <= fstop_hi; i++)
	{
		float mval1 = mpsnr_operator(val1, i);
		float mval2 = mpsnr_operator(val2, i);
		float mdiff = mval1 - mval2;
		summa += mdiff * mdiff;
	}
	return summa;
}

/* See header for documentation */
void compute_error_metrics(
	bool compute_hdr_metrics,
	bool compute_normal_metrics,
	int input_components,
	const astcenc_image* img1,
	const astcenc_image* img2,
	int fstop_lo,
	int fstop_hi
) {
	static const int componentmasks[5] { 0x00, 0x07, 0x0C, 0x07, 0x0F };
	int componentmask = componentmasks[input_components];

	error_accum4 errorsum;
	error_accum4 alpha_scaled_errorsum;
	error_accum4 log_errorsum;
	error_accum4 mpsnr_errorsum;
	double mean_angular_errorsum = 0.0;
	double worst_angular_errorsum = 0.0;

	unsigned int dim_x = astc::min(img1->dim_x, img2->dim_x);
	unsigned int dim_y = astc::min(img1->dim_y, img2->dim_y);
	unsigned int dim_z = astc::min(img1->dim_z, img2->dim_z);

	if (img1->dim_x != img2->dim_x ||
	    img1->dim_y != img2->dim_y ||
	    img1->dim_z != img2->dim_z)
	{
		printf("WARNING: Only intersection of images will be compared:\n"
		       "  Image 1: %dx%dx%d\n"
		       "  Image 2: %dx%dx%d\n",
		       img1->dim_x, img1->dim_y, img1->dim_z,
		       img2->dim_x, img2->dim_y, img2->dim_z);
	}

	double rgb_peak = 0.0;
	unsigned int xsize1 = img1->dim_x;
	unsigned int xsize2 = img2->dim_x;

	for (unsigned int z = 0; z < dim_z; z++)
	{
		for (unsigned int y = 0; y < dim_y; y++)
		{
			for (unsigned int x = 0; x < dim_x; x++)
			{
				vfloat4 color1;
				vfloat4 color2;

				if (img1->data_type == ASTCENC_TYPE_U8)
				{
					uint8_t* data8 = static_cast<uint8_t*>(img1->data[z]);

					color1 = vfloat4(
					    data8[(4 * xsize1 * y) + (4 * x    )],
					    data8[(4 * xsize1 * y) + (4 * x + 1)],
					    data8[(4 * xsize1 * y) + (4 * x + 2)],
					    data8[(4 * xsize1 * y) + (4 * x + 3)]);

					color1 = color1 / 255.0f;
				}
				else if (img1->data_type == ASTCENC_TYPE_F16)
				{
					uint16_t* data16 = static_cast<uint16_t*>(img1->data[z]);

					vint4 color1i = vint4(
					    data16[(4 * xsize1 * y) + (4 * x    )],
					    data16[(4 * xsize1 * y) + (4 * x + 1)],
					    data16[(4 * xsize1 * y) + (4 * x + 2)],
					    data16[(4 * xsize1 * y) + (4 * x + 3)]);

					color1 = float16_to_float(color1i);
					color1 = clamp(0, 65504.0f, color1);
				}
				else // if (img1->data_type == ASTCENC_TYPE_F32)
				{
					assert(img1->data_type == ASTCENC_TYPE_F32);
					float* data32 = static_cast<float*>(img1->data[z]);

					color1 = vfloat4(
					    data32[(4 * xsize1 * y) + (4 * x    )],
					    data32[(4 * xsize1 * y) + (4 * x + 1)],
					    data32[(4 * xsize1 * y) + (4 * x + 2)],
					    data32[(4 * xsize1 * y) + (4 * x + 3)]);

					color1 = clamp(0, 65504.0f, color1);
				}

				if (img2->data_type == ASTCENC_TYPE_U8)
				{
					uint8_t* data8 = static_cast<uint8_t*>(img2->data[z]);

					color2 = vfloat4(
					    data8[(4 * xsize2 * y) + (4 * x    )],
					    data8[(4 * xsize2 * y) + (4 * x + 1)],
					    data8[(4 * xsize2 * y) + (4 * x + 2)],
					    data8[(4 * xsize2 * y) + (4 * x + 3)]);

					color2 = color2 / 255.0f;
				}
				else if (img2->data_type == ASTCENC_TYPE_F16)
				{
					uint16_t* data16 = static_cast<uint16_t*>(img2->data[z]);

					vint4 color2i = vint4(
					    data16[(4 * xsize2 * y) + (4 * x    )],
					    data16[(4 * xsize2 * y) + (4 * x + 1)],
					    data16[(4 * xsize2 * y) + (4 * x + 2)],
					    data16[(4 * xsize2 * y) + (4 * x + 3)]);

					color2 = float16_to_float(color2i);
					color2 = clamp(0, 65504.0f, color2);
				}
				else // if (img2->data_type == ASTCENC_TYPE_F32)
				{
					assert(img2->data_type == ASTCENC_TYPE_F32);
					float* data32 = static_cast<float*>(img2->data[z]);

					color2 = vfloat4(
					    data32[(4 * xsize2 * y) + (4 * x    )],
					    data32[(4 * xsize2 * y) + (4 * x + 1)],
					    data32[(4 * xsize2 * y) + (4 * x + 2)],
					    data32[(4 * xsize2 * y) + (4 * x + 3)]);

					color2 = clamp(0, 65504.0f, color2);
				}

				rgb_peak = astc::max(static_cast<double>(color1.lane<0>()),
				                     static_cast<double>(color1.lane<1>()),
				                     static_cast<double>(color1.lane<2>()),
				                     rgb_peak);

				vfloat4 diffcolor = color1 - color2;
				vfloat4 diffcolor_sq = diffcolor * diffcolor;
				errorsum += diffcolor_sq;

				vfloat4 alpha_scaled_diffcolor = vfloat4(
				    diffcolor.lane<0>() * color1.lane<3>(),
				    diffcolor.lane<1>() * color1.lane<3>(),
				    diffcolor.lane<2>() * color1.lane<3>(),
				    diffcolor.lane<3>());

				vfloat4 alpha_scaled_diffcolor_sq = alpha_scaled_diffcolor * alpha_scaled_diffcolor;
				alpha_scaled_errorsum += alpha_scaled_diffcolor_sq;

				if (compute_hdr_metrics)
				{
					vfloat4 log_input_color1 = log2(color1);
					vfloat4 log_input_color2 = log2(color2);

					vfloat4 log_diffcolor = log_input_color1 - log_input_color2;

					log_errorsum += log_diffcolor * log_diffcolor;

					vfloat4 mpsnr_error = vfloat4(
					    mpsnr_sumdiff(color1.lane<0>(), color2.lane<0>(), fstop_lo, fstop_hi),
					    mpsnr_sumdiff(color1.lane<1>(), color2.lane<1>(), fstop_lo, fstop_hi),
					    mpsnr_sumdiff(color1.lane<2>(), color2.lane<2>(), fstop_lo, fstop_hi),
					    mpsnr_sumdiff(color1.lane<3>(), color2.lane<3>(), fstop_lo, fstop_hi));

					mpsnr_errorsum += mpsnr_error;
				}

				if (compute_normal_metrics)
				{
					// Decode the normal vector
					vfloat4 normal1 = (color1 - 0.5f) * 2.0f;
					normal1 = normalize_safe(normal1.swz<0, 1, 2>(), unit3());

					vfloat4 normal2 = (color2 - 0.5f) * 2.0f;
					normal2 = normalize_safe(normal2.swz<0, 1, 2>(), unit3());

					// Float error can push this outside of valid range for acos, so clamp to avoid NaN issues
					float normal_cos = clamp(-1.0f, 1.0f, dot3(normal1, normal2)).lane<0>();
					float rad_to_degrees = 180.0f / astc::PI_;
					double error_degrees = std::acos(static_cast<double>(normal_cos)) * static_cast<double>(rad_to_degrees);

					mean_angular_errorsum += error_degrees / (dim_x * dim_y * dim_z);
					worst_angular_errorsum = astc::max(worst_angular_errorsum, error_degrees);
				}
			}
		}
	}

	double pixels = static_cast<double>(dim_x * dim_y * dim_z);
	double samples = 0.0;

	double num = 0.0;
	double alpha_num = 0.0;
	double log_num = 0.0;
	double mpsnr_num = 0.0;

	if (componentmask & 1)
	{
		num += errorsum.sum_r;
		alpha_num += alpha_scaled_errorsum.sum_r;
		log_num += log_errorsum.sum_r;
		mpsnr_num += mpsnr_errorsum.sum_r;
		samples += pixels;
	}

	if (componentmask & 2)
	{
		num += errorsum.sum_g;
		alpha_num += alpha_scaled_errorsum.sum_g;
		log_num += log_errorsum.sum_g;
		mpsnr_num += mpsnr_errorsum.sum_g;
		samples += pixels;
	}

	if (componentmask & 4)
	{
		num += errorsum.sum_b;
		alpha_num += alpha_scaled_errorsum.sum_b;
		log_num += log_errorsum.sum_b;
		mpsnr_num += mpsnr_errorsum.sum_b;
		samples += pixels;
	}

	if (componentmask & 8)
	{
		num += errorsum.sum_a;
		alpha_num += alpha_scaled_errorsum.sum_a;
		samples += pixels;
	}

	double denom = samples;
	double stopcount = static_cast<double>(fstop_hi - fstop_lo + 1);
	double mpsnr_denom = pixels * 3.0 * stopcount * 255.0 * 255.0;

	double psnr;
	if (num == 0.0)
	{
		psnr = 999.0;
	}
	else
	{
		psnr = 10.0 * log10(denom / num);
	}

	double rgb_psnr = psnr;

	printf("Quality metrics\n");
	printf("===============\n\n");

	if (componentmask & 8)
	{
		printf("    PSNR (LDR-RGBA):          %9.4f dB\n", psnr);

		double alpha_psnr;
		if (alpha_num == 0.0)
		{
			alpha_psnr = 999.0;
		}
		else
		{
			alpha_psnr = 10.0 * log10(denom / alpha_num);
		}
		printf("    Alpha-weighted PSNR:      %9.4f dB\n", alpha_psnr);

		double rgb_num = errorsum.sum_r + errorsum.sum_g + errorsum.sum_b;
		if (rgb_num == 0.0)
		{
			rgb_psnr = 999.0;
		}
		else
		{
			rgb_psnr = 10.0 * log10(pixels * 3.0 / rgb_num);
		}
		printf("    PSNR (LDR-RGB):           %9.4f dB\n", rgb_psnr);
	}
	else
	{
		printf("    PSNR (LDR-RGB):           %9.4f dB\n", psnr);
	}

	if (compute_hdr_metrics)
	{
		printf("    PSNR (RGB norm to peak):  %9.4f dB (peak %f)\n",
		       rgb_psnr + 20.0 * log10(rgb_peak), rgb_peak);

		double mpsnr;
		if (mpsnr_num == 0.0)
		{
			mpsnr = 999.0;
		}
		else
		{
			mpsnr = 10.0 * log10(mpsnr_denom / mpsnr_num);
		}

		printf("    mPSNR (RGB):              %9.4f dB (fstops %+d to %+d)\n",
		       mpsnr, fstop_lo, fstop_hi);

		double logrmse = sqrt(log_num / pixels);
		printf("    LogRMSE (RGB):            %9.4f\n", logrmse);
	}

	if (compute_normal_metrics)
	{
		printf("    Mean Angular Error:       %9.4f degrees\n", mean_angular_errorsum);
		printf("    Worst Angular Error:      %9.4f degrees\n", worst_angular_errorsum);
	}

	printf("\n");
}
