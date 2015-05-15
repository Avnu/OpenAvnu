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

/**@file*/

#include <windows.h>
#include <stdint.h>
#include <ptptypes.hpp>
#include <minwinbase.h>
#include <debugout.hpp>

#define OUTSTANDING_MESSAGES 10		/*!< Number of outstanding messages on named pipe declaration*/

#define PIPE_PREFIX "\\\\.\\pipe\\"		/*!< PIPE name prefix */
#define P802_1AS_PIPENAME "gptp-ctrl"	/*!< PIPE group name */

/**
 * Provides a data structure for gPTP time
 */
typedef struct {
	int64_t ml_phoffset;		/*!< Master to local phase offset*/
	int64_t ls_phoffset;		/*!< Local to system phase offset*/
	FrequencyRatio ml_freqoffset;	/*!< Master to local frequency offset*/
	FrequencyRatio ls_freqoffset;	/*!< Local to system frequency offset*/ 
	int64_t local_time;				/*!< Local time of last update*/
	uint32_t sync_count;			/*!< Sync messages count*/
	uint32_t pdelay_count;			/*!< pdelay messages count*/
	PortState port_state;			/*!< gPTP port state*/
	DWORD  process_id;				/*!< Process ID number */
} gPtpTimeData;


#pragma pack(push,1)

/**
 * Enumeration named pipe message type. Possible values are:
 *  - BASE_MSG;
 *  - CTRL_MSG;
 *  - QUERY_MSG;
 *  - OFFSET_MSG;
 */
typedef enum { BASE_MSG = 0, CTRL_MSG, QUERY_MSG, OFFSET_MSG } NPIPE_MSG_TYPE;

/**
 * Provides a windows named pipe interface
 */
class WindowsNPipeMessage {
protected:
	DWORD sz;				/*!< Size */
	NPIPE_MSG_TYPE type;	/*!< Message type as NPIPE_MSG_TYPE*/

	OVERLAPPED ol_read;		/*!< Overlapped read*/
	DWORD ol_read_req;		/*!< Overlapped read request */
public:
	/**
	 * @brief  Initializes the interface
	 * @return void
	 */
	void init() {
		sz = sizeof( WindowsNPipeMessage );
	}
	/**
	 * @brief  Writes to the named pipe
	 * @param  pipe Pipe handler
	 * @return TRUE in case of success, FALSE in case of error.
	 */
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
	/**
	 * @brief  Reads from the pipe
	 * @param  pipe Pipe handle
	 * @param  offs base offset
	 * @param  event Event handler
	 * @return -1 if error, 0 if ERROR_IO_PENDING or bytes_read + offs
	 */
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
	/**
	 * @brief  Reads completely the overlapped result
	 * @param  pipe Pipe handler
	 * @return bytes read in case of success, -1 in case of error
	 * @todo Its not clear what GetOverlappedResult does
	 */
	long read_ol_complete( HANDLE pipe ) {
		DWORD bytes_read;
		if( GetOverlappedResult( pipe, &ol_read, &bytes_read, false ) == 0 ) {
			return -1;
		}
		return bytes_read;
	}
	/**
	 * @brief  Reads from the pipe
	 * @param  pipe Pipe handler
	 * @param  offs base offset
	 * @return -1 if error, 0 if ERROR_IO_PENDING or bytes_read + offs
	 */
    long read( HANDLE pipe, long offs ) {
		return read_ol( pipe, offs, NULL );
	}
	/**
	 * @brief  Gets pipe message type
	 * @return Pipe message type
	 */
	NPIPE_MSG_TYPE getType() { return type; }
};

/**
 * Provides an interface for the phase/frequency offsets
 */
class Offset {
public:
	int64_t ml_phoffset;	/*!< Master to local phase offset */
    FrequencyRatio ml_freqoffset;	/*!< Master to local frequency offset */
	int64_t ls_phoffset;	/*!< Local to system phase offset */
    FrequencyRatio ls_freqoffset;	/*!< Local to system frequency offset*/
    uint64_t local_time;	/*!< Local time*/
};

/**
 * Provides an interface to update the Offset information
 */
class WinNPipeOffsetUpdateMessage : public WindowsNPipeMessage {
private:
	Offset offset;
public:
	/**
	 * @brief  Initializes interface
	 * @return void
	 */
	void _init() {
		sz = sizeof(WinNPipeOffsetUpdateMessage);
		type = OFFSET_MSG;
	}
	/**
	 * @brief  Initializes interface and clears the Offset message
	 * @return void
	 */
	void init() {
		_init();
		memset( &this->offset, 0, sizeof( this->offset ));
	}
	/**
	 * @brief  Initializes the interface with specific values
	 * @param  ml_phoffset Master to local phase offset
	 * @param  ml_freqoffset Master to local frequency offset
	 * @param  ls_phoffset Local to system phase offset
	 * @param  ls_freqoffset Local to system frequency offset
	 * @param  local_time Local time
	 * @return void 
	 */
	void init( int64_t ml_phoffset, FrequencyRatio ml_freqoffset, int64_t ls_phoffset, FrequencyRatio ls_freqoffset, uint64_t local_time ) {
		_init();
		this->offset.ml_phoffset = ml_phoffset;
        this->offset.ml_freqoffset = ml_freqoffset;
		this->offset.ls_phoffset = ls_phoffset;
        this->offset.ls_freqoffset = ls_freqoffset;
        this->offset.local_time = local_time;
    }
	/**
	 * @brief  Initializes the interface based on the Offset structure
	 * @param  offset [in] Offset structure
	 * @return void
	 */
	void init( Offset *offset ) {
		_init();
		this->offset = *offset;
    }
	/**
	 * @brief  Gets master to local phase offset
	 * @return master to local phase offset
	 */
	int64_t getMasterLocalOffset() { return offset.ml_phoffset; }
	/**
	 * @brief  Gets Master to local frequency offset
	 * @return Master to local frequency offset
	 */
    FrequencyRatio getMasterLocalFreqOffset() { return offset.ml_freqoffset; }
	/**
	 * @brief  Gets local to system phase offset
	 * @return local to system phase offset
	 */
	int64_t getLocalSystemOffset() { return offset.ls_phoffset; }
	/**
	 * @brief  Gets Local to system frequency offset
	 * @return Local to system frequency offset
	 */
    FrequencyRatio getLocalSystemFreqOffset() { return offset.ls_freqoffset; }
	/**
	 * @brief  Gets local time
	 * @return Local time
	 */
    uint64_t getLocalTime() { return offset.local_time; }
};

/**
 * Enumeration CtrlWhich. It can assume the following values:
 *  - ADD_PEER;
 *  - REMOVE_PEER;
 */
typedef enum { ADD_PEER, REMOVE_PEER } CtrlWhich;
/**
 * Enumeration AddrWhich. It can assume the following values:
 *  - MAC_ADDR;
 *  - IP_ADDR;
 *  - INVALID_ADDR;
 */
typedef enum { MAC_ADDR, IP_ADDR, INVALID_ADDR } AddrWhich;

/**
 * Provides an interface for Peer addresses
 */
class PeerAddr {
public:
	AddrWhich which;	/*!< Peer address */
	/**
	 * shared memory between mac and ip addresses
	 */
	union {
		uint8_t mac[ETHER_ADDR_OCTETS];	/*!< Link Layer address */
		uint8_t ip[IP_ADDR_OCTETS];		/*!< IP Address */
	};
	/**
	 * @brief  Implements the operator '==' overloading method.
	 * @param  other [in] Reference to the peer addresses
	 * @return TRUE if mac or ip are the same. FALSE otherwise.
	 */
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
	/**
	 * @brief  Implements the operator '<' overloading method.
	 * @param  other Reference to the peer addresses to be compared.
	 * @return TRUE if mac or ip address from the object is lower than the peer's.
	 */
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

/**
 * Provides an interface for named pipe control messages
 */
class WinNPipeCtrlMessage : public WindowsNPipeMessage {
private:
	CtrlWhich which;
	PeerAddr addr;
	uint16_t flags;
public:
	/**
	 * @brief  Initializes interace's internal variables.
	 * @return void
	 */
	void init() {
		sz = sizeof(WinNPipeCtrlMessage);
		type = CTRL_MSG;
	}
	/**
	 * @brief  Initializes Interface's internal variables and sets
	 * control and addresses values
	 * @param  which Control enumeration
	 * @param  addr Peer addresses
	 * @return void
	 */
	void init( CtrlWhich which, PeerAddr addr ) {
		init();
		this->which = which;
		this->addr = addr;
		flags = 0;
	}
	/**
	 * @brief  Gets peer addresses
	 * @return PeerAddr structure
	 */
	PeerAddr getPeerAddr() { return addr; }
	/**
	 * @brief  Sets peer address
	 * @param  addr PeerAddr to set
	 * @return void
	 */
	void setPeerAddr( PeerAddr addr ) { this->addr = addr; }
	/**
	 * @brief  Gets control type
	 * @return CtrlWhich type
	 */
	CtrlWhich getCtrlWhich() { return which; }
	/**
	 * @brief  Sets control message type
	 * @param  which CtrlWhich message
	 * @return void
	 */
	void setCtrlWhich( CtrlWhich which ) { this->which = which; }
	/**
	 * @brief  Gets internal flags
	 * @return Internal flags
	 */
	uint16_t getFlags() { return flags; }
};

/**
 * WindowsNPipeQueryMessage is sent from the client to gPTP daemon to query the
 * offset.  The daemon sends WindowsNPipeMessage in response.
 * Currently there is no data associated with this message.
 */
class WinNPipeQueryMessage : public WindowsNPipeMessage {
public:
	/**
	 * @brief  Initializes the interface
	 * @return void
	 */
    void init() { type = OFFSET_MSG; sz = sizeof(*this); }
};

/**
 * Provides the client's named pipe interface
 */
typedef union {
	WinNPipeCtrlMessage a;	/*!< Control message */
	WinNPipeQueryMessage b;	/*!< Query message */
} WindowsNPipeMsgClient;

/**
 * Provides the server's named pipe interface
 */
typedef union {
	WinNPipeOffsetUpdateMessage a;	/*!< Offset update message */
} WindowsNPipeMsgServer;

#define NPIPE_MAX_CLIENT_MSG_SZ (sizeof( WindowsNPipeMsgClient )) /*!< Maximum message size for the client */
#define NPIPE_MAX_SERVER_MSG_SZ (sizeof( WindowsNPipeMsgServer ))	/*!< Maximum message size for the server */
#define NPIPE_MAX_MSG_SZ (NPIPE_MAX_CLIENT_MSG_SZ > NPIPE_MAX_SERVER_MSG_SZ ? NPIPE_MAX_CLIENT_MSG_SZ : NPIPE_MAX_SERVER_MSG_SZ)	/*!< Maximum message size */

#pragma pack(pop)

#endif
