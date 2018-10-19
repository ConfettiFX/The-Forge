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

#ifndef OZZ_OZZ_BASE_IO_ARCHIVE_TRAITS_H_
#define OZZ_OZZ_BASE_IO_ARCHIVE_TRAITS_H_

// Provides traits for customizing archive serialization properties: version,
// tag... See archive.h for more details.

#include <stdint.h>
#include <cstddef>

namespace ozz {
namespace io {

// Forward declaration of archive types.
class OArchive;
class IArchive;

// Default loading and saving external declaration.
// Those template implementations aim to be specialized at compilation time by
// non-member Load and save functions. For example the specialization of the
// Save() function for a type Type is:
// void Save(OArchive& _archive, const Extrusive* _test, size_t _count) {
// }
// The Load() function receives the version _version of type _Ty at the time the
// archive was saved.
// This uses polymorphism rather than template specialization to avoid
// including the file that contains the template definition.
//
// This default function call member _Ty::Load/Save function.
template <typename _Ty>
struct Extern;

// clang-format off

// Declares the current (compile time) version of _type.
// This macro must be used inside namespace ozz::io.
// Syntax is: OZZ_IO_TYPE_VERSION(46, Foo).
#define OZZ_IO_TYPE_VERSION(_version, _type) \
OZZ_STATIC_ASSERT(_version > 0);             \
namespace internal {                         \
template<> struct Version<const _type> {     \
      enum { kValue = _version };            \
};                                           \
}  // internal

// Declares the current (compile time) version of a template _type.
// This macro must be used inside namespace ozz::io.
// OZZ_IO_TYPE_VERSION_T1(46, typename _T1, Foo<_T1>).
#define OZZ_IO_TYPE_VERSION_T1(_version, _arg0, ...) \
OZZ_STATIC_ASSERT(_version > 0);                     \
namespace internal {                                 \
template<_arg0>                                      \
struct Version<const __VA_ARGS__> {                  \
      enum { kValue = _version };                    \
};                                                   \
}  // internal

// Declares the current (compile time) version of a template _type.
// This macro must be used inside namespace ozz::io.
// OZZ_IO_TYPE_VERSION_T2(46, typename _T1, typename _T2, Foo<_T1, _T2>).
#define OZZ_IO_TYPE_VERSION_T2(_version, _arg0, _arg1, ...) \
OZZ_STATIC_ASSERT(_version > 0);                            \
namespace internal {                                        \
template<_arg0, _arg1>                                      \
struct Version<const __VA_ARGS__> {                         \
      enum { kValue = _version };                           \
};                                                          \
  \
}  // internal

// Declares the current (compile time) version of a template _type.
// This macro must be used inside namespace ozz::io.
// OZZ_IO_TYPE_VERSION_T3(
//   46, typename _T1, typename _T2, typename _T3, Foo<_T1, _T2, _T3>).
#define OZZ_IO_TYPE_VERSION_T3(_version, _arg0, _arg1, _arg2, ...) \
OZZ_STATIC_ASSERT(_version > 0);                                   \
namespace internal {                                               \
template<_arg0, _arg1, _arg2>                                      \
struct Version<const __VA_ARGS__> {                                \
      enum { kValue = _version };                                  \
};                                                                 \
  \
}  // internal

// Declares the current (compile time) version of a template _type.
// This macro must be used inside namespace ozz::io.
// OZZ_IO_TYPE_VERSION_T4(
//   46, typename _T1, typename _T2, typename _T3, typename _T4,
//   Foo<_T1, _T2, _T3, _T4>).
#define OZZ_IO_TYPE_VERSION_T4(_version, _arg0, _arg1, _arg2, _arg3, ...) \
OZZ_STATIC_ASSERT(_version > 0);                                          \
namespace internal {                                                      \
template<_arg0, _arg1, _arg2, _arg3>                                      \
struct Version<const __VA_ARGS__> {                                       \
      enum { kValue = _version };                                         \
};                                                                        \
}  // internal

// Declares that _type is not versionable. Its version number is 0.
// Once a type has been declared not versionable, it cannot be changed without
// braking versioning.
// This macro must be used inside namespace ozz::io.
// Syntax is: OZZ_IO_TYPE_NOT_VERSIONABLE(Foo).
#define OZZ_IO_TYPE_NOT_VERSIONABLE(_type) \
namespace internal {                       \
template<> struct Version<const _type> {   \
      enum { kValue = 0 };                 \
};                                         \
}  // internal

// Declares that a template _type is not versionable. Its version number is 0.
// Once a type has been declared not versionable, it cannot be changed without
// braking versioning.
// This macro must be used inside namespace ozz::io.
// Syntax is:
// OZZ_IO_TYPE_NOT_VERSIONABLE_T1(typename _T1, Foo<_T1>).
#define OZZ_IO_TYPE_NOT_VERSIONABLE_T1(_arg0, ...) \
namespace internal {                               \
template<_arg0>                                    \
struct Version<const __VA_ARGS__> {                \
      enum { kValue = 0 };                         \
};                                                 \
}  // internal

// Decline non-versionable template declaration to 2 template arguments.
// Syntax is:
// OZZ_IO_TYPE_NOT_VERSIONABLE_T2(typename _T1, typename _T2, Foo<_T1, _T2>).
#define OZZ_IO_TYPE_NOT_VERSIONABLE_T2(_arg0, _arg1, ...) \
namespace internal {                                      \
template<_arg0, _arg1>                                    \
struct Version<const __VA_ARGS__> {                       \
      enum { kValue = 0 };                                \
};                                                        \
}  // internal

// Decline non-versionable template declaration to 3 template arguments.
// Syntax is:
// OZZ_IO_TYPE_NOT_VERSIONABLE_T3(
//   typename _T1, typename _T2, typename _T3, Foo<_T1, _T2, _T3>).
#define OZZ_IO_TYPE_NOT_VERSIONABLE_T3(_arg0, _arg1, _arg2, ...) \
namespace internal {                                             \
template<_arg0, _arg1, _arg2>                                    \
struct Version<const __VA_ARGS__> {                              \
      enum { kValue = 0 };                                       \
};                                                               \
}  // internal

// Decline non-versionable template declaration to 4 template arguments.
// Syntax is:
// OZZ_IO_TYPE_NOT_VERSIONABLE_T4(
//   typename _T1, typename _T2, typename _T3, typename _T4,
//   Foo<_T1, _T2, _T3, _T4>).
#define OZZ_IO_TYPE_NOT_VERSIONABLE_T4(_arg0, _arg1, _arg2, _arg3, ...) \
namespace internal {                                                    \
template<_arg0, _arg1, _arg2, _arg3>                                    \
struct Version<const __VA_ARGS__> {                                     \
      enum { kValue = 0 };                                              \
};                                                                      \
}  // internal

// Declares the tag of a template _type.
// A tag is a c-string that can be used to check the type (through its tag) of
// the next object to be read from an archive. If no tag is defined, then no
// check is performed.
// This macro must be used inside namespace ozz::io.
// OZZ_IO_TYPE_TAG("Foo", Foo).
#define OZZ_IO_TYPE_TAG(_tag, _type)                                     \
namespace internal {                                                     \
template<> struct Tag<const _type> {                                     \
      /* Length includes null terminated character to detect partial */  \
      /* tag mapping.*/                                                  \
      enum { kTagLength = OZZ_ARRAY_SIZE(_tag) };                        \
      static const char* Get() { return _tag; }                          \
};                                                                       \
}  // internal

// clang-format on

namespace internal {
// Definition of version specializable template struct.
// There's no default implementation in order to force user to define it, which
// in turn forces those who want to serialize an object to include the file that
// defines it's version. This helps with detecting issues at compile time.
template <typename _Ty>
struct Version;

// Defines default tag value, which is disabled.
template <typename _Ty>
struct Tag {
  enum { kTagLength = 0 };
};
}  // namespace internal
}  // namespace io
}  // namespace ozz
#endif  // OZZ_OZZ_BASE_IO_ARCHIVE_TRAITS_H_
