/*
 * Copyright (C) 2017 Art and Logic.
 *
 * Implementation for a linux specific socket abstration providing an object 
 * based interface for dealing with lower level sockets.
 */
#include <iostream>
#include <stdexcept>

#include <netinet/in.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <cstring>
#include <cerrno>

#include "socket.hpp"

ASocket::ASocket(const std::string& interfaceName, uint16_t port) :
 fInterfaceName(interfaceName),
 fBindOk(false),
 fIngressTimeNano(0),
 fContinue(true)
{
	fPort = port;
	fSocketDescriptor = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (-1 == fSocketDescriptor)
	{
		std::string msg = "Failed to open socket: ";
		msg += strerror(errno);
		//std::cerr << msg << std::endl;
		throw(std::runtime_error(msg));
	}
	else
	{
		struct ifreq device;
		memset(&device, 0, sizeof(device));
		interfaceName.copy(device.ifr_name, IFNAMSIZ - 1);
		
		int err = ioctl(fSocketDescriptor, SIOCGIFHWADDR, &device);
		if (-1 == err)
		{
			std::string msg = "Failed to get interface address for '" +
			 interfaceName + "': " + strerror(errno);
			//std::cerr << msg << std::endl;
			throw(std::runtime_error(msg));
		}
		else
		{
			err = ioctl(fSocketDescriptor, SIOCGIFINDEX, &device);
			if (-1 == err)
			{
				std::string msg = "Failed to get interface index: ";
				msg += strerror(errno);
				//std::cerr << msg << std::endl;
				throw(std::runtime_error(msg));
			}
			else
			{
				fInterfaceIndex = device.ifr_ifindex;

				std::cout << "createInterface  ifindex: " << fInterfaceIndex
				 << std::endl;

				Bind();
			}
		}
	}
}
ASocket::~ASocket()
{
	// Intentionally left blank
}

bool ASocket::Open(std::mutex& keeper)
{
	bool ok = false;
	while (fContinue)
	{
		// Check for messages and process them
		Process(keeper);
	}
	return ok;
}

bool ASocket::Close()
{
	fContinue = false;
	return true;
}

bool ASocket::Process(std::mutex& keeper)
{
	bool ok = true;
	ARawPacket msg;
	if (Receive(msg, keeper))
	{
		// Call the derrived classes implementation
		ProcessData(msg);
	}
	return ok;
}

bool ASocket::Receive(ARawPacket& data, std::mutex& keeper)
{
	bool haveData = false;
	struct timeval timeout = { 0, 16000 }; // 16 ms
	{
		std::lock_guard<std::mutex> guard(keeper);

		FD_ZERO(&fReadfds);
		FD_SET(fSocketDescriptor, &fReadfds);

		int err = select(fSocketDescriptor+1, &fReadfds, NULL, NULL, &timeout);
		if (0 == err) 
		{
			// Intentionally left blank
		}
		else if (-1 == err) 
		{
			std::string msg = "select ";
			msg += (EINTR == errno ? "recv signal" : "failed");
			//std::cerr << msg << std::endl;
			throw(std::runtime_error(msg));
		} 
		else if (!FD_ISSET(fSocketDescriptor, &fReadfds)) 
		{
			std::string msg = "FD_ISSET  failed";
			//std::cerr << msg << std::endl;
			throw(std::runtime_error(msg));
		}
		else
		{
			haveData = ReceiveData(data, keeper);
		}
	}
	return haveData;
}

bool ASocket::Send(std::mutex& keeper, const std::string& ipAddress, int port,
 const ARawPacket& packet)
{
	bool ok = true;

	// !!! Add checks for ipv6 addesses right now this only supports ipv4 addresses

	struct sockaddr_in remote;
	memset(&remote, 0, sizeof(remote));
	remote.sin_family = AF_INET;
	remote.sin_port = htons(port);
	
	inet_pton(AF_INET, ipAddress.c_str(), &remote.sin_addr);

	std::lock_guard<std::mutex> guard(keeper);

	const ARawPacket::Buffer& data = packet.Data();

	int err = sendto(fSocketDescriptor, &data[0], data.size(), 0,
	 reinterpret_cast<const sockaddr *>(&remote), sizeof(remote));
	if (-1 == err)
	{
		//std::cerr << "Failed to send(" << errno << "):" << strerror(errno);
		ok = false;
	}
	return ok;
}

void ASocket::Bind()
{
	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_port = htons(fPort);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	int err = bind(fSocketDescriptor, (sockaddr *)&addr, sizeof(addr));
	if (-1 == err)
	{
		//std::cerr << "Call to bind() for failed (" << errno  << "): "
		// << strerror(errno) << std::endl;
		fBindOk = false;
	}
	else
	{
		fBindOk = true;
	}			
}

bool ASocket::ReceiveData(ARawPacket& data, std::mutex& keeper)
{
	bool haveData = false;

   struct msghdr msg;
	struct {
		struct cmsghdr cm;
		char control[256];
	} control;
	struct sockaddr_in remote;
	struct iovec sgentry;

	uint8_t buf[kReceiveBufferSize];

	memset(&msg, 0, sizeof(msg));

	msg.msg_iov = &sgentry;
	msg.msg_iovlen = 1;

	sgentry.iov_base = buf;
	sgentry.iov_len = kReceiveBufferSize;

	memset(&remote, 0, sizeof(remote));

	msg.msg_name = &remote;
	msg.msg_namelen = sizeof(remote);
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	size_t bytesReceived = recvmsg(fSocketDescriptor, &msg, 0);
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	if (bytesReceived < 0) 
	{
		if (ENOMSG == errno)
		{
			// std::cerr << "Got ENOMSG: " << __FILE__
			//  << ":" << __LINE__ << std::endl;
		}
		else
		{
			// std::cerr << "recvmsg() failed: " << strerror(errno)
			//  << std::endl;
		}
	}
	else
	{
		data.IngressTimeNano(ts);
		size_t toCopySize = bytesReceived < kReceiveBufferSize
		 ? bytesReceived : kReceiveBufferSize;
		data.Assign(buf, toCopySize);
		data.RemoteAddress(remote);
		haveData = true;
	}
	return haveData;
}
