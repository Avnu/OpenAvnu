/*
 * Copyright (C) 2017 Art and Logic.
 *
 * These classes provide the interfaces for adding and delete ip addresses for
 * use in the aPTP profile processing via socket communcation from an external
 * application.
 *
 */
#pragma once

#include "rawpacket.hpp"
#include "../../common/avbts_port.hpp"

enum AddressApiMessageTypes {AddressApiAdd = 1, AddressApiDelete};

class AAddressMessage
{
	public:
		AAddressMessage();
		AAddressMessage(const ARawPacket& data);
		virtual ~AAddressMessage();

	public:
		virtual void Process();
		virtual void Process(const AAddressMessage& data, IEEE1588Port *port);

	public:
		unsigned char Type() const;
		void Type(unsigned char type);
		const std::string& Address() const;
		void Address(const std::string& address);
		void Copy(ARawPacket& packet);
		void DebugLog();

	protected:
		unsigned char fType;
		std::string fAddress;
};

class AAddAddressMessage : public AAddressMessage
{
	public:
		AAddAddressMessage();
		AAddAddressMessage(const ARawPacket& data);
		virtual ~AAddAddressMessage();

	public:
		virtual void Process();
		virtual void Process(const AAddressMessage& data, IEEE1588Port *port);
};

class ADeleteAddressMessage : public AAddressMessage
{
	public:
		ADeleteAddressMessage();
		ADeleteAddressMessage(const ARawPacket& data);
		virtual ~ADeleteAddressMessage();

	public:
		virtual void Process();
		virtual void Process(const AAddressMessage& data, IEEE1588Port *port);
};
