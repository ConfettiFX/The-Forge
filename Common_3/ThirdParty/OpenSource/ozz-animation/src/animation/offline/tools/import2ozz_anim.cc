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

#include "ozz/animation/offline/tools/import2ozz.h"

#include <cstdlib>
#include <cstring>

#include "animation/offline/tools/import2ozz_config.h"

#include "ozz/animation/offline/additive_animation_builder.h"
#include "ozz/animation/offline/animation_builder.h"
#include "ozz/animation/offline/animation_optimizer.h"
#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/raw_track.h"
#include "ozz/animation/offline/skeleton_builder.h"
#include "ozz/animation/offline/track_builder.h"
#include "ozz/animation/offline/track_optimizer.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/track.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

#include "ozz/base/log.h"

#include "ozz/options/options.h"

#include <json/json.h>

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
  float translation_ratio =
      non_opt_translations != 0
          ? 100.f * (non_opt_translations - opt_translations) /
                non_opt_translations
          : 0;
  float rotation_ratio =
      non_opt_rotations != 0
          ? 100.f * (non_opt_rotations - opt_rotations) / non_opt_rotations
          : 0;
  float scale_ratio =
      non_opt_scales != 0
          ? 100.f * (non_opt_scales - opt_scales) / non_opt_scales
          : 0;

  ozz::log::LogV() << "Optimization stage results:" << std::endl;
  ozz::log::LogV() << " - Translations key frames optimization: "
                   << translation_ratio << "%" << std::endl;
  ozz::log::LogV() << " - Rotations key frames optimization: " << rotation_ratio
                   << "%" << std::endl;
  ozz::log::LogV() << " - Scaling key frames optimization: " << scale_ratio
                   << "%" << std::endl;
}

ozz::animation::Skeleton* LoadSkeleton(const char* _path) {
  // Reads the skeleton from the binary ozz stream.
  ozz::animation::Skeleton* skeleton = NULL;
  {
    if (*_path == 0) {
      ozz::log::Err() << "Missing input skeleton file from json config."
                      << std::endl;
      return NULL;
    }
    ozz::log::LogV() << "Opens input skeleton ozz binary file: " << _path
                     << std::endl;
    ozz::io::File file(_path, "rb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open input skeleton ozz binary file: \""
                      << _path << "\"" << std::endl;
      return NULL;
    }
    ozz::io::IArchive archive(&file);

    // File could contain a RawSkeleton or a Skeleton.
    if (archive.TestTag<ozz::animation::offline::RawSkeleton>()) {
      ozz::log::LogV() << "Reading RawSkeleton from file." << std::endl;

      // Reading the skeleton cannot file.
      ozz::animation::offline::RawSkeleton raw_skeleton;
      archive >> raw_skeleton;

      // Builds runtime skeleton.
      ozz::log::LogV() << "Builds runtime skeleton." << std::endl;
      ozz::animation::offline::SkeletonBuilder builder;
      skeleton = builder(raw_skeleton);
      if (!skeleton) {
        ozz::log::Err() << "Failed to build runtime skeleton." << std::endl;
        return NULL;
      }
    } else if (archive.TestTag<ozz::animation::Skeleton>()) {
      // Reads input archive to the runtime skeleton.
      // This operation cannot fail.
      skeleton =
          ozz::memory::default_allocator()->New<ozz::animation::Skeleton>();
      archive >> *skeleton;
    } else {
      ozz::log::Err() << "Failed to read input skeleton from binary file: "
                      << _path << std::endl;
      return NULL;
    }
  }
  return skeleton;
}

ozz::String::Std BuildFilename(const char* _filename, const char* _data_name) {
  ozz::String::Std output(_filename);

  for (size_t asterisk = output.find('*'); asterisk != std::string::npos;
       asterisk = output.find('*')) {
    output.replace(asterisk, 1, _data_name);
  }
  return output;
}

bool Export(const ozz::animation::offline::RawAnimation& _raw_animation,
            const ozz::animation::Skeleton& _skeleton,
            const Json::Value& _config, const ozz::Endianness _endianness) {
  // Raw animation to build and output.
  ozz::animation::offline::RawAnimation raw_animation;

  // Make delta animation if requested.
  if (_config["additive"].asBool()) {
    ozz::log::Log() << "Makes additive animation." << std::endl;
    ozz::animation::offline::AdditiveAnimationBuilder additive_builder;
    RawAnimation raw_additive;
    if (!additive_builder(_raw_animation, &raw_additive)) {
      ozz::log::Err() << "Failed to make additive animation." << std::endl;
      return false;
    }
    // checker animation.
    raw_animation = raw_additive;
  } else {
    raw_animation = _raw_animation;
  }

  // Optimizes animation if option is enabled.
  if (_config["optimize"].asBool()) {
    ozz::log::Log() << "Optimizing animation." << std::endl;
    ozz::animation::offline::AnimationOptimizer optimizer;

    // Setup optimizer from config parameters.
    const Json::Value& tolerances = _config["optimization_tolerances"];
    optimizer.translation_tolerance = tolerances["translation"].asFloat();
    optimizer.rotation_tolerance = tolerances["rotation"].asFloat();
    optimizer.scale_tolerance = tolerances["scale"].asFloat();
    optimizer.hierarchical_tolerance = tolerances["hierarchical"].asFloat();

    ozz::animation::offline::RawAnimation raw_optimized_animation;
    if (!optimizer(raw_animation, _skeleton, &raw_optimized_animation)) {
      ozz::log::Err() << "Failed to optimize animation." << std::endl;
      return false;
    }

    // Displays optimization statistics.
    DisplaysOptimizationstatistics(raw_animation, raw_optimized_animation);

    // Brings data back to the raw animation.
    raw_animation = raw_optimized_animation;
  }

  // Builds runtime animation.
  ozz::animation::Animation* animation = NULL;
  if (!_config["raw"].asBool()) {
    ozz::log::Log() << "Builds runtime animation." << std::endl;
    ozz::animation::offline::AnimationBuilder builder;
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
    ozz::String::Std filename = BuildFilename(_config["filename"].asCString(),
                                              _raw_animation.name.c_str());

    ozz::log::LogV() << "Opens output file: " << filename << std::endl;
    ozz::io::File file(filename.c_str(), "wb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open output file: \"" << filename << "\""
                      << std::endl;
      ozz::memory::default_allocator()->Delete(animation);
      return false;
    }

    // Initializes output archive.
    ozz::io::OArchive archive(&file, _endianness);

    // Fills output archive with the animation.
    if (_config["raw"].asBool()) {
      ozz::log::Log() << "Outputs RawAnimation to binary archive." << std::endl;
      archive << raw_animation;
    } else {
      ozz::log::LogV() << "Outputs Animation to binary archive." << std::endl;
      archive << *animation;
    }
  }

  ozz::log::LogV() << "Animation binary archive successfully outputted."
                   << std::endl;

  // Delete local objects.
  ozz::memory::default_allocator()->Delete(animation);

  return true;
}

bool ProcessAnimation(OzzImporter& _converter, const char* _animation_name,
                      const ozz::animation::Skeleton& _skeleton,
                      const Json::Value& _config,
                      const ozz::Endianness _endianness) {
  RawAnimation animation;
  if (!_converter.Import(_animation_name, _skeleton,
                         _config["sampling_rate"].asFloat(), &animation)) {
    ozz::log::Err() << "Failed to import animation \"" << _animation_name
                    << "\"" << std::endl;
    return false;
  } else {
    // Give animation a name
    animation.name = _animation_name;

    return Export(animation, _skeleton, _config, _endianness);
  }
}

template <typename _RawTrack>
struct RawTrackToTrack;

template <>
struct RawTrackToTrack<RawFloatTrack> {
  typedef ozz::animation::FloatTrack Track;
};
template <>
struct RawTrackToTrack<RawFloat2Track> {
  typedef ozz::animation::Float2Track Track;
};
template <>
struct RawTrackToTrack<RawFloat3Track> {
  typedef ozz::animation::Float3Track Track;
};
template <>
struct RawTrackToTrack<RawFloat4Track> {
  typedef ozz::animation::Float4Track Track;
};

template <typename _RawTrack>
bool Export(const _RawTrack& _raw_track, const Json::Value& _config,
            const ozz::Endianness _endianness) {
  // Raw track to build and output.
  _RawTrack raw_track;

  // Optimizes track if option is enabled.
  if (_config["optimize"].asBool()) {
    ozz::log::LogV() << "Optimizing track." << std::endl;
    ozz::animation::offline::TrackOptimizer optimizer;
    optimizer.tolerance = _config["optimization_tolerance"].asFloat();
    _RawTrack raw_optimized_track;
    if (!optimizer(_raw_track, &raw_optimized_track)) {
      ozz::log::Err() << "Failed to optimize track." << std::endl;
      return false;
    }

    // Displays optimization statistics.
    // DisplaysOptimizationstatistics(raw_animation, raw_optimized_animation);

    // Brings data back to the raw track.
    raw_track = raw_optimized_track;
  }

  // Builds runtime track.
  typename RawTrackToTrack<_RawTrack>::Track* track = NULL;
  if (!_config["raw"].asBool()) {
    ozz::log::LogV() << "Builds runtime track." << std::endl;
    ozz::animation::offline::TrackBuilder builder;
    track = builder(raw_track);
    if (!track) {
      ozz::log::Err() << "Failed to build runtime track." << std::endl;
      return false;
    }
  }

  {
    // Prepares output stream. Once the file is opened, nothing should fail as
    // it would leave an invalid file on the disk.

    // Builds output filename.
    const ozz::String::Std filename =
        BuildFilename(_config["filename"].asCString(), _raw_track.name.c_str());

    ozz::log::LogV() << "Opens output file: " << filename << std::endl;
    ozz::io::File file(filename.c_str(), "wb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open output file: " << filename
                      << std::endl;
      ozz::memory::default_allocator()->Delete(track);
      return false;
    }

    // Initializes output archive.
    ozz::io::OArchive archive(&file, _endianness);

    // Fills output archive with the track.
    if (_config["raw"].asBool()) {
      ozz::log::LogV() << "Outputs RawTrack to binary archive." << std::endl;
      archive << raw_track;
    } else {
      ozz::log::LogV() << "Outputs Track to binary archive." << std::endl;
      archive << *track;
    }
  }

  ozz::log::LogV() << "Track binary archive successfully outputted."
                   << std::endl;

  // Delete local objects.
  ozz::memory::default_allocator()->Delete(track);

  return true;
}

template <OzzImporter::NodeProperty::Type _type>
struct TrackFromType;

template <>
struct TrackFromType<OzzImporter::NodeProperty::kFloat1> {
  typedef RawFloatTrack RawTrack;
};
template <>
struct TrackFromType<OzzImporter::NodeProperty::kFloat2> {
  typedef RawFloat2Track RawTrack;
};
template <>
struct TrackFromType<OzzImporter::NodeProperty::kFloat3> {
  typedef RawFloat3Track RawTrack;
};
template <>
struct TrackFromType<OzzImporter::NodeProperty::kFloat4> {
  typedef RawFloat4Track RawTrack;
};

template <OzzImporter::NodeProperty::Type _type>
bool ProcessImportTrackType(OzzImporter& _converter,
                            const char* _animation_name,
                            const char* _joint_name,
                            const OzzImporter::NodeProperty& _property,
                            const Json::Value& _import_config,
                            const ozz::Endianness _endianness) {
  bool success = true;

  typename TrackFromType<_type>::RawTrack track;
  success &= _converter.Import(_animation_name, _joint_name,
                               _property.name.c_str(), 0, &track);

  if (success) {
    // Give the track a name
    track.name = _joint_name;
    track.name += '-';
    track.name += _property.name.c_str();

    success &= Export(track, _import_config, _endianness);
  } else {
    ozz::log::Err() << "Failed to import track \"" << _joint_name << ":"
                    << _property.name << "\"" << std::endl;
  }

  return success;
}

bool ProcessImportTrack(OzzImporter& _converter, const char* _animation_name,
                        const Skeleton& _skeleton,
                        const Json::Value& _import_config,
                        const ozz::Endianness _endianness) {
  // Early out if no name is specified
  const char* joint_name_match = _import_config["joint_name"].asCString();
  const char* ppt_name_match = _import_config["property_name"].asCString();

  // Process every joint that matches.
  bool success = true;
  bool joint_found = false;
  for (int s = 0; success && s < _skeleton.num_joints(); ++s) {
    const char* joint_name = _skeleton.joint_names()[s];
    if (!strmatch(joint_name, joint_name_match)) {
      continue;
    }
    joint_found = true;

    // Node found, need to find matching properties now.
    bool ppt_found = false;
    const OzzImporter::NodeProperties properties =
        _converter.GetNodeProperties(joint_name);
    for (size_t p = 0; p < properties.size(); ++p) {
      const OzzImporter::NodeProperty& property = properties[p];
      // Checks property name matches
      const char* property_name = property.name.c_str();
      if (!strmatch(property_name, ppt_name_match)) {
        continue;
      }
      // Checks property type matches
      const int property_type = _import_config["type"].asInt();
      if (property_type != property.type) {
        ozz::log::Log() << "Incompatible type \"" << property_type
                        << "\" for matching property \"" << joint_name << ":"
                        << property_name << "\" of type \"" << property.type
                        << "\"." << std::endl;
        continue;
      }

      // A property has been found.
      ppt_found = true;

      // Import property depending on its type.
      switch (property.type) {
        case OzzImporter::NodeProperty::kFloat1: {
          success &= ProcessImportTrackType<OzzImporter::NodeProperty::kFloat1>(
              _converter, _animation_name, joint_name, property, _import_config,
              _endianness);
          break;
        }
        case OzzImporter::NodeProperty::kFloat2: {
          success &= ProcessImportTrackType<OzzImporter::NodeProperty::kFloat2>(
              _converter, _animation_name, joint_name, property, _import_config,
              _endianness);
          break;
        }
        case OzzImporter::NodeProperty::kFloat3: {
          success &= ProcessImportTrackType<OzzImporter::NodeProperty::kFloat3>(
              _converter, _animation_name, joint_name, property, _import_config,
              _endianness);
          break;
        }
        case OzzImporter::NodeProperty::kFloat4: {
          success &= ProcessImportTrackType<OzzImporter::NodeProperty::kFloat4>(
              _converter, _animation_name, joint_name, property, _import_config,
              _endianness);
          break;
        }
        default: {
          assert(false && "Unknown property type.");
          success = false;
          break;
        }
      }
    }

    if (!ppt_found) {
      ozz::log::Log() << "No property found for track import definition \""
                      << joint_name_match << ":" << ppt_name_match << "\"."
                      << std::endl;
    }
  }

  if (!joint_found) {
    ozz::log::Log() << "No joint found for track import definition \""
                    << joint_name_match << "\"." << std::endl;
  }

  return success;
}

/*
bool ProcessMotionTrack(OzzImporter& _converter,
                        const char* _animation_name, const Skeleton& _skeleton,
                        const Json::Value& _motion) {
  return true;
}*/

bool ProcessTracks(OzzImporter& _converter, const char* _animation_name,
                   const Skeleton& _skeleton, const Json::Value& _config,
                   const ozz::Endianness _endianness) {
  bool success = true;

  const Json::Value& imports = _config["properties"];
  for (Json::ArrayIndex i = 0; success && i < imports.size(); ++i) {
    success &= ProcessImportTrack(_converter, _animation_name, _skeleton,
                                  imports[i], _endianness);
  }

  /*
    const Json::Value& motions = _config["motions"];
    for (Json::ArrayIndex i = 0; success && i < motions.size(); ++i) {
      success &=
          ProcessMotionTrack(_converter, _animation_name, _skeleton,
    motions[i]);
    }*/

  return success;
}
}  // namespace

bool ImportAnimations(const Json::Value& _config, OzzImporter* _converter,
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
      _converter->GetAnimationNames();

  // Are there animations available
  if (import_animation_names.empty()) {
    ozz::log::Err() << "No animation found." << std::endl;
    return true;
  }

  // Iterates all imported animations, build and output them.
  bool success = true;

  // Import skeleton instance.
  ozz::animation::Skeleton* skeleton =
      LoadSkeleton(skeleton_config["filename"].asCString());
  success &= skeleton != NULL;

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
      success = ProcessAnimation(*_converter, animation_name, *skeleton,
                                 animation_config, _endianness);

      const Json::Value& tracks_config = animation_config["tracks"];
      for (Json::ArrayIndex t = 0; success && t < tracks_config.size(); ++t) {
        success = ProcessTracks(*_converter, animation_name, *skeleton,
                                tracks_config[t], _endianness);
      }
    }
    // Don't display any message if no animation is supposed to be imported.
    if (!matched && *clip_match != 0) {
      ozz::log::Log() << "No matching animation found for \"" << clip_match
                      << "\"." << std::endl;
    }
  }

  ozz::memory::default_allocator()->Delete(skeleton);

  return success;
}  // namespace animation
}  // namespace offline
}  // namespace animation
}  // namespace ozz
