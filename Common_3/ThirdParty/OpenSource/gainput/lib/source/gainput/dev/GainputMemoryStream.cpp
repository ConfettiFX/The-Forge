#include "../../../include/gainput/gainput.h"

#if defined(GAINPUT_DEV) || defined(GAINPUT_ENABLE_RECORDER)
#include "GainputMemoryStream.h"

namespace gainput {

MemoryStream::MemoryStream(void* data, size_t length, size_t capacity, bool ownership) :
	data_(data),
	length_(length),
	capacity_(capacity),
	ownership_(ownership),
	position_(0)
{
	// empty
}

MemoryStream::MemoryStream(size_t capacity, Allocator& allocator) :
	allocator_(&allocator),
	length_(0),
	capacity_(capacity),
	ownership_(true),
	position_(0)
{
	data_ = this->allocator_->Allocate(capacity_);
}

MemoryStream::~MemoryStream()
{
	if (ownership_)
	{
		assert(allocator_);
		allocator_->Deallocate(data_);
	}
}

size_t
MemoryStream::Read(void* dest, size_t readLength)
{
	assert(position_ + readLength <= length_);
	memcpy(dest, (void*)( (uint8_t*)data_ + position_), readLength);
	position_ += readLength;
	return readLength;
}

size_t
MemoryStream::Write(const void* src, size_t writeLength)
{
	assert(position_ + writeLength <= capacity_);
	memcpy((void*)( (uint8_t*)data_ + position_), src, writeLength);
	position_ += writeLength;
	length_ += writeLength;
	return writeLength;
}

bool
MemoryStream::SeekBegin(int offset)
{
	if (offset < 0)
	{
		return false;
	}
	position_ = offset;
	return true;
}

bool
MemoryStream::SeekCurrent(int offset)
{
	if (offset + position_ > length_)
	{
		return false;
	}
	position_ += offset;
	return true;
}

bool
MemoryStream::SeekEnd(int offset)
{
	if (offset > 0)
	{
		return false;
	}
	position_ = length_ + offset;
	return true;
}

}
#endif

