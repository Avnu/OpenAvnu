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

#include <wltimestamper.hpp>
#include <md_wireless.hpp>
#include <min_port.hpp>
#include <avbts_message.hpp>
#include <Windows.h>

bool WirelessTimestamper::addPeer   ( LinkLayerAddress addr ) {
	// Create Media Independent Port
	bool ret = true;
	MediaIndependentPort *min_port =
		new MediaIndependentPort
		(clock, 0, false, condition_factory,
		thread_factory, timer_factory, lock_factory);
	if (min_port == NULL) { ret = false; goto done; }

	// Create Media Dependent Wireless Port
	MediaDependentWirelessPort *md_port = new MediaDependentWirelessPort
		(this, iface_label, condition_factory, thread_factory,
		timer_factory, lock_factory);
	if (md_port == NULL) { ret = false; goto done; }

	md_port->setPeerAddr(&addr);

	min_port->setPort(md_port);

	if (!min_port->init_port()) {
		printf("failed to initialize port \n");
		ret = false;
		goto done;
	}

	// Add the port to the list
	lock();
	peer_map[addr] = new WirelessPortPair(min_port, md_port);
	unlock();

	// Start the port
	md_port->getClock()->timerq_lock();
	md_port->getClock()->lock();
	ret = min_port->processEvent(POWERUP);
	if(ret) min_port->setAsCapable();  // Adding a peer implies that he is asCapable
	md_port->getClock()->unlock();
	md_port->getClock()->timerq_unlock();
	if (!ret) goto done;

	done:
	return true;
}

bool WirelessTimestamper::removePeer( LinkLayerAddress addr ) {
	PeerMapIter iter;

	lock();
	iter = peer_map.find(addr);
	if (iter != peer_map.end()) {
		(*iter).second->iport->stop();
		(*iter).second->wl_port->stop();
		delete iter->second->iport;
		delete iter->second->wl_port;
		delete iter->second;
		peer_map.erase(iter);
		return true;
	}
	unlock();

	return false;
}

net_result WirelessTimestamper::requestTimingMeasurement
(LinkLayerAddress dest, uint16_t seq, WirelessDialog prev_dialog,
uint8_t *follow_up, int followup_length) {
	uint8_t dialog_token = (seq % 255) + 1;
	WirelessDialog next_dialog;
	next_dialog.dialog_token = dialog_token;
	next_dialog.fwup_seq = seq;
	next_dialog.action_devclk = 0;
	lock();
	if (peer_map.count(dest) != 0) {
		peer_map[dest]->wl_port->lock();
		peer_map[dest]->wl_port->setPrevDialog(&next_dialog);
		peer_map[dest]->wl_port->unlock();
	}
	unlock();
	return _requestTimingMeasurement(dest, dialog_token, &prev_dialog, follow_up, followup_length);
}

void WirelessTimestamper::timingMeasurementConfirmCB(LinkLayerAddress addr, WirelessDialog *dialog) {
	// Translate devclk scalar to Timestamp
	lock();
	dialog->action = NS_TO_TIMESTAMP(_32to64(&rollover,&last_count,dialog->action_devclk*10));
	dialog->ack = NS_TO_TIMESTAMP(_32to64(&rollover, &last_count, dialog->ack_devclk*10));
	peer_map[addr]->wl_port->lock();
	WirelessDialog prev_dialog = *peer_map[addr]->wl_port->getPrevDialog();
	SYSTEMTIME systime;
	GetSystemTime(&systime);
	//printf("timingMeasurementConfirmCB:%hu:%hu:%hu.%03hu\n", systime.wHour, systime.wMinute, systime.wSecond, systime.wMilliseconds);
	if (dialog->dialog_token == prev_dialog.dialog_token) {
		dialog->fwup_seq = prev_dialog.fwup_seq;
		//printf("Got confirmation for: %hhu(%u)\n", dialog->dialog_token, dialog->action_devclk);
		peer_map[addr]->wl_port->setPrevDialog(dialog);	
	}
	peer_map[addr]->wl_port->unlock();
	unlock();
}


void WirelessTimestamper::timeMeasurementIndicationCB(LinkLayerAddress addr, WirelessDialog *current, WirelessDialog *previous, uint8_t *buf, size_t buflen) {
	uint64_t link_delay;
	WirelessDialog *prev_local;
	bool event_flag;
	PTPMessageCommon *msg;
	PTPMessageFollowUp *fwup;
	// Translate devclk scalar to Timestamp
	lock();
	if (peer_map.count(addr) != 0) {
		msg = buildPTPMessage((char *)buf, (int)buflen, &addr, peer_map[addr]->wl_port, &event_flag);
		fwup = dynamic_cast<PTPMessageFollowUp *>(msg);
		current->action = NS_TO_TIMESTAMP(_32to64(&rollover, &last_count, current->action_devclk*10));
		current->ack = NS_TO_TIMESTAMP(_32to64(&rollover, &last_count, current->ack_devclk*10));
		peer_map[addr]->wl_port->lock();
		prev_local = peer_map[addr]->wl_port->getPrevDialog();
		printf("Got Indication: T1=%u,T2=%u,T3=%u,T4=%u\n", prev_local->action_devclk, previous->action_devclk,
			previous->ack_devclk, prev_local->ack_devclk);
		if (previous->dialog_token == prev_local->dialog_token && fwup != NULL && !event_flag) {
			previous->action = NS_TO_TIMESTAMP(_32to64(&rollover, &last_count, previous->action_devclk*10));
			previous->ack = NS_TO_TIMESTAMP(_32to64(&rollover, &last_count, previous->ack_devclk*10));
			link_delay  = TIMESTAMP_TO_NS(previous->ack - previous->action) - TIMESTAMP_TO_NS(prev_local->ack - prev_local->action);
			link_delay /= 2;
			peer_map[addr]->wl_port->setLinkDelay(link_delay);
			printf("Link Delay = %llu\n", link_delay);
			fwup->processMessage(peer_map[addr]->wl_port, prev_local->action, link_delay);
		}
		peer_map[addr]->wl_port->setPrevDialog(current);
		peer_map[addr]->wl_port->unlock();
	}
	unlock();
}

WirelessTimestamper::~WirelessTimestamper() {
	PeerMapIter iter;
	lock();
	for (iter = peer_map.begin(); iter != peer_map.end(); ++iter) {
		removePeer(iter->first);
	}
}
