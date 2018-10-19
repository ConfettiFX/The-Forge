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

#ifndef OZZ_OZZ_BASE_MATHS_SIMD_MATH_ARCHIVE_H_
#define OZZ_OZZ_BASE_MATHS_SIMD_MATH_ARCHIVE_H_

//CONFFX_BEGIN
#include "../../../../../../../OS/Math/MathTypes.h"

#include "../io/archive_traits.h"
#include "../platform.h"

namespace ozz {
namespace io {
OZZ_IO_TYPE_NOT_VERSIONABLE(Vector4)
template <>
struct Extern<Vector4> {
  static void Save(OArchive& _archive, const Vector4* _values,
                   size_t _count);
  static void Load(IArchive& _archive, Vector4* _values, size_t _count,
                   uint32_t _version);
};

OZZ_IO_TYPE_NOT_VERSIONABLE(Vector4Int)
template <>
struct Extern<Vector4Int> {
  static void Save(OArchive& _archive, const Vector4Int* _values,
                   size_t _count);
  static void Load(IArchive& _archive, Vector4Int* _values, size_t _count,
                   uint32_t _version);
};

OZZ_IO_TYPE_NOT_VERSIONABLE(Matrix4)
template <>
struct Extern<Matrix4> {
  static void Save(OArchive& _archive, const Matrix4* _values,
                   size_t _count);
  static void Load(IArchive& _archive, Matrix4* _values, size_t _count,
                   uint32_t _version);
};
}  // namespace io
}  // namespace ozz
//CONFFX_END

#endif  // OZZ_OZZ_BASE_MATHS_SIMD_MATH_ARCHIVE_H_
