///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2016, Intel Corporation
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated 
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation 
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to the following conditions:
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of 
// the Software.
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
// THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
// SOFTWARE.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ispc_texcomp.h"
#include "kernel_astc_ispc.h"
#include <cassert>
#include <cstring>
#include <algorithm>
#include <vector>
#include <limits>

void GetProfile_astc_fast(astc_enc_settings* settings, int block_width, int block_height)
{
    settings->block_width = block_width;
    settings->block_height = block_height;
    settings->channels = 3;

    settings->fastSkipTreshold = 5;
    settings->refineIterations = 2;
}

void GetProfile_astc_alpha_fast(astc_enc_settings* settings, int block_width, int block_height)
{
    settings->block_width = block_width;
    settings->block_height = block_height;
    settings->channels = 4;

    settings->fastSkipTreshold = 5;
    settings->refineIterations = 2;
}

void GetProfile_astc_alpha_slow(astc_enc_settings* settings, int block_width, int block_height)
{
    settings->block_width = block_width;
    settings->block_height = block_height;
    settings->channels = 4;

    settings->fastSkipTreshold = 64;
    settings->refineIterations = 2;
}

struct astc_block
{
    int width;
    int height;
    uint8_t dual_plane;
    int weight_range;
    uint8_t weights[64];
    int color_component_selector;

    int partitions;
    int partition_id;
    int color_endpoint_pairs;
    int channels;
    int color_endpoint_modes[4];
    int endpoint_range;
    uint8_t endpoints[18];
};

bool can_store(int value, int bits)
{
    if (value < 0) return false;
    if (value >= 1 << bits) return false;
    return true;
}

int pack_block_mode(astc_block* block)
{
    int block_mode = 0;

    int D = block->dual_plane;
    int H = block->weight_range >= 6;
    int DH = D * 2 + H;
    int R = block->weight_range + 2 - ((H > 0) ? 6 : 0);
    R = R / 2 + R % 2 * 4;

    if (can_store(block->width - 4, 2) && can_store(block->height - 2, 2))
    {
        int B = block->width - 4;
        int A = block->height - 2;

        block_mode = (DH << 9) | (B << 7) | (A << 5) | ((R & 4) << 2) | (R & 3);
    }

    if (can_store(block->width - 8, 2) && can_store(block->height - 2, 2))
    {
        int B = block->width - 8;
        int A = block->height - 2;

        block_mode = (DH << 9) | (B << 7) | (A << 5) | ((R & 4) << 2) | 4 | (R & 3);
    }

    if (can_store(block->width - 2, 2) && can_store(block->height - 8, 2))
    {
        int A = block->width - 2;
        int B = block->height - 8;

        block_mode = (DH << 9) | (B << 7) | (A << 5) | ((R & 4) << 2) | 8 | (R & 3);
    }

    if (can_store(block->width - 2, 2) && can_store(block->height - 6, 1))
    {
        int A = block->width - 2;
        int B = block->height - 6;

        block_mode = (DH << 9) | (B << 7) | (A << 5) | ((R & 4) << 2) | 12 | (R & 3);
    }

    if (can_store(block->width - 2, 1) && can_store(block->height - 2, 2))
    {
        int B = block->width;
        int A = block->height - 2;

        block_mode = (DH << 9) | (B << 7) | (A << 5) | ((R & 4) << 2) | 12 | (R & 3);
    }

    if (DH == 0 && can_store(block->width - 6, 2) && can_store(block->height - 6, 2))
    {
        int A = block->width - 6;
        int B = block->height - 6;

        block_mode = (B << 9) | 256 | (A << 5) | (R << 2);
    }

    return block_mode;
}

int range_table[][3] =
{
    //2^ 3^ 5^
    { 1, 0, 0 }, // 0..1
    { 0, 1, 0 }, // 0..2
    { 2, 0, 0 }, // 0..3

    { 0, 0, 1 }, // 0..4
    { 1, 1, 0 }, // 0..5
    { 3, 0, 0 }, // 0..7

    { 1, 0, 1 }, // 0..9
    { 2, 1, 0 }, // 0..11
    { 4, 0, 0 }, // 0..15

    { 2, 0, 1 }, // 0..19
    { 3, 1, 0 }, // 0..23
    { 5, 0, 0 }, // 0..31

    { 3, 0, 1 }, // 0..39
    { 4, 1, 0 }, // 0..47
    { 6, 0, 0 }, // 0..63

    { 4, 0, 1 }, // 0..79
    { 5, 1, 0 }, // 0..95
    { 7, 0, 0 }, // 0..127

    { 5, 0, 1 }, // 0..159
    { 6, 1, 0 }, // 0..191
    { 8, 0, 0 }, // 0..255
};

int get_levels(int range)
{
    return (1 + 2 * range_table[range][1] + 4 * range_table[range][2]) << range_table[range][0];
}

int sequence_bits(int count, int range)
{
    int bits = count * range_table[range][0];
    bits += (count * range_table[range][1] * 8 + 4) / 5;
    bits += (count * range_table[range][2] * 7 + 2) / 3;
    return bits;
}

void set_bits(uint32_t data[4], int* pos, int bits, uint32_t value)
{
    assert(bits <= 25);
    uint32_t word = *(uint32_t*)(((uint8_t*)data) + *pos / 8);

    //uint32_t mask = (1 << bits) - 1;
    word |= value << (*pos % 8);

    *(uint32_t*)(((uint8_t*)data) + *pos / 8) = word;
    *pos += bits;
}

uint32_t get_field(uint32_t input, int a, int b)
{
    assert(a >= b);
    return (input >> b) & ((1 << (a - b + 1)) - 1);
}

uint32_t get_bit(uint32_t input, int a)
{
    return get_field(input, a, a);
}

void pack_five_trits(uint32_t data[4], int sequence[5], int* pos, int n)
{
    int t[5];
    int m[5];

    for (int i = 0; i < 5; i++)
    {
        t[i] = sequence[i] >> n;
        m[i] = sequence[i] - (t[i] << n);
    }

    int C;

    if (t[1] == 2 && t[2] == 2)
    {
        C = 3 * 4 + t[0];
    }
    else if (t[2] == 2)
    {
        C = t[1] * 16 + t[0] * 4 + 3;
    }
    else
    {
        C = t[2] * 16 + t[1] * 4 + t[0];
    }

    int T;

    if (t[3] == 2 && t[4] == 2)
    {
        T = get_field(C, 4, 2) * 32 + 7 * 4 + get_field(C, 1, 0);
    }
    else
    {
        T = get_field(C, 4, 0);
        if (t[4] == 2)
        {
            T += t[3] * 128 + 3 * 32;
        }
        else
        {
            T += t[4] * 128 + t[3] * 32;
        }
    }

    uint32_t pack1 = 0;
    pack1 |= m[0];
    pack1 |= get_field(T, 1, 0) << n;
    pack1 |= m[1] << (2 + n);

    uint32_t pack2 = 0; 
    pack2 |= get_field(T, 3, 2);
    pack2 |= m[2] << 2;
    pack2 |= get_field(T, 4, 4) << (2 + n);
    pack2 |= m[3] << (3 + n);
    pack2 |= get_field(T, 6, 5) << (3 + n * 2);
    pack2 |= m[4] << (5 + n * 2);
    pack2 |= get_field(T, 7, 7) << (5 + n * 3);

    set_bits(data, pos, 2 + n * 2, pack1);
    set_bits(data, pos, 6 + n * 3, pack2);
}

void pack_three_quint(uint32_t data[4], int sequence[3], int* pos, int n)
{
    int q[3];
    int m[3];

    for (int i = 0; i < 3; i++)
    {
        q[i] = sequence[i] >> n;
        m[i] = sequence[i] - (q[i] << n);
    }

    int Q;

    if (q[0] == 4 && q[1] == 4)
    {
        Q = get_field(q[2], 1, 0) * 8 + 3 * 2 + get_bit(q[2], 2);
    }
    else
    {
        int C;
        if (q[1] == 4)
        {
            C = (q[0] << 3) + 5;
        }
        else
        {
            C = (q[1] << 3) + q[0];
        }

        if (q[2] == 4)
        {
            Q = get_field(~C, 2, 1) * 32 + get_field(C, 4, 3) * 8 + 3 * 2 + get_bit(C, 0);
        }
        else
        {
            Q = q[2] * 32 + get_field(C, 4, 0);
        }
    }

    uint32_t pack = 0;
    pack |= m[0];
    pack |= get_field(Q, 2, 0) << n;
    pack |= m[1] << (3 + n);
    pack |= get_field(Q, 4, 3) << (3 + n * 2);
    pack |= m[2] << (5 + n * 2);
    pack |= get_field(Q, 6, 5) << (5 + n * 3);

    set_bits(data, pos, 7 + n * 3, pack);
}

void pack_integer_sequence(uint32_t output_data[4], uint8_t sequence[], int pos, int count, int range)
{
    int n = range_table[range][0];
    int bits = sequence_bits(count, range);
    int pos0 = pos;

    uint32_t data[5] = { 0 };
    if (range_table[range][1] == 1)
    {
        for (int j = 0; j < (count + 4) / 5; j++)
        {
            int temp[5] = { 0 };
            for (int i = 0; i < std::min(count - j * 5, 5); i++) temp[i] = sequence[j * 5 + i];
            pack_five_trits(data, temp, &pos, n);
        }
    }
    else if (range_table[range][2] == 1)
    {
        for (int j = 0; j < (count + 2) / 3; j++)
        {
            int temp[3] = { 0 };
            for (int i = 0; i < std::min(count - j * 3, 3); i++) temp[i] = sequence[j * 3 + i];
            pack_three_quint(data, temp, &pos, n);
        }
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            set_bits(data, &pos, n, sequence[i]);
        }
    }

    if (pos0 + bits < 96) data[3] = 0;
    if (pos0 + bits < 64) data[2] = 0;
    if (pos0 + bits < 32) data[1] = 0;
    data[(pos0 + bits) / 32] &= (1 << ((pos0 + bits) % 32)) - 1;

    for (int k = 0; k < 4; k++) output_data[k] |= data[k];
}

uint32_t reverse_bits_32(uint32_t input)
{
    uint32_t t = input;
    t = (t << 16) | (t >> 16);
    t = ((t & 0x00FF00FF) << 8) | ((t & 0xFF00FF00) >> 8);
    t = ((t & 0x0F0F0F0F) << 4) | ((t & 0xF0F0F0F0) >> 4);
    t = ((t & 0x33333333) << 2) | ((t & 0xCCCCCCCC) >> 2);
    t = ((t & 0x55555555) << 1) | ((t & 0xAAAAAAAA) >> 1);

    return t;
}

void pack_block(uint32_t data[4], astc_block* block)
{
    memset(data, 0, 16);

    int pos = 0;
    set_bits(data, &pos, 11, pack_block_mode(block));

    int num_weights = block->width * block->height * (block->dual_plane ? 2 : 1);
    int weight_bits = sequence_bits(num_weights, block->weight_range);
    int extra_bits = 0;

    assert(num_weights <= 64);
    assert(24 <= weight_bits && weight_bits <= 96);

    set_bits(data, &pos, 2, block->partitions - 1);
    if (block->partitions > 1)
    {
        set_bits(data, &pos, 10, block->partition_id);

        int min_cem = 16;
        int max_cem = 0;
        for (int j = 0; j < block->partitions; j++)
        {
            min_cem = std::min(min_cem, block->color_endpoint_modes[j]);
            max_cem = std::max(max_cem, block->color_endpoint_modes[j]);
        }
        assert(max_cem / 4 <= min_cem / 4 + 1);

        int CEM = block->color_endpoint_modes[0] << 2;
        if (max_cem != min_cem)
        {
            CEM = std::min(3, min_cem / 4 + 1);
            for (int j = 0; j < block->partitions; j++)
            {
                int c = block->color_endpoint_modes[j] / 4 - ((CEM & 3) - 1);
                int m = block->color_endpoint_modes[j] % 4;
                assert(c == 0 || c == 1);
                CEM |= c << (2 + j);
                CEM |= m << (2 + block->partitions + 2 * j);
            }
            extra_bits = 3 * block->partitions - 4;
            int pos2 = 128 - weight_bits - extra_bits;
            set_bits(data, &pos2, extra_bits, CEM >> 6);
        }
        
        set_bits(data, &pos, 6, CEM & 63);
    }
    else
    {
        set_bits(data, &pos, 4, block->color_endpoint_modes[0]);
    }
    
    if (block->dual_plane)
    {
        assert(block->partitions < 4);
        extra_bits += 2;
        int pos2 = 128 - weight_bits - extra_bits;
        set_bits(data, &pos2, 2, block->color_component_selector);
    }

    int config_bits = pos + extra_bits;
    int remaining_bits = 128 - config_bits - weight_bits;

    int num_cem_pairs = 0;
    for (int j = 0; j < block->partitions; j++) num_cem_pairs += 1 + block->color_endpoint_modes[j] / 4;

    assert(num_cem_pairs <= 9);

    int endpoint_range = -1;
    for (int range = 20; range>0; range--)
    {
        int bits = sequence_bits(2 * num_cem_pairs, range);
        if (bits <= remaining_bits)
        {
            endpoint_range = range;
            break;
        }
    }

    assert(endpoint_range >= 4);
    assert(block->endpoint_range == endpoint_range);

    pack_integer_sequence(data, block->endpoints, pos, 2 * num_cem_pairs, endpoint_range);
    
    uint32_t rdata[4] = { 0, 0, 0, 0 };
    pack_integer_sequence(rdata, block->weights, 0, num_weights, block->weight_range);

    for (int i = 0; i < 4; i++) data[i] |= reverse_bits_32(rdata[3 - i]);    
}

void atsc_rank(const rgba_surface* src, int xx, int yy, uint32_t* mode_buffer, astc_enc_settings* settings)
{
    ispc::astc_rank_ispc((ispc::rgba_surface*)src, xx, yy, mode_buffer, (ispc::astc_enc_settings*)settings);
}

extern "C" void pack_block_c(uint32_t data[4], ispc::astc_block* block)
{
    assert(sizeof(ispc::astc_block) == sizeof(astc_block));
    pack_block(data, (astc_block*)block);
}

void setup_list_context(ispc::astc_enc_context* ctx, uint32_t packed_mode)
{
    ctx->width = 2 + get_field(packed_mode, 15, 13); // 2..8 <= 2^3
    ctx->height = 2 + get_field(packed_mode, 18, 16); // 2..8 <= 2^3
    ctx->dual_plane = get_field(packed_mode, 19, 19); // 0 or 1
    ctx->partitions = 1;
    
    int color_endpoint_modes0 = get_field(packed_mode, 7, 6) * 2 + 6; // 6, 8, 10 or 12
    ctx->color_endpoint_pairs = 1 + (color_endpoint_modes0 / 4);

    ctx->channels = (color_endpoint_modes0 > 8) ? 4 : 3;
}

void astc_encode(const rgba_surface* src, float* block_scores, uint8_t* dst, uint64_t* list, astc_enc_settings* settings)
{
    ispc::astc_enc_context list_context;
    setup_list_context(&list_context, uint32_t(list[1] & 0xFFFFFFFF));

    assert(sizeof(ispc::rgba_surface) == sizeof(rgba_surface));
    assert(sizeof(ispc::astc_enc_settings) == sizeof(astc_enc_settings));
    ispc::astc_encode_ispc((ispc::rgba_surface*)src, block_scores, dst, list, &list_context, (ispc::astc_enc_settings*)settings);
}

void CompressBlocksASTC(const rgba_surface* src, uint8_t* dst, astc_enc_settings* settings)
{
    assert(src->height % settings->block_height == 0);
    assert(src->width % settings->block_width == 0);
    
    assert(settings->block_height <= 8);
    assert(settings->block_width <= 8);
    
    int tex_width = src->width / settings->block_width;
    int programCount = ispc::get_programCount();

    std::vector<float> block_scores(tex_width * src->height / settings->block_height);

    for (int yy = 0; yy < src->height / settings->block_height; yy++)
    for (int xx = 0; xx < tex_width; xx++)
    {
        block_scores[yy * tex_width + xx] = std::numeric_limits<float>::infinity();
    }

    int mode_list_size = 3334;
    int list_size = programCount;
    std::vector<uint64_t> mode_lists(list_size * mode_list_size);
    std::vector<uint32_t> mode_buffer(programCount * settings->fastSkipTreshold);

    for (int yy = 0; yy < src->height / settings->block_height; yy++)
    for (int _x = 0; _x < (tex_width + programCount - 1) / programCount; _x++)
    {
        int xx = _x * programCount;
        atsc_rank(src, xx, yy, mode_buffer.data(), settings);
        
        for (int i = 0; i < settings->fastSkipTreshold; i++)
        for (int k = 0; k < programCount; k++)
        {
            if (xx + k >= tex_width) continue;
                
            uint32_t offset = (yy << 16) + (xx + k);
            uint32_t mode = mode_buffer[programCount * i + k];
            int mode_bin = mode >> 20;
            uint64_t* mode_list = &mode_lists[list_size * mode_bin];

            if (*mode_list < (uint32_t)programCount - 1)
            {
                int index = int(mode_list[0] + 1);
                mode_list[0] = index;

                mode_list[index] = (uint64_t(offset) << 32) + mode;
            }
            else
            {
                mode_list[0] = (uint64_t(offset) << 32) + mode;

                astc_encode(src, block_scores.data(), dst, mode_list, settings);
                memset(mode_list, 0, list_size * sizeof(uint64_t));
            }                
        }
    }

    for (int mode_bin = 0; mode_bin < mode_list_size; mode_bin++)
    {
        uint64_t* mode_list = &mode_lists[list_size * mode_bin];
        if (mode_list[0] == 0) continue;
        mode_list[0] = 0;

        astc_encode(src, block_scores.data(), dst, mode_list, settings);
        memset(mode_list, 0, list_size * sizeof(uint64_t));
    }
}
