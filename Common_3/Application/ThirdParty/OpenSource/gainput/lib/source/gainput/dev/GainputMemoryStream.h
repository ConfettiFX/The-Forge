#ifndef GAINPUTMEMORYSTREAM_H_
#define GAINPUTMEMORYSTREAM_H_

#include "GainputStream.h"

namespace gainput {

class MemoryStream : public Stream
{
public:
	MemoryStream(void* data, size_t length, size_t capacity, bool ownership = false);
	MemoryStream(size_t capacity, Allocator& allocator = GetDefaultAllocator());
	~MemoryStream();

	size_t Read(void* dest, size_t readLength);
	size_t Write(const void* src, size_t writeLength);

	size_t GetSize() const { return length_; }
	size_t GetLeft() const { return length_ - position_; }

	bool SeekBegin(int offset);
	bool SeekCurrent(int offset);
	bool SeekEnd(int offset);

	virtual void Reset() { length_ = 0; position_ = 0; }

	bool IsEof() const
	{
		return position_ >= length_;
	}

	void* GetData() { return data_; }
	size_t GetPosition() const { return position_; }

private:
	Allocator* allocator_;
	void* data_;
	size_t length_;
	size_t capacity_;
	bool ownership_;

	size_t position_;

};

}

#endif

