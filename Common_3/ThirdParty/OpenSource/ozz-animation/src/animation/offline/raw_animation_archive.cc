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

#include "ozz/animation/offline/raw_animation.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/maths/math_archive.h"

#include "ozz/base/containers/string_archive.h"
#include "ozz/base/containers/vector_archive.h"

namespace ozz {
namespace io {

void Extern<animation::offline::RawAnimation>::Save(
    OArchive& _archive, const animation::offline::RawAnimation* _animations,
    size_t _count) {
  for (size_t i = 0; i < _count; ++i) {
    const animation::offline::RawAnimation& animation = _animations[i];
    _archive << animation.duration;
    _archive << animation.tracks;
    _archive << animation.name;
  }
}
void Extern<animation::offline::RawAnimation>::Load(
    IArchive& _archive, animation::offline::RawAnimation* _animations,
    size_t _count, uint32_t _version) {
  (void)_version;
  for (size_t i = 0; i < _count; ++i) {
    animation::offline::RawAnimation& animation = _animations[i];
    _archive >> animation.duration;
    _archive >> animation.tracks;
    if (_version > 1) {
      _archive >> animation.name;
    }
  }
}

// RawAnimation::*Keys' version can be declared locally as it will be saved from
// this cpp file only.

OZZ_IO_TYPE_VERSION(1, animation::offline::RawAnimation::JointTrack)

template <>
struct Extern<animation::offline::RawAnimation::JointTrack> {
  static void Save(OArchive& _archive,
                   const animation::offline::RawAnimation::JointTrack* _tracks,
                   size_t _count) {
    for (size_t i = 0; i < _count; ++i) {
      const animation::offline::RawAnimation::JointTrack& track = _tracks[i];
      _archive << track.translations;
      _archive << track.rotations;
      _archive << track.scales;
    }
  }
  static void Load(IArchive& _archive,
                   animation::offline::RawAnimation::JointTrack* _tracks,
                   size_t _count, uint32_t _version) {
    (void)_version;
    for (size_t i = 0; i < _count; ++i) {
      animation::offline::RawAnimation::JointTrack& track = _tracks[i];
      _archive >> track.translations;
      _archive >> track.rotations;
      _archive >> track.scales;
    }
  }
};

OZZ_IO_TYPE_VERSION(1, animation::offline::RawAnimation::TranslationKey)

template <>
struct Extern<animation::offline::RawAnimation::TranslationKey> {
  static void Save(
      OArchive& _archive,
      const animation::offline::RawAnimation::TranslationKey* _keys,
      size_t _count) {
    for (size_t i = 0; i < _count; ++i) {
      const animation::offline::RawAnimation::TranslationKey& key = _keys[i];
      _archive << key.time;
      _archive << key.value;
    }
  }
  static void Load(IArchive& _archive,
                   animation::offline::RawAnimation::TranslationKey* _keys,
                   size_t _count, uint32_t _version) {
    (void)_version;
    for (size_t i = 0; i < _count; ++i) {
      animation::offline::RawAnimation::TranslationKey& key = _keys[i];
      _archive >> key.time;
      _archive >> key.value;
    }
  }
};

OZZ_IO_TYPE_VERSION(1, animation::offline::RawAnimation::RotationKey)

template <>
struct Extern<animation::offline::RawAnimation::RotationKey> {
  static void Save(OArchive& _archive,
                   const animation::offline::RawAnimation::RotationKey* _keys,
                   size_t _count) {
    for (size_t i = 0; i < _count; ++i) {
      const animation::offline::RawAnimation::RotationKey& key = _keys[i];
      _archive << key.time;
      _archive << key.value;
    }
  }
  static void Load(IArchive& _archive,
                   animation::offline::RawAnimation::RotationKey* _keys,
                   size_t _count, uint32_t _version) {
    (void)_version;
    for (size_t i = 0; i < _count; ++i) {
      animation::offline::RawAnimation::RotationKey& key = _keys[i];
      _archive >> key.time;
      _archive >> key.value;
    }
  }
};

OZZ_IO_TYPE_VERSION(1, animation::offline::RawAnimation::ScaleKey)

template <>
struct Extern<animation::offline::RawAnimation::ScaleKey> {
  static void Save(OArchive& _archive,
                   const animation::offline::RawAnimation::ScaleKey* _keys,
                   size_t _count) {
    for (size_t i = 0; i < _count; ++i) {
      const animation::offline::RawAnimation::ScaleKey& key = _keys[i];
      _archive << key.time;
      _archive << key.value;
    }
  }
  static void Load(IArchive& _archive,
                   animation::offline::RawAnimation::ScaleKey* _keys,
                   size_t _count, uint32_t _version) {
    (void)_version;
    for (size_t i = 0; i < _count; ++i) {
      animation::offline::RawAnimation::ScaleKey& key = _keys[i];
      _archive >> key.time;
      _archive >> key.value;
    }
  }
};
}  // namespace io
}  // namespace ozz
