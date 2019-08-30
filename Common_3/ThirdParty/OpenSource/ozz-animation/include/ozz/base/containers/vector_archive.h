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

#ifndef OZZ_OZZ_BASE_CONTAINERS_VECTOR_ARCHIVE_H_
#define OZZ_OZZ_BASE_CONTAINERS_VECTOR_ARCHIVE_H_

#include "ozz/base/containers/vector.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/platform.h"

namespace ozz {
namespace io {

OZZ_IO_TYPE_NOT_VERSIONABLE_T2(class _Ty, class _Allocator,
                               eastl::vector<_Ty, _Allocator>)

template <class _Ty, class _Allocator>
struct Extern<eastl::vector<_Ty, _Allocator> > {
  inline static void Save(OArchive& _archive,
                          const eastl::vector<_Ty, _Allocator>* _values,
                          size_t _count) {
    for (size_t i = 0; i < _count; i++) {
      const eastl::vector<_Ty, _Allocator>& vector = _values[i];
      const uint32_t size = static_cast<uint32_t>(vector.size());
      _archive << size;
      if (size > 0) {
        _archive << ozz::io::MakeArray(&vector[0], size);
      }
    }
  }
  inline static void Load(IArchive& _archive,
                          eastl::vector<_Ty, _Allocator>* _values, size_t _count,
                          uint32_t _version) {
    (void)_version;
    for (size_t i = 0; i < _count; i++) {
      eastl::vector<_Ty, _Allocator>& vector = _values[i];
      uint32_t size;
      _archive >> size;
      vector.resize(size);
      if (size > 0) {
        _archive >> ozz::io::MakeArray(&vector[0], size);
      }
    }
  }
};
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_CONTAINERS_VECTOR_ARCHIVE_H_
