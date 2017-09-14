/*
 * Copyright (C) 2017 Art and Logic.
 *
 * Implementation for the address api. These classes provide the interfaces for
 * adding and delete ip addresses for use in the aPTP profile processing via
 * socket communcation from an external application.
 *
 */
#include <iostream>
#include <stdexcept>
#include <algorithm>

#include "addressapi.hpp"

static const int kAddressApiOffset = 0;
static const int kAddressApiMsgLength = 46;
static const int kAddressApiIpAddressLength = 45;


std::string& StripTrailing(std::string& value)
{
	using std::string;

   // No need to manipulate empty strings.
   if (value.empty())
   {
      return value;
   }

   // Iterate from the end of the string
   // until we hit the first non-space character
   const string::reverse_iterator limit = value.rend();
   string::reverse_iterator       p     = value.rbegin();
   for (; p != limit && (isspace(*p) || 0 == *p); ++p)
   {
   } // Intentionally empty

   // Remove the spaces
   value.erase(p.base(), value.end());

   return value;
}

AAddressMessage::AAddressMessage()
{}

AAddressMessage::AAddressMessage(const ARawPacket& data)
{
	const ARawPacket::Buffer& buf = data.Data();

	auto begin = buf.begin();
	fType = *begin;	
	++begin;
	fAddress.assign(begin, buf.end());

	StripTrailing(fAddress);

	if (sizeof(fType) + fAddress.length() > kAddressApiMsgLength)
	{
		std::string msg = "Address length exceeds maximum size of " +
		 std::to_string(kAddressApiMsgLength - sizeof(fType));
		GPTP_LOG_ERROR(msg.c_str());
		//std::cerr << msg << std::endl;
		throw(std::runtime_error(msg));
	}
	//DebugLog();
}

AAddressMessage::~AAddressMessage()
{}


void AAddressMessage::Process(const AAddressMessage& data, IEEE1588Port *port)
{}

unsigned char AAddressMessage::Type() const
{
	return fType;
}

void AAddressMessage::Type(unsigned char type)
{
	fType = type;
}

const std::string& AAddressMessage::Address() const
{
	return fAddress;
}

void AAddressMessage::Address(const std::string& address)
{
	fAddress = address;
}

void AAddressMessage::Copy(ARawPacket& packet)
{
	if (sizeof(fType) + fAddress.length() > kAddressApiMsgLength)
	{
		std::string msg = "Address length exceeds maximum size of " +
		 std::to_string(kAddressApiMsgLength - sizeof(fType));
		GPTP_LOG_ERROR(msg.c_str());
		//std::cerr << msg << std::endl;
		throw(std::runtime_error(msg));
	}

	ARawPacket dataPacket;
	dataPacket.PushBack(fType);
	std::for_each(fAddress.begin(), fAddress.end(), [&] (const char value)
	{
		dataPacket.PushBack(static_cast<ARawPacket::ValueType>(value));
	});

	packet.Swap(dataPacket);
}

void AAddressMessage::DebugLog()
{
	std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
	std::cout << "message type: " << int(fType) << std::endl;
	std::cout << "address: " << fAddress << std::endl;
	std::cout << "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~" << std::endl;
}

//-----------------------------------------------------------------------------

AAddAddressMessage::AAddAddressMessage()
{
	fType = AddressApiAdd;
}

AAddAddressMessage::AAddAddressMessage(const ARawPacket& data) :
 AAddressMessage(data)
{
	if (fType != AddressApiAdd)
	{
		std::string msg = "Invalid message type in packet.";
		GPTP_LOG_ERROR(msg.c_str());
		//std::cerr << msg << std::endl;
		throw(std::runtime_error(msg));
	}
}

AAddAddressMessage::~AAddAddressMessage()
{}

void AAddAddressMessage::Process(const AAddressMessage& data, IEEE1588Port *port)
{
	if (port != nullptr)
	{
		port->AddUnicastSendNode(data.Address());
	}
}

//-----------------------------------------------------------------------------

ADeleteAddressMessage::ADeleteAddressMessage()
{
	fType = AddressApiDelete;
}

ADeleteAddressMessage::ADeleteAddressMessage(const ARawPacket& data) :
 AAddressMessage(data)
{
	if (fType != AddressApiDelete)
	{
		std::string msg = "Invalid message type in packet.";
		GPTP_LOG_ERROR(msg.c_str());
		//std::cerr << msg << std::endl;
		throw(std::runtime_error(msg));
	}
}

ADeleteAddressMessage::~ADeleteAddressMessage()
{}

void ADeleteAddressMessage::Process(const AAddressMessage& data, IEEE1588Port *port)
{
	if (port != nullptr)
	{
		port->DeleteUnicastSendNode(data.Address());
	}
}
