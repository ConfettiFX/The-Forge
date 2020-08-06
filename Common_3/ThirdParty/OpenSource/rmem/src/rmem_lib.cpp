/*
 * Copyright (c) 2019 by Milos Tosic. All Rights Reserved.
 * License: http://www.opensource.org/licenses/BSD-2-Clause
 */

#include "../inc/rmem.h"
#include "rmem_hook.h"
#include "rmem_utils.h"

rmem::MemoryHook*& getMemoryHookPtr()
{
	static rmem::MemoryHook* ptr = NULL;
	return ptr;
}

uint8_t* getMemoryHookBuffer()
{
	static uint8_t buffer[sizeof(rmem::MemoryHook)];
	return buffer;
}

/// RMemTagScope constructor, enters the tag scope
RMemTagScope::RMemTagScope( RMemTag* _tag ) : m_tag(_tag)
{
	rmemEnterTag(m_tag);
}

/// RMemTagScope destructor, leaves the tag scope
RMemTagScope::~RMemTagScope()
{
	rmemLeaveTag(m_tag);
}

/// RMemTagRegistration constructor
RMemTagRegistration::RMemTagRegistration(const char* _name, const char* _parentName)
{
	rmemRegisterTag(_name, _parentName);
} 

extern "C" {

#if RMEM_ENABLE_DELAYED_CAPTURE
	bool rmemIsCaptureEnabled(bool _enable = false)
	{
		static bool s_enabled = false;
		s_enabled = s_enabled || _enable;
		return s_enabled;
	}
#endif

	void rmemInit(void* _data)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook != NULL)
			return;

		uint8_t* buff = getMemoryHookBuffer();

		// CONFFX BEGIN: Replace rmemPlacementNew with tf_placement_new.
		tf_placement_new<rmem::MemoryHook>((void*)buff, _data);
		// CONFFX END

		hook = (rmem::MemoryHook*)buff;
	}

	void rmemStartCapture()
	{
#if RMEM_ENABLE_DELAYED_CAPTURE
		rmemIsCaptureEnabled(true);
#endif
	}

	void rmemUnload()
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook == NULL)
			return;

		hook->flush();
	}

	void rmemShutDown()
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook == NULL)
			return;

		hook->~MemoryHook();
		hook = NULL;
	}

	//--------------------------------------------------------------------------

	void rmemRegisterTag(const char* _name, const char* _parentName)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		hook->registerTag(_name, _parentName);
	}

	void rmemEnterTag(RMemTag* _tag)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		hook->enterTag(*_tag);
	}

	void rmemLeaveTag(RMemTag* _tag)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		hook->leaveTag(*_tag);
	}

	void rmemRegisterMarker(RMemMarker* _marker)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		hook->registerMarker(*_marker);
	}

	void rmemSetMarker(RMemMarker* _marker)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		hook->marker(*_marker);
	}

	void rmemRegisterAllocator( const char* _name, uint64_t _handle)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->registerAllocator(_name, _handle);
	}

	void rmemAlloc(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->alloc(_handle, _ptr, _size, _overhead);
	}

	void rmemRealloc(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, void* _prevPtr)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->realloc(_handle, _ptr, _size, _overhead, _prevPtr);
	}

	void rmemAllocAligned(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, uint32_t _alignment)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->allocAligned(_handle, _ptr, _size, _overhead, _alignment);
	}

	void rmemReallocAligned(uint64_t _handle, void* _ptr, uint32_t _size, uint32_t _overhead, void* _prevPtr, uint32_t _alignment)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->reallocAligned(_handle, _ptr, _size, _overhead, _prevPtr, _alignment);
	}

	void rmemFree(uint64_t _handle, void* _ptr)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->free(_handle, _ptr);
	}

	RMemMarker rmemCreateMarker(const char* _name, uint32_t _color)
	{
		RMemMarker marker;
		marker.m_name = _name;
		marker.m_color = _color;
		marker.m_nameHash = rmem::hashStr(_name);
		rmemRegisterMarker( &marker );
		return marker;
	}

	RMemMarker rmemCreateMarkerRGB(const char* _name, uint8_t _r, uint8_t _g, uint8_t _b)
	{
		RMemMarker marker;
		marker.m_name = _name;
		marker.m_color = 0xff000000 | ((uint32_t)_r << 16) | ((uint32_t)_g << 8) | (uint32_t)_b;;
		marker.m_nameHash = rmem::hashStr(_name);
		rmemRegisterMarker( &marker );
		return marker;
	}

	RMemTag rmemCreateTag(const char* _name)
	{
		RMemTag tag;
		tag.m_name = _name;
		tag.m_HashedName = rmem::hashStr(_name);
		return tag;
	}

	void rmemAddModuleC(const char* _name, uint64_t _base, uint32_t _size)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->registerModule(_name, _base, _size);
	}

	void rmemAddModuleW(const wchar_t* _name, uint64_t _base, uint32_t _size)
	{
		rmem::MemoryHook*& hook = getMemoryHookPtr();
		if (hook)
			hook->registerModule(_name, _base, _size);
	}

} // extern "C"
