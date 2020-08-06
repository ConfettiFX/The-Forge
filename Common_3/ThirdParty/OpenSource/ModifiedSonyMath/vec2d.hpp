
// ================================================================================================
// -*- C++ -*-
// File: vectormath/vec2d.hpp
// Author: Guilherme R. Lampert
// Created on: 30/12/16
// Brief: 2D vector and point extensions to the original Vectormath library.
// ================================================================================================

#ifndef VECTORMATH_VEC2D_HPP
#define VECTORMATH_VEC2D_HPP

#include <cmath>
#include <cstdio>

namespace Vectormath
{

// Orbis already has Vector2
#if !VECTORMATH_MODE_SCE
class Vector2;
#endif
class Point2;
class IVector2;
class UVector2;

// ========================================================
// A 2-D unpadded vector (sizeof = 8 bytes)
// ========================================================
// Orbis already has Vector2
#if !VECTORMATH_MODE_SCE
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

// Multiply a 2-D vector per element
//
inline const Vector2 mulPerElem(const Vector2 & vec0, const Vector2 & vec1);

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
#endif
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
// Orbis already has Vector2
#if !VECTORMATH_MODE_SCE
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
#if defined(__linux__)
// linux build uses c++11 standard
    return Vector2(std::fabs(vec.getX()), std::fabs(vec.getY()));
#else
    return Vector2(std::fabsf(vec.getX()), std::fabsf(vec.getY()));
#endif
}

inline const Vector2 mulPerElem(const Vector2 & vec0, const Vector2 & vec1)
{
    return Vector2(vec0[0] * vec1[0], vec0[1] * vec1[1]);
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
#if __linux__
// linux build uses c++11 standard
    return std::sqrt(lengthSqr(vec));
#else
    return std::sqrtf(lengthSqr(vec));
#endif	
}

inline const Vector2 normalize(const Vector2 & vec)
{
    const float lenSqr = lengthSqr(vec);
#if __linux__
// linux build uses c++11 standard
    const float lenInv = (1.0f / std::sqrt(lenSqr));
#else
    const float lenInv = (1.0f / std::sqrtf(lenSqr));
#endif	
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
#endif
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
#if __linux__
// linux build uses c++11 standard
    return Point2(std::fabs(pnt.getX()), std::fabs(pnt.getY()));
#else
    return Point2(std::fabsf(pnt.getX()), std::fabsf(pnt.getY()));
#endif	
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

inline float dist(const Vector2& v0, const Vector2& v1)
{
	return length(v1 - v0);
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

//========================================= #TheForgeMathExtensionsBegin ================================================

// ========================================================
// A 2-D unpadded vector (sizeof = 8 bytes)
// ========================================================

class IVector2
{
	int mX;
	int mY;

public:

	// Default constructor; does no initialization
	//
	inline IVector2() {}

	// Construct a 2-D vector from x and y elements
	//
	inline IVector2(int x, int y);

	// Set all elements of a 2-D vector to the same scalar value
	//
	explicit inline IVector2(int scalar);

	// Set the x element of a 2-D vector
	//
	inline IVector2 & setX(int x);

	// Set the y element of a 2-D vector
	//
	inline IVector2 & setY(int y);

	// Get the x element of a 2-D vector
	//
	inline int getX() const;

	// Get the y element of a 2-D vector
	//
	inline int getY() const;

	// Set an x or y element of a 2-D vector by index
	//
	inline IVector2 & setElem(int idx, int value);

	// Get an x or y element of a 2-D vector by index
	//
	inline int getElem(int idx) const;

	// Subscripting operator to set or get an element
	//
	inline int & operator[](int idx);

	// Subscripting operator to get an element
	//
	inline int operator[](int idx) const;

	// Add two 2-D vectors
	//
	inline const IVector2 operator + (const IVector2 & vec) const;

	// Subtract a 2-D vector from another 2-D vector
	//
	inline const IVector2 operator - (const IVector2 & vec) const;

	// Multiply a 2-D vector by a scalar
	//
	inline const IVector2 operator * (int scalar) const;

	// Divide a 2-D vector by a scalar
	//
	inline const IVector2 operator / (int scalar) const;

	// Perform compound assignment and addition with a 2-D vector
	//
	inline IVector2 & operator += (const IVector2 & vec);

	// Perform compound assignment and subtraction by a 2-D vector
	//
	inline IVector2 & operator -= (const IVector2 & vec);

	// Perform compound assignment and multiplication by a scalar
	//
	inline IVector2 & operator *= (int scalar);

	// Perform compound assignment and division by a scalar
	//
	inline IVector2 & operator /= (int scalar);

	// Negate all elements of a 2-D vector
	//
	inline const IVector2 operator - () const;
	
	// true if x and y are equal
	//
	inline const bool operator == (const IVector2 &v) const;

	// Construct x axis
	//
	static inline const IVector2 xAxis();

	// Construct y axis
	//
	static inline const IVector2 yAxis();
};

// Multiply a 2-D vector by a scalar
//
inline const IVector2 operator * (int scalar, const IVector2 & vec);

// Compute the absolute value of a 2-D vector per element
//
inline const IVector2 absPerElem(const IVector2 & vec);

// Maximum of two 2-D vectors per element
//
inline const IVector2 maxPerElem(const IVector2 & vec0, const IVector2 & vec1);

// Minimum of two 2-D vectors per element
//
inline const IVector2 minPerElem(const IVector2 & vec0, const IVector2 & vec1);

// Maximum element of a 2-D vector
//
inline int maxElem(const IVector2 & vec);

// Minimum element of a 2-D vector
//
inline int minElem(const IVector2 & vec);

#ifdef VECTORMATH_DEBUG

// Print a 2-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const IVector2 & vec);

// Print a 2-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const IVector2 & vec, const char * name);

#endif // VECTORMATH_DEBUG

// ================================================================================================
// IVector2 implementation
// ================================================================================================

inline IVector2::IVector2(int _x, int _y)
	: mX(_x), mY(_y)
{
}

inline IVector2::IVector2(int scalar)
	: mX(scalar), mY(scalar)
{
}

inline IVector2 & IVector2::setX(int _x)
{
	mX = _x;
	return *this;
}

inline IVector2 & IVector2::setY(int _y)
{
	mY = _y;
	return *this;
}

inline int IVector2::getX() const
{
	return mX;
}

inline int IVector2::getY() const
{
	return mY;
}

inline IVector2 & IVector2::setElem(int idx, int value)
{
	*(&mX + idx) = value;
	return *this;
}

inline int IVector2::getElem(int idx) const
{
	return *(&mX + idx);
}

inline int & IVector2::operator[](int idx)
{
	return *(&mX + idx);
}

inline int IVector2::operator[](int idx) const
{
	return *(&mX + idx);
}

inline const IVector2 IVector2::operator + (const IVector2 & vec) const
{
	return IVector2((mX + vec.mX), (mY + vec.mY));
}

inline const IVector2 IVector2::operator - (const IVector2 & vec) const
{
	return IVector2((mX - vec.mX), (mY - vec.mY));
}

inline const IVector2 IVector2::operator * (int scalar) const
{
	return IVector2((mX * scalar), (mY * scalar));
}

inline const IVector2 IVector2::operator / (int scalar) const
{
	return IVector2((mX / scalar), (mY / scalar));
}

inline IVector2 & IVector2::operator += (const IVector2 & vec)
{
	mX += vec.mX;
	mY += vec.mY;
	return *this;
}

inline IVector2 & IVector2::operator -= (const IVector2 & vec)
{
	mX -= vec.mX;
	mY -= vec.mY;
	return *this;
}

inline IVector2 & IVector2::operator *= (int scalar)
{
	mX *= scalar;
	mY *= scalar;
	return *this;
}

inline IVector2 & IVector2::operator /= (int scalar)
{
	mX /= scalar;
	mY /= scalar;
	return *this;
}

inline const IVector2 IVector2::operator - () const
{
	return IVector2(-mX, -mY);
}

inline const bool IVector2::operator == (const IVector2 &v)  const 
{
	return (getX() == v.getX() && getY() == v.getY()); 
}

inline const IVector2 IVector2::xAxis()
{
	return IVector2(1, 0);
}

inline const IVector2 IVector2::yAxis()
{
	return IVector2(0, 1);
}

inline const IVector2 operator * (int scalar, const IVector2 & vec)
{
	return vec * scalar;
}

inline const IVector2 absPerElem(const IVector2 & vec)
{
	return IVector2(vec.getX() < 0 ? -vec.getX() : vec.getX(),
					vec.getY() < 0 ? -vec.getY() : vec.getY());
}

inline const IVector2 maxPerElem(const IVector2 & vec0, const IVector2 & vec1)
{
	return IVector2((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY());
}

inline const IVector2 minPerElem(const IVector2 & vec0, const IVector2 & vec1)
{
	return IVector2((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY());
}

inline int maxElem(const IVector2 & vec)
{
	return (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
}

inline int minElem(const IVector2 & vec)
{
	return (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
}

#ifdef VECTORMATH_DEBUG

inline void print(const IVector2 & vec)
{
	std::printf("( %i %i )\n", vec.getX(), vec.getY());
}

inline void print(const IVector2 & vec, const char * name)
{
	std::printf("%s: ( %i %i )\n", name, vec.getX(), vec.getY());
}

#endif // VECTORMATH_DEBUG


// ========================================================
// A 2-D unpadded vector (sizeof = 8 bytes)
// ========================================================

typedef unsigned int uint;

class UVector2
{
	uint mX;
	uint mY;

public:

	// Default constructor; does no initialization
	//
	inline UVector2() {}

	// Construct a 2-D vector from x and y elements
	//
	inline UVector2(uint x, uint y);

	// Set all elements of a 2-D vector to the same scalar value
	//
	explicit inline UVector2(uint scalar);

	// Set the x element of a 2-D vector
	//
	inline UVector2 & setX(uint x);

	// Set the y element of a 2-D vector
	//
	inline UVector2 & setY(uint y);

	// Get the x element of a 2-D vector
	//
	inline uint getX() const;

	// Get the y element of a 2-D vector
	//
	inline uint getY() const;

	// Set an x or y element of a 2-D vector by index
	//
	inline UVector2 & setElem(uint idx, uint value);

	// Get an x or y element of a 2-D vector by index
	//
	inline uint getElem(uint idx) const;

	// Subscripting operator to set or get an element
	//
	inline uint & operator[](uint idx);

	// Subscripting operator to get an element
	//
	inline uint operator[](uint idx) const;

	// Add two 2-D vectors
	//
	inline const UVector2 operator + (const UVector2 & vec) const;

	// Subtract a 2-D vector from another 2-D vector
	//
	inline const UVector2 operator - (const UVector2 & vec) const;

	// Multiply a 2-D vector by a scalar
	//
	inline const UVector2 operator * (uint scalar) const;

	// Divide a 2-D vector by a scalar
	//
	inline const UVector2 operator / (uint scalar) const;

	// Perform compound assignment and addition with a 2-D vector
	//
	inline UVector2 & operator += (const UVector2 & vec);

	// Perform compound assignment and subtraction by a 2-D vector
	//
	inline UVector2 & operator -= (const UVector2 & vec);

	// Perform compound assignment and multiplication by a scalar
	//
	inline UVector2 & operator *= (uint scalar);

	// Perform compound assignment and division by a scalar
	//
	inline UVector2 & operator /= (uint scalar);

	// Construct x axis
	//
	static inline const UVector2 xAxis();

	// Construct y axis
	//
	static inline const UVector2 yAxis();
};

// Multiply a 2-D vector by a scalar
//
inline const UVector2 operator * (uint scalar, const UVector2 & vec);

// Maximum of two 2-D vectors per element
//
inline const UVector2 maxPerElem(const UVector2 & vec0, const UVector2 & vec1);

// Minimum of two 2-D vectors per element
//
inline const UVector2 minPerElem(const UVector2 & vec0, const UVector2 & vec1);

// Maximum element of a 2-D vector
//
inline uint maxElem(const UVector2 & vec);

// Minimum element of a 2-D vector
//
inline uint minElem(const UVector2 & vec);

#ifdef VECTORMATH_DEBUG

// Print a 2-D vector
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const UVector2 & vec);

// Print a 2-D vector and an associated string identifier
// NOTE:
// Function is only defined when VECTORMATH_DEBUG is defined.
//
inline void print(const UVector2 & vec, const char * name);

#endif // VECTORMATH_DEBUG

// ================================================================================================
// UVector2 implementation
// ================================================================================================

inline UVector2::UVector2(uint _x, uint _y)
	: mX(_x), mY(_y)
{
}

inline UVector2::UVector2(uint scalar)
	: mX(scalar), mY(scalar)
{
}

inline UVector2 & UVector2::setX(uint _x)
{
	mX = _x;
	return *this;
}

inline UVector2 & UVector2::setY(uint _y)
{
	mY = _y;
	return *this;
}

inline uint UVector2::getX() const
{
	return mX;
}

inline uint UVector2::getY() const
{
	return mY;
}

inline UVector2 & UVector2::setElem(uint idx, uint value)
{
	*(&mX + idx) = value;
	return *this;
}

inline uint UVector2::getElem(uint idx) const
{
	return *(&mX + idx);
}

inline uint & UVector2::operator[](uint idx)
{
	return *(&mX + idx);
}

inline uint UVector2::operator[](uint idx) const
{
	return *(&mX + idx);
}

inline const UVector2 UVector2::operator + (const UVector2 & vec) const
{
	return UVector2((mX + vec.mX), (mY + vec.mY));
}

inline const UVector2 UVector2::operator - (const UVector2 & vec) const
{
	return UVector2((mX - vec.mX), (mY - vec.mY));
}

inline const UVector2 UVector2::operator * (uint scalar) const
{
	return UVector2((mX * scalar), (mY * scalar));
}

inline const UVector2 UVector2::operator / (uint scalar) const
{
	return UVector2((mX / scalar), (mY / scalar));
}

inline UVector2 & UVector2::operator += (const UVector2 & vec)
{
	mX += vec.mX;
	mY += vec.mY;
	return *this;
}

inline UVector2 & UVector2::operator -= (const UVector2 & vec)
{
	mX -= vec.mX;
	mY -= vec.mY;
	return *this;
}

inline UVector2 & UVector2::operator *= (uint scalar)
{
	mX *= scalar;
	mY *= scalar;
	return *this;
}

inline UVector2 & UVector2::operator /= (uint scalar)
{
	mX /= scalar;
	mY /= scalar;
	return *this;
}

inline const UVector2 UVector2::xAxis()
{
	return UVector2(1, 0);
}

inline const UVector2 UVector2::yAxis()
{
	return UVector2(0, 1);
}

inline const UVector2 operator * (uint scalar, const UVector2 & vec)
{
	return vec * scalar;
}

inline const UVector2 maxPerElem(const UVector2 & vec0, const UVector2 & vec1)
{
	return UVector2((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY());
}

inline const UVector2 minPerElem(const UVector2 & vec0, const UVector2 & vec1)
{
	return UVector2((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY());
}

inline uint maxElem(const UVector2 & vec)
{
	return (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
}

inline uint minElem(const UVector2 & vec)
{
	return (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
}

#ifdef VECTORMATH_DEBUG

inline void print(const UVector2 & vec)
{
	std::printf("( %u %u )\n", vec.getX(), vec.getY());
}

inline void print(const UVector2 & vec, const char * name)
{
	std::printf("%s: ( %u %u )\n", name, vec.getX(), vec.getY());
}

#endif // VECTORMATH_DEBUG

// ========================================================
// A 2x2 matrix in array-of-structures format
// ========================================================
// Orbis already has Matrix2
#if !VECTORMATH_MODE_SCE
struct Matrix2
{
	Vector2 mCol0;
	Vector2 mCol1;

	Matrix2() {}
	Matrix2(const Vector2 &col0, const Vector2 &col1) 
	{
		mCol0 = col0;
		mCol1 = col1;
	}
	Matrix2(const float m00, const float m01, const float m10, const float m11) 
	{
		mCol0 = Vector2(m00, m10);
		mCol1 = Vector2(m01, m11);
	}

	const Vector2 getCol(int col) const;
	float getElem(int col, int row) const;

	static inline const Matrix2 identity() { return Matrix2(1, 0, 0, 1); }
	static inline const Matrix2 rotate(const float radians);
};

inline Matrix2 operator + (const Matrix2 &m, const Matrix2 &n);
inline Matrix2 operator - (const Matrix2 &m, const Matrix2 &n);
inline Matrix2 operator - (const Matrix2 &m);

inline Matrix2 operator * (const Matrix2 &m, const Matrix2 &n);
inline Vector2 operator * (const Matrix2 &m, const Vector2 &v);
inline Matrix2 operator * (const Matrix2 &m, const float x);

inline float det(const Matrix2 &m);
inline Matrix2 operator ! (const Matrix2 &m);



// ================================================================================================
// Matrix2 implementation
// ================================================================================================

inline Matrix2 operator + (const Matrix2 &m, const Matrix2 &n) { return Matrix2(m.mCol0 + n.mCol0, m.mCol1 + n.mCol1); }
inline Matrix2 operator - (const Matrix2 &m, const Matrix2 &n) { return Matrix2(m.mCol0 - n.mCol0, m.mCol1 - n.mCol1); }
inline Matrix2 operator - (const Matrix2 &m) { return Matrix2(-m.mCol0, -m.mCol1); }

/*
[q,		r			]
[s,		t			]
[a,b][a*q+b*s,	a*r+b*t		]
[c,d][c*q+d*s,	c*r+b*t		]
*/

inline Matrix2 operator * (const Matrix2 &m, const Matrix2 &n) 
{
	return Matrix2(
		m.getCol(0)*n.getCol(0).getX() + m.getCol(1)*n.getCol(0).getY(),
		m.getCol(0)*n.getCol(1).getX() + m.getCol(1)*n.getCol(1).getY()
	);
}

inline Vector2 operator * (const Matrix2 &m, const Vector2 &v) { return Vector2(m.mCol0*v.getX() + m.mCol1*v.getY()); }
inline Matrix2 operator * (const Matrix2 &m, const float x)    { return Matrix2(m.mCol0 * x, m.mCol1 * x); }
inline float det(const Matrix2 &m) { return (m.mCol0.getX() * m.mCol1.getY() - m.mCol1.getX() * m.mCol0.getY()); }

inline Matrix2 operator ! (const Matrix2 &m) 
{
	float invDet = 1.0f / det(m);

	return Matrix2(
		m.mCol1.getY(), -m.mCol1.getX(),
		-m.mCol0.getY(), m.mCol0.getX()) * invDet;
}

inline const Matrix2 Matrix2::rotate(const float angle)
{
	float cosA = cosf(angle), sinA = sinf(angle);
	return Matrix2(cosA, sinA, -sinA, cosA);
}

inline const Vector2 Matrix2::getCol(int col) const
{
	return *(&mCol0 + col);
}

inline float Matrix2::getElem(int col, int row) const
{
	return getCol(col)[row];
}
#endif
//========================================= #TheForgeMathExtensionsEnd ==================================================

} // namespace Vectormath

#endif // VECTORMATH_VEC2D_HPP
