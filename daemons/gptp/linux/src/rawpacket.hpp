/*
 * Copyright (C) 2017 Art and Logic.
 *
 * An abstration for a network data packet that provides a convient interface
 * for managing data. It includes an ingress time stamp so that we can preserve
 * the time at which the packet is received in user space.
 */
#pragma once

#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>

#include <cstring>
#include <arpa/inet.h>


class ARawPacket
{
	public:
		typedef uint8_t Type;
		typedef std::vector<Type> Buffer;

	public:
		ARawPacket() : fIngressTimeNano(0)
		{
			// Intentionally empty
		}

		~ARawPacket()
		{
			// Intentionally empty
		}

	public:
		ARawPacket& Assign(const Type* source, const size_t sourceSize)
		{
			const Type* first = source;
			const Type* last = source + sourceSize;

			fBuffer.resize(sourceSize);
			fBuffer.assign(first, last);

			return *this;
		}

		void Copy(Type* destination, const size_t destinationSize)
		{
			const size_t kBufferSize = fBuffer.size();
			if (kBufferSize <= destinationSize)
			{
				memcpy(destination, &fBuffer[0], kBufferSize);
			}
			else
			{
				std::string msg = "The raw message buffer is larger than the destination.";
				//std::cerr << msg << std::endl;
				throw(std::runtime_error(msg));
			}
		}

		void IngressTimeNano(const struct timespec& ts)
		{
			fIngressTimeNano = (ts.tv_sec * 1000000000) + ts.tv_nsec;
		}

		void IngressTimeNano(int64_t timestamp)
		{
			fIngressTimeNano = timestamp;
		}

		int64_t IngressTimeNano() const
		{
			return fIngressTimeNano;
		}

		void RemoteAddress(const sockaddr_in& other)
		{
			char buf[INET6_ADDRSTRLEN];
			inet_ntop(other.sin_family, &other.sin_addr, buf, sizeof(buf));
			fRemoteAddress = std::string(buf);
		}

		void RemoteAddress(const sockaddr_in6& other)
		{
			char buf[INET6_ADDRSTRLEN];
			inet_ntop(other.sin6_family, &other.sin6_addr, buf, sizeof(buf));
			fRemoteAddress = std::string(buf);
		}

		const std::string& RemoteAddress() const
		{
			return fRemoteAddress;
		}

		const Buffer& Data() const
		{
			return fBuffer;
		}

	private:
		Buffer fBuffer;
		int64_t fIngressTimeNano;
		std::string fRemoteAddress;
};
