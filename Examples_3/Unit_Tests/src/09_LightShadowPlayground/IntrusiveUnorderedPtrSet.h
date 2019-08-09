#pragma once

//ea stl
#include "../../../../Common_3/ThirdParty/OpenSource/EASTL/vector.h"

class CIntrusiveUnorderedSetItemHandle
{
public:
	CIntrusiveUnorderedSetItemHandle() : mIndex(-1) { }
	bool IsInserted() const { return mIndex >= 0; }

protected:
	int mIndex;
};

template< class CContainer, class CItem, CIntrusiveUnorderedSetItemHandle& (CItem::*GetHandle)() >
class CIntrusiveUnorderedPtrSet
{
	struct SHandleSetter : public CIntrusiveUnorderedSetItemHandle
	{
		void Set(int i) { mIndex = i; }
		void Set_Size_t(size_t i) { mIndex = static_cast<int>(i); }
		int Get() const { return mIndex; }
	};

public:
	void Add(CItem* pItem, bool mayBeAlreadyInserted = false)
	{
		SHandleSetter& handle = static_cast<SHandleSetter&>((pItem->*GetHandle)());
		CContainer& container = *static_cast<CContainer*>(this);
		if (handle.IsInserted())
		{
			
		}
		else
		{
			handle.Set_Size_t(container.size());
			container.push_back(pItem);
		}
	}
	void Remove(CItem* pItem, bool mayBeNotInserted = false)
	{
		SHandleSetter& handle = static_cast<SHandleSetter&>((pItem->*GetHandle)());
		CContainer& container = *static_cast<CContainer*>(this);
		if (handle.IsInserted())
		{
			CItem* pLastItem = container.back();
			(pLastItem->*GetHandle)() = handle;
			container[handle.Get()] = pLastItem;
			container.pop_back();
			handle.Set(-1);
		}
		else
		{
			
		}
	}
};

template< class CItem, CIntrusiveUnorderedSetItemHandle& (CItem::*GetHandle)() >
class CVectorBasedIntrusiveUnorderedPtrSet :
	public eastl::vector< CItem* >,
	public CIntrusiveUnorderedPtrSet< CVectorBasedIntrusiveUnorderedPtrSet< CItem, GetHandle >, CItem, GetHandle >
{
};
