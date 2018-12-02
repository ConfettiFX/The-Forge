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

//CONFFX_BEGIN
#include "../../../include/ozz/animation/runtime/blending_job.h"

#include <cassert>
#include <cstddef>

#include "../../../include/ozz/animation/runtime/skeleton.h"

#include "../../../../../../OS/Math/MathTypes.h"
#include "../../../include/ozz/base/maths/math_ex.h"
//CONFFX_END

namespace ozz {
namespace animation {

BlendingJob::Layer::Layer() : weight(0.f) {}

BlendingJob::BlendingJob() : threshold(.1f) {}

namespace {
bool ValidateLayer(const BlendingJob::Layer& _layer, ptrdiff_t _min_range) {
  bool valid = true;

  // Tests transforms validity.
  valid &= _layer.transform.begin != NULL;
  valid &= _layer.transform.end >= _layer.transform.begin;
  valid &= _layer.transform.end - _layer.transform.begin >= _min_range;

  // Joint weights are optional.
  if (_layer.joint_weights.begin != NULL) {
    valid &= _layer.joint_weights.end >= _layer.joint_weights.begin;

    // valid
    valid &=
        _layer.joint_weights.end - _layer.joint_weights.begin >= _min_range;
    // not valid

  } else {
    valid &= _layer.joint_weights.end == NULL;
  }

  return valid;
}
}  // namespace

bool BlendingJob::Validate() const {
  // Don't need any early out, as jobs are valid in most of the performance
  // critical cases.
  // Tests are written in multiple lines in order to avoid branches.
  bool valid = true;

  // Test for valid threshold).
  valid &= threshold > 0.f;

  // Test for NULL begin pointers.
  // Blending layers are mandatory, additive aren't.
  valid &= bind_pose.begin != NULL;
  valid &= output.begin != NULL;

  // Test ranges are valid (implicitly test for NULL end pointers).
  valid &= bind_pose.end >= bind_pose.begin;
  valid &= output.end >= output.begin;

  // The bind pose size defines the ranges of transforms to blend, so all
  // other buffers should be bigger.
  const ptrdiff_t min_range = bind_pose.end - bind_pose.begin;
  valid &= output.end - output.begin >= min_range;

  // Blend layers are optional.
  if (layers.begin != NULL) {
    valid &= layers.end >= layers.begin;
  } else {
    valid &= layers.end == NULL;
  }

  // Validates layers.
  for (const Layer* layer = layers.begin; layers.begin && layer < layers.end;
       ++layer) {
    valid &= ValidateLayer(*layer, min_range);
  }

  // Additive layers are optional.
  if (additive_layers.begin != NULL) {
    valid &= additive_layers.end >= additive_layers.begin;
  } else {
    valid &= additive_layers.end == NULL;
  }

  // Validates additive layers.
  for (const Layer* layer = additive_layers.begin;
       additive_layers.begin &&
       layer < additive_layers.end;  // Handles NULL pointers.
       ++layer) {
    valid &= ValidateLayer(*layer, min_range);
  }

  return valid;
}

namespace {

// Macro that defines the process of blending the 1st pass.
#define OZZ_BLEND_1ST_PASS(_in, _simd_weight, _out)     \
  {                                                     \
    _out->translation = _in.translation * _simd_weight; \
    _out->rotation = _in.rotation * _simd_weight;       \
    _out->scale = _in.scale * _simd_weight;             \
  }

//CONFFX_BEGIN
// Macro that defines the process of blending any pass but the first.
#define OZZ_BLEND_N_PASS(_in, _simd_weight, _out)                           \
  {                                                                         \
    /* Blends translation. */                                               \
    _out->translation = _out->translation + _in.translation * _simd_weight; \
    /* Blends rotations, negates opposed quaternions to be sure to choose*/ \
    /* the shortest path between the two.*/                                 \
    const Vector4 dot = mulPerElem(_out->rotation.x, _in.rotation.x) +        \
                                 mulPerElem(_out->rotation.y, _in.rotation.y) +        \
                                 mulPerElem(_out->rotation.z, _in.rotation.z) +        \
                                 mulPerElem(_out->rotation.w, _in.rotation.w);         \
    const Vector4Int sign = signBit(dot);                                       \
    const SoaQuaternion rotation = {                                  \
        xorPerElem(_in.rotation.x, sign), xorPerElem(_in.rotation.y, sign),   \
        xorPerElem(_in.rotation.z, sign), xorPerElem(_in.rotation.w, sign)};  \
    _out->rotation = _out->rotation + rotation * _simd_weight;              \
    /* Blends scales.*/                                                     \
    _out->scale = _out->scale + _in.scale * _simd_weight;                  \
  }

// Macro that defines the process of adding a pass.
#define OZZ_ADD_PASS(_in, _simd_weight, _out)                                \
  {                                                                          \
    _out.translation = _out.translation + _in.translation * _simd_weight;    \
    /* Interpolate quaternion between identity and src.rotation.*/           \
    /* Quaternion sign is fixed up, so that lerp takes the shortest path.*/  \
    const Vector4Int sign = signBit(_in.rotation.w);                  \
    const SoaQuaternion rotation = {                                   \
        xorPerElem(_in.rotation.x, sign), xorPerElem(_in.rotation.y, sign),    \
        xorPerElem(_in.rotation.z, sign), xorPerElem(_in.rotation.w, sign)};   \
    const SoaQuaternion interp_quat = {                                \
        mulPerElem(rotation.x, _simd_weight), mulPerElem(rotation.y, _simd_weight),                \
        mulPerElem(rotation.z, _simd_weight), mulPerElem((rotation.w - one), _simd_weight) + one}; \
    _out.rotation = NormalizeEst(interp_quat) * _out.rotation;               \
    _out.scale =                                                             \
        _out.scale * (one_minus_weight_f3 + (_in.scale * _simd_weight));     \
  }

// Macro that defines the process of subtracting a pass.
#define OZZ_SUB_PASS(_in, _simd_weight, _out)                                \
  {                                                                          \
    _out.translation = _out.translation - _in.translation * _simd_weight;    \
    /* Interpolate quaternion between identity and src.rotation.*/           \
    /* Quaternion sign is fixed up, so that lerp takes the shortest path.*/  \
    const Vector4Int sign = signBit(_in.rotation.w);                  \
    const SoaQuaternion rotation = {                                   \
        xorPerElem(_in.rotation.x, sign), xorPerElem(_in.rotation.y, sign),    \
        xorPerElem(_in.rotation.z, sign), xorPerElem(_in.rotation.w, sign)};   \
    const SoaQuaternion interp_quat = {                                \
        mulPerElem(rotation.x, _simd_weight), mulPerElem(rotation.y, _simd_weight),                \
        mulPerElem(rotation.z, _simd_weight), mulPerElem((rotation.w - one), _simd_weight) + one}; \
    _out.rotation = Conjugate(NormalizeEst(interp_quat)) * _out.rotation;    \
    const SoaFloat3 rcp_scale = {                                      \
        rcpEst(one_minus_weight + (mulPerElem(_in.scale.x, _simd_weight))),       \
        rcpEst(one_minus_weight + (mulPerElem(_in.scale.y, _simd_weight))),       \
        rcpEst(one_minus_weight + (mulPerElem(_in.scale.z, _simd_weight)))};      \
    _out.scale = _out.scale * rcp_scale;                                     \
  }
//CONFFX_END

// Defines parameters that are passed through blending stages.
struct ProcessArgs {
  ProcessArgs(const BlendingJob& _job)
      : job(_job),
        num_soa_joints(_job.bind_pose.end - _job.bind_pose.begin),
        num_passes(0),
        num_partial_passes(0),
        accumulated_weight(0.f) {
    // The range of all buffers has already been validated.
    assert(job.output.end >= job.output.begin + num_soa_joints);
    assert(OZZ_ARRAY_SIZE(accumulated_weights) >= num_soa_joints);
  }

  // Allocates enough space to store a accumulated weights per-joint.
  // It will be initialized by the first pass processed, if any.
  // This is quite big for a stack allocation (16 byte * maximum number of
  // joints). This is one of the reasons why the number of joints is limited
  // by the API.
  // Note that this array is used with SoA data.
  // This is the first argument in order to avoid wasting too much space with
  // alignment padding.
  Vector4 accumulated_weights[Skeleton::kMaxSoAJoints]; //CONFFX_BEGIN

  // The job to process.
  const BlendingJob& job;

  // The number of transforms to process as defined by the size of the bind
  // pose.
  size_t num_soa_joints;

  // Number of processed blended passes (excluding passes with a weight <= 0.f),
  // including partial passes.
  int num_passes;

  // Number of processed partial blending passes (aka with a weight per-joint).
  int num_partial_passes;

  // The accumulated weight of all layers.
  float accumulated_weight;

 private:
  // Disables assignment operators.
  ProcessArgs(const ProcessArgs&);
  void operator=(const ProcessArgs&);
};

// Blends all layers of the job to its output.
void BlendLayers(ProcessArgs* _args) {
  assert(_args);

  // Iterates through all layers and blend them to the output.
  for (const BlendingJob::Layer* layer = _args->job.layers.begin;
       layer < _args->job.layers.end; ++layer) {
    // Asserts buffer sizes, which must never fail as it has been validated.
    assert(layer->transform.end >=
           layer->transform.begin + _args->num_soa_joints);
    assert(!layer->joint_weights.begin ||
           (layer->joint_weights.end >=
            layer->joint_weights.begin + _args->num_soa_joints));

    // Skip irrelevant layers.
    if (layer->weight <= 0.f) {
      continue;
    }

    // Accumulates global weights.
    _args->accumulated_weight += layer->weight;
//CONFFX_BEGIN
    const Vector4 layer_weight =
        Vector4(layer->weight);

    if (layer->joint_weights.begin) {
      // This layer has per-joint weights.
      ++_args->num_partial_passes;

      if (_args->num_passes == 0) {
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform* dest = _args->job.output.begin + i;
          const Vector4 weight =
              mulPerElem(layer_weight, maxPerElem(layer->joint_weights.begin[i], Vector4::zero()));
          _args->accumulated_weights[i] = weight;
          OZZ_BLEND_1ST_PASS(src, weight, dest);
        }
      } else {
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform* dest = _args->job.output.begin + i;
          const Vector4 weight =
              mulPerElem(layer_weight, maxPerElem(layer->joint_weights.begin[i], Vector4::zero()));
          _args->accumulated_weights[i] =
              _args->accumulated_weights[i] + weight;
          OZZ_BLEND_N_PASS(src, weight, dest);
        }
      }
    } else {
      // This is a full layer.
      if (_args->num_passes == 0) {
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform* dest = _args->job.output.begin + i;
          _args->accumulated_weights[i] = layer_weight;
          OZZ_BLEND_1ST_PASS(src, layer_weight, dest);
        }
      } else {
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform* dest = _args->job.output.begin + i;
          _args->accumulated_weights[i] =
              _args->accumulated_weights[i] + layer_weight;
          OZZ_BLEND_N_PASS(src, layer_weight, dest);
        }
      }
    }
//CONFFX_END

    // One more pass blended.
    ++_args->num_passes;
  }
}

// Blends bind pose to the output if accumulated weight is less than the
// threshold value.
void BlendBindPose(ProcessArgs* _args) {
  assert(_args);

  // Asserts buffer sizes, which must never fail as it has been validated.
  assert(_args->job.bind_pose.end >=
         _args->job.bind_pose.begin + _args->num_soa_joints);

  if (_args->num_partial_passes == 0) {
    // No partial blending pass detected, threshold can be tested globally.
    const float bp_weight = _args->job.threshold - _args->accumulated_weight;

    if (bp_weight > 0.f) {  // The bind-pose is needed if it has a weight.
      if (_args->num_passes == 0) {
        // Strictly copying bind-pose.
        _args->accumulated_weight = 1.f;
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          _args->job.output.begin[i] = _args->job.bind_pose.begin[i];
        }
      } else {
        // Updates global accumulated weight, but not per-joint weight any more
        // because normalization stage will be global also.
        _args->accumulated_weight = _args->job.threshold;

//CONFFX_BEGIN
        const Vector4 simd_bp_weight =
            Vector4(bp_weight);

        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = _args->job.bind_pose.begin[i];
          SoaTransform* dest = _args->job.output.begin + i;
          OZZ_BLEND_N_PASS(src, simd_bp_weight, dest);
        }
//CONFFX_END
      }
    }
  } else {
    // Blending passes contain partial blending, threshold must be tested for
    // each joint.

//CONFFX_BEGIN
    const Vector4 threshold =
        Vector4(_args->job.threshold);

    // There's been at least 1 pass as num_partial_passes != 0.
    assert(_args->num_passes != 0);

    for (size_t i = 0; i < _args->num_soa_joints; ++i) {
      const SoaTransform& src = _args->job.bind_pose.begin[i];
      SoaTransform* dest = _args->job.output.begin + i;
      const Vector4 bp_weight =
          maxPerElem(threshold - _args->accumulated_weights[i], Vector4::zero());
      _args->accumulated_weights[i] =
          maxPerElem(threshold, _args->accumulated_weights[i]);
      OZZ_BLEND_N_PASS(src, bp_weight, dest);
    }
//CONFFX_END
  }
}

//CONFFX_BEGIN
// Normalizes output rotations. Quaternion length cannot be zero as opposed
// quaternions have been fixed up during blending passes.
// Translations and scales are already normalized because weights were
// pre-multiplied by the normalization ratio.
void Normalize(ProcessArgs* _args) {
  assert(_args);

  if (_args->num_partial_passes == 0) {
    // Normalization of a non-partial blending requires to apply the same
    // division to all joints.
    const Vector4 ratio =
        Vector4(1.f / _args->accumulated_weight);
    for (size_t i = 0; i < _args->num_soa_joints; ++i) {
      SoaTransform& dest = _args->job.output.begin[i];
      dest.rotation = NormalizeEst(dest.rotation);
      dest.translation = dest.translation * ratio;
      dest.scale = dest.scale * ratio;
    }
  } else {
    // Partial blending normalization requires to compute the divider per-joint.
    const Vector4 one = Vector4::one();
    for (size_t i = 0; i < _args->num_soa_joints; ++i) {
      const Vector4 ratio = divPerElem(one, _args->accumulated_weights[i]);
      SoaTransform& dest = _args->job.output.begin[i];
      dest.rotation = NormalizeEst(dest.rotation);
      dest.translation = dest.translation * ratio;
      dest.scale = dest.scale * ratio;
    }
  }
}

// Process additive blending pass.
void AddLayers(ProcessArgs* _args) {
  assert(_args);

  // Iterates through all layers and blend them to the output.
  for (const BlendingJob::Layer* layer = _args->job.additive_layers.begin;
       layer < _args->job.additive_layers.end; ++layer) {
    // Asserts buffer sizes, which must never fail as it has been validated.
    assert(layer->transform.end >=
           layer->transform.begin + _args->num_soa_joints);
    assert(!layer->joint_weights.begin ||
           (layer->joint_weights.end >=
            layer->joint_weights.begin + _args->num_soa_joints));

    // Prepares constants.
    const Vector4 one = Vector4::one();

    if (layer->weight > 0.f) {
      // Weight is positive, need to perform additive blending.
      const Vector4 layer_weight =
          Vector4(layer->weight);

      if (layer->joint_weights.begin) {
        // This layer has per-joint weights.
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform& dest = _args->job.output.begin[i];
          const Vector4 weight =
              mulPerElem(layer_weight, maxPerElem(layer->joint_weights.begin[i], Vector4::zero()));
          const Vector4 one_minus_weight = one - weight;
          const SoaFloat3 one_minus_weight_f3 = {
              one_minus_weight, one_minus_weight, one_minus_weight};
          OZZ_ADD_PASS(src, weight, dest);
        }
      } else {
        // This is a full layer.
        const Vector4 one_minus_weight = one - layer_weight;
        const SoaFloat3 one_minus_weight_f3 = {
            one_minus_weight, one_minus_weight, one_minus_weight};

        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform& dest = _args->job.output.begin[i];
          OZZ_ADD_PASS(src, layer_weight, dest);
        }
      }
    } else if (layer->weight < 0.f) {
      // Weight is negative, need to perform subtractive blending.
      const Vector4 layer_weight =
          Vector4(-layer->weight);

      if (layer->joint_weights.begin) {
        // This layer has per-joint weights.
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform& dest = _args->job.output.begin[i];
          const Vector4 weight =
              mulPerElem(layer_weight, maxPerElem(layer->joint_weights.begin[i], Vector4::zero()));
          const Vector4 one_minus_weight = one - weight;
          OZZ_SUB_PASS(src, weight, dest);
        }
      } else {
        // This is a full layer.
        const Vector4 one_minus_weight = one - layer_weight;
        for (size_t i = 0; i < _args->num_soa_joints; ++i) {
          const SoaTransform& src = layer->transform.begin[i];
          SoaTransform& dest = _args->job.output.begin[i];
          OZZ_SUB_PASS(src, layer_weight, dest);
        }
      }
    } else {
      // Skip layer as its weight is 0.
    }
  }
}
//CONFFX_END

}  // namespace

bool BlendingJob::Run() const {
  if (!Validate()) {
    return false;
  }

  // Initializes blended parameters that are exchanged across blend stages.
  ProcessArgs process_args(*this);

  // Blends all layers to the job output buffers.
  BlendLayers(&process_args);

  // Applies bind pose.
  BlendBindPose(&process_args);

  // Normalizes output.
  Normalize(&process_args);

  // Process additive blending.
  AddLayers(&process_args);

  return true;
}
}  // namespace animation
}  // namespace ozz
