#pragma once
#include "../../../../Common_3/Renderer/IRenderer.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/queue.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

#include "SDFVolumeTextureAtlasLayout.h"


class SDFVolumeTextureNode;


class SDFVolumeTextureAtlas
{
public:
	SDFVolumeTextureAtlas(const ivec3& atlasSize);

	void AddVolumeTextureNode(SDFVolumeTextureNode* volumeTextureNode);

	SDFVolumeTextureNode* ProcessQueuedNode();

	SDFVolumeTextureAtlasLayout mSDFVolumeAtlasLayout;

	eastl::queue<SDFVolumeTextureNode*> mPendingNodeQueue;
	eastl::vector<SDFVolumeTextureNode*> mCurrentNodeList;

};