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


#ifndef IPCDEF_HPP
#define IPCDEF_HPP

#include <stdint.h>
#include <ptptypes.hpp>
#include <minwinbase.h>
#include "avbts_osnet.hpp"

#define OUTSTANDING_MESSAGES 10

#define PIPE_PREFIX "\\\\.\\pipe\\"
#define P802_1AS_PIPENAME "gptp-ctrl"

#pragma pack(push,1)

typedef enum { BASE_MSG = 0, CTRL_MSG, QUERY_MSG, OFFSET_MSG } NPIPE_MSG_TYPE;

class WindowsNPipeMessage {
protected:
	DWORD sz;
	NPIPE_MSG_TYPE type;

	OVERLAPPED ol_read;
	DWORD ol_read_req;
public:
	void init() {
		sz = sizeof( WindowsNPipeMessage );
	}
	bool write( HANDLE pipe ) {
        DWORD bytes_written;
        DWORD last_error = ERROR_SUCCESS;
		if( sz == 0 ) return false;
		if( WriteFile( pipe, this, sz, &bytes_written, NULL ) == 0 ) {
            last_error = GetLastError();
        }
        if( last_error == ERROR_SUCCESS || last_error == ERROR_PIPE_LISTENING ) {
            return true;
        }
		XPTPD_ERROR( "Failed to write to named pipe: %u", last_error );
		return false;
    }
    long read_ol( HANDLE pipe, long offs, HANDLE event ) {
		DWORD bytes_read;
		long sz_l = (long) sz;
		LPOVERLAPPED lol;
		if( sz_l - offs <  0 || sz_l == 0 ) return -1;
		if( sz_l - offs == 0 ) return offs;
		ol_read_req = sz_l-offs;
		if( event != NULL ) {
			memset( &ol_read, 0, sizeof( ol_read ));
			ol_read.hEvent = event;
			lol = &ol_read;
		} else {
			lol = NULL;
		}
		if( ReadFile( pipe, ((char *)this)+offs, ol_read_req, &bytes_read, lol ) == 0 ) {
			int err = GetLastError();
			if( err != ERROR_IO_PENDING ) {
				XPTPD_ERROR( "Failed to read %d bytes from named pipe: %u", ol_read_req, err );
			}
			return err == ERROR_IO_PENDING ? 0 : -1;
		}
        return (bytes_read == sz_l-offs) ? offs+bytes_read : -1;
	}
	long read_ol_complete( HANDLE pipe ) {
		DWORD bytes_read;
		if( GetOverlappedResult( pipe, &ol_read, &bytes_read, false ) == 0 ) {
			return -1;
		}
		return bytes_read;
	}
    long read( HANDLE pipe, long offs ) {
		return read_ol( pipe, offs, NULL );
	}
	NPIPE_MSG_TYPE getType() { return type; }
};

class Offset {
public:
	int64_t ml_phoffset;
    FrequencyRatio ml_freqoffset;
	int64_t ls_phoffset;
    FrequencyRatio ls_freqoffset;
    uint64_t local_time;
};

class WinNPipeOffsetUpdateMessage : public WindowsNPipeMessage {
private:
	Offset offset;
public:
	void _init() {
		sz = sizeof(WinNPipeOffsetUpdateMessage);
		type = OFFSET_MSG;
	}
	void init() {
		_init();
		memset( &this->offset, 0, sizeof( this->offset ));
	}
	void init( int64_t ml_phoffset, FrequencyRatio ml_freqoffset, int64_t ls_phoffset, FrequencyRatio ls_freqoffset, uint64_t local_time ) {
		_init();
		this->offset.ml_phoffset = ml_phoffset;
        this->offset.ml_freqoffset = ml_freqoffset;
		this->offset.ls_phoffset = ls_phoffset;
        this->offset.ls_freqoffset = ls_freqoffset;
        this->offset.local_time = local_time;
    }
	void init( Offset *offset ) {
		_init();
		this->offset = *offset;
    }
	int64_t getMasterLocalOffset() { return offset.ml_phoffset; }
    FrequencyRatio getMasterLocalFreqOffset() { return offset.ml_freqoffset; }
	int64_t getLocalSystemOffset() { return offset.ls_phoffset; }
    FrequencyRatio getLocalSystemFreqOffset() { return offset.ls_freqoffset; }
    uint64_t getLocalTime() { return offset.local_time; }
};

typedef enum { ADD_PEER, REMOVE_PEER } CtrlWhich;
typedef enum { MAC_ADDR, IP_ADDR, INVALID_ADDR } AddrWhich;

class PeerAddr {
public:
	AddrWhich which;
	union {
		uint8_t mac[ETHER_ADDR_OCTETS];
		uint8_t ip[IP_ADDR_OCTETS];
	};
	bool operator==(const PeerAddr &other) const {
		int result;
		switch( which ) {
		case MAC_ADDR:
			result = memcmp( &other.mac, &mac, ETHER_ADDR_OCTETS );
			break;
		case IP_ADDR:
			result = memcmp( &other.ip, &ip, IP_ADDR_OCTETS );
			break;
		default:
			result = -1; // != 0
			break;
		}
		return (result == 0) ? true : false;
	}
	bool operator<(const PeerAddr &other) const {
		int result;
		switch( which ) {
		case MAC_ADDR:
			result = memcmp( &other.mac, &mac, ETHER_ADDR_OCTETS );
			break;
		case IP_ADDR:
			result = memcmp( &other.ip, &ip, IP_ADDR_OCTETS );
			break;
		default:
			result = 1; // > 0
			break;
		} 
		return (result < 0) ? true : false;
	}
};

class WinNPipeCtrlMessage : public WindowsNPipeMessage {
private:
	CtrlWhich which;
	PeerAddr addr;
	uint16_t flags;
public:
	void init() {
		sz = sizeof(WinNPipeCtrlMessage);
		type = CTRL_MSG;
	}
	void init( CtrlWhich which, PeerAddr addr ) {
		init();
		this->which = which;
		this->addr = addr;
		flags = 0;
	}
	PeerAddr getPeerAddr() { return addr; }
	void setPeerAddr( PeerAddr addr ) { this->addr = addr; }
	CtrlWhich getCtrlWhich() { return which; }
	void setCtrlWhich( CtrlWhich which ) { this->which = which; }
	uint16_t getFlags() { return flags; }
};

// WindowsNPipeQueryMessage is sent from the client to gPTP daemon to query the offset.  The daemon sends WindowsNPipeMessage in response.
// Currently there is no data associated with this message.

class WinNPipeQueryMessage : public WindowsNPipeMessage {
public:
    void init() { type = OFFSET_MSG; sz = sizeof(*this); }
};

typedef union {
	WinNPipeCtrlMessage a;
	WinNPipeQueryMessage b;
} WindowsNPipeMsgClient;

typedef union {
	WinNPipeOffsetUpdateMessage a;
} WindowsNPipeMsgServer;

#define NPIPE_MAX_CLIENT_MSG_SZ (sizeof( WindowsNPipeMsgClient ))
#define NPIPE_MAX_SERVER_MSG_SZ (sizeof( WindowsNPipeMsgServer ))
#define NPIPE_MAX_MSG_SZ (NPIPE_MAX_CLIENT_MSG_SZ > NPIPE_MAX_SERVER_MSG_SZ ? NPIPE_MAX_CLIENT_MSG_SZ : NPIPE_MAX_SERVER_MSG_SZ)

#pragma pack(pop)

#endif
