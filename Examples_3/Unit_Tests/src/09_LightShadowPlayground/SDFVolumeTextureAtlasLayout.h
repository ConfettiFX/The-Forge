#pragma once
#include "../../../../Common_3/OS/Math/MathTypes.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"



struct SDFTextureLayoutNode
{
	SDFTextureLayoutNode(const ivec3& nodeCoord, const ivec3& nodeSize)
		:mNodeCoord(nodeCoord),
		mNodeSize(nodeSize)
	{
	}
	//node coord not in texel space but in raw volume dimension space
	ivec3 mNodeCoord;
	ivec3 mNodeSize;
	bool mUsed;
};

class SDFVolumeTextureAtlasLayout
{
public:
	SDFVolumeTextureAtlasLayout(const ivec3& atlasLayoutSize);




	bool Add_New_Node(const ivec3& volumeDimension, ivec3& outCoord);

	bool Add_Normal_Node(const ivec3& volumeDimension, ivec3& outCoord);
	bool Add_Double_Node(const ivec3& volumeDimension, ivec3& outCoord);



	ivec3 mAtlasLayoutSize;
	eastl::vector<SDFTextureLayoutNode> mNodes;

	ivec3 mAllocationCoord;
	ivec3 mDoubleAllocationCoord;
	
};