/*
---------------------------------------------------------------------------
Copyright (c) 1998-2010, Brian Gladman, Worcester, UK. All rights reserved.

The redistribution and use of this software (with or without changes)
is allowed without the payment of fees or royalties provided that:

  source code distributions include the above copyright notice, this
  list of conditions and the following disclaimer;

  binary distributions include the above copyright notice, this list
  of conditions and the following disclaimer in their documentation.

This software is provided 'as is' with no explicit or implied warranties
in respect of its operation, including, but not limited to, correctness
and fitness for purpose.
---------------------------------------------------------------------------
Issue Date: 20/12/2007

This code implements sha256, sha384 and sha512 but the latter two 
functions rely on efficient 64-bit integer operations that may not be 
very efficient on 32-bit machines

The sha256 functions use a type 'sha256_ctx' to hold details of the
current hash state and uses the following three calls:

      void sha256_begin( sha256_ctx ctx[1] )
      void sha256_hash( const unsigned char data[],
                           unsigned long len, sha256_ctx ctx[1] )
      void sha_end1( unsigned char hval[], sha256_ctx ctx[1] )

The first subroutine initialises a hash computation by setting up the
context in the sha256_ctx context. The second subroutine hashes 8-bit
bytes from array data[] into the hash state withinh sha256_ctx context,
the number of bytes to be hashed being given by the the unsigned long
integer len.  The third subroutine completes the hash calculation and
places the resulting digest value in the array of 8-bit bytes hval[].

The sha384 and sha512 functions are similar and use the interfaces:

      void sha384_begin( sha384_ctx ctx[1] );
      void sha384_hash( const unsigned char data[],
                           unsigned long len, sha384_ctx ctx[1] );
      void sha384_end( unsigned char hval[], sha384_ctx ctx[1] );

      void sha512_begin( sha512_ctx ctx[1] );
      void sha512_hash( const unsigned char data[],
                           unsigned long len, sha512_ctx ctx[1] );
      void sha512_end( unsigned char hval[], sha512_ctx ctx[1] );

In addition there is a function sha2 that can be used to call all these
functions using a call with a hash length parameter as follows:

      int sha2_begin( unsigned long len, sha2_ctx ctx[1] );
      void sha2_hash( const unsigned char data[],
                           unsigned long len, sha2_ctx ctx[1] );
      void sha2_end( unsigned char hval[], sha2_ctx ctx[1] );

The data block length in any one call to any of these hash functions must 
be no more than 2^32 - 1 bits or 2^29 - 1 bytes.

My thanks to Erik Andersen <andersen@codepoet.org> for testing this code
on big-endian systems and for his assistance with corrections
*/

#if 1
#define UNROLL_SHA2         /* for SHA2 loop unroll     */
#endif

#include <string.h>         /* for memcpy() etc.        */

#include "sha2.h"
#include "brg_endian.h"

#if defined(__cplusplus)
extern "C"
{
#endif

#if defined( _MSC_VER ) && ( _MSC_VER > 800 )
#pragma intrinsic(memcpy)
#pragma intrinsic(memset)
#endif

#if 0 && defined(_MSC_VER)
#define rotl32 _lrotl
#define rotr32 _lrotr
#else
#define rotl32(x,n)   (((x) << n) | ((x) >> (32 - n)))
#define rotr32(x,n)   (((x) >> n) | ((x) << (32 - n)))
#endif

#if !defined(bswap_32)
#define bswap_32(x) ((rotr32((x), 24) & 0x00ff00ff) | (rotr32((x), 8) & 0xff00ff00))
#endif

#if (PLATFORM_BYTE_ORDER == IS_LITTLE_ENDIAN)
#define SWAP_BYTES
#else
#undef  SWAP_BYTES
#endif

#if 0

#define ch(x,y,z)       (((x) & (y)) ^ (~(x) & (z)))
#define maj(x,y,z)      (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

#else   /* Thanks to Rich Schroeppel and Colin Plumb for the following      */

#define ch(x,y,z)       ((z) ^ ((x) & ((y) ^ (z))))
#define maj(x,y,z)      (((x) & (y)) | ((z) & ((x) ^ (y))))

#endif

/* round transforms for SHA256 and SHA512 compression functions */

#define vf(n,i) v[(n - i) & 7]

#define hf(i) (p[i & 15] += \
    g_1(p[(i + 14) & 15]) + p[(i + 9) & 15] + g_0(p[(i + 1) & 15]))

#define v_cycle(i,j)                                \
    vf(7,i) += (j ? hf(i) : p[i]) + k_0[i+j]        \
    + s_1(vf(4,i)) + ch(vf(4,i),vf(5,i),vf(6,i));   \
    vf(3,i) += vf(7,i);                             \
    vf(7,i) += s_0(vf(0,i))+ maj(vf(0,i),vf(1,i),vf(2,i))

#if defined(SHA_224) || defined(SHA_256)

#define SHA256_MASK (SHA256_BLOCK_SIZE - 1)

#if defined(SWAP_BYTES)
#define bsw_32(p,n) \
    { int _i = (n); while(_i--) ((uint32_t*)p)[_i] = bswap_32(((uint32_t*)p)[_i]); }
#else
#define bsw_32(p,n)
#endif

#define s_0(x)  (rotr32((x),  2) ^ rotr32((x), 13) ^ rotr32((x), 22))
#define s_1(x)  (rotr32((x),  6) ^ rotr32((x), 11) ^ rotr32((x), 25))
#define g_0(x)  (rotr32((x),  7) ^ rotr32((x), 18) ^ ((x) >>  3))
#define g_1(x)  (rotr32((x), 17) ^ rotr32((x), 19) ^ ((x) >> 10))
#define k_0     k256

/* rotated SHA256 round definition. Rather than swapping variables as in    */
/* FIPS-180, different variables are 'rotated' on each round, returning     */
/* to their starting positions every eight rounds                           */

#define q(n)  v##n

#define one_cycle(a,b,c,d,e,f,g,h,k,w)  \
    q(h) += s_1(q(e)) + ch(q(e), q(f), q(g)) + k + w; \
    q(d) += q(h); q(h) += s_0(q(a)) + maj(q(a), q(b), q(c))

/* SHA256 mixing data   */

const uint32_t k256[64] =
{   0x428a2f98ul, 0x71374491ul, 0xb5c0fbcful, 0xe9b5dba5ul,
    0x3956c25bul, 0x59f111f1ul, 0x923f82a4ul, 0xab1c5ed5ul,
    0xd807aa98ul, 0x12835b01ul, 0x243185beul, 0x550c7dc3ul,
    0x72be5d74ul, 0x80deb1feul, 0x9bdc06a7ul, 0xc19bf174ul,
    0xe49b69c1ul, 0xefbe4786ul, 0x0fc19dc6ul, 0x240ca1ccul,
    0x2de92c6ful, 0x4a7484aaul, 0x5cb0a9dcul, 0x76f988daul,
    0x983e5152ul, 0xa831c66dul, 0xb00327c8ul, 0xbf597fc7ul,
    0xc6e00bf3ul, 0xd5a79147ul, 0x06ca6351ul, 0x14292967ul,
    0x27b70a85ul, 0x2e1b2138ul, 0x4d2c6dfcul, 0x53380d13ul,
    0x650a7354ul, 0x766a0abbul, 0x81c2c92eul, 0x92722c85ul,
    0xa2bfe8a1ul, 0xa81a664bul, 0xc24b8b70ul, 0xc76c51a3ul,
    0xd192e819ul, 0xd6990624ul, 0xf40e3585ul, 0x106aa070ul,
    0x19a4c116ul, 0x1e376c08ul, 0x2748774cul, 0x34b0bcb5ul,
    0x391c0cb3ul, 0x4ed8aa4aul, 0x5b9cca4ful, 0x682e6ff3ul,
    0x748f82eeul, 0x78a5636ful, 0x84c87814ul, 0x8cc70208ul,
    0x90befffaul, 0xa4506cebul, 0xbef9a3f7ul, 0xc67178f2ul,
};

/* Compile 64 bytes of hash data into SHA256 digest value   */
/* NOTE: this routine assumes that the byte order in the    */
/* ctx->wbuf[] at this point is such that low address bytes */
/* in the ORIGINAL byte stream will go into the high end of */
/* words on BOTH big and little endian systems              */

VOID_RETURN sha256_compile(sha256_ctx ctx[1])
{
#if !defined(UNROLL_SHA2)

    uint32_t j, *p = ctx->wbuf, v[8];

    memcpy(v, ctx->hash, sizeof(ctx->hash));

    for(j = 0; j < 64; j += 16)
    {
        v_cycle( 0, j); v_cycle( 1, j);
        v_cycle( 2, j); v_cycle( 3, j);
        v_cycle( 4, j); v_cycle( 5, j);
        v_cycle( 6, j); v_cycle( 7, j);
        v_cycle( 8, j); v_cycle( 9, j);
        v_cycle(10, j); v_cycle(11, j);
        v_cycle(12, j); v_cycle(13, j);
        v_cycle(14, j); v_cycle(15, j);
    }

    ctx->hash[0] += v[0]; ctx->hash[1] += v[1];
    ctx->hash[2] += v[2]; ctx->hash[3] += v[3];
    ctx->hash[4] += v[4]; ctx->hash[5] += v[5];
    ctx->hash[6] += v[6]; ctx->hash[7] += v[7];

#else

    uint32_t *p = ctx->wbuf,v0,v1,v2,v3,v4,v5,v6,v7;

    v0 = ctx->hash[0]; v1 = ctx->hash[1];
    v2 = ctx->hash[2]; v3 = ctx->hash[3];
    v4 = ctx->hash[4]; v5 = ctx->hash[5];
    v6 = ctx->hash[6]; v7 = ctx->hash[7];

    one_cycle(0,1,2,3,4,5,6,7,k256[ 0],p[ 0]);
    one_cycle(7,0,1,2,3,4,5,6,k256[ 1],p[ 1]);
    one_cycle(6,7,0,1,2,3,4,5,k256[ 2],p[ 2]);
    one_cycle(5,6,7,0,1,2,3,4,k256[ 3],p[ 3]);
    one_cycle(4,5,6,7,0,1,2,3,k256[ 4],p[ 4]);
    one_cycle(3,4,5,6,7,0,1,2,k256[ 5],p[ 5]);
    one_cycle(2,3,4,5,6,7,0,1,k256[ 6],p[ 6]);
    one_cycle(1,2,3,4,5,6,7,0,k256[ 7],p[ 7]);
    one_cycle(0,1,2,3,4,5,6,7,k256[ 8],p[ 8]);
    one_cycle(7,0,1,2,3,4,5,6,k256[ 9],p[ 9]);
    one_cycle(6,7,0,1,2,3,4,5,k256[10],p[10]);
    one_cycle(5,6,7,0,1,2,3,4,k256[11],p[11]);
    one_cycle(4,5,6,7,0,1,2,3,k256[12],p[12]);
    one_cycle(3,4,5,6,7,0,1,2,k256[13],p[13]);
    one_cycle(2,3,4,5,6,7,0,1,k256[14],p[14]);
    one_cycle(1,2,3,4,5,6,7,0,k256[15],p[15]);

    one_cycle(0,1,2,3,4,5,6,7,k256[16],hf( 0));
    one_cycle(7,0,1,2,3,4,5,6,k256[17],hf( 1));
    one_cycle(6,7,0,1,2,3,4,5,k256[18],hf( 2));
    one_cycle(5,6,7,0,1,2,3,4,k256[19],hf( 3));
    one_cycle(4,5,6,7,0,1,2,3,k256[20],hf( 4));
    one_cycle(3,4,5,6,7,0,1,2,k256[21],hf( 5));
    one_cycle(2,3,4,5,6,7,0,1,k256[22],hf( 6));
    one_cycle(1,2,3,4,5,6,7,0,k256[23],hf( 7));
    one_cycle(0,1,2,3,4,5,6,7,k256[24],hf( 8));
    one_cycle(7,0,1,2,3,4,5,6,k256[25],hf( 9));
    one_cycle(6,7,0,1,2,3,4,5,k256[26],hf(10));
    one_cycle(5,6,7,0,1,2,3,4,k256[27],hf(11));
    one_cycle(4,5,6,7,0,1,2,3,k256[28],hf(12));
    one_cycle(3,4,5,6,7,0,1,2,k256[29],hf(13));
    one_cycle(2,3,4,5,6,7,0,1,k256[30],hf(14));
    one_cycle(1,2,3,4,5,6,7,0,k256[31],hf(15));

    one_cycle(0,1,2,3,4,5,6,7,k256[32],hf( 0));
    one_cycle(7,0,1,2,3,4,5,6,k256[33],hf( 1));
    one_cycle(6,7,0,1,2,3,4,5,k256[34],hf( 2));
    one_cycle(5,6,7,0,1,2,3,4,k256[35],hf( 3));
    one_cycle(4,5,6,7,0,1,2,3,k256[36],hf( 4));
    one_cycle(3,4,5,6,7,0,1,2,k256[37],hf( 5));
    one_cycle(2,3,4,5,6,7,0,1,k256[38],hf( 6));
    one_cycle(1,2,3,4,5,6,7,0,k256[39],hf( 7));
    one_cycle(0,1,2,3,4,5,6,7,k256[40],hf( 8));
    one_cycle(7,0,1,2,3,4,5,6,k256[41],hf( 9));
    one_cycle(6,7,0,1,2,3,4,5,k256[42],hf(10));
    one_cycle(5,6,7,0,1,2,3,4,k256[43],hf(11));
    one_cycle(4,5,6,7,0,1,2,3,k256[44],hf(12));
    one_cycle(3,4,5,6,7,0,1,2,k256[45],hf(13));
    one_cycle(2,3,4,5,6,7,0,1,k256[46],hf(14));
    one_cycle(1,2,3,4,5,6,7,0,k256[47],hf(15));

    one_cycle(0,1,2,3,4,5,6,7,k256[48],hf( 0));
    one_cycle(7,0,1,2,3,4,5,6,k256[49],hf( 1));
    one_cycle(6,7,0,1,2,3,4,5,k256[50],hf( 2));
    one_cycle(5,6,7,0,1,2,3,4,k256[51],hf( 3));
    one_cycle(4,5,6,7,0,1,2,3,k256[52],hf( 4));
    one_cycle(3,4,5,6,7,0,1,2,k256[53],hf( 5));
    one_cycle(2,3,4,5,6,7,0,1,k256[54],hf( 6));
    one_cycle(1,2,3,4,5,6,7,0,k256[55],hf( 7));
    one_cycle(0,1,2,3,4,5,6,7,k256[56],hf( 8));
    one_cycle(7,0,1,2,3,4,5,6,k256[57],hf( 9));
    one_cycle(6,7,0,1,2,3,4,5,k256[58],hf(10));
    one_cycle(5,6,7,0,1,2,3,4,k256[59],hf(11));
    one_cycle(4,5,6,7,0,1,2,3,k256[60],hf(12));
    one_cycle(3,4,5,6,7,0,1,2,k256[61],hf(13));
    one_cycle(2,3,4,5,6,7,0,1,k256[62],hf(14));
    one_cycle(1,2,3,4,5,6,7,0,k256[63],hf(15));

    ctx->hash[0] += v0; ctx->hash[1] += v1;
    ctx->hash[2] += v2; ctx->hash[3] += v3;
    ctx->hash[4] += v4; ctx->hash[5] += v5;
    ctx->hash[6] += v6; ctx->hash[7] += v7;
#endif
}

/* SHA256 hash data in an array of bytes into hash buffer   */
/* and call the hash_compile function as required.          */

VOID_RETURN sha256_hash(const unsigned char data[], unsigned long len, sha256_ctx ctx[1])
{   uint32_t pos = (uint32_t)((ctx->count[0] >> 3) & SHA256_MASK);
    const unsigned char *sp = data;
    unsigned char *w = (unsigned char*)ctx->wbuf;
#if SHA2_BITS == 1
    uint32_t ofs = (ctx->count[0] & 7);
#else
    len <<= 3;
#endif
    if((ctx->count[0] += len) < len)
        ++(ctx->count[1]);

#if SHA2_BITS == 1
    if(ofs)                 /* if not on a byte boundary    */
    {
        if(ofs + len < 8)   /* if no added bytes are needed */
        {
            w[pos] |= (*sp >> ofs);
        }
        else                /* otherwise and add bytes      */
        {   unsigned char part = w[pos];

            while((int)(ofs + (len -= 8)) >= 0)
            {
                w[pos++] = part | (*sp >> ofs);
                part = *sp++ << (8 - ofs);
                if(pos == SHA256_BLOCK_SIZE)
                {
                    bsw_32(w, SHA256_BLOCK_SIZE >> 2);
                    sha256_compile(ctx); pos = 0;
                }
            }

            w[pos] = part;
        }
    }
    else    /* data is byte aligned */
#endif
    {   uint32_t space = SHA256_BLOCK_SIZE - pos;

        while(len >= (space << 3))
        {
            memcpy(w + pos, sp, space);
            bsw_32(w, SHA256_BLOCK_SIZE >> 2);
            sha256_compile(ctx); 
            sp += space; len -= (space << 3); 
            space = SHA256_BLOCK_SIZE; pos = 0;
        }
        memcpy(w + pos, sp, (len + 7 * SHA2_BITS) >> 3);
    }
}

/* SHA256 Final padding and digest calculation  */

static void sha_end1(unsigned char hval[], sha256_ctx ctx[1], const unsigned int hlen)
{   uint32_t    i = (uint32_t)((ctx->count[0] >> 3) & SHA256_MASK), m1;

    /* put bytes in the buffer in an order in which references to   */
    /* 32-bit words will put bytes with lower addresses into the    */
    /* top of 32 bit words on BOTH big and little endian machines   */
    bsw_32(ctx->wbuf, (i + 3 + SHA2_BITS) >> 2)

    /* we now need to mask valid bytes and add the padding which is */
    /* a single 1 bit and as many zero bits as necessary. Note that */
    /* we can always add the first padding byte here because the    */
    /* buffer always has at least one empty slot                    */
    m1 = (unsigned char)0x80 >> (ctx->count[0] & 7);
    ctx->wbuf[i >> 2] &= ((0xffffff00 | (~m1 + 1)) << 8 * (~i & 3));
    ctx->wbuf[i >> 2] |= (m1 << 8 * (~i & 3));

    /* we need 9 or more empty positions, one for the padding byte  */
    /* (above) and eight for the length count.  If there is not     */
    /* enough space pad and empty the buffer                        */
    if(i > SHA256_BLOCK_SIZE - 9)
    {
        if(i < 60) ctx->wbuf[15] = 0;
        sha256_compile(ctx);
        i = 0;
    }
    else    /* compute a word index for the empty buffer positions  */
        i = (i >> 2) + 1;

    while(i < 14) /* and zero pad all but last two positions        */
        ctx->wbuf[i++] = 0;

    /* the following 32-bit length fields are assembled in the      */
    /* wrong byte order on little endian machines but this is       */
    /* corrected later since they are only ever used as 32-bit      */
    /* word values.                                                 */
    ctx->wbuf[14] = ctx->count[1];
    ctx->wbuf[15] = ctx->count[0];
    sha256_compile(ctx);

    /* extract the hash value as bytes in case the hash buffer is   */
    /* misaligned for 32-bit words                                  */
    for(i = 0; i < hlen; ++i)
        hval[i] = ((ctx->hash[i >> 2] >> (8 * (~i & 3))) & 0xff);
}

#endif

#if defined(SHA_224)

const uint32_t i224[8] =
{
    0xc1059ed8ul, 0x367cd507ul, 0x3070dd17ul, 0xf70e5939ul,
    0xffc00b31ul, 0x68581511ul, 0x64f98fa7ul, 0xbefa4fa4ul
};

VOID_RETURN sha224_begin(sha224_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha224_ctx));
    memcpy(ctx->hash, i224, sizeof(ctx->hash));
}

VOID_RETURN sha224_end(unsigned char hval[], sha224_ctx ctx[1])
{
    sha_end1(hval, ctx, SHA224_DIGEST_SIZE);
}

VOID_RETURN sha224(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha224_ctx  cx[1];

    sha224_begin(cx);
    sha224_hash(data, len, cx);
    sha_end1(hval, cx, SHA224_DIGEST_SIZE);
}

#endif

#if defined(SHA_256)

const uint32_t i256[8] =
{
    0x6a09e667ul, 0xbb67ae85ul, 0x3c6ef372ul, 0xa54ff53aul,
    0x510e527ful, 0x9b05688cul, 0x1f83d9abul, 0x5be0cd19ul
};

VOID_RETURN sha256_begin(sha256_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha256_ctx));
    memcpy(ctx->hash, i256, sizeof(ctx->hash));
}

VOID_RETURN sha256_end(unsigned char hval[], sha256_ctx ctx[1])
{
    sha_end1(hval, ctx, SHA256_DIGEST_SIZE);
}

VOID_RETURN sha256(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha256_ctx  cx[1];

    sha256_begin(cx);
    sha256_hash(data, len, cx);
    sha_end1(hval, cx, SHA256_DIGEST_SIZE);
}

#endif

#if defined(SHA_384) || defined(SHA_512)

#define SHA512_MASK (SHA512_BLOCK_SIZE - 1)

#define rotr64(x,n)   (((x) >> n) | ((x) << (64 - n)))

#if !defined(bswap_64)
#define bswap_64(x) (((uint64_t)(bswap_32((uint32_t)(x)))) << 32 | bswap_32((uint32_t)((x) >> 32)))
#endif

#if defined(SWAP_BYTES)
#define bsw_64(p,n) \
    { int _i = (n); while(_i--) ((uint64_t*)p)[_i] = bswap_64(((uint64_t*)p)[_i]); }
#else
#define bsw_64(p,n)
#endif

/* SHA512 mixing function definitions   */

#ifdef   s_0
# undef  s_0
# undef  s_1
# undef  g_0
# undef  g_1
# undef  k_0
#endif

#define s_0(x)  (rotr64((x), 28) ^ rotr64((x), 34) ^ rotr64((x), 39))
#define s_1(x)  (rotr64((x), 14) ^ rotr64((x), 18) ^ rotr64((x), 41))
#define g_0(x)  (rotr64((x),  1) ^ rotr64((x),  8) ^ ((x) >>  7))
#define g_1(x)  (rotr64((x), 19) ^ rotr64((x), 61) ^ ((x) >>  6))
#define k_0     k512

/* SHA384/SHA512 mixing data    */

const uint64_t  k512[80] =
{
    li_64(428a2f98d728ae22), li_64(7137449123ef65cd),
    li_64(b5c0fbcfec4d3b2f), li_64(e9b5dba58189dbbc),
    li_64(3956c25bf348b538), li_64(59f111f1b605d019),
    li_64(923f82a4af194f9b), li_64(ab1c5ed5da6d8118),
    li_64(d807aa98a3030242), li_64(12835b0145706fbe),
    li_64(243185be4ee4b28c), li_64(550c7dc3d5ffb4e2),
    li_64(72be5d74f27b896f), li_64(80deb1fe3b1696b1),
    li_64(9bdc06a725c71235), li_64(c19bf174cf692694),
    li_64(e49b69c19ef14ad2), li_64(efbe4786384f25e3),
    li_64(0fc19dc68b8cd5b5), li_64(240ca1cc77ac9c65),
    li_64(2de92c6f592b0275), li_64(4a7484aa6ea6e483),
    li_64(5cb0a9dcbd41fbd4), li_64(76f988da831153b5),
    li_64(983e5152ee66dfab), li_64(a831c66d2db43210),
    li_64(b00327c898fb213f), li_64(bf597fc7beef0ee4),
    li_64(c6e00bf33da88fc2), li_64(d5a79147930aa725),
    li_64(06ca6351e003826f), li_64(142929670a0e6e70),
    li_64(27b70a8546d22ffc), li_64(2e1b21385c26c926),
    li_64(4d2c6dfc5ac42aed), li_64(53380d139d95b3df),
    li_64(650a73548baf63de), li_64(766a0abb3c77b2a8),
    li_64(81c2c92e47edaee6), li_64(92722c851482353b),
    li_64(a2bfe8a14cf10364), li_64(a81a664bbc423001),
    li_64(c24b8b70d0f89791), li_64(c76c51a30654be30),
    li_64(d192e819d6ef5218), li_64(d69906245565a910),
    li_64(f40e35855771202a), li_64(106aa07032bbd1b8),
    li_64(19a4c116b8d2d0c8), li_64(1e376c085141ab53),
    li_64(2748774cdf8eeb99), li_64(34b0bcb5e19b48a8),
    li_64(391c0cb3c5c95a63), li_64(4ed8aa4ae3418acb),
    li_64(5b9cca4f7763e373), li_64(682e6ff3d6b2b8a3),
    li_64(748f82ee5defb2fc), li_64(78a5636f43172f60),
    li_64(84c87814a1f0ab72), li_64(8cc702081a6439ec),
    li_64(90befffa23631e28), li_64(a4506cebde82bde9),
    li_64(bef9a3f7b2c67915), li_64(c67178f2e372532b),
    li_64(ca273eceea26619c), li_64(d186b8c721c0c207),
    li_64(eada7dd6cde0eb1e), li_64(f57d4f7fee6ed178),
    li_64(06f067aa72176fba), li_64(0a637dc5a2c898a6),
    li_64(113f9804bef90dae), li_64(1b710b35131c471b),
    li_64(28db77f523047d84), li_64(32caab7b40c72493),
    li_64(3c9ebe0a15c9bebc), li_64(431d67c49c100d4c),
    li_64(4cc5d4becb3e42b6), li_64(597f299cfc657e2a),
    li_64(5fcb6fab3ad6faec), li_64(6c44198c4a475817)
};

/* Compile 128 bytes of hash data into SHA384/512 digest    */
/* NOTE: this routine assumes that the byte order in the    */
/* ctx->wbuf[] at this point is such that low address bytes */
/* in the ORIGINAL byte stream will go into the high end of */
/* words on BOTH big and little endian systems              */

VOID_RETURN sha512_compile(sha512_ctx ctx[1])
{   uint64_t    v[8], *p = ctx->wbuf;
    uint32_t    j;

    memcpy(v, ctx->hash, sizeof(ctx->hash));

    for(j = 0; j < 80; j += 16)
    {
        v_cycle( 0, j); v_cycle( 1, j);
        v_cycle( 2, j); v_cycle( 3, j);
        v_cycle( 4, j); v_cycle( 5, j);
        v_cycle( 6, j); v_cycle( 7, j);
        v_cycle( 8, j); v_cycle( 9, j);
        v_cycle(10, j); v_cycle(11, j);
        v_cycle(12, j); v_cycle(13, j);
        v_cycle(14, j); v_cycle(15, j);
    }

    ctx->hash[0] += v[0]; ctx->hash[1] += v[1];
    ctx->hash[2] += v[2]; ctx->hash[3] += v[3];
    ctx->hash[4] += v[4]; ctx->hash[5] += v[5];
    ctx->hash[6] += v[6]; ctx->hash[7] += v[7];
}

/* Compile 128 bytes of hash data into SHA256 digest value  */
/* NOTE: this routine assumes that the byte order in the    */
/* ctx->wbuf[] at this point is in such an order that low   */
/* address bytes in the ORIGINAL byte stream placed in this */
/* buffer will now go to the high end of words on BOTH big  */
/* and little endian systems                                */

VOID_RETURN sha512_hash(const unsigned char data[], unsigned long len, sha512_ctx ctx[1])
{   uint32_t pos = (uint32_t)(ctx->count[0] >> 3) & SHA512_MASK;
    const unsigned char *sp = data;
    unsigned char *w = (unsigned char*)ctx->wbuf;
#if SHA2_BITS == 1
    uint32_t ofs = (ctx->count[0] & 7);
#else
    len <<= 3;
#endif

    if((ctx->count[0] += len) < len)
        ++(ctx->count[1]);

#if SHA2_BITS == 1
    if(ofs)                 /* if not on a byte boundary    */
    {
        if(ofs + len < 8)   /* if no added bytes are needed */
        {
            w[pos] |= (*sp >> ofs);
        }
        else                /* otherwise and add bytes      */
        {   unsigned char part = w[pos];

            while((int)(ofs + (len -= 8)) >= 0)
            {
                w[pos++] = part | (*sp >> ofs);
                part = *sp++ << (8 - ofs);
                if(pos == SHA512_BLOCK_SIZE)
                {
                    bsw_64(w, SHA512_BLOCK_SIZE >> 3);
                    sha512_compile(ctx); pos = 0;
                }
            }

            w[pos] = part;
        }
    }
    else    /* data is byte aligned */
#endif
    {   uint32_t space = SHA512_BLOCK_SIZE - pos;

        while(len >= (space << 3))
        {
            memcpy(w + pos, sp, space);
            bsw_64(w, SHA512_BLOCK_SIZE >> 3);
            sha512_compile(ctx); 
            sp += space; len -= (space << 3); 
            space = SHA512_BLOCK_SIZE; pos = 0;
        }
        memcpy(w + pos, sp, (len + 7 * SHA2_BITS) >> 3);
    }
}

/* SHA384/512 Final padding and digest calculation  */

static void sha_end2(unsigned char hval[], sha512_ctx ctx[1], const unsigned int hlen)
{   uint32_t     i = (uint32_t)((ctx->count[0] >> 3) & SHA512_MASK);
    uint64_t     m1;

    /* put bytes in the buffer in an order in which references to   */
    /* 32-bit words will put bytes with lower addresses into the    */
    /* top of 32 bit words on BOTH big and little endian machines   */
    bsw_64(ctx->wbuf, (i + 7 + SHA2_BITS) >> 3);

    /* we now need to mask valid bytes and add the padding which is */
    /* a single 1 bit and as many zero bits as necessary. Note that */
    /* we can always add the first padding byte here because the    */
    /* buffer always has at least one empty slot                    */
    m1 = (unsigned char)0x80 >> (ctx->count[0] & 7);
    ctx->wbuf[i >> 3] &= ((li_64(ffffffffffffff00) | (~m1 + 1)) << 8 * (~i & 7));
    ctx->wbuf[i >> 3] |= (m1 << 8 * (~i & 7));

    /* we need 17 or more empty byte positions, one for the padding */
    /* byte (above) and sixteen for the length count.  If there is  */
    /* not enough space pad and empty the buffer                    */
    if(i > SHA512_BLOCK_SIZE - 17)
    {
        if(i < 120) ctx->wbuf[15] = 0;
        sha512_compile(ctx);
        i = 0;
    }
    else
        i = (i >> 3) + 1;

    while(i < 14)
        ctx->wbuf[i++] = 0;

    /* the following 64-bit length fields are assembled in the      */
    /* wrong byte order on little endian machines but this is       */
    /* corrected later since they are only ever used as 64-bit      */
    /* word values.                                                 */
    ctx->wbuf[14] = ctx->count[1];
    ctx->wbuf[15] = ctx->count[0];
    sha512_compile(ctx);

    /* extract the hash value as bytes in case the hash buffer is   */
    /* misaligned for 32-bit words                                  */
    for(i = 0; i < hlen; ++i)
        hval[i] = ((ctx->hash[i >> 3] >> (8 * (~i & 7))) & 0xff);
}

#endif

#if defined(SHA_384)

/* SHA384 initialisation data   */

const uint64_t  i384[80] =
{
    li_64(cbbb9d5dc1059ed8), li_64(629a292a367cd507),
    li_64(9159015a3070dd17), li_64(152fecd8f70e5939),
    li_64(67332667ffc00b31), li_64(8eb44a8768581511),
    li_64(db0c2e0d64f98fa7), li_64(47b5481dbefa4fa4)
};

VOID_RETURN sha384_begin(sha384_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha384_ctx));
    memcpy(ctx->hash, i384, sizeof(ctx->hash));
}

VOID_RETURN sha384_end(unsigned char hval[], sha384_ctx ctx[1])
{
    sha_end2(hval, ctx, SHA384_DIGEST_SIZE);
}

VOID_RETURN sha384(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha384_ctx  cx[1];

    sha384_begin(cx);
    sha384_hash(data, len, cx);
    sha_end2(hval, cx, SHA384_DIGEST_SIZE);
}

#endif

#if defined(SHA_512)

/* SHA512 initialisation data   */

static const uint64_t i512[SHA512_DIGEST_SIZE >> 3] =
{
    li_64(6a09e667f3bcc908), li_64(bb67ae8584caa73b),
    li_64(3c6ef372fe94f82b), li_64(a54ff53a5f1d36f1),
    li_64(510e527fade682d1), li_64(9b05688c2b3e6c1f),
    li_64(1f83d9abfb41bd6b), li_64(5be0cd19137e2179)
};

/* FIPS PUB 180-4: SHA-512/256 */

static const uint64_t i512_256[SHA512_DIGEST_SIZE >> 3] =
{
    li_64(22312194fc2bf72c), li_64(9f555fa3c84c64c2),
    li_64(2393b86b6f53b151), li_64(963877195940eabd),
    li_64(96283ee2a88effe3), li_64(be5e1e2553863992),
    li_64(2b0199fc2c85b8aa), li_64(0eb72ddc81c52ca2),
};

/* FIPS PUB 180-4: SHA-512/224 */

static const uint64_t i512_224[SHA512_DIGEST_SIZE >> 3] =
{
    li_64(8c3d37c819544da2), li_64(73e1996689dcd4d6),
    li_64(1dfab7ae32ff9c82), li_64(679dd514582f9fcf),
    li_64(0f6d2b697bd44da8), li_64(77e36f7304c48942),
    li_64(3f9d85a86a1d36c8), li_64(1112e6ad91d692a1),
};

/* FIPS PUB 180-4: SHA-512/192 (unsanctioned; facilitates using AES-192) */

static const uint64_t i512_192[SHA512_DIGEST_SIZE >> 3] =
{
    li_64(010176140648b233), li_64(db92aeb1eebadd6f),
    li_64(83a9e27aa1d5ea62), li_64(ec95f77eb609b4e1),
    li_64(71a99185c75caefa), li_64(006e8f08baf32e3c),
    li_64(6a2b21abd2db2aec), li_64(24926cdbd918a27f),
};

/* FIPS PUB 180-4: SHA-512/128 (unsanctioned; facilitates using AES-128) */

static const uint64_t i512_128[SHA512_DIGEST_SIZE >> 3] =
{
    li_64(c953a21464c3e8cc), li_64(06cc9cfd166a34b5),
    li_64(647e88dabf8b24ab), li_64(8513e4dc05a078ac),
    li_64(7266fcfb7cba0534), li_64(854a78e2ecd19b93),
    li_64(8618061711cec2dd), li_64(b20d8506efb929b1),
};

VOID_RETURN sha512_begin(sha512_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha512_ctx));
    memcpy(ctx->hash, i512, sizeof(ctx->hash));
}

VOID_RETURN sha512_256_begin(sha512_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha512_ctx));
    memcpy(ctx->hash, i512_256, sizeof(ctx->hash));
}

VOID_RETURN sha512_224_begin(sha512_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha512_ctx));
    memcpy(ctx->hash, i512_224, sizeof(ctx->hash));
}

VOID_RETURN sha512_192_begin(sha512_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha512_ctx));
    memcpy(ctx->hash, i512_192, sizeof(ctx->hash));
}

VOID_RETURN sha512_128_begin(sha512_ctx ctx[1])
{
    memset(ctx, 0, sizeof(sha512_ctx));
    memcpy(ctx->hash, i512_128, sizeof(ctx->hash));
}

VOID_RETURN sha512_end(unsigned char hval[], sha512_ctx ctx[1])
{
    sha_end2(hval, ctx, SHA512_DIGEST_SIZE);
}

VOID_RETURN sha512_256_end(unsigned char hval[], sha512_ctx ctx[1])
{
    sha_end2(hval, ctx, SHA512_256_DIGEST_SIZE);
}

VOID_RETURN sha512_224_end(unsigned char hval[], sha512_ctx ctx[1])
{
    sha_end2(hval, ctx, SHA512_224_DIGEST_SIZE);
}

VOID_RETURN sha512_192_end(unsigned char hval[], sha512_ctx ctx[1])
{
    sha_end2(hval, ctx, SHA512_192_DIGEST_SIZE);
}

VOID_RETURN sha512_128_end(unsigned char hval[], sha512_ctx ctx[1])
{
    sha_end2(hval, ctx, SHA512_128_DIGEST_SIZE);
}

VOID_RETURN sha512(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha512_ctx  cx[1];

    sha512_begin(cx);
    sha512_hash(data, len, cx);
    sha512_end(hval, cx);
}

VOID_RETURN sha512_256(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha512_ctx  cx[1];

    sha512_256_begin(cx);
    sha512_256_hash(data, len, cx);
    sha512_256_end(hval, cx);
}

VOID_RETURN sha512_224(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha512_ctx  cx[1];

    sha512_224_begin(cx);
    sha512_224_hash(data, len, cx);
    sha512_224_end(hval, cx);
}

VOID_RETURN sha512_192(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha512_ctx  cx[1];

    sha512_192_begin(cx);
    sha512_192_hash(data, len, cx);
    sha512_192_end(hval, cx);
}

VOID_RETURN sha512_128(unsigned char hval[], const unsigned char data[], unsigned long len)
{   sha512_ctx  cx[1];

    sha512_128_begin(cx);
    sha512_128_hash(data, len, cx);
    sha512_128_end(hval, cx);
}

#endif

#if defined(SHA_2)

#define CTX_224(x)  ((x)->uu->ctx256)
#define CTX_256(x)  ((x)->uu->ctx256)
#define CTX_384(x)  ((x)->uu->ctx512)
#define CTX_512(x)  ((x)->uu->ctx512)

/* SHA2 initialisation */

INT_RETURN sha2_begin(unsigned long len, sha2_ctx ctx[1])
{
    switch(len)
    {
#if defined(SHA_224)
        case 224:
        case  28:   CTX_256(ctx)->count[0] = CTX_256(ctx)->count[1] = 0;
                    memcpy(CTX_256(ctx)->hash, i224, 32);
                    ctx->sha2_len = 28; return EXIT_SUCCESS;
#endif
#if defined(SHA_256)
        case 256:
        case  32:   CTX_256(ctx)->count[0] = CTX_256(ctx)->count[1] = 0;
                    memcpy(CTX_256(ctx)->hash, i256, 32);
                    ctx->sha2_len = 32; return EXIT_SUCCESS;
#endif
#if defined(SHA_384)
        case 384:
        case  48:   CTX_384(ctx)->count[0] = CTX_384(ctx)->count[1] = 0;
                    memcpy(CTX_384(ctx)->hash, i384, 64);
                    ctx->sha2_len = 48; return EXIT_SUCCESS;
#endif
#if defined(SHA_512)
        case 512:
        case  64:   CTX_512(ctx)->count[0] = CTX_512(ctx)->count[1] = 0;
                    memcpy(CTX_512(ctx)->hash, i512, 64);
                    ctx->sha2_len = 64; return EXIT_SUCCESS;
#endif
        default:    return EXIT_FAILURE;
    }
}

VOID_RETURN sha2_hash(const unsigned char data[], unsigned long len, sha2_ctx ctx[1])
{
    switch(ctx->sha2_len)
    {
#if defined(SHA_224)
        case 28: sha224_hash(data, len, CTX_224(ctx)); return;
#endif
#if defined(SHA_256)
        case 32: sha256_hash(data, len, CTX_256(ctx)); return;
#endif
#if defined(SHA_384)
        case 48: sha384_hash(data, len, CTX_384(ctx)); return;
#endif
#if defined(SHA_512)
        case 64: sha512_hash(data, len, CTX_512(ctx)); return;
#endif
    }
}

VOID_RETURN sha2_end(unsigned char hval[], sha2_ctx ctx[1])
{
    switch(ctx->sha2_len)
    {
#if defined(SHA_224)
        case 28: sha_end1(hval, CTX_224(ctx), SHA224_DIGEST_SIZE); return;
#endif
#if defined(SHA_256)
        case 32: sha_end1(hval, CTX_256(ctx), SHA256_DIGEST_SIZE); return;
#endif
#if defined(SHA_384)
        case 48: sha_end2(hval, CTX_384(ctx), SHA384_DIGEST_SIZE); return;
#endif
#if defined(SHA_512)
        case 64: sha_end2(hval, CTX_512(ctx), SHA512_DIGEST_SIZE); return;
#endif
    }
}

INT_RETURN sha2(unsigned char hval[], unsigned long size,
                                const unsigned char data[], unsigned long len)
{   sha2_ctx    cx[1];

    if(sha2_begin(size, cx) == EXIT_SUCCESS)
    {
        sha2_hash(data, len, cx); sha2_end(hval, cx); return EXIT_SUCCESS;
    }
    else
        return EXIT_FAILURE;
}

#endif

#if defined(__cplusplus)
}
#endif
