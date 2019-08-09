#include "ASMQuadTree.h"
#include "ASMTileCache.h"
#include "Helper.h"


ASMQuadTree::ASMQuadTree()
{
	mRoots.reserve(32);
}

ASMQuadTree::~ASMQuadTree()
{
	Reset();
}


void ASMQuadTree::Reset()
{
	while (!mRoots.empty())
	{
		conf_delete(mRoots.back());
	}
}

QuadTreeNode* ASMQuadTree::FindRoot(const AABBox& bbox)
{
	for (int i = 0; i < mRoots.size(); ++i)
	{
		if (!(mRoots[i]->mBBox != bbox))
		{
			return mRoots[i];
		}
	}
	return NULL;
}

QuadTreeNode::QuadTreeNode(ASMQuadTree* pQuadTree, QuadTreeNode* pParent)
	:m_pQuadTree(pQuadTree),
	m_pParent(pParent),
	mLastFrameVerified(0),
	mNumChildren(0),
	m_pTile(NULL),
	m_pLayerTile(NULL)
{
	memset(mChildren, 0, sizeof(mChildren));

	if (m_pParent != NULL)
	{
		mRefinement = m_pParent->mRefinement + 1;
		mRootNodesIndex = -1;
	}
	else
	{
		mRefinement = 0;
		mRootNodesIndex = static_cast<int>(m_pQuadTree->mRoots.size());
		m_pQuadTree->mRoots.push_back(this);
	}
}


QuadTreeNode::~QuadTreeNode()
{
	if (m_pTile)
	{
		m_pTile->Free();
	}

	if (m_pLayerTile)
	{
		m_pLayerTile->Free();
	}

	for (int i = 0; i < 4; ++i)
	{
		if (mChildren[i])
		{
			conf_delete(mChildren[i]);
		}
	}

	if (m_pParent)
	{
		for (int i = 0; i < 4; ++i)
		{
			if (m_pParent->mChildren[i] == this)
			{
				m_pParent->mChildren[i] = NULL;
				--m_pParent->mNumChildren;
				break;
			}
		}
	}
	else
	{
		QuadTreeNode* pLast = m_pQuadTree->mRoots.back();
		pLast->mRootNodesIndex = mRootNodesIndex;
		m_pQuadTree->mRoots[mRootNodesIndex] = pLast;
		m_pQuadTree->mRoots.pop_back();
	}

}


const AABBox QuadTreeNode::GetChildBBox(int childIndex)
{
	static const vec3 quadrantOffsets[] =
	{
		vec3(0.0f, 0.0f, 0.0f),
		vec3(1.0f, 0.0f, 0.0f),
		vec3(1.0f, 1.0f, 0.0f),
		vec3(0.0f, 1.0f, 0.0f),
	};	
	vec3 halfSize = 0.5f * (mBBox.m_max - mBBox.m_min);
	vec3 bboxMin = mBBox.m_min +  Helper::Piecewise_Prod( quadrantOffsets[childIndex], halfSize );
	vec3 bboxMax = bboxMin + halfSize;
	return AABBox(bboxMin, bboxMax);
}

QuadTreeNode* QuadTreeNode::AddChild(int childIndex)
{
	if (!mChildren[childIndex])
	{
		mChildren[childIndex] = conf_new(QuadTreeNode, m_pQuadTree, this);
		++mNumChildren;
	}
	return mChildren[childIndex];
}