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

#include "animation/offline/tools/import2ozz_skel.h"

#include <cstdlib>
#include <cstring>
#include <iomanip>

#include "animation/offline/tools/import2ozz_config.h"

#include "ozz/animation/offline/tools/import2ozz.h"

#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/offline/skeleton_builder.h"

#include "ozz/animation/runtime/skeleton.h"

#include "ozz/base/containers/map.h"
#include "ozz/base/containers/set.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

#include "ozz/base/log.h"

#include <json/json.h>

namespace ozz {
namespace animation {
namespace offline {
namespace {

// Uses a set to detect names uniqueness.
typedef ozz::Set<const char*, ozz::str_less>::Std Names;

bool ValidateJointNamesUniquenessRecurse(
    const RawSkeleton::Joint::Children& _joints, Names* _names) {
  for (size_t i = 0; i < _joints.size(); ++i) {
    const RawSkeleton::Joint& joint = _joints[i];
    const char* name = joint.name.c_str();
    if (!_names->insert(name).second) {
      ozz::log::Err()
          << "Skeleton contains at least one non-unique joint name \"" << name
          << "\", which is not supported." << std::endl;
      return false;
    }
    if (!ValidateJointNamesUniquenessRecurse(_joints[i].children, _names)) {
      return false;
    }
  }
  return true;
}

bool ValidateJointNamesUniqueness(const RawSkeleton& _skeleton) {
  Names joint_names;
  return ValidateJointNamesUniquenessRecurse(_skeleton.roots, &joint_names);
}

void LogHierarchy(const RawSkeleton::Joint::Children& _children,
                  int _depth = 0) {
  const std::streamsize pres = ozz::log::LogV().stream().precision();
  for (size_t i = 0; i < _children.size(); ++i) {
    const RawSkeleton::Joint& joint = _children[i];
    ozz::log::LogV() << std::setw(_depth) << std::setfill('.') << "";
    ozz::log::LogV() << joint.name.c_str() << std::setprecision(4)
                     << " t: " << joint.transform.translation.x << ", "
                     << joint.transform.translation.y << ", "
                     << joint.transform.translation.z
                     << " r: " << joint.transform.rotation.x << ", "
                     << joint.transform.rotation.y << ", "
                     << joint.transform.rotation.z << ", "
                     << joint.transform.rotation.w
                     << " s: " << joint.transform.scale.x << ", "
                     << joint.transform.scale.y << ", "
                     << joint.transform.scale.z << std::endl;

    // Recurse
    LogHierarchy(joint.children, _depth + 1);
  }
  ozz::log::LogV() << std::setprecision(pres);
}
}  // namespace

bool ImportSkeleton(const Json::Value& _config, OzzImporter* _converter,
                    const ozz::Endianness _endianness) {
  const Json::Value& skeleton_config = _config["skeleton"];
  const Json::Value& import_config = skeleton_config["import"];

  // First check that we're actually expecting to import a skeleton.
  if (!import_config["enable"].asBool()) {
    ozz::log::Log() << "Skeleton build disabled, import will be skipped."
                    << std::endl;
    return true;
  }

  // Setup node types import properties.
  const Json::Value& types_config = import_config["types"];
  OzzImporter::NodeType types = {0};
  types.skeleton = types_config["skeleton"].asBool();
  types.marker = types_config["marker"].asBool();
  types.camera = types_config["camera"].asBool();
  types.geometry = types_config["geometry"].asBool();
  types.light = types_config["light"].asBool();
  types.any = types_config["any"].asBool();

  ozz::animation::offline::RawSkeleton raw_skeleton;
  if (!_converter->Import(&raw_skeleton, types)) {
    ozz::log::Err() << "Failed to import skeleton." << std::endl;
    return false;
  }

  // Log skeleton hierarchy
  if (ozz::log::GetLevel() == ozz::log::kVerbose) {
    LogHierarchy(raw_skeleton.roots);
  }

  // Non unique joint names are not supported.
  if (!(ValidateJointNamesUniqueness(raw_skeleton))) {
    // Log Err is done by the validation function.
    return false;
  }

  // Needs to be done before opening the output file, so that if it fails then
  // there's no invalid file outputted.
  ozz::animation::Skeleton* skeleton = NULL;
  if (!import_config["raw"].asBool()) {
    // Builds runtime skeleton.
    ozz::log::Log() << "Builds runtime skeleton." << std::endl;
    ozz::animation::offline::SkeletonBuilder builder;
    skeleton = builder(raw_skeleton);
    if (!skeleton) {
      ozz::log::Err() << "Failed to build runtime skeleton." << std::endl;
      return false;
    }
  }

  // Prepares output stream. File is a RAII so it will close automatically at
  // the end of this scope.
  // Once the file is opened, nothing should fail as it would leave an invalid
  // file on the disk.
  {
    const char* filename = skeleton_config["filename"].asCString();
    ozz::log::Log() << "Opens output file: " << filename << std::endl;
    ozz::io::File file(filename, "wb");
    if (!file.opened()) {
      ozz::log::Err() << "Failed to open output file: \"" << filename << "\"."
                      << std::endl;
      ozz::memory::default_allocator()->Delete(skeleton);
      return false;
    }

    // Initializes output archive.
    ozz::io::OArchive archive(&file, _endianness);

    // Fills output archive with the skeleton.
    if (import_config["raw"].asBool()) {
      ozz::log::Log() << "Outputs RawSkeleton to binary archive." << std::endl;
      archive << raw_skeleton;
    } else {
      ozz::log::Log() << "Outputs Skeleton to binary archive." << std::endl;
      archive << *skeleton;
    }
    ozz::log::Log() << "Skeleton binary archive successfully outputted."
                    << std::endl;
  }

  // Delete local objects.
  ozz::memory::default_allocator()->Delete(skeleton);

  return true;
}
}  // namespace offline
}  // namespace animation
}  // namespace ozz
