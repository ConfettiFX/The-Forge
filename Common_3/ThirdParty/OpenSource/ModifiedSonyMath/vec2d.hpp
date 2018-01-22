
// ================================================================================================
// -*- C++ -*-
// File: vectormath/vec2d.hpp
// Author: Guilherme R. Lampert
// Created on: 30/12/16
// Brief: 2D vector and point extensions to the original Vectormath library.
// ================================================================================================

#ifndef VECTORMATH_VEC2D_HPP
#define VECTORMATH_VEC2D_HPP

namespace Vectormath
{

class Vector2;
class Point2;

// ========================================================
// A 2-D unpadded vector (sizeof = 8 bytes)
// ========================================================

class Vector2
{
    float mX;
    float mY;

public:

    // Default constructor; does no initialization
    //
    inline Vector2() { }

    // Construct a 2-D vector from x and y elements
    //
    inline Vector2(float x, float y);

    // Copy elements from a 2-D point into a 2-D vector
    //
    explicit inline Vector2(const Point2 & pnt);

    // Set all elements of a 2-D vector to the same scalar value
    //
    explicit inline Vector2(float scalar);

    // Set the x element of a 2-D vector
    //
    inline Vector2 & setX(float x);

    // Set the y element of a 2-D vector
    //
    inline Vector2 & setY(float y);

    // Get the x element of a 2-D vector
    //
    inline float getX() const;

    // Get the y element of a 2-D vector
    //
    inline float getY() const;

    // Set an x or y element of a 2-D vector by index
    //
    inline Vector2 & setElem(int idx, float value);

    // Get an x or y element of a 2-D vector by index
    //
    inline float getElem(int idx) const;

    // Subscripting operator to set or get an element
    //
    inline float & operator[](int idx);

    // Subscripting operator to get an element
    //
    inline float operator[](int idx) const;

    // Add two 2-D vectors
    //
    inline const Vector2 operator + (const Vector2 & vec) const;

    // Subtract a 2-D vector from another 2-D vector
    //
    inline const Vector2 operator - (const Vector2 & vec) const;

    // Add a 2-D vector to a 2-D point
    //
    inline const Point2 operator + (const Point2 & pnt) const;

    // Multiply a 2-D vector by a scalar
    //
    inline const Vector2 operator * (float scalar) const;

    // Divide a 2-D vector by a scalar
    //
    inline const Vector2 operator / (float scalar) const;

    // Perform compound assignment and addition with a 2-D vector
    //
    inline Vector2 & operator += (const Vector2 & vec);

    // Perform compound assignment and subtraction by a 2-D vector
    //
    inline Vector2 & operator -= (const Vector2 & vec);

    // Perform compound assignment and multiplication by a scalar
    //
    inline Vector2 & operator *= (float scalar);

    // Perform compound assignment and division by a scalar
    //
    inline Vector2 & operator /= (float scalar);

    // Negate all elements of a 2-D vector
    //
    inline const Vector2 operator - () const;

    // Construct x axis
    //
    static inline const Vector2 xAxis();

    // Construct y axis
    //
    static inline const Vector2 yAxis();
};

// Multiply a 2-D vector by a scalar
//
inline const Vector2 operator * (float scalar, const Vector2 & vec);

// Compute the absolute value of a 2-D vector per element
//
inline const Vector2 absPerElem(const Vector2 & vec);

// Maximum of two 2-D vectors per element
//
inline const Vector2 maxPerElem(const Vector2 & vec0, const Vector2 & vec1);

// Minimum of two 2-D vectors per element
//
inline const Vector2 minPerElem(const Vector2 & vec0, const Vector2 & vec1);

// Maximum element of a 2-D vector
//
inline float maxElem(const Vector2 & vec);

// Minimum element of a 2-D vector
//
inline float minElem(const Vector2 & vec);

// Compute the dot product of two 2-D vectors
//
inline float dot(const Vector2 & vec0, const Vector2 & vec1);

// Compute the square of the length of a 2-D vector
//
inline float lengthSqr(const Vector2 & vec);

// Compute the length of a 2-D vector
//
inline float length(const Vector2 & vec);

// Normalize a 2-D vector
// NOTE:
// The result is unpredictable when all elements of vec are at or near zero.
//
inline const Vector2 normalize(const Vector2 & vec);

// Linear interpolation between two 2-D vectors
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Vector2 lerp(float t, const Vector2 & vec0, const Vector2 & vec1);

#ifdef VECTORMATH_DEBUG

// Print a 2-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Vector2 & vec);

// Print a 2-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Vector2 & vec, const char * name);

#endif // VECTORMATH_DEBUG

// ========================================================
// A 2-D unpadded point (sizeof = 8 bytes)
// ========================================================

class Point2
{
    float mX;
    float mY;

public:

    // Default constructor; does no initialization
    //
    inline Point2() { }

    // Construct a 2-D point from x and y elements
    //
    inline Point2(float x, float y);

    // Copy elements from a 2-D vector into a 2-D point
    //
    explicit inline Point2(const Vector2 & vec);

    // Set all elements of a 2-D point to the same scalar value
    //
    explicit inline Point2(float scalar);

    // Set the x element of a 2-D point
    //
    inline Point2 & setX(float x);

    // Set the y element of a 2-D point
    //
    inline Point2 & setY(float y);

    // Get the x element of a 2-D point
    //
    inline float getX() const;

    // Get the y element of a 2-D point
    //
    inline float getY() const;

    // Set an x or y element of a 2-D point by index
    //
    inline Point2 & setElem(int idx, float value);

    // Get an x or y element of a 2-D point by index
    //
    inline float getElem(int idx) const;

    // Subscripting operator to set or get an element
    //
    inline float & operator[](int idx);

    // Subscripting operator to get an element
    //
    inline float operator[](int idx) const;

    // Subtract a 2-D point from another 2-D point
    //
    inline const Vector2 operator - (const Point2 & pnt) const;

    // Add a 2-D point to a 2-D vector
    //
    inline const Point2 operator + (const Vector2 & vec) const;

    // Subtract a 2-D vector from a 2-D point
    //
    inline const Point2 operator - (const Vector2 & vec) const;

    // Perform compound assignment and addition with a 2-D vector
    //
    inline Point2 & operator += (const Vector2 & vec);

    // Perform compound assignment and subtraction by a 2-D vector
    //
    inline Point2 & operator -= (const Vector2 & vec);
};

// Compute the absolute value of a 2-D point per element
//
inline const Point2 absPerElem(const Point2 & pnt);

// Maximum of two 2-D points per element
//
inline const Point2 maxPerElem(const Point2 & pnt0, const Point2 & pnt1);

// Minimum of two 2-D points per element
//
inline const Point2 minPerElem(const Point2 & pnt0, const Point2 & pnt1);

// Maximum element of a 2-D point
//
inline float maxElem(const Point2 & pnt);

// Minimum element of a 2-D point
//
inline float minElem(const Point2 & pnt);

// Compute the square of the distance of a 2-D point from the coordinate-system origin
//
inline float distSqrFromOrigin(const Point2 & pnt);

// Compute the distance of a 2-D point from the coordinate-system origin
//
inline float distFromOrigin(const Point2 & pnt);

// Compute the square of the distance between two 2-D points
//
inline float distSqr(const Point2 & pnt0, const Point2 & pnt1);

// Compute the distance between two 2-D points
//
inline float dist(const Point2 & pnt0, const Point2 & pnt1);

// Linear interpolation between two 2-D points
// NOTE:
// Does not clamp t between 0 and 1.
//
inline const Point2 lerp(float t, const Point2 & pnt0, const Point2 & pnt1);

#ifdef VECTORMATH_DEBUG

// Print a 2-D point
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Point2 & pnt);

// Print a 2-D point and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const Point2 & pnt, const char * name);

#endif // VECTORMATH_DEBUG

// ================================================================================================
// Vector2 implementation
// ================================================================================================

inline Vector2::Vector2(float _x, float _y)
    : mX(_x), mY(_y)
{
}

inline Vector2::Vector2(const Point2 & pnt)
    : mX(pnt.getX()), mY(pnt.getY())
{
}

inline Vector2::Vector2(float scalar)
    : mX(scalar), mY(scalar)
{
}

inline Vector2 & Vector2::setX(float _x)
{
    mX = _x;
    return *this;
}

inline Vector2 & Vector2::setY(float _y)
{
    mY = _y;
    return *this;
}

inline float Vector2::getX() const
{
    return mX;
}

inline float Vector2::getY() const
{
    return mY;
}

inline Vector2 & Vector2::setElem(int idx, float value)
{
    *(&mX + idx) = value;
    return *this;
}

inline float Vector2::getElem(int idx) const
{
    return *(&mX + idx);
}

inline float & Vector2::operator[](int idx)
{
    return *(&mX + idx);
}

inline float Vector2::operator[](int idx) const
{
    return *(&mX + idx);
}

inline const Vector2 Vector2::operator + (const Vector2 & vec) const
{
    return Vector2((mX + vec.mX), (mY + vec.mY));
}

inline const Vector2 Vector2::operator - (const Vector2 & vec) const
{
    return Vector2((mX - vec.mX), (mY - vec.mY));
}

inline const Point2 Vector2::operator + (const Point2 & pnt) const
{
    return Point2((mX + pnt.getX()), (mY + pnt.getY()));
}

inline const Vector2 Vector2::operator * (float scalar) const
{
    return Vector2((mX * scalar), (mY * scalar));
}

inline const Vector2 Vector2::operator / (float scalar) const
{
    return Vector2((mX / scalar), (mY / scalar));
}

inline Vector2 & Vector2::operator += (const Vector2 & vec)
{
    mX += vec.mX;
    mY += vec.mY;
    return *this;
}

inline Vector2 & Vector2::operator -= (const Vector2 & vec)
{
    mX -= vec.mX;
    mY -= vec.mY;
    return *this;
}

inline Vector2 & Vector2::operator *= (float scalar)
{
    mX *= scalar;
    mY *= scalar;
    return *this;
}

inline Vector2 & Vector2::operator /= (float scalar)
{
    mX /= scalar;
    mY /= scalar;
    return *this;
}

inline const Vector2 Vector2::operator - () const
{
    return Vector2(-mX, -mY);
}

inline const Vector2 Vector2::xAxis()
{
    return Vector2(1.0f, 0.0f);
}

inline const Vector2 Vector2::yAxis()
{
    return Vector2(0.0f, 1.0f);
}

inline const Vector2 operator * (float scalar, const Vector2 & vec)
{
    return vec * scalar;
}

inline const Vector2 absPerElem(const Vector2 & vec)
{
    return Vector2(std::fabsf(vec.getX()), std::fabsf(vec.getY()));
}

inline const Vector2 maxPerElem(const Vector2 & vec0, const Vector2 & vec1)
{
    return Vector2((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
                   (vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY());
}

inline const Vector2 minPerElem(const Vector2 & vec0, const Vector2 & vec1)
{
    return Vector2((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
                   (vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY());
}

inline float maxElem(const Vector2 & vec)
{
    return (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
}

inline float minElem(const Vector2 & vec)
{
    return (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
}

inline float dot(const Vector2 & vec0, const Vector2 & vec1)
{
    float result;
    result = (vec0.getX() * vec1.getX());
    result = (result + (vec0.getY() * vec1.getY()));
    return result;
}

inline float lengthSqr(const Vector2 & vec)
{
    float result;
    result = (vec.getX() * vec.getX());
    result = (result + (vec.getY() * vec.getY()));
    return result;
}

inline float length(const Vector2 & vec)
{
    return std::sqrtf(lengthSqr(vec));
}

inline const Vector2 normalize(const Vector2 & vec)
{
    const float lenSqr = lengthSqr(vec);
    const float lenInv = (1.0f / std::sqrtf(lenSqr));
    return Vector2((vec.getX() * lenInv), (vec.getY() * lenInv));
}

inline const Vector2 lerp(float t, const Vector2 & vec0, const Vector2 & vec1)
{
    return (vec0 + ((vec1 - vec0) * t));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Vector2 & vec)
{
    std::printf("( %f %f )\n", vec.getX(), vec.getY());
}

inline void print(const Vector2 & vec, const char * name)
{
    std::printf("%s: ( %f %f )\n", name, vec.getX(), vec.getY());
}

#endif // VECTORMATH_DEBUG

// ================================================================================================
// Point2 implementation
// ================================================================================================

inline Point2::Point2(float _x, float _y)
    : mX(_x), mY(_y)
{
}

inline Point2::Point2(const Vector2 & vec)
    : mX(vec.getX()), mY(vec.getY())
{
}

inline Point2::Point2(float scalar)
    : mX(scalar), mY(scalar)
{
}

inline Point2 & Point2::setX(float _x)
{
    mX = _x;
    return *this;
}

inline Point2 & Point2::setY(float _y)
{
    mY = _y;
    return *this;
}

inline float Point2::getX() const
{
    return mX;
}

inline float Point2::getY() const
{
    return mY;
}

inline Point2 & Point2::setElem(int idx, float value)
{
    *(&mX + idx) = value;
    return *this;
}

inline float Point2::getElem(int idx) const
{
    return *(&mX + idx);
}

inline float & Point2::operator[](int idx)
{
    return *(&mX + idx);
}

inline float Point2::operator[](int idx) const
{
    return *(&mX + idx);
}

inline const Vector2 Point2::operator - (const Point2 & pnt) const
{
    return Vector2((mX - pnt.mX), (mY - pnt.mY));
}

inline const Point2 Point2::operator + (const Vector2 & vec) const
{
    return Point2((mX + vec.getX()), (mY + vec.getY()));
}

inline const Point2 Point2::operator - (const Vector2 & vec) const
{
    return Point2((mX - vec.getX()), (mY - vec.getY()));
}

inline Point2 & Point2::operator += (const Vector2 & vec)
{
    mX += vec.getX();
    mY += vec.getY();
    return *this;
}

inline Point2 & Point2::operator -= (const Vector2 & vec)
{
    mX -= vec.getX();
    mY -= vec.getY();
    return *this;
}

inline const Point2 absPerElem(const Point2 & pnt)
{
    return Point2(std::fabsf(pnt.getX()), std::fabsf(pnt.getY()));
}

inline const Point2 maxPerElem(const Point2 & pnt0, const Point2 & pnt1)
{
    return Point2((pnt0.getX() > pnt1.getX()) ? pnt0.getX() : pnt1.getX(),
                  (pnt0.getY() > pnt1.getY()) ? pnt0.getY() : pnt1.getY());
}

inline const Point2 minPerElem(const Point2 & pnt0, const Point2 & pnt1)
{
    return Point2((pnt0.getX() < pnt1.getX()) ? pnt0.getX() : pnt1.getX(),
                  (pnt0.getY() < pnt1.getY()) ? pnt0.getY() : pnt1.getY());
}

inline float maxElem(const Point2 & pnt)
{
    return (pnt.getX() > pnt.getY()) ? pnt.getX() : pnt.getY();
}

inline float minElem(const Point2 & pnt)
{
    return (pnt.getX() < pnt.getY()) ? pnt.getX() : pnt.getY();
}

inline float distSqrFromOrigin(const Point2 & pnt)
{
    return lengthSqr(Vector2(pnt));
}

inline float distFromOrigin(const Point2 & pnt)
{
    return length(Vector2(pnt));
}

inline float distSqr(const Point2 & pnt0, const Point2 & pnt1)
{
    return lengthSqr(pnt1 - pnt0);
}

inline float dist(const Point2 & pnt0, const Point2 & pnt1)
{
    return length(pnt1 - pnt0);
}

inline const Point2 lerp(float t, const Point2 & pnt0, const Point2 & pnt1)
{
    return (pnt0 + ((pnt1 - pnt0) * t));
}

#ifdef VECTORMATH_DEBUG

inline void print(const Point2 & pnt)
{
    std::printf("( %f %f )\n", pnt.getX(), pnt.getY());
}

inline void print(const Point2 & pnt, const char * name)
{
    std::printf("%s: ( %f %f )\n", name, pnt.getX(), pnt.getY());
}

#endif // VECTORMATH_DEBUG

} // namespace Vectormath

#endif // VECTORMATH_VEC2D_HPP
