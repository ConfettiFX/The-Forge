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

#include "ozz/animation/offline/fbx/fbx_animation.h"

#include "ozz/animation/offline/fbx/fbx.h"

#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_animation_utils.h"
#include "ozz/animation/offline/raw_track.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/skeleton_utils.h"

#include "ozz/base/log.h"

#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/transform.h"

namespace ozz {
namespace animation {
namespace offline {
namespace fbx {

namespace {
struct SamplingInfo {
  float start;
  float end;
  float duration;
  float frequency;
};

SamplingInfo ExtractSamplingInfo(FbxScene* _scene, FbxAnimStack* _anim_stack,
                                 float _sampling_rate) {
  SamplingInfo info;

  // Extract animation duration.
  FbxTimeSpan time_spawn;
  const FbxTakeInfo* take_info = _scene->GetTakeInfo(_anim_stack->GetName());
  if (take_info) {
    time_spawn = take_info->mLocalTimeSpan;
  } else {
    _scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(time_spawn);
  }

  // Get frame rate from the scene.
  FbxTime::EMode mode = _scene->GetGlobalSettings().GetTimeMode();
  const float scene_frame_rate =
      static_cast<float>((mode == FbxTime::eCustom)
                             ? _scene->GetGlobalSettings().GetCustomFrameRate()
                             : FbxTime::GetFrameRate(mode));

  // Deduce sampling period.
  // Scene frame rate is used when provided argument is <= 0.
  float sampling_rate;
  if (_sampling_rate > 0.f) {
    sampling_rate = _sampling_rate;
    log::LogV() << "Using sampling rate of " << sampling_rate << "hz."
                << std::endl;
  } else {
    sampling_rate = scene_frame_rate;
    log::LogV() << "Using scene sampling rate of " << sampling_rate << "hz."
                << std::endl;
  }
  info.frequency = sampling_rate;

  // Get scene start and end.
  info.start = static_cast<float>(time_spawn.GetStart().GetSecondDouble());
  info.end = static_cast<float>(time_spawn.GetStop().GetSecondDouble());

  // Duration could be 0 if it's just a pose. In this case we'll set a default
  // 1s duration.
  if (info.end > info.start) {
    info.duration = info.end - info.start;
  } else {
    info.duration = 1.f;
  }

  return info;
}

bool ExtractAnimation(FbxSceneLoader& _scene_loader, const SamplingInfo& _info,
                      const Skeleton& _skeleton, RawAnimation* _animation) {
  FbxScene* scene = _scene_loader.scene();
  assert(scene);

  // Set animation data.
  _animation->duration = _info.duration;

  // Locates all skeleton nodes in the fbx scene. Some might be nullptr.
  ozz::vector<FbxNode*> nodes;
  for (int i = 0; i < _skeleton.num_joints(); i++) {
    const char* joint_name = _skeleton.joint_names()[i];
    nodes.push_back(scene->FindNodeByName(joint_name));
  }

  // Preallocates and initializes world matrices.
  const FixedRateSamplingTime fixed_it(_info.duration, _info.frequency);

  ozz::vector<float> times;
  times.resize(fixed_it.num_keys());
  ozz::vector<ozz::vector<ozz::math::Float4x4>> world_matrices;
  world_matrices.resize(_skeleton.num_joints());
  for (int i = 0; i < _skeleton.num_joints(); i++) {
    world_matrices[i].resize(fixed_it.num_keys());
  }

  // Goes through the whole timeline to compute animated word matrices.
  // Fbx sdk seems to compute nodes transformation for the whole scene, so it's
  // much faster to query all nodes at once for the same time t.
  FbxAnimEvaluator* evaluator = scene->GetAnimationEvaluator();
  for (size_t k = 0; k < fixed_it.num_keys(); ++k) {
    const float t = fixed_it.time(k);
    times[k] = t;

    // Goes through all nodes.
    for (int i = 0; i < _skeleton.num_joints(); i++) {
      FbxNode* node = nodes[i];
      if (node) {
        const FbxAMatrix fbx_matrix = evaluator->GetNodeGlobalTransform(
            node, FbxTimeSeconds(t + _info.start));
        const math::Float4x4 matrix =
            _scene_loader.converter()->ConvertMatrix(fbx_matrix);
        world_matrices[i][k] = matrix;
      }
    }
  }

  // Builds world matrices for non-animated joints.
  // Skeleton is order with parents first, so it can be traversed in order.
  for (int i = 0; i < _skeleton.num_joints(); i++) {
    // Initializes non-animated joints.
    FbxNode* node = nodes[i];
    if (node == nullptr) {
      ozz::log::LogV() << "No animation track found for joint \""
                       << _skeleton.joint_names()[i]
                       << "\". Using skeleton bind pose instead." << std::endl;

      const math::Transform& bind_pose =
          ozz::animation::GetJointLocalBindPose(_skeleton, i);
      const math::SimdFloat4 t =
          math::simd_float4::Load3PtrU(&bind_pose.translation.x);
      const math::SimdFloat4 q =
          math::simd_float4::LoadPtrU(&bind_pose.rotation.x);
      const math::SimdFloat4 s =
          math::simd_float4::Load3PtrU(&bind_pose.scale.x);
      const math::Float4x4 local_matrix = math::Float4x4::FromAffine(t, q, s);

      ozz::vector<math::Float4x4>& node_matrices = world_matrices[i];
      const int16_t parent = _skeleton.joint_parents()[i];
      if (parent != Skeleton::kNoParent) {
        const ozz::vector<math::Float4x4>& parent_matrices =
            world_matrices[parent];
        assert(fixed_it.num_keys() == parent_matrices.size());
        for (size_t k = 0; k < fixed_it.num_keys(); ++k) {
          node_matrices[k] = parent_matrices[k] * local_matrix;
        }
      } else {
        for (size_t k = 0; k < fixed_it.num_keys(); ++k) {
          node_matrices[k] = local_matrix;
        }
      }
    }
  }

  // Builds world inverse matrices.
  ozz::vector<ozz::vector<ozz::math::Float4x4>> world_inv_matrices;
  world_inv_matrices.resize(_skeleton.num_joints());
  for (int i = 0; i < _skeleton.num_joints(); i++) {
    const ozz::vector<math::Float4x4>& node_world_matrices = world_matrices[i];
    ozz::vector<math::Float4x4>& node_world_inv_matrices =
        world_inv_matrices[i];
    node_world_inv_matrices.resize(fixed_it.num_keys());
    for (size_t p = 0; p < fixed_it.num_keys(); ++p) {
      node_world_inv_matrices[p] = Invert(node_world_matrices[p]);
    }
  }

  // Builds local space animation tracks.
  // Allocates all tracks with the same number of joints as the skeleton.
  // Tracks that would not be found will be set to skeleton bind-pose
  // transformation.
  _animation->tracks.resize(_skeleton.num_joints());
  for (int i = 0; i < _skeleton.num_joints(); i++) {
    RawAnimation::JointTrack& track = _animation->tracks[i];
    track.rotations.resize(fixed_it.num_keys());
    track.translations.resize(fixed_it.num_keys());
    track.scales.resize(fixed_it.num_keys());

    const int16_t parent = _skeleton.joint_parents()[i];
    ozz::vector<math::Float4x4>& node_world_matrices = world_matrices[i];
    ozz::vector<math::Float4x4>& node_world_inv_matrices =
        world_inv_matrices[parent != Skeleton::kNoParent ? parent : 0];

    for (size_t n = 0; n < fixed_it.num_keys(); ++n) {
      // Builds local matrix;
      math::Float4x4 local_matrix;
      if (parent != Skeleton::kNoParent) {
        local_matrix = node_world_inv_matrices[n] * node_world_matrices[n];
      } else {
        local_matrix = node_world_matrices[n];
      }

      // Convert to transform structure.
      math::SimdFloat4 t, q, s;
      if (!ToAffine(local_matrix, &t, &q, &s)) {
        ozz::log::Err() << "Failed to extract animation transform for joint\""
                        << _skeleton.joint_names()[i]
                        << "\" at t = " << times[n] << "s." << std::endl;
        return false;
      }

      ozz::math::Transform transform;
      math::Store3PtrU(t, &transform.translation.x);
      math::StorePtrU(math::Normalize4(q), &transform.rotation.x);
      math::Store3PtrU(s, &transform.scale.x);

      // Fills corresponding track.
      const float time = times[n];
      const RawAnimation::TranslationKey tkey = {time, transform.translation};
      track.translations[n] = tkey;
      const RawAnimation::RotationKey rkey = {time, transform.rotation};
      track.rotations[n] = rkey;
      const RawAnimation::ScaleKey skey = {time, transform.scale};
      track.scales[n] = skey;
    }
  }
  return _animation->Validate();
}

template <typename _Type>
bool PropertyGetValueAsFloat(FbxPropertyValue& _property_value,
                             EFbxType _fbx_type, float* _value) {
  _Type value;
  assert(_property_value.GetSizeOf() == sizeof(_Type));
  bool success = _property_value.Get(&value, _fbx_type);
  *_value = static_cast<float>(value);
  return success;
}

bool GetValue(FbxSceneLoader& _scene_loader, FbxPropertyValue& _property_value,
              EFbxType _fbx_type, OzzImporter::NodeProperty::Type _type,
              float* _value) {
  (void)_scene_loader;
  // Only supported types are enumerated, so this function should not be
  // called for something else but kFloat1.
  (void)_type;
  assert(_type == OzzImporter::NodeProperty::kFloat1);

  switch (_fbx_type) {
    case eFbxBool: {
      return PropertyGetValueAsFloat<bool>(_property_value, _fbx_type, _value);
    }
    case eFbxChar: {
      return PropertyGetValueAsFloat<int8_t>(_property_value, _fbx_type,
                                             _value);
    }
    case eFbxUChar: {
      return PropertyGetValueAsFloat<uint8_t>(_property_value, _fbx_type,
                                              _value);
    }
    case eFbxShort: {
      return PropertyGetValueAsFloat<int16_t>(_property_value, _fbx_type,
                                              _value);
    }
    case eFbxUShort: {
      return PropertyGetValueAsFloat<uint16_t>(_property_value, _fbx_type,
                                               _value);
    }
    case eFbxInt:
    case eFbxEnum:
    case eFbxEnumM: {
      return PropertyGetValueAsFloat<int32_t>(_property_value, _fbx_type,
                                              _value);
    }
    case eFbxUInt: {
      return PropertyGetValueAsFloat<uint32_t>(_property_value, _fbx_type,
                                               _value);
    }
    case eFbxFloat: {
      return PropertyGetValueAsFloat<float>(_property_value, _fbx_type, _value);
    }
    case eFbxDouble: {
      return PropertyGetValueAsFloat<double>(_property_value, _fbx_type,
                                             _value);
    }
    default: {
      // Only supported types are enumerated, so this function should not be
      // called for something else.
      assert(false);
      return false;
    }
  }
}

bool GetValue(FbxSceneLoader& _scene_loader, FbxPropertyValue& _property_value,
              EFbxType _fbx_type, OzzImporter::NodeProperty::Type _type,
              ozz::math::Float2* _value) {
  (void)_scene_loader;
  // Only supported types are enumerated, so this function should not be
  // called for something else but eFbxDouble2 / kFloat2.
  (void)_type;
  (void)_fbx_type;
  assert(_fbx_type == eFbxDouble2 &&
         _type == OzzImporter::NodeProperty::kFloat2);

  double dvalue[2];
  if (!_property_value.Get(&dvalue, eFbxDouble2)) {
    return false;
  }
  _value->x = static_cast<float>(dvalue[0]);
  _value->y = static_cast<float>(dvalue[1]);

  return true;
}

bool GetValue(FbxSceneLoader& _scene_loader, FbxPropertyValue& _property_value,
              EFbxType _fbx_type, OzzImporter::NodeProperty::Type _type,
              ozz::math::Float3* _value) {
  // Only supported types are enumerated, so this function should not be
  // called for something else but eFbxDouble3 / (kFloat3, kPosision).
  (void)_type;
  (void)_fbx_type;
  assert(_fbx_type == eFbxDouble3 &&
         (_type == OzzImporter::NodeProperty::kFloat3 ||
          _type == OzzImporter::NodeProperty::kPoint ||
          _type == OzzImporter::NodeProperty::kVector));

  FbxVector4 vec4;
  if (!_property_value.Get(vec4.Buffer(), eFbxDouble3)) {
    return false;
  }

  // Type needs scene / axis conversion
  const FbxSystemConverter* conv = _scene_loader.converter();
  switch (_type) {
    default: {
      // No conversion required
      _value->x = static_cast<float>(vec4[0]);
      _value->y = static_cast<float>(vec4[1]);
      _value->z = static_cast<float>(vec4[2]);
      break;
    }
    case OzzImporter::NodeProperty::kPoint: {
      *_value = conv->ConvertPoint(vec4);
      break;
    }
    case OzzImporter::NodeProperty::kVector: {
      *_value = conv->ConvertVector(vec4);
      break;
    }
  }

  return true;
}  // namespace

bool GetValue(FbxSceneLoader& _scene_loader, FbxPropertyValue& _property_value,
              EFbxType _fbx_type, OzzImporter::NodeProperty::Type _type,
              ozz::math::Float4* _value) {
  (void)_scene_loader;
  // Only supported types are enumerated, so this function should not be
  // called for something else but eFbxDouble4 / kFloat4.
  (void)_type;
  (void)_fbx_type;
  assert(_fbx_type == eFbxDouble4 &&
         _type == OzzImporter::NodeProperty::kFloat4);

  double dvalue[4];
  if (!_property_value.Get(&dvalue, eFbxDouble4)) {
    return false;
  }
  _value->x = static_cast<float>(dvalue[0]);
  _value->y = static_cast<float>(dvalue[1]);
  _value->z = static_cast<float>(dvalue[2]);
  _value->w = static_cast<float>(dvalue[3]);

  return true;
}

template <typename _Track>
bool ExtractCurve(FbxSceneLoader& _scene_loader, FbxProperty& _property,
                  EFbxType _fbx_type, OzzImporter::NodeProperty::Type _type,
                  const SamplingInfo& _info, _Track* _track) {
  assert(_track->keyframes.size() == 0);
  FbxScene* scene = _scene_loader.scene();
  assert(scene);

  FbxAnimEvaluator* evaluator = scene->GetAnimationEvaluator();

  if (!_property.IsAnimated()) {
    FbxPropertyValue& property_value =
        evaluator->GetPropertyValue(_property, FbxTimeSeconds(0.));

    typename _Track::ValueType value;
    if (!GetValue(_scene_loader, property_value, _fbx_type, _type, &value)) {
      return false;
    }

    // Build and push keyframe
    const typename _Track::Keyframe key = {RawTrackInterpolation::kStep, 0.f,
                                           value};
    _track->keyframes.push_back(key);
  } else {
    const FixedRateSamplingTime fixed_it(_info.duration, _info.frequency);
    _track->keyframes.resize(fixed_it.num_keys());

    // Evaluate values at the specified time.
    for (size_t k = 0; k < fixed_it.num_keys(); ++k) {
      const float t = fixed_it.time(k);

      FbxPropertyValue& property_value = evaluator->GetPropertyValue(
          _property, FbxTimeSeconds(t + _info.start));

      // It shouldn't fail as property type is known.
      typename _Track::ValueType value;
      if (!GetValue(_scene_loader, property_value, _fbx_type, _type, &value)) {
        return false;
      }

      // Build and push keyframe
      const typename _Track::Keyframe key = {RawTrackInterpolation::kLinear,
                                             t / _info.duration, value};
      _track->keyframes[k] = key;
    }
  }

  return _track->Validate();
}

const char* FbxTypeToString(EFbxType _type) {
  switch (_type) {
    case eFbxUndefined:
      return "eFbxUndefined - Unidentified";
    case eFbxChar:
      return "eFbxChar - 8 bit signed integer";
    case eFbxUChar:
      return "eFbxUChar - 8 bit unsigned integer";
    case eFbxShort:
      return "eFbxShort - 16 bit signed integer";
    case eFbxUShort:
      return "eFbxUShort - 16 bit unsigned integer";
    case eFbxUInt:
      return "eFbxUInt - 32 bit unsigned integer";
    case eFbxLongLong:
      return "eFbxLongLong - 64 bit signed integer";
    case eFbxULongLong:
      return "eFbxULongLong - 64 bit unsigned integer";
    case eFbxHalfFloat:
      return "eFbxHalfFloat - 16 bit floating point";
    case eFbxBool:
      return "eFbxBool - Boolean";
    case eFbxInt:
      return "eFbxInt - 32 bit signed integer";
    case eFbxFloat:
      return "eFbxFloat - Floating point value";
    case eFbxDouble:
      return "eFbxDouble - Double width floating point value";
    case eFbxDouble2:
      return "eFbxDouble2 - Vector of two double values";
    case eFbxDouble3:
      return "eFbxDouble3 - Vector of three double values";
    case eFbxDouble4:
      return "eFbxDouble4 - Vector of four double values";
    case eFbxDouble4x4:
      return "eFbxDouble4x4 - Four vectors of four double values";
    case eFbxEnum:
      return "eFbxEnum - Enumeration";
    case eFbxEnumM:
      return "eFbxEnumM - Enumeration allowing duplicated items";
    case eFbxString:
      return "eFbxString - string";
    case eFbxTime:
      return "eFbxTime - Time value";
    case eFbxReference:
      return "eFbxReference - Reference to object or property";
    case eFbxBlob:
      return "eFbxBlob - Binary data block type";
    case eFbxDistance:
      return "eFbxDistance - Distance";
    case eFbxDateTime:
      return "eFbxDateTime - Date and time";
    default:
      return "Unknown";
  }
}

bool ExtractProperty(FbxSceneLoader& _scene_loader, const SamplingInfo& _info,
                     FbxProperty& _property,
                     OzzImporter::NodeProperty::Type _type,
                     RawFloatTrack* _track) {
  const EFbxType fbx_type = _property.GetPropertyDataType().GetType();
  switch (fbx_type) {
    case eFbxChar:
    case eFbxUChar:
    case eFbxShort:
    case eFbxUShort:
    case eFbxInt:
    case eFbxUInt:
    case eFbxBool:
    case eFbxFloat:
    case eFbxDouble:
    case eFbxEnum:
    case eFbxEnumM: {
      return ExtractCurve(_scene_loader, _property, fbx_type, _type, _info,
                          _track);
    }
    default: {
      log::Err() << "Float track can't be imported from a track of type: "
                 << FbxTypeToString(fbx_type) << "\"" << std::endl;
      return false;
    }
  }
}

bool ExtractProperty(FbxSceneLoader& _scene_loader, const SamplingInfo& _info,
                     FbxProperty& _property,
                     OzzImporter::NodeProperty::Type _type,
                     RawFloat2Track* _track) {
  const EFbxType fbx_type = _property.GetPropertyDataType().GetType();
  switch (fbx_type) {
    case eFbxDouble2: {
      return ExtractCurve(_scene_loader, _property, fbx_type, _type, _info,
                          _track);
    }
    default: {
      log::Err() << "Float2 track can't be imported from a track of type: "
                 << FbxTypeToString(fbx_type) << "\"" << std::endl;
      return false;
    }
  }
}

bool ExtractProperty(FbxSceneLoader& _scene_loader, const SamplingInfo& _info,
                     FbxProperty& _property,
                     OzzImporter::NodeProperty::Type _type,
                     RawFloat3Track* _track) {
  const EFbxType fbx_type = _property.GetPropertyDataType().GetType();
  switch (fbx_type) {
    case eFbxDouble3: {
      return ExtractCurve(_scene_loader, _property, fbx_type, _type, _info,
                          _track);
    }
    default: {
      log::Err() << "Float3 track can't be imported from a track of type: "
                 << FbxTypeToString(fbx_type) << "\"" << std::endl;
      return false;
    }
  }
}

bool ExtractProperty(FbxSceneLoader& _scene_loader, const SamplingInfo& _info,
                     FbxProperty& _property,
                     OzzImporter::NodeProperty::Type _type,
                     RawFloat4Track* _track) {
  const EFbxType fbx_type = _property.GetPropertyDataType().GetType();
  switch (fbx_type) {
    case eFbxDouble4: {
      return ExtractCurve(_scene_loader, _property, fbx_type, _type, _info,
                          _track);
    }
    default: {
      log::Err() << "Float4 track can't be imported from a track of type: "
                 << FbxTypeToString(fbx_type) << "\"" << std::endl;
      return false;
    }
  }
}

template <typename _Track>
bool ExtractTrackImpl(const char* _animation_name, const char* _node_name,
                      const char* _track_name,
                      OzzImporter::NodeProperty::Type _type,
                      FbxSceneLoader& _scene_loader, float _sampling_rate,
                      _Track* _track) {
  FbxScene* scene = _scene_loader.scene();
  assert(scene);

  // Reset track
  *_track = _Track();

  FbxAnimStack* anim_stack =
      scene->FindSrcObject<FbxAnimStack>(_animation_name);
  if (!anim_stack) {
    return false;
  }

  // Extract sampling info relative to the stack.
  const SamplingInfo& info =
      ExtractSamplingInfo(scene, anim_stack, _sampling_rate);

  FbxNode* node = scene->FindNodeByName(_node_name);
  if (!node) {
    ozz::log::Err() << "Invalid node name \"" << _node_name << "\""
                    << std::endl;
    return false;
  }

  FbxProperty property = node->FindProperty(_track_name);
  if (!property.IsValid()) {
    ozz::log::Err() << "Invalid property name \"" << _track_name << "\""
                    << std::endl;
    return false;
  }

  return ExtractProperty(_scene_loader, info, property, _type, _track);
}
}  // namespace

OzzImporter::AnimationNames GetAnimationNames(FbxSceneLoader& _scene_loader) {
  OzzImporter::AnimationNames names;

  const FbxScene* scene = _scene_loader.scene();
  for (int i = 0; i < scene->GetSrcObjectCount<FbxAnimStack>(); ++i) {
    FbxAnimStack* anim_stack = scene->GetSrcObject<FbxAnimStack>(i);
    names.push_back(ozz::string(anim_stack->GetName()));
  }

  return names;
}

bool ExtractAnimation(const char* _animation_name,
                      FbxSceneLoader& _scene_loader, const Skeleton& _skeleton,
                      float _sampling_rate,
                      ozz::animation::offline::RawAnimation* _animation) {
  FbxScene* scene = _scene_loader.scene();
  assert(scene);

  bool success = false;

  FbxAnimStack* anim_stack =
      scene->FindSrcObject<FbxAnimStack>(_animation_name);
  if (anim_stack) {
    // Extract sampling info relative to the stack.
    const SamplingInfo& info =
        ExtractSamplingInfo(scene, anim_stack, _sampling_rate);

    // Setup Fbx animation evaluator.
    scene->SetCurrentAnimationStack(anim_stack);

    success = ExtractAnimation(_scene_loader, info, _skeleton, _animation);
  }

  // Clears output if something failed during import, avoids partial data.
  if (!success) {
    *_animation = ozz::animation::offline::RawAnimation();
  }

  return success;
}

OzzImporter::NodeProperties GetNodeProperties(FbxSceneLoader& _scene_loader,
                                              const char* _node_name) {
  OzzImporter::NodeProperties properties;
  FbxScene* scene = _scene_loader.scene();
  const FbxNode* node = scene->FindNodeByName(_node_name);
  if (!node) {
    ozz::log::LogV() << "Invalid node name \"" << _node_name << "\""
                     << std::endl;
    return properties;
  }

  for (FbxProperty fbx_property = node->GetFirstProperty();
       fbx_property.IsValid();
       fbx_property = node->GetNextProperty(fbx_property)) {
    const char* ppt_name = fbx_property.GetNameAsCStr();

    const EFbxType type = fbx_property.GetPropertyDataType().GetType();
    switch (type) {
      case eFbxChar:
      case eFbxUChar:
      case eFbxShort:
      case eFbxUShort:
      case eFbxInt:
      case eFbxUInt:
      case eFbxBool:
      case eFbxFloat:
      case eFbxDouble:
      case eFbxEnum:
      case eFbxEnumM: {
        const OzzImporter::NodeProperty ppt = {
            ppt_name, OzzImporter::NodeProperty::kFloat1};
        properties.push_back(ppt);
        break;
      }
      case eFbxDouble2: {
        const OzzImporter::NodeProperty ppt = {
            ppt_name, OzzImporter::NodeProperty::kFloat2};
        properties.push_back(ppt);
        break;
      }
      case eFbxDouble3: {
        const OzzImporter::NodeProperty ppt = {
            ppt_name, OzzImporter::NodeProperty::kFloat3};
        properties.push_back(ppt);
        break;
      }
      case eFbxDouble4: {
        const OzzImporter::NodeProperty ppt = {
            ppt_name, OzzImporter::NodeProperty::kFloat4};
        properties.push_back(ppt);
        break;
      }
      default: {
        log::LogV() << "Node property \"" << ppt_name
                    << "\" doesn't have a importable type\""
                    << FbxTypeToString(type) << "\"" << std::endl;
        break;
      }
    }
  }
  return properties;
}

bool ExtractTrack(const char* _animation_name, const char* _node_name,
                  const char* _track_name,
                  OzzImporter::NodeProperty::Type _type,
                  FbxSceneLoader& _scene_loader, float _sampling_rate,
                  RawFloatTrack* _track) {
  return ExtractTrackImpl(_animation_name, _node_name, _track_name, _type,
                          _scene_loader, _sampling_rate, _track);
}

bool ExtractTrack(const char* _animation_name, const char* _node_name,
                  const char* _track_name,
                  OzzImporter::NodeProperty::Type _type,
                  FbxSceneLoader& _scene_loader, float _sampling_rate,
                  RawFloat2Track* _track) {
  return ExtractTrackImpl(_animation_name, _node_name, _track_name, _type,
                          _scene_loader, _sampling_rate, _track);
}

bool ExtractTrack(const char* _animation_name, const char* _node_name,
                  const char* _track_name,
                  OzzImporter::NodeProperty::Type _type,
                  FbxSceneLoader& _scene_loader, float _sampling_rate,
                  RawFloat3Track* _track) {
  return ExtractTrackImpl(_animation_name, _node_name, _track_name, _type,
                          _scene_loader, _sampling_rate, _track);
}

bool ExtractTrack(const char* _animation_name, const char* _node_name,
                  const char* _track_name,
                  OzzImporter::NodeProperty::Type _type,
                  FbxSceneLoader& _scene_loader, float _sampling_rate,
                  RawFloat4Track* _track) {
  return ExtractTrackImpl(_animation_name, _node_name, _track_name, _type,
                          _scene_loader, _sampling_rate, _track);
}
}  // namespace fbx
}  // namespace offline
}  // namespace animation
}  // namespace ozz
