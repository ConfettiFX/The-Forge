#ifndef CAMERA_H
#define CAMERA_H

#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "Helper.h"

class Camera
{
public:
	Camera() : 
		mProjMat(mat4::identity()),
		mInvProjMat(mat4::identity())
	{
		SetViewMatrix(mat4::identity());
		OnUpdate();
	}
	void SetViewMatrix(const mat4& viewMat)
	{
		mViewMat = viewMat;
		mInvViewMat = inverse(mViewMat);
		OnUpdate();
	}
	void SetProjection(const mat4& projMat)
	{
		mProjMat = projMat;
		mInvProjMat = inverse(projMat);
		OnUpdate();
	}

	const mat4& GetViewMatrix() const { return mViewMat; }
	const mat4& GetProjection() const { return mProjMat; }
	const mat4& GetViewProjection() const { return mViewProjMat; }
	const mat4& GetViewProjectionInverse() const{ return mInvViewProjMat; }
	const mat4& GetViewMatrixInverse() const { return mInvViewMat; }

	const vec3 GetPosition() const { return Helper::Vec4_To_Vec3(mInvViewMat.getCol3()); }
private:

	void OnUpdate()
	{
		mViewProjMat = mProjMat * mViewMat;
		mInvViewProjMat = mInvViewMat * mInvProjMat;
	}
private:
	mat4 mViewMat;
	mat4 mInvViewMat;
	mat4 mProjMat;
	mat4 mInvProjMat;
	mat4 mViewProjMat;
	mat4 mInvViewProjMat;
};


#endif