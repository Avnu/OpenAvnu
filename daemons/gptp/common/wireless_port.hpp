/******************************************************************************

Copyright (c) 2009-2015, Intel Corporation
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

#ifndef WIRELESS_PORT_HPP
#define WIRELESS_PORT_HPP

#include <common_port.hpp>
#include <avbts_oscondition.hpp>

class WirelessDialog
{
public:
	Timestamp action;
	uint64_t action_devclk;
	Timestamp ack;
	uint64_t ack_devclk;
	uint8_t dialog_token;
	uint16_t fwup_seq;

	WirelessDialog( uint32_t action, uint32_t ack, uint8_t dialog_token )
	{
		this->action_devclk = action; this->ack_devclk = ack;
		this->dialog_token = dialog_token;
	}

	WirelessDialog() { dialog_token = 0; }

	WirelessDialog & operator=( const WirelessDialog & a )
	{
		if (this != &a)
		{
			this->ack = a.ack;
			this->ack_devclk = a.ack_devclk;
			this->action = a.action;
			this->action_devclk = a.action_devclk;
			this->dialog_token = a.dialog_token;
			this->fwup_seq = a.fwup_seq;
		}
		return *this;
	}
};

class WirelessPort : public CommonPort
{
private:
	OSCondition *port_ready_condition;
	LinkLayerAddress peer_addr;
	WirelessDialog prev_dialog;

public:
	WirelessPort( PortInit_t *portInit, LinkLayerAddress peer_addr );
	virtual ~WirelessPort();

	/**
	* @brief Media specific port initialization
	* @return true on success
	*/
	bool _init_port( void );

	/**
	* @brief Perform media specific event handling action
	* @return true if event is handled without errors
	*/
	bool _processEvent( Event e );

	/**
	* @brief Process message
	* @param buf [in] Pointer to the data buffer
	* @param length Size of the message
	* @param remote [in] source address of message
	* @param link_speed [in] for receive operation
	* @return void
	*/
	void processMessage
	(char *buf, int length, LinkLayerAddress *remote, uint32_t link_speed);

	/**
	* @brief Sends a general message to a port. No timestamps
	* @param buf [in] Pointer to the data buffer
	* @param len Size of the message
	* @param mcast_type Enumeration
	* MulticastType (pdelay, none or other). Depracated.
	* @param destIdentity Destination port identity
	* @return void
	*/
	void sendGeneralPort
	( uint16_t etherType, uint8_t * buf, int len, MulticastType mcast_type,
	  PortIdentity * destIdentity );

	/**
	* @brief Nothing required for wireless port
	*/
	void syncDone() {}

	/**
	* @brief  Switches port to a gPTP master
	* @param  annc If TRUE, starts announce event timer.
	* @return void
	*/
	void becomeMaster( bool annc );

	/**
	* @brief  Switches port to a gPTP slave.
	* @param  restart_syntonization if TRUE, restarts the syntonization
	* @return void
	*/
	void becomeSlave( bool restart_syntonization );

	/**
	* @brief Receives messages from the network interface
	* @return Its an infinite loop. Returns false in case of error.
	*/
	bool openPort();

	/**
	 * @brief Wraps open port method for argument to thread
	 * @param larg pointer to WirelessPort object
	 * @return thread exit code
	 */
	static OSThreadExitCode _openPort( void *larg )
	{
		WirelessPort *port = (decltype(port))larg;

		if (!port->openPort())
			return osthread_error;

		return osthread_ok;
	}

	/**
	* @brief Sets previous dialog
	* @param dialog new value of prev_dialog
	*/
	void setPrevDialog( WirelessDialog *dialog )
	{
		prev_dialog = *dialog;
	}

	/**
	* @brief Sets previous dialog
	* @return reference to prev_dialog
	*/
	WirelessDialog *getPrevDialog( void )
	{
		return &prev_dialog;
	}
};

#endif/*WIRELESS_PORT_HPP*/
