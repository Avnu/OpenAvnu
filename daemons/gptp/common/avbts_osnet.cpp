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

#include <avbts_osnet.hpp>
#include <regex>

std::map<factory_name_t, OSNetworkInterfaceFactory *>
OSNetworkInterfaceFactory::factoryMap;

LinkLayerAddress::LinkLayerAddress(const std::string& ip, uint16_t portNumber) :
 fPort(portNumber)
{
   GPTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  ip:%s  portNumber:%d", (ip.empty() ? "EMPTY" : ip.c_str()), portNumber);

   std::regex ipv4Re(R"(\b(?:(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\.){3}(?:25[0-5]|2[0-4][0-9]|1[0-9][0-9]|[1-9]?[0-9])\b)");
   if (std::regex_search(ip, ipv4Re))
   {
      fIpVersion = 4;
      memset(fIpv4Addr, 0, sizeof(fIpv4Addr));
      ParseIpAddress(ip, fIpv4Addr, ETHER_ADDR_OCTETS, ".");
   }
   else
   {
      // Assume ipv6
      // !!! Add ipv6 regex check
      fIpVersion = 6;
      memset(fIpv6Addr, 0, sizeof(fIpv6Addr));
      inet_pton(AF_INET6, ip.c_str(), fIpv6Addr);
   }
}