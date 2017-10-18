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

#include <wireless_port.hpp>
#include <wireless_tstamper.hpp>
#include <avbts_clock.hpp>

WirelessPort::~WirelessPort()
{
	// Intentionally left blank
}

WirelessPort::WirelessPort( PortInit_t *portInit, LinkLayerAddress peer_addr )
: CommonPort( portInit )
{
	this->peer_addr = peer_addr;

	setAsCapable( true );
	if ( getInitSyncInterval() == LOG2_INTERVAL_INVALID )
		setInitSyncInterval (-3 );       // 125 ms
	if ( getInitPDelayInterval() == LOG2_INTERVAL_INVALID )
		setInitPDelayInterval( 127 );  // 1 second

	prev_dialog.dialog_token = 0;
}

bool WirelessPort::_init_port(void)
{
	port_ready_condition = condition_factory->createCondition();

	return true;
}

bool WirelessPort::_processEvent( Event e )
{
	bool ret;

	switch (e)
	{
	default:
		GPTP_LOG_ERROR
		("Unhandled event type in "
			"WirelessPort::processEvent(), %d", e);
		ret = false;
		break;

	case POWERUP:
	case INITIALIZE:
	{
		port_ready_condition->wait_prelock();

		if ( !linkOpen(_openPort, (void *)this ))
		{
			GPTP_LOG_ERROR( "Error creating port thread" );
			ret = false;
			break;
		}

		port_ready_condition->wait();

		ret = true;
		break;
	}

	case SYNC_INTERVAL_TIMEOUT_EXPIRES:
	{
		WirelessTimestamper *timestamper =
			dynamic_cast<WirelessTimestamper *>( _hw_timestamper );
		PTPMessageFollowUp *follow_up;
		PortIdentity dest_id;
		uint16_t seq;
		uint8_t buffer[128];
		size_t length;

		memset(buffer, 0, sizeof( buffer ));

		if( prev_dialog.dialog_token != 0 )
		{
			follow_up = new PTPMessageFollowUp( this );

			getPortIdentity(dest_id);
			follow_up->setPortIdentity(&dest_id);
			follow_up->setSequenceId(prev_dialog.fwup_seq);
			follow_up->setPreciseOriginTimestamp
				( prev_dialog.action );
			length = follow_up->buildMessage
				(this, buffer + timestamper->getFwUpOffset());

			delete follow_up;
		}

		seq = getNextSyncSequenceId();
		ret = timestamper->requestTimingMeasurement
			( &peer_addr, seq, &prev_dialog, buffer, (int)length )
			== net_succeed;

		ret = true;
		break;
	}

	case ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES:
	case SYNC_RECEIPT_TIMEOUT_EXPIRES:
		ret = false;
		break;

	case STATE_CHANGE_EVENT:
		ret = false;
		break;
	}

	return ret;
}

bool WirelessPort::openPort()
{
	port_ready_condition->signal();

	while (true)
	{
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);
		uint32_t link_speed;

		if(( rrecv = recv( &remote, buf, length, link_speed ))
		   == net_succeed )
		{
			processMessage
			((char *)buf, (int)length, &remote, link_speed );
		}
		else if( rrecv == net_fatal )
		{
			GPTP_LOG_ERROR( "read from network interface failed" );
			this->processEvent( FAULT_DETECTED );
			break;
		}
	}

	return false;
}

void WirelessPort::sendGeneralPort
(uint16_t etherType, uint8_t * buf, int len, MulticastType mcast_type,
	PortIdentity * destIdentity)
{
	net_result rtx = send( &peer_addr, etherType, buf, len, false);
	if( rtx != net_succeed )
		GPTP_LOG_ERROR( "sendGeneralPort(): failure" );

	return;
}

void WirelessPort::processMessage
(char *buf, int length, LinkLayerAddress *remote, uint32_t link_speed)
{
	GPTP_LOG_VERBOSE( "Processing network buffer" );

	PTPMessageCommon *msg =
		buildPTPMessage( buf, (int)length, remote, this );

	if( msg == NULL )
	{
		GPTP_LOG_ERROR( "Discarding invalid message" );
		return;
	}
	GPTP_LOG_VERBOSE( "Processing message" );

	if( !msg->isEvent( ))
	{
		msg->processMessage( this );
	} else
	{
		GPTP_LOG_ERROR( "Received event message on port "
				"incapable of processing" );
		msg->PTPMessageCommon::processMessage( this );
	}

	if (msg->garbage())
		delete msg;
}

void WirelessPort::becomeSlave(bool restart_syntonization)
{
	clock->deleteEventTimerLocked(this, ANNOUNCE_INTERVAL_TIMEOUT_EXPIRES);
	clock->deleteEventTimerLocked( this, SYNC_INTERVAL_TIMEOUT_EXPIRES );

	setPortState( PTP_SLAVE );

	GPTP_LOG_STATUS("Switching to Slave");
	if( restart_syntonization ) clock->newSyntonizationSetPoint();

	getClock()->updateFUPInfo();
}

void WirelessPort::becomeMaster(bool annc)
{
	setPortState( PTP_MASTER );
	// Stop announce receipt timeout timer
	clock->deleteEventTimerLocked( this, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES);

	// Stop sync receipt timeout timer
	stopSyncReceiptTimer();

	if (annc)
		startAnnounce();

	startSyncIntervalTimer( 16000000 );
	GPTP_LOG_STATUS( "Switching to Master" );

	clock->updateFUPInfo();
}
