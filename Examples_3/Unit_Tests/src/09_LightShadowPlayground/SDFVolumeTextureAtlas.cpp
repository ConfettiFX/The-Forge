#include "SDFVolumeTextureAtlas.h"
#include "SDFVolumeData.h"



SDFVolumeTextureAtlas::SDFVolumeTextureAtlas(const ivec3& atlasSize)
	:mSDFVolumeAtlasLayout(atlasSize)
{
}


void SDFVolumeTextureAtlas::AddVolumeTextureNode(SDFVolumeTextureNode* volumeTextureNode)
{
	ivec3 atlasCoord = volumeTextureNode->mAtlasAllocationCoord;

	if (volumeTextureNode->mHasBeenAdded)
	{
		return;
	}

	mSDFVolumeAtlasLayout.Add_New_Node(volumeTextureNode->mSDFVolumeData->mSDFVolumeSize, 
		volumeTextureNode->mAtlasAllocationCoord);
	mPendingNodeQueue.push(volumeTextureNode);
	volumeTextureNode->mHasBeenAdded = true;
}


SDFVolumeTextureNode* SDFVolumeTextureAtlas::ProcessQueuedNode()
{
	if (mPendingNodeQueue.empty())
	{
		return NULL;
	}

	SDFVolumeTextureNode* node = mPendingNodeQueue.front();
	mCurrentNodeList.push_back(node);
	mPendingNodeQueue.pop();
	return node;
}