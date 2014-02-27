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

#include <windows.h>
#include <PeerList.hpp>
#include <IPCListener.hpp>
#include <ipcdef.hpp>

#define OUTSTANDING_MESSAGES 10

DWORD WINAPI IPCListener::IPCListenerLoop( IPCSharedData *arg ) {
	PeerList *list = arg->list;
	LockableOffset *loffset = arg->offset;
	HANDLE pipe = INVALID_HANDLE_VALUE;

	uint8_t tmp[NPIPE_MAX_MSG_SZ];
	OVERLAPPED pipe_ol;
	HANDLE ol_event;
	enum { PIPE_CLOSED, PIPE_UNCONNECT, PIPE_CONNECT_PENDING, PIPE_CONNECT, PIPE_READ_PENDING } pipe_state;

	// Open named pipe
    char pipename[64];
    strncpy_s( pipename, 64, PIPE_PREFIX, 63 );
	strncpy_s( pipename+strlen(pipename), 64-strlen(pipename), P802_1AS_PIPENAME, 63-strlen(pipename) );
	ol_event = CreateEvent( NULL, true, false, NULL );
	pipe_state = PIPE_CLOSED;

	DWORD retval = -1;

	while( !exit_waiting ) {
		int err;
		DWORD ret;
		if( pipe_state < PIPE_UNCONNECT ) {
			if( pipe != INVALID_HANDLE_VALUE ) {
				DisconnectNamedPipe( pipe );
				CloseHandle( pipe );
			}
			pipe = CreateNamedPipe( pipename, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE , PIPE_UNLIMITED_INSTANCES,
				OUTSTANDING_MESSAGES*NPIPE_MAX_SERVER_MSG_SZ, OUTSTANDING_MESSAGES*NPIPE_MAX_MSG_SZ, 0, NULL );
			if( pipe == INVALID_HANDLE_VALUE ) {
				XPTPD_ERROR( "Open pipe error (%s): %d", pipename, GetLastError() );
				goto do_error;
			}
			pipe_state = PIPE_UNCONNECT;
		} else if( pipe_state < PIPE_CONNECT ) {
			if( pipe_state != PIPE_CONNECT_PENDING ) {
				memset( &pipe_ol, 0, sizeof( pipe_ol ));
				pipe_ol.hEvent = ol_event;
				if( ResetEvent( ol_event ) == 0 ) goto do_error;
				if( ConnectNamedPipe( pipe, &pipe_ol ) != 0 ) {
					// Successfully connected
					pipe_state = PIPE_CONNECT;
					continue;
				} else {
					err = GetLastError();
					switch( err ) {
					default:
						XPTPD_ERROR( "Attempt to connect on Pipe failed, %d", err );
						goto do_error;
					case ERROR_PIPE_CONNECTED:
						pipe_state = PIPE_CONNECT;
						continue;
					case ERROR_IO_PENDING:
						pipe_state = PIPE_CONNECT_PENDING;
					}
				}
			}
			ret = WaitForSingleObject( ol_event, 200 );
			switch( ret ) {
			case WAIT_OBJECT_0:
				pipe_state = PIPE_CONNECT;
			case WAIT_TIMEOUT:
				continue;
			default:
				goto do_error;
			}
		} else {
			// We're connected
			long readlen;
			if( pipe_state < PIPE_READ_PENDING ) {
				// Wait for message - Read Base Message
				((WindowsNPipeMessage *)tmp)->init();
				if( ResetEvent( ol_event ) == 0 ) goto do_error;
				if(( readlen =  ((WindowsNPipeMessage *)tmp)->read_ol( pipe, 0, ol_event )) == -1 ) {
					err = GetLastError();
					switch( err ) {
					default:
						XPTPD_ERROR( "Failed to read from pipe @%u,%d", __LINE__, err );
						goto do_error;
					case ERROR_BROKEN_PIPE:
						pipe_state = PIPE_CLOSED;
						continue;
					case ERROR_IO_PENDING:
						;
					}
				}
				// Fall through to here whether data is available or not
				pipe_state = PIPE_READ_PENDING;
			}
			ret = WaitForSingleObject( ol_event, 200 );
			switch( ret ) {
			default:
				goto do_error;
			case WAIT_TIMEOUT:
				continue;
			case WAIT_OBJECT_0:
				if(( readlen = ((WinNPipeCtrlMessage *)tmp)->read_ol_complete( pipe )) == -1 ) {
					err = GetLastError();
					if( err == ERROR_BROKEN_PIPE ) {
						pipe_state = PIPE_CLOSED;
						continue;
					}
					XPTPD_ERROR( "Failed to read from pipe @%u,%d", __LINE__, err );
					goto do_error;
				}
				switch( ((WindowsNPipeMessage *)tmp)->getType() ) {
				case CTRL_MSG:
					((WinNPipeCtrlMessage *)tmp)->init();
					if(( readlen = ((WinNPipeCtrlMessage *)tmp)->read( pipe, readlen )) == -1 ) {
						XPTPD_ERROR( "Failed to read from pipe @%u", __LINE__ );
						goto do_error;
					}
					//readlen may not be set properly ??
					// Attempt to add or remove from the list
					switch( ((WinNPipeCtrlMessage *)tmp)->getCtrlWhich() ) {
					default:
						XPTPD_ERROR( "Recvd CTRL cmd specifying illegal operation @%u", __LINE__ );
						goto do_error;
					case ADD_PEER:
						if( !list->IsReady() || !list->add( ((WinNPipeCtrlMessage *)tmp)->getPeerAddr() ) ) {
							XPTPD_ERROR( "Failed to add peer @%u", __LINE__ );
						}
						break;
					case REMOVE_PEER:
						if( !list->IsReady() || !list->remove( ((WinNPipeCtrlMessage *)tmp)->getPeerAddr() ) ) {
							XPTPD_ERROR( "Failed to remove peer @%u", __LINE__ );
						}
						break;
					}
					break;
				case OFFSET_MSG:
					((WinNPipeQueryMessage *)tmp)->init();
					if(( readlen = ((WinNPipeQueryMessage *)tmp)->read( pipe, readlen )) == -1 ) {
						XPTPD_ERROR( "Failed to read from pipe @%u", __LINE__ );
						goto do_error;
					}
					// Create an offset message and send it
					loffset->get();
					if( loffset->isReady() ) ((WinNPipeOffsetUpdateMessage *)tmp)->init((Offset *)loffset);
					else ((WinNPipeOffsetUpdateMessage *)tmp)->init();
					loffset->put();
					((WinNPipeOffsetUpdateMessage *)tmp)->write(pipe);
					break;
				default:
					XPTPD_ERROR( "Recvd Unknown Message" );
					// Is this recoverable?
					goto do_error;
				}
				pipe_state = PIPE_CONNECT;
			}
		}
	}


	retval = 0; // Exit normally
do_error:
	// Close Named Pipe
	if( pipe != INVALID_HANDLE_VALUE ) CloseHandle( pipe );

	return retval;
}

































