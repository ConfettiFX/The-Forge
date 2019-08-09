#ifndef HELPER_H
#define HELPER_H



#include "Constant.h"

//Math
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/Renderer/IRenderer.h"



class CPUImage;

#define Epilson (1.e-4f)

namespace Helper
{

	

	//the second parameter is the reciprocal
	inline vec3 Piecewise_Division(const vec3& left, const vec3& right)
	{
		return vec3(left.getX() / right.getX(),
			left.getY() / right.getY(), left.getZ() / right.getZ());
	}

	inline vec3 Piecewise_Prod(const vec3& left, const vec3& right)
	{
		return vec3(left.getX() * right.getX(), 
			left.getY() * right.getY(), left.getZ() * right.getZ());
	}

	inline vec4 Piecewise_Prod(const vec4& left, const vec4& right)
	{
		return vec4(left.getX() * right.getX(),
			left.getY() * right.getY(), left.getZ() * right.getZ(),
			left.getW() * right.getW());
	}

	inline vec3 Vec4_To_Vec3(const vec4& v)
	{
		return vec3(v.getX(), v.getY(), v.getZ());
	}

	inline vec4 Vec3_To_Vec4(const vec3& v, float w = 0.f)
	{
		return vec4(v.getX(), v.getY(), v.getZ(), w);
	}

	inline vec3 Vec2_To_Vec3(const vec2& v, float z = 0.f)
	{
		return vec3(v.getX(), v.getY(), z);
	}

	inline vec2 Vec3_To_Vec2(const vec3& v)
	{
		return vec2(v.getX(), v.getY());
	}


	inline vec3 Project(const vec3& v, float w, const mat4& projMat)
	{
		vec4 newV = projMat * Helper::Vec3_To_Vec4(v, w);
		return vec3(newV.getX() / newV.getW(), newV.getY() / newV.getW(), newV.getZ() / newV.getW());
	}

	inline vec3 ivec3ToVec3f(const ivec3& i_vec3)
	{
		return vec3((float)i_vec3.getX(), (float)i_vec3.getY(), (float)i_vec3.getZ());
	}

	inline float Vec3SquaredSize(const vec3& v)
	{
		return (v.getX() * v.getX()) + (v.getY() * v.getY()) + (v.getZ() * v.getZ());
	}


	inline float GetMatrixMaximumScale(const mat4& matrix) 
	{
		float col0SquaredLength = Helper::Vec3SquaredSize(Helper::Vec4_To_Vec3(matrix.getCol0()));
		float col1SquaredLength = Helper::Vec3SquaredSize(Helper::Vec4_To_Vec3(matrix.getCol1()));
		float col2SquaredLength = Helper::Vec3SquaredSize(Helper::Vec4_To_Vec3(matrix.getCol2()));

		float finalColSquaredLength = fmax( 
			fmax(col0SquaredLength, col1SquaredLength), col2SquaredLength);
		

		return sqrt(finalColSquaredLength);
	}


	static void setRenderTarget(
		Cmd* cmd, uint32_t count, RenderTarget** pDestinationRenderTargets, RenderTarget* pDepthStencilTarget, LoadActionsDesc* loadActions,
		const vec2& viewPortLoc, const vec2& viewPortSize )
	{
		if (count == 0 && pDestinationRenderTargets == NULL && pDepthStencilTarget == NULL)
			cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, -1, -1);
		else
		{
			cmdBindRenderTargets(cmd, count, pDestinationRenderTargets, pDepthStencilTarget, loadActions, NULL, NULL, -1, -1);
			// sets the rectangles to match with first attachment, I know that it's not very portable.
			RenderTarget* pSizeTarget = pDepthStencilTarget ? pDepthStencilTarget : pDestinationRenderTargets[0];
			cmdSetViewport(cmd, viewPortLoc.getX(), viewPortLoc.getY(), viewPortSize.getX(), viewPortSize.getY(), 0.0f, 1.0f);
			cmdSetScissor(cmd, 0, 0, pSizeTarget->mDesc.mWidth, pSizeTarget->mDesc.mHeight);
		}
	}
	
	inline vec3 Min_Vec3(const vec3& v0, const vec3& v1)
	{
		return vec3( min(v0.getX(), v1.getX()), min(v0.getY(), v1.getY()), min(v0.getZ(), v1.getZ()) );
	}

	inline vec3 Max_Vec3(const vec3& v0, const vec3& v1)
	{
		return vec3( max(v0.getX(), v1.getX()), max(v0.getY(), v1.getY()), max(v0.getZ(), v1.getZ()) );
	}


	

	inline vec3 Vec3XOR(const vec3& v0, const vec3& v1)
	{
		return vec3(v0.getY() * v1.getZ() - v0.getZ() * v1.getY(),
			v0.getZ() * v1.getX() - v0.getX() * v1.getZ(),
			v0.getX() * v1.getY() - v0.getY() * v1.getX());
	}


	inline float GenerateRandomFloat() 
	{ 
		return (float)rand() / (float)RAND_MAX; 
	}

	inline float GetMaxElem(const vec3& v)
	{
		return fmax(v.getX(), fmax(v.getY(), v.getZ()));
	}


	ivec4 LoadVec4WithUV(CPUImage* image, const vec2& UV);


	/*vec3 Min(const vec3& left, const vec3& right)
	{
		min();
	}*/

}



#endif