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
 * @brief Functions for printing build info and help messages.
 */

#include "astcenccli_internal.h"
#include "astcenccli_version.h"

/** @brief The version header. */
static const char *astcenc_copyright_string =
R"(astcenc v%s, %u-bit %s%s%s
Copyright (c) 2011-%s Arm Limited. All rights reserved.
)";

/** @brief The short-form help text. */
static const char *astcenc_short_help =
R"(
Basic usage:

To compress an image use:
    astcenc {-cl|-cs|-ch|-cH} <in> <out> <blockdim> <quality> [options]

e.g. using LDR profile, 8x6 blocks, and the thorough quality preset:
    astcenc -cl kodim01.png kodim01.astc 8x6 -thorough

To decompress an image use:
    astcenc {-dl|-ds|-dh|-dH} <in> <out>

e.g. using LDR profile:
    astcenc -dl kodim01.astc kodim01.png

To perform a compression test, writing back the decompressed output, use:
    astcenc {-tl|-ts|-th|-tH} <in> <out> <blockdim> <quality> [options]

e.g. using LDR profile, 8x6 blocks, and the thorough quality preset:
    astcenc -tl kodim01.png kodim01-test.png 8x6 -thorough

The -*l options are used to configure the codec to support only the linear
LDR profile, preventing use of the HDR encoding features.

The -*s options are used to configure the codec to support only
the sRGB LDR profile, preventing use of the HDR encoding features. Input
texture data must be encoded in the sRGB colorspace for this option to
provide correct output results.

The -*h/-*H options are used to configure the codec to support the HDR ASTC
color profile. Textures compressed with this profile may fail to decompress
correctly on GPU hardware without HDR profile support. The -*h options
configure the compressor for HDR RGB components and an LDR alpha component.
The -*H options configure the compressor for HDR across all 4 components.

For full help documentation run 'astcenc -help'.
)";

/** @brief The long-form help text. */
static const char *astcenc_long_help = R"(
NAME
       astcenc - compress or decompress images using the ASTC format

SYNOPSIS
       astcenc {-h|-help}
       astcenc {-v|-version}
       astcenc {-cl|-cs|-ch|-cH} <in> <out> <blocksize> <quality> [options]
       astcenc {-dl|-ds|-dh|-dH} <in> <out> <blocksize> <quality> [options]
       astcenc {-tl|-ts|-th|-tH} <in> <out> <blocksize> <quality> [options]

DESCRIPTION
       astcenc compresses image files into the Adaptive Scalable Texture
       Compression (ASTC) image format, a lossy compression format design
       for use in real-time graphics applications. It is a fully featured
       compressor implementation, supporting all of the compression
       profiles and block sizes specified by the ASTC format:

           All color profiles (LDR linear, LDR sRGB, and HDR)
           All 2D block sizes (4x4 though to 12x12)
           All 3D block sizes (3x3x3 through to 6x6x6)

       The compressor provides a flexible quality level, allowing users to
       trade off compressed image quality against compression performance.
       For ease of use, a number of quality presets are also provided. For
       advanced users the compressor provides many additional control
       options for fine tuning quality.

       astcenc can also be used to decompress ASTC compressed images, and
       perform compression image quality analysis.

COMPRESSION
       To compress an image using the ASTC format you must specify the
       color profile, the input file name, the output file name, the target
       block size, and the quality preset.

       The color profile is specified using the -cl (LDR linear), -cs (LDR
       sRGB), -ch (HDR RGB, LDR A), or -cH (HDR RGBA) encoder options. Note
       that not all GPUs implementing ASTC support the HDR profile.

       The input file path must match a valid file format for compression,
       and the output file format must be a valid output for compression.
       See the FILE FORMATS section for the list of supported formats.

       The block size must be a valid ASTC block size. Every block
       compresses into 128 bits of compressed output, so the block size
       determines the compressed data bitrate.

       Supported 2D block sizes are:

             4x4: 8.00 bpp        10x5: 2.56 bpp
             5x4: 6.40 bpp        10x6: 2.13 bpp
             5x5: 5.12 bpp         8x8: 2.00 bpp
             6x5: 4.27 bpp        10x8: 1.60 bpp
             6x6: 3.56 bpp       10x10: 1.28 bpp
             8x5: 3.20 bpp       12x10: 1.07 bpp
             8x6: 2.67 bpp       12x12: 0.89 bpp

       Supported 3D block sizes are:

           3x3x3: 4.74 bpp       5x5x4: 1.28 bpp
           4x3x3: 3.56 bpp       5x5x5: 1.02 bpp
           4x4x3: 2.67 bpp       6x5x5: 0.85 bpp
           4x4x4: 2.00 bpp       6x6x5: 0.71 bpp
           5x4x4: 1.60 bpp       6x6x6: 0.59 bpp

       The quality level configures the quality-performance tradeoff for
       the compressor; more complete searches of the search space improve
       image quality at the expense of compression time. The quality level
       can be set to any value between 0 (fastest) and 100 (thorough), or
       to a fixed quality preset:

           -fastest       (equivalent to quality =   0)
           -fast          (equivalent to quality =  10)
           -medium        (equivalent to quality =  60)
           -thorough      (equivalent to quality =  98)
           -exhaustive    (equivalent to quality = 100)

       For compression of production content we recommend using a quality
       level equivalent to -medium or higher.

       Using quality levels higher than -thorough will significantly
       increase compression time, but typically only gives minor quality
       improvements.

       There are a number of additional compressor options which are useful
       to consider for common usage, based on the type of image data being
       compressed.

       -mask
           The input texture is a mask texture with unrelated data stored
           in the various color components, so enable error heuristics that
           aim to improve quality by minimizing the effect of error
           cross-talk across the color components.

       -normal
           The input texture is a three component linear LDR normal map
           storing unit length normals as (R=X, G=Y, B=Z). The output will
           be a two component X+Y normal map stored as (RGB=X, A=Y). The Z
           component can be recovered programmatically in shader code by
           using the equation:

               nml.xy = texture(...).ga;              // Load in [0,1]
               nml.xy = nml.xy * 2.0 - 1.0;           // Unpack to [-1,1]
               nml.z = sqrt(1 - dot(nml.xy, nml.xy)); // Compute Z

           Alternative component swizzles can be set with -esw and -dsw
           parameters.

       -rgbm <max>
           The input texture is an RGBM encoded texture, storing values HDR
           values between 0 and <max> in an LDR container format with a
           shared multiplier. Shaders reconstruct the HDR value as:

               vec3 hdr_value = tex.rgb * tex.a * max;

           The compression behavior of the ASTC format for RGBM data
           requires that the user's RGBM encoding preprocess keeps values
           of M above a lower threshold to avoid them quantizing to zero
           during compression. We recommend trying 16/255 or 32/255.

       -perceptual
           The codec should optimize perceptual error, instead of direct
           RMS error. This aims to improves perceived image quality, but
           typically lowers the measured PSNR score. Perceptual methods are
           currently only available for normal maps and RGB color data.

       -zdim <zdim>
           Load a sequence of <zdim> 2D image slices to use as a 3D image.
           The input filename given is used is decorated with the postfix
           "_<slice>" to find the file to load. For example, an input named
           "input.png" would load as input_0.png, input_1.png, etc.

       -pp-normalize
            Run a preprocess over the image that forces normal vectors to
            be unit length. Preprocessing applies before any codec encoding
            swizzle, so normal data must be in the RGB components in the
            source image.

       -pp-premultiply
            Run a preprocess over the image that scales RGB components in
            the image by the alpha value. Preprocessing applies before any
            codec encoding swizzle, so color data must be in the RGB
            components in the source image.)"
// This split in the literals is needed for Visual Studio; the compiler
// will concatenate these two strings together ...
R"(

COMPRESSION TIPS & TRICKS
       ASTC is a block-based format that can be prone to block artifacts.
       If block artifacts are a problem when compressing a given texture,
       increasing the compressor quality preset can help to alleviate the
       problem.

       If a texture exhibits severe block artifacts in only some of the
       color components, which is a common problem for mask textures, then
       using the -cw option to raise the weighting of the affected color
       component(s) may help. For example, if the green color component is
       particularly badly encoded then try '-cw 1 6 1 1'.

ADVANCED COMPRESSION
       Error weighting options
       -----------------------

       These options provide low-level control of the codec error metric
       computation, used to determine what good compression looks like.

       -a <radius>
           For textures with alpha component, scale per-texel weights by
           the alpha value. The alpha value chosen for scaling of any
           particular texel is taken as an average across a neighborhood of
           the texel defined by the <radius> argument. Setting <radius> to
           0 causes only the texel's own alpha to be used.

           ASTC blocks that are entirely zero weighted, after the radius is
           taken into account, are replaced by constant color blocks. This
           is an RDO-like technique to improve compression ratio in any
           application packaging compression that is applied.

       -cw <red> <green> <blue> <alpha>
           Assign an additional weight scaling to each color component,
           allowing the components to be treated differently in terms of
           error significance. Set values above 1 to increase a component's
           significance, and values below 1 to decrease it. Set to 0 to
           exclude a component from error computation.

       -mpsnr <low> <high>
           Set the low and high f-stop values for the mPSNR error metric.
           The mPSNR error metric only applies to HDR textures.

       Performance-quality tradeoff options
       ------------------------------------

       These options provide low-level control of the codec heuristics that
       drive the performance-quality trade off. The presets vary by block
       bitrate; the recommended starting point for a 4x4 block is very
       different to a 8x8 block. The presets documented here are for the
	   high bitrate mode (fewer than 25 texels).

       -partitioncountlimit <number>
           Test up to and including <number> partitions for each block.
           Higher numbers give better quality, as more complex blocks can
           be encoded, but will increase search time. Preset defaults are:

               -fastest    : 2
               -fast       : 3
               -medium     : 4
               -thorough   : 4
               -exhaustive : 4

       -partitionindexlimit <number>
           Test <number> block partition indices for each partition count.
           Higher numbers give better quality, however large values give
           diminishing returns especially for smaller block sizes. Preset
           defaults are:

               -fastest    :    8
               -fast       :   12
               -medium     :   26
               -thorough   :   76
               -exhaustive : 1024

       -blockmodelimit <number>
           Test block modes below <number> usage centile in an empirically
           determined distribution of block mode frequency. This option is
           ineffective for 3D textures. Preset defaults are:

               -fastest    :  40
               -fast       :  55
               -medium     :  76
               -thorough   :  93
               -exhaustive : 100

       -refinementlimit <value>
           Iterate only <value> refinement iterations on colors and
           weights. Minimum value is 1. Preset defaults are:

               -fastest    : 2
               -fast       : 3
               -medium     : 3
               -thorough   : 4
               -exhaustive : 4

       -candidatelimit <value>
           Trial only <value> candidate encodings for each block mode:

               -fastest    : 2
               -fast       : 3
               -medium     : 3
               -thorough   : 4
               -exhaustive : 4

       -dblimit <number>
           Stop compression work on a block as soon as the PSNR of the
           block, measured in dB, exceeds <number>. This option is
           ineffective for HDR textures. Preset defaults, where N is the
           number of texels in a block, are:

               -fastest    : MAX(63-19*log10(N),  85-35*log10(N))
               -fast       : MAX(63-19*log10(N),  85-35*log10(N))
               -medium     : MAX(70-19*log10(N),  95-35*log10(N))
               -thorough   : MAX(77-19*log10(N), 105-35*log10(N))
               -exhaustive : 999

       -2partitionlimitfactor <factor>
           Stop compression work on a block after only testing blocks with
           up to two partitions and one plane of weights, unless the two
           partition error term is lower than the error term from encoding
           with one partition by more than the specified factor. Preset
           defaults are:

               -fastest    :  1.0
               -fast       :  1.0
               -medium     :  1.2
               -thorough   :  2.5
               -exhaustive : 10.0

       -3partitionlimitfactor <factor>
           Stop compression work on a block after only testing blocks with
           up to three partitions and one plane of weights, unless the three
           partition error term is lower than the error term from encoding
           with two partitions by more than the specified factor. Preset
           defaults are:

               -fastest    :  1.00
               -fast       :  1.10
               -medium     :  1.25
               -thorough   :  1.25
               -exhaustive : 10.00

       -2planelimitcorrelation <factor>
           Stop compression after testing only one plane of weights, unless
           the minimum color correlation factor between any pair of color
           components is below this factor. This option is ineffective for
           normal maps. Preset defaults are:

               -fastest    : 0.50
               -fast       : 0.65
               -medium     : 0.85
               -thorough   : 0.95
               -exhaustive : 0.99

       -lowweightmodelimit <weight count>
           Use a simpler weight search for weight counts less than or
           equal to this threshold. Preset defaults are bitrate dependent:

               -fastest    : 25
               -fast       : 20
               -medium     : 16
               -thorough   : 12
               -exhaustive : 0

       Other options
       -------------

       -esw <swizzle>
           Specify an encoding swizzle to reorder the color components
           before compression. The swizzle is specified using a four
           character string, which defines the format ordering used by
           the compressor.

           The characters may be taken from the set [rgba01], selecting
           either input color components or a literal zero or one. For
           example to swap the RG components, and replace alpha with 1,
           the swizzle 'grb1' should be used.

           By default all 4 post-swizzle components are included in the
           compression error metrics. When using -esw to map two
           component data to the L+A endpoint (e.g. -esw rrrg) the
           luminance data stored in the RGB components will be weighted 3
           times more strongly than the alpha component. This can be
           corrected using the -ssw option to specify which components
           will be sampled at runtime e.g. -ssw ra.

       -ssw <swizzle>
           Specify a sampling swizzle to identify which color components
           are actually read by the application shader program. For example,
           using -ssw ra tells the compressor that the green and blue error
           does not matter because the data is not actually read.

           The sampling swizzle is based on the channel ordering after the
           -esw transform has been applied. Note -ssw exposes the same
           functionality as -cw, but in a more user-friendly form.

       -dsw <swizzle>
           Specify a decompression swizzle used to reorder the color
           components after decompression. The swizzle is specified using
           the same method as the -esw option, with support for an extra
           "z" character. This is used to specify that the compressed data
           stores an X+Y normal map, and that the Z output component
           should be reconstructed from the two components stored in the
           data. For the typical ASTC normal encoding, which uses an
           'rrrg' compression swizzle, you should specify an 'raz1'
           swizzle for decompression.

       -yflip
           Flip the image in the vertical axis prior to compression and
           after decompression. Note that using this option in a test mode
           (-t*) will have no effect as the image will be flipped twice.

       -j <threads>
           Explicitly specify the number of threads to use in the codec. If
           not specified, the codec will use one thread per CPU detected in
           the system.

       -silent
           Suppresses all non-essential diagnostic output from the codec.
           Error messages will always be printed, as will mandatory outputs
           for the selected operation mode. For example, the test mode will
           always output image quality metrics and compression time but
           will suppress all other output.)"
// This split in the literals is needed for Visual Studio; the compiler
// will concatenate these two strings together ...
R"(

DECOMPRESSION
       To decompress an image stored in the ASTC format you must specify
       the color profile, the input file name, and the output file name.

       The color profile is specified using the -dl (LDR linear), -ds (LDR
       sRGB), -dh (HDR RGB, LDR A), or -dH (HDR RGBA) decoder options.

       The input file path must match a valid file format for
       decompression, and the output file format must be a valid output for
       a decompressed image. Note that not all output formats that the
       compression path can produce are supported for decompression. See
       the FILE FORMATS section for the list of supported formats.

       The -dsw option documented in ADVANCED COMPRESSION option
       documentation is also relevant to decompression.

TEST
       To perform a compression test which round-trips a single image
       through compression and decompression and stores the decompressed
       result back to file, you must specify same settings as COMPRESSION
       other than swapping the color profile to select test mode. Note that
       the compressed intermediate data is discarded in this mode.

       The color profile is specified using the -tl (LDR linear), -ts (LDR
       sRGB), -th (HDR RGB, LDR A), or -tH (HDR RGBA) encoder options.

       This operation mode will print error metrics suitable for either LDR
       and HDR images, allowing some assessment of the compression image
       quality.

COMPRESSION FILE FORMATS
       The following formats are supported as compression inputs:

           LDR Formats:
               BMP (*.bmp)
               PNG (*.png)
               Targa (*.tga)
               JPEG (*.jpg)

           HDR Formats:
               OpenEXR (*.exr)
               Radiance HDR (*.hdr)

           Container Formats:
               Khronos Texture KTX (*.ktx)
               DirectDraw Surface DDS (*.dds)

       For the KTX and DDS formats only a subset of the features of the
       formats are supported:

           Texture topology must be 2D, 2D-array, 3D, or cube-map. Note
           that 2D-array textures are treated as 3D block input.

           Texel format must be R, RG, RGB, BGR, RGBA, BGRA, L, or LA.

           Only the first mipmap in the file will be read.

       The following formats are supported as compression outputs:

           ASTC (*.astc)
           Khronos Texture KTX (*.ktx)


DECOMPRESSION FILE FORMATS
       The following formats are supported as decompression inputs:

           ASTC (*.astc)
           Khronos Texture KTX (*.ktx)

       The following formats are supported as decompression outputs:

           LDR Formats:
               BMP (*.bmp)
               PNG (*.png)
               Targa (*.tga)

           HDR Formats:
               OpenEXR (*.exr)
               Radiance HDR (*.hdr)

           Container Formats:
               Khronos Texture KTX (*.ktx)
               DirectDraw Surface DDS (*.dds)

QUICK REFERENCE

       To compress an image use:
           astcenc {-cl|-cs|-ch|-cH} <in> <out> <blockdim> <quality> [options]

       To decompress an image use:
           astcenc {-dl|-ds|-dh|-dH} <in> <out>

       To perform a quality test use:
           astcenc {-tl|-ts|-th|-tH} <in> <out> <blockdim> <quality> [options]

       Mode -*l = linear LDR, -*s = sRGB LDR, -*h = HDR RGB/LDR A, -*H = HDR.
       Quality = -fastest/-fast/-medium/-thorough/-exhaustive/a float [0-100].
)";

/* See header for documentation. */
void astcenc_print_header()
{
#if (ASTCENC_AVX == 2)
	const char* simdtype = "avx2";
#elif (ASTCENC_SSE == 41)
	const char* simdtype = "sse4.1";
#elif (ASTCENC_SSE == 20)
	const char* simdtype = "sse2";
#elif (ASTCENC_NEON == 1)
	const char* simdtype = "neon";
#else
	const char* simdtype = "none";
#endif

#if (ASTCENC_POPCNT == 1)
	const char* pcnttype = "+popcnt";
#else
	const char* pcnttype = "";
#endif

#if (ASTCENC_F16C == 1)
	const char* f16ctype = "+f16c";
#else
	const char* f16ctype = "";
#endif

	unsigned int bits = static_cast<unsigned int>(sizeof(void*) * 8);
	printf(astcenc_copyright_string,
	       VERSION_STRING, bits, simdtype, pcnttype, f16ctype, YEAR_STRING);
}

/* See header for documentation. */
void astcenc_print_shorthelp()
{
	astcenc_print_header();
	printf("%s", astcenc_short_help);
}

/* See header for documentation. */
void astcenc_print_longhelp()
{
	astcenc_print_header();
	printf("%s", astcenc_long_help);
}
