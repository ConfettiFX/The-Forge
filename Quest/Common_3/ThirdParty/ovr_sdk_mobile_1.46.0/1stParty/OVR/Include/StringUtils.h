/************************************************************************************

Filename    :   String_Utils.h
Content     :   std::string utility functions.
Created     :   May, 2014
Authors     :   J.M.P. van Waveren

Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*************************************************************************************/

#pragma once

#include "OVR_Types.h"
#include "OVR_Std.h"

#include <string>
#include <vector>

namespace OVR {
namespace StringUtils {
//
// Convert a string to a common type.
//

template <typename _type_>
inline size_t StringTo(_type_& value, const char* string) {
    return 0;
}

template <typename _type_>
inline size_t StringTo(_type_* valueArray, const int count, const char* string) {
    size_t length = 0;
    length += strspn(string + length, "{ \t\n\r");
    for (int i = 0; i < count; i++) {
        length += StringTo<_type_>(valueArray[i], string + length);
    }
    length += strspn(string + length, "} \t\n\r");
    return length;
}

template <typename _type_>
inline size_t StringTo(std::vector<_type_>& valueArray, const char* string) {
    size_t length = 0;
    length += strspn(string + length, "{ \t\n\r");
    for (;;) {
        _type_ value;
        size_t s = StringTo<_type_>(value, string + length);
        if (s == 0)
            break;
        valueArray.push_back(value);
        length += s;
    }
    length += strspn(string + length, "} \t\n\r");
    return length;
}

// specializations

template <>
inline size_t StringTo(short& value, const char* str) {
    char* endptr;
    value = (short)strtol(str, &endptr, 10);
    return size_t(endptr - str);
}
template <>
inline size_t StringTo(unsigned short& value, const char* str) {
    char* endptr;
    value = (unsigned short)strtoul(str, &endptr, 10);
    return size_t(endptr - str);
}
template <>
inline size_t StringTo(int& value, const char* str) {
    char* endptr;
    value = strtol(str, &endptr, 10);
    return size_t(endptr - str);
}
template <>
inline size_t StringTo(unsigned int& value, const char* str) {
    char* endptr;
    value = strtoul(str, &endptr, 10);
    return size_t(endptr - str);
}
template <>
inline size_t StringTo(float& value, const char* str) {
    char* endptr;
    value = strtof(str, &endptr);
    return size_t(endptr - str);
}
template <>
inline size_t StringTo(double& value, const char* str) {
    char* endptr;
    value = strtod(str, &endptr);
    return size_t(endptr - str);
}

template <>
inline size_t StringTo(Vector2f& value, const char* string) {
    return StringTo(&value.x, 2, string);
}
template <>
inline size_t StringTo(Vector2d& value, const char* string) {
    return StringTo(&value.x, 2, string);
}
template <>
inline size_t StringTo(Vector2i& value, const char* string) {
    return StringTo(&value.x, 2, string);
}

template <>
inline size_t StringTo(Vector3f& value, const char* string) {
    return StringTo(&value.x, 3, string);
}
template <>
inline size_t StringTo(Vector3d& value, const char* string) {
    return StringTo(&value.x, 3, string);
}
template <>
inline size_t StringTo(Vector3i& value, const char* string) {
    return StringTo(&value.x, 3, string);
}

template <>
inline size_t StringTo(Vector4f& value, const char* string) {
    return StringTo(&value.x, 4, string);
}
template <>
inline size_t StringTo(Vector4d& value, const char* string) {
    return StringTo(&value.x, 4, string);
}
template <>
inline size_t StringTo(Vector4i& value, const char* string) {
    return StringTo(&value.x, 4, string);
}

template <>
inline size_t StringTo(Matrix4f& value, const char* string) {
    return StringTo(&value.M[0][0], 16, string);
}
template <>
inline size_t StringTo(Matrix4d& value, const char* string) {
    return StringTo(&value.M[0][0], 16, string);
}

template <>
inline size_t StringTo(Quatf& value, const char* string) {
    return StringTo(&value.x, 4, string);
}
template <>
inline size_t StringTo(Quatd& value, const char* string) {
    return StringTo(&value.x, 4, string);
}

template <>
inline size_t StringTo(Planef& value, const char* string) {
    return StringTo(&value.N.x, 4, string);
}
template <>
inline size_t StringTo(Planed& value, const char* string) {
    return StringTo(&value.N.x, 4, string);
}

template <>
inline size_t StringTo(Bounds3f& value, const char* string) {
    return StringTo(value.b, 2, string);
}
template <>
inline size_t StringTo(Bounds3d& value, const char* string) {
    return StringTo(value.b, 2, string);
}

template <typename _type_>
inline std::string ToString(const _type_& value) {
    return std::string();
}

inline std::string FormattedStringV(const char* format, va_list argList) {
    std::string result;

    const size_t size1 = OVR::OVR_vscprintf(format, argList);

    result.reserve(size1 + 1);

    const size_t size2 = OVR::OVR_vsprintf((char*)result.data(), size1 + 1, format, argList);

    OVR_UNUSED1(size2);
    OVR_ASSERT(size1 == size2);

    return result;
}

inline std::string Va(const char* format, ...) {
    va_list argList;

    va_start(argList, format);
    const std::string result = FormattedStringV(format, argList);
    va_end(argList);

    return result;
}

template <typename _type_>
inline std::string ToString(const _type_* valueArray, const int count) {
    std::string string = "{";
    for (int i = 0; i < count; i++) {
        string += ToString(valueArray[i]);
    }
    string += "}";
    return string;
}

template <typename _type_>
inline std::string ToString(const std::vector<_type_>& valueArray) {
    std::string string = "{";
    for (int i = 0; i < valueArray.GetSizeI(); i++) {
        string += ToString(valueArray[i]);
    }
    string += "}";
    return string;
}

// specializations

template <>
inline std::string ToString(const short& value) {
    return std::string(Va(" %hi", value));
}
template <>
inline std::string ToString(const unsigned short& value) {
    return std::string(Va(" %uhi", value));
}
template <>
inline std::string ToString(const int& value) {
    return std::string(Va(" %li", value));
}
template <>
inline std::string ToString(const unsigned int& value) {
    return std::string(Va(" %uli", value));
}
template <>
inline std::string ToString(const float& value) {
    return std::string(Va(" %f", value));
}
template <>
inline std::string ToString(const double& value) {
    return std::string(Va(" %f", value));
}

template <>
inline std::string ToString(const Vector2f& value) {
    return std::string(Va("{ %f %f }", value.x, value.y));
}
template <>
inline std::string ToString(const Vector2d& value) {
    return std::string(Va("{ %f %f }", value.x, value.y));
}
template <>
inline std::string ToString(const Vector2i& value) {
    return std::string(Va("{ %d %d }", value.x, value.y));
}

template <>
inline std::string ToString(const Vector3f& value) {
    return std::string(Va("{ %f %f %f }", value.x, value.y, value.z));
}
template <>
inline std::string ToString(const Vector3d& value) {
    return std::string(Va("{ %f %f %f }", value.x, value.y, value.z));
}
template <>
inline std::string ToString(const Vector3i& value) {
    return std::string(Va("{ %d %d %d }", value.x, value.y, value.z));
}

template <>
inline std::string ToString(const Vector4f& value) {
    return std::string(Va("{ %f %f %f %f }", value.x, value.y, value.z, value.w));
}
template <>
inline std::string ToString(const Vector4d& value) {
    return std::string(Va("{ %f %f %f %f }", value.x, value.y, value.z, value.w));
}
template <>
inline std::string ToString(const Vector4i& value) {
    return std::string(Va("{ %d %d %d %d }", value.x, value.y, value.z, value.w));
}

template <>
inline std::string ToString(const Matrix4f& value) {
    return std::string(
        Va("{ %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f }",
           value.M[0][0],
           value.M[0][1],
           value.M[0][2],
           value.M[0][3],
           value.M[1][0],
           value.M[1][1],
           value.M[1][2],
           value.M[1][3],
           value.M[2][0],
           value.M[2][1],
           value.M[2][2],
           value.M[2][3],
           value.M[3][0],
           value.M[3][1],
           value.M[3][2],
           value.M[3][3]));
}
template <>
inline std::string ToString(const Matrix4d& value) {
    return std::string(
        Va("{ %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f %f }",
           value.M[0][0],
           value.M[0][1],
           value.M[0][2],
           value.M[0][3],
           value.M[1][0],
           value.M[1][1],
           value.M[1][2],
           value.M[1][3],
           value.M[2][0],
           value.M[2][1],
           value.M[2][2],
           value.M[2][3],
           value.M[3][0],
           value.M[3][1],
           value.M[3][2],
           value.M[3][3]));
}

template <>
inline std::string ToString(const Quatf& value) {
    return std::string(Va("{ %f %f %f %f }", value.x, value.y, value.z, value.w));
}
template <>
inline std::string ToString(const Quatd& value) {
    return std::string(Va("{ %f %f %f %f }", value.x, value.y, value.z, value.w));
}

template <>
inline std::string ToString(const Planef& value) {
    return std::string(Va("{ %f %f %f %f }", value.N.x, value.N.y, value.N.z, value.D));
}
template <>
inline std::string ToString(const Planed& value) {
    return std::string(Va("{ %f %f %f %f }", value.N.x, value.N.y, value.N.z, value.D));
}

template <>
inline std::string ToString(const Bounds3f& value) {
    return std::string(
        Va("{{ %f %f %f }{ %f %f %f }}",
           value.b[0].x,
           value.b[0].y,
           value.b[0].z,
           value.b[1].x,
           value.b[1].y,
           value.b[1].z));
}
template <>
inline std::string ToString(const Bounds3d& value) {
    return std::string(
        Va("{{ %f %f %f }{ %f %f %f }}",
           value.b[0].x,
           value.b[0].y,
           value.b[0].z,
           value.b[1].x,
           value.b[1].y,
           value.b[1].z));
}

inline bool EndsWith(std::string const& value, std::string const& ending) {
    if (ending.size() > value.size()) {
        return false;
    }
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

} // namespace StringUtils
} // namespace OVR
