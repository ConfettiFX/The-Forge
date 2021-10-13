#define f4(X) vec4(X)
#define f3(X) vec3(X)
#define f2(X) vec2(X)
#define u4(X) ivec4(X)
#define u3(X) ivec3(X)
#define u2(X) ivec2(X)
#define i4(X) ivec4(X)
#define i3(X) ivec3(X)
#define i2(X) ivec2(X)

#define SV_INSTANCEID gles_InstanceID

#define SHADER_CONSTANT(INDEX, TYPE, NAME, VALUE) const TYPE NAME = VALUE

#define STRUCT(NAME) struct NAME
#define DATA(TYPE, NAME, SEM) TYPE NAME
#define RES(TYPE, NAME, FREQ, REG, BINDING) uniform TYPE NAME
#define RES_IN(TYPE, NAME) attribute TYPE NAME
#define RES_OUT(TYPE, NAME) varying TYPE NAME

#define int2 ivec2
#define int3 ivec3
#define int4 ivec4
#define uint int
#define uint2 ivec2
#define uint3 ivec3
#define uint4 ivec4
#define float2 vec2
#define float3 vec3
#define float4 vec4
#define float2x2 mat2
#define float3x3 mat3
#define float4x4 mat4

#define f2x2 mat2
#define f3x3 mat3
#define f4x4 mat4

#define double2 vec2
#define double3 vec3
#define double4 vec4
#define double2x2 mat2
#define double3x3 mat3
#define double4x4 mat4

#define short4 int4
#define short3 int3
#define short2 int2
#define short  int

#define ushort4 uint4
#define ushort3 uint3
#define ushort2 uint2
#define ushort  uint

#define make_f4x4_row_elems(E00, E01, E02, E03, E10, E11, E12, E13, E20, E21, E22, E23, E30, E31, E32, E33) f4x4(E00, E10, E20, E30, E01, E11, E21, E31, E02, E12, E22, E32, E03, E13, E23, E33)
#define Identity() f4x4(1.0)

#define getCol(M, I) M[I]
#define getCol0(M) getCol(M, 0)
#define getCol1(M) getCol(M, 1)
#define getCol2(M) getCol(M, 2)
#define getCol3(M) getCol(M, 3)

#define lerp    mix
#define round(X) int(X)
#define Tex2D(T) sampler2D
#define SamplerComparisonState highp sampler2DShadow

void setCol(inout f4x4 M, in float4 col, const int i)
{ M[i] = col; }
#define setCol0(M, C) setCol(M, C, 0)
#define setCol1(M, C) setCol(M, C, 1)
#define setCol2(M, C) setCol(M, C, 2)
#define setCol3(M, C) setCol(M, C, 3)

#define SamplerState int
#define NO_SAMPLER 0u

#define SampleTex2D(TEX, SMP, P)            _SampleTex2D((TEX), vec2((P).xy) )
float4 _SampleTex2D(sampler2D TEX, float2 P) { return texture2D(TEX, P); }


#define SampleLvlTex2D(TEX, SMP, P, LOD)  _SampleTex2DLod((TEX), vec2((P).xy), float((LOD)))
// TODO: find a way to get texture LOD with offset in glsl 100
#define SampleLvlOffsetTex2D(TEX, SMP, P, LOD, OFFSET) _SampleTex2DLod((TEX), vec2((P).xy), float((LOD)))
#ifdef STAGE_FRAG
float4 _SampleTex2DLod(sampler2D TEX, float2 P, float LOD) { return texture2D(TEX, P, LOD); }
#endif
#ifdef STAGE_VERT
float4 _SampleTex2DLod(sampler2D TEX, float2 P, float LOD) { return texture2DLod(TEX, P, LOD); }
#endif

#define inout(T) inout T
#define out(T) out T
#define in(T) in T

#define SampleTex2DProj(TEX, SMP, P)       _SampleTex2DProj((TEX), vec4((P).xyzw) )
float4 _SampleTex2DProj(sampler2D TEX, float4 P) { return texture2DProj(TEX, P); }

#ifdef GL_EXT_shadow_samplers 
#define CompareTex2DProj(TEX, SMP, P)       shadow2DProjEXT(TEX, P)
#else
#define CompareTex2DProj(TEX, SMP, P)       shadow2DProj(TEX, P)
#endif

// #define mul(a, b) (a * b)
float3 mul(float3 a, f3x3 b) { return a * b; }
float3 mul(f3x3 a, float3 b) { return a * b; }
float4 mul(f4x4 a, float4 b) { return a * b; }
float4 mul(float4 a, f4x4 b) { return a * b; }
f4x4 mul(f4x4 a, f4x4 b) { return a * b; }
float3 mul(float3 a, float b) { return a * b; }

#define DECLARE_RESOURCES()
#define STATIC
#define INLINE

#define UNROLL_N(X)
#define UNROLL
#define LOOP
#define FLATTEN

float saturate(float VALUE) { return clamp(VALUE, 0.0, 1.0); }
vec2 saturate(vec2 VALUE) { return clamp(VALUE, 0.0, 1.0); }
vec3 saturate(vec3 VALUE) { return clamp(VALUE, 0.0, 1.0); }
vec4 saturate(vec4 VALUE) { return clamp(VALUE, 0.0, 1.0); }

#define sincos(angle, s, c) { (s) = sin(angle); (c) = cos(angle); }
#define rcp(VALUE)			(1.0 / (VALUE))
#define frac(VALUE)			fract(VALUE)

#define ddx(VALUE)			VALUE // dFdx(VALUE) TODO: required extension not requested yet: GL_OES_standard_derivatives
#define ddy(VALUE)			VALUE // dFdy(VALUE) TODO: required extension not requested yet: GL_OES_standard_derivatives
#define isnan(VALUE)        false //isnan(x) available from glsl > v130

// matching hlsl semantics, glsl mod preserves sign(Y)
#define fmod(X, Y)           (abs(mod(X, Y))*sign(X))

#ifndef STAGE_VERT
    #define VR_VIEW_ID(VID) (0)
#else
    #define VR_VIEW_ID 0
#endif
#define VR_MULTIVIEW_COUNT 1