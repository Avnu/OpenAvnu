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

#include <avbts_message.hpp>
#include <md_wireless.hpp>
#include <avbts_message.hpp>
#include <ieee1588.hpp>

LinkLayerAddress MediaDependentWirelessPort::multicast_dest(OTHER_MULTICAST);

typedef struct {
	MediaDependentWirelessPort *port;
	OSCondition *ready;
} OpenPortWrapperArg;


OSThreadExitCode openPortWirelessWrapper( void *arg_in )
{
	OSThreadExitCode ret = osthread_ok;
	OpenPortWrapperArg *arg = (OpenPortWrapperArg *) arg_in;

	if (!arg->port->_openPort( arg->ready )) {
		ret = osthread_error;
	}
	delete arg;
	return ret;
}

bool MediaDependentWirelessPort::openPort
( OSCondition *port_ready_condition )
{
	OpenPortWrapperArg *arg = new OpenPortWrapperArg;
	listening_thread = thread_factory->createThread();
	port_stop_condition = condition_factory->createCondition();
	arg->port = this;
	arg->ready = port_ready_condition;
	if(!listening_thread->start(openPortWirelessWrapper, (void *) arg)) {
		XPTPD_ERROR("Error creating port thread");
		return false;
	}
	return true;
}

bool MediaDependentWirelessPort::_openPort( OSCondition *port_ready_condition )
{
	bool ret = true;
	port_ready_condition->signal(); /* Check return value? */

	while ( !do_exit ) {
		PTPMessageCommon *msg;
		uint8_t buf[128];
		LinkLayerAddress remote;
		net_result rrecv;
		size_t length = sizeof(buf);
		bool event = false;
		Timestamp rx_timestamp;

		if ((rrecv = net_iface->nrecv(&remote, buf, length)) == net_succeed) {
			XPTPD_INFO("Processing network buffer");

			getClock()->timerq_lock();
			getClock()->lock();
			getPort()->lock();

			msg = buildPTPMessage
				((char *)buf, (int)length, &remote, this, &event );

			if( msg == NULL ) {
				XPTPD_ERROR( "Received ill-formed PTP message, discarding" );
				continue;
			}

			if( event ) {
				XPTPD_ERROR
					( "Received well-formed, but illegal PTP message, "
					  "discarding" );
				continue;
			}

			XPTPD_INFO("Processing message");
			msg->processMessage((MediaDependentPort *) this );
			if (msg->garbage()) {
				delete msg;
			}

			getPort()->unlock();
			getClock()->unlock();
			getClock()->timerq_unlock();
		} else if (rrecv == net_fatal) {
			XPTPD_ERROR("read from network interface failed");
			this->processEvent(FAULT_DETECTED);
			ret = false;
			break;
		}
	}

	port_stop_condition->signal();

	return ret;
}

MediaDependentWirelessPort::~MediaDependentWirelessPort() {
	delete prev_dialog;
}

void MediaDependentWirelessPort::setPrevDialog(WirelessDialog *dialog) {
	*prev_dialog = *dialog;
}

WirelessDialog *MediaDependentWirelessPort::getPrevDialog() {
	return prev_dialog;
}


MediaDependentWirelessPort::MediaDependentWirelessPort
( WirelessTimestamper * timestamper, InterfaceLabel * net_label,
  OSConditionFactory * condition_factory, OSThreadFactory * thread_factory,
  OSTimerFactory * timer_factory, OSLockFactory * lock_factory )
	: MediaDependentPort
( timer_factory, thread_factory, net_label, condition_factory, lock_factory )
{
	prev_dialog = new WirelessDialog;
	_hw_timestamper = timestamper;
	do_exit = false;
}

bool MediaDependentWirelessPort::init_port( MediaIndependentPort *port )
{
	if( !MediaDependentPort::init_port( port )) return false;

	if (!OSNetworkInterfaceFactory::buildInterface
	    (&net_iface, factory_name_t("default"), net_label, _hw_timestamper, &tm_peer_addr))
		return false;

	this->net_iface = net_iface;
	prev_dialog->dialog_token = 0;

	if( _hw_timestamper != NULL ) {
		if( !_hw_timestamper->HWTimestamper_init
			( net_label, net_iface, lock_factory, thread_factory,
			  timer_factory )) {
			XPTPD_ERROR
				( "Failed to initialize hardware timestamper, "
				  "falling back to software timestamping" );
			_hw_timestamper = NULL;
		}
	}

	return true;
}

net_result MediaDependentWirelessPort::sendGeneralMessage
( uint8_t * buf, size_t size )
{
	return net_iface->send( &multicast_dest, (uint8_t *) buf, size, false);
}

bool MediaDependentWirelessPort::processSync(uint16_t seq, bool grandmaster, long *elapsed_time) {
	WirelessTimestamper *timestamper = dynamic_cast<WirelessTimestamper *>(_hw_timestamper);
	if (timestamper != NULL) {
		uint8_t *buffer = NULL;
		size_t length = 0;
		if (prev_dialog->dialog_token != 0) {
			PortIdentity dest_id;
			Timestamp system_time;
			Timestamp device_time;
			clock_offset_t offset;
			uint32_t device_sync_time_offset;
			FrequencyRatio local_system_freqoffset;

			memset(&offset, 0, sizeof(offset));
			local_system_freqoffset = getLocalSystemRateOffset();
			XPTPD_INFOL(SYNC_DEBUG, "local-system frequency offset: %Lf",
				local_system_freqoffset);
			getDeviceTime(system_time, device_time);

			if (getPort()->getPortNumber() == 1) {
				offset.ml_freqoffset = 1.0;
				offset.ml_phoffset = 0;
				offset.sync_count = getPort()->getSyncCount();
				offset.pdelay_count = getPort()->getPdelayCount();
				if (grandmaster) {
					offset.master_time = TIMESTAMP_TO_NS(system_time);
					offset.ls_phoffset = 0;
					offset.ls_freqoffset = 1.0;
				}
				else {
					offset.master_time = TIMESTAMP_TO_NS(device_time);
					offset.ls_phoffset = TIMESTAMP_TO_NS(system_time - device_time);
					offset.ls_freqoffset = local_system_freqoffset;
				}
				
				getPort()->getClock()->setMasterOffset(&offset);
				//dumpOffset(stderr, &offset);
			}

			device_sync_time_offset = TIMESTAMP_TO_NS(device_time - prev_dialog->action);
			// Calculate sync timestamp in terms of system clock
			system_time = (uint64_t)
				(((FrequencyRatio)device_sync_time_offset) /
					local_system_freqoffset);

			PTPMessageFollowUp *follow_up;
			follow_up = new PTPMessageFollowUp(this);
			port->getPortIdentity(dest_id);
			follow_up->setPortIdentity(&dest_id);
			follow_up->setSequenceId(prev_dialog->fwup_seq);
			if (!grandmaster) {
				uint64_t correction_field;
				unsigned residence_time;
				follow_up->setPreciseOriginTimestamp
					(getClock()->getLastSyncOriginTime());
				// Calculate residence time
				if (getClock()->getLastSyncReceiveDeviceId() ==
					getTimestampDeviceId()) {
					// Same timestamp source
					residence_time =
						TIMESTAMP_TO_NS
						((prev_dialog->action) -
						(getClock()->getLastSyncReceiveDeviceTime()));
					residence_time = (unsigned)(((FrequencyRatio)residence_time)*
						getClock()->getLastSyncCumulativeOffset());
					follow_up->setRateOffset
						(getClock()->getLastSyncCumulativeOffset());
				}
				else {
					// Different timestamp source
					residence_time =
						TIMESTAMP_TO_NS
						(system_time -
						(getClock()->getLastSyncReceiveSystemTime()));
					residence_time = (unsigned)(((FrequencyRatio)residence_time)*
						getClock()->getMasterSystemFrequencyOffset());
					follow_up->setRateOffset
						(getClock()->getMasterSystemFrequencyOffset() /
						offset.ls_freqoffset);
				}
				// Add to correction field
				correction_field = getClock()->getLastSyncCorrection();
				correction_field += ((uint64_t)residence_time) << 16;
				follow_up->setCorrectionField(correction_field);
			}
			else {
				follow_up->setPreciseOriginTimestamp(system_time);
			}
			// Copy the followup into buffer
			buffer = new uint8_t[128];
			length = follow_up->buildMessage(buffer);
			delete follow_up;
		}
		// Request Timing Measurement
		unlock();
		// Make sure we're unlocked here.  If the underlying operation is blocking, the callback will likely occur before 'request'
		// call returns
		timestamper->requestTimingMeasurement(tm_peer_addr, seq, *prev_dialog, buffer, (int)length);
		lock();
		if (buffer != NULL) delete[] buffer;
		return true;
	}
	return false;
}

