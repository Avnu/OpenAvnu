/*
 * Copyright (C) 2017 Art and Logic.
 *
 * An abstration for a network data packet that provides a convient interface
 * for managing data. It includes an ingress time stamp so that we can preserve
 * the time at which the packet is received in user space.
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
		typedef uint8_t ValueType;
		typedef std::vector<ValueType> Buffer;

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
		ARawPacket& Assign(const Buffer& rawBuffer)
		{
			fBuffer.resize(rawBuffer.size());
			fBuffer = rawBuffer;
			return *this;
		}

		ARawPacket& Assign(const ValueType* source, const size_t sourceSize)
		{
			const ValueType* first = source;
			const ValueType* last = source + sourceSize;

			fBuffer.resize(sourceSize);
			fBuffer.assign(first, last);

			return *this;
		}

		void Copy(ValueType* destination, const size_t destinationSize)
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

		void PushBack(const ValueType& value)
		{
			fBuffer.push_back(value);
		}

		void Swap(ARawPacket& other)
		{
			std::swap(fBuffer, other.fBuffer);
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
