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

#define FACTORY_NAME_LENGTH 48
#define ETHER_ADDR_OCTETS 6
#define PTP_ETHERTYPE 0x88F7
#define DEFAULT_TIMEOUT 1	// milliseconds

class LinkLayerAddress:public InterfaceLabel {
 private:
	uint8_t addr[ETHER_ADDR_OCTETS];
 public:
	LinkLayerAddress() {
	};
	LinkLayerAddress(uint64_t address_scalar) {
		uint8_t *ptr;
		address_scalar <<= 16;
		for (ptr = addr; ptr < addr + ETHER_ADDR_OCTETS; ++ptr) {
			*ptr = (address_scalar & 0xFF00000000000000ULL) >> 56;
			address_scalar <<= 8;
		}
	}
	LinkLayerAddress(uint8_t * address_octet_array) {
		uint8_t *ptr;
		for (ptr = addr; ptr < addr + ETHER_ADDR_OCTETS;
		     ++ptr, ++address_octet_array)
		{
			*ptr = *address_octet_array;
		}
	}
	bool operator==(const LinkLayerAddress & cmp) const {
		return memcmp
			(this->addr, cmp.addr, ETHER_ADDR_OCTETS) == 0 ? true : false;
	}
	bool operator<(const LinkLayerAddress & cmp)const {
		return memcmp
			(this->addr, cmp.addr, ETHER_ADDR_OCTETS) < 0 ? true : false;
	}
	bool operator>(const LinkLayerAddress & cmp)const {
		return memcmp
			(this->addr, cmp.addr, ETHER_ADDR_OCTETS) < 0 ? true : false;
	}
	void toOctetArray(uint8_t * address_octet_array) {
		uint8_t *ptr;
		for (ptr = addr; ptr < addr + ETHER_ADDR_OCTETS;
		     ++ptr, ++address_octet_array)
		{
			*address_octet_array = *ptr;
		}
	}
};

class InterfaceName: public InterfaceLabel {
 private:
	char *name;
 public:
	InterfaceName() { }
	InterfaceName(char *name, int length) {
		this->name = new char[length + 1];
		PLAT_strncpy(this->name, name, length);
	}
	bool operator==(const InterfaceName & cmp) const {
		return strcmp(name, cmp.name) == 0 ? true : false;
	}
	bool operator<(const InterfaceName & cmp)const {
		return strcmp(name, cmp.name) < 0 ? true : false;
	}
	bool operator>(const InterfaceName & cmp)const {
		return strcmp(name, cmp.name) < 0 ? true : false;
	} 
	bool toString(char *string, size_t length) {
		if (length >= strlen(name) + 1) {
			PLAT_strncpy(string, name, length);
			return true;
		}
		return false;
	}
};

class factory_name_t {
 private:
	char name[FACTORY_NAME_LENGTH];
	factory_name_t();
 public:
	factory_name_t(const char *name_a) {
		PLAT_strncpy(name, name_a, FACTORY_NAME_LENGTH - 1);
	} 
	bool operator==(const factory_name_t & cmp) {
		return strcmp(cmp.name, this->name) == 0 ? true : false;
	}
	bool operator<(const factory_name_t & cmp)const {
		return strcmp(cmp.name, this->name) < 0 ? true : false;
	}
	bool operator>(const factory_name_t & cmp)const {
		return strcmp(cmp.name, this->name) > 0 ? true : false;
	}
};

typedef enum { net_trfail, net_fatal, net_succeed } net_result;

class OSNetworkInterface {
 public:
	virtual net_result send
	(LinkLayerAddress * addr, uint8_t * payload, size_t length,
	 bool timestamp) = 0;
	virtual net_result recv
	(LinkLayerAddress * addr, uint8_t * payload, size_t & length) = 0;
	virtual void getLinkLayerAddress(LinkLayerAddress * addr) = 0;
	virtual unsigned getPayloadOffset() = 0;
	virtual ~OSNetworkInterface() = 0;
};

inline OSNetworkInterface::~OSNetworkInterface() {}

class OSNetworkInterfaceFactory;

typedef std::map < factory_name_t, OSNetworkInterfaceFactory * >FactoryMap_t;

class OSNetworkInterfaceFactory {
 public:
	static bool registerFactory
	(factory_name_t id, OSNetworkInterfaceFactory * factory) {
		FactoryMap_t::iterator iter = factoryMap.find(id);
		if (iter != factoryMap.end())
			return false;
		factoryMap[id] = factory;
		return true;
	}
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
