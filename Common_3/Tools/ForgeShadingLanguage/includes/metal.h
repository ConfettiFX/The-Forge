#ifndef _METAL_H
#define _METAL_H

#include <metal_stdlib>
using namespace metal;

#define f4(X) float4(X,X,X,X)
#define f3(X) float3(X,X,X)
#define f2(X) float2(X,X)
#define u4(X) uint4(X,X,X,X)
#define u3(X) uint3(X,X,X)
#define u2(X) uint2(X,X)
#define i4(X) int4(X,X,X,X)
#define i3(X) int3(X,X,X)
#define i2(X) int2(X,X)
#define h4(X) half4(X,X,X,X)
#define h3(X) half3(X,X,X)
#define h2(X) half2(X,X)

#define min16float  half
#define min16float2 half2
#define min16float3 half3
#define min16float4 half4

#define fast_min fast::min
#define fast_max fast::max

float length(int2 x)
{ return length(float2(x)); }

#define SV_VERTEXID         [[vertex_id]]
#define SV_INSTANCEID       [[instance_id]]
#define SV_ISFRONTFACE      [[front_facing]]
#define SV_GROUPID          [[threadgroup_position_in_grid]]
#define SV_DISPATCHTHREADID [[thread_position_in_grid]]
#define SV_GROUPTHREADID    [[thread_position_in_threadgroup]]
#define SV_GROUPINDEX       [[thread_index_in_threadgroup]]
#define SV_SAMPLEINDEX      [[sample_id]]
#define SV_PRIMITIVEID      [[primitive_id]]
#define SV_DOMAINLOCATION   [[position_in_patch]]
#define SV_SHADINGRATE      [[_ERROR_NOT_IMPLEMENTED]]

#define UPDATE_FREQ_NONE      0
#define UPDATE_FREQ_PER_FRAME 1
#define UPDATE_FREQ_PER_BATCH 2
#define UPDATE_FREQ_PER_DRAW  3
#define UPDATE_FREQ_USER      4
#define MAX_BUFFER_BINDINGS  31

#define STATIC constant
#define INLINE inline

#define FLAT(X) X

// #define SHADER_VIS_VS   1
// #define SHADER_VIS_TC   2
// #define SHADER_VIS_TE   4
// #define SHADER_VIS_GS   8
// #define SHADER_VIS_PS   16
// #define SHADER_VIS_ALL SHADER_VIS_VS | SHADER_VIS_TC | SHADER_VIS_TE | SHADER_VIS_GS | SHADER_VIS_PS
// #define SHADER_VIS_CS SHADER_VIS_ALL

// global resource access
#define Get(X) _Get_##X

#define ddx dfdx
#define ddy dfdy

#define SV_Depth float

bool2 And( bool2 a, bool2 b)
{ return a && b; }

#define GreaterThan(A, B)      ((A) > (B))
#define GreaterThanEqual(A, B) ((A) >= (B))
#define LessThan(A, B)         ((A) < (B))
#define LessThanEqual(A, B)    ((A) <= (B))

#define AllGreaterThan(X, Y)      all(GreaterThan(X, Y))
#define AllGreaterThanEqual(X, Y) all(GreaterThanEqual(X, Y))
#define AllLessThan(X, Y)         all(LessThan(X, Y))
#define AllLessThanEqual(X, Y)    all(LessThanEqual(X, Y))

#define AnyGreaterThan(X, Y)      any(GreaterThan(X, Y))
#define AnyGreaterThanEqual(X, Y) any(GreaterThanEqual(X, Y))
#define AnyLessThan(X, Y)         any(LessThan(X, Y))
#define AnyLessThanEqual(X, Y)    any(LessThanEqual(X, Y))

bool3 Equal(float3 X, float Y) { return X == f3(Y);}

bool allGreaterThan(const float4 a, const float4 b)
{ return all(a > b); }

#define GroupMemoryBarrier() threadgroup_barrier(mem_flags::mem_threadgroup)
#define AllMemoryBarrier() threadgroup_barrier(mem_flags::mem_device | mem_flags::mem_threadgroup)

float degrees(float radians)
{ return (radians * 3.14159265359 / 180.0); }

float radians(float degrees)
{ return (degrees * 180.0 / 3.14159265359); }


#define rcp(X) (1.0 / (X))

// parameter qualifiers, map 'in T x' to 'const thread T& x', 'out/inout T x' to 'thread T& x'
// for shared variables, 'shared(T) x' get mapped to 'threadgroup T& x'

#define inout_array(T, X) thread T (&X)
#define out_array inout_array

#define groupshared(T) threadgroup T&

#define inout(T) thread T&
#define out inout
#define in(T)    thread T

#define UNROLL_N(X)
#define UNROLL
#define FLATTEN
#define LOOP

#define sincos(angle, s, c) ((s) = sincos(angle, c))

#define STRUCT(NAME) struct NAME
#define DATA(TYPE, NAME, SEM) TYPE NAME
#define CBUFFER(NAME, REG, FREQ, BINDING) struct NAME
#define PUSH_CONSTANT(NAME, REG) STRUCT(NAME)

/* Matrix */

// float3x3 f3x3(float4x4 m)
// { return float3x3(m[0].xyz, m[1].xyz, m[2].xyz); }

#define to_f3x3(M) float3x3(M[0].xyz, M[1].xyz, M[2].xyz)

#define f2x2 float2x2
#define f2x3 float2x3
#define f2x4 float2x4
#define f3x2 float3x2
#define f3x3 float3x3
#define f3x4 float3x4
#define f4x2 float4x2
#define f4x3 float4x3
#define f4x4 float4x4

#define make_f2x2_cols(C0, C1) f2x2(C0, C1)
#define make_f2x2_rows(R0, R1) transpose(f2x2(R0, R1))
#define make_f2x2_col_elems f2x2
#define make_f2x3_cols(C0, C1) f2x3(C0, C1)
#define make_f2x3_rows(R0, R1, R2) transpose(f3x2(R0, R1, R2))
#define make_f2x3_col_elems f2x3
#define make_f2x4_cols(C0, C1) f2x4(C0, C1)
#define make_f2x4_rows(R0, R1, R2, R3) transpose(f4x2(R0, R1, R2, R3))
#define make_f2x4_col_elems f2x4

#define make_f3x2_cols(C0, C1, C2) f3x2(C0, C1, C2)
#define make_f3x2_rows(R0, R1) transpose(f2x3(R0, R1))
#define make_f3x2_col_elems f3x2
#define make_f3x3_cols(C0, C1, C2) f3x3(C0, C1,C2)
#define make_f3x3_rows(R0, R1, R2) transpose(f3x3(R0, R1, R2))
#define make_f3x3_col_elems f3x3
#define make_f3x3_row_elems(E00, E01, E02, E10, E11, E12, E20, E21, E22) \
    f3x3(E00, E10, E20, E01, E11, E21, E02, E12, E22)
#define make_f3x4_cols(C0, C1, C2) f3x4(C0, C1,C2)
#define make_f3x4_rows(R0, R1, R2, R3) transpose(f4x3(R0, R1, R2, R3))
#define make_f3x4_col_elems f3x4

#define make_f4x2_cols(C0, C1, C2, C3) f4x2(C0, C1, C2, C3)
#define make_f4x2_rows(R0, R1) transpose(f2x4(R0, R1))
#define make_f4x2_col_elems f4x2
#define make_f4x3_cols(C0, C1, C2, C3) f4x3(C0, C1, C2, C3)
#define make_f4x3_rows(R0, R1, R2) transpose(f3x4(R0, R1, R2))
#define make_f4x3_col_elems f4x3
#define make_f4x4_cols(C0, C1, C2, C3) f4x4(C0, C1, C2, C3)
#define make_f4x4_rows(R0, R1, R2, R3) transpose(f4x4(R0, R1, R2, R3))
#define make_f4x4_col_elems f4x4
#define make_f4x4_row_elems(E00, E01, E02, E03, E10, E11, E12, E13, E20, E21, E22, E23, E30, E31, E32, E33) \
f4x4(E00, E10, E20, E30, E01, E11, E21, E31, E02, E12, E22, E32, E03, E13, E23, E33)

inline f4x4 Identity() { return f4x4(1.0f); }

#define setElem(M, I, J, V) {M[I][J] = V;}
#define getElem(M, I, J) (M[I][J])


#define getCol(M, I) M[I]
#define getCol0(M) getCol(M, 0)
#define getCol1(M) getCol(M, 1)
#define getCol2(M) getCol(M, 2)
#define getCol3(M) getCol(M, 3)

inline float4 getRow(const thread float4x4& M, const uint i) { return float4(M[0][i], M[1][i], M[2][i], M[3][i]); }
inline float4 getRow(const thread float4x3& M, const uint i) { return float4(M[0][i], M[1][i], M[2][i], M[3][i]); }
inline float4 getRow(const thread float4x2& M, const uint i) { return float4(M[0][i], M[1][i], M[2][i], M[3][i]); }

inline float3 getRow(const thread float3x4& M, const uint i) { return float3(M[0][i], M[1][i], M[2][i]); }
inline float3 getRow(const thread float3x3& M, const uint i) { return float3(M[0][i], M[1][i], M[2][i]); }
inline float3 getRow(const thread float3x2& M, const uint i) { return float3(M[0][i], M[1][i], M[2][i]); }

inline float2 getRow(const thread float2x4& M, const uint i) { return float2(M[0][i], M[1][i]); }
inline float2 getRow(const thread float2x3& M, const uint i) { return float2(M[0][i], M[1][i]); }
inline float2 getRow(const thread float2x2& M, const uint i) { return float2(M[0][i], M[1][i]); }

#define getRow0(M) getRow(M, 0)
#define getRow1(M) getRow(M, 1)
#define getRow2(M) getRow(M, 2)
#define getRow3(M) getRow(M, 3)


inline float4x4 setCol(inout(float4x4) M, in(float4) col, const uint i) { M[i] = col; return M; }
inline float4x3 setCol(inout(float4x3) M, in(float3) col, const uint i) { M[i] = col; return M; }
inline float4x2 setCol(inout(float4x2) M, in(float2) col, const uint i) { M[i] = col; return M; }

inline float3x4 setCol(inout(float3x4) M, in(float4) col, const uint i) { M[i] = col; return M; }
inline float3x3 setCol(inout(float3x3) M, in(float3) col, const uint i) { M[i] = col; return M; }
inline float3x2 setCol(inout(float3x2) M, in(float2) col, const uint i) { M[i] = col; return M; }

inline float2x4 setCol(inout(float2x4) M, in(float4) col, const uint i) { M[i] = col; return M; }
inline float2x3 setCol(inout(float2x3) M, in(float3) col, const uint i) { M[i] = col; return M; }
inline float2x2 setCol(inout(float2x2) M, in(float2) col, const uint i) { M[i] = col; return M; }

#define setCol0(M, C) setCol(M, C, 0)
#define setCol1(M, C) setCol(M, C, 1)
#define setCol2(M, C) setCol(M, C, 2)
#define setCol3(M, C) setCol(M, C, 3)


inline float4x4 setRow(inout(float4x4) M, in(float4) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; M[3][i] = row[3]; return M; }
inline float4x3 setRow(inout(float4x3) M, in(float4) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; M[3][i] = row[3]; return M; }
inline float4x2 setRow(inout(float4x2) M, in(float4) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; M[3][i] = row[3]; return M; }

inline float3x4 setRow(inout(float3x4) M, in(float3) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; return M; }
inline float3x3 setRow(inout(float3x3) M, in(float3) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; return M; }
inline float3x2 setRow(inout(float3x2) M, in(float3) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; M[2][i] = row[2]; return M; }

inline float2x4 setRow(inout(float2x4) M, in(float2) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; return M; }
inline float2x3 setRow(inout(float2x3) M, in(float2) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; return M; }
inline float2x2 setRow(inout(float2x2) M, in(float2) row, const uint i) { M[0][i] = row[0]; M[1][i] = row[1]; return M; }

#define setRow0(M, R) setRow(M, R, 0)
#define setRow1(M, R) setRow(M, R, 1)
#define setRow2(M, R) setRow(M, R, 2)
#define setRow3(M, R) setRow(M, R, 3)


void AtomicAdd(threadgroup atomic_uint& DEST, uint VALUE, thread uint& ORIGINAL_VALUE)
{ ORIGINAL_VALUE = atomic_fetch_add_explicit(&(DEST), VALUE, memory_order_relaxed); }

void AtomicAdd(threadgroup uint& DEST, uint VALUE, thread uint& ORIGINAL_VALUE)
{ ORIGINAL_VALUE = atomic_fetch_add_explicit((threadgroup atomic_uint*)&(DEST), VALUE, memory_order_relaxed); }

void AtomicAdd(volatile device uint& DEST, uint VALUE, thread uint& ORIGINAL_VALUE)
{ ORIGINAL_VALUE = atomic_fetch_add_explicit((volatile device atomic_uint*)&(DEST), VALUE, memory_order_relaxed); }

void AtomicAdd(device atomic_uint& DEST, uint VALUE, threadgroup uint& ORIGINAL_VALUE)
{ ORIGINAL_VALUE = atomic_fetch_add_explicit(&(DEST), VALUE, memory_order_relaxed); }

void AtomicAdd(device atomic_uint& DEST, uint VALUE, thread uint& ORIGINAL_VALUE)
{ ORIGINAL_VALUE = atomic_fetch_add_explicit(&(DEST), VALUE, memory_order_relaxed); }

void AtomicAdd(device uint& DEST, threadgroup atomic_uint& VALUE, threadgroup atomic_uint& ORIGINAL_VALUE)
{ 
    uint index = atomic_load_explicit(&VALUE, memory_order_relaxed);
    atomic_store_explicit(&ORIGINAL_VALUE, 
        atomic_fetch_add_explicit((device atomic_uint*)&(DEST), index, memory_order_relaxed),
        memory_order_relaxed);
}

#define AtomicStore(DEST, VALUE) \
    atomic_store_explicit(&DEST, VALUE, memory_order_relaxed)
#define AtomicLoad(SRC) \
    atomic_load_explicit(&SRC, memory_order_relaxed)
#define AtomicExchange(DEST, VALUE, ORIGINAL_VALUE) \
    ORIGINAL_VALUE = atomic_exchange_explicit(&DEST, VALUE, memory_order_relaxed)

#define AtomicMin3D( DST, COORD, VALUE, ORIGINAL_VALUE ) \
{ (ORIGINAL_VALUE) = atomic_fetch_min_explicit( ((device atomic_uint*)&(DST)[uint(COORD)]) , (VALUE), memory_order_relaxed ); }

inline void AtomicMax( device uint& DST, uint VAL)
{
    atomic_fetch_max_explicit((device atomic_uint*)&DST, VAL, memory_order_relaxed);
}
inline void AtomicMin( device uint& DST, uint VAL)
{
    atomic_fetch_min_explicit((device atomic_uint*)&DST, VAL, memory_order_relaxed);
}

// #define AtomicMax(DST, VAL) \
// atomic_fetch_max_explicit((device atomic_uint*)&fsData.IntermediateBuffer[index], UINT_MAX, memory_order_relaxed);

#define clip(COND) if( (COND) < 0 )  discard_fragment()

#define NonUniformResourceIndex(X) (X)
#define BeginNonUniformResourceIndex(X, Y)
// #define BeginNonUniformResourceIndex(X)
#define EndNonUniformResourceIndex()

#define frac fract
#define lerp mix
#define mul(X, Y) ((X) * (Y))
#define discard discard_fragment()

#define SampleLvlTexCube(NAME, SAMPLER, COORD, LEVEL) NAME.sample(SAMPLER, COORD, level(LEVEL))
#define SampleLvlTex3D(NAME, SAMPLER, COORD, LEVEL) NAME.sample(SAMPLER, COORD, level(LEVEL))

#define SampleTexCube(NAME, SAMPLER, COORD) (NAME).sample( (SAMPLER), (COORD) )

#define SamplerState sampler
#define SamplerComparisonState sampler
#define SampleTex1D(NAME, SAMPLER, COORD) NAME.sample(SAMPLER, COORD)
#define SampleTex2D SampleTex1D
#define SampleTex3D SampleTex1D

#define LoadRWTex2D(TEX, P) (TEX).read(uint2((P).xy))
#define LoadRWTex3D(TEX, P) (TEX).read(uint3((P).xyz))

#define LoadTex2D(NAME, SAMPLER, COORD, L) NAME.read( uint2((COORD).xy), uint(L) )
// #define LoadLvlTex2D(TEX, SMP, P, L) (TEX).read(ushort2((P).xy), L)
// #define LoadTex2DArray(NAME, SAMPLER, COORD) NAME.read( (COORD).xy, (COORD).z, 0 )
// #define LoadTex3D(NAME, SAMPLER, COORD, L) NAME.read( uint3((COORD).xyz), uint(L) )
#define LoadTex3D(TEX, SMP, P, LOD) _LoadTex3D(TEX, uint3(P), uint(LOD))

template <typename T, metal::access A>
vec<T, 4> _LoadTex3D(texture2d_array<T, A> tex, uint3 p, uint lod)
{ return tex.read(p, lod); }

template <typename T, metal::access A>
vec<T, 4> _LoadTex3D(texture3d<T, A> tex, uint3 p, uint lod)
{ return tex.read(p, lod); }

#define LoadLvlOffsetTex2D(TEX, SMP, P, L, O) (TEX).read( ushort2((P).xy + (O).xy), L )

#define Load2D(TEX, P) (TEX).read(uint2(P))

#define SampleGradTex2D(NAME, SAMPLER, COORD, DX, DY) (NAME).sample( (SAMPLER), (COORD), gradient2d((DX), (DY)) )

#define SampleLvlOffsetTex2D(TEX, SMP, P, L, O) (TEX).sample( (SMP), (P), level(L), (O) )
#define SampleLvlOffsetTex3D(TEX, SMP, P, L, O) (TEX).sample( (SMP), (P), level(L), (O) )

#define CompareTex2D(TEX, SMP, PC) (TEX).sample_compare(SMP, (PC).xy, (PC).z, level(0))
#define CompareTex2DProj(TEX, SMP, PC) (TEX).sample_compare(SMP, (PC).xy/(PC).w, (PC).z/(PC).w, level(0))

// #define GatherRedTex2D(NAME, SAMPLER, COORD) textureGather(sampler2D( (NAME), (SAMPLER) ), (COORD) ).rrrr
#define GatherRedTex2D(NAME, SAMPLER, COORD) (NAME).gather( (SAMPLER), (COORD), int2(0) ).rrrr
// Tex.gather(TexSampler, f2TexCoord, int2(0));

// #define BUFFER(NAME) NAME

#define METAL_half   half
#define METAL_float  float
#define METAL_float2 float
#define METAL_float3 float
#define METAL_float4 float
#define METAL_uint   uint
#define METAL_uint2  uint
#define METAL_uint3  uint
#define METAL_uint4  uint
#define METAL_int    int
#define METAL_int2   int
#define METAL_int3   int
#define METAL_int4   int
#define METAL_T(ELEM_TYPE) METAL_##ELEM_TYPE

template<typename T, metal::access A>
int2 GetDimensions(const texture2d<T, A> t, sampler s)
    { return int2(t.get_width(), t.get_height()); }
template<typename T, metal::access A>
int2 GetDimensions(const texturecube<T, A> t, sampler s)
    { return int2(t.get_width(), t.get_height()); }

template<typename T, metal::access A>
int2 GetDimensions(const texture2d<T, A> t, uint _NO_SAMPLER)
    { return int2(t.get_width(), t.get_height()); }
template<typename T, metal::access A>
int2 GetDimensions(const texturecube<T, A> t, uint _NO_SAMPLER)
    { return int2(t.get_width(), t.get_height()); }
	
#define GetDimensionsMS(tex, dim) int2 dim = int2(tex.get_width(), tex.get_height());
	
#define NO_SAMPLER 0u

#define Buffer(T) T
#define RWBuffer(T) T
#define WBuffer(T) T
#define RWCoherentBuffer(T) volatile T

#define ByteBuffer uint
#define RWByteBuffer uint
#define LoadByte(BYTE_BUFFER, ADDRESS) ((BYTE_BUFFER)[(ADDRESS)>>2])
#define LoadByte4(BYTE_BUFFER, ADDRESS) uint4( \
    BYTE_BUFFER[((ADDRESS)>>2)+0], \
    BYTE_BUFFER[((ADDRESS)>>2)+1], \
    BYTE_BUFFER[((ADDRESS)>>2)+2], \
    BYTE_BUFFER[((ADDRESS)>>2)+3])

#define StoreByte(BYTE_BUFFER, ADDRESS, VALUE) \
    (BYTE_BUFFER)[((ADDRESS)>>2)+0] = VALUE;

// #define asfloat(X) as_type<float>(X)
inline float4 asfloat(uint4 X) { return as_type<float4>(X); }
inline float asfloat(uint X) { return as_type<float>(X); }
inline uint asuint(float X) { return as_type<uint>(X); }
inline uint2 asuint(float2 X) { return as_type<uint2>(X); }
inline uint3 asuint(float3 X) { return as_type<uint3>(X); }
inline uint4 asuint(float4 X) { return as_type<uint4>(X); }

#define TexCube(ELEM_TYPE) texturecube<METAL_T(ELEM_TYPE), access::sample>

#define RWTex1D(ELEM_TYPE) texture1d<METAL_T(ELEM_TYPE), access::read_write>
#define RWTex2D(ELEM_TYPE) texture2d<METAL_T(ELEM_TYPE), access::read_write>
#define RWTex3D(ELEM_TYPE) texture3d<METAL_T(ELEM_TYPE), access::read_write>

#define RWTex1DArray(ELEM_TYPE) texture1d_array<METAL_T(ELEM_TYPE), access::read_write>
#define RWTex2DArray(ELEM_TYPE) texture2d_array<METAL_T(ELEM_TYPE), access::read_write>
#define RWTex3DArray(ELEM_TYPE) texture3d_array<METAL_T(ELEM_TYPE), access::read_write>

#define WTex1D(ELEM_TYPE) texture1d<METAL_T(ELEM_TYPE), access::write>
#define WTex2D(ELEM_TYPE) texture2d<METAL_T(ELEM_TYPE), access::write>
#define WTex3D(ELEM_TYPE) texture3d<METAL_T(ELEM_TYPE), access::write>

#define WTex1DArray(ELEM_TYPE) texture1d_array<METAL_T(ELEM_TYPE), access::write>
#define WTex2DArray(ELEM_TYPE) texture2d_array<METAL_T(ELEM_TYPE), access::write>
#define WTex3DArray(ELEM_TYPE) texture3d_array<METAL_T(ELEM_TYPE), access::write>

#define RTex1D(ELEM_TYPE) texture1d<METAL_T(ELEM_TYPE), access::read>
#define RTex2D(ELEM_TYPE) texture2d<METAL_T(ELEM_TYPE), access::read>
#define RTex3D(ELEM_TYPE) texture3d<METAL_T(ELEM_TYPE), access::read>

#define RTex1DArray(ELEM_TYPE) texture1d_array<METAL_T(ELEM_TYPE), access::read>
#define RTex2DArray(ELEM_TYPE) texture2d_array<METAL_T(ELEM_TYPE), access::read>
#define RTex3DArray(ELEM_TYPE) texture3d_array<METAL_T(ELEM_TYPE), access::read>

#define Tex1D(ELEM_TYPE) texture1d<METAL_T(ELEM_TYPE), access::sample>
#define Tex2D(ELEM_TYPE) texture2d<METAL_T(ELEM_TYPE), access::sample>
#define Tex3D(ELEM_TYPE) texture3d<METAL_T(ELEM_TYPE), access::sample>

#define Tex1DMS(ELEM_TYPE, SMP_CNT) texture1d_ms<METAL_T(ELEM_TYPE), access::read>
#define Tex2DMS(ELEM_TYPE, SMP_CNT) texture2d_ms<METAL_T(ELEM_TYPE), access::read>
#define Tex3DMS(ELEM_TYPE, SMP_CNT) texture3d_ms<METAL_T(ELEM_TYPE), access::read>

#define Tex1DArray(ELEM_TYPE) texture1d_array<METAL_T(ELEM_TYPE), access::sample>
#define Tex2DArray(ELEM_TYPE) texture2d_array<METAL_T(ELEM_TYPE), access::sample>
#define Tex3DArray(ELEM_TYPE) texture3d_array<METAL_T(ELEM_TYPE), access::sample>

#define Depth2D(ELEM_TYPE) depth2d<METAL_T(ELEM_TYPE), access::sample>
#define Depth2DMS(ELEM_TYPE, SMP_CNT) depth2d_ms<METAL_T(ELEM_TYPE), access::read>

#define RasterizerOrderedTex2D RWTex2D

#define SampleTex2DArray(NAME, SAMPLER, COORD) NAME.sample(SAMPLER, COORD.xy, uint(COORD.z))

float4 _to4(float4 x) { return x; }
float4 _to4(float3 x) { return float4(x, 0); }
float4 _to4(float2 x) { return float4(x, 0, 0); }
float4 _to4(float  x) { return float4(x, 0, 0, 0); }
half4  _to4(half4 x)  { return x; }
half4  _to4(half3 x)  { return half4(x, 0); }
half4  _to4(half2 x)  { return half4(x, 0, 0); }
half4  _to4(half  x)  { return half4(x, 0, 0, 0); }
uint4  _to4(uint4  x) { return x; }
uint4  _to4(uint3  x) { return uint4(x, 0); }
uint4  _to4(uint2  x) { return uint4(x, 0, 0); }
uint4  _to4(uint   x) { return uint4(x, 0, 0, 0); }
int4   _to4(int4   x) { return x; }
int4   _to4(int3   x) { return int4(x, 0); }
int4   _to4(int2   x) { return int4(x, 0, 0); }
int4   _to4(int    x) { return int4(x, 0, 0, 0); }


#define LoadTex2DMS(NAME, SAMPLER, COORD, SMP) _to4(NAME.read( (COORD).xy, SMP ))
#define LoadTex2DArrayMS(NAME, SAMPLER, COORD, SMP) _to4(NAME.read( (COORD).xyz, SMP ))
#define SampleLvlTex2D(NAME, SAMPLER, COORD, LEVEL) _to4(NAME.sample(SAMPLER, COORD, level(LEVEL)))
#define SampleTex2DProj(NAME, SAMPLER, COORD) _to4(NAME.sample(SAMPLER, float4(COORD).xy / float4(COORD).w))

#define Write2D(NAME, COORD, VAL) NAME.write(_to4(VAL), uint2(COORD.xy))
// #define Write3D(NAME, P, VAL) NAME.write(_to4(VAL), uint3((P).xyz))
// #define Write2DArray(TEX, P, I, VAL) (TEX).write(_to4(VAL), uint2((P).xy), uint(I))
// #define Write3D(NAME, COORD, VAL) NAME.write(_to4(VAL), uint2(COORD.xy), COORD.z)

#define Write3D(TEX, P, V) _Write3D(TEX, uint3(P), _to4(V))

template <typename T, metal::access A>
void _Write3D(texture2d<T, A> tex, uint3 p, vec<T, 4> v)
{ tex.write(v, p); }

template <typename T, metal::access A>
void _Write3D(texture2d_array<T, A> tex, uint3 p, vec<T, 4> v)
{ tex.write(v, uint2(p.xy), uint(p.z)); }

template <typename T, metal::access A>
void _Write3D(texture3d<T, A> tex, uint3 p, vec<T, 4> v)
{ tex.write(v, p); }

// texture1d<METAL_T(ELEM_TYPE), access::read_write>


// void Write3D( RWTex3D(half) TEX, uint3 P, half4 V )
// { TEX.write(_to4(V), P); }
// void Write3D( RWTex2DArray(float) TEX, uint3 P, float4 V )
// { TEX.write(_to4(V), P.xy, P.z); }

#define SHADER_CONSTANT(INDEX, TYPE, NAME, VALUE) constant TYPE NAME [[function_constant(INDEX)]]

#define FSL_CONST(TYPE, NAME) const TYPE NAME

bool any(float2 x) { return any(x!= 0.0f); }
bool any(float3 x) { return any(x!= 0.0f); }

#ifdef ENABLE_WAVEOPS
    #include <metal_simdgroup>
    #include <metal_quadgroup>
    #define ballot_t uint4
    #define WaveGetLaneIndex() simd_lane_id
    #define WaveIsFirstLane simd_is_first
    #define WaveGetMaxActiveIndex() simd_max(WaveGetLaneIndex())
    #define countbits popcount
    #define CountBallot _popcount_u4
    inline ballot_t WaveActiveBallot(bool expr)
    {
        simd_vote activeLaneMask = simd_ballot(expr);
        // simd_ballot() returns a 64-bit integer-like object, but
        // SPIR-V callers expect a uint4. We must convert.
        // FIXME: This won't include higher bits if Apple ever supports
        // 128 lanes in an SIMD-group.
        return uint4(
            (uint)(((simd_vote::vote_t)activeLaneMask)       & 0xFFFFFFFF),
            (uint)(((simd_vote::vote_t)activeLaneMask >> 32) & 0xFFFFFFFF),
            uint2(0));
    }
    #define WaveReadLaneFirst simd_broadcast_first
    #define WaveActiveSum simd_sum
    #define WavePrefixSum simd_prefix_exclusive_sum
    #define QuadReadAcrossX(X) quad_shuffle(X, (WaveGetLaneIndex() % 4u + 1) % 4)
    #define QuadReadAcrossY(X) quad_shuffle(X, (WaveGetLaneIndex() % 4u + 2) % 4)
    #define WaveReadLaneAt(X, Y) simd_shuffle(X, Y)
    #define WaveActiveAnyTrue simd_any

    inline uint _popcount_u4(uint4 values)
    {
        const uint4 counts = popcount(values);
        return counts.x + counts.y + counts.z + counts.w;
    }

    // simd_lane_id is made available inside MetalShader by fsl using ENABLE_WAVEOPS()
    #define WavePrefixCountBits(EXPR) _WavePrefixCountBits(EXPR, simd_lane_id)

    inline uint _WavePrefixCountBits(bool expr, uint simd_lane_id)
    {
        const uint4 ballot = WaveActiveBallot(expr);
        uint4 mask = uint4(
            extract_bits(0xFFFFFFFF, 0, min(simd_lane_id, 32u)),
            extract_bits(0xFFFFFFFF, 0, (uint)max((int)simd_lane_id - 32, 0)),
            uint2(0));
        return _popcount_u4(ballot & mask);
    }

    #define WaveActiveCountBits(EXPR) _popcount_u4(WaveActiveBallot(EXPR))

#endif

#define EnablePSInterlock()
#define BeginPSInterlock()
#define EndPSInterlock()

// tessellation
// #define SV_PrimitiveID(N) N
#define INPUT_PATCH(T, NC) const thread T*
#define TESS_VS_SHADER(X)
#define PATCH_CONSTANT_FUNC(X)
#define TESS_LAYOUT(X, Y, Z)
#define OUTPUT_CONTROL_POINTS(X)
#define MAX_TESS_FACTOR(X)

#define DECLARE_RESOURCES()
#define INDIRECT_DRAW()
#define SET_OUTPUT_FORMAT(FMT)
#define PS_ZORDER_EARLYZ()

#ifndef STAGE_VERT
    #define VR_VIEW_ID(VID) (0)
#else
    #define VR_VIEW_ID 0
#endif
#define VR_MULTIVIEW_COUNT 1

#if __METAL_VERSION__ >= 210
// Code that requires features introduced in Metal 2.1.
// #TODO - Define this
// #define INDIRECT_COMMAND_BUFFER 1

#define COMMAND_BUFFER command_buffer
#define PRIMITIVE_TYPE primitive_type
#define PRIMITIVE_TYPE_POINT PRIMITIVE_TYPE::point
#define PRIMITIVE_TYPE_LINE PRIMITIVE_TYPE::line
#define PRIMITIVE_TYPE_LINE_STRIP PRIMITIVE_TYPE::line_strip
#define PRIMITIVE_TYPE_TRIANGLE PRIMITIVE_TYPE::triangle
#define PRIMITIVE_TYPE_TRIANGLE_STRIP PRIMITIVE_TYPE::triangle_strip

void cmdDrawInstanced(command_buffer cmdbuf, uint slot, PRIMITIVE_TYPE prim, uint vertexCount, uint firstVertex, uint instanceCount, uint firstInstance)
{
	render_command cmd(cmdbuf, slot);
	cmd.draw_primitives(prim, firstVertex, vertexCount, instanceCount, firstInstance);
}

void cmdDraw(command_buffer cmdbuf, uint slot, PRIMITIVE_TYPE prim, uint vertexCount, uint firstVertex, uint instanceCount, uint firstInstance)
{
	cmdDrawInstanced(cmdbuf, slot, prim, vertexCount, firstVertex, 1, 0);
}

template<typename T>
void cmdDrawIndexedInstanced(command_buffer cmdbuf, uint slot, PRIMITIVE_TYPE prim, T indexBuffer, uint indexCount, uint firstIndex, uint firstVertex, uint instanceCount, uint firstInstance)
{
	render_command cmd(cmdbuf, slot);
	cmd.draw_indexed_primitives(prim, indexCount, indexBuffer + firstIndex, instanceCount, firstVertex, firstInstance);
}

template<typename T>
void cmdDrawIndexed(command_buffer cmdbuf, uint slot, PRIMITIVE_TYPE prim, T indexBuffer, uint indexCount, uint firstIndex, uint firstVertex)
{
	cmdDrawIndexedInstanced(cmdbuf, slot, prim, indexBuffer, indexCount, firstIndex, firstVertex, 1, 0);
}

#endif

#endif // _METAL_H
