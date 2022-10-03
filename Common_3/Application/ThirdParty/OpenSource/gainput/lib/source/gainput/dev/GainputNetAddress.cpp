#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_DEV
#include "GainputNetAddress.h"

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

namespace gainput {

NetAddress::NetAddress(const char* ip, unsigned port)
{
#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	struct in_addr inp;
	if (!inet_aton(ip, &inp))
	{
		assert(false);
		return;
	}
	addr.sin_addr.s_addr = inp.s_addr;
#elif defined(GAINPUT_PLATFORM_WIN)
	addr.sin_addr.s_addr = inet_addr(ip);
#endif

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
}

NetAddress::NetAddress(const struct sockaddr_in& rhs)
{
	addr.sin_family = rhs.sin_family;
	addr.sin_addr.s_addr = rhs.sin_addr.s_addr;
	addr.sin_port = rhs.sin_port;
}

}
#endif
#endif

