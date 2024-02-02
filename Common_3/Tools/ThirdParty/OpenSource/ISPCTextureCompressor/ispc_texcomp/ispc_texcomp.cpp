////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016-2019, Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
////////////////////////////////////////////////////////////////////////////////

#include "ispc_texcomp.h"
#include "kernel_ispc.h"
#include <memory.h> // memcpy

void GetProfile_ultrafast(bc7_enc_settings* settings)
{
    settings->channels = 3;

	// mode02
	settings->mode_selection[0] = false;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode13
	settings->mode_selection[1] = false;
	settings->fastSkipTreshold_mode1 = 3;
	settings->fastSkipTreshold_mode3 = 1;
    settings->fastSkipTreshold_mode7 = 0;

	settings->refineIterations[1] = 2;
	settings->refineIterations[3] = 1;

	// mode45
	settings->mode_selection[2] = false;

    settings->mode45_channel0 = 0;
	settings->refineIterations_channel = 0;
	settings->refineIterations[4] = 2;
	settings->refineIterations[5] = 2;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 1;
}

void GetProfile_veryfast(bc7_enc_settings* settings)
{
    settings->channels = 3;

	// mode02
	settings->mode_selection[0] = false;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode13
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 3;
	settings->fastSkipTreshold_mode3 = 1;
    settings->fastSkipTreshold_mode7 = 0;

	settings->refineIterations[1] = 2;
	settings->refineIterations[3] = 1;

	// mode45
	settings->mode_selection[2] = false;

    settings->mode45_channel0 = 0;
	settings->refineIterations_channel = 0;
	settings->refineIterations[4] = 2;
	settings->refineIterations[5] = 2;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 1;
}

void GetProfile_fast(bc7_enc_settings* settings)
{	
    settings->channels = 3;

	// mode02
	settings->mode_selection[0] = false;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode13
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 12;
	settings->fastSkipTreshold_mode3 = 4;
    settings->fastSkipTreshold_mode7 = 0;

	settings->refineIterations[1] = 2;
	settings->refineIterations[3] = 1;

	// mode45
	settings->mode_selection[2] = false;

    settings->mode45_channel0 = 0;
	settings->refineIterations_channel = 0;
	settings->refineIterations[4] = 2;
	settings->refineIterations[5] = 2;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2;
}

void GetProfile_basic(bc7_enc_settings* settings)
{	
    settings->channels = 3;

	// mode02
	settings->mode_selection[0] = true;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode13
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 8+4;
	settings->fastSkipTreshold_mode3 = 8;
    settings->fastSkipTreshold_mode7 = 0;

	settings->refineIterations[1] = 2;
	settings->refineIterations[3] = 2;

	// mode45
	settings->mode_selection[2] = true;

    settings->mode45_channel0 = 0;
	settings->refineIterations_channel = 2;
	settings->refineIterations[4] = 2;
	settings->refineIterations[5] = 2;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2;
}

void GetProfile_slow(bc7_enc_settings* settings)
{	
    settings->channels = 3;

	int moreRefine = 2;
	// mode02
	settings->mode_selection[0] = true;
	settings->skip_mode2 = false;

	settings->refineIterations[0] = 2+moreRefine;
	settings->refineIterations[2] = 2+moreRefine;

	// mode13
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 64;
	settings->fastSkipTreshold_mode3 = 64;
	settings->fastSkipTreshold_mode7 = 0;

	settings->refineIterations[1] = 2+moreRefine;
	settings->refineIterations[3] = 2+moreRefine;

	// mode45
	settings->mode_selection[2] = true;

    settings->mode45_channel0 = 0;
	settings->refineIterations_channel = 2+moreRefine;
	settings->refineIterations[4] = 2+moreRefine;
	settings->refineIterations[5] = 2+moreRefine;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2+moreRefine;
}

void GetProfile_alpha_ultrafast(bc7_enc_settings* settings)
{	
    settings->channels = 4;

    // mode02
	settings->mode_selection[0] = false;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode137
	settings->mode_selection[1] = false;
	settings->fastSkipTreshold_mode1 = 0;
	settings->fastSkipTreshold_mode3 = 0;
    settings->fastSkipTreshold_mode7 = 4;

	settings->refineIterations[1] = 1;
	settings->refineIterations[3] = 1;
    settings->refineIterations[7] = 2;

	// mode45
	settings->mode_selection[2] = true;
    
    settings->mode45_channel0 = 3;
    settings->refineIterations_channel = 1;
	settings->refineIterations[4] = 1;
	settings->refineIterations[5] = 1;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2;
}

void GetProfile_alpha_veryfast(bc7_enc_settings* settings)
{	
    settings->channels = 4;

    // mode02
	settings->mode_selection[0] = false;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode137
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 0;
	settings->fastSkipTreshold_mode3 = 0;
    settings->fastSkipTreshold_mode7 = 4;

	settings->refineIterations[1] = 1;
	settings->refineIterations[3] = 1;
    settings->refineIterations[7] = 2;

	// mode45
	settings->mode_selection[2] = true;
    
    settings->mode45_channel0 = 3;
    settings->refineIterations_channel = 2;
	settings->refineIterations[4] = 2;
	settings->refineIterations[5] = 2;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2;
}

void GetProfile_alpha_fast(bc7_enc_settings* settings)
{	
    settings->channels = 4;

    // mode02
	settings->mode_selection[0] = false;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode137
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 4;
	settings->fastSkipTreshold_mode3 = 4;
    settings->fastSkipTreshold_mode7 = 8;

	settings->refineIterations[1] = 1;
	settings->refineIterations[3] = 1;
    settings->refineIterations[7] = 2;

	// mode45
	settings->mode_selection[2] = true;
    
    settings->mode45_channel0 = 3;
    settings->refineIterations_channel = 2;
	settings->refineIterations[4] = 2;
	settings->refineIterations[5] = 2;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2;
}

void GetProfile_alpha_basic(bc7_enc_settings* settings)
{	
    settings->channels = 4;

    // mode02
	settings->mode_selection[0] = true;
	settings->skip_mode2 = true;

	settings->refineIterations[0] = 2;
	settings->refineIterations[2] = 2;

	// mode137
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 8+4;
	settings->fastSkipTreshold_mode3 = 8;
    settings->fastSkipTreshold_mode7 = 8;

	settings->refineIterations[1] = 2;
	settings->refineIterations[3] = 2;
    settings->refineIterations[7] = 2;

	// mode45
	settings->mode_selection[2] = true;
    
    settings->mode45_channel0 = 0;
    settings->refineIterations_channel = 2;
	settings->refineIterations[4] = 2;
	settings->refineIterations[5] = 2;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2;
}

void GetProfile_alpha_slow(bc7_enc_settings* settings)
{	
    settings->channels = 4;

	int moreRefine = 2;
	// mode02
	settings->mode_selection[0] = true;
	settings->skip_mode2 = false;

	settings->refineIterations[0] = 2+moreRefine;
	settings->refineIterations[2] = 2+moreRefine;

	// mode137
	settings->mode_selection[1] = true;
	settings->fastSkipTreshold_mode1 = 64;
	settings->fastSkipTreshold_mode3 = 64;
    settings->fastSkipTreshold_mode7 = 64;

	settings->refineIterations[1] = 2+moreRefine;
	settings->refineIterations[3] = 2+moreRefine;
	settings->refineIterations[7] = 2+moreRefine;

	// mode45
	settings->mode_selection[2] = true;

    settings->mode45_channel0 = 0;
	settings->refineIterations_channel = 2+moreRefine;
	settings->refineIterations[4] = 2+moreRefine;
	settings->refineIterations[5] = 2+moreRefine;

	// mode6
	settings->mode_selection[3] = true;

	settings->refineIterations[6] = 2+moreRefine;
}

void GetProfile_bc6h_veryfast(bc6h_enc_settings* settings)
{
    settings->slow_mode = false;
    settings->fast_mode = true;
    settings->fastSkipTreshold = 0;
    settings->refineIterations_1p = 0;
    settings->refineIterations_2p = 0;
}

void GetProfile_bc6h_fast(bc6h_enc_settings* settings)
{
    settings->slow_mode = false;
    settings->fast_mode = true;
    settings->fastSkipTreshold = 2;
    settings->refineIterations_1p = 0;
    settings->refineIterations_2p = 1;
}

void GetProfile_bc6h_basic(bc6h_enc_settings* settings)
{
    settings->slow_mode = false;
    settings->fast_mode = false;
    settings->fastSkipTreshold = 4;
    settings->refineIterations_1p = 2;
    settings->refineIterations_2p = 2;
}

void GetProfile_bc6h_slow(bc6h_enc_settings* settings)
{
    settings->slow_mode = true;
    settings->fast_mode = false;
    settings->fastSkipTreshold = 10;
    settings->refineIterations_1p = 2;
    settings->refineIterations_2p = 2;
}

void GetProfile_bc6h_veryslow(bc6h_enc_settings* settings)
{
    settings->slow_mode = true;
    settings->fast_mode = false;
    settings->fastSkipTreshold = 32;
    settings->refineIterations_1p = 2;
    settings->refineIterations_2p = 2;
}

void GetProfile_etc_slow(etc_enc_settings* settings)
{
    settings->fastSkipTreshold = 6;
}

void ReplicateBorders(rgba_surface* dst_slice, const rgba_surface* src_tex, int start_x, int start_y, int bpp)
{
    int bytes_per_pixel = bpp >> 3;
    
    bool aliasing = false;
    if (&src_tex->ptr[src_tex->stride * start_y + bytes_per_pixel * start_x] == dst_slice->ptr) aliasing = true;

    for (int y = 0; y < dst_slice->height; y++)
    for (int x = 0; x < dst_slice->width; x++)
    {
        int xx = start_x + x;
        int yy = start_y + y;

        if (aliasing && xx < src_tex->width && yy < src_tex->height) continue;

        if (xx >= src_tex->width) xx = src_tex->width - 1;
        if (yy >= src_tex->height) yy = src_tex->height - 1;

        void* dst = &dst_slice->ptr[dst_slice->stride * y + bytes_per_pixel * x];
        void* src = &src_tex->ptr[src_tex->stride * yy + bytes_per_pixel * xx];

        memcpy(dst, src, bytes_per_pixel);
    }
}

void CompressBlocksBC1(const rgba_surface* src, uint8_t* dst)
{
	ispc::CompressBlocksBC1_ispc((ispc::rgba_surface*)src, dst);
}

void CompressBlocksBC3(const rgba_surface* src, uint8_t* dst)
{
	ispc::CompressBlocksBC3_ispc((ispc::rgba_surface*)src, dst);
}

void CompressBlocksBC4(const rgba_surface* src, uint8_t* dst)
{
	ispc::CompressBlocksBC4_ispc((ispc::rgba_surface*)src, dst);
}

void CompressBlocksBC5(const rgba_surface* src, uint8_t* dst)
{
	ispc::CompressBlocksBC5_ispc((ispc::rgba_surface*)src, dst);
}

void CompressBlocksBC7(const rgba_surface* src, uint8_t* dst, bc7_enc_settings* settings)
{
	ispc::CompressBlocksBC7_ispc((ispc::rgba_surface*)src, dst, (ispc::bc7_enc_settings*)settings);
}

void CompressBlocksBC6H(const rgba_surface* src, uint8_t* dst, bc6h_enc_settings* settings)
{
    ispc::CompressBlocksBC6H_ispc((ispc::rgba_surface*)src, dst, (ispc::bc6h_enc_settings*)settings);
}

void CompressBlocksETC1(const rgba_surface* src, uint8_t* dst, etc_enc_settings* settings)
{
    ispc::CompressBlocksETC1_ispc((ispc::rgba_surface*)src, dst, (ispc::etc_enc_settings*)settings);
}
