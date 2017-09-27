/*
 * Copyright (C) 2017 Art and Logic.
 *
 * A linux specific socket abstration. It provides an object based interface for
 * dealing with lower level sockets.
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

#include <mutex>
#include <string>

#include "rawpacket.hpp"

class ASocket
{
	public:
		static const size_t kReceiveBufferSize = 128;

	public:
		ASocket(const std::string& interfaceName, uint16_t port = 0);
		virtual ~ASocket();

   public:
		bool Open(std::mutex& keeper);

		bool Close();

		bool Send(std::mutex& keeper, const std::string& ipAddress, int port,
		const ARawPacket& packet);


		// To be implemented by derrived classes, received data is passed into
		// this member function.
		virtual void ProcessData(ARawPacket& data) = 0;

	private:
		bool Process(std::mutex& keeper);
		bool Receive(ARawPacket& data, std::mutex& keeper);
		void Bind();
		bool ReceiveData(ARawPacket& data, std::mutex& keeper);

	private:
		std::string fInterfaceName;
		int fSocketDescriptor;
		int fInterfaceIndex;
		bool fBindOk;
		uint16_t fPort;
		fd_set fReadfds;
		int64_t fIngressTimeNano;
		bool fContinue;
};
