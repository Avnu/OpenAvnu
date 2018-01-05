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

std::map<factory_name_t, std::shared_ptr<OSNetworkInterfaceFactory> >
OSNetworkInterfaceFactory::factoryMap;

LinkLayerAddress::LinkLayerAddress(const std::string& ip, uint16_t portNumber) :
 fPort(portNumber)
{
   size_t pos = ip.find(":");
   memset(fIpv6Addr, 0, sizeof(fIpv6Addr));
   fIpv4Addr = 0;
   int ok;
   if (pos != std::string::npos)
   {
      fIpVersion = 6;
      in6_addr dest;
      ok = inet_pton(AF_INET6, ip.c_str(), &dest);
      memcpy(fIpv6Addr, dest.s6_addr, sizeof(dest.s6_addr));
   }
   else
   {
      fIpVersion = 4;
      sockaddr_in dest;
      ok = inet_pton(AF_INET, ip.c_str(), &(dest.sin_addr));
      fIpv4Addr = dest.sin_addr.s_addr;
   }

   GPTP_LOG_VERBOSE("LinkLayerAddress::LinkLayerAddress  ip:%s  portNumber:%d  "
    "fIpVersion:%d", (ip.empty() ? "EMPTY" : ip.c_str()), portNumber, fIpVersion);

   if (0 == ok)
   {
      GPTP_LOG_ERROR("Error constructing LinkLayerAddress: %s does not contain "
       "a valid address string.", ip.c_str());
   }
   else if (-1 == ok)
   {
      std::string msg = "Error constructing LinkLayerAddress address family error: ";
      msg += strerror(errno);
      GPTP_LOG_ERROR(msg.c_str());
   }
}