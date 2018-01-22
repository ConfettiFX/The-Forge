
// ================================================================================================
// -*- C++ -*-
// File: vectormath/common.hpp
// Author: Guilherme R. Lampert
// Created on: 30/12/16
// Brief: Extra helper functions added to the Vectormath library.
// ================================================================================================

#ifndef VECTORMATH_COMMON_HPP
#define VECTORMATH_COMMON_HPP

namespace Vectormath
{

inline float * toFloatPtr(Point2  & p)    { return reinterpret_cast<float *>(&p); } //  2 floats - default alignment
inline float * toFloatPtr(Point3  & p)    { return reinterpret_cast<float *>(&p); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Vector2 & v)    { return reinterpret_cast<float *>(&v); } //  2 floats - default alignment
inline float * toFloatPtr(Vector3 & v)    { return reinterpret_cast<float *>(&v); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Vector4 & v)    { return reinterpret_cast<float *>(&v); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Quat    & q)    { return reinterpret_cast<float *>(&q); } //  4 floats - 16 bytes aligned
inline float * toFloatPtr(Matrix3 & m)    { return reinterpret_cast<float *>(&m); } // 12 floats - 16 bytes aligned
inline float * toFloatPtr(Matrix4 & m)    { return reinterpret_cast<float *>(&m); } // 16 floats - 16 bytes aligned
inline float * toFloatPtr(Transform3 & t) { return reinterpret_cast<float *>(&t); } // 16 floats - 16 bytes aligned

inline const float * toFloatPtr(const Point2  & p)    { return reinterpret_cast<const float *>(&p); }
inline const float * toFloatPtr(const Point3  & p)    { return reinterpret_cast<const float *>(&p); }
inline const float * toFloatPtr(const Vector2 & v)    { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Vector3 & v)    { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Vector4 & v)    { return reinterpret_cast<const float *>(&v); }
inline const float * toFloatPtr(const Quat    & q)    { return reinterpret_cast<const float *>(&q); }
inline const float * toFloatPtr(const Matrix3 & m)    { return reinterpret_cast<const float *>(&m); }
inline const float * toFloatPtr(const Matrix4 & m)    { return reinterpret_cast<const float *>(&m); }
inline const float * toFloatPtr(const Transform3 & t) { return reinterpret_cast<const float *>(&t); }

// Shorthand to discard the last element of a Vector4 and get a Point3.
inline Point3 toPoint3(const Vector4 & v4)
{
    return Point3(v4[0], v4[1], v4[2]);
}

// Convert from world (global) coordinates to local model coordinates.
// Input matrix must be the inverse of the model matrix, e.g.: 'inverse(modelMatrix)'.
inline Point3 worldPointToModel(const Matrix4 & invModelToWorldMatrix, const Point3 & point)
{
    return toPoint3(invModelToWorldMatrix * point);
}

// Makes a plane projection matrix that can be used for simple object shadow effects.
// The W component of the light position vector should be 1 for a point light and 0 for directional.
inline Matrix4 makeShadowMatrix(const Vector4 & plane, const Vector4 & light)
{
    Matrix4 shadowMat;
    const auto dot = (plane[0] * light[0]) +
                     (plane[1] * light[1]) +
                     (plane[2] * light[2]) +
                     (plane[3] * light[3]);

    shadowMat[0][0] = dot - (light[0] * plane[0]);
    shadowMat[1][0] =     - (light[0] * plane[1]);
    shadowMat[2][0] =     - (light[0] * plane[2]);
    shadowMat[3][0] =     - (light[0] * plane[3]);

    shadowMat[0][1] =     - (light[1] * plane[0]);
    shadowMat[1][1] = dot - (light[1] * plane[1]);
    shadowMat[2][1] =     - (light[1] * plane[2]);
    shadowMat[3][1] =     - (light[1] * plane[3]);

    shadowMat[0][2] =     - (light[2] * plane[0]);
    shadowMat[1][2] =     - (light[2] * plane[1]);
    shadowMat[2][2] = dot - (light[2] * plane[2]);
    shadowMat[3][2] =     - (light[2] * plane[3]);

    shadowMat[0][3] =     - (light[3] * plane[0]);
    shadowMat[1][3] =     - (light[3] * plane[1]);
    shadowMat[2][3] =     - (light[3] * plane[2]);
    shadowMat[3][3] = dot - (light[3] * plane[3]);

    return shadowMat;
}

} // namespace Vectormath

#endif // VECTORMATH_COMMON_HPP
