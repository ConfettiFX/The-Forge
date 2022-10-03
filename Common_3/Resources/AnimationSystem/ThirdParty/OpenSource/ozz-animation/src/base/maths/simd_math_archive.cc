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

#include "../../../include/ozz/base/maths/simd_math_archive.h"

#include "../../../include/ozz/base/io/archive.h"

namespace ozz {
namespace io {

void Extern<Quat>::Save(OArchive& _archive, const Quat* _values,
                           size_t _count) {
  _archive << MakeArray(reinterpret_cast<const float*>(_values), 4 * _count);
}
void Extern<Quat>::Load(IArchive& _archive, Quat* _values, size_t _count,
                           uint32_t _version) {
  (void)_version;
  _archive >> MakeArray(reinterpret_cast<float*>(_values), 4 * _count);
}

void Extern<Vector2>::Save(OArchive& _archive, const Vector2* _values,
    size_t _count) {
    COMPILE_ASSERT(sizeof(Vector2) / sizeof(float) == 2);
    _archive << MakeArray(reinterpret_cast<const float*>(_values), 2 * _count);
}
void Extern<Vector2>::Load(IArchive& _archive, Vector2* _values, size_t _count,
    uint32_t _version) {
    (void)_version;
    COMPILE_ASSERT(sizeof(Vector2) / sizeof(float) == 2);
    _archive >> MakeArray(reinterpret_cast<float*>(_values), 2 * _count);
}

void Extern<Vector3>::Save(OArchive& _archive, const Vector3* _values,
    size_t _count) {
    COMPILE_ASSERT(sizeof(Vector3) / sizeof(float) == 4);
    _archive << MakeArray(reinterpret_cast<const float*>(_values), 4 * _count);
}
void Extern<Vector3>::Load(IArchive& _archive, Vector3* _values, size_t _count,
    uint32_t _version) {
    (void)_version;
    COMPILE_ASSERT(sizeof(Vector3) / sizeof(float) == 4);
    _archive >> MakeArray(reinterpret_cast<float*>(_values), 4 * _count);
}

void Extern<Vector4>::Save(OArchive& _archive, const Vector4* _values,
                           size_t _count) {

    COMPILE_ASSERT(sizeof(Vector4) / sizeof(float) == 4);
  _archive << MakeArray(reinterpret_cast<const float*>(_values), 4 * _count);
}
void Extern<Vector4>::Load(IArchive& _archive, Vector4* _values, size_t _count,
                           uint32_t _version) {
  (void)_version;
  COMPILE_ASSERT(sizeof(Vector4) / sizeof(float) == 4);
  _archive >> MakeArray(reinterpret_cast<float*>(_values), 4 * _count);
}

void Extern<Matrix4>::Save(OArchive& _archive, const Matrix4* _values,
                           size_t _count) {
  _archive << MakeArray(reinterpret_cast<const float*>(_values), 16 * _count);
}
void Extern<Matrix4>::Load(IArchive& _archive, Matrix4* _values, size_t _count,
                           uint32_t _version) {
  (void)_version;
  _archive >> MakeArray(reinterpret_cast<float*>(_values), 16 * _count);
}

void Extern<AffineTransform>::Save(OArchive& _archive, const AffineTransform* _values,
    size_t _count) {
    COMPILE_ASSERT(sizeof(AffineTransform) / sizeof(float) == 12);
    _archive << MakeArray(reinterpret_cast<const float*>(&_values->translation),
        12 * _count);
}
void Extern<AffineTransform>::Load(IArchive& _archive, AffineTransform* _values,
    size_t _count, uint32_t _version) {
    (void)_version;
    COMPILE_ASSERT(sizeof(AffineTransform) / sizeof(float) == 12);
    _archive >>
        MakeArray(reinterpret_cast<float*>(&_values->translation), 12 * _count);
}

void Extern<SoaTransform>::Save(OArchive& _archive, const SoaTransform* _values,
                                size_t _count) {
  COMPILE_ASSERT(sizeof(SoaTransform) / sizeof(float) == 40);
  _archive << MakeArray(reinterpret_cast<const float*>(&_values->translation.x),
                        40 * _count);
}
void Extern<SoaTransform>::Load(IArchive& _archive, SoaTransform* _values,
                                size_t _count, uint32_t _version) {
  (void)_version;
  COMPILE_ASSERT(sizeof(SoaTransform) / sizeof(float) == 40);
  _archive >>
      MakeArray(reinterpret_cast<float*>(&_values->translation.x), 40 * _count);
}


}  // namespace io
}  // namespace ozz
