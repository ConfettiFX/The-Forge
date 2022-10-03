#ifndef GAINPUTLISTENER_H_
#define GAINPUTLISTENER_H_

#include "gainput/GainputAllocator.h"
#include "GainputNetAddress.h"

namespace gainput {

class NetConnection;

class NetListener
{
public:
	NetListener(const NetAddress& address, Allocator& allocator = GetDefaultAllocator());
	~NetListener();

	bool Start(bool shouldBlock);
	void Stop();

	NetConnection* Accept();

private:
	NetAddress address;
	Allocator& allocator;
	bool blocking;

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	int fd;
#elif defined(GAINPUT_PLATFORM_WIN)
	SOCKET listenSocket;
#endif

};

}

#endif

