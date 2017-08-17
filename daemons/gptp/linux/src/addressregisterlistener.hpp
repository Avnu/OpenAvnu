/*
 * Copyright (C) 2017 Art and Logic.
 *
 * An address registration api listener socket that listens on port 9001
 * for address api messages.
 *
 */

#pragma once

#include <map>

#include "rawpacket.hpp"
#include "socket.hpp"
#include "addressapi.hpp"
#include "linux_hal_common.hpp"

class AAddressRegisterListener : public ASocket
{
	public:
		static const int kPortNumber = 9000;

	public:
		AAddressRegisterListener(const std::string& interfaceName,
		 IEEE1588Port *ptpPort) :
		 ASocket(interfaceName, ptpPort->AdrRegSocketPort(), ptpPort->IpVersion()),
		 fPtpPort(ptpPort)
		{
			// Intentionally empty
		}

		virtual ~AAddressRegisterListener()
		{
			// Intentionally empty
		}

		bool Start();
		void Stop();
		virtual void ProcessData(ARawPacket& data);

		void PtpPort(IEEE1588Port* ptpPort)
		{
			fPtpPort = ptpPort;
		}

		void ThreadFactory(std::shared_ptr<LinuxThreadFactory> factory)
		{
			fThreadFactory = factory;
		}

	private:
		IEEE1588Port *fPtpPort;
		std::shared_ptr<LinuxThreadFactory> fThreadFactory;
		std::shared_ptr<OSThread> fThread;
};
