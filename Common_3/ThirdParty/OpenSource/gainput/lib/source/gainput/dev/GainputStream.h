#ifndef GAINPUTSTREAM_H_
#define GAINPUTSTREAM_H_

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_GGP) || defined(GAINPUT_PLATFORM_ORBIS)
#include <arpa/inet.h>
#include <stdint.h>
#elif defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_XBOX_ONE)
#include <Winsock2.h>
typedef unsigned __int16 uint16_t;
typedef __int16 int16_t;
typedef unsigned __int32 uint32_t;
typedef __int32 int32_t;
#elif defined(GAINPUT_PLATFORM_NX64)
#include <stdint.h>
#include <arpa/inet.h>
#endif

namespace gainput {

class Stream
{
public:
	virtual ~Stream() { }

	virtual size_t Read(void* dest, size_t length) = 0;
	virtual size_t Write(const void* src, size_t length) = 0;

	virtual size_t GetSize() const = 0;
	virtual size_t GetLeft() const = 0;

	virtual bool SeekBegin(int offset) = 0;
	virtual bool SeekCurrent(int offset) = 0;
	virtual bool SeekEnd(int offset) = 0;

	virtual void Reset() = 0;

	virtual bool IsEof() const = 0;

	template<class T> size_t Read(T& dest) { return Read(&dest, sizeof(T)); }
	template<class T> size_t Write(const T& src) { return Write(&src, sizeof(T)); }

};

template<>
inline
size_t
Stream::Read(uint16_t& dest)
{
	const size_t result = Read(&dest, sizeof(dest));
	dest = ntohs(dest);
	return result;
}

template<>
inline
size_t
Stream::Read(int16_t& dest)
{
	const size_t result = Read(&dest, sizeof(dest));
	dest = ntohs((uint16_t)dest);
	return result;
}

template<>
inline
size_t
Stream::Read(uint32_t& dest)
{
	const size_t result = Read(&dest, sizeof(dest));
	dest = ntohl(dest);
	return result;
}

template<>
inline
size_t
Stream::Read(int32_t& dest)
{
	const size_t result = Read(&dest, sizeof(dest));
	dest = ntohl((uint32_t)dest);
	return result;
}

template<>
inline
size_t
Stream::Read(float& dest)
{
	const size_t result = Read(&dest, sizeof(dest));
	const uint32_t tmpInt = ntohl(*(uint32_t*)&dest);
	const float* tmpFloatP = (float*)&tmpInt;
	dest = *tmpFloatP;
	return result;
}


template<>
inline
size_t
Stream::Write(const uint16_t& src)
{
	const uint16_t tmp = htons(src);
	return Write(&tmp, sizeof(tmp));
}

template<>
inline
size_t
Stream::Write(const int16_t& src)
{
	const int16_t tmp = htons((uint16_t)src);
	return Write(&tmp, sizeof(tmp));
}

template<>
inline
size_t
Stream::Write(const uint32_t& src)
{
	const uint32_t tmp = htonl(src);
	return Write(&tmp, sizeof(tmp));
}

template<>
inline
size_t
Stream::Write(const int32_t& src)
{
	const int32_t tmp = htonl((uint32_t)src);
	return Write(&tmp, sizeof(tmp));
}

template<>
inline
size_t
Stream::Write(const float& src)
{
	const uint32_t tmpInt = htonl(*(uint32_t*)&src);
	const float* tmpFloatP = (float*)&tmpInt;
	const float tmp = *tmpFloatP;
	return Write(&tmp, sizeof(tmp));
}


}

#endif

