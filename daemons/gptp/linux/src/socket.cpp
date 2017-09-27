/*
 * Copyright (C) 2017 Art and Logic.
 *
 * Implementation for a linux specific socket abstration providing an object
 * based interface for dealing with lower level sockets.
 *  Redistribution and use in source and binary forms, with or without 
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, 
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright 
 *      notice, this list of conditions and the following disclaimer in the 
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of Art and Logic nor the names of its 
 *      contributors may be used to endorse or promote products derived from 
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
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
	fSocketDescriptor = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	if (-1 == fSocketDescriptor)
	{
		std::string msg = "Failed to open socket: ";
		msg += strerror(errno);
		//std::cerr << msg << std::endl;
		throw(std::runtime_error(msg));
	}
	else
	{
		// Configure the socket to send to and receive from ipv4 or ipv6 addresses
		int v6OnlyValue = 0;
		setsockopt(fSocketDescriptor, SOL_SOCKET, IPV6_V6ONLY, &v6OnlyValue,
		 sizeof(v6OnlyValue));

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

				int value = 1;
				if (-1 == setsockopt(fSocketDescriptor, SOL_SOCKET, SO_REUSEADDR,
				 &value, sizeof(value)))
				{
 		   		std::cout << "warning setsocokopt failed." << std::endl;
				}

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

	struct sockaddr_in6 remoteIpv6;
	sockaddr * remote;
	size_t remoteSize;

	memset(&remoteIpv6, 0, sizeof(remoteIpv6));
	remoteIpv6.sin6_port = htons(port);
	remoteIpv6.sin6_family = AF_INET6;
	remote = reinterpret_cast<sockaddr*>(&remoteIpv6);
	remoteSize = sizeof(remoteIpv6);
	inet_pton(AF_INET6, ipAddress.c_str(), &remoteIpv6.sin6_addr);

	std::lock_guard<std::mutex> guard(keeper);

	const ARawPacket::Buffer& data = packet.Data();

	int err = sendto(fSocketDescriptor, &data[0], data.size(), 0, remote, remoteSize);
	if (-1 == err)
	{
		//std::cerr << "Failed to send(" << errno << "):" << strerror(errno);
		ok = false;
	}
	return ok;
}

void ASocket::Bind()
{
	sockaddr_in6 addrIpv6;
	sockaddr *addr;
	size_t addrSize;

	memset(&addrIpv6, 0, sizeof(addrIpv6));
	addrIpv6.sin6_port = htons(fPort);
	addrIpv6.sin6_addr = in6addr_any;
	addrIpv6.sin6_family = AF_INET6;
	addr = reinterpret_cast<sockaddr*>(&addrIpv6);
	addrSize = sizeof(addrIpv6);

	int err = bind(fSocketDescriptor, addr, addrSize);
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

	struct sockaddr_storage remoteAddress;
	size_t remoteSize = sizeof(struct sockaddr_storage);

	struct iovec sgentry;

	ARawPacket::Buffer buf(kReceiveBufferSize, 0);

	memset(&msg, 0, sizeof(msg));

	msg.msg_iov = &sgentry;
	msg.msg_iovlen = 1;

	sgentry.iov_base = &buf[0];
	sgentry.iov_len = kReceiveBufferSize;

	msg.msg_name = &remoteAddress;
	msg.msg_namelen = remoteSize;
	msg.msg_control = &control;
	msg.msg_controllen = sizeof(control);

	std::cout << "ASocket::ReceiveData  2" << std::endl;
	size_t bytesReceived = recvmsg(fSocketDescriptor, &msg, 0);
	if (bytesReceived <= 0)
	{
		// if (ENOMSG == errno)
		// {
		//  	//std::cerr << "Got ENOMSG: " << __FILE__
		//  	//  << ":" << __LINE__ << std::endl;
		// }
		// else
		// {
		//  	//std::cerr << "recvmsg() failed: " << strerror(errno)
		//  	//  << std::endl;
		// }
	}
	else
	{
		// Assign the received data to the ARawDataPacket
		// size_t toCopySize = bytesReceived < kReceiveBufferSize
		//  ? bytesReceived : kReceiveBufferSize;
		data.Assign(buf);

		// Extract and assign the remote address to the ARawDataPacket
		if (AF_INET == remoteAddress.ss_family)
		{
			sockaddr_in* address = reinterpret_cast<sockaddr_in*>(&remoteAddress);
			data.RemoteAddress(*address);
		}
		else
		{
			sockaddr_in6* address = reinterpret_cast<sockaddr_in6*>(&remoteAddress);
			data.RemoteAddress(*address);
		}

		// Extract and assign the packet timestamp (in nanoseconds) to the ARawDataPacket
		if (!(buf[0] & 0x8))
		{
			struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
			while (cmsg != NULL)
			{
				if (SOL_SOCKET == cmsg->cmsg_level &&
				 SO_TIMESTAMPING == cmsg->cmsg_type) 
				{
					struct timespec *ts_device, *ts_system;
					ts_system = ((struct timespec *) CMSG_DATA(cmsg)) + 1;
					ts_device = ts_system + 1;
					data.IngressTimeNano(*ts_device);
					break;
				}
				cmsg = CMSG_NXTHDR(&msg,cmsg);
			}
		}

		haveData = true;
	}
	return haveData;
}
