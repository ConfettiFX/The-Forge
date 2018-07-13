#include "../../../include/gainput/gainput.h"

#ifdef GAINPUT_DEV
#include "GainputNetAddress.h"
#include "GainputNetConnection.h"

#include "GainputStream.h"

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_WIN) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
#include <cassert>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace gainput {

#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
NetConnection::NetConnection(const NetAddress& address, Allocator& allocator) :
	allocator(allocator),
	address(address),
	fd(-1)
{
}

NetConnection::NetConnection(const NetAddress& remoteAddress, int fd, Allocator& allocator) :
	allocator(allocator),
	address(remoteAddress),
	fd(fd)
{
}

NetConnection::~NetConnection()
{
	Close();
}

bool
NetConnection::Connect(bool shouldBlock)
{
	assert(fd == -1);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == -1)
	{
		return false;
	}

	if (connect(fd, (struct sockaddr*)&address.GetAddr(), sizeof(struct sockaddr_in)) == -1)
	{
		return false;
	}

	if (!shouldBlock && fcntl(fd, F_SETFL, O_NONBLOCK) == -1)
	{
		return false;
	}

	return true;
}

bool
NetConnection::IsConnected() const
{
	return fd != -1;
}

void
NetConnection::Close()
{
	if (fd == -1)
	{
		return;
	}

	close(fd);
	fd = -1;
}
#elif defined(GAINPUT_PLATFORM_WIN)
NetConnection::NetConnection(const NetAddress& address, Allocator& allocator) :
	allocator(allocator),
	address(address),
	fd(INVALID_SOCKET)
{
}

NetConnection::NetConnection(const NetAddress& remoteAddress, SOCKET fd, Allocator& allocator) :
	allocator(allocator),
	address(remoteAddress),
	fd(fd)
{
}

NetConnection::~NetConnection()
{
	Close();
}

bool
NetConnection::Connect(bool shouldBlock)
{
	assert(fd == INVALID_SOCKET);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd == INVALID_SOCKET)
	{
		return false;
	}

	if (connect(fd, (struct sockaddr*)&address.GetAddr(), sizeof(struct sockaddr_in)) == -1)
	{
		return false;
	}

	if (!shouldBlock)
	{
		u_long NonBlock = 1;
		if (ioctlsocket(fd, FIONBIO, &NonBlock) == SOCKET_ERROR)
		{
			return false;
		}
	}

	return true;
}

bool
NetConnection::IsConnected() const
{
	return fd != INVALID_SOCKET;
}

void
NetConnection::Close()
{
	if (fd == INVALID_SOCKET)
	{
		return;
	}

	closesocket(fd);
	fd = INVALID_SOCKET;
}
#endif

bool
NetConnection::IsReady(bool read, bool write)
{
	fd_set read_fds;
	fd_set write_fds;
	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);

	if (read)
	{
		FD_SET(fd, &read_fds);
	}
	if (write)
	{
		FD_SET(fd, &write_fds);
	}

	struct timeval timeout;
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	int rc = select(sizeof(read_fds)*8, &read_fds, &write_fds, 0, &timeout);
	if (rc == -1)
	{
		return false;
	}

	bool result = false;
	if (rc > 0)
	{
		result = true;
		if (read && !FD_ISSET(fd, &read_fds))
		{
			result = false;
		}
		if (write && !FD_ISSET(fd, &write_fds))
		{
			result = false;
		}
	}

	return result;
}

size_t
NetConnection::Send(const void* buf, size_t length)
{
	if (!IsConnected())
	{
		return 0;
	}
	assert(IsConnected());
#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID)
	std::ptrdiff_t const result = send(fd, buf, length, MSG_NOSIGNAL);
	if (result == -1 && errno == EPIPE)
#elif defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC)|| defined(GAINPUT_PLATFORM_TVOS)
	std::ptrdiff_t const result = send(fd, buf, length, 0);
	if (result == -1 && errno == EPIPE)
#elif defined(GAINPUT_PLATFORM_WIN)
	std::ptrdiff_t const result = send(fd, (const char*)buf, length, 0);
	if (result == -1)
#endif
	{
		Close();
	}
	return result >= 0 ? static_cast<std::size_t>(result) : 0;
}

size_t
NetConnection::Send(Stream& stream)
{
	const size_t length = stream.GetLeft();
	char* buf = (char*)allocator.Allocate(length);
	stream.Read(buf, length);
	size_t sentLength = Send(buf, length);
	allocator.Deallocate(buf);
	return sentLength;
}

size_t
NetConnection::Receive(void* buffer, size_t length)
{
	assert(IsConnected());
#if defined(GAINPUT_PLATFORM_LINUX) || defined(GAINPUT_PLATFORM_ANDROID) || defined(GAINPUT_PLATFORM_IOS) || defined(GAINPUT_PLATFORM_MAC) || defined(GAINPUT_PLATFORM_TVOS)
	ssize_t receivedLength = recv(fd, (char*)buffer, length, 0);
	if (receivedLength == -1)
#elif defined(GAINPUT_PLATFORM_WIN)
	int receivedLength = recv(fd, (char*)buffer, length, 0);
	if (receivedLength == SOCKET_ERROR)
#endif
	{
		return 0;
	}
	return receivedLength;
}

size_t
NetConnection::Receive(Stream& stream, size_t maxLength)
{
	char* buf = (char*)allocator.Allocate(maxLength);
	size_t receivedLength = Receive(buf, maxLength);
	stream.Write(buf, receivedLength);
	stream.SeekBegin(0);
	allocator.Deallocate(buf);
	return receivedLength;
}

}

#endif
#endif

