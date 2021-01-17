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

#include "animation/offline/tools/import2ozz_anim.h"

#include <json/json.h>

#include <cstdlib>
#include <cstring>

#include "animation/offline/tools/import2ozz_config.h"
#include "animation/offline/tools/import2ozz_track.h"
#include "ozz/animation/offline/additive_animation_builder.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/animation_optimizer.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/offline/tools/import2ozz.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/memory/unique_ptr.h"
#include "ozz/options/options.h"

namespace ozz {
namespace animation {
namespace offline {
namespace {

void DisplaysOptimizationstatistics(const RawAnimation& _non_optimized,
                                    const RawAnimation& _optimized) {
  size_t opt_translations = 0, opt_rotations = 0, opt_scales = 0;
  for (size_t i = 0; i < _optimized.tracks.size(); ++i) {
    const RawAnimation::JointTrack& track = _optimized.tracks[i];
    opt_translations += track.translations.size();
    opt_rotations += track.rotations.size();
    opt_scales += track.scales.size();
  }
  size_t non_opt_translations = 0, non_opt_rotations = 0, non_opt_scales = 0;
  for (size_t i = 0; i < _non_optimized.tracks.size(); ++i) {
    const RawAnimation::JointTrack& track = _non_optimized.tracks[i];
    non_opt_translations += track.translations.size();
    non_opt_rotations += track.rotations.size();
    non_opt_scales += track.scales.size();
  }

  // Computes optimization ratios.
  float translation_ratio = opt_translations != 0
                                ? 1.f * non_opt_translations / opt_translations
                                : 0.f;
  float rotation_ratio =
      opt_rotations != 0 ? 1.f * non_opt_rotations / opt_rotations : 0.f;
  float scale_ratio = opt_scales != 0 ? 1.f * non_opt_scales / opt_scales : 0.f;

  ozz::log::LogV log;
  ozz::log::FloatPrecision precision_scope(log, 1);
  log << "Optimization stage results:" << std::endl;
  log << " - Translations: " << translation_ratio << ":1" << std::endl;
  log << " - Rotations: " << rotation_ratio << ":1" << std::endl;
  log << " - Scales: " << scale_ratio << ":1" << std::endl;
}

unique_ptr<ozz::animation::Skeleton> LoadSkeleton(const char* _path) {
  // Reads the skeleton from the binary ozz stream.
  unique_ptr<ozz::animation::Skeleton> skeleton;
  {
    if (*_path == 0) {
      ozz::log::Err() << "Missing input skeleton file from json config."
                      << std::endl;
      return nullptr;
    }
    ozz::log::LogV() << "Opens input skeleton ozz binary file: " << _path
                     << std::endl;
    ozz::io::File file(_path, "rb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open input skeleton ozz binary file: \""
                      << _path << "\"" << std::endl;
      return nullptr;
    }
    ozz::io::IArchive archive(&file);

    // File could contain a RawSkeleton or a Skeleton.
    if (archive.TestTag<RawSkeleton>()) {
      ozz::log::LogV() << "Reading RawSkeleton from file." << std::endl;

      // Reading the skeleton cannot file.
      RawSkeleton raw_skeleton;
      archive >> raw_skeleton;

      // Builds runtime skeleton.
      ozz::log::LogV() << "Builds runtime skeleton." << std::endl;
      SkeletonBuilder builder;
      skeleton = builder(raw_skeleton);
      if (!skeleton) {
        ozz::log::Err() << "Failed to build runtime skeleton." << std::endl;
        return nullptr;
      }
    } else if (archive.TestTag<Skeleton>()) {
      // Reads input archive to the runtime skeleton.
      // This operation cannot fail.
      skeleton = make_unique<Skeleton>();
      archive >> *skeleton;
    } else {
      ozz::log::Err() << "Failed to read input skeleton from binary file: "
                      << _path << std::endl;
      return nullptr;
    }
  }
  return skeleton;
}

vector<math::Transform> SkeletonBindPoseSoAToAoS(const Skeleton& _skeleton) {
  // Copy skeleton bind pose to AoS form.
  vector<math::Transform> transforms(_skeleton.num_joints());
  for (int i = 0; i < _skeleton.num_soa_joints(); ++i) {
    const math::SoaTransform& soa_transform = _skeleton.joint_bind_poses()[i];
    math::SimdFloat4 translation[4];
    math::SimdFloat4 rotation[4];
    math::SimdFloat4 scale[4];
    math::Transpose3x4(&soa_transform.translation.x, translation);
    math::Transpose4x4(&soa_transform.rotation.x, rotation);
    math::Transpose3x4(&soa_transform.scale.x, scale);
    for (int j = 0; j < 4 && i * 4 + j < _skeleton.num_joints(); ++j) {
      math::Transform& out = transforms[i * 4 + j];
      math::Store3PtrU(translation[j], &out.translation.x);
      math::StorePtrU(rotation[j], &out.rotation.x);
      math::Store3PtrU(scale[j], &out.scale.x);
    }
  }
  return transforms;
}

bool Export(OzzImporter& _importer, const RawAnimation& _input_animation,
            const Skeleton& _skeleton, const Json::Value& _config,
            const ozz::Endianness _endianness) {
  // Raw animation to build and output. Initial setup is just a copy.
  RawAnimation raw_animation = _input_animation;

  // Optimizes animation if option is enabled.
  // Must be done before converting to additive, to be sure hierarchy length is
  // valid when optimizing.
  if (_config["optimize"].asBool()) {
    ozz::log::Log() << "Optimizing animation." << std::endl;
    AnimationOptimizer optimizer;

    // Setup optimizer from config parameters.
    const Json::Value& tolerances = _config["optimization_settings"];
    optimizer.setting.tolerance = tolerances["tolerance"].asFloat();
    optimizer.setting.distance = tolerances["distance"].asFloat();

    // Builds per joint settings.
    const Json::Value& joints_config = tolerances["override"];
    for (Json::ArrayIndex i = 0; i < joints_config.size(); ++i) {
      const Json::Value& joint_config = joints_config[i];

      // Prepares setting.
      AnimationOptimizer::Setting setting;
      setting.tolerance = joint_config["tolerance"].asFloat();
      setting.distance = joint_config["distance"].asFloat();

      // Push it for all matching joints.
      // Settings are overwritten if one has already been pushed.
      bool found = false;
      const char* name_pattern = joint_config["name"].asCString();
      for (int j = 0; j < _skeleton.num_joints(); ++j) {
        const char* joint_name = _skeleton.joint_names()[j];
        if (strmatch(joint_name, name_pattern)) {
          found = true;

          ozz::log::LogV() << "Found joint \"" << joint_name
                           << "\" matching pattern \"" << name_pattern
                           << "\" for joint optimization setting override."
                           << std::endl;

          const AnimationOptimizer::JointsSetting::value_type entry(j, setting);
          const bool newly =
              optimizer.joints_setting_override.insert(entry).second;
          if (!newly) {
            ozz::log::Log() << "Redundant optimization setting for pattern \""
                            << name_pattern << "\"" << std::endl;
          }
        }
      }

      if (!found) {
        ozz::log::Log()
            << "No joint found for optimization setting for pattern \""
            << name_pattern << "\"" << std::endl;
      }
    }

    RawAnimation raw_optimized_animation;
    if (!optimizer(raw_animation, _skeleton, &raw_optimized_animation)) {
      ozz::log::Err() << "Failed to optimize animation." << std::endl;
      return false;
    }

    // Displays optimization statistics.
    DisplaysOptimizationstatistics(raw_animation, raw_optimized_animation);

    // Brings data back to the raw animation.
    raw_animation = raw_optimized_animation;
  } else {
    ozz::log::LogV() << "Optimization for animation \"" << _input_animation.name
                     << "\" is disabled." << std::endl;
  }

  // Make delta animation if requested.
  if (_config["additive"].asBool()) {
    ozz::log::Log() << "Makes additive animation." << std::endl;

    AdditiveAnimationBuilder additive_builder;
    RawAnimation raw_additive;

    AdditiveReferenceEnum::Value reference;
    bool enum_found = AdditiveReference::GetEnumFromName(
        _config["additive_reference"].asCString(), &reference);
    assert(enum_found);  // Already checked on config side.

    bool succeeded = false;
    if (enum_found && reference == AdditiveReferenceEnum::kSkeleton) {
      const vector<math::Transform> transforms =
          SkeletonBindPoseSoAToAoS(_skeleton);
      succeeded =
          additive_builder(raw_animation, make_span(transforms), &raw_additive);
    } else {
      succeeded = additive_builder(raw_animation, &raw_additive);
    }

    if (!succeeded) {
      ozz::log::Err() << "Failed to make additive animation." << std::endl;
      return false;
    }

    // Now use additive animation.
    raw_animation = raw_additive;
  }

  // Builds runtime animation.
  unique_ptr<Animation> animation;
  if (!_config["raw"].asBool()) {
    ozz::log::Log() << "Builds runtime animation." << std::endl;
    AnimationBuilder builder;
    animation = builder(raw_animation);
    if (!animation) {
      ozz::log::Err() << "Failed to build runtime animation." << std::endl;
      return false;
    }
  }

  {
    // Prepares output stream. File is a RAII so it will close automatically
    // at the end of this scope. Once the file is opened, nothing should fail
    // as it would leave an invalid file on the disk.

    // Builds output filename.
    ozz::string filename = _importer.BuildFilename(
        _config["filename"].asCString(), raw_animation.name.c_str());

    ozz::log::LogV() << "Opens output file: \"" << filename << "\""
                     << std::endl;
    ozz::io::File file(filename.c_str(), "wb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open output file: \"" << filename << "\""
                      << std::endl;
      return false;
    }

    // Initializes output archive.
    ozz::io::OArchive archive(&file, _endianness);

    // Fills output archive with the animation.
    if (_config["raw"].asBool()) {
      ozz::log::Log() << "Outputs RawAnimation to binary archive." << std::endl;
      archive << raw_animation;
    } else {
      ozz::log::Log() << "Outputs Animation to binary archive." << std::endl;
      archive << *animation;
    }
  }

  ozz::log::LogV() << "Animation binary archive successfully outputted."
                   << std::endl;

  return true;
}  // namespace

bool ProcessAnimation(OzzImporter& _importer, const char* _animation_name,
                      const Skeleton& _skeleton, const Json::Value& _config,
                      const ozz::Endianness _endianness) {
  RawAnimation animation;

  ozz::log::Log() << "Extracting animation \"" << _animation_name << "\""
                  << std::endl;

  if (!_importer.Import(_animation_name, _skeleton,
                        _config["sampling_rate"].asFloat(), &animation)) {
    ozz::log::Err() << "Failed to import animation \"" << _animation_name
                    << "\"" << std::endl;
    return false;
  } else {
    // Give animation a name
    animation.name = _animation_name;

    return Export(_importer, animation, _skeleton, _config, _endianness);
  }
}
}  // namespace

AdditiveReference::EnumNames AdditiveReference::GetNames() {
  static const char* kNames[] = {"animation", "skeleton"};
  const EnumNames enum_names = {OZZ_ARRAY_SIZE(kNames), kNames};
  return enum_names;
}

bool ImportAnimations(const Json::Value& _config, OzzImporter* _importer,
                      const ozz::Endianness _endianness) {
  const Json::Value& skeleton_config = _config["skeleton"];
  const Json::Value& animations_config = _config["animations"];

  if (animations_config.size() == 0) {
    ozz::log::Log() << "Configuration contains no animation import "
                       "definition, animations import will be skipped."
                    << std::endl;
    return true;
  }

  // Get all available animation names.
  const OzzImporter::AnimationNames& import_animation_names =
      _importer->GetAnimationNames();

  // Are there animations available
  if (import_animation_names.empty()) {
    ozz::log::Err() << "No animation found." << std::endl;
    return true;
  }

  // Iterates all imported animations, build and output them.
  bool success = true;

  // Import skeleton instance.
  unique_ptr<Skeleton> skeleton(
      LoadSkeleton(skeleton_config["filename"].asCString()));
  success &= skeleton.get() != nullptr;

  // Loop though all existing animations, and export those who match
  // configuration.
  for (Json::ArrayIndex i = 0; success && i < animations_config.size(); ++i) {
    const Json::Value& animation_config = animations_config[i];
    const char* clip_match = animation_config["clip"].asCString();

    if (*clip_match == 0) {
      ozz::log::Log() << "No clip name provided. Animation import "
                         "will be skipped."
                      << std::endl;
      continue;
    }

    bool matched = false;
    for (size_t j = 0; success && j < import_animation_names.size(); ++j) {
      const char* animation_name = import_animation_names[j].c_str();
      if (!strmatch(animation_name, clip_match)) {
        continue;
      }

      matched = true;
      success = ProcessAnimation(*_importer, animation_name, *skeleton,
                                 animation_config, _endianness);

      const Json::Value& tracks_config = animation_config["tracks"];
      for (Json::ArrayIndex t = 0; success && t < tracks_config.size(); ++t) {
        success = ProcessTracks(*_importer, animation_name, *skeleton,
                                tracks_config[t], _endianness);
      }
    }
    // Don't display any message if no animation is supposed to be imported.
    if (!matched && *clip_match != 0) {
      ozz::log::Log() << "No matching animation found for \"" << clip_match
                      << "\"." << std::endl;
    }
  }

  return success;
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
