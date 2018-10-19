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

#ifndef OZZ_OZZ_BASE_MATHS_SOA_MATH_ARCHIVE_H_
#define OZZ_OZZ_BASE_MATHS_SOA_MATH_ARCHIVE_H_

//CONFFX_BEGIN
#include "../../../../../../../OS/Math/MathTypes.h"

#include "../io/archive_traits.h"
#include "../platform.h"

namespace ozz {
namespace io {
OZZ_IO_TYPE_NOT_VERSIONABLE(SoaFloat2)
template <>
struct Extern<SoaFloat2> {
  static void Save(OArchive& _archive, const SoaFloat2* _values,
                   size_t _count);
  static void Load(IArchive& _archive, SoaFloat2* _values, size_t _count,
                   uint32_t _version);
};

OZZ_IO_TYPE_NOT_VERSIONABLE(SoaFloat3)
template <>
struct Extern<SoaFloat3> {
  static void Save(OArchive& _archive, const SoaFloat3* _values,
                   size_t _count);
  static void Load(IArchive& _archive, SoaFloat3* _values, size_t _count,
                   uint32_t _version);
};

OZZ_IO_TYPE_NOT_VERSIONABLE(SoaFloat4)
template <>
struct Extern<SoaFloat4> {
  static void Save(OArchive& _archive, const SoaFloat4* _values,
                   size_t _count);
  static void Load(IArchive& _archive, SoaFloat4* _values, size_t _count,
                   uint32_t _version);
};

OZZ_IO_TYPE_NOT_VERSIONABLE(SoaQuaternion)
template <>
struct Extern<SoaQuaternion> {
  static void Save(OArchive& _archive, const SoaQuaternion* _values,
                   size_t _count);
  static void Load(IArchive& _archive, SoaQuaternion* _values,
                   size_t _count, uint32_t _version);
};

OZZ_IO_TYPE_NOT_VERSIONABLE(SoaFloat4x4)
template <>
struct Extern<SoaFloat4x4> {
  static void Save(OArchive& _archive, const SoaFloat4x4* _values,
                   size_t _count);
  static void Load(IArchive& _archive, SoaFloat4x4* _values,
                   size_t _count, uint32_t _version);
};

OZZ_IO_TYPE_NOT_VERSIONABLE(SoaTransform)
template <>
struct Extern<SoaTransform> {
  static void Save(OArchive& _archive, const SoaTransform* _values,
                   size_t _count);
  static void Load(IArchive& _archive, SoaTransform* _values,
                   size_t _count, uint32_t _version);
};
}  // namespace io
}  // namespace ozz
//CONFFX_END

#endif  // OZZ_OZZ_BASE_MATHS_SOA_MATH_ARCHIVE_H_
