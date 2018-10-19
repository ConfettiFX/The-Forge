//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) 2017 Guillaume Blanc                                         //
//                                                                            //
// Permission is hereby granted, free of charge, to any person obtaining a    //
// copy of this software and associated documentation files (the "Software"), //
// to deal in the Software without restriction, including without limitation  //
// the rights to use, copy, modify, merge, publish, distribute, sublicense,   //
// and/or sell copies of the Software, and to permit persons to whom the      //
// Software is furnished to do so, subject to the following conditions:       //
//                                                                            //
// The above copyright notice and this permission notice shall be included in //
// all copies or substantial portions of the Software.                        //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//----------------------------------------------------------------------------//

#include "ozz/geometry/runtime/skinning_job.h"

#include <cassert>

#include "ozz/base/maths/simd_math.h"

namespace ozz {
namespace geometry {

SkinningJob::SkinningJob()
    : vertex_count(0),
      influences_count(0),
      joint_indices_stride(0),
      joint_weights_stride(0),
      in_positions_stride(0),
      in_normals_stride(0),
      in_tangents_stride(0),
      out_positions_stride(0),
      out_normals_stride(0),
      out_tangents_stride(0) {}

bool SkinningJob::Validate() const {
  // Start validation of all parameters.
  bool valid = true;

  // Checks influences bounds.
  valid &= influences_count > 0;

  // Checks joints matrices, required.
  valid &= joint_matrices.begin != NULL;
  valid &= joint_matrices.end >= joint_matrices.begin;

  // Checks optional inverse transpose matrices.
  if (joint_inverse_transpose_matrices.begin) {
    valid &= joint_inverse_transpose_matrices.begin != NULL;
    valid &= joint_inverse_transpose_matrices.end >=
             joint_inverse_transpose_matrices.begin;
  }

  // Prepares local variables used to compute buffer size.
  const int vertex_count_minus_1 = vertex_count > 0 ? vertex_count - 1 : 0;
  const int vertex_count_at_least_1 = vertex_count > 0;

  // Checks indices, required.
  valid &= joint_indices.begin != NULL;
  valid &= joint_indices.size() >=
           joint_indices_stride * vertex_count_minus_1 +
               sizeof(uint16_t) * influences_count * vertex_count_at_least_1;

  // Checks weights, required if influences_count > 1.
  if (influences_count != 1) {
    valid &= joint_weights.begin != NULL;
    valid &=
        joint_weights.size() >=
        joint_weights_stride * vertex_count_minus_1 +
            sizeof(float) * (influences_count - 1) * vertex_count_at_least_1;
  }

  // Checks positions, mandatory.
  valid &= in_positions.begin != NULL;
  valid &=
      in_positions.size() >= in_positions_stride * vertex_count_minus_1 +
                                 sizeof(float) * 3 * vertex_count_at_least_1;
  valid &= out_positions.begin != NULL;
  valid &=
      out_positions.size() >= out_positions_stride * vertex_count_minus_1 +
                                  sizeof(float) * 3 * vertex_count_at_least_1;

  // Checks normals, optional.
  if (in_normals.begin) {
    valid &=
        in_normals.size() >= in_normals_stride * vertex_count_minus_1 +
                                 sizeof(float) * 3 * vertex_count_at_least_1;
    valid &= out_normals.begin != NULL;
    valid &=
        out_normals.size() >= out_normals_stride * vertex_count_minus_1 +
                                  sizeof(float) * 3 * vertex_count_at_least_1;

    // Checks tangents, optional but requires normals.
    if (in_tangents.begin) {
      valid &=
          in_tangents.size() >= in_tangents_stride * vertex_count_minus_1 +
                                    sizeof(float) * 3 * vertex_count_at_least_1;
      valid &= out_tangents.begin != NULL;
      valid &= out_tangents.size() >=
               out_tangents_stride * vertex_count_minus_1 +
                   sizeof(float) * 3 * vertex_count_at_least_1;
    }
  } else {
    // Tangents are not supported if normals are not there.
    valid &= in_tangents.begin == NULL;
    valid &= in_tangents.end == NULL;
  }

  return valid;
}

// For performance optimization reasons, every skinning variants (positions,
// positions + normals, 1 to n influences...) are implemented as separate
// specialized functions.
// To cope with the error prone aspect of implementing every function, we
// define a skeleton code (SKINNING_FN) for the skinning loop, which internally
// calls MACRO that are shared or specialized according to skinning variants.

// Defines the skeleton code for the per vertex skinning loop.
#define SKINNING_FN(_type, _it, _inf)                                        \
  void SKINNING_FN_NAME(_type, _it, _inf)(const SkinningJob& _job) {         \
    ASSERT_##_type() ASSERT_##_it() INIT_##_type() INIT_W##_inf()            \
        const int loops = _job.vertex_count - 1;                             \
    for (int i = 0; i < loops; ++i) {                                        \
      PREPARE_##_inf##_INNER(_it) TRANSFORM_##_type##_INNER() NEXT_##_type() \
          NEXT_W##_inf()                                                     \
    }                                                                        \
    PREPARE_##_inf##_OUTER(_it) TRANSFORM_##_type##_OUTER()                  \
  }

// Defines skinning function name.
#define SKINNING_FN_NAME(_type, _it, _inf) Skinning##_type##_it##_inf

// Implements pre-conditions assertions.
#define ASSERT_P()                                      \
  assert(_job.vertex_count&& _job.in_positions.begin && \
         !_job.in_normals.begin && !_job.in_tangents.begin);

#define ASSERT_PN()                                                          \
  assert(                                                                    \
      _job.vertex_count&& _job.in_positions.begin&& _job.in_normals.begin && \
      !_job.in_tangents.begin);

#define ASSERT_PNT()                                                   \
  assert(_job.vertex_count&& _job.in_positions.begin&& _job.in_normals \
             .begin&& _job.in_tangents.begin);

#define ASSERT_NOIT()

#define ASSERT_IT() assert(_job.joint_inverse_transpose_matrices.begin);

// Implements loop initializations for positions, ...
#define INIT_P()                                            \
  const uint16_t* joint_indices = _job.joint_indices.begin; \
  const float* in_positions = _job.in_positions.begin;      \
  float* out_positions = _job.out_positions.begin;

#define INIT_PN()                                  \
  INIT_P();                                        \
  const float* in_normals = _job.in_normals.begin; \
  float* out_normals = _job.out_normals.begin;

#define INIT_PNT()                                   \
  INIT_PN();                                         \
  const float* in_tangents = _job.in_tangents.begin; \
  float* out_tangents = _job.out_tangents.begin;

// Implements loop initializations for weights.
// Note that if the number of influences per vertex is 1, then there's no weight
// as it's implicitly 1.
#define INIT_W1()

#define INIT_W2()                                        \
  const math::SimdFloat4 one = math::simd_float4::one(); \
  const float* joint_weights = _job.joint_weights.begin;

#define INIT_W3() INIT_W2()

#define INIT_W4() INIT_W2()

#define INIT_WN() INIT_W2()

// Implements pointer striding.
#define NEXT(_type, _current, _stride) \
  reinterpret_cast<_type>(reinterpret_cast<uintptr_t>(_current) + _stride)

#define NEXT_W1()

#define NEXT_W2() \
  joint_weights = NEXT(const float*, joint_weights, _job.joint_weights_stride);

#define NEXT_W3() NEXT_W2()

#define NEXT_W4() NEXT_W2()

#define NEXT_WN() NEXT_W2()

#define NEXT_P()                                                             \
  joint_indices =                                                            \
      NEXT(const uint16_t*, joint_indices, _job.joint_indices_stride);       \
  in_positions = NEXT(const float*, in_positions, _job.in_positions_stride); \
  out_positions = NEXT(float*, out_positions, _job.out_positions_stride);

#define NEXT_PN()                                                      \
  NEXT_P();                                                            \
  in_normals = NEXT(const float*, in_normals, _job.in_normals_stride); \
  out_normals = NEXT(float*, out_normals, _job.out_normals_stride);

#define NEXT_PNT()                                                        \
  NEXT_PN();                                                              \
  in_tangents = NEXT(const float*, in_tangents, _job.in_tangents_stride); \
  out_tangents = NEXT(float*, out_tangents, _job.out_tangents_stride);

// Implements weighted matrix preparation.
// _INNER functions are intended to be used inside the vertex loop. They take
// advantage of the fact that the buffers they are reading from contain enough
// remaining data to use more optimized SIMD load functions. At the opposite,
// _OUTER functions restrict access to data that are sure to be readable from
// the buffer.
#define PREPARE_1_INNER(_it)                                 \
  const uint16_t i0 = joint_indices[0];                      \
  const math::Float4x4& transform = _job.joint_matrices[i0]; \
  PREPARE_##_it##_1()

#define PREPARE_1_OUTER(_it) PREPARE_1_INNER(_it)

#define PREPARE_NOIT()                            \
  const math::Float4x4& it_transform = transform; \
  (void)it_transform;

#define PREPARE_NOIT_1() PREPARE_NOIT()

#define PREPARE_IT_1()                 \
  const math::Float4x4& it_transform = \
      _job.joint_inverse_transpose_matrices[i0];

#define PREPARE_2_INNER(_it)                                                   \
  const math::SimdFloat4 w0 = math::simd_float4::Load1PtrU(joint_weights + 0); \
  const uint16_t i0 = joint_indices[0];                                        \
  const uint16_t i1 = joint_indices[1];                                        \
  const math::Float4x4& m0 = _job.joint_matrices[i0];                          \
  const math::Float4x4& m1 = _job.joint_matrices[i1];                          \
  const math::SimdFloat4 w1 = one - w0;                                        \
  const math::Float4x4 transform =                                             \
      math::ColumnMultiply(m0, w0) + math::ColumnMultiply(m1, w1);             \
  PREPARE_##_it##_2()

#define PREPARE_NOIT_2() PREPARE_NOIT()

#define PREPARE_IT_2()                                                    \
  const math::Float4x4& mit0 = _job.joint_inverse_transpose_matrices[i0]; \
  const math::Float4x4& mit1 = _job.joint_inverse_transpose_matrices[i1]; \
  const math::Float4x4 it_transform =                                     \
      math::ColumnMultiply(mit0, w0) + math::ColumnMultiply(mit1, w1);

#define PREPARE_2_OUTER(_it) PREPARE_2_INNER(_it)

#define PREPARE_3_CONCAT(_it)                                     \
  const uint16_t i0 = joint_indices[0];                           \
  const uint16_t i1 = joint_indices[1];                           \
  const uint16_t i2 = joint_indices[2];                           \
  const math::Float4x4& m0 = _job.joint_matrices[i0];             \
  const math::Float4x4& m1 = _job.joint_matrices[i1];             \
  const math::Float4x4& m2 = _job.joint_matrices[i2];             \
  const math::SimdFloat4 w2 = one - (w0 + w1);                    \
  const math::Float4x4 transform = math::ColumnMultiply(m0, w0) + \
                                   math::ColumnMultiply(m1, w1) + \
                                   math::ColumnMultiply(m2, w2);  \
  PREPARE_##_it##_3()

#define PREPARE_NOIT_3() PREPARE_NOIT()

#define PREPARE_IT_3()                                                    \
  const math::Float4x4& mit0 = _job.joint_inverse_transpose_matrices[i0]; \
  const math::Float4x4& mit1 = _job.joint_inverse_transpose_matrices[i1]; \
  const math::Float4x4& mit2 = _job.joint_inverse_transpose_matrices[i2]; \
  const math::Float4x4 it_transform = math::ColumnMultiply(mit0, w0) +    \
                                      math::ColumnMultiply(mit1, w1) +    \
                                      math::ColumnMultiply(mit2, w2);

#define PREPARE_3_INNER(_it)                                             \
  const math::SimdFloat4 w = math::simd_float4::LoadPtrU(joint_weights); \
  const math::SimdFloat4 w0 = math::SplatX(w);                           \
  const math::SimdFloat4 w1 = math::SplatY(w);                           \
  PREPARE_3_CONCAT(_it)

#define PREPARE_3_OUTER(_it)                                                   \
  const math::SimdFloat4 w0 = math::simd_float4::Load1PtrU(joint_weights + 0); \
  const math::SimdFloat4 w1 = math::simd_float4::Load1PtrU(joint_weights + 1); \
  PREPARE_3_CONCAT(_it)

#define PREPARE_4_CONCAT(_it)                                       \
  const uint16_t i0 = joint_indices[0];                             \
  const uint16_t i1 = joint_indices[1];                             \
  const uint16_t i2 = joint_indices[2];                             \
  const uint16_t i3 = joint_indices[3];                             \
  const math::Float4x4& m0 = _job.joint_matrices[i0];               \
  const math::Float4x4& m1 = _job.joint_matrices[i1];               \
  const math::Float4x4& m2 = _job.joint_matrices[i2];               \
  const math::Float4x4& m3 = _job.joint_matrices[i3];               \
  const math::SimdFloat4 w3 = one - (w0 + w1 + w2);                 \
  const math::Float4x4 transform =                                  \
      math::ColumnMultiply(m0, w0) + math::ColumnMultiply(m1, w1) + \
      math::ColumnMultiply(m2, w2) + math::ColumnMultiply(m3, w3);  \
  PREPARE_##_it##_4()

#define PREPARE_NOIT_4() PREPARE_NOIT()

#define PREPARE_IT_4()                                                    \
  const math::Float4x4& mit0 = _job.joint_inverse_transpose_matrices[i0]; \
  const math::Float4x4& mit1 = _job.joint_inverse_transpose_matrices[i1]; \
  const math::Float4x4& mit2 = _job.joint_inverse_transpose_matrices[i2]; \
  const math::Float4x4& mit3 = _job.joint_inverse_transpose_matrices[i3]; \
  const math::Float4x4 it_transform =                                     \
      math::ColumnMultiply(mit0, w0) + math::ColumnMultiply(mit1, w1) +   \
      math::ColumnMultiply(mit2, w2) + math::ColumnMultiply(mit3, w3);

#define PREPARE_4_INNER(_it)                                             \
  const math::SimdFloat4 w = math::simd_float4::LoadPtrU(joint_weights); \
  const math::SimdFloat4 w0 = math::SplatX(w);                           \
  const math::SimdFloat4 w1 = math::SplatY(w);                           \
  const math::SimdFloat4 w2 = math::SplatZ(w);                           \
  PREPARE_4_CONCAT(_it)

#define PREPARE_4_OUTER(_it)                                                   \
  const math::SimdFloat4 w0 = math::simd_float4::Load1PtrU(joint_weights + 0); \
  const math::SimdFloat4 w1 = math::simd_float4::Load1PtrU(joint_weights + 1); \
  const math::SimdFloat4 w2 = math::simd_float4::Load1PtrU(joint_weights + 2); \
  PREPARE_4_CONCAT(_it)

#define PREPARE_NOIT_N()                                                     \
  math::SimdFloat4 wsum = math::simd_float4::Load1PtrU(joint_weights + 0);   \
  math::Float4x4 transform =                                                 \
      math::ColumnMultiply(_job.joint_matrices[joint_indices[0]], wsum);     \
  const int last = _job.influences_count - 1;                                \
  for (int j = 1; j < last; ++j) {                                           \
    const math::SimdFloat4 w =                                               \
        math::simd_float4::Load1PtrU(joint_weights + j);                     \
    wsum = wsum + w;                                                         \
    transform = transform + math::ColumnMultiply(                            \
                                _job.joint_matrices[joint_indices[j]], w);   \
  }                                                                          \
  transform =                                                                \
      transform + math::ColumnMultiply(                                      \
                      _job.joint_matrices[joint_indices[last]], one - wsum); \
  PREPARE_NOIT()

#define PREPARE_IT_N()                                                        \
  math::SimdFloat4 wsum = math::simd_float4::Load1PtrU(joint_weights + 0);    \
  const uint16_t i0 = joint_indices[0];                                       \
  math::Float4x4 transform =                                                  \
      math::ColumnMultiply(_job.joint_matrices[i0], wsum);                    \
  math::Float4x4 it_transform =                                               \
      math::ColumnMultiply(_job.joint_inverse_transpose_matrices[i0], wsum);  \
  const int last = _job.influences_count - 1;                                 \
  for (int j = 1; j < last; ++j) {                                            \
    const uint16_t ij = joint_indices[j];                                     \
    const math::SimdFloat4 w =                                                \
        math::simd_float4::Load1PtrU(joint_weights + j);                      \
    wsum = wsum + w;                                                          \
    transform = transform + math::ColumnMultiply(_job.joint_matrices[ij], w); \
    it_transform =                                                            \
        it_transform +                                                        \
        math::ColumnMultiply(_job.joint_inverse_transpose_matrices[ij], w);   \
  }                                                                           \
  const math::SimdFloat4 wlast = one - wsum;                                  \
  const int ilast = joint_indices[last];                                      \
  transform =                                                                 \
      transform + math::ColumnMultiply(_job.joint_matrices[ilast], wlast);    \
  it_transform =                                                              \
      it_transform + math::ColumnMultiply(                                    \
                         _job.joint_inverse_transpose_matrices[ilast], wlast);

#define PREPARE_N_INNER(_it) PREPARE_##_it##_N()

#define PREPARE_N_OUTER(_it) PREPARE_##_it##_N()

// Implement point and vector transformation. _INNER and _OUTER have the same
// meaning as defined for the PREPARE functions.
#define TRANSFORM_P_INNER()                                                \
  const math::SimdFloat4 in_p = math::simd_float4::LoadPtrU(in_positions); \
  const math::SimdFloat4 out_p = TransformPoint(transform, in_p);          \
  math::Store3PtrU(out_p, out_positions);

#define TRANSFORM_PN_INNER()                                             \
  TRANSFORM_P_INNER();                                                   \
  const math::SimdFloat4 in_n = math::simd_float4::LoadPtrU(in_normals); \
  const math::SimdFloat4 out_n = TransformVector(it_transform, in_n);    \
  math::Store3PtrU(out_n, out_normals);

#define TRANSFORM_PNT_INNER()                                             \
  TRANSFORM_PN_INNER();                                                   \
  const math::SimdFloat4 in_t = math::simd_float4::LoadPtrU(in_tangents); \
  const math::SimdFloat4 out_t = TransformVector(it_transform, in_t);     \
  math::Store3PtrU(out_t, out_tangents);

#define TRANSFORM_P_OUTER()                                                 \
  const math::SimdFloat4 in_p = math::simd_float4::Load3PtrU(in_positions); \
  const math::SimdFloat4 out_p = TransformPoint(transform, in_p);           \
  math::Store3PtrU(out_p, out_positions);

#define TRANSFORM_PN_OUTER()                                              \
  TRANSFORM_P_OUTER();                                                    \
  const math::SimdFloat4 in_n = math::simd_float4::Load3PtrU(in_normals); \
  const math::SimdFloat4 out_n = TransformVector(it_transform, in_n);     \
  math::Store3PtrU(out_n, out_normals);

#define TRANSFORM_PNT_OUTER()                                              \
  TRANSFORM_PN_OUTER();                                                    \
  const math::SimdFloat4 in_t = math::simd_float4::Load3PtrU(in_tangents); \
  const math::SimdFloat4 out_t = TransformVector(it_transform, in_t);      \
  math::Store3PtrU(out_t, out_tangents);

// Instantiates all skinning function variants.
SKINNING_FN(P, NOIT, 1)
SKINNING_FN(PN, NOIT, 1)
SKINNING_FN(PNT, NOIT, 1)
SKINNING_FN(PN, IT, 1)
SKINNING_FN(PNT, IT, 1)
SKINNING_FN(P, NOIT, 2)
SKINNING_FN(PN, NOIT, 2)
SKINNING_FN(PNT, NOIT, 2)
SKINNING_FN(PN, IT, 2)
SKINNING_FN(PNT, IT, 2)
SKINNING_FN(P, NOIT, 3)
SKINNING_FN(PN, NOIT, 3)
SKINNING_FN(PNT, NOIT, 3)
SKINNING_FN(PN, IT, 3)
SKINNING_FN(PNT, IT, 3)
SKINNING_FN(P, NOIT, 4)
SKINNING_FN(PN, NOIT, 4)
SKINNING_FN(PNT, NOIT, 4)
SKINNING_FN(PN, IT, 4)
SKINNING_FN(PNT, IT, 4)
SKINNING_FN(P, NOIT, N)
SKINNING_FN(PN, NOIT, N)
SKINNING_FN(PNT, NOIT, N)
SKINNING_FN(PN, IT, N)
SKINNING_FN(PNT, IT, N)

// Defines a matrix of skinning function pointers. This matrix will then be
// indexed according to skinning jobs parameters.
typedef void (*SkiningFct)(const SkinningJob&);
static const SkiningFct kSkinningFct[2][5][3] = {
    {
        {&SKINNING_FN_NAME(P, NOIT, 1), &SKINNING_FN_NAME(PN, NOIT, 1),
         &SKINNING_FN_NAME(PNT, NOIT, 1)},
        {&SKINNING_FN_NAME(P, NOIT, 2), &SKINNING_FN_NAME(PN, NOIT, 2),
         &SKINNING_FN_NAME(PNT, NOIT, 2)},
        {&SKINNING_FN_NAME(P, NOIT, 3), &SKINNING_FN_NAME(PN, NOIT, 3),
         &SKINNING_FN_NAME(PNT, NOIT, 3)},
        {&SKINNING_FN_NAME(P, NOIT, 4), &SKINNING_FN_NAME(PN, NOIT, 4),
         &SKINNING_FN_NAME(PNT, NOIT, 4)},
        {&SKINNING_FN_NAME(P, NOIT, N), &SKINNING_FN_NAME(PN, NOIT, N),
         &SKINNING_FN_NAME(PNT, NOIT, N)},
    },
    {
        {&SKINNING_FN_NAME(P, NOIT, 1), &SKINNING_FN_NAME(PN, IT, 1),
         &SKINNING_FN_NAME(PNT, IT, 1)},
        {&SKINNING_FN_NAME(P, NOIT, 2), &SKINNING_FN_NAME(PN, IT, 2),
         &SKINNING_FN_NAME(PNT, IT, 2)},
        {&SKINNING_FN_NAME(P, NOIT, 3), &SKINNING_FN_NAME(PN, IT, 3),
         &SKINNING_FN_NAME(PNT, IT, 3)},
        {&SKINNING_FN_NAME(P, NOIT, 4), &SKINNING_FN_NAME(PN, IT, 4),
         &SKINNING_FN_NAME(PNT, IT, 4)},
        {&SKINNING_FN_NAME(P, NOIT, N), &SKINNING_FN_NAME(PN, IT, N),
         &SKINNING_FN_NAME(PNT, IT, N)},
    }};

// Implements job Run function.
bool SkinningJob::Run() const {
  // Exit with an error if job is invalid.
  if (!Validate()) {
    return false;
  }

  // Early out if no vertex. This isn't an error.
  // Skinning function algorithm doesn't support the case.
  if (vertex_count == 0) {
    return true;
  }

  // Find skinning function index.
  const size_t it = joint_inverse_transpose_matrices.begin != NULL;
  assert(it < OZZ_ARRAY_SIZE(kSkinningFct));
  const size_t inf =
      static_cast<size_t>(influences_count) > OZZ_ARRAY_SIZE(kSkinningFct[0])
          ? OZZ_ARRAY_SIZE(kSkinningFct[0]) - 1
          : influences_count - 1;
  assert(inf < OZZ_ARRAY_SIZE(kSkinningFct[0]));
  const size_t fct = (in_normals.begin != NULL) + (in_tangents.begin != NULL);
  assert(fct < OZZ_ARRAY_SIZE(kSkinningFct[0][0]));

  // Calls skinning function. Cannot fail because job is valid.
  kSkinningFct[it][inf][fct](*this);

  return true;
}
}  // namespace geometry
}  // namespace ozz
