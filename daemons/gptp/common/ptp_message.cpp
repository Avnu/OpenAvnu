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
#include <md_ethport.hpp>
#include <avbts_ostimer.hpp>

#include <stdio.h>
#include <string.h>
#include <math.h>

PTPMessageCommon::PTPMessageCommon(MediaDependentPort * port)
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
	//port->getPort()->getPortIdentity( *sourcePortIdentity );

	return;
}

/* Determine whether the message was sent by given communication technology,
   uuid, and port id fields */
bool PTPMessageCommon::isSenderEqual(PortIdentity portIdentity)
{
	return portIdentity == *sourcePortIdentity;
}

PTPMessageCommon *buildPTPMessage
( char *buf, int size, LinkLayerAddress * remote, MediaDependentPort * port,
  bool *event )
{
	PTPMessageCommon *msg = NULL;
	MessageType messageType;
	uint8_t tspec_msg_t = 0;
	uint8_t ptp_version;
	
	uint16_t sequenceId;
	PortIdentity *sourcePortIdentity;
	Timestamp timestamp(0, 0, 0);
	unsigned counter_value = 0;

	ClockIdentity from;
	ClockIdentity self = port->getPort()->getClock()->getClockIdentity();
	
	if (size < PTP_COMMON_HDR_LENGTH) {
		goto done;
	}

	memcpy(&tspec_msg_t,
	       buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
	       sizeof(tspec_msg_t));
	memcpy(&ptp_version,
	       buf + PTP_COMMON_HDR_PTP_VERSION(PTP_COMMON_HDR_OFFSET),
	       sizeof(ptp_version));

	if( tspec_msg_t >> 4 != 0x1 || ptp_version != 2 ) {
		XPTPD_ERROR( "Received payload that doesn't look like a 802.1AS message" );
		goto done;
	}

	messageType = (MessageType) (tspec_msg_t & 0xF);
	*event = (messageType & 0xF) >> 3 ? false : true;

	from.set((uint8_t *)
			 (buf + PTP_COMMON_HDR_SOURCE_CLOCK_ID(PTP_COMMON_HDR_OFFSET)));
	if( from == self ) {
		XPTPD_ERROR( "Received message with own source port identity" );
		goto done;
	}

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

	switch (messageType) {
	case SYNC_MESSAGE:
		XPTPD_INFOL( SYNC_DEBUG, "*** Received Sync message" );

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_SYNC_LENGTH) {
			goto done;
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
		XPTPD_INFOL( SYNC_DEBUG, "*** Received Follow Up message" );

		// Be sure buffer is the correction size
		if (size < (int) (PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + 
						  sizeof(FollowUpTLV))) {
			goto done;
		}
		{
			PTPMessageFollowUp *followup_msg =
			    new PTPMessageFollowUp();
			followup_msg->messageType = messageType;
			// Copy in v2 sync specific fields
			memcpy(&(followup_msg-> preciseOriginTimestamp.seconds_ms),
			       buf + PTP_FOLLOWUP_SEC_MS(PTP_FOLLOWUP_OFFSET), sizeof
				   (followup_msg->preciseOriginTimestamp.seconds_ms));
			memcpy(&(followup_msg-> preciseOriginTimestamp.seconds_ls),
			       buf + PTP_FOLLOWUP_SEC_LS(PTP_FOLLOWUP_OFFSET), sizeof
				   (followup_msg->preciseOriginTimestamp.seconds_ls));
			memcpy(&(followup_msg->preciseOriginTimestamp.nanoseconds),
			       buf + PTP_FOLLOWUP_NSEC(PTP_FOLLOWUP_OFFSET),sizeof
				   (followup_msg->preciseOriginTimestamp.nanoseconds));

			followup_msg->preciseOriginTimestamp.seconds_ms =
			    PLAT_ntohs
				(followup_msg->preciseOriginTimestamp.seconds_ms);
			followup_msg->preciseOriginTimestamp.seconds_ls =
			    PLAT_ntohl
				(followup_msg->preciseOriginTimestamp.seconds_ls);
			followup_msg->preciseOriginTimestamp.nanoseconds =
			    PLAT_ntohl
				(followup_msg->preciseOriginTimestamp.nanoseconds);

			memcpy
				( &(followup_msg->tlv), buf+PTP_FOLLOWUP_LENGTH,
				  sizeof(followup_msg->tlv) );

			msg = followup_msg;
		}
		break;
	case PATH_DELAY_REQ_MESSAGE:
		XPTPD_INFOL( PDELAY_DEBUG, "*** Received PDelay Request message" );

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH
		    && /* For Broadcom compatibility */ size != 46) {
			goto done;
		}
		{
			PTPMessagePathDelayReq *pdelay_req_msg =
			    new PTPMessagePathDelayReq();
			pdelay_req_msg->messageType = messageType;

			msg = pdelay_req_msg;
		}
		break;
	case PATH_DELAY_RESP_MESSAGE:
		XPTPD_INFOL
			( PDELAY_DEBUG, "*** Received PDelay Response message" );

		// Be sure buffer is the correction size
		if (size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_RESP_LENGTH) {
			goto done;
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


			memcpy
				(&(pdelay_resp_msg->requestReceiptTimestamp.seconds_ms),
				 buf + PTP_PDELAY_RESP_SEC_MS(PTP_PDELAY_RESP_OFFSET),
				 sizeof(pdelay_resp_msg->requestReceiptTimestamp.seconds_ms));
			memcpy
				(& (pdelay_resp_msg->requestReceiptTimestamp.seconds_ls),
				 buf + PTP_PDELAY_RESP_SEC_LS(PTP_PDELAY_RESP_OFFSET),
				 sizeof(pdelay_resp_msg->requestReceiptTimestamp.seconds_ls));
			memcpy
				(&(pdelay_resp_msg->requestReceiptTimestamp.nanoseconds),
				 buf + PTP_PDELAY_RESP_NSEC(PTP_PDELAY_RESP_OFFSET),
				 sizeof(pdelay_resp_msg->requestReceiptTimestamp.nanoseconds));

			pdelay_resp_msg->requestReceiptTimestamp.seconds_ms =
			    PLAT_ntohs
				(pdelay_resp_msg->requestReceiptTimestamp.seconds_ms);
			pdelay_resp_msg->requestReceiptTimestamp.seconds_ls =
			    PLAT_ntohl
				(pdelay_resp_msg->requestReceiptTimestamp.seconds_ls);
			pdelay_resp_msg->requestReceiptTimestamp.nanoseconds =
			    PLAT_ntohl
				(pdelay_resp_msg->requestReceiptTimestamp.nanoseconds);

			msg = pdelay_resp_msg;
		}
		break;
	case PATH_DELAY_FOLLOWUP_MESSAGE:
		XPTPD_INFO("*** Received PDelay Response FollowUp message");

		// Be sure buffer is the correction size
		{
			PTPMessagePathDelayRespFollowUp *pdelay_resp_fwup_msg =
			    new PTPMessagePathDelayRespFollowUp();
			pdelay_resp_fwup_msg->messageType = messageType;
			// Copy in v2 PDelay Response specific fields
			pdelay_resp_fwup_msg->requestingPortIdentity =
			    new PortIdentity
				((uint8_t *) buf + PTP_PDELAY_FOLLOWUP_REQ_CLOCK_ID
				 (PTP_PDELAY_RESP_OFFSET), (uint16_t *)
				 (buf + PTP_PDELAY_FOLLOWUP_REQ_PORT_ID
				  (PTP_PDELAY_FOLLOWUP_OFFSET)));

			memcpy
				(&(pdelay_resp_fwup_msg->responseOriginTimestamp.seconds_ms),
				 buf + PTP_PDELAY_FOLLOWUP_SEC_MS(PTP_PDELAY_FOLLOWUP_OFFSET),
				 sizeof
				 (pdelay_resp_fwup_msg->responseOriginTimestamp.seconds_ms));
			memcpy
				(& (pdelay_resp_fwup_msg->responseOriginTimestamp.seconds_ls),
				 buf + PTP_PDELAY_FOLLOWUP_SEC_LS(PTP_PDELAY_FOLLOWUP_OFFSET),
				 sizeof
				 (pdelay_resp_fwup_msg->responseOriginTimestamp.seconds_ls));
			memcpy
				(&(pdelay_resp_fwup_msg->responseOriginTimestamp.nanoseconds),
				 buf + PTP_PDELAY_FOLLOWUP_NSEC (PTP_PDELAY_FOLLOWUP_OFFSET),
				 sizeof
				 (pdelay_resp_fwup_msg->responseOriginTimestamp.nanoseconds));

			pdelay_resp_fwup_msg->responseOriginTimestamp.seconds_ms =
			    PLAT_ntohs
				(pdelay_resp_fwup_msg->responseOriginTimestamp.seconds_ms);
			pdelay_resp_fwup_msg-> responseOriginTimestamp.seconds_ls =
			    PLAT_ntohl
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.seconds_ls);
			pdelay_resp_fwup_msg->responseOriginTimestamp.nanoseconds =
			    PLAT_ntohl
			    (pdelay_resp_fwup_msg->responseOriginTimestamp.nanoseconds);

			msg = pdelay_resp_fwup_msg;
		}
		break;
	case ANNOUNCE_MESSAGE:
		{
			PTPMessageAnnounce *annc = new PTPMessageAnnounce();
			annc->messageType = messageType;

			memcpy
				(&(annc->currentUtcOffset), buf +
				 PTP_ANNOUNCE_CURRENT_UTC_OFFSET(PTP_ANNOUNCE_OFFSET),
				 sizeof(annc->currentUtcOffset));
			annc->currentUtcOffset = PLAT_ntohs(annc->currentUtcOffset);
			memcpy
				(&(annc->grandmasterPriority1), buf +
				 PTP_ANNOUNCE_GRANDMASTER_PRIORITY1(PTP_ANNOUNCE_OFFSET),
				 sizeof(annc->grandmasterPriority1));
			memcpy
				( annc->grandmasterClockQuality, buf +
				  PTP_ANNOUNCE_GRANDMASTER_CLOCK_QUALITY(PTP_ANNOUNCE_OFFSET),
				  sizeof( *(annc->grandmasterClockQuality) ));
			annc-> grandmasterClockQuality->offsetScaledLogVariance =
				PLAT_ntohs
				( annc->grandmasterClockQuality->offsetScaledLogVariance );
			memcpy
				(&(annc->grandmasterPriority2), buf +
				 PTP_ANNOUNCE_GRANDMASTER_PRIORITY2(PTP_ANNOUNCE_OFFSET),
				 sizeof(annc->grandmasterPriority2));
			memcpy
				(&(annc->grandmasterIdentity), buf +
				 PTP_ANNOUNCE_GRANDMASTER_IDENTITY(PTP_ANNOUNCE_OFFSET),
				 PTP_CLOCK_IDENTITY_LENGTH);
			memcpy
				(&(annc->stepsRemoved), buf +
				 PTP_ANNOUNCE_STEPS_REMOVED(PTP_ANNOUNCE_OFFSET),
				 sizeof(annc->stepsRemoved));
			annc->stepsRemoved = PLAT_ntohs(annc->stepsRemoved);
			memcpy
				(&(annc->timeSource), buf +
				 PTP_ANNOUNCE_TIME_SOURCE(PTP_ANNOUNCE_OFFSET),
				 sizeof(annc->timeSource));

			msg = annc;
		}
		break;
	default:

		XPTPD_ERROR("Received unsupported message type, %d",
					(int)messageType);
		goto done;
	}

	msg->_gc = false;

	// Copy in common header fields
	memcpy
		(&(msg->versionPTP), buf + PTP_COMMON_HDR_PTP_VERSION
		 (PTP_COMMON_HDR_OFFSET), sizeof(msg->versionPTP));
	memcpy
		(&(msg->messageLength), buf + PTP_COMMON_HDR_MSG_LENGTH
		 (PTP_COMMON_HDR_OFFSET), sizeof(msg->messageLength));
	msg->messageLength = PLAT_ntohs(msg->messageLength);
	memcpy
		(&(msg->domainNumber), buf + PTP_COMMON_HDR_DOMAIN_NUMBER
		 (PTP_COMMON_HDR_OFFSET), sizeof(msg->domainNumber));
	memcpy
		(&(msg->flags), buf + PTP_COMMON_HDR_FLAGS(PTP_COMMON_HDR_OFFSET),
		 PTP_FLAGS_LENGTH);
	memcpy
		(&(msg->correctionField), buf + PTP_COMMON_HDR_CORRECTION
		 (PTP_COMMON_HDR_OFFSET), sizeof(msg->correctionField));
	msg->correctionField =
		byte_swap64(msg->correctionField); // Assuming LE machine
	msg->sourcePortIdentity = sourcePortIdentity;
	msg->sequenceId = sequenceId;
	memcpy
		(&(msg->control), buf + PTP_COMMON_HDR_CONTROL(PTP_COMMON_HDR_OFFSET),
		 sizeof(msg->control));
	memcpy
		(&(msg->logMeanMessageInterval), buf +
		 PTP_COMMON_HDR_LOG_MSG_INTRVL(PTP_COMMON_HDR_OFFSET), 
		 sizeof(msg->logMeanMessageInterval));

	msg->_timestamp = timestamp;
	msg->_timestamp_counter_value = counter_value;

 done:
	return msg;
}

bool PTPMessageCommon::processMessage( MediaDependentPort * port )
{
	_gc = true;
	return true;
}

void PTPMessageCommon::buildCommonHeader(uint8_t * buf)
{
	unsigned char tspec_msg_t;
	tspec_msg_t = messageType | 0x10;
	//tspec_msg_t = messageType;
	long long correctionField_BE =
		byte_swap64(correctionField);	// Assume LE machine
	uint16_t messageLength_NO = PLAT_htons(messageLength);

	memcpy
		(buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
		 &tspec_msg_t, sizeof(tspec_msg_t));
	memcpy
		(buf + PTP_COMMON_HDR_PTP_VERSION(PTP_COMMON_HDR_OFFSET),
		 &versionPTP, sizeof(versionPTP));
	memcpy
		(buf + PTP_COMMON_HDR_MSG_LENGTH(PTP_COMMON_HDR_OFFSET),
		 &messageLength_NO, sizeof(messageLength_NO));
	memcpy
		(buf + PTP_COMMON_HDR_DOMAIN_NUMBER(PTP_COMMON_HDR_OFFSET),
		 &domainNumber, sizeof(domainNumber));
	memcpy
		(buf + PTP_COMMON_HDR_FLAGS(PTP_COMMON_HDR_OFFSET), &flags,
		 PTP_FLAGS_LENGTH);
	memcpy
		(buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
		 &correctionField_BE, sizeof(correctionField));

	sourcePortIdentity->getClockIdentityString
		((uint8_t *) buf + PTP_COMMON_HDR_SOURCE_CLOCK_ID
		 (PTP_COMMON_HDR_OFFSET));
	sourcePortIdentity->getPortNumberNO
		((uint16_t *)
		 (buf + PTP_COMMON_HDR_SOURCE_PORT_ID(PTP_COMMON_HDR_OFFSET)));

	XPTPD_INFO("Sending Sequence Id: %u", sequenceId);
	sequenceId = PLAT_htons(sequenceId);
	memcpy
		(buf + PTP_COMMON_HDR_SEQUENCE_ID(PTP_COMMON_HDR_OFFSET),
		 &sequenceId, sizeof(sequenceId));
	sequenceId = PLAT_ntohs(sequenceId);
	memcpy
		(buf + PTP_COMMON_HDR_CONTROL(PTP_COMMON_HDR_OFFSET), &control,
		 sizeof(control));
	memcpy
		(buf + PTP_COMMON_HDR_LOG_MSG_INTRVL(PTP_COMMON_HDR_OFFSET),
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

bool PTPMessageEther::processMessage( MediaDependentPort *port_in ) {
	MediaDependentEtherPort *port =
		dynamic_cast<MediaDependentEtherPort *>(port_in);
	if( port == NULL ) return false;
	return processMessage(port);
}
net_result PTPMessageEther::sendPort( MediaDependentPort *port_in ) {
	MediaDependentEtherPort *port =
		dynamic_cast<MediaDependentEtherPort *>(port_in);
	if( port == NULL ) return net_fatal;
	return sendPort(port);
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

	return (memcmp(this1, that1, 14) < 0) ? true : false;
}

PTPMessageSync::PTPMessageSync() {
}

PTPMessageSync::~PTPMessageSync() {
}

PTPMessageSync::PTPMessageSync( MediaDependentPort *port ) :
	PTPMessageEther(port)
{
	messageType = SYNC_MESSAGE;	// This is an event message
	control = SYNC;

	flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);

	originTimestamp = port->getPort()->getClock()->getTime();

	logMeanMessageInterval = port->getPort()->getSyncInterval();
}

net_result PTPMessageSync::sendPort( MediaDependentEtherPort * port )
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

	return port->sendEventPort( buf_t, messageLength, MCAST_OTHER );
}

PTPMessageAnnounce::PTPMessageAnnounce( MediaIndependentPort * port) :
	PTPMessageCommon(port->getPort())
{
	messageType = ANNOUNCE_MESSAGE;	// This is an event message
	sequenceId = port->getNextAnnounceSequenceId();
	ClockIdentity id;
	control = MESSAGE_OTHER;
	ClockIdentity clock_identity;

	id = port->getClock()->getClockIdentity();
	tlv.setClockIdentity(&id);

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

net_result PTPMessageAnnounce::sendPort( MediaDependentPort * port )
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
	    PTP_COMMON_HDR_LENGTH + PTP_ANNOUNCE_LENGTH + sizeof(tlv);
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

	return port->sendGeneralMessage( buf_t, messageLength );
}

bool PTPMessageAnnounce::processMessage( MediaDependentPort *port )
{
	MediaIndependentPort *iport = port->getPort();
	IEEE1588Clock *clock = port->getPort()->getClock();

	// Delete announce receipt timeout
	clock->deleteEventTimer
		( iport, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES);

	// If steps removed >= 255, this is invalid
	if( stepsRemoved >= 255 ) {
		goto bail;
	}

	// Add message to the list
	iport->addQualifiedAnnounce(this);

	if( clock->getPriority1() != 255 ) {
		clock->addEventTimer
			(iport, STATE_CHANGE_EVENT, 16000000);
	}

 bail:
	clock->addEventTimer
		(iport, ANNOUNCE_RECEIPT_TIMEOUT_EXPIRES,
		 (unsigned long long)
		 (ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER *
		  (pow
		   ((double)2, iport->getAnnounceInterval()) *
		   1000000000.0)));

	return true;
}

bool PTPMessageSync::processMessage( MediaDependentEtherPort * port )
{
	MediaIndependentPort *iport = port->getPort();
	bool ret = true;

	PTPMessageSync *old_sync;
	Timestamp system_time;
	Timestamp device_time;
	Timestamp corrected_sync_time;

	// Expire any SYNC_RECEIPT timers that exist
	if (iport->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored when in this state
		ret = false;
		goto done;
	}
	if (iport->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		if( !port->recoverPort() ) {
			ret = false;
			goto done;
		}
	}

	iport->stopSyncReceiptTimeout();

	XPTPD_INFO("PTP assist flag is not set, FLAGS[0,1] = %u,%u", flags[0],
			   flags[1]);

	// Ignore PTP_ASSIST flag for 802.1AS
	old_sync = port->getLastSync();
	if (old_sync != NULL) {
		delete old_sync;
	}
	port->setLastSync(this);
	_gc = false;

	iport->startSyncReceiptTimeout();

 done:
	return ret;
}

PTPMessageFollowUp::PTPMessageFollowUp
( MediaDependentPort *port ) : PTPMessageEther( port )
{
	messageType = FOLLOWUP_MESSAGE;
	control = FOLLOWUP;

	logMeanMessageInterval = port->getPort()->getSyncInterval();

	return;
}

net_result PTPMessageFollowUp::sendPort( MediaDependentEtherPort * port )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	memset(buf_t, 0, 256);

	buildMessage(buf_ptr);

	return port->sendGeneralPort( buf_t, messageLength, MCAST_OTHER );
}

size_t PTPMessageFollowUp::buildMessage(uint8_t *buf_ptr)
{
	unsigned char tspec_msg_t = 0x0;
	Timestamp preciseOriginTimestamp_BE;
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

	memcpy(buf_ptr + PTP_FOLLOWUP_SEC_MS(PTP_FOLLOWUP_OFFSET),
		&(preciseOriginTimestamp_BE.seconds_ms),
		sizeof(preciseOriginTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_FOLLOWUP_SEC_LS(PTP_FOLLOWUP_OFFSET),
		&(preciseOriginTimestamp_BE.seconds_ls),
		sizeof(preciseOriginTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_FOLLOWUP_NSEC(PTP_FOLLOWUP_OFFSET),
		&(preciseOriginTimestamp_BE.nanoseconds),
		sizeof(preciseOriginTimestamp.nanoseconds));
	tlv.toByteString(buf_ptr + PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH);

	XPTPD_INFO
		("Follow-Up Time: %u seconds(hi)", preciseOriginTimestamp.seconds_ms);
	XPTPD_INFO
		("Follow-Up Time: %u seconds", preciseOriginTimestamp.seconds_ls);
	XPTPD_INFO
		("FW-UP Time: %u nanoseconds", preciseOriginTimestamp.nanoseconds);
	XPTPD_INFO
		("FW-UP Time: %x seconds", preciseOriginTimestamp.seconds_ls);
	XPTPD_INFO
		("FW-UP Time: %x nanoseconds", preciseOriginTimestamp.nanoseconds);

	return PTP_COMMON_HDR_LENGTH + PTP_FOLLOWUP_LENGTH + sizeof(tlv);
}

bool PTPMessageFollowUp::processMessage(MediaDependentPort *port, Timestamp receipt, uint64_t delay) {
	MediaIndependentPort *iport = port->getPort();
	IEEE1588Clock *clock = port->getPort()->getClock();
	Timestamp system_time, device_time;
	uint32_t device_sync_time_offset;
	Timestamp corrected_sync_time;
	clock_offset_t offset;
	int correction;

	memset(&offset, 0, sizeof(offset));
	port->getDeviceTime(system_time, device_time);

	offset.ls_freqoffset = port->getLocalSystemRateOffset();

	offset.ml_freqoffset = tlv.getRateOffset();
	offset.ml_freqoffset /= 2ULL << 41;
	offset.ml_freqoffset += 1.0;
	offset.ml_freqoffset /= iport->getPeerRateOffset();

	correctionField = (uint64_t)
		((correctionField >> 16) / offset.ml_freqoffset);
	correction = (int)(delay + correctionField);
	corrected_sync_time = receipt;

	if (correction > 0)
		corrected_sync_time -  correction;
	else
		corrected_sync_time + -correction;

	/* Adjust local_clock to correspond to sync_arrival */
	device_sync_time_offset =
		TIMESTAMP_TO_NS(device_time - corrected_sync_time);

	system_time - (uint64_t)
		(((FrequencyRatio)device_sync_time_offset) /
			offset.ls_freqoffset);


	offset.ml_phoffset =
		corrected_sync_time > preciseOriginTimestamp ?
		TIMESTAMP_TO_NS(corrected_sync_time - preciseOriginTimestamp) :
		-(TIMESTAMP_TO_NS(preciseOriginTimestamp - corrected_sync_time));


	offset.ls_phoffset =
		system_time > corrected_sync_time ?
		TIMESTAMP_TO_NS(system_time - corrected_sync_time) :
		-(TIMESTAMP_TO_NS(corrected_sync_time - system_time));

	offset.master_time = TIMESTAMP_TO_NS(preciseOriginTimestamp);
	offset.sync_count = iport->getPdelayCount();
	offset.pdelay_count = iport->getSyncCount();

	port->getPort()->getClock()->setMasterOffset(&offset);

	if (iport->doSyntonize()) {
		iport->adjustPhaseError
			(offset.ml_phoffset, offset.ml_freqoffset);
	}

	clock->setLastSyncOriginTime(preciseOriginTimestamp);
	clock->setLastSyncReceiveSystemTime(system_time);
	clock->setLastSyncReceiveDeviceTime(receipt);
	clock->setLastSyncCumulativeOffset
		(offset.ml_freqoffset);
	clock->setLastSyncReceiveDeviceId
		(port->getTimestampDeviceId());
	clock->setLastSyncValid();

	iport->syncDone();

	return true;
}

bool PTPMessageFollowUp::processMessage(MediaDependentEtherPort *port)
{
	MediaIndependentPort *iport = port->getPort();
	bool ret = true;

	Timestamp sync_arrival;
	uint64_t delay;
	PortIdentity sync_id;
	PTPMessageSync *sync;

	XPTPD_INFO("Processing a follow-up message");

	if ( iport->getPortState() == PTP_DISABLED ) {
		// Do nothing Sync messages should be ignored when in this state
		ret = false;
		goto done;
	}
	if ( iport->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		if( !iport->recoverPort()) {
			ret = false;
			goto done;
		}
	}

	sync = port->getLastSync();
	if (sync == NULL) {
		XPTPD_ERROR("Received Follow Up but there is no sync message");
		goto done;
	}
	sync->getPortIdentity(&sync_id);

	if (sync->getSequenceId() != sequenceId || sync_id != *sourcePortIdentity)
		{
			XPTPD_ERROR
				("Received Follow Up but cannot find corresponding Sync");
			goto done;
		}

	if( iport->getPortState() == PTP_SLAVE ) {
		iport->incSyncCount();
		sync_arrival = sync->getTimestamp();

		delay = port->getLinkDelay();
		if ((delay = port->getLinkDelay()) == 3600000000000) {
			ret = false;
			goto done;
		}

		ret = processMessage(port, sync_arrival,delay);
	}

 done:
	_gc = true;
	port->setLastSync(NULL);
	delete sync;
	
	return ret;
}



PTPMessagePathDelayReq::PTPMessagePathDelayReq(MediaDependentPort * port):
	PTPMessageEther(port)
{
	logMeanMessageInterval = 0;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_REQ_MESSAGE;
	return;
}

bool PTPMessagePathDelayReq::processMessage( MediaDependentEtherPort * port )
{
	MediaIndependentPort *iport = port->getPort();
	bool ret = true;
	OSTimer *timer = port->getTimerFactory()->createTimer();
	PortIdentity resp_fwup_id;

	PTPMessagePathDelayResp *resp;
	PTPMessagePathDelayRespFollowUp *resp_fwup;

	int ts_good;
	Timestamp resp_timestamp;
	PortIdentity resp_id;
	PortIdentity req_id;
	long wait_time;

	if (iport->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		ret = false;
		goto done;
	}

	if (iport->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		if( !iport->recoverPort() ) {
			ret = false;
			goto done;
		}
	}

	/* Generate and send message */
	resp = new PTPMessagePathDelayResp(port);
	port->getPort()->getPortIdentity(resp_id);
	resp->setPortIdentity(&resp_id);
	resp->setRequestingPortIdentity( sourcePortIdentity );
	
	resp->setSequenceId(sequenceId);

	XPTPD_INFO("Process PDelay Request SeqId: %u\t", sequenceId);

	resp->setRequestReceiptTimestamp(_timestamp);
	if( port->getTimestampVersion() != _timestamp._version ) {
		delete resp;
		ret = false;
		goto done;
	}
	port->getTxLock();
	resp->sendPort(port);

	XPTPD_INFO("Sent path delay response");

	XPTPD_INFO("Start TS Read");

	wait_time = TX_TIMEOUT_BASE;
	ts_good = port->getTxTimestampRetry
		( resp, resp_timestamp, TX_TIMEOUT_ITER, &wait_time );

	port->putTxLock();

	if (!ts_good) {
		XPTPD_ERROR
			( "Error (TX) timestamping PDelay Response, error=%d\t",
			  ts_good );
		delete resp;
		ret = false;
		goto done;
	}

	resp_fwup = new PTPMessagePathDelayRespFollowUp(port);
	port->getPort()->getPortIdentity(resp_fwup_id);
	resp_fwup->setPortIdentity(&resp_fwup_id);
	resp_fwup->setSequenceId(sequenceId);
	resp_fwup->setRequestingPortIdentity(sourcePortIdentity);
	resp_fwup->setResponseOriginTimestamp(resp_timestamp);
	long long turnaround;
	turnaround =
	    (resp_timestamp.seconds_ls - _timestamp.seconds_ls) * 1000000000LL;

	XPTPD_INFO("Response Depart(sec): %u", resp_timestamp.seconds_ls);
	XPTPD_INFO("Request Arrival(sec): %u", _timestamp.seconds_ls);
	XPTPD_INFO("#1 Correction Field: %Ld", turnaround);
	
	turnaround += resp_timestamp.nanoseconds;

	XPTPD_INFO("#2 Correction Field: %Ld", turnaround);

	turnaround -= _timestamp.nanoseconds;

	XPTPD_INFO("#3 Correction Field: %Ld", turnaround);

	resp_fwup->setCorrectionField(0);
	resp_fwup->sendPort(port);

	XPTPD_INFO("Sent path delay response fwup");

	delete resp;
	delete resp_fwup;

 done:
	delete timer;
	_gc = true;

	return ret;
}

net_result PTPMessagePathDelayReq::sendPort( MediaDependentEtherPort *port )
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0;
	memset(buf_t, 0, 256);
	/* Create packet in buf */
	/* Copy in common header */
	messageLength = PTP_COMMON_HDR_LENGTH + PTP_PDELAY_REQ_LENGTH;
	tspec_msg_t |= messageType & 0xF;
	buildCommonHeader(buf_ptr);

	return port->sendEventPort( buf_t, messageLength, MCAST_PDELAY );
}

PTPMessagePathDelayResp::PTPMessagePathDelayResp
(MediaDependentEtherPort *port) : PTPMessageEther(port)
{
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

bool PTPMessagePathDelayResp::processMessage( MediaDependentEtherPort * port )
{
	MediaIndependentPort *iport = port->getPort();
	IEEE1588Clock *clock = port->getPort()->getClock();

	if (iport->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		return false;
	}
	if (iport->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return false;
	}
	
	if (port->tryPDelayRxLock() != true) {
		XPTPD_ERROR("Failed to get PDelay RX Lock");
		return false;
	}

	clock->deleteEventTimer
		(iport, PDELAY_RESP_RECEIPT_TIMEOUT_EXPIRES);
	PTPMessagePathDelayResp *old_pdelay_resp = port->getLastPDelayResp();
	if (old_pdelay_resp != NULL) {
		delete old_pdelay_resp;
	}
	port->setLastPDelayResp(this);

	port->putPDelayRxLock();
	_gc = false;

	return true;
}

net_result PTPMessagePathDelayResp::sendPort( MediaDependentEtherPort * port )
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

	XPTPD_INFO("PDelay Resp Timestamp: %u,%u",
			   requestReceiptTimestamp.seconds_ls,
			   requestReceiptTimestamp.nanoseconds);

	return port->sendEventPort(buf_t, messageLength, MCAST_PDELAY );
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
(MediaDependentEtherPort * port) : PTPMessageEther (port)
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

bool PTPMessagePathDelayRespFollowUp::processMessage
(MediaDependentEtherPort * port)
{
	MediaIndependentPort *iport = port->getPort();
	IEEE1588Clock *clock = port->getPort()->getClock();
	bool ret = true;

	PTPMessagePathDelayReq *req;
	PTPMessagePathDelayResp *resp;

	PortIdentity req_id;
	PortIdentity resp_id;

	Timestamp remote_resp_tx_timestamp(0, 0, 0);
	Timestamp request_tx_timestamp(0, 0, 0);
	Timestamp remote_req_rx_timestamp(0, 0, 0);
	Timestamp response_rx_timestamp(0, 0, 0);

	if (iport->getPortState() == PTP_DISABLED) {
		// Do nothing all messages should be ignored when in this state
		return false;
	}
	if (iport->getPortState() == PTP_FAULTY) {
		// According to spec recovery is implementation specific
		port->recoverPort();
		return false;
	}

	if (port->tryPDelayRxLock() != true)
		return false;

	req = port->getLastPDelayReq();
	resp = port->getLastPDelayResp();

	if (req == NULL) {
		/* Shouldn't happen */
		ret = false;
		goto abort;
	}

	if (resp == NULL) {
		/* Probably shouldn't happen either */
		ret = false;
		goto abort;
	}

	req->getPortIdentity(&req_id);
	resp->getRequestingPortIdentity(&resp_id);

	if (req->getSequenceId() != sequenceId) {
		ret = false;
		goto abort;
	}

	// Check if we have received a response
	if( resp->getSequenceId() != sequenceId
	    || resp_id != *requestingPortIdentity ) {
		uint16_t resp_port_number;
		uint16_t req_port_number;
		resp_id.getPortNumber(&resp_port_number);
		requestingPortIdentity->getPortNumber(&req_port_number);
		XPTPD_ERROR
		    ("Received PDelay Response Follow Up but cannot find "
			 "corresponding response");
		XPTPD_ERROR("%hu, %hu, %hu, %hu", resp->getSequenceId(),
					sequenceId, resp_port_number, req_port_number);

		ret = false;
		goto abort;
	}

	XPTPD_INFO("Request Sequence Id: %u", req->getSequenceId());
	XPTPD_INFO("Response Sequence Id: %u", resp->getSequenceId());
	XPTPD_INFO("Follow-Up Sequence Id: %u", req->getSequenceId());

	int64_t link_delay;
	unsigned long long turn_around;

	/* Assume that we are a two step clock, otherwise originTimestamp 
	   may be used */
	request_tx_timestamp = req->getTimestamp();
	if( request_tx_timestamp.nanoseconds == INVALID_TIMESTAMP.nanoseconds ) {
		/* Stop processing the packet */
		ret = false;
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
		clock->addEventTimer
			(iport, PDELAY_DEFERRED_PROCESSING, 1000000);
		ret = true;
		goto defer;
	}

	remote_req_rx_timestamp = resp->getRequestReceiptTimestamp();
	response_rx_timestamp = resp->getTimestamp();
	remote_resp_tx_timestamp = responseOriginTimestamp;

	if( request_tx_timestamp._version != response_rx_timestamp._version ) {
		ret = false;
		goto abort;
	}

	iport->incPdelayCount();

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
	if 
		( iport->getPeerRateOffset() > .998 &&
		  iport->getPeerRateOffset() < 1.002 ) {
		turn_around = (int64_t) (turn_around * iport->getPeerRateOffset());
	}

	XPTPD_INFO
		("Turn Around Adjustment %Lf",
		 ((long long)turn_around * iport->getPeerRateOffset()) /
		 1000000000000LL);
	XPTPD_INFO
		("Step #1: Turn Around Adjustment %Lf",
		 ((long long)turn_around * iport->getPeerRateOffset()));
	XPTPD_INFO("Adjusted Peer turn around is %Lu", turn_around);

	/* Subtract turn-around time from link delay after rate adjustment */
	link_delay -= turn_around;
	link_delay /= 2;

	{
		uint64_t mine_elapsed;
	    uint64_t theirs_elapsed;
	    Timestamp prev_peer_ts_mine;
	    Timestamp prev_peer_ts_theirs;
	    FrequencyRatio rate_offset;
	    if( iport->getPeerOffset( prev_peer_ts_mine, prev_peer_ts_theirs )) {
			mine_elapsed =  TIMESTAMP_TO_NS
				(request_tx_timestamp)-TIMESTAMP_TO_NS(prev_peer_ts_mine);
			theirs_elapsed = TIMESTAMP_TO_NS
				(remote_req_rx_timestamp)-TIMESTAMP_TO_NS(prev_peer_ts_theirs);
			theirs_elapsed -= port->getLinkDelay();
			theirs_elapsed += link_delay;
			rate_offset =  ((FrequencyRatio) mine_elapsed)/theirs_elapsed;
			iport->setPeerRateOffset(rate_offset);
			XPTPD_INFOL( PDELAY_DEBUG, "Peer Rate Ratio: %Lf", rate_offset );
			port->getPort()->setAsCapable();
		}
	}
	port->setLinkDelay( link_delay );
	iport->setPeerOffset( request_tx_timestamp, remote_req_rx_timestamp );

	XPTPD_INFOL(PDELAY_DEBUG, "Link Delay: %lld", (long long)link_delay);

 abort:
	delete req;
	port->setLastPDelayReq(NULL);
	delete resp;
	port->setLastPDelayResp(NULL);

	_gc = true;

 defer:
	port->putPDelayRxLock();

	return ret;
}

net_result PTPMessagePathDelayRespFollowUp::sendPort
(MediaDependentEtherPort * port)
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

	XPTPD_INFO("PDelay Resp Timestamp: %u,%u",
			   responseOriginTimestamp.seconds_ls,
			   responseOriginTimestamp.nanoseconds);

	return port->sendGeneralPort( buf_t, messageLength, MCAST_OTHER );
}

void PTPMessagePathDelayRespFollowUp::setRequestingPortIdentity
(PortIdentity * identity)
{
	*requestingPortIdentity = *identity;
}
