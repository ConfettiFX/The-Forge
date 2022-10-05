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

#include "../../../include/ozz/animation/runtime/skeleton.h"

#include <cstring>

#include "../../../include/ozz/base/io/archive.h"
#include "../../../include/ozz/base/maths/math_ex.h"
#include "../../../include/ozz/base/maths/soa_math_archive.h"
#include "../../../include/ozz/base/memory/allocator.h"

namespace ozz {
namespace animation {

Skeleton::Skeleton() {}

Skeleton::Skeleton(Skeleton&& _other) { *this = std::move(_other); }

Skeleton& Skeleton::operator=(Skeleton&& _other) {
  std::swap(joint_rest_poses_, _other.joint_rest_poses_);
  std::swap(joint_parents_, _other.joint_parents_);
  std::swap(joint_names_, _other.joint_names_);

  return *this;
}

char* Skeleton::Allocate(size_t _chars_size, size_t _num_joints) {
  // Distributes buffer memory while ensuring proper alignment (serves larger
  // alignment values first).
  static_assert(alignof(SoaTransform) >= alignof(char*) &&
                    alignof(char*) >= alignof(int16_t) &&
                    alignof(int16_t) >= alignof(char),
                "Must serve larger alignment values first)");

  ASSERT(joint_rest_poses_.size() == 0 && joint_names_.size() == 0 &&
         joint_parents_.size() == 0);

  // Early out if no joint.
  if (_num_joints == 0) {
    return NULL;
  }

  // Rest poses have SoA format
  const size_t num_soa_joints = (_num_joints + 3) / 4;
  const size_t joint_rest_poses_size = num_soa_joints * sizeof(SoaTransform);
  const size_t names_size = _num_joints * sizeof(char*);
  const size_t joint_parents_size = _num_joints * sizeof(int16_t);
  const size_t buffer_size =
      names_size + _chars_size + joint_parents_size + joint_rest_poses_size;

  // Allocates whole buffer.
  span<byte> buffer = {static_cast<byte*>(memory::default_allocator()->Allocate(
                           buffer_size, alignof(SoaTransform))),
                       buffer_size};

  // Serves larger alignment values first.
  // Rest pose first, biggest alignment.
  joint_rest_poses_ = fill_span<SoaTransform>(buffer, num_soa_joints);

  // Then names array, second biggest alignment.
  joint_names_ = fill_span<char*>(buffer, _num_joints);

  // Parents, third biggest alignment.
  joint_parents_ = fill_span<int16_t>(buffer, _num_joints);

  // Remaning buffer will be used to store joint names.
  ASSERT(buffer.size_bytes() == _chars_size &&
         "Whole buffer should be consumned");
  return reinterpret_cast<char*>(buffer.data());
}

void Skeleton::Deallocate() {
  memory::default_allocator()->Deallocate(
      as_writable_bytes(joint_rest_poses_).data());
  joint_rest_poses_ = {};
  joint_names_ = {};
  joint_parents_ = {};
}

void Skeleton::Save(ozz::io::OArchive& _archive) const {
  const int32_t num_joints = this->num_joints();

  // Early out if skeleton's empty.
  _archive << num_joints;
  if (!num_joints) {
    return;
  }

  // Stores names. They are all concatenated in the same buffer, starting at
  // joint_names_[0].
  size_t chars_count = 0;
  for (int32_t i = 0; i < num_joints; ++i) {
    chars_count += (strlen(joint_names_[i]) + 1) * sizeof(char);
  }
  _archive << static_cast<int32_t>(chars_count);
  _archive << ozz::io::MakeArray(joint_names_[0], chars_count);
  _archive << ozz::io::MakeArray(joint_parents_);
  _archive << ozz::io::MakeArray(joint_rest_poses_);
}

void Skeleton::Load(ozz::io::IArchive& _archive, uint32_t _version) {
  // Deallocate skeleton in case it was already used before.
  Deallocate();

  if (_version != 2) {
    LOGF(eERROR, "Unsupported Skeleton version %d.", _version);
    return;
  }

  int32_t num_joints;
  _archive >> num_joints;

  // Early out if skeleton's empty.
  if (!num_joints) {
    return;
  }

  // Read names.
  int32_t chars_count;
  _archive >> chars_count;

  // Allocates all skeleton data members.
  char* cursor = Allocate(chars_count, num_joints);

  // Reads name's buffer, they are all contiguous in the same buffer.
  _archive >> ozz::io::MakeArray(cursor, chars_count);

  // Fixes up array of pointers. Stops at num_joints - 1, so that it doesn't
  // read memory past the end of the buffer.
  for (int32_t i = 0; i < num_joints - 1; ++i) {
    joint_names_[i] = cursor;
    cursor += strlen(joint_names_[i]) + 1;
  }
  // num_joints is > 0, as this was tested at the beginning of the function.
  joint_names_[num_joints - 1] = cursor;

  _archive >> ozz::io::MakeArray(joint_parents_);
  _archive >> ozz::io::MakeArray(joint_rest_poses_);
}
}  // namespace animation
}  // namespace ozz
