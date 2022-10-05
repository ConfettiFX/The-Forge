
#include "../../include/gainput/gainput.h"

#include "../../include/gainput/GainputLog.h"

namespace gainput
{

DefaultAllocator&
GetDefaultAllocator()
{
	static DefaultAllocator da;
	return da;
}


TrackingAllocator::TrackingAllocator(Allocator& backingAllocator, Allocator& internalAllocator)
	: backingAllocator_(backingAllocator),
	internalAllocator_(internalAllocator),
	allocations_(internalAllocator.New<HashMap<void*, size_t> >(internalAllocator)),
	allocateCount_(0),
	deallocateCount_(0),
	allocatedMemory_(0)
{
}

TrackingAllocator::~TrackingAllocator()
{
	internalAllocator_.Delete(allocations_);
}

void* TrackingAllocator::Allocate(size_t size, size_t align)
{
	void* ptr = backingAllocator_.Allocate(size, align);
	(*allocations_)[ptr] = size;
	++allocateCount_;
	allocatedMemory_ += size;
	return ptr;
}

void TrackingAllocator::Deallocate(void* ptr)
{
	HashMap<void*, size_t>::iterator it = allocations_->find(ptr);
	if (it == allocations_->end())
	{
		GAINPUT_LOG("Warning: Trying to deallocate unknown memory block: %p\n", ptr);
	}
	else
	{
		allocatedMemory_ -= it->second;
		allocations_->erase(it);
	}
	++deallocateCount_;
	backingAllocator_.Deallocate(ptr);
}

}

