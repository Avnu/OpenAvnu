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

#include <wireless_tstamper.hpp>
#include <wireless_port.hpp>

net_result WirelessTimestamper::requestTimingMeasurement
( LinkLayerAddress *dest, uint16_t seq, WirelessDialog *prev_dialog,
  uint8_t *follow_up, int followup_length )
{
	WirelessDialog next_dialog;
	net_result retval;
	TIMINGMSMT_REQUEST *req;

	// Valid dialog token > 0 && < 256
	next_dialog.dialog_token = (seq % MAX_DIALOG_TOKEN) + 1;
	next_dialog.fwup_seq = seq;
	next_dialog.action_devclk = 0;
	req = (TIMINGMSMT_REQUEST *)follow_up;

	// File in request
	req->DialogToken = next_dialog.dialog_token;
	req->FollowUpDialogToken = prev_dialog->dialog_token;
	req->Category = 0x0;
	req->Action = 0x0;
	req->WiFiVSpecHdr.ElementId = FWUP_VDEF_TAG;
	req->WiFiVSpecHdr.Length = 0;
	if( req->FollowUpDialogToken != 0 && prev_dialog->action_devclk != 0 )
	{
		req->T1 = (uint32_t)prev_dialog->action_devclk;
		req->T4 = (uint32_t)prev_dialog->ack_devclk;
		req->WiFiVSpecHdr.Length = followup_length +
			(uint8_t)sizeof(FwUpLabel); // 1 byte type
		req->PtpSpec.FwUpLabel = FwUpLabel;
	}
	dest->toOctetArray(req->PeerMACAddress);

	retval = _requestTimingMeasurement( req );
	port->setPrevDialog(&next_dialog);

	return retval;
}

void WirelessTimestamper::timingMeasurementConfirmCB
( LinkLayerAddress addr, WirelessDialog *dialog )
{
	WirelessDialog *prev_dialog = port->getPrevDialog();

	// Translate action dev clock to Timestamp
	// ACK timestamp isn't needed
	dialog->action.set64(dialog->action_devclk*10);

	if (dialog->dialog_token == prev_dialog->dialog_token)
	{
		dialog->fwup_seq = prev_dialog->fwup_seq;
		port->setPrevDialog(dialog);
	}
}

void WirelessTimestamper::timeMeasurementIndicationCB
( LinkLayerAddress addr, WirelessDialog *current, WirelessDialog *previous,
  uint8_t *buf, size_t buflen )
{
	uint64_t link_delay;
	WirelessDialog *prev_local;
	PTPMessageCommon *msg;
	PTPMessageFollowUp *fwup;
	// Translate devclk scalar to Timestamp

	msg = buildPTPMessage((char *)buf, (int)buflen, &addr, port);
	fwup = dynamic_cast<PTPMessageFollowUp *> (msg);
	current->action_devclk *= 10;
	current->ack_devclk *= 10;
	current->action.set64(current->action_devclk);
	prev_local = port->getPrevDialog();
	if ( previous->dialog_token == prev_local->dialog_token &&
	     fwup != NULL && previous->action_devclk != 0)
	{
		previous->action_devclk *= 10;
		previous->ack_devclk *= 10;
		unsigned round_trip = (unsigned)
			(previous->ack_devclk - previous->action_devclk);
		unsigned turn_around = (unsigned)
			(prev_local->ack_devclk - prev_local->action_devclk);
		link_delay = (round_trip - turn_around) / 2;
		port->setLinkDelay(link_delay);
		GPTP_LOG_VERBOSE( "Link Delay = %llu(RT=%u,TA=%u,"
				  "T4=%llu,T1=%llu,DT=%hhu)",
				  link_delay, round_trip, turn_around,
				  previous->ack_devclk,
				  previous->action_devclk,
				  previous->dialog_token);
		fwup->processMessage(port, prev_local->action);
	}

	port->setPrevDialog(current);
}
