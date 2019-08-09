#include "Helper.h"
#include "CPUImage.h"



namespace Helper
{
	ivec4 LoadVec4WithUV(CPUImage* image, const vec2& UV)
	{
		if (UV.getX() > 1.f || UV.getY() > 1.f)
		{
			return ivec4(0, 0, 0, 1);
		}
		vec2 finalUV = UV;
				
		if (finalUV.getY() <= Epilson)
		{
			finalUV.setY(0.f);
		}
		if (finalUV.getX() <= Epilson)
		{
			finalUV.setX(0.f);
		}


		uint32_t pixelCoordX = (uint32_t) floor( (float)(image->GetWidth() - 1) * finalUV.getX() );
		uint32_t pixelCoordY = (uint32_t) floor( (float)(image->GetHeight() - 1) * finalUV.getY() );

		uint32_t finalPixelCoordLoc = 4 * (pixelCoordX + image->GetWidth() * pixelCoordY);

		/*if (finalPixelCoordLoc >= image->GetWidth() * image->GetHeight() * 4 || 
			finalPixelCoordLoc < 0 || finalUV.getX() > 1.f || finalUV.getY() > 1.f)
		{
			LOGF(LogLevel::eINFO, "Load vec4 index %d, pixelCoordX %d, pixelCoordY %d, UV %f %f", finalPixelCoordLoc, pixelCoordX, pixelCoordY, finalUV.getX(), finalUV.getY());
		}*/

		unsigned char* beginData = image->GetPixels(0);
		beginData = beginData + finalPixelCoordLoc;
		ivec4 finalColor
		(
			(int)(*(beginData + 0)),
			(int)(*(beginData + 1)),
			(int)(*(beginData + 2)),
			(int)(*(beginData + 3))
		);
	
		
		return finalColor;
	}
}

