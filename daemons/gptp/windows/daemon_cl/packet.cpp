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


#include "packet.hpp"
#include "platform.hpp"

#include <pcap.h>
#include "iphlpapi.h"

#define MAX_FRAME_SIZE 96

#define WINPCAP_INTERFACENAMEPREFIX "rpcap://\\Device\\NPF_"
#define WINPCAP_INTERFACENAMEPREFIX_LENGTH 20
#define WINPCAP_INTERFACENAMESUFFIX_LENGTH 38
#define WINPCAP_INTERFACENAME_LENGTH WINPCAP_INTERFACENAMEPREFIX_LENGTH+WINPCAP_INTERFACENAMESUFFIX_LENGTH

struct packet_handle {
    pcap_t *iface;
    char errbuf[PCAP_ERRBUF_SIZE];
    packet_addr_t iface_addr;
    HANDLE capture_lock;
    uint16_t ethertype;
    struct bpf_program filter;
};


packet_error_t mallocPacketHandle( struct packet_handle **phandle ) {
    packet_error_t ret = PACKET_NO_ERROR;

    packet_handle *handle = (struct packet_handle *) malloc((size_t) sizeof( *handle ));
    if( handle == NULL ) {
        ret = PACKET_NOMEMORY_ERROR;
        goto fnexit;
    }
    *phandle = handle;

    if(( handle->capture_lock = CreateMutex( NULL, FALSE, NULL )) == NULL ) {
        ret = PACKET_CREATEMUTEX_ERROR;
        goto fnexit;
    }
    handle->iface = NULL;

fnexit:
    return ret;
}

void freePacketHandle( struct packet_handle *handle ) {
    CloseHandle( handle->capture_lock );
    free((void *) handle );
}

packet_error_t openInterfaceByAddr( struct packet_handle *handle, packet_addr_t *addr, int timeout ) {
    packet_error_t ret = PACKET_NO_ERROR;
    char name[WINPCAP_INTERFACENAME_LENGTH+1] = "\0";

    PIP_ADAPTER_ADDRESSES pAdapterAddress;
    IP_ADAPTER_ADDRESSES AdapterAddress[32];       // Allocate information for up to 32 NICs
    DWORD dwBufLen = sizeof(AdapterAddress);  // Save memory size of buffer

    DWORD dwStatus = GetAdaptersAddresses( AF_UNSPEC, 0, NULL, AdapterAddress, &dwBufLen);

    if( dwStatus != ERROR_SUCCESS ) {
        ret = PACKET_IFLOOKUP_ERROR;
        goto fnexit;
    }

    for( pAdapterAddress = AdapterAddress; pAdapterAddress != NULL; pAdapterAddress = pAdapterAddress->Next ) {
        if( pAdapterAddress->PhysicalAddressLength == ETHER_ADDR_OCTETS &&
            memcmp( pAdapterAddress->PhysicalAddress, addr->addr, ETHER_ADDR_OCTETS ) == 0 ) {
            break;
        }
    }

    if( pAdapterAddress != NULL ) {
        strcpy_s( name, WINPCAP_INTERFACENAMEPREFIX );
        strncpy_s( name+WINPCAP_INTERFACENAMEPREFIX_LENGTH, WINPCAP_INTERFACENAMESUFFIX_LENGTH+1, pAdapterAddress->AdapterName, WINPCAP_INTERFACENAMESUFFIX_LENGTH );
        printf( "Opening: %s\n", name );
        handle->iface = pcap_open(  name, MAX_FRAME_SIZE, PCAP_OPENFLAG_MAX_RESPONSIVENESS | PCAP_OPENFLAG_PROMISCUOUS | PCAP_OPENFLAG_NOCAPTURE_LOCAL,
                                    timeout, NULL, handle->errbuf );
        if( handle->iface == NULL ) {
            ret = PACKET_IFLOOKUP_ERROR;
            goto fnexit;
        }
        handle->iface_addr = *addr;
    } else {
        ret = PACKET_IFNOTFOUND_ERROR;
        goto fnexit;
    }

fnexit:
    return ret;
}

void closeInterface( struct packet_handle *pfhandle ) {
    if( pfhandle->iface != NULL ) pcap_close( pfhandle->iface );
}


packet_error_t sendFrame( struct packet_handle *handle, packet_addr_t *addr, uint16_t ethertype, uint8_t *payload, size_t length ) {
    packet_error_t ret = PACKET_NO_ERROR;
    uint16_t ethertype_no = PLAT_htons( ethertype );
    uint8_t *payload_ptr = payload;

    if( length < PACKET_HDR_LENGTH ) {
        ret = PACKET_BADBUFFER_ERROR;
    }

    // Build Header
    memcpy( payload_ptr, addr->addr, ETHER_ADDR_OCTETS ); payload_ptr+= ETHER_ADDR_OCTETS;
    memcpy( payload_ptr, handle->iface_addr.addr, ETHER_ADDR_OCTETS ); payload_ptr+= ETHER_ADDR_OCTETS;
    memcpy( payload_ptr, &ethertype_no, sizeof( ethertype_no ));

    if( pcap_sendpacket( handle->iface, payload, (int) length+PACKET_HDR_LENGTH ) != 0 ) {
        ret = PACKET_XMIT_ERROR;
        goto fnexit;
    }

fnexit:
    return ret;
}

packet_error_t packetBind( struct packet_handle *handle, uint16_t ethertype ) {
    packet_error_t ret = PACKET_NO_ERROR;
    char filter_expression[32] = "ether proto 0x";

    sprintf_s( filter_expression+strlen(filter_expression), 31-strlen(filter_expression), "%hx", ethertype );
    if( pcap_compile( handle->iface, &handle->filter, filter_expression, 1, 0 ) == -1 ) {
        ret = PACKET_BIND_ERROR;
        goto fnexit;
    }
    if( pcap_setfilter( handle->iface, &handle->filter ) == -1 ) {
        ret = PACKET_BIND_ERROR;
        goto fnexit;
    }

    handle->ethertype = ethertype;

fnexit:
    return ret;
}

// Call to recvFrame must be thread-safe.  However call to pcap_next_ex() isn't because of somewhat undefined memory management semantics.
// Wrap call to pcap library with mutex
packet_error_t recvFrame( struct packet_handle *handle, packet_addr_t *addr, uint8_t *payload, size_t &length ) {
    packet_error_t ret = PACKET_NO_ERROR;
    struct pcap_pkthdr *hdr_r;
    u_char *data;

    int pcap_result;
    DWORD wait_result;

    wait_result = WaitForSingleObject( handle->capture_lock, 1000 );
    if( wait_result != WAIT_OBJECT_0 ) {
        ret = PACKET_GETMUTEX_ERROR;
        goto fnexit;
    }

    pcap_result = pcap_next_ex( handle->iface, &hdr_r, (const u_char **) &data );
    if( pcap_result == 0 ) {
        ret = PACKET_RECVTIMEOUT_ERROR;
    } else if( pcap_result < 0 ) {
        ret = PACKET_RECVFAILED_ERROR;
    } else {
        length = hdr_r->len-PACKET_HDR_LENGTH >= length ? length : hdr_r->len-PACKET_HDR_LENGTH;
        memcpy( payload, data+PACKET_HDR_LENGTH, length );
        memcpy( addr->addr, data+ETHER_ADDR_OCTETS, ETHER_ADDR_OCTETS );
    }
    
    if( !ReleaseMutex( handle->capture_lock )) {
        ret = PACKET_RLSMUTEX_ERROR;
        goto fnexit;
    }

fnexit:
    return ret;
}
