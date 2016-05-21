/******************************************************************************

  Copyright (c) 2009-2012, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

******************************************************************************/

#ifndef AVBTS_OSNET_HPP
#define AVBTS_OSNET_HPP

#include <stdint.h>
#include <string.h>
#include <map>
#include <ieee1588.hpp>
#include <ptptypes.hpp>

/**@file*/

#define FACTORY_NAME_LENGTH 48		/*!< Factory name maximum length */
#define DEFAULT_TIMEOUT 1			/*!< Default timeout in milliseconds*/

/**
 * LinkLayerAddress Class
 * Provides methods for initializing and comparing ethernet addresses.
 */
class LinkLayerAddress:public InterfaceLabel {
 private:
	//!< Ethernet address
	uint8_t addr[ETHER_ADDR_OCTETS];
 public:
	/**
	 * Default constructor
	 */
	LinkLayerAddress() {
	};

	/**
	 * Receives a 64bit scalar
	 * address and initializes its internal octet array with
	 * the first 48 bits.
	 * @param address_scalar 64 bit address
	 */
	LinkLayerAddress(uint64_t address_scalar) {
		uint8_t *ptr;
		address_scalar <<= 16;
        if(addr == NULL)
            return;

        for (ptr = addr; ptr < addr + ETHER_ADDR_OCTETS; ++ptr) {
			*ptr = (address_scalar & 0xFF00000000000000ULL) >> 56;
			address_scalar <<= 8;
		}
	}

	/**
	 * Receives an address as an array of octets
	 * and copies the first 6 over the internal ethernet address.
	 * @param address_octet_array Array of octets containing the address
	 */
	LinkLayerAddress(uint8_t * address_octet_array) {
		uint8_t *ptr;
        if( addr == NULL || address_octet_array == NULL)
            return;

		for (ptr = addr; ptr < addr + ETHER_ADDR_OCTETS;
		     ++ptr, ++address_octet_array)
		{
			*ptr = *address_octet_array;
		}
	}

	/**
	 * @brief  Operator '==' overloading method.
	 * It provides a comparison between cmp and the class ethernet address defined
	 * at its constructor.
	 * @param  cmp Value to be compared against.
	 * @return TRUE if they are equal; FALSE otherwise.
	 */
	bool operator==(const LinkLayerAddress & cmp) const {
		return memcmp
			(this->addr, cmp.addr, ETHER_ADDR_OCTETS) == 0 ? true : false;
	}

	/**
	 * @brief  Operator '<' overloading method.
	 ** It provides a comparison between cmp and the class ethernet address defined
	 * at its constructor.
	 * @param  cmp Value to be compared against.
	 * @return TRUE if cmp is lower than addr, FALSE otherwise.
	 */
	bool operator<(const LinkLayerAddress & cmp)const {
		return memcmp
			(this->addr, cmp.addr, ETHER_ADDR_OCTETS) < 0 ? true : false;
	}

	/**
	 * @brief  Operator '>' overloading method.
	 ** It provides a comparison between cmp and the class ethernet address defined
	 * at its constructor.
	 * @param  cmp Value to be compared against.
	 * @return TRUE if cmp is bigger than addr, FALSE otherwise.
	 */
	bool operator>(const LinkLayerAddress & cmp)const {
		return memcmp
			(this->addr, cmp.addr, ETHER_ADDR_OCTETS) < 0 ? true : false;
	}

	/**
	 * @brief  Gets first 6 bytes from ethernet address of
	 * object LinkLayerAddress.
	 * @param  address_octet_array [out] Pointer to store the
	 * ethernet address information.
	 * @return void
	 */
	void toOctetArray(uint8_t * address_octet_array) {
		uint8_t *ptr;
        if(addr == NULL || address_octet_array == NULL)
            return;
		for (ptr = addr; ptr < addr + ETHER_ADDR_OCTETS;
		     ++ptr, ++address_octet_array)
		{
			*address_octet_array = *ptr;
		}
	}
};

/**
 * Class InterfaceName
 * Provides methods for dealing with the network interface name
 * @todo: Destructor doesnt delete this->name.
 */
class InterfaceName: public InterfaceLabel {
 private:
	//!< Interface name
	char *name;
 public:
	/**
	 * Default constructor
	 */
	InterfaceName() { }
	/**
	 * Initializes Interface name with name and size lenght+1
	 * @param name [in] String with the interface name
	 * @param length Size of name
	 */
	InterfaceName(char *name, int length) {
		this->name = new char[length + 1];
		PLAT_strncpy(this->name, name, length);
	}
    ~InterfaceName() {
        delete(this->name);
    }

	/**
	 * @brief  Operator '==' overloading method.
	 * Compares parameter cmp to the interface name
	 * @param  cmp String to be compared
	 * @return TRUE if they are equal, FALSE otherwise
	 */
	bool operator==(const InterfaceName & cmp) const {
		return strcmp(name, cmp.name) == 0 ? true : false;
	}

	/**
	 * @brief  Operator '<' overloading method.
	 * Compares cmp to the interface name
	 * @param  cmp String to be compared
	 * @return TRUE if interface name is found to be less than cmd. FALSE otherwise
	 */
	bool operator<(const InterfaceName & cmp)const {
		return strcmp(name, cmp.name) < 0 ? true : false;
	}

	/**
	 * @brief  Operator '>' overloading method.
	 * Compares cmp to the interface name
	 * @param  cmp String to be compared
	 * @return TRUE if the interface name is found to be greater than cmd. FALSE otherwise
	 */
	bool operator>(const InterfaceName & cmp)const {
		return strcmp(name, cmp.name) < 0 ? true : false;
	}

	/**
	 * @brief  Gets interface name from the class' internal variable
	 * @param  string [out] String to store interface's name
	 * @param  length Length of string
	 * @return TRUE if length is greater than size of interface name plus one. FALSE otherwise.
	 */
	bool toString(char *string, size_t length) {
        if(string == NULL)
            return false;

		if (length >= strlen(name) + 1) {
			PLAT_strncpy(string, name, length);
			return true;
		}
		return false;
	}
};

/**
 * factory_name_t class
 * Provides a generic class to be used as a key to create factory maps.
 */
class factory_name_t {
 private:
	/*<! Factory name*/
	char name[FACTORY_NAME_LENGTH];
	factory_name_t();
 public:
	/**
	 * Assign a name to the factory_name
	 * @param name_a [in] Name to be assigned to the object
	 */
	factory_name_t(const char *name_a) {
		PLAT_strncpy(name, name_a, FACTORY_NAME_LENGTH - 1);
	}

	/**
	 * @brief  Operator '==' overloading method
	 * Compares cmp to the factory name
	 * @param  cmp String to be compared
	 * @return TRUE if they are equal, FALSE otherwise
	 */
	bool operator==(const factory_name_t & cmp) {
		return strcmp(cmp.name, this->name) == 0 ? true : false;
	}

	/**
	 * @brief  Operator '<' overloading method
	 * Compares cmp to the factory name
	 * @param  cmp String to be compared
	 * @return TRUE if the factory_name is to be found less than cmp, FALSE otherwise
	 */
	bool operator<(const factory_name_t & cmp)const {
		return strcmp(cmp.name, this->name) < 0 ? true : false;
	}

	/**
	 * @brief  Operator '>' overloading method
	 * Compares cmp to the factory name
	 * @param  cmp String to be compared
	 * @return TRUE if the factory_name is to be found greater than cmp, FALSE otherwise
	 */
	bool operator>(const factory_name_t & cmp)const {
		return strcmp(cmp.name, this->name) > 0 ? true : false;
	}
};

/**
 * Enumeration net_result:
 * 	- net_trfail
 * 	- net_fatal
 * 	- net_succeed
 */
typedef enum { net_trfail, net_fatal, net_succeed } net_result;


/**
 * Enumeration net_link_event:
 * 	- net_linkup
 * 	- net_linkdown
 */
typedef enum { NET_LINK_EVENT_DOWN, NET_LINK_EVENT_UP, NET_LINK_EVENT_FAIL } net_link_event;

/**
 * Provides a generic network interface
 */
class OSNetworkInterface {
 public:
	 /**
	  * @brief Sends a packet to a remote address
	  * @param addr [in] Remote link layer address
	  * @param etherType [in] The EtherType of the message
	  * @param payload [in] Data buffer
	  * @param length Size of data buffer
	  * @param timestamp TRUE if to use the event socket with the PTP multicast address. FALSE if to use
	  * a general socket.
	  */
	 virtual net_result send
		(LinkLayerAddress * addr, uint16_t etherType, uint8_t * payload, size_t length,
		  bool timestamp) = 0;

	 /**
	  * @brief  Receives data
	  * @param  addr [out] Destination Mac Address
	  * @param  payload [out] Payload received
	  * @param  length [out] Received length
	  * @return net_result enumeration
	  */
	 virtual net_result nrecv
		(LinkLayerAddress * addr, uint8_t * payload, size_t & length, struct phy_delay *delay) = 0;

	 /**
	  * @brief Get Link Layer address (mac address)
	  * @param addr [out] Link Layer address
	  * @return void
	  */
	 virtual void getLinkLayerAddress(LinkLayerAddress * addr) = 0;

	 /**
	  * @brief Watch for netlink changes.
	  */
	 virtual void watchNetLink(IEEE1588Port *pPort) = 0;

	 /**
	  * @brief  Provides generic method for getting the payload offset
	  */
	 virtual unsigned getPayloadOffset() = 0;

	 /**
	  * Native support for polimorphic destruction
	  */
	 virtual ~OSNetworkInterface() = 0;
};

inline OSNetworkInterface::~OSNetworkInterface() {}

class OSNetworkInterfaceFactory;

/**
 * Provides a map for the OSNetworkInterfaceFactory::registerFactory method
 */
typedef std::map < factory_name_t, OSNetworkInterfaceFactory * >FactoryMap_t;

/**
 * Builds and registers a network interface
 */
class OSNetworkInterfaceFactory {
 public:
	 /**
	  * @brief  Registers network factory
	  * @param id
	  * @param factory Factory name
	  * @return TRUE success, FALSE when could not register it.
	  */
	static bool registerFactory
	(factory_name_t id, OSNetworkInterfaceFactory * factory) {
		FactoryMap_t::iterator iter = factoryMap.find(id);
		if (iter != factoryMap.end())
			return false;
		factoryMap[id] = factory;
		return true;
	}

	/**
	 * @brief Builds the network interface
	 * @param iface [out] Pointer to interface name
	 * @param id Factory name index
	 * @param iflabel Interface label
	 * @param timestamper HWTimestamper class pointer
	 * @return TRUE ok, FALSE error.
	 */
	static bool buildInterface
	(OSNetworkInterface ** iface, factory_name_t id, InterfaceLabel * iflabel,
	 HWTimestamper * timestamper) {
		return factoryMap[id]->createInterface
			(iface, iflabel, timestamper);
	}
	virtual ~OSNetworkInterfaceFactory() = 0;
private:
	virtual bool createInterface
	(OSNetworkInterface ** iface, InterfaceLabel * iflabel,
	 HWTimestamper * timestamper) = 0;
	static FactoryMap_t factoryMap;
};

inline OSNetworkInterfaceFactory::~OSNetworkInterfaceFactory() { }

#endif
