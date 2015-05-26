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

/**@file*/

/**
 * Type Peer address with state
 */
typedef struct {
	PeerAddr addr;	/*!< Peer address */
	void *state;	/*!< Used by consumers of */
} PeerAddrWithState;

/**
 * Peer init handler callback definition
 */
typedef bool (*peer_init_handler)( void **, void *, PeerAddr addr );
/**
 * Peer remove callback definition
 */
typedef bool (*peer_rm_handler)( void *, void * );

/**
 * Type peer vector
 */
typedef std::vector<PeerAddrWithState> PeerVector;

/**
 * Peer List interface
 */
class PeerList : public Lockable {
private:
	peer_init_handler init;
	void *handler_arg;
	peer_rm_handler rm;
	PeerVector internal_vector;
	bool ready;
public:
	typedef PeerVector::const_iterator const_iterator;	/*!< Peer constant iterator*/
	typedef PeerVector::iterator PeerVectorIt;			/*!< Peer vector iterator*/
	/**
	 * Initializes peer list
	 */
	PeerList() { rm = NULL; init = NULL; }
	/**
	 * @brief  Look for a peer address
	 * @param  addr Peer address
	 * @param  found [out] Set to true when address is found
	 * @return PeerVector internal
	 */
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
	/**
	 * @brief  Add a new peer address
	 * @param  addr PeerAddr address
	 * @return TRUE if successfully added. FALSE if it already exists
	 */
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
	/**
	 * @brief  Remove a peer address from the internal list
	 * @param  addr Address to remove
	 * @return FALSE if the address is not found. TRUE if successfully removed.
	 */
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
	/**
	 * @brief  Gets the beginning of the sequence container
	 * @return An iterator to the beginning of the sequence container
	 */
	const_iterator begin() {
		return internal_vector.begin();
	}
	/**
	 * @brief  Gets the end of the sequence container
	 * @return An iterator to the end of the sequence container
	 */
	const_iterator end() {
		return internal_vector.end();
	}
	/**
	 * @brief  Sets init handler
	 * @param  init Peer init handler
	 * @param  init_arg [in] Init arguments
	 * @return void
	 */
	void setInit( peer_init_handler init, void *init_arg ) {
		this->init = init;
		this->handler_arg = init_arg;
	}
	/**
	 * @brief  Sets peer remove callback on the PeerList object
	 * @param  rm rm handler
	 * @return void
	 */
	void setRm( peer_rm_handler rm ) {
		this->rm = rm;
	}
	/**
	 * @brief  Gets ready flag
	 * @return TRUE if ready. FALSE otherwise
	 */
	bool IsReady() { return ready; }
	/**
	 * @brief  Sets ready flag
	 * @param  ready Flag value to be set
	 * @return void
	 */
	void setReady( bool ready ) { this->ready = ready; }
};

#endif/*PEERLIST_HPP*/
