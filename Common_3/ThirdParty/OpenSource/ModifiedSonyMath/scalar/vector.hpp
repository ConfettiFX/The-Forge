/*
   Copyright (C) 2006, 2007 Sony Computer Entertainment Inc.
   All rights reserved.

   Redistribution and use in source and binary forms,
   with or without modification, are permitted provided that the
   following conditions are met:
	* Redistributions of source code must retain the above copyright
	  notice, this list of conditions and the following disclaimer.
	* Redistributions in binary form must reproduce the above copyright
	  notice, this list of conditions and the following disclaimer in the
	  documentation and/or other materials provided with the distribution.
	* Neither the name of the Sony Computer Entertainment Inc nor the names
	  of its contributors may be used to endorse or promote products derived
	  from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef VECTORMATH_SCALAR_VECTOR_HPP
#define VECTORMATH_SCALAR_VECTOR_HPP

namespace Vectormath
{
namespace Scalar
{

// Small epsilon value
static const float VECTORMATH_SLERP_TOL = 0.999f;

// ========================================================
// Vector3
// ========================================================

inline Vector3::Vector3(const Vector3 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
}

inline Vector3::Vector3(float _x, float _y, float _z)
{
	mX = _x;
	mY = _y;
	mZ = _z;
}

inline Vector3::Vector3(const Point3 & pnt)
{
	mX = pnt.getX();
	mY = pnt.getY();
	mZ = pnt.getZ();
}

inline Vector3::Vector3(float scalar)
{
	mX = scalar;
	mY = scalar;
	mZ = scalar;
}

inline const Vector3 Vector3::xAxis()
{
	return Vector3(1.0f, 0.0f, 0.0f);
}

inline const Vector3 Vector3::yAxis()
{
	return Vector3(0.0f, 1.0f, 0.0f);
}

inline const Vector3 Vector3::zAxis()
{
	return Vector3(0.0f, 0.0f, 1.0f);
}

inline const Vector3 lerp(float t, const Vector3 & vec0, const Vector3 & vec1)
{
	return (vec0 + ((vec1 - vec0) * t));
}

inline const Vector3 slerp(float t, const Vector3 & unitVec0, const Vector3 & unitVec1)
{
	float recipSinAngle, scale0, scale1, cosAngle, angle;
	cosAngle = dot(unitVec0, unitVec1);
	if (cosAngle < VECTORMATH_SLERP_TOL)
	{
		angle = std::acosf(cosAngle);
		recipSinAngle = (1.0f / std::sinf(angle));
		scale0 = (std::sinf(((1.0f - t) * angle)) * recipSinAngle);
		scale1 = (std::sinf((t * angle)) * recipSinAngle);
	}
	else
	{
		scale0 = (1.0f - t);
		scale1 = t;
	}
	return ((unitVec0 * scale0) + (unitVec1 * scale1));
}

inline Vector3 & Vector3::operator = (const Vector3 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
	return *this;
}

inline Vector3 & Vector3::setX(float _x)
{
	mX = _x;
	return *this;
}

inline float Vector3::getX() const
{
	return mX;
}

inline Vector3 & Vector3::setY(float _y)
{
	mY = _y;
	return *this;
}

inline float Vector3::getY() const
{
	return mY;
}

inline Vector3 & Vector3::setZ(float _z)
{
	mZ = _z;
	return *this;
}

inline float Vector3::getZ() const
{
	return mZ;
}

inline Vector3 & Vector3::setW(float _w)
{
	mW = _w;
	return *this;
}

inline float Vector3::getW() const
{
	return mW;
}

inline Vector3 & Vector3::setElem(int idx, float value)
{
	*(&mX + idx) = value;
	return *this;
}

inline float Vector3::getElem(int idx) const
{
	return *(&mX + idx);
}

inline float & Vector3::operator[](int idx)
{
	return *(&mX + idx);
}

inline float Vector3::operator[](int idx) const
{
	return *(&mX + idx);
}

inline const Vector3 Vector3::operator + (const Vector3 & vec) const
{
	return Vector3((mX + vec.mX),
				   (mY + vec.mY),
				   (mZ + vec.mZ));
}

inline const Vector3 Vector3::operator - (const Vector3 & vec) const
{
	return Vector3((mX - vec.mX),
				   (mY - vec.mY),
				   (mZ - vec.mZ));
}

inline const Point3 Vector3::operator + (const Point3 & pnt) const
{
	return Point3((mX + pnt.getX()),
				  (mY + pnt.getY()),
				  (mZ + pnt.getZ()));
}

inline const Vector3 Vector3::operator * (float scalar) const
{
	return Vector3((mX * scalar),
				   (mY * scalar),
				   (mZ * scalar));
}

inline Vector3 & Vector3::operator += (const Vector3 & vec)
{
	*this = *this + vec;
	return *this;
}

inline Vector3 & Vector3::operator -= (const Vector3 & vec)
{
	*this = *this - vec;
	return *this;
}

inline Vector3 & Vector3::operator *= (float scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const Vector3 Vector3::operator / (float scalar) const
{
	return Vector3((mX / scalar),
				   (mY / scalar),
				   (mZ / scalar));
}

inline Vector3 & Vector3::operator /= (float scalar)
{
	*this = *this / scalar;
	return *this;
}

inline const Vector3 Vector3::operator - () const
{
	return Vector3(-mX, -mY, -mZ);
}

inline const Vector3 operator * (float scalar, const Vector3 & vec)
{
	return vec * scalar;
}

inline const Vector3 mulPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
	return Vector3((vec0.getX() * vec1.getX()),
				   (vec0.getY() * vec1.getY()),
				   (vec0.getZ() * vec1.getZ()));
}

inline const Vector3 divPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
	return Vector3((vec0.getX() / vec1.getX()),
				   (vec0.getY() / vec1.getY()),
				   (vec0.getZ() / vec1.getZ()));
}

inline const Vector3 recipPerElem(const Vector3 & vec)
{
	return Vector3((1.0f / vec.getX()),
				   (1.0f / vec.getY()),
				   (1.0f / vec.getZ()));
}

inline const Vector3 sqrtPerElem(const Vector3 & vec)
{
	return Vector3(std::sqrtf(vec.getX()),
				   std::sqrtf(vec.getY()),
				   std::sqrtf(vec.getZ()));
}

inline const Vector3 rsqrtPerElem(const Vector3 & vec)
{
	return Vector3((1.0f / std::sqrtf(vec.getX())),
				   (1.0f / std::sqrtf(vec.getY())),
				   (1.0f / std::sqrtf(vec.getZ())));
}

inline const Vector3 absPerElem(const Vector3 & vec)
{
	return Vector3(std::fabsf(vec.getX()),
				   std::fabsf(vec.getY()),
				   std::fabsf(vec.getZ()));
}

inline const Vector3 copySignPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
	return Vector3((vec1.getX() < 0.0f) ? -std::fabsf(vec0.getX()) : std::fabsf(vec0.getX()),
				   (vec1.getY() < 0.0f) ? -std::fabsf(vec0.getY()) : std::fabsf(vec0.getY()),
				   (vec1.getZ() < 0.0f) ? -std::fabsf(vec0.getZ()) : std::fabsf(vec0.getZ()));
}

inline const Vector3 maxPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
	return Vector3((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
				   (vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY(),
				   (vec0.getZ() > vec1.getZ()) ? vec0.getZ() : vec1.getZ());
}

inline float maxElem(const Vector3 & vec)
{
	float result;
	result = (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() > result)     ? vec.getZ() : result;
	return result;
}

inline const Vector3 minPerElem(const Vector3 & vec0, const Vector3 & vec1)
{
	return Vector3((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
				   (vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY(),
				   (vec0.getZ() < vec1.getZ()) ? vec0.getZ() : vec1.getZ());
}

inline float minElem(const Vector3 & vec)
{
	float result;
	result = (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() < result)     ? vec.getZ() : result;
	return result;
}

inline float sum(const Vector3 & vec)
{
	float result;
	result = (vec.getX() + vec.getY());
	result = (result + vec.getZ());
	return result;
}

inline float dot(const Vector3 & vec0, const Vector3 & vec1)
{
	float result;
	result = (vec0.getX() * vec1.getX());
	result = (result + (vec0.getY() * vec1.getY()));
	result = (result + (vec0.getZ() * vec1.getZ()));
	return result;
}

inline float lengthSqr(const Vector3 & vec)
{
	float result;
	result = (vec.getX() * vec.getX());
	result = (result + (vec.getY() * vec.getY()));
	result = (result + (vec.getZ() * vec.getZ()));
	return result;
}

inline float length(const Vector3 & vec)
{
	return std::sqrtf(lengthSqr(vec));
}

inline const Vector3 normalize(const Vector3 & vec)
{
	float lenSqr, lenInv;
	lenSqr = lengthSqr(vec);
	lenInv = (1.0f / std::sqrtf(lenSqr));
	return Vector3((vec.getX() * lenInv),
				   (vec.getY() * lenInv),
				   (vec.getZ() * lenInv));
}

inline const Vector3 cross(const Vector3 & vec0, const Vector3 & vec1)
{
	return Vector3(((vec0.getY() * vec1.getZ()) - (vec0.getZ() * vec1.getY())),
				   ((vec0.getZ() * vec1.getX()) - (vec0.getX() * vec1.getZ())),
				   ((vec0.getX() * vec1.getY()) - (vec0.getY() * vec1.getX())));
}

inline const Vector3 select(const Vector3 & vec0, const Vector3 & vec1, bool select1)
{
	return Vector3((select1) ? vec1.getX() : vec0.getX(),
				   (select1) ? vec1.getY() : vec0.getY(),
				   (select1) ? vec1.getZ() : vec0.getZ());
}

#ifdef VECTORMATH_DEBUG

inline void print(const Vector3 & vec)
{
	std::printf("( %f %f %f )\n", vec.getX(), vec.getY(), vec.getZ());
}

inline void print(const Vector3 & vec, const char * name)
{
	std::printf("%s: ( %f %f %f )\n", name, vec.getX(), vec.getY(), vec.getZ());
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Vector4
// ========================================================

inline Vector4::Vector4(const Vector4 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
	mW = vec.mW;
}

inline Vector4::Vector4(float _x, float _y, float _z, float _w)
{
	mX = _x;
	mY = _y;
	mZ = _z;
	mW = _w;
}

inline Vector4::Vector4(const Vector3 & xyz, float _w)
{
	this->setXYZ(xyz);
	this->setW(_w);
}

inline Vector4::Vector4(const Vector3 & vec)
{
	mX = vec.getX();
	mY = vec.getY();
	mZ = vec.getZ();
	mW = 0.0f;
}

inline Vector4::Vector4(const Point3 & pnt)
{
	mX = pnt.getX();
	mY = pnt.getY();
	mZ = pnt.getZ();
	mW = 1.0f;
}

inline Vector4::Vector4(const Quat & quat)
{
	mX = quat.getX();
	mY = quat.getY();
	mZ = quat.getZ();
	mW = quat.getW();
}

inline Vector4::Vector4(float scalar)
{
	mX = scalar;
	mY = scalar;
	mZ = scalar;
	mW = scalar;
}

//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

inline const Vector4 Vector4::fromVector4Int(const Vector4Int vecInt)
{
	Vector4 ret = {};
	ret.mX = static_cast<float>(vecInt.x);
	ret.mY = static_cast<float>(vecInt.y);
	ret.mZ = static_cast<float>(vecInt.z);
	ret.mW = static_cast<float>(vecInt.w);
	return ret;
}

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================

inline const Vector4 Vector4::xAxis()
{
	return Vector4(1.0f, 0.0f, 0.0f, 0.0f);
}

inline const Vector4 Vector4::yAxis()
{
	return Vector4(0.0f, 1.0f, 0.0f, 0.0f);
}

inline const Vector4 Vector4::zAxis()
{
	return Vector4(0.0f, 0.0f, 1.0f, 0.0f);
}

inline const Vector4 Vector4::wAxis()
{
	return Vector4(0.0f, 0.0f, 0.0f, 1.0f);
}

//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

inline const Vector4 Vector4::zero()
{
	return Vector4(0.0f, 0.0f, 0.0f, 0.0f);
}

inline const Vector4 Vector4::one()
{
	return Vector4(1.0f, 1.0f, 1.0f, 1.0f);
}

inline const float * Vector4::getXPtr() const
{
	return &mX;
}

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================


inline const Vector4 lerp(float t, const Vector4 & vec0, const Vector4 & vec1)
{
	return (vec0 + ((vec1 - vec0) * t));
}

inline const Vector4 slerp(float t, const Vector4 & unitVec0, const Vector4 & unitVec1)
{
	float recipSinAngle, scale0, scale1, cosAngle, angle;
	cosAngle = dot(unitVec0, unitVec1);
	if (cosAngle < VECTORMATH_SLERP_TOL)
	{
		angle = std::acosf(cosAngle);
		recipSinAngle = (1.0f / std::sinf(angle));
		scale0 = (std::sinf(((1.0f - t) * angle)) * recipSinAngle);
		scale1 = (std::sinf((t * angle)) * recipSinAngle);
	}
	else
	{
		scale0 = (1.0f - t);
		scale1 = t;
	}
	return ((unitVec0 * scale0) + (unitVec1 * scale1));
}

inline Vector4 & Vector4::operator = (const Vector4 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
	mW = vec.mW;
	return *this;
}

inline Vector4 & Vector4::setXYZ(const Vector3 & vec)
{
	mX = vec.getX();
	mY = vec.getY();
	mZ = vec.getZ();
	return *this;
}

inline const Vector3 Vector4::getXYZ() const
{
	return Vector3(mX, mY, mZ);
}

inline Vector4 & Vector4::setX(float _x)
{
	mX = _x;
	return *this;
}

inline float Vector4::getX() const
{
	return mX;
}

inline Vector4 & Vector4::setY(float _y)
{
	mY = _y;
	return *this;
}

inline float Vector4::getY() const
{
	return mY;
}

inline Vector4 & Vector4::setZ(float _z)
{
	mZ = _z;
	return *this;
}

inline float Vector4::getZ() const
{
	return mZ;
}

inline Vector4 & Vector4::setW(float _w)
{
	mW = _w;
	return *this;
}

inline float Vector4::getW() const
{
	return mW;
}

inline Vector4 & Vector4::setElem(int idx, float value)
{
	*(&mX + idx) = value;
	return *this;
}

inline float Vector4::getElem(int idx) const
{
	return *(&mX + idx);
}

inline float & Vector4::operator[](int idx)
{
	return *(&mX + idx);
}

inline float Vector4::operator[](int idx) const
{
	return *(&mX + idx);
}

inline const Vector4 Vector4::operator + (const Vector4 & vec) const
{
	return Vector4((mX + vec.mX),
				   (mY + vec.mY),
				   (mZ + vec.mZ),
				   (mW + vec.mW));
}

inline const Vector4 Vector4::operator - (const Vector4 & vec) const
{
	return Vector4((mX - vec.mX),
				   (mY - vec.mY),
				   (mZ - vec.mZ),
				   (mW - vec.mW));
}

inline const Vector4 Vector4::operator * (float scalar) const
{
	return Vector4((mX * scalar),
				   (mY * scalar),
				   (mZ * scalar),
				   (mW * scalar));
}

inline Vector4 & Vector4::operator += (const Vector4 & vec)
{
	*this = *this + vec;
	return *this;
}

inline Vector4 & Vector4::operator -= (const Vector4 & vec)
{
	*this = *this - vec;
	return *this;
}

inline Vector4 & Vector4::operator *= (float scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const Vector4 Vector4::operator / (float scalar) const
{
	return Vector4((mX / scalar),
				   (mY / scalar),
				   (mZ / scalar),
				   (mW / scalar));
}

inline Vector4 & Vector4::operator /= (float scalar)
{
	*this = *this / scalar;
	return *this;
}

inline const Vector4 Vector4::operator - () const
{
	return Vector4(-mX, -mY, -mZ, -mW);
}

inline const Vector4 operator * (float scalar, const Vector4 & vec)
{
	return vec * scalar;
}

inline const Vector4 mulPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
	return Vector4((vec0.getX() * vec1.getX()),
				   (vec0.getY() * vec1.getY()),
				   (vec0.getZ() * vec1.getZ()),
				   (vec0.getW() * vec1.getW()));
}

inline const Vector4 divPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
	return Vector4((vec0.getX() / vec1.getX()),
				   (vec0.getY() / vec1.getY()),
				   (vec0.getZ() / vec1.getZ()),
				   (vec0.getW() / vec1.getW()));
}

inline const Vector4 recipPerElem(const Vector4 & vec)
{
	return Vector4((1.0f / vec.getX()),
				   (1.0f / vec.getY()),
				   (1.0f / vec.getZ()),
				   (1.0f / vec.getW()));
}

inline const Vector4 sqrtPerElem(const Vector4 & vec)
{
	return Vector4(std::sqrtf(vec.getX()),
				   std::sqrtf(vec.getY()),
				   std::sqrtf(vec.getZ()),
				   std::sqrtf(vec.getW()));
}


//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

#define RCP_EST(_in, _out)                 \
do {                                           \
	const float in = _in;                      \
	const union {                              \
	  float f;                                 \
	  int i;                                   \
	} uf = {in};                               \
	const union {                              \
	  int i;                                   \
	  float f;                                 \
	} ui = {(0x3f800000 * 2) - uf.i};          \
	const float fp = ui.f * (2.f - in * ui.f); \
	_out = fp * (2.f - in * fp);               \
} while (void(0), 0)

#define RSQRT_EST(_in, _out)                               \
do {                                                           \
	const float in = _in;                                      \
	union {                                                    \
	  float f;                                                 \
	  int i;                                                   \
	} uf = {in};                                               \
	union {                                                    \
	  int i;                                                   \
	  float f;                                                 \
	} ui = {0x5f3759df - (uf.i / 2)};                          \
	const float fp = ui.f * (1.5f - (in * .5f * ui.f * ui.f)); \
	_out = fp * (1.5f - (in * .5f * fp * fp));                 \
} while (void(0), 0)

#define RSQRT_EST_NR(_in, _out)                \
do {                                               \
	float fp2;                                     \
	RSQRT_EST(_in, fp2);                       \
	_out = fp2 * (1.5f - (_in * .5f * fp2 * fp2)); \
} while (void(0), 0)

inline const Vector4 rcpEst(const Vector4& v) {
  Vector4 ret;
  float vX = v.getX();
  float vY = v.getY();
  float vZ = v.getZ();
  float vW = v.getW();
  
  float rX = ret.getX();
  float rY = ret.getY();
  float rZ = ret.getZ();
  float rW = ret.getW();

  RCP_EST(vX, rX);
  RCP_EST(vY, rY);
  RCP_EST(vZ, rZ);
  RCP_EST(vW, rW);

  ret.setX(rX);
  ret.setY(rY);
  ret.setZ(rZ);
  ret.setW(rW);

  return ret;
}

inline const Vector4 rSqrtEst(const Vector4& v) {
  Vector4 ret;
  float vX = v.getX();
  float vY = v.getY();
  float vZ = v.getZ();
  float vW = v.getW();
  
  float rX = ret.getX();
  float rY = ret.getY();
  float rZ = ret.getZ();
  float rW = ret.getW();

  RSQRT_EST(vX, rX);
  RSQRT_EST(vY, rY);
  RSQRT_EST(vZ, rZ);
  RSQRT_EST(vW, rW);

  ret.setX(rX);
  ret.setY(rY);
  ret.setZ(rZ);
  ret.setW(rW);

  return ret;
}

inline const Vector4 rSqrtEstNR(const Vector4& v) {
  Vector4 ret;
  float vX = v.getX();
  float vY = v.getY();
  float vZ = v.getZ();
  float vW = v.getW();
  
  float rX = ret.getX();
  float rY = ret.getY();
  float rZ = ret.getZ();
  float rW = ret.getW();

  RSQRT_EST_NR(vX, rX);
  RSQRT_EST_NR(vY, rY);
  RSQRT_EST_NR(vZ, rZ);
  RSQRT_EST_NR(vW, rW);

  ret.setX(rX);
  ret.setY(rY);
  ret.setZ(rZ);
  ret.setW(rW);

  return ret;
}

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================

inline const Vector4 rsqrtPerElem(const Vector4 & vec)
{
	return Vector4((1.0f / std::sqrtf(vec.getX())),
				   (1.0f / std::sqrtf(vec.getY())),
				   (1.0f / std::sqrtf(vec.getZ())),
				   (1.0f / std::sqrtf(vec.getW())));
}

inline const Vector4 absPerElem(const Vector4 & vec)
{
	return Vector4(std::fabsf(vec.getX()),
				   std::fabsf(vec.getY()),
				   std::fabsf(vec.getZ()),
				   std::fabsf(vec.getW()));
}

inline const Vector4 copySignPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
	return Vector4((vec1.getX() < 0.0f) ? -std::fabsf(vec0.getX()) : std::fabsf(vec0.getX()),
				   (vec1.getY() < 0.0f) ? -std::fabsf(vec0.getY()) : std::fabsf(vec0.getY()),
				   (vec1.getZ() < 0.0f) ? -std::fabsf(vec0.getZ()) : std::fabsf(vec0.getZ()),
				   (vec1.getW() < 0.0f) ? -std::fabsf(vec0.getW()) : std::fabsf(vec0.getW()));
}

inline const Vector4 maxPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
	return Vector4((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
				   (vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY(),
				   (vec0.getZ() > vec1.getZ()) ? vec0.getZ() : vec1.getZ(),
				   (vec0.getW() > vec1.getW()) ? vec0.getW() : vec1.getW());
}

inline float maxElem(const Vector4 & vec)
{
	float result;
	result = (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() > result)     ? vec.getZ() : result;
	result = (vec.getW() > result)     ? vec.getW() : result;
	return result;
}

inline const Vector4 minPerElem(const Vector4 & vec0, const Vector4 & vec1)
{
	return Vector4((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
				   (vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY(),
				   (vec0.getZ() < vec1.getZ()) ? vec0.getZ() : vec1.getZ(),
				   (vec0.getW() < vec1.getW()) ? vec0.getW() : vec1.getW());
}

inline float minElem(const Vector4 & vec)
{
	float result;
	result = (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() < result)     ? vec.getZ() : result;
	result = (vec.getW() < result)     ? vec.getW() : result;
	return result;
}

inline float sum(const Vector4 & vec)
{
	float result;
	result = (vec.getX() + vec.getY());
	result = (result + vec.getZ());
	result = (result + vec.getW());
	return result;
}

inline float dot(const Vector4 & vec0, const Vector4 & vec1)
{
	float result;
	result = (vec0.getX() * vec1.getX());
	result = (result + (vec0.getY() * vec1.getY()));
	result = (result + (vec0.getZ() * vec1.getZ()));
	result = (result + (vec0.getW() * vec1.getW()));
	return result;
}

inline float lengthSqr(const Vector4 & vec)
{
	float result;
	result = (vec.getX() * vec.getX());
	result = (result + (vec.getY() * vec.getY()));
	result = (result + (vec.getZ() * vec.getZ()));
	result = (result + (vec.getW() * vec.getW()));
	return result;
}

inline float length(const Vector4 & vec)
{
	return std::sqrtf(lengthSqr(vec));
}

inline const Vector4 normalize(const Vector4 & vec)
{
	float lenSqr, lenInv;
	lenSqr = lengthSqr(vec);
	lenInv = (1.0f / std::sqrtf(lenSqr));
	return Vector4((vec.getX() * lenInv),
				   (vec.getY() * lenInv),
				   (vec.getZ() * lenInv),
				   (vec.getW() * lenInv));
}

inline const Vector4 select(const Vector4 & vec0, const Vector4 & vec1, bool select1)
{
	return Vector4((select1) ? vec1.getX() : vec0.getX(),
				   (select1) ? vec1.getY() : vec0.getY(),
				   (select1) ? vec1.getZ() : vec0.getZ(),
				   (select1) ? vec1.getW() : vec0.getW());
}

//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

inline const Vector4Int cmpEq(const Vector4& a, const Vector4& b) {
	const Vector4Int ret = {
		-static_cast<int>(a.getX() == b.getX()), -static_cast<int>(a.getY() == b.getY()),
		-static_cast<int>(a.getZ() == b.getZ()), -static_cast<int>(a.getW() == b.getW())};
	return ret;
}

inline const Vector4Int cmpNotEq(const Vector4& a, const Vector4& b) {
	const Vector4Int ret = {
		-static_cast<int>(a.getX() != b.getX()), -static_cast<int>(a.getY() != b.getY()),
		-static_cast<int>(a.getZ() != b.getZ()), -static_cast<int>(a.getW() != b.getW())};
	return ret;
}

inline const Vector4Int cmpLt(const Vector4& a, const Vector4& b) {
	const Vector4Int ret = {
		-static_cast<int>(a.getX() < b.getX()), -static_cast<int>(a.getY() < b.getY()),
		-static_cast<int>(a.getZ() < b.getZ()), -static_cast<int>(a.getW() < b.getW())};
	return ret;
}

inline const Vector4Int cmpLe(const Vector4& a, const Vector4& b) {
	const Vector4Int ret = {
		-static_cast<int>(a.getX() <= b.getX()), -static_cast<int>(a.getY() <= b.getY()),
		-static_cast<int>(a.getZ() <= b.getZ()), -static_cast<int>(a.getW() <= b.getW())};
	return ret;
}

inline const Vector4Int cmpGt(const Vector4& a, const Vector4& b) {
	const Vector4Int ret = {
		-static_cast<int>(a.getX() > b.getX()), -static_cast<int>(a.getY() > b.getY()),
		-static_cast<int>(a.getZ() > b.getZ()), -static_cast<int>(a.getW() > b.getW())};
	return ret;
}

inline const Vector4Int cmpGe(const Vector4& a, const Vector4& b) {
	const Vector4Int ret = {
		-static_cast<int>(a.getX() >= b.getX()), -static_cast<int>(a.getY() >= b.getY()),
		-static_cast<int>(a.getZ() >= b.getZ()), -static_cast<int>(a.getW() >= b.getW())};
	return ret;
}

inline const Vector4Int signBit(const Vector4& v) {
	VectorFI4 fi = {v};
	const Vector4Int ret = {fi.i.x & static_cast<int>(0x80000000),
						fi.i.y & static_cast<int>(0x80000000),
						fi.i.z & static_cast<int>(0x80000000),
						fi.i.w & static_cast<int>(0x80000000)};
	return ret;
}

inline const Vector4 xorPerElem(const Vector4& a, const Vector4Int& b) {
	const VectorFI4 c = {a};
	const VectorIF4 ret = {
		{c.i.x ^ b.x, c.i.y ^ b.y, c.i.z ^ b.z, c.i.w ^ b.w}};
	return ret.f;
}
	
inline const Vector4 orPerElem(const Vector4& a, const Vector4Int& b) {
	const VectorFI4 c = {a};
	const VectorIF4 ret = {
		{c.i.x | b.x, c.i.y | b.y, c.i.z | b.z, c.i.w | b.w}};
	return ret.f;
}

inline const Vector4 orPerElem(const Vector4& a, const Vector4& b) {
	const VectorFI4 c = {a};
	const VectorFI4 d = {b};
	const VectorIF4 ret = {
		{c.i.x | d.i.x, c.i.y | d.i.y, c.i.z | d.i.z, c.i.w | d.i.w}};
	return ret.f;
}

inline const Vector4 andPerElem(const Vector4& a, const Vector4Int& b) {
	const VectorFI4 c = {a};
	const VectorIF4 ret = {
		{c.i.x & b.x, c.i.y & b.y, c.i.z & b.z, c.i.w & b.w}};
	return ret.f;
}

inline float HalfToFloat(uint16_t _h) {
  const union {
	uint32_t u;
	float f;
  } magic = {(254 - 15) << 23};
  const union {
	uint32_t u;
	float f;
  } infnan = {(127 + 16) << 23};

  const uint32_t sign = _h & 0x8000;
  const union {
	int32_t u;
	float f;
  } exp_mant = {(_h & 0x7fff) << 13};
  const union {
	float f;
	uint32_t u;
  } adjust = {exp_mant.f * magic.f};
  // Make sure Inf/NaN survive
  const union {
	uint32_t u;
	float f;
  } result = {(adjust.f >= infnan.f ? (adjust.u | 255 << 23) : adjust.u) |
			  (sign << 16)};
  return result.f;
}

inline const Vector4 halfToFloat(const Vector4Int& vecInt) {

	const Vector4 ret = {
		HalfToFloat(vecInt.x & 0x0000ffff), HalfToFloat(vecInt.y & 0x0000ffff),
		HalfToFloat(vecInt.z & 0x0000ffff), HalfToFloat(vecInt.w & 0x0000ffff)};
	return ret;
}

inline void transpose3x4(const Vector4 in[3], Vector4 out[4]) {

	out[0].setX(in[0].getX());
	out[0].setY(in[1].getX());
	out[0].setZ(in[2].getX());
	out[0].setW(0.f);
	out[1].setX(in[0].getY());
	out[1].setY(in[1].getY());
	out[1].setZ(in[2].getY());
	out[1].setW(0.f);
	out[2].setX(in[0].getZ());
	out[2].setY(in[1].getZ());
	out[2].setZ(in[2].getZ());
	out[2].setW(0.f);
	out[3].setX(in[0].getW());
	out[3].setY(in[1].getW());
	out[3].setZ(in[2].getW());
	out[3].setW(0.f);
}

inline void transpose4x4(const Vector4 in[4], Vector4 out[4]) {

	out[0].setX(in[0].getX());
	out[1].setX(in[0].getY());
	out[2].setX(in[0].getZ());
	out[3].setX(in[0].getW());
	out[0].setY(in[1].getX());
	out[1].setY(in[1].getY());
	out[2].setY(in[1].getZ());
	out[3].setY(in[1].getW());
	out[0].setZ(in[2].getX());
	out[1].setZ(in[2].getY());
	out[2].setZ(in[2].getZ());
	out[3].setZ(in[2].getW());
	out[0].setW(in[3].getX());
	out[1].setW(in[3].getY());
	out[2].setW(in[3].getZ());
	out[3].setW(in[3].getW());
}

//CONFFX_TEST_BEGIN
inline void transpose4x3(const Vector4 in[4], Vector4 out[4]) {

	out[0].setX(in[0].getX());
	out[0].setY(in[1].getX());
	out[0].setZ(in[2].getX());
	out[0].setW(in[3].getX());
	out[1].setX(in[0].getY());
	out[1].setY(in[1].getY());
	out[1].setZ(in[2].getY());
	out[1].setW(in[3].getY());
	out[2].setX(in[0].getZ());
	out[2].setY(in[1].getZ());
	out[2].setZ(in[2].getZ());
	out[2].setW(in[3].getZ());
}
//CONFFX_TEST_END

inline void transpose16x16(const Vector4 in[16], Vector4 out[16]) {
	for (int i = 0; i < 4; ++i) {
		const int i4 = i * 4;
		
		out[i4 + 0].setX(*(in[0].getXPtr() + i));
		out[i4 + 0].setY(*(in[1].getXPtr() + i));
		out[i4 + 0].setZ(*(in[2].getXPtr() + i));
		out[i4 + 0].setW(*(in[3].getXPtr() + i));
		out[i4 + 1].setX(*(in[4].getXPtr() + i));
		out[i4 + 1].setY(*(in[5].getXPtr() + i));
		out[i4 + 1].setZ(*(in[6].getXPtr() + i));
		out[i4 + 1].setW(*(in[7].getXPtr() + i));
		out[i4 + 2].setX(*(in[8].getXPtr() + i));
		out[i4 + 2].setY(*(in[9].getXPtr() + i));
		out[i4 + 2].setZ(*(in[10].getXPtr() + i));
		out[i4 + 2].setW(*(in[11].getXPtr() + i));
		out[i4 + 3].setX(*(in[12].getXPtr() + i));
		out[i4 + 3].setY(*(in[13].getXPtr() + i));
		out[i4 + 3].setZ(*(in[14].getXPtr() + i));
		out[i4 + 3].setW(*(in[15].getXPtr() + i));
	}
}

inline void storePtrU(const Vector4& v, float* f) {
	f[0] = v.getX();
	f[1] = v.getY();
	f[2] = v.getZ();
	f[3] = v.getW();
}

inline void store3PtrU(const Vector4& v, float* f) {
	f[0] = v.getX();
	f[1] = v.getY();
	f[2] = v.getZ();
}

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================
//========================================= #TheForgeMathExtensionsEnd ================================================


#ifdef VECTORMATH_DEBUG

inline void print(const Vector4 & vec)
{
	std::printf("( %f %f %f %f )\n", vec.getX(), vec.getY(), vec.getZ(), vec.getW());
}

inline void print(const Vector4 & vec, const char * name)
{
	std::printf("%s: ( %f %f %f %f )\n", name, vec.getX(), vec.getY(), vec.getZ(), vec.getW());
}

#endif // VECTORMATH_DEBUG

// ========================================================
// Point3
// ========================================================

inline Point3::Point3(const Point3 & pnt)
{
	mX = pnt.mX;
	mY = pnt.mY;
	mZ = pnt.mZ;
}

inline Point3::Point3(float _x, float _y, float _z)
{
	mX = _x;
	mY = _y;
	mZ = _z;
}

inline Point3::Point3(const Vector3 & vec)
{
	mX = vec.getX();
	mY = vec.getY();
	mZ = vec.getZ();
}

inline Point3::Point3(float scalar)
{
	mX = scalar;
	mY = scalar;
	mZ = scalar;
}

inline const Point3 lerp(float t, const Point3 & pnt0, const Point3 & pnt1)
{
	return (pnt0 + ((pnt1 - pnt0) * t));
}

inline Point3 & Point3::operator = (const Point3 & pnt)
{
	mX = pnt.mX;
	mY = pnt.mY;
	mZ = pnt.mZ;
	return *this;
}

inline Point3 & Point3::setX(float _x)
{
	mX = _x;
	return *this;
}

inline float Point3::getX() const
{
	return mX;
}

inline Point3 & Point3::setY(float _y)
{
	mY = _y;
	return *this;
}

inline float Point3::getY() const
{
	return mY;
}

inline Point3 & Point3::setZ(float _z)
{
	mZ = _z;
	return *this;
}

inline float Point3::getZ() const
{
	return mZ;
}

inline Point3 & Point3::setW(float _w)
{
	mW = _w;
	return *this;
}

inline float Point3::getW() const
{
	return mW;
}

inline Point3 & Point3::setElem(int idx, float value)
{
	*(&mX + idx) = value;
	return *this;
}

inline float Point3::getElem(int idx) const
{
	return *(&mX + idx);
}

inline float & Point3::operator[](int idx)
{
	return *(&mX + idx);
}

inline float Point3::operator[](int idx) const
{
	return *(&mX + idx);
}

inline const Vector3 Point3::operator - (const Point3 & pnt) const
{
	return Vector3((mX - pnt.mX), (mY - pnt.mY), (mZ - pnt.mZ));
}

inline const Point3 Point3::operator + (const Vector3 & vec) const
{
	return Point3((mX + vec.getX()), (mY + vec.getY()), (mZ + vec.getZ()));
}

inline const Point3 Point3::operator - (const Vector3 & vec) const
{
	return Point3((mX - vec.getX()), (mY - vec.getY()), (mZ - vec.getZ()));
}

inline Point3 & Point3::operator += (const Vector3 & vec)
{
	*this = *this + vec;
	return *this;
}

inline Point3 & Point3::operator -= (const Vector3 & vec)
{
	*this = *this - vec;
	return *this;
}

inline const Point3 mulPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
	return Point3((pnt0.getX() * pnt1.getX()),
				  (pnt0.getY() * pnt1.getY()),
				  (pnt0.getZ() * pnt1.getZ()));
}

inline const Point3 divPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
	return Point3((pnt0.getX() / pnt1.getX()),
				  (pnt0.getY() / pnt1.getY()),
				  (pnt0.getZ() / pnt1.getZ()));
}

inline const Point3 recipPerElem(const Point3 & pnt)
{
	return Point3((1.0f / pnt.getX()),
				  (1.0f / pnt.getY()),
				  (1.0f / pnt.getZ()));
}

inline const Point3 sqrtPerElem(const Point3 & pnt)
{
	return Point3(std::sqrtf(pnt.getX()),
				  std::sqrtf(pnt.getY()),
				  std::sqrtf(pnt.getZ()));
}

inline const Point3 rsqrtPerElem(const Point3 & pnt)
{
	return Point3((1.0f / std::sqrtf(pnt.getX())),
				  (1.0f / std::sqrtf(pnt.getY())),
				  (1.0f / std::sqrtf(pnt.getZ())));
}

inline const Point3 absPerElem(const Point3 & pnt)
{
	return Point3(std::fabsf(pnt.getX()),
				  std::fabsf(pnt.getY()),
				  std::fabsf(pnt.getZ()));
}

inline const Point3 copySignPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
	return Point3((pnt1.getX() < 0.0f) ? -std::fabsf(pnt0.getX()) : std::fabsf(pnt0.getX()),
				  (pnt1.getY() < 0.0f) ? -std::fabsf(pnt0.getY()) : std::fabsf(pnt0.getY()),
				  (pnt1.getZ() < 0.0f) ? -std::fabsf(pnt0.getZ()) : std::fabsf(pnt0.getZ()));
}

inline const Point3 maxPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
	return Point3((pnt0.getX() > pnt1.getX()) ? pnt0.getX() : pnt1.getX(),
				  (pnt0.getY() > pnt1.getY()) ? pnt0.getY() : pnt1.getY(),
				  (pnt0.getZ() > pnt1.getZ()) ? pnt0.getZ() : pnt1.getZ());
}

inline float maxElem(const Point3 & pnt)
{
	float result;
	result = (pnt.getX() > pnt.getY()) ? pnt.getX() : pnt.getY();
	result = (pnt.getZ() > result)     ? pnt.getZ() : result;
	return result;
}

inline const Point3 minPerElem(const Point3 & pnt0, const Point3 & pnt1)
{
	return Point3((pnt0.getX() < pnt1.getX()) ? pnt0.getX() : pnt1.getX(),
				  (pnt0.getY() < pnt1.getY()) ? pnt0.getY() : pnt1.getY(),
				  (pnt0.getZ() < pnt1.getZ()) ? pnt0.getZ() : pnt1.getZ());
}

inline float minElem(const Point3 & pnt)
{
	float result;
	result = (pnt.getX() < pnt.getY()) ? pnt.getX() : pnt.getY();
	result = (pnt.getZ() < result)     ? pnt.getZ() : result;
	return result;
}

inline float sum(const Point3 & pnt)
{
	float result;
	result = (pnt.getX() + pnt.getY());
	result = (result + pnt.getZ());
	return result;
}

inline const Point3 scale(const Point3 & pnt, float scaleVal)
{
	return mulPerElem(pnt, Point3(scaleVal));
}

inline const Point3 scale(const Point3 & pnt, const Vector3 & scaleVec)
{
	return mulPerElem(pnt, Point3(scaleVec));
}

inline float projection(const Point3 & pnt, const Vector3 & unitVec)
{
	float result;
	result = (pnt.getX() * unitVec.getX());
	result = (result + (pnt.getY() * unitVec.getY()));
	result = (result + (pnt.getZ() * unitVec.getZ()));
	return result;
}

inline float distSqrFromOrigin(const Point3 & pnt)
{
	return lengthSqr(Vector3(pnt));
}

inline float distFromOrigin(const Point3 & pnt)
{
	return length(Vector3(pnt));
}

inline float distSqr(const Point3 & pnt0, const Point3 & pnt1)
{
	return lengthSqr(pnt1 - pnt0);
}

inline float dist(const Point3 & pnt0, const Point3 & pnt1)
{
	return length(pnt1 - pnt0);
}

inline const Point3 select(const Point3 & pnt0, const Point3 & pnt1, bool select1)
{
	return Point3((select1) ? pnt1.getX() : pnt0.getX(),
				  (select1) ? pnt1.getY() : pnt0.getY(),
				  (select1) ? pnt1.getZ() : pnt0.getZ());
}

#ifdef VECTORMATH_DEBUG

inline void print(const Point3 & pnt)
{
	std::printf("( %f %f %f )\n", pnt.getX(), pnt.getY(), pnt.getZ());
}

inline void print(const Point3 & pnt, const char * name)
{
	std::printf("%s: ( %f %f %f )\n", name, pnt.getX(), pnt.getY(), pnt.getZ());
}

#endif // VECTORMATH_DEBUG


//========================================= #TheForgeMathExtensionsBegin ================================================
//========================================= #TheForgeAnimationMathExtensionsBegin =======================================

// ========================================================
// Vector4Int
// ========================================================

namespace vector4int {

inline Vector4Int zero() {
  const Vector4Int ret = {0, 0, 0, 0};
  return ret;
}

inline Vector4Int one() {
  const Vector4Int ret = {1, 1, 1, 1};
  return ret;
}

inline Vector4Int x_axis() {
  const Vector4Int ret = {1, 0, 0, 0};
  return ret;
}

inline Vector4Int y_axis() {
  const Vector4Int ret = {0, 1, 0, 0};
  return ret;
}

inline Vector4Int z_axis() {
  const Vector4Int ret = {0, 0, 1, 0};
  return ret;
}

inline Vector4Int w_axis() {
  const Vector4Int ret = {0, 0, 0, 1};
  return ret;
}

inline Vector4Int all_true() {
  const Vector4Int ret = {~0, ~0, ~0, ~0};
  return ret;
}

inline Vector4Int all_false() {
  const Vector4Int ret = {0, 0, 0, 0};
  return ret;
}

inline Vector4Int mask_sign() {
  const Vector4Int ret = {
	  static_cast<int>(0x80000000), static_cast<int>(0x80000000),
	  static_cast<int>(0x80000000), static_cast<int>(0x80000000)};
  return ret;
}

inline Vector4Int mask_not_sign() {
  const Vector4Int ret = {
	  static_cast<int>(0x7fffffff), static_cast<int>(0x7fffffff),
	  static_cast<int>(0x7fffffff), static_cast<int>(0x7fffffff)};
  return ret;
}

inline Vector4Int mask_ffff() {
  const Vector4Int ret = {~0, ~0, ~0, ~0};
  return ret;
}

inline Vector4Int mask_fff0() {
  const Vector4Int ret = {~0, ~0, ~0, 0};
  return ret;
}

inline Vector4Int mask_0000() {
  const Vector4Int ret = {0, 0, 0, 0};
  return ret;
}

inline Vector4Int mask_f000() {
  const Vector4Int ret = {~0, 0, 0, 0};
  return ret;
}

inline Vector4Int mask_0f00() {
  const Vector4Int ret = {0, ~0, 0, 0};
  return ret;
}

inline Vector4Int mask_00f0() {
  const Vector4Int ret = {0, 0, ~0, 0};
  return ret;
}

inline Vector4Int mask_000f() {
  const Vector4Int ret = {0, 0, 0, ~0};
  return ret;
}

inline Vector4Int Load(int _x, int _y, int _z, int _w) {
  const Vector4Int ret = {_x, _y, _z, _w};
  return ret;
}

inline Vector4Int LoadX(int _x) {
  const Vector4Int ret = {_x, 0, 0, 0};
  return ret;
}

inline Vector4Int Load1(int _x) {
  const Vector4Int ret = {_x, _x, _x, _x};
  return ret;
}

inline Vector4Int Load(bool _x, bool _y, bool _z, bool _w) {
  const Vector4Int ret = {-static_cast<int>(_x), -static_cast<int>(_y),
						-static_cast<int>(_z), -static_cast<int>(_w)};
  return ret;
}

inline Vector4Int LoadX(bool _x) {
  const Vector4Int ret = {-static_cast<int>(_x), 0, 0, 0};
  return ret;
}

inline Vector4Int Load1(bool _x) {
  const int i = -static_cast<int>(_x);
  const Vector4Int ret = {i, i, i, i};
  return ret;
}

inline Vector4Int LoadPtr(const int* _i) {
  const Vector4Int ret = {_i[0], _i[1], _i[2], _i[3]};
  return ret;
}

inline Vector4Int LoadXPtr(const int* _i) {
  const Vector4Int ret = {*_i, 0, 0, 0};
  return ret;
}

inline Vector4Int Load1Ptr(const int* _i) {
  const Vector4Int ret = {*_i, *_i, *_i, *_i};
  return ret;
}

inline Vector4Int Load2Ptr(const int* _i) {
  const Vector4Int ret = {_i[0], _i[1], 0, 0};
  return ret;
}

inline Vector4Int Load3Ptr(const int* _i) {
  const Vector4Int ret = {_i[0], _i[1], _i[2], 0};
  return ret;
}

inline Vector4Int LoadPtrU(const int* _i) {
  const Vector4Int ret = {_i[0], _i[1], _i[2], _i[3]};
  return ret;
}

inline Vector4Int LoadXPtrU(const int* _i) {
  const Vector4Int ret = {*_i, 0, 0, 0};
  return ret;
}

inline Vector4Int Load1PtrU(const int* _i) {
  const Vector4Int ret = {*_i, *_i, *_i, *_i};
  return ret;
}

inline Vector4Int Load2PtrU(const int* _i) {
  const Vector4Int ret = {_i[0], _i[1], 0, 0};
  return ret;
}

inline Vector4Int Load3PtrU(const int* _i) {
  const Vector4Int ret = {_i[0], _i[1], _i[2], 0};
  return ret;
}

inline Vector4Int FromFloatRound(const Vector4& _f) {
  const Vector4Int ret = {
	  static_cast<int>(floor(_f.getX() + .5f)), static_cast<int>(floor(_f.getY() + .5f)),
	  static_cast<int>(floor(_f.getZ() + .5f)), static_cast<int>(floor(_f.getW() + .5f))};
  return ret;
}

inline Vector4Int FromFloatTrunc(const Vector4& _f) {
  const Vector4Int ret = {static_cast<int>(_f.getX()), static_cast<int>(_f.getY()),
						static_cast<int>(_f.getZ()), static_cast<int>(_f.getW())};
  return ret;
}

}  // namespace vector4int

inline int GetX(const Vector4Int& _v) { return _v.x; }

inline int GetY(const Vector4Int& _v) { return _v.y; }

inline int GetZ(const Vector4Int& _v) { return _v.z; }

inline int GetW(const Vector4Int& _v) { return _v.w; }

inline Vector4Int SetX(const Vector4Int& _v, int _i) {
  const Vector4Int ret = {_i, _v.y, _v.z, _v.w};
  return ret;
}

inline Vector4Int SetY(const Vector4Int& _v, int _i) {
  const Vector4Int ret = {_v.x, _i, _v.z, _v.w};
  return ret;
}

inline Vector4Int SetZ(const Vector4Int& _v, int _i) {
  const Vector4Int ret = {_v.x, _v.y, _i, _v.w};
  return ret;
}

inline Vector4Int SetW(const Vector4Int& _v, int _i) {
  const Vector4Int ret = {_v.x, _v.y, _v.z, _i};
  return ret;
}

inline Vector4Int SetI(const Vector4Int& _v, int _ith, int _i) {
  Vector4Int ret = _v;
  (&ret.x)[_ith] = _i;
  return ret;
}

inline void StorePtr(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
  _i[1] = _v.y;
  _i[2] = _v.z;
  _i[3] = _v.w;
}

inline void Store1Ptr(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
}

inline void Store2Ptr(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
  _i[1] = _v.y;
}

inline void Store3Ptr(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
  _i[1] = _v.y;
  _i[2] = _v.z;
}

inline void StorePtrU(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
  _i[1] = _v.y;
  _i[2] = _v.z;
  _i[3] = _v.w;
}

inline void Store1PtrU(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
}

inline void Store2PtrU(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
  _i[1] = _v.y;
}

inline void Store3PtrU(const Vector4Int& _v, int* _i) {
  _i[0] = _v.x;
  _i[1] = _v.y;
  _i[2] = _v.z;
}

inline Vector4Int SplatX(const Vector4Int& _a) {
  const Vector4Int ret = {_a.x, _a.x, _a.x, _a.x};
  return ret;
}

inline Vector4Int SplatY(const Vector4Int& _a) {
  const Vector4Int ret = {_a.y, _a.y, _a.y, _a.y};
  return ret;
}

inline Vector4Int SplatZ(const Vector4Int& _a) {
  const Vector4Int ret = {_a.z, _a.z, _a.z, _a.z};
  return ret;
}

inline Vector4Int SplatW(const Vector4Int& _a) {
  const Vector4Int ret = {_a.w, _a.w, _a.w, _a.w};
  return ret;
}

inline int MoveMask(const Vector4Int& _v) {
  return ((_v.x & 0x80000000) >> 31) | ((_v.y & 0x80000000) >> 30) |
		 ((_v.z & 0x80000000) >> 29) | ((_v.w & 0x80000000) >> 28);
}

inline bool AreAllTrue(const Vector4Int& _v) {
  return _v.x != 0 && _v.y != 0 && _v.z != 0 && _v.w != 0;
}

inline bool AreAllTrue3(const Vector4Int& _v) {
  return _v.x != 0 && _v.y != 0 && _v.z != 0;
}

inline bool AreAllTrue2(const Vector4Int& _v) { return _v.x != 0 && _v.y != 0; }

inline bool AreAllTrue1(const Vector4Int& _v) { return _v.x != 0; }

inline bool AreAllFalse(const Vector4Int& _v) {
  return _v.x == 0 && _v.y == 0 && _v.z == 0 && _v.w == 0;
}

inline bool AreAllFalse3(const Vector4Int& _v) {
  return _v.x == 0 && _v.y == 0 && _v.z == 0;
}

inline bool AreAllFalse2(const Vector4Int& _v) { return _v.x == 0 && _v.y == 0; }

inline bool AreAllFalse1(const Vector4Int& _v) { return _v.x == 0; }

inline Vector4Int MAdd(const Vector4Int& _a, const Vector4Int& _b, const Vector4Int& _addend) {
  const Vector4Int ret = {_a.x * _b.x + _addend.x, _a.y * _b.y + _addend.y,
						_a.z * _b.z + _addend.z, _a.w * _b.w + _addend.w};
  return ret;
}

inline Vector4Int DivX(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x / _b.x, _a.y, _a.z, _a.w};
  return ret;
}

inline Vector4Int HAdd2(const Vector4Int& _v) {
  const Vector4Int ret = {_v.x + _v.y, _v.y, _v.z, _v.w};
  return ret;
}

inline Vector4Int HAdd3(const Vector4Int& _v) {
  const Vector4Int ret = {_v.x + _v.y + _v.z, _v.y, _v.z, _v.w};
  return ret;
}

inline Vector4Int HAdd4(const Vector4Int& _v) {
  const Vector4Int ret = {_v.x + _v.y + _v.z + _v.w, _v.y, _v.z, _v.w};
  return ret;
}

inline Vector4Int Dot2(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x * _b.x + _a.y * _b.y, _a.y, _a.z, _a.w};
  return ret;
}

inline Vector4Int Dot3(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x * _b.x + _a.y * _b.y + _a.z * _b.z, _a.y, _a.z,
						_a.w};
  return ret;
}

inline Vector4Int Dot4(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x * _b.x + _a.y * _b.y + _a.z * _b.z + _a.w * _b.w,
						_a.y, _a.z, _a.w};
  return ret;
}
inline Vector4Int Abs(const Vector4Int& _v) {
  const Vector4Int mash = {_v.x >> 31, _v.y >> 31, _v.z >> 31, _v.w >> 31};
  const Vector4Int ret = {
	  (_v.x + (mash.x)) ^ (mash.x), (_v.y + (mash.y)) ^ (mash.y),
	  (_v.z + (mash.z)) ^ (mash.z), (_v.w + (mash.w)) ^ (mash.w)};
  return ret;
}

inline Vector4Int Sign(const Vector4Int& _v) {
  const Vector4Int ret = {
	  _v.x & static_cast<int>(0x80000000), _v.y & static_cast<int>(0x80000000),
	  _v.z & static_cast<int>(0x80000000), _v.w & static_cast<int>(0x80000000)};
  return ret;
}

inline Vector4Int Min(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x < _b.x ? _a.x : _b.x, _a.y < _b.y ? _a.y : _b.y,
						_a.z < _b.z ? _a.z : _b.z, _a.w < _b.w ? _a.w : _b.w};
  return ret;
}

inline Vector4Int Max(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x > _b.x ? _a.x : _b.x, _a.y > _b.y ? _a.y : _b.y,
						_a.z > _b.z ? _a.z : _b.z, _a.w > _b.w ? _a.w : _b.w};
  return ret;
}

inline Vector4Int Min0(const Vector4Int& _v) {
  const Vector4Int ret = {_v.x < 0 ? _v.x : 0, _v.y < 0 ? _v.y : 0,
						_v.z < 0 ? _v.z : 0, _v.w < 0 ? _v.w : 0};
  return ret;
}

inline Vector4Int Max0(const Vector4Int& _v) {
  const Vector4Int ret = {_v.x > 0 ? _v.x : 0, _v.y > 0 ? _v.y : 0,
						_v.z > 0 ? _v.z : 0, _v.w > 0 ? _v.w : 0};
  return ret;
}

inline Vector4Int Clamp(const Vector4Int& _a, const Vector4Int& _v, const Vector4Int& _b) {
  const Vector4Int min = {_v.x < _b.x ? _v.x : _b.x, _v.y < _b.y ? _v.y : _b.y,
						_v.z < _b.z ? _v.z : _b.z, _v.w < _b.w ? _v.w : _b.w};
  const Vector4Int r = {_a.x > min.x ? _a.x : min.x, _a.y > min.y ? _a.y : min.y,
					  _a.z > min.z ? _a.z : min.z, _a.w > min.w ? _a.w : min.w};
  return r;
}

inline Vector4Int Select(const Vector4Int& _b, const Vector4Int& _true, const Vector4Int& _false) {
  const Vector4Int ret = {_false.x ^ (_b.x & (_true.x ^ _false.x)),
						_false.y ^ (_b.y & (_true.y ^ _false.y)),
						_false.z ^ (_b.z & (_true.z ^ _false.z)),
						_false.w ^ (_b.w & (_true.w ^ _false.w))};
  return ret;
}

inline Vector4Int And(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x & _b.x, _a.y & _b.y, _a.z & _b.z, _a.w & _b.w};
  return ret;
}

inline Vector4Int Or(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x | _b.x, _a.y | _b.y, _a.z | _b.z, _a.w | _b.w};
  return ret;
}

inline Vector4Int Xor(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {_a.x ^ _b.x, _a.y ^ _b.y, _a.z ^ _b.z, _a.w ^ _b.w};
  return ret;
}

inline Vector4Int Not(const Vector4Int& _v) {
  const Vector4Int ret = {~_v.x, ~_v.y, ~_v.z, ~_v.w};
  return ret;
}

inline Vector4Int ShiftL(const Vector4Int& _v, int _bits) {
  const Vector4Int ret = {_v.x << _bits, _v.y << _bits, _v.z << _bits,
						_v.w << _bits};
  return ret;
}

inline Vector4Int ShiftR(const Vector4Int& _v, int _bits) {
  const Vector4Int ret = {_v.x >> _bits, _v.y >> _bits, _v.z >> _bits,
						_v.w >> _bits};
  return ret;
}

inline Vector4Int ShiftRu(const Vector4Int& _v, int _bits) {
  const union IU {
	int i[4];
	unsigned int u[4];
  } iu = {{_v.x, _v.y, _v.z, _v.w}};
  const union UI {
	unsigned int u[4];
	int i[4];
  } ui = {
	  {iu.u[0] >> _bits, iu.u[1] >> _bits, iu.u[2] >> _bits, iu.u[3] >> _bits}};
  const Vector4Int ret = {ui.i[0], ui.i[1], ui.i[2], ui.i[3]};
  return ret;
}

inline Vector4Int CmpEq(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {
	  -static_cast<int>(_a.x == _b.x), -static_cast<int>(_a.y == _b.y),
	  -static_cast<int>(_a.z == _b.z), -static_cast<int>(_a.w == _b.w)};
  return ret;
}

inline Vector4Int CmpNe(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {
	  -static_cast<int>(_a.x != _b.x), -static_cast<int>(_a.y != _b.y),
	  -static_cast<int>(_a.z != _b.z), -static_cast<int>(_a.w != _b.w)};
  return ret;
}

inline Vector4Int CmpLt(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {
	  -static_cast<int>(_a.x < _b.x), -static_cast<int>(_a.y < _b.y),
	  -static_cast<int>(_a.z < _b.z), -static_cast<int>(_a.w < _b.w)};
  return ret;
}

inline Vector4Int CmpLe(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {
	  -static_cast<int>(_a.x <= _b.x), -static_cast<int>(_a.y <= _b.y),
	  -static_cast<int>(_a.z <= _b.z), -static_cast<int>(_a.w <= _b.w)};
  return ret;
}

inline Vector4Int CmpGt(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {
	  -static_cast<int>(_a.x > _b.x), -static_cast<int>(_a.y > _b.y),
	  -static_cast<int>(_a.z > _b.z), -static_cast<int>(_a.w > _b.w)};
  return ret;
}

inline Vector4Int CmpGe(const Vector4Int& _a, const Vector4Int& _b) {
  const Vector4Int ret = {
	  -static_cast<int>(_a.x >= _b.x), -static_cast<int>(_a.y >= _b.y),
	  -static_cast<int>(_a.z >= _b.z), -static_cast<int>(_a.w >= _b.w)};
  return ret;
}

//========================================= #TheForgeAnimationMathExtensionsEnd =======================================

// ========================================================
// IVector3
// ========================================================

inline IVector3::IVector3(int _x, int _y, int _z)
{
	mX = _x;
	mY = _y;
	mZ = _z;
	mW = 0;
}

inline IVector3::IVector3(int scalar)
{
	mX = mY = mZ = scalar;
	mW = 0;
}

inline const IVector3 IVector3::xAxis()
{
	return IVector3(1, 0, 0);
}

inline const IVector3 IVector3::yAxis()
{
	return IVector3(0, 1, 0);
}

inline const IVector3 IVector3::zAxis()
{
	return IVector3(0, 0, 1);
}

inline IVector3 & IVector3::operator = (const IVector3 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
	mW = vec.mW;
	return *this;
}

inline IVector3 & IVector3::setX(int _x)
{
	mX = _x;
	return *this;
}

inline const int IVector3::getX() const
{
	return mX;
}

inline IVector3 & IVector3::setY(int _y)
{
	mY = _y;
	return *this;
}

inline const int IVector3::getY() const
{
	return mY;
}

inline IVector3 & IVector3::setZ(int _z)
{
	mZ = _z;
	return *this;
}

inline const int IVector3::getZ() const
{
	return mZ;
}

inline IVector3 & IVector3::setW(int _w)
{
	mW = _w;
	return *this;
}

inline const int IVector3::getW() const
{
	return mW;
}

inline IVector3 & IVector3::setElem(int idx, int value)
{
	((int*)(this))[idx] = value;
	return *this;
}

inline const int IVector3::getElem(int idx) const
{
	return ((int*)(this))[idx];
}

inline int& IVector3::operator[](int idx)
{
	return ((int*)(this))[idx];
}

inline const int IVector3::operator[](int idx) const
{
	return ((int*)(this))[idx];
}

inline const IVector3 IVector3::operator + (const IVector3 & vec) const
{
	return IVector3(mX + vec.mX, mY + vec.mY, mZ + vec.mZ);
}

inline const IVector3 IVector3::operator - (const IVector3 & vec) const
{
	return IVector3(mX - vec.mX, mY - vec.mY, mZ - vec.mZ);
}

inline const IVector3 IVector3::operator * (int scalar) const
{
	return IVector3(mX * scalar, mY * scalar, mZ * scalar);
}

inline IVector3 & IVector3::operator += (const IVector3 & vec)
{
	*this = *this + vec;
	return *this;
}

inline IVector3 & IVector3::operator -= (const IVector3 & vec)
{
	*this = *this - vec;
	return *this;
}

inline IVector3 & IVector3::operator *= (int scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const IVector3 IVector3::operator / (int scalar) const
{
	return IVector3(mX / scalar, mY / scalar, mZ / scalar);
}

inline IVector3 & IVector3::operator /= (int scalar)
{
	*this = *this / scalar;
	return *this;
}

inline const IVector3 IVector3::operator - () const
{
	return IVector3(-mX, -mY, -mZ);
}

inline const IVector3 operator * (int scalar, const IVector3 & vec)
{
	return IVector3(vec.getX() * scalar, vec.getY() * scalar, vec.getZ() * scalar);
}

inline const IVector3 mulPerElem(const IVector3 & vec0, const IVector3 & vec1)
{
	return IVector3(vec0.getX() * vec1.getX(), vec0.getY() * vec1.getX(), vec0.getZ() * vec1.getZ());
}

inline const IVector3 divPerElem(const IVector3 & vec0, const IVector3 & vec1)
{
	return IVector3(vec0.getX() / vec1.getX(), vec0.getY() / vec1.getX(), vec0.getZ() / vec1.getZ());
}

inline const IVector3 absPerElem(const IVector3 & vec)
{
	return IVector3(vec.getX() < 0 ? -vec.getX() : vec.getX(),
					vec.getY() < 0 ? -vec.getY() : vec.getY(),
					vec.getZ() < 0 ? -vec.getZ() : vec.getZ());
}

inline const IVector3 copySignPerElem(const IVector3 & vec0, const IVector3 & vec1)
{
	IVector3 vAbs = absPerElem(vec0);
	return IVector3(vec1.getX() < 0.0f ? -vAbs.getX() : vAbs.getX(),
		vec1.getY() < 0.0f ? -vAbs.getY() : vAbs.getY(),
		vec1.getZ() < 0.0f ? -vAbs.getZ() : vAbs.getZ());
}

inline const IVector3 maxPerElem(const IVector3 & vec0, const IVector3 & vec1)
{
	return IVector3((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() > vec1.getZ()) ? vec0.getZ() : vec1.getZ());
}

inline const int maxElem(const IVector3 & vec)
{
	int result;
	result = (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() > result) ? vec.getZ() : result;
	return result;
}

inline const IVector3 minPerElem(const IVector3 & vec0, const IVector3 & vec1)
{
	return IVector3((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() < vec1.getZ()) ? vec0.getZ() : vec1.getZ());
}

inline const int minElem(const IVector3 & vec)
{
	int result;
	result = (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() < result) ? vec.getZ() : result;
	return result;
}

inline const int sum(const IVector3 & vec)
{
	return vec.getX() + vec.getY() + vec.getZ();
}

#ifdef VECTORMATH_DEBUG

inline void print(const IVector3 & vec)
{
	std::printf("( %i %i %i )\n", vec.getX(), vec.getY(), vec.getZ());
}

inline void print(const IVector3 & vec, const char * name)
{
	std::printf("%s: ( %i %i %i )\n", name, vec.getX(), vec.getY(), vec.getZ());
}

#endif // VECTORMATH_DEBUG



// ========================================================
// UVector3
// ========================================================

inline UVector3::UVector3(uint _x, uint _y, uint _z)
{
	mX = _x;
	mY = _y;
	mZ = _z;
	mW = 0;
}

inline UVector3::UVector3(uint scalar)
{
	mX = mY = mZ = scalar;
	mW = 0;
}

inline const UVector3 UVector3::xAxis()
{
	return UVector3(1, 0, 0);
}

inline const UVector3 UVector3::yAxis()
{
	return UVector3(0, 1, 0);
}

inline const UVector3 UVector3::zAxis()
{
	return UVector3(0, 0, 1);
}

inline UVector3 & UVector3::operator = (const UVector3 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
	mW = vec.mW;
	return *this;
}

inline UVector3 & UVector3::setX(uint _x)
{
	mX = _x;
	return *this;
}

inline const uint UVector3::getX() const
{
	return mX;
}

inline UVector3 & UVector3::setY(uint _y)
{
	mY = _y;
	return *this;
}

inline const uint UVector3::getY() const
{
	return mY;
}

inline UVector3 & UVector3::setZ(uint _z)
{
	mZ = _z;
	return *this;
}

inline const uint UVector3::getZ() const
{
	return mZ;
}

inline UVector3 & UVector3::setW(uint _w)
{
	mW = _w;
	return *this;
}

inline const uint UVector3::getW() const
{
	return mW;
}

inline UVector3 & UVector3::setElem(uint idx, uint value)
{
	((uint*)(this))[idx] = value;
	return *this;
}

inline const uint UVector3::getElem(uint idx) const
{
	return ((uint*)this)[idx];
}

inline uint& UVector3::operator[](uint idx)
{
	return ((uint*)this)[idx];
}

inline const uint UVector3::operator[](uint idx) const
{
	return ((uint*)this)[idx];
}

inline const UVector3 UVector3::operator + (const UVector3 & vec) const
{
	return UVector3(mX + vec.mX, mY + vec.mY, mZ + vec.mZ);
}

inline const UVector3 UVector3::operator - (const UVector3 & vec) const
{
	return UVector3(mX - vec.mX, mY - vec.mY, mZ - vec.mZ);
}

inline const UVector3 UVector3::operator * (uint scalar) const
{
	return UVector3(mX * scalar, mY * scalar, mZ * scalar);
}

inline UVector3 & UVector3::operator += (const UVector3 & vec)
{
	*this = *this + vec;
	return *this;
}

inline UVector3 & UVector3::operator -= (const UVector3 & vec)
{
	*this = *this - vec;
	return *this;
}

inline UVector3 & UVector3::operator *= (uint scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const UVector3 UVector3::operator / (uint scalar) const
{
	return UVector3(mX / scalar, mY / scalar, mZ / scalar);
}

inline UVector3 & UVector3::operator /= (uint scalar)
{
	*this = *this / scalar;
	return *this;
}

inline const UVector3 operator * (uint scalar, const UVector3 & vec)
{
	return UVector3(scalar * vec.getX(), scalar * vec.getY(), scalar * vec.getZ());
}

inline const UVector3 mulPerElem(const UVector3 & vec0, const UVector3 & vec1)
{
	return UVector3(vec0.getX() * vec1.getX(), vec0.getY() * vec1.getY(), vec0.getZ() * vec1.getZ());
}

inline const UVector3 divPerElem(const UVector3 & vec0, const UVector3 & vec1)
{
	return UVector3(vec0.getX() / vec1.getX(), vec0.getY() / vec1.getY(), vec0.getZ() / vec1.getZ());
}

inline const UVector3 maxPerElem(const UVector3 & vec0, const UVector3 & vec1)
{
	return UVector3((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() > vec1.getZ()) ? vec0.getZ() : vec1.getZ());
}

inline const uint maxElem(const UVector3 & vec)
{
	uint result;
	result = (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() > result) ? vec.getZ() : result;
	return result;
}

inline const UVector3 minPerElem(const UVector3 & vec0, const UVector3 & vec1)
{
	return UVector3((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() < vec1.getZ()) ? vec0.getZ() : vec1.getZ());
}

inline const uint minElem(const UVector3 & vec)
{
	uint result;
	result = (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() < result) ? vec.getZ() : result;
	return result;
}

inline const uint sum(const UVector3 & vec)
{
	return vec.getX() + vec.getY() + vec.getZ();
}

#ifdef VECTORMATH_DEBUG

inline void print(const UVector3 & vec)
{
	std::printf("( %u %u %u )\n", vec.getX(), vec.getY(), vec.getZ());
}

inline void print(const UVector3 & vec, const char * name)
{
	std::printf("%s: ( %u %u %u )\n", name, vec.getX(), vec.getY(), vec.getZ());
}

#endif // VECTORMATH_DEBUG



// ========================================================
// IVector4
// ========================================================

inline IVector4::IVector4(int _x, int _y, int _z, int _w)
{
	mX = _x;
	mY = _y;
	mZ = _z;
	mW = _w;
}

inline IVector4::IVector4(int scalar)
{
	mX = mY = mZ = mW = scalar;
}

inline const IVector4 IVector4::xAxis()
{
	return IVector4(1, 0, 0, 0);
}

inline const IVector4 IVector4::yAxis()
{
	return IVector4(0, 1, 0, 0);
}

inline const IVector4 IVector4::zAxis()
{
	return IVector4(0, 0, 1, 0);
}

inline const IVector4 IVector4::wAxis()
{
	return IVector4(0, 0, 0, 1);
}

inline IVector4 & IVector4::operator = (const IVector4 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
	mW = vec.mW;
	return *this;
}

inline IVector4 & IVector4::setX(int _x)
{
	mX = _x;
	return *this;
}

inline const int IVector4::getX() const
{
	return mX;
}

inline IVector4 & IVector4::setY(int _y)
{
	mY = _y;
	return *this;
}

inline const int IVector4::getY() const
{
	return mY;
}

inline IVector4 & IVector4::setZ(int _z)
{
	mZ = _z;
	return *this;
}

inline const int IVector4::getZ() const
{
	return mZ;
}

inline IVector4 & IVector4::setW(int _w)
{
	mW = _w;
	return *this;
}

inline const int IVector4::getW() const
{
	return mW;
}

inline IVector4 & IVector4::setElem(int idx, int value)
{
	((int*)(this))[idx] = value;
	return *this;
}

inline const int IVector4::getElem(int idx) const
{
	return ((int*)this)[idx];
}

inline int& IVector4::operator[](int idx)
{
	return ((int*)this)[idx];
}

inline const int IVector4::operator[](int idx) const
{
	return ((int*)this)[idx];
}

inline const IVector4 IVector4::operator + (const IVector4 & vec) const
{
	return IVector4(mX + vec.mX, mY + vec.mY, mZ + vec.mZ, mW + vec.mW);
}

inline const IVector4 IVector4::operator - (const IVector4 & vec) const
{
	return IVector4(mX - vec.mX, mY - vec.mY, mZ - vec.mZ, mW - vec.mW);
}

inline const IVector4 IVector4::operator * (int scalar) const
{
	return IVector4(mX * scalar, mY * scalar, mZ * scalar, mW * scalar);
}

inline IVector4 & IVector4::operator += (const IVector4 & vec)
{
	*this = *this + vec;
	return *this;
}

inline IVector4 & IVector4::operator -= (const IVector4 & vec)
{
	*this = *this - vec;
	return *this;
}

inline IVector4 & IVector4::operator *= (int scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const IVector4 IVector4::operator / (int scalar) const
{
	return IVector4(mX / scalar, mY / scalar, mZ / scalar, mW / scalar);
}

inline IVector4 & IVector4::operator /= (int scalar)
{
	*this = *this / scalar;
	return *this;
}

inline const IVector4 IVector4::operator - () const
{
	return IVector4(-mX, -mY, -mZ, -mW);
}

inline const IVector4 operator * (int scalar, const IVector4 & vec)
{
	return IVector4(vec.getX() * scalar, vec.getY() * scalar, vec.getZ() * scalar, vec.getW() * scalar);
}

inline const IVector4 mulPerElem(const IVector4 & vec0, const IVector4 & vec1)
{
	return IVector4(vec0.getX() * vec1.getX(), vec0.getY() * vec1.getY(), vec0.getZ() * vec1.getZ(), vec0.getW() * vec1.getW());
}

inline const IVector4 divPerElem(const IVector4 & vec0, const IVector4 & vec1)
{
	return IVector4(vec0.getX() / vec1.getX(), vec0.getY() / vec1.getY(), vec0.getZ() / vec1.getZ(), vec0.getW() / vec1.getW());
}

inline const IVector4 absPerElem(const IVector4 & vec)
{
	return IVector4(vec.getX() < 0 ? -vec.getX() : vec.getX(),
					vec.getY() < 0 ? -vec.getY() : vec.getY(),
					vec.getZ() < 0 ? -vec.getZ() : vec.getZ(),
					vec.getW() < 0 ? -vec.getW() : vec.getW());
}

inline const IVector4 copySignPerElem(const IVector4 & vec0, const IVector4 & vec1)
{
	IVector4 vAbs = absPerElem(vec0);
	return IVector4(vec1.getX() < 0.0f ? -vAbs.getX() : vAbs.getX(),
					vec1.getY() < 0.0f ? -vAbs.getY() : vAbs.getY(),
					vec1.getZ() < 0.0f ? -vAbs.getZ() : vAbs.getZ(),
					vec1.getW() < 0.0f ? -vAbs.getW() : vAbs.getW());
}

inline const IVector4 maxPerElem(const IVector4 & vec0, const IVector4 & vec1)
{
	return IVector4((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() > vec1.getZ()) ? vec0.getZ() : vec1.getZ(),
		(vec0.getW() > vec1.getW()) ? vec0.getW() : vec1.getW());
}

inline const int maxElem(const IVector4 & vec)
{
	int result;
	result = (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() > result) ? vec.getZ() : result;
	result = (vec.getW() > result) ? vec.getW() : result;
	return result;
}

inline const IVector4 minPerElem(const IVector4 & vec0, const IVector4 & vec1)
{
	return IVector4((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() < vec1.getZ()) ? vec0.getZ() : vec1.getZ(),
		(vec0.getW() < vec1.getW()) ? vec0.getW() : vec1.getW());
}

inline const int minElem(const IVector4 & vec)
{
	int result;
	result = (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() < result) ? vec.getZ() : result;
	result = (vec.getW() < result) ? vec.getW() : result;
	return result;
}

inline const int sum(const IVector4 & vec)
{
	return vec.getX() + vec.getY() + vec.getZ() + vec.getW();
}

#ifdef VECTORMATH_DEBUG

inline void print(const IVector4 & vec)
{
	std::printf("( %i %i %i %i )\n", vec.getX(), vec.getY(), vec.getZ(), vec.getW());
}

inline void print(const IVector4 & vec, const char * name)
{
	std::printf("%s: ( %i %i %i %i )\n", name, vec.getX(), vec.getY(), vec.getZ(), vec.getW());
}

#endif // VECTORMATH_DEBUG

// ========================================================
// UVector4
// ========================================================

inline UVector4::UVector4(uint _x, uint _y, uint _z, uint _w)
{
	mX = _x;
	mY = _y;
	mZ = _z;
	mW = _w;
}

inline UVector4::UVector4(uint scalar)
{
	mX = mY = mZ = mW = scalar;
}

inline const UVector4 UVector4::xAxis()
{
	return UVector4(1, 0, 0, 0);
}

inline const UVector4 UVector4::yAxis()
{
	return UVector4(0, 1, 0, 0);
}

inline const UVector4 UVector4::zAxis()
{
	return UVector4(0, 0, 1, 0);
}

inline const UVector4 UVector4::wAxis()
{
	return UVector4(0, 0, 0, 1);
}

inline UVector4 & UVector4::operator = (const UVector4 & vec)
{
	mX = vec.mX;
	mY = vec.mY;
	mZ = vec.mZ;
	mW = vec.mW;
	return *this;
}

inline UVector4 & UVector4::setX(uint _x)
{
	mX = _x;
	return *this;
}

inline const uint UVector4::getX() const
{
	return mX;
}

inline UVector4 & UVector4::setY(uint _y)
{
	mY = _y;
	return *this;
}

inline const uint UVector4::getY() const
{
	return mY;
}

inline UVector4 & UVector4::setZ(uint _z)
{
	mZ = _z;
	return *this;
}

inline const uint UVector4::getZ() const
{
	return mZ;
}

inline UVector4 & UVector4::setW(uint _w)
{
	mW = _w;
	return *this;
}

inline const uint UVector4::getW() const
{
	return mW;
}

inline UVector4 & UVector4::setElem(uint idx, uint value)
{
	((uint*)(this))[idx] = value;
	return *this;
}

inline const uint UVector4::getElem(uint idx) const
{
	return ((uint*)this)[idx];
}

inline uint& UVector4::operator[](uint idx)
{
	return ((uint*)this)[idx];
}

inline const uint UVector4::operator[](uint idx) const
{
	return ((uint*)this)[idx];
}

inline const UVector4 UVector4::operator + (const UVector4 & vec) const
{
	return UVector4(mX + vec.mX, mY + vec.mY, mZ + vec.mZ, mW + vec.mW);
}

inline const UVector4 UVector4::operator - (const UVector4 & vec) const
{
	return UVector4(mX - vec.mX, mY - vec.mY, mZ - vec.mZ, mW - vec.mW);
}

inline const UVector4 UVector4::operator * (uint scalar) const
{
	return UVector4(mX * scalar, mY * scalar, mZ * scalar, mW * scalar);
}

inline UVector4 & UVector4::operator += (const UVector4 & vec)
{
	*this = *this + vec;
	return *this;
}

inline UVector4 & UVector4::operator -= (const UVector4 & vec)
{
	*this = *this - vec;
	return *this;
}

inline UVector4 & UVector4::operator *= (uint scalar)
{
	*this = *this * scalar;
	return *this;
}

inline const UVector4 UVector4::operator / (uint scalar) const
{
	return UVector4(mX / scalar, mY / scalar, mZ / scalar, mW / scalar);
}

inline UVector4 & UVector4::operator /= (uint scalar)
{
	*this = *this / scalar;
	return *this;
}

inline const UVector4 operator * (uint scalar, const UVector4 & vec)
{
	return UVector4(vec.getX() * scalar, vec.getY() * scalar, vec.getZ() * scalar, vec.getW() * scalar);
}

inline const UVector4 mulPerElem(const UVector4 & vec0, const UVector4 & vec1)
{
	return UVector4(vec0.getX() * vec1.getX(), vec0.getY() * vec1.getY(), vec0.getZ() * vec1.getZ(), vec0.getW() * vec1.getW());
}

inline const UVector4 divPerElem(const UVector4 & vec0, const UVector4 & vec1)
{
	return UVector4(vec0.getX() / vec1.getX(), vec0.getY() / vec1.getY(), vec0.getZ() / vec1.getZ(), vec0.getW() / vec1.getW());
}

inline const UVector4 maxPerElem(const UVector4 & vec0, const UVector4 & vec1)
{
	return UVector4((vec0.getX() > vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() > vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() > vec1.getZ()) ? vec0.getZ() : vec1.getZ(),
		(vec0.getW() > vec1.getW()) ? vec0.getW() : vec1.getW());
}

inline const uint maxElem(const UVector4 & vec)
{
	uint result;
	result = (vec.getX() > vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() > result) ? vec.getZ() : result;
	result = (vec.getW() > result) ? vec.getW() : result;
	return result;
}

inline const UVector4 minPerElem(const UVector4 & vec0, const UVector4 & vec1)
{
	return UVector4((vec0.getX() < vec1.getX()) ? vec0.getX() : vec1.getX(),
		(vec0.getY() < vec1.getY()) ? vec0.getY() : vec1.getY(),
		(vec0.getZ() < vec1.getZ()) ? vec0.getZ() : vec1.getZ(),
		(vec0.getW() < vec1.getW()) ? vec0.getW() : vec1.getW());
}

inline const uint minElem(const UVector4 & vec)
{
	uint result;
	result = (vec.getX() < vec.getY()) ? vec.getX() : vec.getY();
	result = (vec.getZ() < result) ? vec.getZ() : result;
	result = (vec.getW() < result) ? vec.getW() : result;
	return result;
}

inline const uint sum(const UVector4 & vec)
{
	return vec.getX() + vec.getY() + vec.getZ() + vec.getW();
}

#ifdef VECTORMATH_DEBUG

inline void print(const UVector4 & vec)
{
	std::printf("( %u %u %u %u )\n", vec.getX(), vec.getY(), vec.getZ(), vec.getW());
}

inline void print(const UVector4 & vec, const char * name)
{
	std::printf("%s: ( %u %u %u %u )\n", name, vec.getX(), vec.getY(), vec.getZ(), vec.getW());
}

#endif // VECTORMATH_DEBUG
//========================================= #TheForgeMathExtensionsEnd ================================================


} // namespace Scalar
} // namespace Vectormath

#endif // VECTORMATH_SCALAR_VECTOR_HPP
