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

#ifndef WLTIMESTAMPER_HPP
#define WLTIMESTAMPER_HPP

#include <avbts_oslock.hpp>
#include <ptptypes.hpp>
#include <vector>
#include <list>
#include <stdint.h>
#include <avbts_osnet.hpp>
#include <timestamper.hpp>
#include <md_wireless.hpp>


class MediaDependentPort;
class InterfaceLabel;
class OSNetworkInterface;
class Timestamp;
class PortIdentity;
class OSThread;
class OSThreadFactory;
class OSTimerFactory;
class MediaDependentWirelessPort;

/* id identifies the timestamper 0 is reserved meaning no timestamper is 
   availabled */

class WirelessDialog {
public:
	Timestamp action;
	uint32_t action_devclk;
	Timestamp ack;
	uint32_t ack_devclk;
	uint8_t dialog_token;
	uint16_t fwup_seq;
	WirelessDialog(uint32_t action, uint32_t ack, uint8_t dialog_token) {
		this->action_devclk = action; this->ack_devclk = ack;
		this->dialog_token = dialog_token;
	}
	WirelessDialog() { dialog_token = 0; }
	WirelessDialog & operator=(const WirelessDialog & a) {
		if (this != &a) {
			this->ack = a.ack;
			this->ack_devclk = a.ack_devclk;
			this->action = a.action;
			this->action_devclk = a.action_devclk;
			this->dialog_token = a.dialog_token;
		}
		return *this;
	}

};

inline uint64_t _32to64(uint32_t *rollover, uint32_t *prev, uint32_t curr) {
	uint64_t ret;
	if (curr < *prev && (*prev - curr) >(UINT32_MAX >> 1)) {
		*rollover = *rollover + 1;
	}
	ret = *rollover;
	ret <<= 32;
	ret += curr;
	*prev = curr;
	return ret;
}


class WirelessTimestamper : public Timestamper {
private:
	IEEE1588Clock *clock;
	OSConditionFactory *condition_factory;
	InterfaceLabel *iface_label;
	class WirelessPortPair {
	public:
		WirelessPortPair
			(MediaIndependentPort *iport, MediaDependentWirelessPort *wl_port) {
			this->iport = iport;
			this->wl_port = wl_port;
		}
		MediaIndependentPort *iport;
		MediaDependentWirelessPort *wl_port;
	};
	typedef std::map<LinkLayerAddress, WirelessPortPair *> PeerMap;
	PeerMap peer_map;
	typedef PeerMap::iterator PeerMapIter;

	uint32_t last_count;
	uint32_t rollover;
protected:
	OSLock *glock;
public:
	virtual bool HWTimestamper_init
		(InterfaceLabel *iface_label, OSNetworkInterface *iface,
		OSLockFactory *lock_factory, OSThreadFactory *thread_factory,
		OSTimerFactory *timer_factory) {

		if (!initialized) {
			this->iface_label = iface_label;
		}

		Timestamper::HWTimestamper_init
			(iface_label, iface, lock_factory, thread_factory,
			timer_factory);

		return true;
	}

	virtual net_result _requestTimingMeasurement
		(LinkLayerAddress dest, uint8_t dialog_token, WirelessDialog *prev_dialog,
		uint8_t *follow_up, int followup_length) = 0;
	net_result requestTimingMeasurement
		(LinkLayerAddress dest, uint16_t seq, WirelessDialog prev_dialog,
		uint8_t *follow_up, int followup_length);

	WirelessTimestamper
		(OSConditionFactory *condition_factory) {
		this->condition_factory = condition_factory;
		last_count = 0;
		rollover = 0;
	}
	virtual ~WirelessTimestamper();
	void setClock(IEEE1588Clock *clock) { this->clock = clock; }

	bool addPeer(LinkLayerAddress addr);
	bool removePeer(LinkLayerAddress addr);

	void timingMeasurementConfirmCB(LinkLayerAddress addr, WirelessDialog *dialog);
	void timeMeasurementIndicationCB(LinkLayerAddress addr, WirelessDialog *current, WirelessDialog *previous, uint8_t *buf, size_t buflen);
};


#endif/*WLTIMESTAMPER_HPP*/
