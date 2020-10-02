/**********************************************************************
Copyright (c) 2020 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#ifndef FFX_SSSR_INDIRECT_ARGS
#define FFX_SSSR_INDIRECT_ARGS

RWBuffer<uint> g_tile_counter       : register(u0);
RWBuffer<uint> g_ray_counter        : register(u1);
RWBuffer<uint> g_intersect_args     : register(u2);
RWBuffer<uint> g_denoiser_args      : register(u3);

[numthreads(1, 1, 1)]
void main()
{
    uint tile_counter = g_tile_counter[0];
    uint ray_counter = g_ray_counter[0];

    g_tile_counter[0] = 0;
    g_ray_counter[0] = 0;

    g_intersect_args[0] = (ray_counter + 63) / 64;
    g_intersect_args[1] = 1;
    g_intersect_args[2] = 1;

    g_denoiser_args[0] = tile_counter;
    g_denoiser_args[1] = 1;
    g_denoiser_args[2] = 1;
}

#endif // FFX_SSSR_INDIRECT_ARGS