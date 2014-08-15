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

#ifndef PEERLIST_HPP
#define PEERLIST_HPP

#include <vector>
#include <ipcdef.hpp>
#include <Lockable.hpp>

typedef struct {
	PeerAddr addr;
	void *state; // Used by consumers of 
} PeerAddrWithState;


typedef bool (*peer_init_handler)( void **, void *, PeerAddr addr );
typedef bool (*peer_rm_handler)( void *, void * );

typedef std::vector<PeerAddrWithState> PeerVector;

class PeerList : public Lockable {
private:
	peer_init_handler init;
	void *handler_arg;
	peer_rm_handler rm;
	PeerVector internal_vector;
	bool ready;
public:
	typedef PeerVector::const_iterator const_iterator;
	typedef PeerVector::iterator PeerVectorIt;
	PeerList() { rm = NULL; init = NULL; }
	PeerVectorIt find( PeerAddr addr, bool *found ) {
		PeerVectorIt it = internal_vector.begin();
		*found = false;
		if( internal_vector.size() == 0 ) {
			goto done;
		}
		for( ;it < internal_vector.end(); ++it ) {
			if( addr == (*it).addr ) {
				*found = true;
				break;
			}
			if( addr < (*it).addr ) {
				break;
			}
		}
done:
		return it;
	}	
	bool add( PeerAddr addr ) {
		bool found;
		PeerVectorIt it;
		it = find( addr, &found );
		if( found ) {
			return false;
		} else {
			PeerAddrWithState addr_state = { addr, NULL };
			if( init ) {
				if( !init( &addr_state.state, handler_arg, addr )) {
					//DBGPRINT("Call to initialize peer state failed");
					// return false?
				} else {
					internal_vector.insert( it, addr_state );
				}
			}
		}
		return true;
	}
	bool remove( PeerAddr addr ) {
		bool found;
		PeerVectorIt it;
		it = find( addr, &found );
		if( !found ) {
			return false;
		} else {
			if( rm != NULL ) {
				if( !rm( it->state, handler_arg ) ) {
					fprintf( stderr, "Call to cleanup peer state failed\n" );
				}
			}
			internal_vector.erase( it );
		}
		return true;
	}
	const_iterator begin() {
		return internal_vector.begin();
	}
	const_iterator end() {
		return internal_vector.end();
	}
	void setInit( peer_init_handler init, void *init_arg ) {
		this->init = init;
		this->handler_arg = init_arg;
	}
	void setRm( peer_rm_handler rm ) {
		this->rm = rm;
	}
	bool IsReady() { return ready; }
	void setReady( bool ready ) { this->ready = ready; }
};

#endif/*PEERLIST_HPP*/