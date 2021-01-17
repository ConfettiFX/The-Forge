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

#ifndef OZZ_OZZ_BASE_CONTAINERS_STRING_ARCHIVE_H_
#define OZZ_OZZ_BASE_CONTAINERS_STRING_ARCHIVE_H_

#include "ozz/base/containers/string.h"
#include "ozz/base/io/archive_traits.h"
#include "ozz/base/platform.h"

namespace ozz {
namespace io {

OZZ_IO_TYPE_NOT_VERSIONABLE(ozz::string)

template <>
struct Extern<ozz::string> {
  static void Save(OArchive& _archive, const ozz::string* _values,
                   size_t _count);
  static void Load(IArchive& _archive, ozz::string* _values, size_t _count,
                   uint32_t _version);
};
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_STRING_ARCHIVE_H_
