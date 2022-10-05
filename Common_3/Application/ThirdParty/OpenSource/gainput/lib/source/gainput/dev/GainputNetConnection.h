#ifndef GAINPUTCONNECTION_H_
#define GAINPUTCONNECTION_H_

#include "GainputNetAddress.h"

namespace gainput {

class Stream;

class NetConnection
{
public:
	NetConnection(const NetAddress& address, Allocator& allocator = GetDefaultAllocator());
#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	NetConnection(const NetAddress& remoteAddress, int fd, Allocator& allocator = GetDefaultAllocator());
#elif defined(GAINPUT_PLATFORM_WIN)
	NetConnection(const NetAddress& remoteAddress, SOCKET fd, Allocator& allocator = GetDefaultAllocator());
#endif
	~NetConnection();

	bool Connect(bool shouldBlock);
	void Close();
	bool IsConnected() const;
	bool IsReady(bool read, bool write);

	size_t Send(const void* buffer, size_t length);
	size_t Send(Stream& stream);

	size_t Receive(void* buffer, size_t length);
	size_t Receive(Stream& stream, size_t maxLength);

private:
	Allocator& allocator;
	NetAddress address;

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	int fd;
#elif defined(GAINPUT_PLATFORM_WIN)
	SOCKET fd;
#endif

};

}

#endif

