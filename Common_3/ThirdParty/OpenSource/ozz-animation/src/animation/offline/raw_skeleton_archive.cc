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

#include "ozz/animation/offline/raw_skeleton.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/maths/math_archive.h"

#include "ozz/base/containers/string_archive.h"
#include "ozz/base/containers/vector_archive.h"

namespace ozz {
namespace io {

void Extern<animation::offline::RawSkeleton>::Save(
    OArchive& _archive, const animation::offline::RawSkeleton* _skeletons,
    size_t _count) {
  for (size_t i = 0; i < _count; ++i) {
    const animation::offline::RawSkeleton& skeleton = _skeletons[i];
    _archive << skeleton.roots;
  }
}
void Extern<animation::offline::RawSkeleton>::Load(
    IArchive& _archive, animation::offline::RawSkeleton* _skeletons,
    size_t _count, uint32_t _version) {
  (void)_version;
  for (size_t i = 0; i < _count; ++i) {
    animation::offline::RawSkeleton& skeleton = _skeletons[i];
    _archive >> skeleton.roots;
  }
}

// RawSkeleton::Joint' version can be declared locally as it will be saved from
// this cpp file only.
OZZ_IO_TYPE_VERSION(1, animation::offline::RawSkeleton::Joint)

template <>
struct Extern<animation::offline::RawSkeleton::Joint> {
  static void Save(OArchive& _archive,
                   const animation::offline::RawSkeleton::Joint* _joints,
                   size_t _count) {
    for (size_t i = 0; i < _count; ++i) {
      const animation::offline::RawSkeleton::Joint& joint = _joints[i];
      _archive << joint.name;
      _archive << joint.transform;
      _archive << joint.children;
    }
  }
  static void Load(IArchive& _archive,
                   animation::offline::RawSkeleton::Joint* _joints,
                   size_t _count, uint32_t _version) {
    (void)_version;
    for (size_t i = 0; i < _count; ++i) {
      animation::offline::RawSkeleton::Joint& joint = _joints[i];
      _archive >> joint.name;
      _archive >> joint.transform;
      _archive >> joint.children;
    }
  }
};
}  // namespace io
}  // namespace ozz
