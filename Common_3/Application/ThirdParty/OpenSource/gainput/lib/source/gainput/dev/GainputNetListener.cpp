#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_DEV
#include "GainputNetAddress.h"
#include "GainputNetConnection.h"
#include "GainputNetListener.h"

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
#include <fcntl.h>
#include <unistd.h>

namespace gainput {

NetListener::NetListener(const NetAddress& address, Allocator& allocator) :
	address(address),
	allocator(allocator),
	blocking(true),
	fd(-1)
{

}

NetListener::~NetListener()
{
	Stop();
}

bool
NetListener::Start(bool shouldBlock)
{
	assert(fd == -1);

	blocking = shouldBlock;

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{
		return false;
	}

	if (!shouldBlock && fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		return false;
	}

	const int sock_reuse_optval = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &sock_reuse_optval, sizeof(sock_reuse_optval));

	if (bind(fd, (struct sockaddr*)&address.GetAddr(), sizeof(struct sockaddr_in)) == -1)
	{
		return false;
	}

	if (listen(fd, 50) == -1)
	{
		return false;
	}

	return true;
}

void
NetListener::Stop()
{
	if (fd == -1)
	{
		return;
	}

	close(fd);
	fd = -1;
}

NetConnection*
NetListener::Accept()
{
	assert(fd != -1);
	struct sockaddr_in addr;
	socklen_t addr_len = sizeof(struct sockaddr_in);

	int remoteFd = accept(fd, (struct sockaddr*)&addr, &addr_len);
	if (remoteFd == -1)
	{
		return 0;
	}

	if (!blocking)
	{
		fcntl(remoteFd, F_SETFL, O_NONBLOCK);
	}

	NetAddress remoteAddress(addr);
	NetConnection* connection = allocator.New<NetConnection>(remoteAddress, remoteFd, allocator);

	return connection;
}

}

#elif defined(GAINPUT_PLATFORM_WIN)

namespace gainput {

NetListener::NetListener(const NetAddress& address, Allocator& allocator) :
	address(address),
	allocator(allocator),
	blocking(true),
	listenSocket(INVALID_SOCKET)
{

}

NetListener::~NetListener()
{
	Stop();
}

bool
NetListener::Start(bool shouldBlock)
{
	assert(listenSocket == INVALID_SOCKET);
	listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSocket == INVALID_SOCKET)
	{
		return false;
	}

	if (!shouldBlock)
	{
		u_long NonBlock = 1;
		if (ioctlsocket(listenSocket, FIONBIO, &NonBlock) == SOCKET_ERROR)
		{
			return false;
		}
	}

	if (bind(listenSocket, (struct sockaddr*)&address.GetAddr(), sizeof(struct sockaddr_in)) == SOCKET_ERROR)
	{
		closesocket(listenSocket);
		return false;
	}

	if (listen(listenSocket, SOMAXCONN ) == SOCKET_ERROR)
	{
		closesocket(listenSocket);
		return false;
	}

	return true;
}

void
NetListener::Stop()
{
	if (listenSocket == INVALID_SOCKET)
	{
		return;
	}

	closesocket(listenSocket);
	listenSocket = INVALID_SOCKET;
}

NetConnection*
NetListener::Accept()
{
	assert(listenSocket != INVALID_SOCKET);
	struct sockaddr_in addr;
	int addr_len = sizeof(struct sockaddr_in);
	SOCKET remoteSocket = accept(listenSocket, (struct sockaddr*)&addr, &addr_len);
	if (remoteSocket == INVALID_SOCKET)
	{
		return 0;
	}
	NetAddress remoteAddress(addr);
	NetConnection* connection = allocator.New<NetConnection>(remoteAddress, remoteSocket, allocator);
	return connection;
}

}


#endif
#endif

