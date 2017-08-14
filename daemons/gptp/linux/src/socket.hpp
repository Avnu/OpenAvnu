/*
 * Copyright (C) 2017 Art and Logic.
 *
 * A linux specific socket abstration. It provides an object based interface for
 * dealing with lower level sockets.
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
		ASocket(const std::string& interfaceName, uint16_t port = 0,
		 int ipVersion = 4);
		virtual ~ASocket();

   public:
		bool Open(std::mutex& keeper);

		bool Close();

		bool Send(std::mutex& keeper, const std::string& ipAddress, int port,
		const ARawPacket& packet);

		void IpVersion(int version);

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
		int fIpVersion;
};
