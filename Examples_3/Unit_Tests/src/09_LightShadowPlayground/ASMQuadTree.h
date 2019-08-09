#pragma once

#include "AABBox.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/string.h"
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"



class QuadTreeNode;
class ASMTileCacheEntry;


class ASMQuadTree
{
public:
	ASMQuadTree();
	~ASMQuadTree();

	void Reset();

	QuadTreeNode* FindRoot(const AABBox& BBox);

	const eastl::vector<QuadTreeNode*>& GetRoots() const { return mRoots; }

protected:
	eastl::vector<QuadTreeNode*> mRoots;

	friend class QuadTreeNode;
};

class QuadTreeNode 
{
public:
	AABBox mBBox;
	unsigned int mLastFrameVerified;

	QuadTreeNode* m_pParent;
	QuadTreeNode* mChildren[4];
	ASMTileCacheEntry* m_pTile;
	ASMTileCacheEntry* m_pLayerTile;
	unsigned char mRefinement;
	unsigned char mNumChildren;

	QuadTreeNode(ASMQuadTree* pQuadTree, QuadTreeNode* pParent);
	~QuadTreeNode();

	const AABBox GetChildBBox(int childIndex);
	QuadTreeNode* AddChild(int childIndex);

	ASMTileCacheEntry*& GetTile() { return m_pTile; }
	ASMTileCacheEntry*& GetLayerTile() { return m_pLayerTile; }

	ASMTileCacheEntry* GetTile(bool isLayer) const { return isLayer ? m_pLayerTile : m_pTile; }

protected:
	ASMQuadTree* m_pQuadTree;
	int mRootNodesIndex;
};
