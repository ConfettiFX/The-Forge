#pragma once

#include "../../../../Common_3/OS/Math/MathTypes.h"

namespace AtlasQuads
{
	struct SQuad
	{
		vec4 m_pos;

		static const SQuad Get(
			int dstRectW, int dstRectH,
			int dstX, int dstY, int dstW, int dstH)
		{
			SQuad q;

			q.m_pos.setX(float(dstRectW) / float(dstW));
			q.m_pos.setY(float(dstRectH) / float(dstH));
			q.m_pos.setZ(q.m_pos.getX() + 2.0f*float(dstX) / float(dstW) - 1.0f);
			q.m_pos.setW(1.0f - q.m_pos.getY() - 2.0f*float(dstY) / float(dstH));

			return q;
		}
	};

	struct SFillQuad : public SQuad
	{
		vec4 m_misc;

		static const SFillQuad Get(
			const vec4& miscParams,
			int dstRectW, int dstRectH,
			int dstX, int dstY, int dstW, int dstH)
		{
			SFillQuad q;

			static_cast<SQuad&>(q) = SQuad::Get(dstRectW, dstRectH, dstX, dstY, dstW, dstH);

			q.m_misc = miscParams;

			return q;
		}
	};

	struct SCopyQuad : public SFillQuad
	{
		vec4 m_texCoord;

		static const SCopyQuad Get(
			const vec4& miscParams,
			int dstRectW, int dstRectH,
			int dstX, int dstY, int dstW, int dstH,
			int srcRectW, int srcRectH,
			int srcX, int srcY, int srcW, int srcH)
		{
			SCopyQuad q;

			static_cast<SFillQuad&>(q) = SFillQuad::Get(miscParams, dstRectW, dstRectH, dstX, dstY, dstW, dstH);

			// Align with pixel center @ (0.5, 0.5).
			q.m_pos.setZ( q.m_pos.getZ() + 1.0f / float(dstW));
			q.m_pos.setW( q.m_pos.getW() - 1.0f / float(dstH));

			q.m_texCoord.setX ( float(srcRectW) / float(srcW) );
			q.m_texCoord.setY(  float(srcRectH) / float(srcH) );
			q.m_texCoord.setZ ( (float(srcX) + 0.5f) / float(srcW) );
			q.m_texCoord.setW( (float(srcY) + 0.5f) / float(srcH) );

			return q;
		}
	};
	

} 
