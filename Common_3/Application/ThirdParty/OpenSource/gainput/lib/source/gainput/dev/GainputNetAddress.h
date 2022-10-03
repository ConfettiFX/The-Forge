#ifndef GAINPUTADDRESS_H_
#define GAINPUTADDRESS_H_

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#elif defined(GAINPUT_PLATFORM_WIN)
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

namespace gainput {

class NetAddress
{
public:
	NetAddress(const char* ip, unsigned port);

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	NetAddress(const struct sockaddr_in& rhs);

	const struct sockaddr_in& GetAddr() const { return addr; }
	struct sockaddr_in& GetAddr() { return addr; }
#endif

	NetAddress& operator = (const NetAddress& rhs);

private:
#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	struct sockaddr_in addr;
#endif

};


inline
NetAddress&
NetAddress::operator = (const NetAddress& rhs)
{
#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	addr.sin_family = rhs.addr.sin_family;
	addr.sin_addr.s_addr = rhs.addr.sin_addr.s_addr;
	addr.sin_port = rhs.addr.sin_port;
#endif
	return *this;
}

}

#endif

