# About

This is the official repository for the Arm® Adaptive Scalable Texture
Compression (ASTC) Encoder, `astcenc`, a command-line tool for compressing
and decompressing images using the ASTC texture compression standard.

## The ASTC format

The ASTC compressed data format, developed by Arm® and AMD, has been adopted as
an official extension to the Open GL®, OpenGL ES, and Vulkan® graphics APIs. It
provides a major step forward in terms of both the image quality at a given
bitrate, and the format and bitrate flexibility available to content creators.
This allows more assets to use compression, often at a reduced bitrate compared
to other formats, reducing memory storage and bandwidth requirements.

Read the [ASTC Format Overview][1] for a quick introduction to the format, or
read the full [Khronos Data Format Specification][2] for all the details.

## License

This project is licensed under the Apache 2.0 license. By downloading any
component from this repository you acknowledge that you accept terms specified
in the [LICENSE.txt](LICENSE.txt) file.

# Encoder feature support

The encoder supports compression of low dynamic range (BMP, JPEG, PNG, TGA) and
high dynamic range (EXR, HDR) images, as well as a subset of image data wrapped
in the DDS and KTX container formats, into ASTC or KTX format output images.

The decoder supports decompression of ASTC or KTX format input images into low
dynamic range (BMP, PNG, TGA), high dynamic range (EXR, HDR), or DDS and KTX
wrapped output images.

The encoder allows control over the compression time/quality tradeoff with
`exhaustive`, `thorough`, `medium`, `fast`, and `fastest` encoding quality
presets.

The encoder allows compression time and quality analysis by reporting the
compression time, and the Peak Signal-to-Noise Ratio (PSNR) between the input
image and the compressed output.

## ASTC format support

The `astcenc` compressor supports generation of images for all three profiles
allowed by the ASTC specification:

* 2D Low Dynamic Range (LDR profile)
* 2D LDR and High Dynamic Range (HDR profile)
* 2D and 3D, LDR and HDR (Full profile)

It also supports all of the ASTC block sizes and compression modes, allowing
content creators to use the full spectrum of quality-to-bitrate options ranging
from 0.89 bits/pixel up to 8 bits/pixel.

# Prebuilt binaries

Release build binaries for the `astcenc` stable releases are provided in the
[GitHub Releases page][3].

**Latest 4.x stable release:** 4.1
* Change log: [4.x series](./Docs/ChangeLog-4x.md)

**Latest 3.x stable release:** 3.7
* Change log: [3.x series](./Docs/ChangeLog-3x.md)

Binaries are provided for 64-bit builds on Windows, macOS, and Linux. The
builds of the astcenc are provided as multiple binaries, each tuned for a
specific SIMD instruction set.

For x86-64 we provide, in order of increasing performance:

* `astcenc-sse2` - uses SSE2
* `astcenc-sse4.1` - uses SSE4.1 and POPCNT
* `astcenc-avx2` - uses AVX2, SSE4.2, POPCNT, and F16C

The x86-64 SSE2 builds will work on all x86-64 machines, but it is the slowest
of the three. The other two require extended CPU instruction set support which
is not universally available, but each step gains ~15% more performance.

For Apple silicon macOS devices we provide:

* `astcenc-neon` - uses NEON


## Repository branches

The `main` branch is an active development branch for the compressor. It aims
to be a stable branch for the latest major release series, but as it is used
for ongoing development expect it to have some volatility. We recommend using
the latest stable release tag for production development.

The `2.x` and `3.x` branches are a stable branches for the 2.x and 3.x release
series. They are no longer under active development, but are supported branches
that will continue to get backported bug fixes.

The `1.x` branch is a stable branch for the 1.x release series. It is no longer
under active development or getting bug fixes.

Any other branches you might find are development branches for new features or
optimizations, so might be interesting to play with but should be considered
transient and unstable.


# Getting started

Open a terminal, change to the appropriate directory for your system, and run
the astcenc encoder program, like this on Linux or macOS:

    ./astcenc

... or like this on Windows:

    astcenc

Invoking `astcenc -help` gives an extensive help message, including usage
instructions and details of all available command line options. A summary of
the main encoder options are shown below.

## Compressing an image

Compress an image using the `-cl` \ `-cs` \ `-ch` \ `-cH` modes. For example:

    astcenc -cl example.png example.astc 6x6 -medium

This compresses `example.png` using the LDR color profile and a 6x6 block
footprint (3.56 bits/pixel). The `-medium` quality preset gives a reasonable
image quality for a relatively fast compression speed, so is a good starting
point for compression. The output is stored to a linear color space compressed
image, `example.astc`.

The modes available are:

* `-cl` : use the linear LDR color profile.
* `-cs` : use the sRGB LDR color profile.
* `-ch` : use the HDR color profile, tuned for HDR RGB and LDR A.
* `-cH` : use the HDR color profile, tuned for HDR RGBA.

## Decompressing an image

Decompress an image using the `-dl` \ `-ds` \ `-dh` \ `-dH` modes. For example:

    astcenc -dh example.astc example.tga

This decompresses `example.astc` using the full HDR feature profile, storing
the decompressed output to `example.tga`.

The modes available mirror the options used for compression, but use a `d`
prefix. Note that for decompression there is no difference between the two HDR
modes, they are both provided simply to maintain symmetry across operations.

## Measuring image quality

Review the compression quality using the `-tl` \ `-ts` \ `-th` \ `-tH` modes.
For example:

    astcenc -tl example.png example.tga 5x5 -thorough

This is equivalent to using using the LDR color profile and a 5x5 block size
to compress the image, using the `-thorough` quality preset, and then
immediately decompressing the image and saving the result. This can be used
to enable a visual inspection of the compressed image quality. In addition
this mode also prints out some image quality metrics to the console.

The modes available mirror the options used for compression, but use a `t`
prefix.

## Experimenting

Efficient real-time graphics benefits from minimizing compressed texture size,
as it reduces memory footprint, reduces memory bandwidth, saves energy, and can
improve texture cache efficiency. However, like any lossy compression format
there will come a point where the compressed image quality is unacceptable
because there are simply not enough bits to represent the output with the
precision needed. We recommend experimenting with the block footprint to find
the optimum balance between size and quality, as the finely adjustable
compression ratio is one of major strengths of the ASTC format.

The compression speed can be controlled from `-fastest`, through `-fast`,
`-medium` and `-thorough`, up to `-exhaustive`. In general, the more time the
encoder has to spend looking for good encodings the better the results, but it
does result in increasingly small improvements for the amount of time required.

There are many other command line options for tuning the encoder parameters
which can be used to fine tune the compression algorithm. See the command line
help message for more details.

# Documentation

The [ASTC Format Overview](./Docs/FormatOverview.md) page provides a high level
introduction to the ASTC texture format, how it encodes data, and why it is
both flexible and efficient.

The [Effective ASTC Encoding](./Docs/Encoding.md) page looks at some of the
guidelines that should be followed when compressing data using `astcenc`.
It covers:

* How to efficiently encode data with fewer than 4 channels.
* How to efficiently encode normal maps, sRGB data, and HDR data.
* Coding equivalents to other compression formats.

The [.astc File Format](./Docs/FileFormat.md) page provides a light-weight
specification for the `.astc` file format and how to read or write it.

The [Building ASTC Encoder](./Docs/Building.md) page provides instructions on
how to build `astcenc` from the sources in this repository.

The [Testing ASTC Encoder](./Docs/Testing.md) page provides instructions on
how to test any modifications to the source code in this repository.

# Support

If you have issues with the `astcenc` encoder, or questions about the ASTC
texture format itself, please raise them in the GitHub issue tracker.

If you have any questions about Arm GPUs, application development for Arm GPUs,
or general mobile graphics development or technology please submit them on the
[Arm Community graphics forums][4].

- - -

_Copyright © 2013-2022, Arm Limited and contributors. All rights reserved._

[1]: ./Docs/FormatOverview.md
[2]: https://www.khronos.org/registry/DataFormat/specs/1.3/dataformat.1.3.html#ASTC
[3]: https://github.com/ARM-software/astc-encoder/releases
[4]: https://community.arm.com/support-forums/f/graphics-gaming-and-vr-forum/
