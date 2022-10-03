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

#include "../../../include/ozz/base/maths/math_archive.h"



#include "../../../include/ozz/base/io/archive.h"
#include "../../../include/ozz/base/maths/rect.h"

namespace ozz {
namespace io {

void Extern<math::RectFloat>::Save(OArchive& _archive,
                                   const math::RectFloat* _values,
                                   size_t _count) {
  _archive << MakeArray(&_values->left, 4 * _count);
}
void Extern<math::RectFloat>::Load(IArchive& _archive, math::RectFloat* _values,
                                   size_t _count, uint32_t _version) {
  (void)_version;
  _archive >> MakeArray(&_values->left, 4 * _count);
}

void Extern<math::RectInt>::Save(OArchive& _archive,
                                 const math::RectInt* _values, size_t _count) {
  _archive << MakeArray(&_values->left, 4 * _count);
}
void Extern<math::RectInt>::Load(IArchive& _archive, math::RectInt* _values,
                                 size_t _count, uint32_t _version) {
  (void)_version;
  _archive >> MakeArray(&_values->left, 4 * _count);
}
}  // namespace io
}  // namespace ozz
