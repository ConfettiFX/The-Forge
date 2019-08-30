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
#include "../../../include/ozz/animation/runtime/skeleton.h"
#include <cstring>
#include "../../../include/ozz/base/io/archive.h"
#include "../../../include/ozz/base/maths/math_ex.h"
#include "../../../include/ozz/base/maths/soa_math_archive.h"
#include "../../../include/ozz/base/memory/allocator.h"

#include "../../../../../Common_3/OS/Interfaces/ILog.h"
#include "../../../../../Common_3/OS/Math/MathTypes.h"

#include "../../EASTL/internal/char_traits.h"
//CONFFX_END

namespace ozz {
namespace io {
// JointProperties' version can be declared locally as it will be saved from
// this
// cpp file only.
OZZ_IO_TYPE_VERSION(1, animation::Skeleton::JointProperties)

// Specializes Skeleton::JointProperties. This structure's bitset isn't written
// as-is because of endianness issues.
template <>
struct Extern<animation::Skeleton::JointProperties> {
  static void Save(OArchive& _archive,
                   const animation::Skeleton::JointProperties* _properties,
                   size_t _count) {
    for (size_t i = 0; i < _count; ++i) {
      uint16_t parent = _properties[i].parent;
      _archive << parent;
      bool is_leaf = _properties[i].is_leaf != 0;
      _archive << is_leaf;
    }
  }
  static void Load(IArchive& _archive,
                   animation::Skeleton::JointProperties* _properties,
                   size_t _count, uint32_t _version) {
    (void)_version;
    for (size_t i = 0; i < _count; ++i) {
      uint16_t parent;
      _archive >> parent;
      _properties[i].parent = parent;
      bool is_leaf;
      _archive >> is_leaf;
      _properties[i].is_leaf = is_leaf;
    }
  }
};
}  // namespace io

namespace animation {

Skeleton::Skeleton() {}

Skeleton::~Skeleton() { 
	
	//Deallocate(); 

}

//CONFFX_BEGIN
char* Skeleton::Allocate(size_t _chars_size, size_t _num_joints) {
  // Distributes buffer memory while ensuring proper alignment (serves larger
  // alignment values first).
  OZZ_STATIC_ASSERT(
      OZZ_ALIGN_OF(SoaTransform) >= OZZ_ALIGN_OF(char*) &&
      OZZ_ALIGN_OF(char*) >= OZZ_ALIGN_OF(Skeleton::JointProperties) &&
      OZZ_ALIGN_OF(Skeleton::JointProperties) >= OZZ_ALIGN_OF(char));

  assert(bind_pose_.size() == 0 && joint_names_.size() == 0 &&
         joint_properties_.size() == 0);

  // Early out if no joint.
  if (_num_joints == 0) {
    return NULL;
  }

  // Bind poses have SoA format
  const size_t bind_poses_size =
      (_num_joints + 3) / 4 * sizeof(SoaTransform);
  const size_t names_size = _num_joints * sizeof(char*);
  const size_t properties_size =
      _num_joints * sizeof(Skeleton::JointProperties);
  const size_t buffer_size =
      names_size + _chars_size + properties_size + bind_poses_size;

  // Allocates whole buffer.
  char* buffer = reinterpret_cast<char*>(memory::default_allocator()->Allocate(
      buffer_size, OZZ_ALIGN_OF(SoaTransform)));

  // Serves larger alignment values first.
  // Bind pose first, biggest alignment.
  bind_pose_.begin = reinterpret_cast<SoaTransform*>(buffer);
  assert(math::IsAligned(bind_pose_.begin, OZZ_ALIGN_OF(SoaTransform)));
  buffer += bind_poses_size;
  bind_pose_.end = reinterpret_cast<SoaTransform*>(buffer);

  // Then names array, second biggest alignment.
  joint_names_.begin = reinterpret_cast<char**>(buffer);
  assert(math::IsAligned(joint_names_.begin, OZZ_ALIGN_OF(char**)));
  buffer += names_size;
  joint_names_.end = reinterpret_cast<char**>(buffer);

  // Properties, third biggest alignment.
  joint_properties_.begin =
      reinterpret_cast<Skeleton::JointProperties*>(buffer);
  assert(math::IsAligned(joint_properties_.begin,
                         OZZ_ALIGN_OF(Skeleton::JointProperties)));
  buffer += properties_size;
  joint_properties_.end = reinterpret_cast<Skeleton::JointProperties*>(buffer);

  // Remaning buffer will be used to store joint names.
  return buffer;
}
//CONFFX_END

void Skeleton::Deallocate() {
  memory::default_allocator()->Deallocate(bind_pose_.begin);
  bind_pose_.Clear();
  joint_names_.Clear();
  joint_properties_.Clear();
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
  for (int i = 0; i < num_joints; ++i) {
    chars_count += (eastl::CharStrlen(joint_names_[i]) + 1) * sizeof(char);
  }
  _archive << static_cast<int32_t>(chars_count);
  _archive << ozz::io::MakeArray(joint_names_[0], chars_count);

  // Stores joint's properties.
  _archive << ozz::io::MakeArray(joint_properties_);

  // Stores bind poses.
  _archive << ozz::io::MakeArray(bind_pose_);
}

void Skeleton::Load(ozz::io::IArchive& _archive, uint32_t _version) {
  // Deallocate skeleton in case it was already used before.
  Deallocate();

  if (_version != 1) {
    LOGF(LogLevel::eERROR, "Unsupported Skeleton version %d.", _version);
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
  for (int i = 0; i < num_joints - 1; ++i) {
    joint_names_[i] = cursor;
    cursor += eastl::CharStrlen(joint_names_[i]) + 1;
  }
  // num_joints is > 0, as this was tested at the beginning of the function.
  joint_names_[num_joints - 1] = cursor;

  _archive >> ozz::io::MakeArray(joint_properties_);
  _archive >> ozz::io::MakeArray(bind_pose_);
}
}  // namespace animation
}  // namespace ozz
