//----------------------------------------------------------------------------//
//                                                                            //
// ozz-animation is hosted at http://github.com/guillaumeblanc/ozz-animation  //
// and distributed under the MIT License (MIT).                               //
//                                                                            //
// Copyright (c) Guillaume Blanc                                              //
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

#include "../../../include/ozz/animation/offline/animation_optimizer.h"


#include <cstddef>
#include <functional>

// Internal include file
#define OZZ_INCLUDE_PRIVATE_HEADER  // Allows to include private headers.
#include "../src/animation/offline/decimate.h"
#include "../../../include/ozz/animation/offline/raw_animation.h"
#include "../../../include/ozz/animation/offline/raw_animation_utils.h"
#include "../../../include/ozz/animation/runtime/skeleton.h"
#include "../../../include/ozz/animation/runtime/skeleton_utils.h"
#include "../../../include/ozz/base/containers/vector.h"
#include "../../../include/ozz/base/maths/math_constant.h"
#include "../../../include/ozz/base/maths/math_ex.h"

namespace ozz {
namespace animation {
namespace offline {

// Setup default values (favoring quality).
AnimationOptimizer::AnimationOptimizer()
{
  joints_setting_override = nullptr;
}

namespace {

AnimationOptimizer::Setting GetJointSetting(
    AnimationOptimizer& _optimizer, int32_t _joint) {
  AnimationOptimizer::Setting setting = _optimizer.setting;
  
	AnimationOptimizer::JointSetting* pNode = hmgetp_null(_optimizer.joints_setting_override, _joint);
  if (pNode) {
    setting = pNode->value;
  }
  return setting;
}

struct HierarchyBuilder {
  HierarchyBuilder(const RawAnimation* _animation, const Skeleton* _skeleton,
                   AnimationOptimizer* _optimizer)
      : specs(nullptr),
        animation(_animation),
        optimizer(_optimizer) {
    ASSERT(_animation->num_tracks() == _skeleton->num_joints());
    arrsetlen(specs, (uint32_t)arrlen(_animation->tracks));

    // Computes hierarchical scale, iterating skeleton forward (root to
    // leaf).
    IterateJointsDF(*_skeleton, [this](int32_t joint, int32_t parent) { ComputeScaleForward(joint, parent); });

    // Computes hierarchical length, iterating skeleton backward (leaf to root).
    IterateJointsDFReverse(*_skeleton, [this](int32_t joint, int32_t parent) { ComputeLengthBackward(joint, parent); });
  }
  ~HierarchyBuilder()
  {
      arrfree(specs);
  }

  struct Spec {
    float length;  // Length of a joint hierarchy (max of all child).
    float scale;   // Scale of a joint hierarchy (accumulated from all parents).
    float tolerance;  // Tolerance of a joint hierarchy (min of all child).
  };

  // Defines the length of a joint hierarchy (of all child).
  Spec* specs;

 private:
  // Extracts maximum translations and scales for each track/joint.
  void ComputeScaleForward(int32_t _joint, int32_t _parent) {
    Spec& joint_spec = specs[_joint];

    // Compute joint maximum animated scale.
    float max_scale = 0.f;
    const RawAnimation::JointTrack& track = animation->tracks[_joint];
    if (arrlen(track.scales) != 0) {
      for (size_t j = 0; j < (uint32_t)arrlen(track.scales); ++j) {
        const Vector3& scale = track.scales[j].value;
        const float max_element = math::Max(
            max(abs(scale.getX()), abs(scale.getY())), abs(scale.getZ()));
        max_scale = math::Max(max_scale, max_element);
      }
    } else {
      max_scale = 1.f;  // Default scale.
    }

    // Accumulate with parent scale.
    joint_spec.scale = max_scale;
    if (_parent != Skeleton::kNoParent) {
      const Spec& parent_spec = specs[_parent];
      joint_spec.scale *= parent_spec.scale;
    }

    // Computes self setting distance and tolerance.
    // Distance is now scaled with accumulated parent scale.
    const AnimationOptimizer::Setting setting =
        GetJointSetting(*optimizer, _joint);
    joint_spec.length = setting.distance * specs[_joint].scale;
    joint_spec.tolerance = setting.tolerance;
  }

  // Propagate child translations back to the root.
  void ComputeLengthBackward(int32_t _joint, int32_t _parent) {
    // Self translation doesn't matter if joint has no parent.
    if (_parent == Skeleton::kNoParent) {
      return;
    }

    // Compute joint maximum animated length.
    float max_length_sq = 0.f;
    const RawAnimation::JointTrack& track = animation->tracks[_joint];
    for (size_t j = 0; j < (uint32_t)arrlen(track.translations); ++j) {
      max_length_sq = max(max_length_sq, (float)lengthSqr(track.translations[j].value));
    }
    const float max_length = sqrt(max_length_sq);

    const Spec& joint_spec = specs[_joint];
    Spec& parent_spec = specs[_parent];

    // Set parent hierarchical spec to its most impacting child, aka max
    // length and min tolerance.
    parent_spec.length = math::Max(
        parent_spec.length, joint_spec.length + max_length * parent_spec.scale);
    parent_spec.tolerance =
        math::Min(parent_spec.tolerance, joint_spec.tolerance);
  }

  // Disables copy and assignment.
  HierarchyBuilder(const HierarchyBuilder&);
  void operator=(const HierarchyBuilder&);

  // Targeted animation.
  const RawAnimation* animation;

  // Usefull to access settings and compute hierarchy length.
  AnimationOptimizer* optimizer;
};

class PositionAdapter {
 public:
  PositionAdapter(float _scale) : scale_(_scale) {}
  bool Decimable(const RawAnimation::TranslationKey&) const { return true; }
  RawAnimation::TranslationKey Lerp(
      const RawAnimation::TranslationKey& _left,
      const RawAnimation::TranslationKey& _right,
      const RawAnimation::TranslationKey& _ref) const {
    const float alpha = (_ref.time - _left.time) / (_right.time - _left.time);
    ASSERT(alpha >= 0.f && alpha <= 1.f);
    const RawAnimation::TranslationKey key = {
        _ref.time, LerpTranslation(_left.value, _right.value, alpha)};
    return key;
  }
  float Distance(const RawAnimation::TranslationKey& _a,
                 const RawAnimation::TranslationKey& _b) const {
      return length(_a.value - _b.value)* scale_;
  }

 private:
  float scale_;
};

class RotationAdapter {
 public:
  RotationAdapter(float _radius) : radius_(_radius) {}
  bool Decimable(const RawAnimation::RotationKey&) const { return true; }
  RawAnimation::RotationKey Lerp(const RawAnimation::RotationKey& _left,
                                 const RawAnimation::RotationKey& _right,
                                 const RawAnimation::RotationKey& _ref) const {
    const float alpha = (_ref.time - _left.time) / (_right.time - _left.time);
    ASSERT(alpha >= 0.f && alpha <= 1.f);
    const RawAnimation::RotationKey key = {
        _ref.time, LerpRotation(_left.value, _right.value, alpha)};
    return key;
  }
  float Distance(const RawAnimation::RotationKey& _left,
                 const RawAnimation::RotationKey& _right) const {
    // Compute the shortest uint32_t angle between the 2 quaternions.
    // cos_half_angle is w component of a-1 * b.
    const float cos_half_angle = dot(_left.value, _right.value);
    const float sine_half_angle =
        sqrt(1.f - math::Min(1.f, cos_half_angle * cos_half_angle));
    // Deduces distance between 2 points on a circle with radius and a given
    // angle. Using half angle helps as it allows to have a right-angle
    // triangle.
    const float distance = 2.f * sine_half_angle * radius_;
    return distance;
  }

 private:
  float radius_;
};

class ScaleAdapter {
 public:
  ScaleAdapter(float _length) : length_(_length) {}
  bool Decimable(const RawAnimation::ScaleKey&) const { return true; }
  RawAnimation::ScaleKey Lerp(const RawAnimation::ScaleKey& _left,
                              const RawAnimation::ScaleKey& _right,
                              const RawAnimation::ScaleKey& _ref) const {
    const float alpha = (_ref.time - _left.time) / (_right.time - _left.time);
    ASSERT(alpha >= 0.f && alpha <= 1.f);
    const RawAnimation::ScaleKey key = {
        _ref.time, LerpScale(_left.value, _right.value, alpha)};
    return key;
  }
  float Distance(const RawAnimation::ScaleKey& _left,
                 const RawAnimation::ScaleKey& _right) const {
    return length(_left.value - _right.value) * length_;
  }

 private:
  float length_;
};
}  // namespace

bool AnimationOptimizer::operator()(const RawAnimation& _input,
                                    const Skeleton& _skeleton,
                                    RawAnimation* _output) {
  if (!_output) {
    return false;
  }
  // Reset output animation to default.
  *_output = RawAnimation();

  // Validate animation.
  if (!_input.Validate()) {
    return false;
  }

  const int32_t num_tracks = _input.num_tracks();

  // Validates the skeleton matches the animation.
  if (num_tracks != _skeleton.num_joints()) {
    return false;
  }

  // First computes bone lengths, that will be used when filtering.
  const HierarchyBuilder hierarchy(&_input, &_skeleton, this);

  // Rebuilds output animation.
  bassign(&_output->name, &_input.name);
  _output->duration = _input.duration;
  arrsetlen(_output->tracks, num_tracks);
  memset(_output->tracks, 0, sizeof(*_output->tracks) * num_tracks);

  for (int32_t i = 0; i < num_tracks; ++i) {
    const RawAnimation::JointTrack& input = _input.tracks[i];
    RawAnimation::JointTrack& output = _output->tracks[i];

    // Gets joint specs back.
    const float joint_length = hierarchy.specs[i].length;
    const int32_t parent = _skeleton.joint_parents()[i];
    const float parent_scale =
        (parent != Skeleton::kNoParent) ? hierarchy.specs[parent].scale : 1.f;
    const float tolerance = hierarchy.specs[i].tolerance;

    // Filters independently T, R and S tracks.
    // This joint translation is affected by parent scale.
    const PositionAdapter tadap(parent_scale);
    Decimate(input.translations, tadap, tolerance, &output.translations);
    // This joint rotation affects children translations/length.
    const RotationAdapter radap(joint_length);
    Decimate(input.rotations, radap, tolerance, &output.rotations);
    // This joint scale affects children translations/length.
    const ScaleAdapter sadap(joint_length);
    Decimate(input.scales, sadap, tolerance, &output.scales);
  }

  // Output animation is always valid though.
  return _output->Validate();
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
