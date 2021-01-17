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

#//---------------------------------------------------------------------------//
// Initial gltf2ozz implementation author: Alexander Dzhoganov                //
// https://github.com/guillaumeblanc/ozz-animation/pull/70                    //
//----------------------------------------------------------------------------//

#include <cassert>
#include <cstring>

#include "ozz/animation/offline/raw_animation_utils.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/containers/map.h"
#include "ozz/base/containers/set.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/simd_math.h"

#define TINYGLTF_IMPLEMENTATION

// No support for image loading or writing
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4702)  // unreachable code
#pragma warning(disable : 4267)  // conversion from 'size_t' to 'type'
#endif                           // _MSC_VER

#include "extern/tiny_gltf.h"

#ifdef _MSC_VER
#pragma warning(pop)
#endif  // _MSC_VER

namespace {

template <typename _VectorType>
bool FixupNames(_VectorType& _data, const char* _pretty_name,
                const char* _prefix_name) {
  ozz::set<std::string> names;
  for (size_t i = 0; i < _data.size(); ++i) {
    bool renamed = false;
    typename _VectorType::const_reference data = _data[i];

    std::string name(data.name.c_str());

    // Fixes unnamed animations.
    if (name.length() == 0) {
      renamed = true;
      name = _prefix_name;
      name += std::to_string(i);
    }

    // Fixes duplicated names, while it has duplicates
    for (auto it = names.find(name); it != names.end(); it = names.find(name)) {
      renamed = true;
      name += "_";
      name += std::to_string(i);
    }

    // Update names index.
    if (!names.insert(name).second) {
      assert(false && "Algorithm must ensure no duplicated animation names.");
    }

    if (renamed) {
      ozz::log::LogV() << _pretty_name << " #" << i << " with name \""
                       << data.name << "\" was renamed to \"" << name
                       << "\" in order to avoid duplicates." << std::endl;

      // Actually renames tinygltf data.
      _data[i].name = name;
    }
  }

  return true;
}

// Returns the address of a gltf buffer given an accessor.
// Performs basic checks to ensure the data is in the correct format
template <typename T>
ozz::span<const T> BufferView(const tinygltf::Model& _model,
                              const tinygltf::Accessor& _accessor) {
  const int32_t component_size =
      tinygltf::GetComponentSizeInBytes(_accessor.componentType);
  const int32_t element_size =
      component_size * tinygltf::GetTypeSizeInBytes(_accessor.type);
  if (element_size != sizeof(T)) {
    ozz::log::Err() << "Invalid buffer view access. Expected element size '"
                    << sizeof(T) << " got " << element_size << " instead."
                    << std::endl;
    return ozz::span<const T>();
  }

  const tinygltf::BufferView& bufferView =
      _model.bufferViews[_accessor.bufferView];
  const tinygltf::Buffer& buffer = _model.buffers[bufferView.buffer];
  const T* begin = reinterpret_cast<const T*>(
      buffer.data.data() + bufferView.byteOffset + _accessor.byteOffset);
  return ozz::span<const T>(begin, _accessor.count);
}

// Samples a linear animation channel
// There is an exact mapping between gltf and ozz keyframes so we just copy
// everything over.
template <typename _KeyframesType>
bool SampleLinearChannel(const tinygltf::Model& _model,
                         const tinygltf::Accessor& _output,
                         const ozz::span<const float>& _timestamps,
                         _KeyframesType* _keyframes) {
  const size_t gltf_keys_count = _output.count;

  if (gltf_keys_count == 0) {
    _keyframes->clear();
    return true;
  }

  typedef typename _KeyframesType::value_type::Value ValueType;
  const ozz::span<const ValueType> values =
      BufferView<ValueType>(_model, _output);
  if (values.size_bytes() / sizeof(ValueType) != gltf_keys_count ||
      _timestamps.size() != gltf_keys_count) {
    ozz::log::Err() << "gltf format error, inconsistent number of keys."
                    << std::endl;
    return false;
  }

  _keyframes->reserve(_output.count);
  for (size_t i = 0; i < _output.count; ++i) {
    const typename _KeyframesType::value_type key{_timestamps[i], values[i]};
    _keyframes->push_back(key);
  }

  return true;
}

// Samples a step animation channel
// There are twice-1 as many ozz keyframes as gltf keyframes
template <typename _KeyframesType>
bool SampleStepChannel(const tinygltf::Model& _model,
                       const tinygltf::Accessor& _output,
                       const ozz::span<const float>& _timestamps,
                       _KeyframesType* _keyframes) {
  const size_t gltf_keys_count = _output.count;

  if (gltf_keys_count == 0) {
    _keyframes->clear();
    return true;
  }

  typedef typename _KeyframesType::value_type::Value ValueType;
  const ozz::span<const ValueType> values =
      BufferView<ValueType>(_model, _output);
  if (values.size_bytes() / sizeof(ValueType) != gltf_keys_count ||
      _timestamps.size() != gltf_keys_count) {
    ozz::log::Err() << "gltf format error, inconsistent number of keys."
                    << std::endl;
    return false;
  }

  // A step is created with 2 consecutive keys. Last step is a single key.
  size_t numKeyframes = gltf_keys_count * 2 - 1;
  _keyframes->resize(numKeyframes);

  for (size_t i = 0; i < _output.count; i++) {
    typename _KeyframesType::reference key = _keyframes->at(i * 2);
    key.time = _timestamps[i];
    key.value = values[i];

    if (i < _output.count - 1) {
      typename _KeyframesType::reference next_key = _keyframes->at(i * 2 + 1);
      next_key.time = nexttowardf(_timestamps[i + 1], 0.f);
      next_key.value = values[i];
    }
  }

  return true;
}

// Samples a hermite spline in the form
// p(t) = (2t^3 - 3t^2 + 1)p0 + (t^3 - 2t^2 + t)m0 + (-2t^3 + 3t^2)p1 + (t^3 -
// t^2)m1 where t is a value between 0 and 1 p0 is the starting point at t = 0
// m0 is the scaled starting tangent at t = 0
// p1 is the ending point at t = 1
// m1 is the scaled ending tangent at t = 1
// p(t) is the resulting point value
template <typename T>
T SampleHermiteSpline(float _alpha, const T& p0, const T& m0, const T& p1,
                      const T& m1) {
  assert(_alpha >= 0.f && _alpha <= 1.f);

  const float t1 = _alpha;
  const float t2 = _alpha * _alpha;
  const float t3 = t2 * _alpha;

  // a = 2t^3 - 3t^2 + 1
  const float a = 2.0f * t3 - 3.0f * t2 + 1.0f;
  // b = t^3 - 2t^2 + t
  const float b = t3 - 2.0f * t2 + t1;
  // c = -2t^3 + 3t^2
  const float c = -2.0f * t3 + 3.0f * t2;
  // d = t^3 - t^2
  const float d = t3 - t2;

  // p(t) = a * p0 + b * m0 + c * p1 + d * m1
  T pt = p0 * a + m0 * b + p1 * c + m1 * d;
  return pt;
}

// Samples a cubic-spline channel
// the number of keyframes is determined from the animation duration and given
// sample rate
template <typename _KeyframesType>
bool SampleCubicSplineChannel(const tinygltf::Model& _model,
                              const tinygltf::Accessor& _output,
                              const ozz::span<const float>& _timestamps,
                              float _sampling_rate, float _duration,
                              _KeyframesType* _keyframes) {
  (void)_duration;

  assert(_output.count % 3 == 0);
  size_t gltf_keys_count = _output.count / 3;

  if (gltf_keys_count == 0) {
    _keyframes->clear();
    return true;
  }

  typedef typename _KeyframesType::value_type::Value ValueType;
  const ozz::span<const ValueType> values =
      BufferView<ValueType>(_model, _output);
  if (values.size_bytes() / (sizeof(ValueType) * 3) != gltf_keys_count ||
      _timestamps.size() != gltf_keys_count) {
    ozz::log::Err() << "gltf format error, inconsistent number of keys."
                    << std::endl;
    return false;
  }

  // Iterate keyframes at _sampling_rate steps, between first and last time
  // stamps.
  ozz::animation::offline::FixedRateSamplingTime fixed_it(
      _timestamps[gltf_keys_count - 1] - _timestamps[0], _sampling_rate);
  _keyframes->resize(fixed_it.num_keys());
  size_t cubic_key0 = 0;
  for (size_t k = 0; k < fixed_it.num_keys(); ++k) {
    const float time = fixed_it.time(k) + _timestamps[0];

    // Creates output key.
    typename _KeyframesType::value_type key;
    key.time = time;

    // Makes sure time is in between the correct cubic keyframes.
    while (_timestamps[cubic_key0 + 1] < time) {
      cubic_key0++;
    }
    assert(_timestamps[cubic_key0] <= time &&
           time <= _timestamps[cubic_key0 + 1]);

    // Interpolate cubic key
    const float t0 = _timestamps[cubic_key0];      // keyframe before time
    const float t1 = _timestamps[cubic_key0 + 1];  // keyframe after time
    const float alpha = (time - t0) / (t1 - t0);
    const ValueType& p0 = values[cubic_key0 * 3 + 1];
    const ValueType m0 = values[cubic_key0 * 3 + 2] * (t1 - t0);
    const ValueType& p1 = values[(cubic_key0 + 1) * 3 + 1];
    const ValueType m1 = values[(cubic_key0 + 1) * 3] * (t1 - t0);
    key.value = SampleHermiteSpline(alpha, p0, m0, p1, m1);

    // Pushes interpolated key.
    _keyframes->at(k) = key;
  }

  return true;
}

template <typename _KeyframesType>
bool SampleChannel(const tinygltf::Model& _model,
                   const std::string& _interpolation,
                   const tinygltf::Accessor& _output,
                   const ozz::span<const float>& _timestamps,
                   float _sampling_rate, float _duration,
                   _KeyframesType* _keyframes) {
  bool valid = false;
  if (_interpolation == "LINEAR") {
    valid = SampleLinearChannel(_model, _output, _timestamps, _keyframes);
  } else if (_interpolation == "STEP") {
    valid = SampleStepChannel(_model, _output, _timestamps, _keyframes);
  } else if (_interpolation == "CUBICSPLINE") {
    valid = SampleCubicSplineChannel(_model, _output, _timestamps,
                                     _sampling_rate, _duration, _keyframes);
  } else {
    ozz::log::Err() << "Invalid or unknown interpolation type '"
                    << _interpolation << "'." << std::endl;
    valid = false;
  }

  // Check if sorted (increasing time, might not be stricly increasing).
  if (valid) {
    valid = std::is_sorted(_keyframes->begin(), _keyframes->end(),
                           [](typename _KeyframesType::const_reference _a,
                              typename _KeyframesType::const_reference _b) {
                             return _a.time < _b.time;
                           });
    if (!valid) {
      ozz::log::Log()
          << "gltf format error, keyframes are not sorted in increasing order."
          << std::endl;
    }
  }

  // Remove keyframes with strictly equal times, keeping the first one.
  if (valid) {
    auto new_end = std::unique(_keyframes->begin(), _keyframes->end(),
                               [](typename _KeyframesType::const_reference _a,
                                  typename _KeyframesType::const_reference _b) {
                                 return _a.time == _b.time;
                               });
    if (new_end != _keyframes->end()) {
      _keyframes->erase(new_end, _keyframes->end());

      ozz::log::Log() << "gltf format error, keyframe times are not unique. "
                         "Imported data were modified to remove keyframes at "
                         "consecutive equivalent times."
                      << std::endl;
    }
  }
  return valid;
}

ozz::animation::offline::RawAnimation::TranslationKey
CreateTranslationBindPoseKey(const tinygltf::Node& _node) {
  ozz::animation::offline::RawAnimation::TranslationKey key;
  key.time = 0.0f;

  if (_node.translation.empty()) {
    key.value = ozz::math::Float3::zero();
  } else {
    key.value = ozz::math::Float3(static_cast<float>(_node.translation[0]),
                                  static_cast<float>(_node.translation[1]),
                                  static_cast<float>(_node.translation[2]));
  }

  return key;
}

ozz::animation::offline::RawAnimation::RotationKey CreateRotationBindPoseKey(
    const tinygltf::Node& _node) {
  ozz::animation::offline::RawAnimation::RotationKey key;
  key.time = 0.0f;

  if (_node.rotation.empty()) {
    key.value = ozz::math::Quaternion::identity();
  } else {
    key.value = ozz::math::Quaternion(static_cast<float>(_node.rotation[0]),
                                      static_cast<float>(_node.rotation[1]),
                                      static_cast<float>(_node.rotation[2]),
                                      static_cast<float>(_node.rotation[3]));
  }
  return key;
}

ozz::animation::offline::RawAnimation::ScaleKey CreateScaleBindPoseKey(
    const tinygltf::Node& _node) {
  ozz::animation::offline::RawAnimation::ScaleKey key;
  key.time = 0.0f;

  if (_node.scale.empty()) {
    key.value = ozz::math::Float3::one();
  } else {
    key.value = ozz::math::Float3(static_cast<float>(_node.scale[0]),
                                  static_cast<float>(_node.scale[1]),
                                  static_cast<float>(_node.scale[2]));
  }
  return key;
}

// Creates the default transform for a gltf node
bool CreateNodeTransform(const tinygltf::Node& _node,
                         ozz::math::Transform* _transform) {
  *_transform = ozz::math::Transform::identity();

  if (!_node.matrix.empty()) {
    const ozz::math::Float4x4 matrix = {
        {ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[0]),
                                      static_cast<float>(_node.matrix[1]),
                                      static_cast<float>(_node.matrix[2]),
                                      static_cast<float>(_node.matrix[3])),
         ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[4]),
                                      static_cast<float>(_node.matrix[5]),
                                      static_cast<float>(_node.matrix[6]),
                                      static_cast<float>(_node.matrix[7])),
         ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[8]),
                                      static_cast<float>(_node.matrix[9]),
                                      static_cast<float>(_node.matrix[10]),
                                      static_cast<float>(_node.matrix[11])),
         ozz::math::simd_float4::Load(static_cast<float>(_node.matrix[12]),
                                      static_cast<float>(_node.matrix[13]),
                                      static_cast<float>(_node.matrix[14]),
                                      static_cast<float>(_node.matrix[15]))}};
    ozz::math::SimdFloat4 translation, rotation, scale;
    if (ToAffine(matrix, &translation, &rotation, &scale)) {
      ozz::math::Store3PtrU(translation, &_transform->translation.x);
      ozz::math::StorePtrU(translation, &_transform->rotation.x);
      ozz::math::Store3PtrU(translation, &_transform->scale.x);
      return true;
    }

    ozz::log::Err() << "Failed to extract transformation from node \""
                    << _node.name << "\"." << std::endl;
    return false;
  }

  if (!_node.translation.empty()) {
    _transform->translation =
        ozz::math::Float3(static_cast<float>(_node.translation[0]),
                          static_cast<float>(_node.translation[1]),
                          static_cast<float>(_node.translation[2]));
  }
  if (!_node.rotation.empty()) {
    _transform->rotation =
        ozz::math::Quaternion(static_cast<float>(_node.rotation[0]),
                              static_cast<float>(_node.rotation[1]),
                              static_cast<float>(_node.rotation[2]),
                              static_cast<float>(_node.rotation[3]));
  }
  if (!_node.scale.empty()) {
    _transform->scale = ozz::math::Float3(static_cast<float>(_node.scale[0]),
                                          static_cast<float>(_node.scale[1]),
                                          static_cast<float>(_node.scale[2]));
  }

  return true;
}
}  // namespace

class GltfImporter : public ozz::animation::offline::OzzImporter {
 public:
  GltfImporter() {
    // We don't care about image data but we have to provide this callback
    // because we're not loading the stb library
    auto image_loader = [](tinygltf::Image*, const int, std::string*,
                           std::string*, int, int, const unsigned char*, int,
                           void*) { return true; };
    m_loader.SetImageLoader(image_loader, NULL);
  }

 private:
  bool Load(const char* _filename) override {
    bool success = false;
    std::string errors;
    std::string warnings;

    // Finds file extension.
    const char* separator = std::strrchr(_filename, '.');
    const char* ext = separator != NULL ? separator + 1 : "";

    // Tries to guess whether the input is a gltf json or a glb binary based on
    // the file extension
    if (std::strcmp(ext, "glb") == 0) {
      success =
          m_loader.LoadBinaryFromFile(&m_model, &errors, &warnings, _filename);
    } else {
      if (std::strcmp(ext, "gltf") != 0) {
        ozz::log::Log() << "Unknown file extension '" << ext
                        << "', assuming a JSON-formatted gltf." << std::endl;
      }

      success =
          m_loader.LoadASCIIFromFile(&m_model, &errors, &warnings, _filename);
    }

    // Prints any errors or warnings emitted by the loader
    if (!warnings.empty()) {
      ozz::log::Log() << "glTF parsing warnings: " << warnings << std::endl;
    }

    if (!errors.empty()) {
      ozz::log::Err() << "glTF parsing errors: " << errors << std::endl;
    }

    if (success) {
      ozz::log::Log() << "glTF parsed successfully." << std::endl;
    }

    if (success) {
      success &= FixupNames(m_model.scenes, "Scene", "scene_");
      success &= FixupNames(m_model.nodes, "Node", "node_");
      success &= FixupNames(m_model.animations, "Animation", "animation_");
    }

    return success;
  }

  // Given a skin find which of its joints is the skeleton root and return it
  // returns -1 if the skin has no associated joints
  int FindSkinRootJointIndex(const tinygltf::Skin& skin) {
    if (skin.joints.empty()) {
      return -1;
    }

    if (skin.skeleton != -1) {
      return skin.skeleton;
    }

    ozz::map<int, int> parents;
    for (int node : skin.joints) {
      for (int child : m_model.nodes[node].children) {
        parents[child] = node;
      }
    }

    int root = skin.joints[0];
    while (parents.find(root) != parents.end()) {
      root = parents[root];
    }

    return root;
  }

  bool Import(ozz::animation::offline::RawSkeleton* _skeleton,
              const NodeType& _types) override {
    (void)_types;

    if (m_model.scenes.empty()) {
      ozz::log::Err() << "No scenes found." << std::endl;
      return false;
    }

    // If no default scene has been set then take the first one spec does not
    // disallow gltfs without a default scene but it makes more sense to keep
    // going instead of throwing an error here
    int defaultScene = m_model.defaultScene;
    if (defaultScene == -1) {
      defaultScene = 0;
    }

    tinygltf::Scene& scene = m_model.scenes[defaultScene];
    ozz::log::LogV() << "Importing from default scene #" << defaultScene
                     << " with name \"" << scene.name << "\"." << std::endl;

    if (scene.nodes.empty()) {
      ozz::log::Err() << "Scene has no node." << std::endl;
      return false;
    }

    // Get all the skins belonging to this scene
    ozz::vector<int> roots;
    ozz::vector<tinygltf::Skin> skins = GetSkinsForScene(scene);
    if (skins.empty()) {
      ozz::log::Log() << "No skin exists in the scene, the whole scene graph "
                         "will be considered as a skeleton."
                      << std::endl;
      // Uses all scene nodes.
      for (auto& node : scene.nodes) {
        roots.push_back(node);
      }
    } else {
      if (skins.size() > 1) {
        ozz::log::Log() << "Multiple skins exist in the scene, they will all "
                           "be exported to a single skeleton."
                        << std::endl;
      }

      // Uses all skins root
      for (auto& skin : skins) {
        const int root = FindSkinRootJointIndex(skin);
        if (root == -1) {
          continue;
        }
        roots.push_back(root);
      }
    }

    // Remove nodes listed multiple times.
    std::sort(roots.begin(), roots.end());
    roots.erase(std::unique(roots.begin(), roots.end()), roots.end());

    // Traverses the scene graph and record all joints starting from the roots.
    _skeleton->roots.resize(roots.size());
    for (size_t i = 0; i < roots.size(); ++i) {
      const tinygltf::Node& root_node = m_model.nodes[roots[i]];
      ozz::animation::offline::RawSkeleton::Joint& root_joint =
          _skeleton->roots[i];
      if (!ImportNode(root_node, &root_joint)) {
        return false;
      }
    }

    if (!_skeleton->Validate()) {
      ozz::log::Err() << "Output skeleton failed validation. This is likely an "
                         "implementation issue."
                      << std::endl;
      return false;
    }

    return true;
  }

  // Recursively import a node's children
  bool ImportNode(const tinygltf::Node& _node,
                  ozz::animation::offline::RawSkeleton::Joint* _joint) {
    // Names joint.
    _joint->name = _node.name.c_str();

    // Fills transform.
    if (!CreateNodeTransform(_node, &_joint->transform)) {
      return false;
    }

    // Allocates all children at once.
    _joint->children.resize(_node.children.size());

    // Fills each child information.
    for (size_t i = 0; i < _node.children.size(); ++i) {
      const tinygltf::Node& child_node = m_model.nodes[_node.children[i]];
      ozz::animation::offline::RawSkeleton::Joint& child_joint =
          _joint->children[i];

      if (!ImportNode(child_node, &child_joint)) {
        return false;
      }
    }

    return true;
  }

  // Returns all animations in the gltf document.
  AnimationNames GetAnimationNames() override {
    AnimationNames animNames;
    for (size_t i = 0; i < m_model.animations.size(); ++i) {
      tinygltf::Animation& animation = m_model.animations[i];
      assert(animation.name.length() != 0);
      animNames.push_back(animation.name.c_str());
    }

    return animNames;
  }

  bool Import(const char* _animation_name,
              const ozz::animation::Skeleton& skeleton, float _sampling_rate,
              ozz::animation::offline::RawAnimation* _animation) override {
    if (_sampling_rate == 0.0f) {
      _sampling_rate = 30.0f;

      static bool samplingRateWarn = false;
      if (!samplingRateWarn) {
        ozz::log::LogV() << "The animation sampling rate is set to 0 "
                            "(automatic) but glTF does not carry scene frame "
                            "rate information. Assuming a sampling rate of "
                         << _sampling_rate << "hz." << std::endl;

        samplingRateWarn = true;
      }
    }

    // Find the corresponding gltf animation
    std::vector<tinygltf::Animation>::const_iterator gltf_animation =
        std::find_if(begin(m_model.animations), end(m_model.animations),
                     [_animation_name](const tinygltf::Animation& _animation) {
                       return _animation.name == _animation_name;
                     });
    assert(gltf_animation != end(m_model.animations));

    _animation->name = gltf_animation->name.c_str();

    // Animation duration is determined during sampling from the duration of the
    // longest channel
    _animation->duration = 0.0f;

    const int num_joints = skeleton.num_joints();
    _animation->tracks.resize(num_joints);

    // gltf stores animations by splitting them in channels
    // where each channel targets a node's property i.e. translation, rotation
    // or scale. ozz expects animations to be stored per joint so we create a
    // map where we record the associated channels for each joint
    ozz::cstring_map<std::vector<const tinygltf::AnimationChannel*>>
        channels_per_joint;

    for (const tinygltf::AnimationChannel& channel : gltf_animation->channels) {
      // Reject if no node is targetted.
      if (channel.target_node == -1) {
        continue;
      }

      // Reject if path isn't about skeleton animation.
      bool valid_target = false;
      for (const char* path : {"translation", "rotation", "scale"}) {
        valid_target |= channel.target_path == path;
      }
      if (!valid_target) {
        continue;
      }

      const tinygltf::Node& target_node = m_model.nodes[channel.target_node];
      channels_per_joint[target_node.name.c_str()].push_back(&channel);
    }

    // For each joint get all its associated channels, sample them and record
    // the samples in the joint track
    const ozz::span<const char* const> joint_names = skeleton.joint_names();
    for (int i = 0; i < num_joints; i++) {
      auto& channels = channels_per_joint[joint_names[i]];
      auto& track = _animation->tracks[i];

      for (auto& channel : channels) {
        auto& sampler = gltf_animation->samplers[channel->sampler];
        if (!SampleAnimationChannel(m_model, sampler, channel->target_path,
                                    _sampling_rate, &_animation->duration,
                                    &track)) {
          return false;
        }
      }

      const tinygltf::Node* node = FindNodeByName(joint_names[i]);
      assert(node != nullptr);

      // Pads the bind pose transform for any joints which do not have an
      // associated channel for this animation
      if (track.translations.empty()) {
        track.translations.push_back(CreateTranslationBindPoseKey(*node));
      }
      if (track.rotations.empty()) {
        track.rotations.push_back(CreateRotationBindPoseKey(*node));
      }
      if (track.scales.empty()) {
        track.scales.push_back(CreateScaleBindPoseKey(*node));
      }
    }

    ozz::log::LogV() << "Processed animation '" << _animation->name
                     << "' (tracks: " << _animation->tracks.size()
                     << ", duration: " << _animation->duration << "s)."
                     << std::endl;

    if (!_animation->Validate()) {
      ozz::log::Err() << "Animation '" << _animation->name
                      << "' failed validation." << std::endl;
      return false;
    }

    return true;
  }

  bool SampleAnimationChannel(
      const tinygltf::Model& _model, const tinygltf::AnimationSampler& _sampler,
      const std::string& _target_path, float _sampling_rate, float* _duration,
      ozz::animation::offline::RawAnimation::JointTrack* _track) {
    // Validate interpolation type.
    if (_sampler.interpolation.empty()) {
      ozz::log::Err() << "Invalid sampler interpolation." << std::endl;
      return false;
    }

    auto& input = m_model.accessors[_sampler.input];
    assert(input.maxValues.size() == 1);

    // The max[0] property of the input accessor is the animation duration
    // this is required to be present by the spec:
    // "Animation Sampler's input accessor must have min and max properties
    // defined."
    const float duration = static_cast<float>(input.maxValues[0]);

    // If this channel's duration is larger than the animation's duration
    // then increase the animation duration to match.
    if (duration > *_duration) {
      *_duration = duration;
    }

    assert(input.type == TINYGLTF_TYPE_SCALAR);
    auto& _output = m_model.accessors[_sampler.output];
    assert(_output.type == TINYGLTF_TYPE_VEC3 ||
           _output.type == TINYGLTF_TYPE_VEC4);

    const ozz::span<const float> timestamps = BufferView<float>(_model, input);
    if (timestamps.empty()) {
      return true;
    }

    // Builds keyframes.
    bool valid = false;
    if (_target_path == "translation") {
      valid =
          SampleChannel(m_model, _sampler.interpolation, _output, timestamps,
                        _sampling_rate, duration, &_track->translations);
    } else if (_target_path == "rotation") {
      valid =
          SampleChannel(m_model, _sampler.interpolation, _output, timestamps,
                        _sampling_rate, duration, &_track->rotations);
      if (valid) {
        // Normalize quaternions.
        for (auto& key : _track->rotations) {
          key.value = ozz::math::Normalize(key.value);
        }
      }
    } else if (_target_path == "scale") {
      valid =
          SampleChannel(m_model, _sampler.interpolation, _output, timestamps,
                        _sampling_rate, duration, &_track->scales);
    } else {
      assert(false && "Invalid target path");
    }

    return valid;
  }

  // Returns all skins belonging to a given gltf scene
  ozz::vector<tinygltf::Skin> GetSkinsForScene(
      const tinygltf::Scene& _scene) const {
    ozz::set<int> open;
    ozz::set<int> found;

    for (int nodeIndex : _scene.nodes) {
      open.insert(nodeIndex);
    }

    while (!open.empty()) {
      int nodeIndex = *open.begin();
      found.insert(nodeIndex);
      open.erase(nodeIndex);

      auto& node = m_model.nodes[nodeIndex];
      for (int childIndex : node.children) {
        open.insert(childIndex);
      }
    }

    ozz::vector<tinygltf::Skin> skins;
    for (const tinygltf::Skin& skin : m_model.skins) {
      if (!skin.joints.empty() && found.find(skin.joints[0]) != found.end()) {
        skins.push_back(skin);
      }
    }

    return skins;
  }

  const tinygltf::Node* FindNodeByName(const std::string& _name) const {
    for (const tinygltf::Node& node : m_model.nodes) {
      if (node.name == _name) {
        return &node;
      }
    }

    return nullptr;
  }

  // no support for user-defined tracks
  NodeProperties GetNodeProperties(const char*) override {
    return NodeProperties();
  }
  bool Import(const char*, const char*, const char*, NodeProperty::Type, float,
              ozz::animation::offline::RawFloatTrack*) override {
    return false;
  }
  bool Import(const char*, const char*, const char*, NodeProperty::Type, float,
              ozz::animation::offline::RawFloat2Track*) override {
    return false;
  }
  bool Import(const char*, const char*, const char*, NodeProperty::Type, float,
              ozz::animation::offline::RawFloat3Track*) override {
    return false;
  }
  bool Import(const char*, const char*, const char*, NodeProperty::Type, float,
              ozz::animation::offline::RawFloat4Track*) override {
    return false;
  }

  tinygltf::TinyGLTF m_loader;
  tinygltf::Model m_model;
};

int main(int _argc, const char** _argv) {
  GltfImporter converter;
  return converter(_argc, _argv);
}
