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

#include <ieee1588.hpp>
#include <avbts_clock.hpp>
#include <avbts_message.hpp>
#include <ether_port.hpp>
#include <avbts_ostimer.hpp>

#include <stdio.h>
#include <string.h>
#include <math.h>

PTPMessageCommon::PTPMessageCommon( CommonPort *port )
{
	// Fill in fields using port/clock dataset as a template
	versionPTP = GPTP_VERSION;
	versionNetwork = PTP_NETWORK_VERSION;
	domainNumber = port->getClock()->getDomain();
	// Set flags as necessary
	memset(flags, 0, PTP_FLAGS_LENGTH);
	flags[PTP_PTPTIMESCALE_BYTE] |= (0x1 << PTP_PTPTIMESCALE_BIT);
	correctionField = 0;
	_gc = false;
	sourcePortIdentity = new PortIdentity();

	return;
}

/* Determine whether the message was sent by given communication technology,
   uuid, and port id fields */
bool PTPMessageCommon::isSenderEqual(PortIdentity portIdentity)
{
	return portIdentity == *sourcePortIdentity;
}

PTPMessageCommon *buildPTPMessage
( char *buf, int size, LinkLayerAddress *remote,
  EtherPort *port )
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	PTPMessageCommon *msg = NULL;
	PTPMessageId messageId;
	MessageType messageType;
	unsigned char tspec_msg_t = 0;
	unsigned char transportSpecific = 0;

	uint16_t sequenceId;
	PortIdentity *sourcePortIdentity;
	Timestamp timestamp(0, 0, 0);
	unsigned counter_value = 0;

#if PTP_DEBUG
	{
		int i;
		GPTP_LOG_VERBOSE("Packet Dump:\n");
		for (i = 0; i < size; ++i) {
			GPTP_LOG_VERBOSE("%hhx\t", buf[i]);
			if (i % 8 == 7)
				GPTP_LOG_VERBOSE("\n");
		}
		if (i % 8 != 0)
			GPTP_LOG_VERBOSE("\n");
	}
#endif

	memcpy(&tspec_msg_t,
	       buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
	       sizeof(tspec_msg_t));
	messageType = (MessageType) (tspec_msg_t & 0xF);
	transportSpecific = (tspec_msg_t >> 4) & 0x0F;

	sourcePortIdentity = new PortIdentity
		((uint8_t *)
		 (buf + PTP_COMMON_HDR_SOURCE_CLOCK_ID
		  (PTP_COMMON_HDR_OFFSET)),
		 (uint16_t *)
		 (buf + PTP_COMMON_HDR_SOURCE_PORT_ID
		  (PTP_COMMON_HDR_OFFSET)));

	memcpy
		(&(sequenceId),
		 buf + PTP_COMMON_HDR_SEQUENCE_ID(PTP_COMMON_HDR_OFFSET),
		 sizeof(sequenceId));
	sequenceId = PLAT_ntohs(sequenceId);

	GPTP_LOG_VERBOSE("Captured Sequence Id: %u", sequenceId);
	messageId.setMessageType(messageType);
	messageId.setSequenceId(sequenceId);


	if (!(messageType >> 3)) {
		int iter = 5;
		long req = 4000;	// = 1 ms
		int ts_good =
		    port->getRxTimestamp
			(sourcePortIdentity, messageId, timestamp, counter_value, false);
		while (ts_good != GPTP_EC_SUCCESS && iter-- != 0) {
			// Waits at least 1 time slice regardless of size of 'req'
			timer->sleep(req);
			if (ts_good != GPTP_EC_EAGAIN)
				GPTP_LOG_ERROR(
					"Error (RX) timestamping RX event packet (Retrying), error=%d",
					  ts_good );
			ts_good =
			    port->getRxTimestamp(sourcePortIdentity, messageId,
						 timestamp, counter_value,
						 iter == 0);
			req *= 2;
		}
		if (ts_good != GPTP_EC_SUCCESS) {
			char msg[HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
			port->getExtendedError(msg);
			GPTP_LOG_ERROR
			    ("*** Received an event packet but cannot retrieve timestamp, discarding. messageType=%u,error=%d\n%s",
			     messageType, ts_good, msg);
			//_exit(-1);
			goto abort;
		}

		else {
			GPTP_LOG_VERBOSE("Timestamping event packet");
		}

	}

	if (1 != transportSpecific) {
		GPTP_LOG_EXCEPTION("*** Received message with unsupported transportSpecific type=%d", transportSpecific);
		goto abort;
	}
 
	switch (messageType) {
	case SYNC_MESSAGE:

		GPTP_LOG_DEBUG("*** Received Sync message" );
		GPTP_LOG_VERBOSE("Sync RX timestamp = %hu,%u,%u", timestamp.seconds_ms, timestamp.seconds_ls, timestamp.nanoseconds );

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_SYNC_LENGTH) {
			goto abort;
		}
		{
			PTPMessageSync *sync_msg = new PTPMessageSync();
			sync_msg->messageType = messageType;
			// Copy in v2 sync specific fields
			memcpy(&(sync_msg->originTimestamp.seconds_ms),
			       buf + PTP_SYNC_SEC_MS(PTP_SYNC_OFFSET),
			       sizeof(sync_msg->originTimestamp.seconds_ms));
			memcpy(&(sync_msg->originTimestamp.seconds_ls),
			       buf + PTP_SYNC_SEC_LS(PTP_SYNC_OFFSET),
			       sizeof(sync_msg->originTimestamp.seconds_ls));
			memcpy(&(sync_msg->originTimestamp.nanoseconds),
			       buf + PTP_SYNC_NSEC(PTP_SYNC_OFFSET),
			       sizeof(sync_msg->originTimestamp.nanoseconds));
			msg = sync_msg;
		}
		break;
	case FOLLOWUP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received Follow Up message");

		// Be sure buffer is the correction size
		if (size < (int)(PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + sizeof(FollowUpTLV))) {
			goto abort;
		}
		{
			PTPMessageFollowUp *followup_msg =
			    new PTPMessageFollowUp();
			followup_msg->messageType = messageType;
			// Copy in v2 sync specific fields
			memcpy(&
			       (followup_msg->
				preciseOriginTimestamp.seconds_ms),
			       buf + PTP_FOLLOWUP_SEC_MS(PTP_FOLLOWUP_OFFSET),
			       sizeof(followup_msg->
				      preciseOriginTimestamp.seconds_ms));
			memcpy(&
			       (followup_msg->
				preciseOriginTimestamp.seconds_ls),
			       buf + PTP_FOLLOWUP_SEC_LS(PTP_FOLLOWUP_OFFSET),
			       sizeof(followup_msg->
				      preciseOriginTimestamp.seconds_ls));
			memcpy(&
			       (followup_msg->
				preciseOriginTimestamp.nanoseconds),
			       buf + PTP_FOLLOWUP_NSEC(PTP_FOLLOWUP_OFFSET),
			       sizeof(followup_msg->
				      preciseOriginTimestamp.nanoseconds));

			followup_msg->preciseOriginTimestamp.seconds_ms =
			    PLAT_ntohs(followup_msg->
				       preciseOriginTimestamp.seconds_ms);
			followup_msg->preciseOriginTimestamp.seconds_ls =
			    PLAT_ntohl(followup_msg->
				       preciseOriginTimestamp.seconds_ls);
			followup_msg->preciseOriginTimestamp.nanoseconds =
			    PLAT_ntohl(followup_msg->
				       preciseOriginTimestamp.nanoseconds);

			memcpy( &(followup_msg->tlv),
				buf+PTP_FOLLOWUP_OFFSET+PTP_FOLLOWUP_LENGTH,
				sizeof(followup_msg->tlv) );

			msg = followup_msg;
        }

		break;
	case PATH_DELAY_REQ_MESSAGE:

		GPTP_LOG_DEBUG("*** Received PDelay Request message");

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH
		    && /* For Broadcom compatibility */ size != 46) {
			goto abort;
		}
		{
			PTPMessagePathDelayReq *pdelay_req_msg =
			    new PTPMessagePathDelayReq();
			pdelay_req_msg->messageType = messageType;

#if 0
            /*TODO: Do we need the code below? Can we remove it?*/
			// The origin timestamp for PDelay Request packets has been eliminated since it is unused
			// Copy in v2 PDelay Request specific fields
			memcpy(&(pdelay_req_msg->originTimestamp.seconds_ms),
			       buf +
			       PTP_PDELAY_REQ_SEC_MS(PTP_PDELAY_REQ_OFFSET),
			       sizeof(pdelay_req_msg->
				      originTimestamp.seconds_ms));
			memcpy(&(pdelay_req_msg->originTimestamp.seconds_ls),
			       buf +
			       PTP_PDELAY_REQ_SEC_LS(PTP_PDELAY_REQ_OFFSET),
			       sizeof(pdelay_req_msg->
				      originTimestamp.seconds_ls));
			memcpy(&(pdelay_req_msg->originTimestamp.nanoseconds),
			       buf + PTP_PDELAY_REQ_NSEC(PTP_PDELAY_REQ_OFFSET),
			       sizeof(pdelay_req_msg->
				      originTimestamp.nanoseconds));

			pdelay_req_msg->originTimestamp.seconds_ms =
			    PLAT_ntohs(pdelay_req_msg->
				       originTimestamp.seconds_ms);
			pdelay_req_msg->originTimestamp.seconds_ls =
			    PLAT_ntohl(pdelay_req_msg->
				       originTimestamp.seconds_ls);
			pdelay_req_msg->originTimestamp.nanoseconds =
			    PLAT_ntohl(pdelay_req_msg->
				       originTimestamp.nanoseconds);
#endif

			msg = pdelay_req_msg;
		}
		break;
	case PATH_DELAY_RESP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received PDelay Response message, Timestamp %u (sec) %u (ns), seqID %u",
			   timestamp.seconds_ls, timestamp.nanoseconds,
			   sequenceId);

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH) {
			goto abort;
		}
		{
			PTPMessagePathDelayResp *pdelay_resp_msg =
			    new PTPMessagePathDelayResp();
			pdelay_resp_msg->messageType = messageType;
			// Copy in v2 PDelay Response specific fields
			pdelay_resp_msg->requestingPortIdentity =
			    new PortIdentity((uint8_t *) buf +
					     PTP_PDELAY_RESP_REQ_CLOCK_ID
					     (PTP_PDELAY_RESP_OFFSET),
					     (uint16_t *) (buf +
							   PTP_PDELAY_RESP_REQ_PORT_ID
							   (PTP_PDELAY_RESP_OFFSET)));

#ifdef DEBUG
			for (int n = 0; n < PTP_CLOCK_IDENTITY_LENGTH; ++n) {	// MMM
				GPTP_LOG_VERBOSE("%c",
					pdelay_resp_msg->
					requestingPortIdentity.clockIdentity
					[n]);
			}
#endif

			memcpy(& (pdelay_resp_msg->requestReceiptTimestamp.seconds_ms),
			       buf + PTP_PDELAY_RESP_SEC_MS(PTP_PDELAY_RESP_OFFSET),
			       sizeof
				   (pdelay_resp_msg->requestReceiptTimestamp.seconds_ms));
			memcpy(&
			       (pdelay_resp_msg->
				requestReceiptTimestamp.seconds_ls),
			       buf +
			       PTP_PDELAY_RESP_SEC_LS(PTP_PDELAY_RESP_OFFSET),
			       sizeof(pdelay_resp_msg->
				      requestReceiptTimestamp.seconds_ls));
			memcpy(&
			       (pdelay_resp_msg->
				requestReceiptTimestamp.nanoseconds),
			       buf +
			       PTP_PDELAY_RESP_NSEC(PTP_PDELAY_RESP_OFFSET),
			       sizeof(pdelay_resp_msg->
				      requestReceiptTimestamp.nanoseconds));

			pdelay_resp_msg->requestReceiptTimestamp.seconds_ms =
			    PLAT_ntohs(pdelay_resp_msg->requestReceiptTimestamp.seconds_ms);
			pdelay_resp_msg->requestReceiptTimestamp.seconds_ls =
			    PLAT_ntohl(pdelay_resp_msg->requestReceiptTimestamp.seconds_ls);
			pdelay_resp_msg->requestReceiptTimestamp.nanoseconds =
			    PLAT_ntohl(pdelay_resp_msg->requestReceiptTimestamp.nanoseconds);

			msg = pdelay_resp_msg;
		}
		break;
	case PATH_DELAY_FOLLOWUP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received PDelay Response FollowUp message");

		// Be sure buffer is the correction size
//     if( size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_FOLLOWUP_LENGTH ) {
//       goto abort;
//     }
		{
			PTPMessagePathDelayRespFollowUp *pdelay_resp_fwup_msg =
			    new PTPMessagePathDelayRespFollowUp();
			pdelay_resp_fwup_msg->messageType = messageType;
			// Copy in v2 PDelay Response specific fields
			pdelay_resp_fwup_msg->requestingPortIdentity =
			    new PortIdentity((uint8_t *) buf +
					     PTP_PDELAY_FOLLOWUP_REQ_CLOCK_ID
					     (PTP_PDELAY_RESP_OFFSET),
					     (uint16_t *) (buf +
							   PTP_PDELAY_FOLLOWUP_REQ_PORT_ID
							   (PTP_PDELAY_FOLLOWUP_OFFSET)));

			memcpy(&
			       (pdelay_resp_fwup_msg->
				responseOriginTimestamp.seconds_ms),
			       buf +
			       PTP_PDELAY_FOLLOWUP_SEC_MS
			       (PTP_PDELAY_FOLLOWUP_OFFSET),
			       sizeof
			       (pdelay_resp_fwup_msg->responseOriginTimestamp.
				seconds_ms));
			memcpy(&
			       (pdelay_resp_fwup_msg->
				responseOriginTimestamp.seconds_ls),
			       buf +
			       PTP_PDELAY_FOLLOWUP_SEC_LS
			       (PTP_PDELAY_FOLLOWUP_OFFSET),
			       sizeof
			       (pdelay_resp_fwup_msg->responseOriginTimestamp.
				seconds_ls));
			memcpy(&
			       (pdelay_resp_fwup_msg->
				responseOriginTimestamp.nanoseconds),
			       buf +
			       PTP_PDELAY_FOLLOWUP_NSEC
			       (PTP_PDELAY_FOLLOWUP_OFFSET),
			       sizeof
			       (pdelay_resp_fwup_msg->responseOriginTimestamp.
				nanoseconds));

			pdelay_resp_fwup_msg->
			    responseOriginTimestamp.seconds_ms =
			    PLAT_ntohs
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.
			     seconds_ms);
			pdelay_resp_fwup_msg->
			    responseOriginTimestamp.seconds_ls =
			    PLAT_ntohl
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.
			     seconds_ls);
			pdelay_resp_fwup_msg->
			    responseOriginTimestamp.nanoseconds =
			    PLAT_ntohl
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.
			     nanoseconds);

			msg = pdelay_resp_fwup_msg;
		}
		break;
	case ANNOUNCE_MESSAGE:

		GPTP_LOG_VERBOSE("*** Received Announce message");

		{
			PTPMessageAnnounce *annc = new PTPMessageAnnounce();
			annc->messageType = messageType;
			int tlv_length = size - PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH;

			memcpy(&(annc->currentUtcOffset),
			       buf +
			       PTP_ANNOUNCE_CURRENT_UTC_OFFSET
			       (PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->currentUtcOffset));
			annc->currentUtcOffset =
			    PLAT_ntohs(annc->currentUtcOffset);
			memcpy(&(annc->grandmasterPriority1),
			       buf +
			       PTP_ANNOUNCE_GRANDMASTER_PRIORITY1
			       (PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->grandmasterPriority1));
			memcpy( annc->grandmasterClockQuality,
				buf+
				PTP_ANNOUNCE_GRANDMASTER_CLOCK_QUALITY
				(PTP_ANNOUNCE_OFFSET),
				sizeof( *annc->grandmasterClockQuality ));
			annc->
			  grandmasterClockQuality->offsetScaledLogVariance =
			  PLAT_ntohs
			  ( annc->grandmasterClockQuality->
			    offsetScaledLogVariance );
			memcpy(&(annc->grandmasterPriority2),
			       buf +
			       PTP_ANNOUNCE_GRANDMASTER_PRIORITY2
			       (PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->grandmasterPriority2));
			memcpy(&(annc->grandmasterIdentity),
			       buf +
			       PTP_ANNOUNCE_GRANDMASTER_IDENTITY
			       (PTP_ANNOUNCE_OFFSET),
			       PTP_CLOCK_IDENTITY_LENGTH);
			memcpy(&(annc->stepsRemoved),
			       buf +
			       PTP_ANNOUNCE_STEPS_REMOVED(PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->stepsRemoved));
			annc->stepsRemoved = PLAT_ntohs(annc->stepsRemoved);
			memcpy(&(annc->timeSource),
			       buf +
			       PTP_ANNOUNCE_TIME_SOURCE(PTP_ANNOUNCE_OFFSET),
			       sizeof(annc->timeSource));

			// Parse TLV if it exists
			buf += PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH;
			if( tlv_length > (int) (2*sizeof(uint16_t)) && PLAT_ntohs(*((uint16_t *)buf)) == PATH_TRACE_TLV_TYPE)  {
				buf += sizeof(uint16_t);
				tlv_length -= sizeof(uint16_t);
				annc->tlv.parseClockIdentity((uint8_t *)buf, tlv_length);
			}

			msg = annc;
		}
		break;

	case SIGNALLING_MESSAGE:
		{
			PTPMessageSignalling *signallingMsg = new PTPMessageSignalling();
			signallingMsg->messageType = messageType;

			memcpy(&(signallingMsg->targetPortIdentify),
			       buf + PTP_SIGNALLING_TARGET_PORT_IDENTITY(PTP_SIGNALLING_OFFSET),
			       sizeof(signallingMsg->targetPortIdentify));

			memcpy( &(signallingMsg->tlv), buf + PTP_SIGNALLING_OFFSET + PTP_SIGNALLING_LENGTH, sizeof(signallingMsg->tlv) );

			msg = signallingMsg;
		}
		break;

	default:

		GPTP_LOG_EXCEPTION("Received unsupported message type, %d",
		            (int)messageType);
		port->incCounter_ieee8021AsPortStatRxPTPPacketDiscard();

		goto abort;
	}

	msg->_gc = false;

	// Copy in common header fields
	memcpy(&(msg->versionPTP),
	       buf + PTP_COMMON_HDR_PTP_VERSION(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->versionPTP));
	memcpy(&(msg->messageLength),
	       buf + PTP_COMMON_HDR_MSG_LENGTH(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->messageLength));
	msg->messageLength = PLAT_ntohs(msg->messageLength);
	memcpy(&(msg->domainNumber),
	       buf + PTP_COMMON_HDR_DOMAIN_NUMBER(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->domainNumber));
	memcpy(&(msg->flags), buf + PTP_COMMON_HDR_FLAGS(PTP_COMMON_HDR_OFFSET),
	       PTP_FLAGS_LENGTH);
	memcpy(&(msg->correctionField),
	       buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->correctionField));
	msg->correctionField = PLAT_ntohll(msg->correctionField);
	msg->sourcePortIdentity = sourcePortIdentity;
	msg->sequenceId = sequenceId;
	memcpy(&(msg->control),
	       buf + PTP_COMMON_HDR_CONTROL(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->control));
	memcpy(&(msg->logMeanMessageInterval),
	       buf + PTP_COMMON_HDR_LOG_MSG_INTRVL(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->logMeanMessageInterval));

	port->addSockAddrMap(msg->sourcePortIdentity, remote);

	msg->_timestamp = timestamp;
	msg->_timestamp_counter_value = counter_value;

	delete timer;

	return msg;

abort:
	delete sourcePortIdentity;
	delete timer;

	return NULL;
}

void PTPMessageCommon::processMessage( EtherPort *port )
{
	_gc = true;
	return;
}

void PTPMessageCommon::buildCommonHeader(uint8_t * buf)
{
	unsigned char tspec_msg_t;
    /*TODO: Message type assumes value sbetween 0x0 and 0xD (its an enumeration).
     * So I am not sure why we are adding 0x10 to it
     */
	tspec_msg_t = messageType | 0x10;
	long long correctionField_BE = PLAT_htonll(correctionField);
	uint16_t messageLength_NO = PLAT_htons(messageLength);

	memcpy(buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
	       &tspec_msg_t, sizeof(tspec_msg_t));
	memcpy(buf + PTP_COMMON_HDR_PTP_VERSION(PTP_COMMON_HDR_OFFSET),
	       &versionPTP, sizeof(versionPTP));
	memcpy(buf + PTP_COMMON_HDR_MSG_LENGTH(PTP_COMMON_HDR_OFFSET),
	       &messageLength_NO, sizeof(messageLength_NO));
	memcpy(buf + PTP_COMMON_HDR_DOMAIN_NUMBER(PTP_COMMON_HDR_OFFSET),
	       &domainNumber, sizeof(domainNumber));
	memcpy(buf + PTP_COMMON_HDR_FLAGS(PTP_COMMON_HDR_OFFSET), &flags,
	       PTP_FLAGS_LENGTH);
	memcpy(buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       &correctionField_BE, sizeof(correctionField));

	sourcePortIdentity->getClockIdentityString
	  ((uint8_t *) buf+
	   PTP_COMMON_HDR_SOURCE_CLOCK_ID(PTP_COMMON_HDR_OFFSET));
	sourcePortIdentity->getPortNumberNO
	  ((uint16_t *) (buf + PTP_COMMON_HDR_SOURCE_PORT_ID
			 (PTP_COMMON_HDR_OFFSET)));

	GPTP_LOG_VERBOSE("Sending Sequence Id: %u", sequenceId);
	sequenceId = PLAT_htons(sequenceId);
	memcpy(buf + PTP_COMMON_HDR_SEQUENCE_ID(PTP_COMMON_HDR_OFFSET),
	       &sequenceId, sizeof(sequenceId));
	sequenceId = PLAT_ntohs(sequenceId);
	memcpy(buf + PTP_COMMON_HDR_CONTROL(PTP_COMMON_HDR_OFFSET), &control,
	       sizeof(control));
	memcpy(buf + PTP_COMMON_HDR_LOG_MSG_INTRVL(PTP_COMMON_HDR_OFFSET),
	       &logMeanMessageInterval, sizeof(logMeanMessageInterval));

	return;
}

void PTPMessageCommon::getPortIdentity(PortIdentity * identity)
{
	*identity = *sourcePortIdentity;
}

void PTPMessageCommon::setPortIdentity(PortIdentity * identity)
{
	*sourcePortIdentity = *identity;
}

PTPMessageCommon::~PTPMessageCommon(void)
{
	delete sourcePortIdentity;
	return;
}

PTPMessageAnnounce::PTPMessageAnnounce(void)
{
	grandmasterClockQuality = new ClockQuality();
}

PTPMessageAnnounce::~PTPMessageAnnounce(void)
{
	delete grandmasterClockQuality;
}

bool PTPMessageAnnounce::isBetterThan(PTPMessageAnnounce * msg)
{
	unsigned char this1[14];
	unsigned char that1[14];
	uint16_t tmp;

	this1[0] = grandmasterPriority1;
	that1[0] = msg->getGrandmasterPriority1();

	this1[1] = grandmasterClockQuality->cq_class;
	that1[1] = msg->getGrandmasterClockQuality()->cq_class;

	this1[2] = grandmasterClockQuality->clockAccuracy;
	that1[2] = msg->getGrandmasterClockQuality()->clockAccuracy;

	tmp = grandmasterClockQuality->offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(this1 + 3, &tmp, sizeof(tmp));
	tmp = msg->getGrandmasterClockQuality()->offsetScaledLogVariance;
	tmp = PLAT_htons(tmp);
	memcpy(that1 + 3, &tmp, sizeof(tmp));

	this1[5] = grandmasterPriority2;
	that1[5] = msg->getGrandmasterPriority2();

	this->getGrandmasterIdentity((char *)this1 + 6);
	msg->getGrandmasterIdentity((char *)that1 + 6);

#if 0
	GPTP_LOG_VERBOSE("Us: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_VERBOSE("%hhx", this1[i]);
	GPTP_LOG_VERBOSE("\n");
	GPTP_LOG_VERBOSE("Them: ");
	for (int i = 0; i < 14; ++i)
		GPTP_LOG_VERBOSE("%hhx", that1[i]);
	GPTP_LOG_VERBOSE("\n");
#endif

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}


PTPMessageSync::PTPMessageSync() {
}

PTPMessageSync::~PTPMessageSync() {
}

PTPMessageSync::PTPMessageSync( EtherPort *port ) :
	PTPMessageCommon( port )
{
	messageType = SYNC_MESSAGE;	// This is an event message
	sequenceId = port->getNextSyncSequenceId();
	control = SYNC;

	flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);

	originTimestamp = port->getClock()->getTime();

	logMeanMessageInterval = port->getSyncInterval();
	return;
}

void PTPMessageSync::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;
	Timestamp originTimestamp_BE;
	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_SYNC_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	// Get timestamp
	originTimestamp = port->getClock()->getTime();
	originTimestamp_BE.seconds_ms = PLAT_htons(originTimestamp.seconds_ms);
	originTimestamp_BE.seconds_ls = PLAT_htonl(originTimestamp.seconds_ls);
	originTimestamp_BE.nanoseconds =
	    PLAT_htonl(originTimestamp.nanoseconds);
	// Copy in v2 sync specific fields
	memcpy(buf_ptr + PTP_SYNC_SEC_MS(PTP_SYNC_OFFSET),
	       &(originTimestamp_BE.seconds_ms),
	       sizeof(originTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_SYNC_SEC_LS(PTP_SYNC_OFFSET),
	       &(originTimestamp_BE.seconds_ls),
	       sizeof(originTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_SYNC_NSEC(PTP_SYNC_OFFSET),
	       &(originTimestamp_BE.nanoseconds),
	       sizeof(originTimestamp.nanoseconds));

	port->sendEventPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);
	port->incCounter_ieee8021AsPortStatTxSyncCount();

	return;
}

PTPMessageAnnounce::PTPMessageAnnounce( CommonPort *port ) :
	PTPMessageCommon( port )
{
	messageType = ANNOUNCE_MESSAGE;	// This is an event message
	sequenceId = port->getNextAnnounceSequenceId();
	ClockIdentity id;
	control = MESSAGE_OTHER;
	ClockIdentity clock_identity;

	id = port->getClock()->getClockIdentity();
	tlv.appendClockIdentity(&id);

	currentUtcOffset = port->getClock()->getCurrentUtcOffset();
	grandmasterPriority1 = port->getClock()->getPriority1();
	grandmasterPriority2 = port->getClock()->getPriority2();
	grandmasterClockQuality = new ClockQuality();
	*grandmasterClockQuality = port->getClock()->getClockQuality();
	stepsRemoved = 0;
	timeSource = port->getClock()->getTimeSource();
	clock_identity = port->getClock()->getGrandmasterClockIdentity();
	clock_identity.getIdentityString(grandmasterIdentity);

	logMeanMessageInterval = port->getAnnounceInterval();
	return;
}

void PTPMessageAnnounce::sendPort
( CommonPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;

	uint16_t currentUtcOffset_l = PLAT_htons(currentUtcOffset);
	uint16_t stepsRemoved_l = PLAT_htons(stepsRemoved);
	ClockQuality clockQuality_l = *grandmasterClockQuality;
	clockQuality_l.offsetScaledLogVariance =
	    PLAT_htons(clockQuality_l.offsetScaledLogVariance);

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength =
		PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH + tlv.length();
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	memcpy(buf_ptr + PTP_ANNOUNCE_CURRENT_UTC_OFFSET(PTP_ANNOUNCE_OFFSET),
	       &currentUtcOffset_l, sizeof(currentUtcOffset));
	memcpy(buf_ptr +
	       PTP_ANNOUNCE_GRANDMASTER_PRIORITY1(PTP_ANNOUNCE_OFFSET),
	       &grandmasterPriority1, sizeof(grandmasterPriority1));
	memcpy(buf_ptr +
	       PTP_ANNOUNCE_GRANDMASTER_CLOCK_QUALITY(PTP_ANNOUNCE_OFFSET),
	       &clockQuality_l, sizeof(clockQuality_l));
	memcpy(buf_ptr +
	       PTP_ANNOUNCE_GRANDMASTER_PRIORITY2(PTP_ANNOUNCE_OFFSET),
	       &grandmasterPriority2, sizeof(grandmasterPriority2));
	memcpy( buf_ptr+
		PTP_ANNOUNCE_GRANDMASTER_IDENTITY(PTP_ANNOUNCE_OFFSET),
		grandmasterIdentity, PTP_CLOCK_IDENTITY_LENGTH );
	memcpy(buf_ptr + PTP_ANNOUNCE_STEPS_REMOVED(PTP_ANNOUNCE_OFFSET),
	       &stepsRemoved_l, sizeof(stepsRemoved));
	memcpy(buf_ptr + PTP_ANNOUNCE_TIME_SOURCE(PTP_ANNOUNCE_OFFSET),
	       &timeSource, sizeof(timeSource));
	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);
	port->incCounter_ieee8021AsPortStatTxAnnounce();

	return;
}

void PTPMessageAnnounce::processMessage( EtherPort *port )
{
	ClockIdentity my_clock_identity;

	port->incCounter_ieee8021AsPortStatRxAnnounce();

	// Delete announce receipt timeout
	port->getClock()->deleteEventTimerLocked
		(port, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES);

	if( stepsRemoved >= 255 ) goto bail;

	// Reject Announce message from myself
	my_clock_identity = port->getClock()->getClockIdentity();
	if( sourcePortIdentity->getClockIdentity() == my_clock_identity ) {
		goto bail;
	}

	if(tlv.has(&my_clock_identity)) {
		goto bail;
	}

	// Add message to the list
	port->setQualifiedAnnounce( this );

	port->getClock()->addEventTimerLocked(port, STATE_CHANGE_EVENT, 16000000);
 bail:
	port->getClock()->addEventTimerLocked
		(port, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		 (unsigned long long)
		 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER *
		  (pow
		   ((double)2,
			port->getAnnounceInterval()) *
		   1000000000.0)));
}

void PTPMessageSync::processMessage( EtherPort *port )
{
	if (port->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxSyncCount();

	if( flags[PTP_ASSIST_BYTE] & (0x1<<PTP_ASSIST_BIT)) {
		PTPMessageSync *old_sync = port->getLastSync();

		if (old_sync != NULL) {
			delete old_sync;
		}
		port->setLastSync(this);
		_gc = false;
		goto done;
	} else {
		GPTP_LOG_ERROR("PTP assist flag is not set, discarding invalid sync");
		_gc = true;
		goto done;
	}

 done:
	return;
}

PTPMessageFollowUp::PTPMessageFollowUp( EtherPort *port ) :
	PTPMessageCommon( port )
{
	messageType = FOLLOWUP_MESSAGE;	/* This is an event message */
	control = FOLLOWUP;

	logMeanMessageInterval = port->getSyncInterval();

	return;
}

void PTPMessageFollowUp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;
	Timestamp preciseOriginTimestamp_BE;
	memset(buf_t, 0, 256);
	/* Create packet in buf
	   Copy in common header */
	messageLength =
	    PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + sizeof(tlv);
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	preciseOriginTimestamp_BE.seconds_ms =
	    PLAT_htons(preciseOriginTimestamp.seconds_ms);
	preciseOriginTimestamp_BE.seconds_ls =
	    PLAT_htonl(preciseOriginTimestamp.seconds_ls);
	preciseOriginTimestamp_BE.nanoseconds =
	    PLAT_htonl(preciseOriginTimestamp.nanoseconds);
	/* Copy in v2 sync specific fields */
	memcpy(buf_ptr + PTP_FOLLOWUP_SEC_MS(PTP_FOLLOWUP_OFFSET),
	       &(preciseOriginTimestamp_BE.seconds_ms),
	       sizeof(preciseOriginTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_FOLLOWUP_SEC_LS(PTP_FOLLOWUP_OFFSET),
	       &(preciseOriginTimestamp_BE.seconds_ls),
	       sizeof(preciseOriginTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_FOLLOWUP_NSEC(PTP_FOLLOWUP_OFFSET),
	       &(preciseOriginTimestamp_BE.nanoseconds),
	       sizeof(preciseOriginTimestamp.nanoseconds));

	/*Change time base indicator to Network Order before sending it*/
	uint16_t tbi_NO = PLAT_htonl(tlv.getGMTimeBaseIndicator());
	tlv.setGMTimeBaseIndicator(tbi_NO);
	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH);

	GPTP_LOG_VERBOSE
		("Follow-Up Time: %u seconds(hi)", preciseOriginTimestamp.seconds_ms);
	GPTP_LOG_VERBOSE
		("Follow-Up Time: %u seconds", preciseOriginTimestamp.seconds_ls);
	GPTP_LOG_VERBOSE
		("FW-UP Time: %u nanoseconds", preciseOriginTimestamp.nanoseconds);
	GPTP_LOG_VERBOSE
		("FW-UP Time: %x seconds", preciseOriginTimestamp.seconds_ls);
	GPTP_LOG_VERBOSE
		("FW-UP Time: %x nanoseconds", preciseOriginTimestamp.nanoseconds);
#ifdef DEBUG
	GPTP_LOG_VERBOSE("Follow-up Dump:");
	for (int i = 0; i < messageLength; ++i) {
		GPTP_LOG_VERBOSE("%d:%02x ", i, (unsigned char)buf_t[i]);
	}
#endif

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);

	port->incCounter_ieee8021AsPortStatTxFollowUpCount();

	return;
}

void PTPMessageFollowUp::processMessage( EtherPort *port )
{
	uint64_t delay;
	Timestamp sync_arrival;
	Timestamp system_time(0, 0, 0);
	Timestamp device_time(0, 0, 0);

	signed long long local_system_offset;
	signed long long scalar_offset;

	FrequencyRatio local_clock_adjustment;
	FrequencyRatio local_system_freq_offset;
	FrequencyRatio master_local_freq_offset;
	int64_t correction;
	int32_t scaledLastGmFreqChange = 0;
	scaledNs scaledLastGmPhaseChange;

	GPTP_LOG_DEBUG("Processing a follow-up message");

	// Expire any SYNC_RECEIPT timers that exist
	port->stopSyncReceiptTimer();

	if (port->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxFollowUpCount();

	PortIdentity sync_id;
	PTPMessageSync *sync = port->getLastSync();
	if (sync == NULL) {
		GPTP_LOG_ERROR("Received Follow Up but there is no sync message");
		return;
	}
	sync->getPortIdentity(&sync_id);

	if (sync->getSequenceId() != sequenceId || sync_id != *sourcePortIdentity)
	{
		unsigned int cnt = 0;

		if( !port->incWrongSeqIDCounter(&cnt) )
		{
			port->becomeMaster( true );
			port->setWrongSeqIDCounter(0);
		}
		GPTP_LOG_ERROR
		    ("Received Follow Up %d times but cannot find corresponding Sync", cnt);
		goto done;
	}

	if (sync->getTimestamp()._version != port->getTimestampVersion())
	{
		GPTP_LOG_ERROR("Received Follow Up but timestamp version indicates Sync is out of date");
		goto done;
	}

	sync_arrival = sync->getTimestamp();

	if( !port->getLinkDelay(&delay) ) {
		goto done;
	}

	master_local_freq_offset  =  tlv.getRateOffset();
	master_local_freq_offset /= 1ULL << 41;
	master_local_freq_offset += 1.0;
	master_local_freq_offset /= port->getPeerRateOffset();

	correctionField /= 1 << 16;
	correction = (int64_t)((delay * master_local_freq_offset) + correctionField );

	if( correction > 0 )
	  TIMESTAMP_ADD_NS( preciseOriginTimestamp, correction );
	else TIMESTAMP_SUB_NS( preciseOriginTimestamp, -correction );
	scalar_offset  = TIMESTAMP_TO_NS( sync_arrival );
	scalar_offset -= TIMESTAMP_TO_NS( preciseOriginTimestamp );

	GPTP_LOG_VERBOSE
		("Followup Correction Field: %Ld, Link Delay: %lu", correctionField,
		 delay);
	GPTP_LOG_VERBOSE
		("FollowUp Scalar = %lld", scalar_offset);

	/* Otherwise synchronize clock with approximate time from Sync message */
	uint32_t local_clock, nominal_clock_rate;
	uint32_t device_sync_time_offset;

	port->getDeviceTime(system_time, device_time, local_clock,
			    nominal_clock_rate);
	GPTP_LOG_VERBOSE
		( "Device Time = %llu,System Time = %llu",
		  TIMESTAMP_TO_NS(device_time), TIMESTAMP_TO_NS(system_time));

	/* Adjust local_clock to correspond to sync_arrival */
	device_sync_time_offset =
	    TIMESTAMP_TO_NS(device_time) - TIMESTAMP_TO_NS(sync_arrival);

	GPTP_LOG_VERBOSE
	    ("ptp_message::FollowUp::processMessage System time: %u,%u "
		 "Device Time: %u,%u",
	     system_time.seconds_ls, system_time.nanoseconds,
	     device_time.seconds_ls, device_time.nanoseconds);

	local_clock_adjustment =
	  port->getClock()->
	  calcMasterLocalClockRateDifference
	  ( preciseOriginTimestamp, sync_arrival );

	/*Update information on local status structure.*/
	scaledLastGmFreqChange = (int32_t)((1.0/local_clock_adjustment -1.0) * (1ULL << 41));
	scaledLastGmPhaseChange.setLSB( tlv.getRateOffset() );
	port->getClock()->getFUPStatus()->setScaledLastGmFreqChange( scaledLastGmFreqChange );
	port->getClock()->getFUPStatus()->setScaledLastGmPhaseChange( scaledLastGmPhaseChange );

	if( port->getPortState() == PTP_SLAVE )
	{
		/* The sync_count counts the number of sync messages received
		   that influence the time on the device. Since adjustments are only
		   made in the PTP_SLAVE state, increment it here */
		port->incSyncCount();

		/* Do not call calcLocalSystemClockRateDifference it updates state
		   global to the clock object and if we are master then the network
		   is transitioning to us not being master but the master process
		   is still running locally */
		local_system_freq_offset =
			port->getClock()
			->calcLocalSystemClockRateDifference
			( device_time, system_time );
		TIMESTAMP_SUB_NS
			( system_time, (uint64_t)
			  (((FrequencyRatio) device_sync_time_offset)/
			   local_system_freq_offset) );
		local_system_offset =
			TIMESTAMP_TO_NS(system_time) - TIMESTAMP_TO_NS(sync_arrival);

		port->getClock()->setMasterOffset
			( port, scalar_offset, sync_arrival, local_clock_adjustment,
			  local_system_offset, system_time, local_system_freq_offset,
			  port->getSyncCount(), port->getPdelayCount(),
			  port->getPortState(), port->getAsCapable() );
		port->syncDone();
		// Restart the SYNC_RECEIPT timer
		port->startSyncReceiptTimer((unsigned long long)
			 (SYNC_RECEIPT_TIMEOUT_MULTIPLIER *
			  ((double) pow((double)2, port->getSyncInterval()) *
			   1000000000.0)));
	}

	uint16_t lastGmTimeBaseIndicator;
	lastGmTimeBaseIndicator = port->getLastGmTimeBaseIndicator();
	if ((lastGmTimeBaseIndicator > 0) && (tlv.getGmTimeBaseIndicator() != lastGmTimeBaseIndicator)) {
		GPTP_LOG_EXCEPTION("Sync discontinuity");
	}
	port->setLastGmTimeBaseIndicator(tlv.getGmTimeBaseIndicator());

done:
	_gc = true;
	port->setLastSync(NULL);
	delete sync;

	return;
}

PTPMessagePathDelayReq::PTPMessagePathDelayReq
( EtherPort *port ) : PTPMessageCommon( port )
{
	logMeanMessageInterval = 0;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_REQ_MESSAGE;
	sequenceId = port->getNextPDelaySequenceId();
	return;
}

void PTPMessagePathDelayReq::processMessage( EtherPort *port )
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	PortIdentity resp_fwup_id;
	PortIdentity requestingPortIdentity_p;
	PTPMessagePathDelayResp *resp;
	PortIdentity resp_id;
	PTPMessagePathDelayRespFollowUp *resp_fwup;

	int ts_good;
	Timestamp resp_timestamp;
	unsigned resp_timestamp_counter_value;
	unsigned req = TX_TIMEOUT_BASE;
	int iter = TX_TIMEOUT_ITER;

	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		goto done;
	}

	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		goto done;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayRequest();

	/* Generate and send message */
	resp = new PTPMessagePathDelayResp(port);
	port->getPortIdentity(resp_id);
	resp->setPortIdentity(&resp_id);
	resp->setSequenceId(sequenceId);

	GPTP_LOG_DEBUG("Process PDelay Request SeqId: %u\t", sequenceId);

#ifdef DEBUG
	for (int n = 0; n < PTP_CLOCK_IDENTITY_LENGTH; ++n) {
		GPTP_LOG_VERBOSE("%c", resp_id.clockIdentity[n]);
	}
#endif

	this->getPortIdentity(&requestingPortIdentity_p);
	resp->setRequestingPortIdentity(&requestingPortIdentity_p);
	resp->setRequestReceiptTimestamp(_timestamp);
	port->getTxLock();
	resp->sendPort(port, sourcePortIdentity);

	GPTP_LOG_DEBUG("*** Sent PDelay Response message");

	GPTP_LOG_VERBOSE("Start TS Read");
	ts_good = port->getTxTimestamp
		(resp, resp_timestamp, resp_timestamp_counter_value, false);

	GPTP_LOG_VERBOSE("Done TS Read");

	while (ts_good != GPTP_EC_SUCCESS && iter-- != 0) {
		timer->sleep(req);
		if (ts_good == GPTP_EC_EAGAIN && iter < 1)
			GPTP_LOG_ERROR( "Error (TX) timestamping PDelay Response "
						 "(Retrying-%d), error=%d", iter, ts_good);
		ts_good = port->getTxTimestamp
			(resp, resp_timestamp, resp_timestamp_counter_value, iter == 0);
		req *= 2;
	}
	port->putTxLock();

	if (ts_good != GPTP_EC_SUCCESS) {
		char msg[HWTIMESTAMPER_EXTENDED_MESSAGE_SIZE];
		port->getExtendedError(msg);
		GPTP_LOG_ERROR
			( "Error (TX) timestamping PDelay Response, error=%d\t%s",
			  ts_good, msg);
		delete resp;
		goto done;
	}

	if( resp_timestamp._version != _timestamp._version ) {
		GPTP_LOG_ERROR("TX timestamp version mismatch: %u/%u",
			    resp_timestamp._version, _timestamp._version);
#if 0 // discarding the request could lead to the peer setting the link to non-asCapable
		delete resp;
		goto done;
#endif
	}

	resp_fwup = new PTPMessagePathDelayRespFollowUp(port);
	port->getPortIdentity(resp_fwup_id);
	resp_fwup->setPortIdentity(&resp_fwup_id);
	resp_fwup->setSequenceId(sequenceId);
	resp_fwup->setRequestingPortIdentity(sourcePortIdentity);
	resp_fwup->setResponseOriginTimestamp(resp_timestamp);
	long long turnaround;
	turnaround =
	    (resp_timestamp.seconds_ls - _timestamp.seconds_ls) * 1000000000LL;

	GPTP_LOG_VERBOSE("Response Depart(sec): %u", resp_timestamp.seconds_ls);
	GPTP_LOG_VERBOSE("Request Arrival(sec): %u", _timestamp.seconds_ls);
	GPTP_LOG_VERBOSE("#1 Correction Field: %Ld", turnaround);

	turnaround += resp_timestamp.nanoseconds;

	GPTP_LOG_VERBOSE("#2 Correction Field: %Ld", turnaround);

	turnaround -= _timestamp.nanoseconds;

	GPTP_LOG_VERBOSE("#3 Correction Field: %Ld", turnaround);

	resp_fwup->setCorrectionField(0);
	resp_fwup->sendPort(port, sourcePortIdentity);

	GPTP_LOG_DEBUG("*** Sent PDelay Response FollowUp message");

	delete resp;
	delete resp_fwup;

done:
	delete timer;
	_gc = true;
	return;
}

void PTPMessagePathDelayReq::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	if(port->pdelayHalted())
		return;

	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	memset(buf_t, 0, 256);
	/* Create packet in buf */
	/* Copy in common header */
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	port->sendEventPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY, destIdentity);
	port->incCounter_ieee8021AsPortStatTxPdelayRequest();
	return;
}

PTPMessagePathDelayResp::PTPMessagePathDelayResp
( EtherPort *port ) : PTPMessageCommon( port )
{
    /*TODO: Why 0x7F?*/
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_RESP_MESSAGE;
	versionPTP = GPTP_VERSION;
	requestingPortIdentity = new PortIdentity();

	flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);

	return;
}

PTPMessagePathDelayResp::~PTPMessagePathDelayResp()
{
	delete requestingPortIdentity;
}

void PTPMessagePathDelayResp::processMessage( EtherPort *port )
{
	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayResponse();

	if (port->tryPDelayRxLock() != true) {
		GPTP_LOG_ERROR("Failed to get PDelay RX Lock");
		return;
	}

	PortIdentity resp_id;
	PortIdentity oldresp_id;
	uint16_t resp_port_number;
	uint16_t oldresp_port_number;

	PTPMessagePathDelayResp *old_pdelay_resp = port->getLastPDelayResp();
	if( old_pdelay_resp == NULL ) {
		goto bypass_verify_duplicate;
	}

	old_pdelay_resp->getPortIdentity(&oldresp_id);
	oldresp_id.getPortNumber(&oldresp_port_number);
	getPortIdentity(&resp_id);
	resp_id.getPortNumber(&resp_port_number);

	/* In the case where we have multiple PDelay responses for the same
	 * PDelay request, and they come from different sources, it is necessary
	 * to verify if this happens 3 times (sequentially). If it does, PDelayRequests
	 * are halted for 5 minutes
	 */
	if( getSequenceId() == old_pdelay_resp->getSequenceId() )
	{
		/*If the duplicates are in sequence and from different sources*/
		if( (resp_port_number != oldresp_port_number ) && (
					(port->getLastInvalidSeqID() + 1 ) == getSequenceId() ||
					port->getDuplicateRespCounter() == 0 ) ){
			GPTP_LOG_ERROR("Two responses for same Request. seqID %d. First Response Port# %hu. Second Port# %hu. Counter %d",
				getSequenceId(), oldresp_port_number, resp_port_number, port->getDuplicateRespCounter());

			if( port->incrementDuplicateRespCounter() ) {
				GPTP_LOG_ERROR("Remote misbehaving. Stopping PDelay Requests for 5 minutes.");
				port->stopPDelay();
				port->getClock()->addEventTimerLocked
					(port, PDELAY_RESP_PEER_MISBEHAVING_TIMEOUT_EXPIRES, (int64_t)(300 * 1000000000.0));
			}
		}
		else {
			port->setDuplicateRespCounter(0);
		}
		port->setLastInvalidSeqID(getSequenceId());
	}
	else
	{
		port->setDuplicateRespCounter(0);
	}

bypass_verify_duplicate:
	port->setLastPDelayResp(this);

	if (old_pdelay_resp != NULL) {
		delete old_pdelay_resp;
	}

	port->putPDelayRxLock();
	_gc = false;

	return;
}

void PTPMessagePathDelayResp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	Timestamp requestReceiptTimestamp_BE;
	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	requestReceiptTimestamp_BE.seconds_ms =
	    PLAT_htons(requestReceiptTimestamp.seconds_ms);
	requestReceiptTimestamp_BE.seconds_ls =
	    PLAT_htonl(requestReceiptTimestamp.seconds_ls);
	requestReceiptTimestamp_BE.nanoseconds =
	    PLAT_htonl(requestReceiptTimestamp.nanoseconds);

	// Copy in v2 PDelay_Req specific fields
	requestingPortIdentity->getClockIdentityString
	  (buf_ptr + PTP_PDELAY_RESP_REQ_CLOCK_ID
	   (PTP_PDELAY_RESP_OFFSET));
	requestingPortIdentity->getPortNumberNO
		((uint16_t *)
		 (buf_ptr + PTP_PDELAY_RESP_REQ_PORT_ID
		  (PTP_PDELAY_RESP_OFFSET)));
	memcpy(buf_ptr + PTP_PDELAY_RESP_SEC_MS(PTP_PDELAY_RESP_OFFSET),
	       &(requestReceiptTimestamp_BE.seconds_ms),
	       sizeof(requestReceiptTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_PDELAY_RESP_SEC_LS(PTP_PDELAY_RESP_OFFSET),
	       &(requestReceiptTimestamp_BE.seconds_ls),
	       sizeof(requestReceiptTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_PDELAY_RESP_NSEC(PTP_PDELAY_RESP_OFFSET),
	       &(requestReceiptTimestamp_BE.nanoseconds),
	       sizeof(requestReceiptTimestamp.nanoseconds));

	GPTP_LOG_VERBOSE("PDelay Resp Timestamp: %u,%u",
		   requestReceiptTimestamp.seconds_ls,
		   requestReceiptTimestamp.nanoseconds);

	port->sendEventPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY, destIdentity);
	port->incCounter_ieee8021AsPortStatTxPdelayResponse();
	return;
}

void PTPMessagePathDelayResp::setRequestingPortIdentity
(PortIdentity * identity)
{
	*requestingPortIdentity = *identity;
}

void PTPMessagePathDelayResp::getRequestingPortIdentity
(PortIdentity * identity)
{
	*identity = *requestingPortIdentity;
}

PTPMessagePathDelayRespFollowUp::PTPMessagePathDelayRespFollowUp
( EtherPort *port ) : PTPMessageCommon( port )
{
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_FOLLOWUP_MESSAGE;
	versionPTP = GPTP_VERSION;
	requestingPortIdentity = new PortIdentity();

	return;
}

PTPMessagePathDelayRespFollowUp::~PTPMessagePathDelayRespFollowUp()
{
	delete requestingPortIdentity;
}

#define US_PER_SEC 1000000
void PTPMessagePathDelayRespFollowUp::processMessage
( EtherPort *port )
{
	Timestamp remote_resp_tx_timestamp(0, 0, 0);
	Timestamp request_tx_timestamp(0, 0, 0);
	Timestamp remote_req_rx_timestamp(0, 0, 0);
	Timestamp response_rx_timestamp(0, 0, 0);

	if (port->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		return;
	}
	if (port->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxPdelayResponseFollowUp();

	if (port->tryPDelayRxLock() != true)
		return;

	PTPMessagePathDelayReq *req = port->getLastPDelayReq();
	PTPMessagePathDelayResp *resp = port->getLastPDelayResp();

	PortIdentity req_id;
	PortIdentity resp_id;
	PortIdentity fup_sourcePortIdentity;
	PortIdentity resp_sourcePortIdentity;
	ClockIdentity req_clkId;
	ClockIdentity resp_clkId;

	uint16_t resp_port_number;
	uint16_t req_port_number;

	if (req == NULL) {
		/* Shouldn't happen */
		GPTP_LOG_ERROR
		    (">>> Received PDelay followup but no REQUEST exists");
		goto abort;
	}

	if (resp == NULL) {
		/* Probably shouldn't happen either */
		GPTP_LOG_ERROR
		    (">>> Received PDelay followup but no RESPONSE exists");

		goto abort;
	}

	req->getPortIdentity(&req_id);
	resp->getRequestingPortIdentity(&resp_id);
	req_clkId = req_id.getClockIdentity();
	resp_clkId = resp_id.getClockIdentity();
	resp_id.getPortNumber(&resp_port_number);
	requestingPortIdentity->getPortNumber(&req_port_number);
	resp->getPortIdentity(&resp_sourcePortIdentity);
	getPortIdentity(&fup_sourcePortIdentity);

	if( req->getSequenceId() != sequenceId ) {
		GPTP_LOG_ERROR
			(">>> Received PDelay FUP has different seqID than the PDelay request (%d/%d)",
			 sequenceId, req->getSequenceId() );
		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if (resp->getSequenceId() != sequenceId) {
		GPTP_LOG_ERROR
			("Received PDelay Response Follow Up but cannot find "
			 "corresponding response");
		GPTP_LOG_ERROR("%hu, %hu, %hu, %hu", resp->getSequenceId(),
				sequenceId, resp_port_number, req_port_number);

		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if (req_clkId != resp_clkId ) {
		GPTP_LOG_ERROR
			("ClockID Resp/Req differs. PDelay Response ClockID: %s PDelay Request ClockID: %s",
			 req_clkId.getIdentityString().c_str(), resp_clkId.getIdentityString().c_str() );
		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if ( resp_port_number != req_port_number ) {
		GPTP_LOG_ERROR
			("Request port number (%hu) is different from Response port number (%hu)",
				resp_port_number, req_port_number);

		goto abort;
	}

	/*
	 * IEEE 802.1AS, Figure 11-8, subclause 11.2.15.3
	 */
	if ( fup_sourcePortIdentity != resp_sourcePortIdentity ) {
		GPTP_LOG_ERROR("Source port identity from PDelay Response/FUP differ");

		goto abort;
	}

	port->getClock()->deleteEventTimerLocked
		(port, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES);

	GPTP_LOG_VERBOSE("Request Sequence Id: %u", req->getSequenceId());
	GPTP_LOG_VERBOSE("Response Sequence Id: %u", resp->getSequenceId());
	GPTP_LOG_VERBOSE("Follow-Up Sequence Id: %u", req->getSequenceId());

	int64_t link_delay;
	unsigned long long turn_around;

	/* Assume that we are a two step clock, otherwise originTimestamp
	   may be used */
	request_tx_timestamp = req->getTimestamp();
	if( request_tx_timestamp.nanoseconds == INVALID_TIMESTAMP.nanoseconds ) {
		/* Stop processing the packet */
		goto abort;
	}
	if (request_tx_timestamp.nanoseconds ==
	    PDELAY_PENDING_TIMESTAMP.nanoseconds) {
		// Defer processing
		if(
			port->getLastPDelayRespFollowUp() != NULL &&
			port->getLastPDelayRespFollowUp() != this )
		{
			delete port->getLastPDelayRespFollowUp();
		}
		port->setLastPDelayRespFollowUp(this);
		port->getClock()->addEventTimerLocked
			(port, PDELAY_DEFERRED_PROCESSING, 1000000);
		goto defer;
	}
	remote_req_rx_timestamp = resp->getRequestReceiptTimestamp();
	response_rx_timestamp = resp->getTimestamp();
	remote_resp_tx_timestamp = responseOriginTimestamp;

	if( request_tx_timestamp._version != response_rx_timestamp._version ) {
		GPTP_LOG_ERROR("RX timestamp version mismatch %d/%d",
			    request_tx_timestamp._version, response_rx_timestamp._version );
		goto abort;
	}

	port->incPdelayCount();


	link_delay =
		((response_rx_timestamp.seconds_ms * 1LL -
		  request_tx_timestamp.seconds_ms) << 32) * 1000000000;
	link_delay +=
		(response_rx_timestamp.seconds_ls * 1LL -
		 request_tx_timestamp.seconds_ls) * 1000000000;
	link_delay +=
		(response_rx_timestamp.nanoseconds * 1LL -
		 request_tx_timestamp.nanoseconds);

	turn_around =
		((remote_resp_tx_timestamp.seconds_ms * 1LL -
		  remote_req_rx_timestamp.seconds_ms) << 32) * 1000000000;
	turn_around +=
		(remote_resp_tx_timestamp.seconds_ls * 1LL -
		 remote_req_rx_timestamp.seconds_ls) * 1000000000;
	turn_around +=
		(remote_resp_tx_timestamp.nanoseconds * 1LL -
		 remote_req_rx_timestamp.nanoseconds);

	// Adjust turn-around time for peer to local clock rate difference
	// TODO: Are these .998 and 1.002 specifically defined in the standard?
	// Should we create a define for them ?
	if
		( port->getPeerRateOffset() > .998 &&
		  port->getPeerRateOffset() < 1.002 ) {
		turn_around = (int64_t) (turn_around * port->getPeerRateOffset());
	}

	GPTP_LOG_VERBOSE
		("Turn Around Adjustment %Lf",
		 ((long long)turn_around * port->getPeerRateOffset()) /
		 1000000000000LL);
	GPTP_LOG_VERBOSE
		("Step #1: Turn Around Adjustment %Lf",
		 ((long long)turn_around * port->getPeerRateOffset()));
	GPTP_LOG_VERBOSE("Adjusted Peer turn around is %Lu", turn_around);

	/* Subtract turn-around time from link delay after rate adjustment */
	link_delay -= turn_around;
	link_delay /= 2;

	{
		uint64_t mine_elapsed;
		uint64_t theirs_elapsed;
		Timestamp prev_peer_ts_mine;
		Timestamp prev_peer_ts_theirs;
		FrequencyRatio rate_offset;
		if( port->getPeerOffset( prev_peer_ts_mine, prev_peer_ts_theirs )) {
			FrequencyRatio upper_ratio_limit, lower_ratio_limit;
			upper_ratio_limit = PPM_OFFSET_TO_RATIO(UPPER_LIMIT_PPM);
			lower_ratio_limit = PPM_OFFSET_TO_RATIO(LOWER_LIMIT_PPM);

			mine_elapsed =  TIMESTAMP_TO_NS(request_tx_timestamp)-TIMESTAMP_TO_NS(prev_peer_ts_mine);
			theirs_elapsed = TIMESTAMP_TO_NS(remote_req_rx_timestamp)-TIMESTAMP_TO_NS(prev_peer_ts_theirs);
			theirs_elapsed -= port->getLinkDelay();
			theirs_elapsed += link_delay < 0 ? 0 : link_delay;
			rate_offset =  ((FrequencyRatio) mine_elapsed)/theirs_elapsed;

			if( rate_offset < upper_ratio_limit && rate_offset > lower_ratio_limit ) {
				port->setPeerRateOffset(rate_offset);
			}
		}
	}
	if( !port->setLinkDelay( link_delay ) ) {
		if (!port->getAutomotiveProfile()) {
			GPTP_LOG_ERROR("Link delay %ld beyond neighborPropDelayThresh; not AsCapable", link_delay);
			port->setAsCapable( false );
		}
	} else {
		if (!port->getAutomotiveProfile()) {
			port->setAsCapable( true );
		}
	}
	port->setPeerOffset( request_tx_timestamp, remote_req_rx_timestamp );

 abort:
	delete req;
	port->setLastPDelayReq(NULL);
	delete resp;
	port->setLastPDelayResp(NULL);

	_gc = true;

 defer:
	port->putPDelayRxLock();

	return;
}

void PTPMessagePathDelayRespFollowUp::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	Timestamp responseOriginTimestamp_BE;
	memset(buf_t, 0, 256);
	/* Create packet in buf
	   Copy in common header */
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);
	responseOriginTimestamp_BE.seconds_ms =
		PLAT_htons(responseOriginTimestamp.seconds_ms);
	responseOriginTimestamp_BE.seconds_ls =
		PLAT_htonl(responseOriginTimestamp.seconds_ls);
	responseOriginTimestamp_BE.nanoseconds =
		PLAT_htonl(responseOriginTimestamp.nanoseconds);

	// Copy in v2 PDelay_Req specific fields
	requestingPortIdentity->getClockIdentityString
		(buf_ptr + PTP_PDELAY_FOLLOWUP_REQ_CLOCK_ID
		 (PTP_PDELAY_FOLLOWUP_OFFSET));
	requestingPortIdentity->getPortNumberNO
		((uint16_t *)
		 (buf_ptr + PTP_PDELAY_FOLLOWUP_REQ_PORT_ID
		  (PTP_PDELAY_FOLLOWUP_OFFSET)));
	memcpy
		(buf_ptr + PTP_PDELAY_FOLLOWUP_SEC_MS(PTP_PDELAY_FOLLOWUP_OFFSET),
		 &(responseOriginTimestamp_BE.seconds_ms),
		 sizeof(responseOriginTimestamp.seconds_ms));
	memcpy
		(buf_ptr + PTP_PDELAY_FOLLOWUP_SEC_LS(PTP_PDELAY_FOLLOWUP_OFFSET),
		 &(responseOriginTimestamp_BE.seconds_ls),
		 sizeof(responseOriginTimestamp.seconds_ls));
	memcpy
		(buf_ptr + PTP_PDELAY_FOLLOWUP_NSEC(PTP_PDELAY_FOLLOWUP_OFFSET),
		 &(responseOriginTimestamp_BE.nanoseconds),
		 sizeof(responseOriginTimestamp.nanoseconds));

	GPTP_LOG_VERBOSE("PDelay Resp Timestamp: %u,%u",
		   responseOriginTimestamp.seconds_ls,
		   responseOriginTimestamp.nanoseconds);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_PDELAY, destIdentity);
	port->incCounter_ieee8021AsPortStatTxPdelayResponseFollowUp();
	return;
}

void PTPMessagePathDelayRespFollowUp::setRequestingPortIdentity
(PortIdentity * identity)
{
	*requestingPortIdentity = *identity;
}


 PTPMessageSignalling::PTPMessageSignalling(void)
{
}

PTPMessageSignalling::PTPMessageSignalling
( EtherPort *port ) : PTPMessageCommon( port )
{
	messageType = SIGNALLING_MESSAGE;
	sequenceId = port->getNextSignalSequenceId();

	targetPortIdentify = (int8_t)0xff;

	control = MESSAGE_OTHER;

	logMeanMessageInterval = 0x7F;    // 802.1AS 2011 10.5.2.2.11 logMessageInterval (Integer8)
}

 PTPMessageSignalling::~PTPMessageSignalling(void)
{
}

void PTPMessageSignalling::setintervals(int8_t linkDelayInterval, int8_t timeSyncInterval, int8_t announceInterval)
{
	tlv.setLinkDelayInterval(linkDelayInterval);
	tlv.setTimeSyncInterval(timeSyncInterval);
	tlv.setAnnounceInterval(announceInterval);
}

void PTPMessageSignalling::sendPort
( EtherPort *port, PortIdentity *destIdentity )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;

	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_SIGNALLING_LENGTH + sizeof(tlv);
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);

	memcpy(buf_ptr + PTP_SIGNALLING_TARGET_PORT_IDENTITY(PTP_SIGNALLING_OFFSET),
	       &targetPortIdentify, sizeof(targetPortIdentify));

	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_SIGNALLING_LENGTH);

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, MCAST_OTHER, destIdentity);
}

void PTPMessageSignalling::processMessage( EtherPort *port )
{
	long long unsigned int waitTime;

	GPTP_LOG_STATUS("Signalling Link Delay Interval: %d", tlv.getLinkDelayInterval());
	GPTP_LOG_STATUS("Signalling Sync Interval: %d", tlv.getTimeSyncInterval());
	GPTP_LOG_STATUS("Signalling Announce Interval: %d", tlv.getAnnounceInterval());

	char linkDelayInterval = tlv.getLinkDelayInterval();
	char timeSyncInterval = tlv.getTimeSyncInterval();
	char announceInterval = tlv.getAnnounceInterval();

	if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		port->setInitPDelayInterval();

		waitTime = ((long long) (pow((double)2, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		// TODO: No send functionality needs to be implemented.
		GPTP_LOG_WARNING("Signal received to stop sending pDelay messages: Not implemented");
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		// Nothing to do
	}
	else {
		port->setPDelayInterval(linkDelayInterval);

		waitTime = ((long long) (pow((double)2, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
	}

	if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		port->resetInitSyncInterval();

		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startSyncIntervalTimer(waitTime);
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		// TODO: No send functionality needs to be implemented.
		GPTP_LOG_WARNING("Signal received to stop sending Sync messages: Not implemented");
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		// Nothing to do
	}
	else {
		port->setSyncInterval(timeSyncInterval);

		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startSyncIntervalTimer(waitTime);
	}

	if (!port->getAutomotiveProfile()) {
		if (announceInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
			// TODO: Needs implementation
			GPTP_LOG_WARNING("Signal received to set Announce message to initial interval: Not implemented");
		}
		else if (announceInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
			// TODO: No send functionality needs to be implemented.
			GPTP_LOG_WARNING("Signal received to stop sending Announce messages: Not implemented");
		}
		else if (announceInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
			// Nothing to do
		}
		else {
			port->setAnnounceInterval(announceInterval);

			waitTime = ((long long) (pow((double)2, port->getAnnounceInterval()) *  1000000000.0));
			waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
			port->startAnnounceIntervalTimer(waitTime);
		}
	}
}
