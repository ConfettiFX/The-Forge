#include "SDFVolumeTextureAtlasLayout.h"
#include "SDF_Constant.h"
#include "../../../../Common_3/OS/Interfaces/ILog.h"


SDFVolumeTextureAtlasLayout::SDFVolumeTextureAtlasLayout(const ivec3& atlasLayoutSize)
	:mAtlasLayoutSize(atlasLayoutSize)
{
	mAllocationCoord = ivec3(-SDF_MAX_VOXEL_ONE_DIMENSION_X, 0, SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z * 3);
	mDoubleAllocationCoord = ivec3(-SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X, 0, 0);
}

bool SDFVolumeTextureAtlasLayout::Add_New_Node(const ivec3& volumeDimension, ivec3& outCoord)
{
	if (volumeDimension.getX() <= SDF_MAX_VOXEL_ONE_DIMENSION_X && 
		volumeDimension.getY() <= SDF_MAX_VOXEL_ONE_DIMENSION_Y && 
		volumeDimension.getZ() <= SDF_MAX_VOXEL_ONE_DIMENSION_Z)
	{
		return Add_Normal_Node(volumeDimension, outCoord);
	}
	return Add_Double_Node(volumeDimension, outCoord);
}

bool SDFVolumeTextureAtlasLayout::Add_Normal_Node(const ivec3& volumeDimension, ivec3& outCoord)
{
	if ((mAllocationCoord.getX() + (SDF_MAX_VOXEL_ONE_DIMENSION_X * 2)) <= mAtlasLayoutSize.getX())
	{
		mAllocationCoord.setX(mAllocationCoord.getX() + SDF_MAX_VOXEL_ONE_DIMENSION_X);
		mNodes.push_back(SDFTextureLayoutNode(
			mAllocationCoord, volumeDimension));

	}
	else if ((mAllocationCoord.getY() + (SDF_MAX_VOXEL_ONE_DIMENSION_Y * 2)) <= mAtlasLayoutSize.getY())
	{
		mAllocationCoord.setX(0);
		mAllocationCoord.setY(mAllocationCoord.getY() + SDF_MAX_VOXEL_ONE_DIMENSION_Y);

		mNodes.push_back(SDFTextureLayoutNode(
			mAllocationCoord, volumeDimension));
	}
	else if ((mAllocationCoord.getZ() + (SDF_MAX_VOXEL_ONE_DIMENSION_Z * 2)) <= mAtlasLayoutSize.getZ())
	{
		mAllocationCoord.setX(0);
		mAllocationCoord.setY(0);
		mAllocationCoord.setZ(mAllocationCoord.getZ() + SDF_MAX_VOXEL_ONE_DIMENSION_Z);

		mNodes.push_back(SDFTextureLayoutNode(
			mAllocationCoord, volumeDimension));
	}

	else
	{
		return false;
	}
	outCoord = mNodes.back().mNodeCoord;

	//LOGF(LogLevel::eINFO, "Atlas layout out coord %d %d %d",
		//outCoord.getX(), outCoord.getY(), outCoord.getZ());
	return true;
}


bool SDFVolumeTextureAtlasLayout::Add_Double_Node(const ivec3& volumeDimension, ivec3& outCoord)
{
	if ((mDoubleAllocationCoord.getX() + (SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X * 2)) <= mAtlasLayoutSize.getX())
	{
		mDoubleAllocationCoord.setX(mDoubleAllocationCoord.getX() + SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_X);
		mNodes.push_back(SDFTextureLayoutNode(
			mDoubleAllocationCoord, volumeDimension));

	}
	else if ((mDoubleAllocationCoord.getY() + (SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y * 2)) <= mAtlasLayoutSize.getY())
	{
		mDoubleAllocationCoord.setX(0);
		mDoubleAllocationCoord.setY(mDoubleAllocationCoord.getY() + SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Y);

		mNodes.push_back(SDFTextureLayoutNode(
			mDoubleAllocationCoord, volumeDimension));
	}
	else if ((mDoubleAllocationCoord.getZ() + (SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z * 2)) <= mAtlasLayoutSize.getZ())
	{
		mDoubleAllocationCoord.setX(0);
		mDoubleAllocationCoord.setY(0);
		mDoubleAllocationCoord.setZ(mDoubleAllocationCoord.getZ() + SDF_DOUBLE_MAX_VOXEL_ONE_DIMENSION_Z);

		mNodes.push_back(SDFTextureLayoutNode(
			mDoubleAllocationCoord, volumeDimension));
	}

	else
	{
		return false;
	}
	outCoord = mNodes.back().mNodeCoord;

	//LOGF(LogLevel::eINFO, "Atlas layout out double coord %d %d %d",
		//outCoord.getX(), outCoord.getY(), outCoord.getZ());
	return true;
}