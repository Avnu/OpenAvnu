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
#include <avbts_port.hpp>
#include <avbts_ostimer.hpp>

#include <cinttypes>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <math.h>
#include <sstream>

#include <chrono>

using namespace std::chrono;

steady_clock::time_point lastSync = steady_clock::now();

PTPMessageCommon::PTPMessageCommon(IEEE1588Port * port)
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

	sourcePortIdentity = std::make_shared<PortIdentity>();
	sequenceId = 0;

	return;
}

PTPMessageCommon&
PTPMessageCommon::assign(const PTPMessageCommon& other)
{
	if (this != &other)
	{
		versionPTP = other.versionPTP;
		versionNetwork = other.versionNetwork;
		messageType = other.messageType;

		GPTP_LOG_VERBOSE("PTPMessageComon::assign   other.sourcePortIdentity == nullptr: %s", (other.sourcePortIdentity == nullptr ? 
			"true" : "false"));
		sourcePortIdentity = nullptr == other.sourcePortIdentity
		 ? std::make_shared<PortIdentity>() : other.sourcePortIdentity;

		sequenceId = other.sequenceId;
		control = other.control;
		memcpy(flags, other.flags, sizeof(flags));
		messageLength = other.messageLength;
		logMeanMessageInterval = other.logMeanMessageInterval;
		correctionField = other.correctionField;
		domainNumber = other.domainNumber;

		_timestamp = other._timestamp;
		_timestamp_counter_value = other._timestamp_counter_value;
		_gc = other._gc;
	}
	return *this;
}

/* Determine whether the message was sent by given communication technology,
   uuid, and port id fields */
bool PTPMessageCommon::isSenderEqual(const PortIdentity& portIdentity)
{
	return portIdentity == *sourcePortIdentity;
}
 
void PTPMessageCommon::MaybePerformCalculations(IEEE1588Port *port)
{
	if (V2_E2E == port->getDelayMechanism())
	{
		GPTP_LOG_DEBUG("*** MaybePerformCalculations--------------------------------------");

		if (PTP_SLAVE == port->getPortState()) 
		{
			std::shared_ptr<PortIdentity> respPortIdentity;
			port->getPortIdentity(respPortIdentity);

			GPTP_LOG_DEBUG("MaybePerformCalculations Processing as slave.");
			PTPMessageSync *sync = port->getLastSync();
			PTPMessageDelayReq req = port->getLastDelayReq();
			PTPMessageDelayResp resp = port->getLastDelayResp();
			PTPMessageFollowUp fup = port->getLastFollowUp();
			ALastTimestampKeeper tsKeeper = port->LastTimestamps();

			if (sync)
			{
				// Ensure our last values are from the same "group" of 
				// sync/followup/delay_req/delay_resp messages from the
				// same clock.
				uint16_t syncSeqId = sync->getSequenceId();
				std::shared_ptr<PortIdentity> syncPortId = sync->getPortIdentity();
				std::shared_ptr<PortIdentity> slavePortId = std::make_shared<PortIdentity>();
				port->getPortIdentity(slavePortId);
				
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   sync clockId:%s", sync->getPortIdentity()->getClockIdentity().getIdentityString().c_str());
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   req clockId:%s", port->getLastDelayReq().getPortIdentity()->getClockIdentity().getIdentityString().c_str());
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   fup clockId:%s", port->getLastFollowUp().getPortIdentity()->getClockIdentity().getIdentityString().c_str());
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   slavePortId clockId:%s", slavePortId->getClockIdentity().getIdentityString().c_str());
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   port->getPortIdentity clockId:%s", port->getPortIdentity()->getClockIdentity().getIdentityString().c_str());
				
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   req.getSequenceId():%d", req.getSequenceId());
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   resp.getSequenceId():%d", resp.getSequenceId());					
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   fup.getSequenceId():%d", fup.getSequenceId());

				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   resp check - - - -");
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations  synPortId.sameClocks(resp): %s", (syncPortId->sameClocks(resp.getPortIdentity()) ? "true":"false"));
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   fup check - - - -");
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations  synPortId.sameClocks(fup): %s", (syncPortId->sameClocks(fup.getPortIdentity()) ? "true":"false"));
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations   req check - - - -");
				// GPTP_LOG_VERBOSE("PTPMessageCommon::MaybePerformCalculations  slavePortId.sameClocks(req): %s", (port->getPortIdentity()->sameClocks(req.getPortIdentity()) ? "true":"false"));
				
				uint16_t reqSeqId = req.getSequenceId();
				uint16_t respSeqId = resp.getSequenceId();
				uint16_t fupSeqId = fup.getSequenceId();

				if (syncSeqId == reqSeqId &&
				 syncSeqId == respSeqId && 
				 syncSeqId == fupSeqId &&
				 port->getPortIdentity()->sameClocks(req.getPortIdentity()) &&
				 syncPortId->sameClocks(resp.getPortIdentity()) &&
				 syncPortId->sameClocks(fup.getPortIdentity()))
				{
					uint64_t t1 = TIMESTAMP_TO_NS(fup.getPreciseOriginTimestamp());
					//uint64_t rawt1 = t1;
					uint64_t t2 = TIMESTAMP_TO_NS(sync->getTimestamp()); 
					uint64_t t3 = TIMESTAMP_TO_NS(req.getTimestamp());
					uint64_t t4 = TIMESTAMP_TO_NS(resp.getRequestReceiptTimestamp());
					//uint64_t rawt4 = t4;

               uint64_t fupCorrection = fup.getCorrectionField();
               uint64_t respCorrection = resp.getCorrectionField();

               // The correction fileds are in 2^16 so correct them by shifting
               // right 16 bits and ignore the fractional part
               fupCorrection >>= 16;
               respCorrection >>= 16;

               // Adjust t1 and t4 by the correction field
               t1 += fupCorrection;
               t4 += respCorrection;

					// std::cout << "MasterOffset:" << port->MasterOffset() << std::endl;
					// std::cout << "raw t1:" << t1 << std::endl;
					// std::cout << "raw t2:" << t2 << std::endl;
					// std::cout << "raw t3:" << t3 << std::endl;
					// std::cout << "raw t4:" << t4 << std::endl;

					uint64_t lt1 = tsKeeper.fT1;
					uint64_t lt2 = tsKeeper.fT2;
					uint64_t lt3 = tsKeeper.fT3;
					uint64_t lt4 = tsKeeper.fT4;

					typedef FrequencyRatio FR;

					FR localtime;
					FR remote;
					
					// Ensure that  t4 >= t1 and t3 >= t2 
					if (t4 >= t1 && t3 >= t2)
					{
						GPTP_LOG_VERBOSE("fupCorrection: %" PRIu64, fupCorrection);
						GPTP_LOG_VERBOSE("respCorrection: %" PRIu64, respCorrection);

						GPTP_LOG_VERBOSE("t1: %" PRIu64, t1);
						//GPTP_LOG_VERBOSE("rawt1: %" PRIu64, rawt1);
						GPTP_LOG_VERBOSE("t2: %" PRIu64, t2);
						GPTP_LOG_VERBOSE("t3: %" PRIu64, t3);
						GPTP_LOG_VERBOSE("t4: %" PRIu64, t4);
						//GPTP_LOG_VERBOSE("rawt4: %" PRIu64, rawt4);

						int64_t check = ((t4-t1) - (t3-t2))/2;
						GPTP_LOG_VERBOSE("check: %" PRIu64, check);
						GPTP_LOG_VERBOSE("(t4-t1): %" PRIu64, (t4-t1));
						GPTP_LOG_VERBOSE("(t3-t2): %" PRIu64, (t3-t2));
						GPTP_LOG_VERBOSE("(t4-t1) - (t3-t2): %" PRIu64, (t4-t1) - (t3-t2));
						GPTP_LOG_VERBOSE("((t4-t1) - (t3-t2))/2: %" PRIu64, ((t4-t1) - (t3-t2)) / 2);

						GPTP_LOG_VERBOSE("correctionFieldSync: %" PRIu64, sync->getCorrectionField());
						GPTP_LOG_VERBOSE("correctionFieldFollowup: %" PRIu64, fup.getCorrectionField());
						GPTP_LOG_VERBOSE("correctionFileDelayResp: %" PRIu64, resp.getCorrectionField());

						// check is <= 80 ms
						if (check <= 80000000)
						{
							GPTP_LOG_VERBOSE("$$$$$$$$$$$$$$$$$$$$$$$$$$  check passed!!!!!!!!!!!!!!!!!!!!!!!!!!!!!  goodSyncCount:%d", port->GoodSyncCount());
							// Do this for the 2nd successful sync that has a less than
							// check value
							if (port->GoodSyncCount() > 1)
							{
								localtime = FR(t2 + t3) / 2;
								remote = FR(t4 + t1) / 2;

								FR difflocaltime = localtime - port->LastLocaltime();
								FR diffRemote = remote - port->LastRemote();

								// Add additional checking 
								FR remoteDiffLimit = diffRemote / 100;
								FR remoteDiffUpper = diffRemote + remoteDiffLimit;
								FR remoteDiffLower = diffRemote - remoteDiffLimit;

								GPTP_LOG_VERBOSE("badDiffCount: %d", port->BadDiffCount());
								GPTP_LOG_VERBOSE("difflocaltime: %Le", difflocaltime);
								GPTP_LOG_VERBOSE("remoteDiffUpper: %Le", remoteDiffUpper);
								GPTP_LOG_VERBOSE("remoteDiffLower: %Le", remoteDiffLower);


								// Limit the precision of our floating point values
								int kLimit = 100;
								difflocaltime = ceil(difflocaltime * kLimit) / kLimit;
								remoteDiffUpper = ceil(remoteDiffUpper * kLimit) / kLimit;
								remoteDiffLower = ceil(remoteDiffLower * kLimit) / kLimit;

								// Limit to 5 bad diffs otherwise we loose too many
								if ((difflocaltime <= remoteDiffUpper &&
									difflocaltime >= remoteDiffLower) ||
									port->BadDiffCount() > 5)
								{
									port->BadDiffCount(0);
									GPTP_LOG_VERBOSE("Setting badDiffCount  0");

									FR RR1 = 1.0;
									FR RR2 = 1.0;

									//FrequencyRatio RR1 = FrequencyRatio(t2 + t3) / 2.0;
									//FrequencyRatio RR2 = FrequencyRatio(t1 + t4) / 2.0;

									//(((t4 - t1) / 2) - ((lt4 - lt1) / 2)) / (((t3 - t2) / 2) - ((lt3 - lt2) / 2))
									if (lt1 > 0 && lt2 > 0 && lt3 > 0 && lt4 > 0)
									{
										FR denom = (FR(t3-t2)/FR(2.0)) - (FR(lt3-lt2)/FR(2.0));
										RR1 = 0 == denom 
										 ? 1 : ((FR(t4-t1)/FR(2.0)) - (FR(lt4-lt1)/FR(2.0))) / denom;
										denom = (FR(t4-t1)/FR(2.0)) - (FR(lt4-lt1)/FR(2.0));
										RR2 = 0 == denom
										 ? 1 : ((FR(t3-t2)/FR(2.0)) - (FR(lt3-lt2)/FR(2.0))) / denom;
									}

									// FrequencyRatio RR1 = (FrequencyRatio(t4-t1)/2) / (FrequencyRatio(t3-t2)/2);
									// FrequencyRatio RR2 = FrequencyRatio(t3-t2) / FrequencyRatio(t4-t1);

									FrequencyRatio meanPathDelay;
									if (port->SmoothRateChange())
									{
										FR lastRateRatio = port->LastFilteredRateRatio();
										// We will use an IIR to smooth the rate ratio. The 
										// IIR will differ slightly depending on the physical 
										// network (wifi vs wired).
										FR filteredRateRatioMS = port->IsWireless()
										 ? (FR(lastRateRatio * 255) / 256) + (RR2 / 256)
										 : (FR(lastRateRatio * 63) / 64) + (RR2 / 64);

										port->LastFilteredRateRatio(filteredRateRatioMS);

										meanPathDelay = ((t4-t1) - filteredRateRatioMS * (t3-t2)) / 2.0;						
										GPTP_LOG_INFO("lastRateRatio: %Le", lastRateRatio);
										GPTP_LOG_INFO("filteredRateRatioMS: %Le", filteredRateRatioMS);
									}
									else
									{
										meanPathDelay = ((t4-t1) - RR2 * (t3-t2)) / 2.0;
									}

									// Apply IIR filter to the meanPathDelay differing
									// slightly depending on the physical network.
									FR lastMeanPathDelay = port->meanPathDelay();
									FR filteredMeanPathDelay = port->IsWireless()
									 ? (FR(lastMeanPathDelay * 255) / 256) + (meanPathDelay / 256)
									 : (FR(lastMeanPathDelay * 63) / 64) + (meanPathDelay / 64);

									//port->MasterOffset(offset);
									port->meanPathDelay(filteredMeanPathDelay);

									port->setLinkDelay(filteredMeanPathDelay);

									port->PushMasterSlaveRateRatio(RR1);
									port->PushSlaveMasterRateRatio(RR2);

									GPTP_LOG_VERBOSE("lt1: %" PRIu64, lt1);
									GPTP_LOG_VERBOSE("lt2: %" PRIu64, lt2);
									GPTP_LOG_VERBOSE("lt3: %" PRIu64, lt3);
									GPTP_LOG_VERBOSE("lt4: %" PRIu64, lt4);

									GPTP_LOG_VERBOSE("RR1((((t4-t1)/2) - ((lt4-lt1)/2)) / (((t3-t2)/2) - ((lt3-lt2)/2))): %Le", RR1);
									GPTP_LOG_VERBOSE("RR2((((t3-t2)/2) - ((lt3-lt2)/2)) / (((t4-t1)/2) - ((lt4-lt1)/2))): %Le", RR2);
									GPTP_LOG_VERBOSE("meanPathDelay: %Le", meanPathDelay);							
									GPTP_LOG_VERBOSE("filteredMeanPathDelay: %Le", filteredMeanPathDelay);							
									//GPTP_LOG_VERBOSE("offset((t4 - t1 - t3 + t2)/2): %Le", offset);							
									GPTP_LOG_VERBOSE("correctionFieldSync: %" PRIu64, sync->getCorrectionField());
									GPTP_LOG_VERBOSE("correctionFieldFollowup: %" PRIu64, fup.getCorrectionField());
									GPTP_LOG_VERBOSE("correctionFileDelayResp: %" PRIu64, resp.getCorrectionField());						

									std::cout << "RR1((((t4-t1)/2) - ((lt4-lt1)/2)) / (((t3-t2)/2) - ((lt3-lt2)/2))):" << RR1 << std::endl;
									std::cout << "RR2((((t3-t2)/2) - ((lt3-lt2)/2)) / (((t4-t1)/2) - ((lt4-lt1)/2))):" << RR2 << std::endl;						
								}
								else
								{
									port->BadDiffCount(port->BadDiffCount() + 1);
								}
							}

							port->LastLocaltime(localtime);
							port->LastRemote(remote);
							if (port->GoodSyncCount() < 2)
							{
								port->GoodSyncCount(port->GoodSyncCount() + 1);
							}							
						}
						port->LastTimestamps(ALastTimestampKeeper(t1, t2, t3, t4));
					}
					else
					{
						GPTP_LOG_INFO("t1: %" PRIu64, t1);
						GPTP_LOG_INFO("t2: %" PRIu64, t2);
						GPTP_LOG_INFO("t3: %" PRIu64, t3);
						GPTP_LOG_INFO("t4: %" PRIu64, t4);

						GPTP_LOG_INFO("t4 > t1 && t3 > t2  IS FALSE");
					}
				}
			}
		}
	}	
}

PTPMessageCommon *buildPTPMessage
(char *buf, int size, LinkLayerAddress * remote, IEEE1588Port * port, 
 const Timestamp& ingressTime)
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	PTPMessageCommon *msg = NULL;
	PTPMessageId messageId;
	MessageType messageType;
	unsigned char tspec_msg_t = 0;
	unsigned char transportSpecific = 0;

	uint16_t sequenceId;
	std::shared_ptr<PortIdentity> sourcePortIdentity;
	Timestamp timestamp(0, 0, 0);
	unsigned counter_value = 0;

#if PTP_DEBUG
	{
		std::stringstream ss;
		int i;
		GPTP_LOG_VERBOSE("Packet Dump:\n");
		ss << "\n";
		for (i = 0; i < size; ++i) {
			ss << std::hex << int(buf[i]) << "\t";
			//GPTP_LOG_VERBOSE("%hhx\t", buf[i]);
			if (i > 0 &&  i % 8 == 0)
				//GPTP_LOG_VERBOSE("\n");
				ss << "\n";
		}
		ss << "\n";
		GPTP_LOG_VERBOSE("%s", ss.str().c_str());
		//if (i % 8 != 0)
		//	GPTP_LOG_VERBOSE("\n");
	}
#endif

	memcpy(&tspec_msg_t,
	       buf + PTP_COMMON_HDR_TRANSSPEC_MSGTYPE(PTP_COMMON_HDR_OFFSET),
	       sizeof(tspec_msg_t));
	messageType = (MessageType) (tspec_msg_t & 0xF);
	GPTP_LOG_VERBOSE("RAW messageType:%d", messageType);
	transportSpecific = (tspec_msg_t >> 4) & 0x0F;

	sourcePortIdentity = std::make_shared<PortIdentity>(
		(uint8_t *)(buf + PTP_COMMON_HDR_SOURCE_CLOCK_ID(PTP_COMMON_HDR_OFFSET)),
		(uint16_t *)(buf + PTP_COMMON_HDR_SOURCE_PORT_ID(PTP_COMMON_HDR_OFFSET)));

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
			    ("*** Received an event packet but cannot retrieve timestamp, "
			    	"discarding. messageType=%u,error=%d\n%s",
			     messageType, ts_good, msg);
			//_exit(-1);
			goto abort;
		}

		else {
			GPTP_LOG_VERBOSE("Timestamping event packet");
		}

	}

	if (1 != transportSpecific) {
		GPTP_LOG_EXCEPTION("*** Received message with unsupported "
		 "transportSpecific type=%d", transportSpecific);
		goto abort;
	}
 
	switch (messageType) {
	case SYNC_MESSAGE:
	{
		GPTP_LOG_DEBUG("*** Received Sync message: %d", sequenceId);
		// port->getRxTimestamp(sourcePortIdentity, messageId, 
		// 	timestamp, counter_value, false);
		timestamp = ingressTime;
		GPTP_LOG_VERBOSE("Sync RX timestamp = %hu,%u,%u", timestamp.seconds_ms, timestamp.seconds_ls, timestamp.nanoseconds );

		steady_clock::time_point curSync = steady_clock::now();	
		duration<double> timeSpan = duration_cast<duration<double>>(curSync - lastSync);
		lastSync = curSync;
		GPTP_LOG_INFO("Sync delta: %f", timeSpan.count());

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

			memcpy(&(sync_msg->correctionField),
	       buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       sizeof(sync_msg->correctionField));

			msg = sync_msg;

			GPTP_LOG_VERBOSE("Received sync  originTimestamp:%d", sync_msg->originTimestamp.seconds_ms);
			GPTP_LOG_VERBOSE("Received sync  correctionField:%d", sync_msg->correctionField);
		}
	}
	break;

	case FOLLOWUP_MESSAGE:

		GPTP_LOG_DEBUG("*** Received Follow Up message: %d", sequenceId);

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

			followup_msg->sequenceId = sequenceId;

			memcpy(&(followup_msg->correctionField),
	       buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       sizeof(followup_msg->correctionField));

			msg = followup_msg;
			msg->setPortIdentity(sourcePortIdentity);

			port->setLastFollowUp(static_cast<PTPMessageFollowUp*>(msg));
			port->HaveFollowup(true);

			GPTP_LOG_VERBOSE("Received Follow Up  precideOriginTimestamp:%d", followup_msg->preciseOriginTimestamp.seconds_ms);
			GPTP_LOG_VERBOSE("Received Follow Up  correctionField:%d", followup_msg->correctionField);
			GPTP_LOG_VERBOSE("Received Follow Up  clockId:%s",msg->getPortIdentity()->getClockIdentity().getIdentityString().c_str());
        }
		break;

	case DELAY_REQ_MESSAGE:
		GPTP_LOG_DEBUG("*** Received DELAY_REQ message: %d", sequenceId);
		{		
			// Be sure buffer is the correction size
			if (size < PTP_COMMON_HDR_LENGTH + PTP_DELAY_REQ_LENGTH) {
				GPTP_LOG_ERROR("DELAY_REQ size is invalid.  size(%d) < %d", size,
				 PTP_COMMON_HDR_LENGTH + PTP_DELAY_REQ_LENGTH);
				goto abort;
			}

			timestamp = ingressTime;
			std::shared_ptr<PortIdentity> reqPortId = port->getPortIdentity();
			msg = new PTPMessageDelayReq(messageType);
			msg->setPortIdentity(reqPortId);
		}
		break;

	case DELAY_RESP_MESSAGE:
		GPTP_LOG_DEBUG("*** Received DELAY_RESP message ==================================: %d", sequenceId);
		{
			// Be sure buffer is the correction size
			if (size < PTP_COMMON_HDR_LENGTH + PTP_DELAY_RESP_LENGTH) {
				GPTP_LOG_ERROR("ERROR  delay_resp message length is invalid.  size:%d  calculated:%d", size, (PTP_COMMON_HDR_LENGTH + PTP_DELAY_RESP_LENGTH));
				goto abort;
			}
			PTPMessageDelayResp *resp = new PTPMessageDelayResp(port);
			resp->sequenceId = sequenceId;			
			resp->setPortIdentity(sourcePortIdentity);
			memcpy(&(resp->requestReceiptTimestamp.seconds_ms),
			       buf + PTP_DELAY_RESP_SEC_MS(PTP_DELAY_RESP_OFFSET),
			       sizeof
				   (resp->requestReceiptTimestamp.seconds_ms));
			memcpy(&(resp->requestReceiptTimestamp.seconds_ls),
			       buf + PTP_DELAY_RESP_SEC_LS(PTP_DELAY_RESP_OFFSET),
			       sizeof(resp->requestReceiptTimestamp.seconds_ls));
			memcpy(&(resp->requestReceiptTimestamp.nanoseconds),
			       buf + PTP_DELAY_RESP_NSEC(PTP_DELAY_RESP_OFFSET),
			       sizeof(resp->requestReceiptTimestamp.nanoseconds));

			resp->requestReceiptTimestamp.seconds_ms =
			    PLAT_ntohs(resp->requestReceiptTimestamp.seconds_ms);
			resp->requestReceiptTimestamp.seconds_ls =
			    PLAT_ntohl(resp->requestReceiptTimestamp.seconds_ls);
			resp->requestReceiptTimestamp.nanoseconds =
			    PLAT_ntohl(resp->requestReceiptTimestamp.nanoseconds);

			memcpy(&(resp->correctionField),
	       buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       sizeof(resp->correctionField));

			msg = resp;

			port->setLastDelayResp(resp);
			port->HaveDelayResp(true);
			GPTP_LOG_VERBOSE("Received delay resp  requestReceiptTimestamp:%d", resp->requestReceiptTimestamp.seconds_ms);
			GPTP_LOG_VERBOSE("Received delay resp  correctionField:%d", resp->correctionField);
		}
		break;
#ifndef APTP
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
			    std::make_shared<PortIdentity>((uint8_t *) buf +
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
      if( size < PTP_COMMON_HDR_LENGTH + PTP_PDELAY_FOLLOWUP_LENGTH ) {
        goto abort;
      }
		{
			PTPMessagePathDelayRespFollowUp *pdelay_resp_fwup_msg =
			    new PTPMessagePathDelayRespFollowUp();
			pdelay_resp_fwup_msg->messageType = messageType;
			// Copy in v2 PDelay Response specific fields
			pdelay_resp_fwup_msg->requestingPortIdentity =
			    std::make_shared<PortIdentity>((uint8_t *) buf +
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
#endif // ifndef RPI		
	case ANNOUNCE_MESSAGE:

		GPTP_LOG_VERBOSE("*** Received Announce message: %d", sequenceId);

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
			if (tlv_length > (int) (2*sizeof(uint16_t)) &&
			 PLAT_ntohs(*((uint16_t *)buf)) == PATH_TRACE_TLV_TYPE)
			{
				buf += sizeof(uint16_t);
				tlv_length -= sizeof(uint16_t);
				annc->tlv.parseClockIdentity((uint8_t *)buf, tlv_length);
			}

			msg = annc;
		}
		break;

	case SIGNALLING_MESSAGE:
		GPTP_LOG_DEBUG("*** Received SIGNALLING message: %d", sequenceId);
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

	GPTP_LOG_VERBOSE("Common correctionField: %d", msg->correctionField);

	msg->setPortIdentity(sourcePortIdentity);
	msg->sequenceId = sequenceId;
	memcpy(&(msg->control),
	       buf + PTP_COMMON_HDR_CONTROL(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->control));
	memcpy(&(msg->logMeanMessageInterval),
	       buf + PTP_COMMON_HDR_LOG_MSG_INTRVL(PTP_COMMON_HDR_OFFSET),
	       sizeof(msg->logMeanMessageInterval));

	GPTP_LOG_VERBOSE("clock id (raw):%s", sourcePortIdentity ? sourcePortIdentity->getClockIdentity().getIdentityString().c_str() : "EMPTY sourcePortIdentity");
	msg->logCommonHeader();

	port->addSockAddrMap(msg->sourcePortIdentity, remote);

	uint16_t debugPort;
	msg->sourcePortIdentity->getPortNumber(&debugPort);
	GPTP_LOG_VERBOSE("Added msg port %d  remote address: %s", debugPort, remote->AddressString().c_str());

	msg->_timestamp = timestamp;
	msg->_timestamp_counter_value = counter_value;

	GPTP_LOG_VERBOSE("timestamp:%s", timestamp.toString());

	delete timer;

	return msg;

abort:
	delete timer;

	return NULL;
}

void PTPMessageCommon::processMessage(IEEE1588Port * port)
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
#ifdef APTP
	// Per Apple Vendor PTP Profile 2017 - force two way and unicast
	flags[0] |= 6;
#endif
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
}

void PTPMessageCommon::logCommonHeader()
{
	uint16_t portNumber = 0;
	sourcePortIdentity->getPortNumber(&portNumber);

	//GPTP_LOG_VERBOSE("transport specific: %d", transportSpecific);
	GPTP_LOG_VERBOSE("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~"); 
	GPTP_LOG_VERBOSE("message type: %d", messageType);
	GPTP_LOG_VERBOSE("ptp version: %d", (versionPTP & 0xF));
	GPTP_LOG_VERBOSE("message length: %d", messageLength);
	GPTP_LOG_VERBOSE("domain number:%d", domainNumber);
	GPTP_LOG_VERBOSE("flags: 0x%08x", flags);
	GPTP_LOG_VERBOSE("correction field: %d", correctionField);
	GPTP_LOG_VERBOSE("source port id (clock id): %s",
	 sourcePortIdentity->getClockIdentity().getIdentityString().c_str());
	GPTP_LOG_VERBOSE("source port id (port #): %d", portNumber);
	GPTP_LOG_VERBOSE("sequence id: %d", sequenceId);
	GPTP_LOG_VERBOSE("control field: %d", control);
	GPTP_LOG_VERBOSE("log message interval: %d", logMeanMessageInterval);
	GPTP_LOG_VERBOSE("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
}

void PTPMessageCommon::getPortIdentity(std::shared_ptr<PortIdentity> identity)
{
	identity = sourcePortIdentity;
}

std::shared_ptr<PortIdentity> PTPMessageCommon::getPortIdentity() const
{
	return sourcePortIdentity;	
}

void PTPMessageCommon::setPortIdentity(std::shared_ptr<PortIdentity> identity)
{
	sourcePortIdentity = identity;
}

PTPMessageCommon::~PTPMessageCommon(void)
{
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

PTPMessageSync::PTPMessageSync(IEEE1588Port * port) : PTPMessageCommon(port)
{
	messageType = SYNC_MESSAGE;	// This is an event message
	sequenceId = port->getNextSyncSequenceId();
	control = SYNC;

	flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);

	originTimestamp = port->getClock()->getTime();

	logMeanMessageInterval = port->getSyncInterval();
	return;
}

PTPMessageSync& PTPMessageSync::assign(const PTPMessageSync& other)
{
	if (this != &other)
	{
		PTPMessageCommon::assign(other);
		originTimestamp = other.originTimestamp;
	}
	return *this;
}

void PTPMessageSync::sendPort(IEEE1588Port * port, std::shared_ptr<PortIdentity> destIdentity)
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

	MulticastType broadcastType = 
#ifdef APTP
	MCAST_NONE;
#else
	MCAST_OTHER;
#endif
	GPTP_LOG_VERBOSE("***  SYNC  before sendEventPort  destIdentity is %s", (destIdentity ? "NOT NULL" : "NULL"));
	port->sendEventPort(PTP_ETHERTYPE, buf_t, messageLength, broadcastType, destIdentity);
	port->incCounter_ieee8021AsPortStatTxSyncCount();

	return;
}

 PTPMessageAnnounce::PTPMessageAnnounce(IEEE1588Port * port) : PTPMessageCommon
    (port)
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

void PTPMessageAnnounce::sendPort(IEEE1588Port * port,
				  std::shared_ptr<PortIdentity> destIdentity)
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

	MulticastType broadcastType = 
#ifdef APTP
	MCAST_NONE;
#else
	MCAST_OTHER;
#endif

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, broadcastType, destIdentity);
	port->incCounter_ieee8021AsPortStatTxAnnounce();
}

void PTPMessageAnnounce::processMessage(IEEE1588Port * port)
{
	if (*(port->getPortIdentity()) == *getPortIdentity())
	{
		// Ignore messages from self
		return;
	}

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
	port->addQualifiedAnnounce(this);

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

void PTPMessageSync::processMessage(IEEE1588Port * port)
{
	PortState state = port->getPortState();	
	if (PTP_DISABLED == state || PTP_INITIALIZING == state || PTP_LISTENING == state) 
	{
		// Do nothing Sync messages should be ignored when in these states
		return;
	}

	if (*(port->getPortIdentity()) == *getPortIdentity())
	{
		// Ignore messages from self
		return;
	}

	if (PTP_FAULTY == state) 
	{
		// According to spec recovery is implementation specific
		port->recoverPort();
		return;
	}

	port->incCounter_ieee8021AsPortStatRxSyncCount();

	// Check if we are in slave mode
	if (PTP_SLAVE == state)
	{
		PTPMessageDelayReq req(port);
		Timestamp zeroTime;
		GPTP_LOG_DEBUG("Process Sync Request SeqId: %u\t", sequenceId);
		req.setOriginTimestamp(zeroTime);		
		req.setSequenceId(sequenceId);
		
		std::shared_ptr<PortIdentity> reqPortId = std::make_shared<PortIdentity>();
		port->getPortIdentity(reqPortId);
		req.setPortIdentity(reqPortId);
		GPTP_LOG_VERBOSE("PTPMessageSync::processMessage   req clockId:%s",
		 req.getPortIdentity()->getClockIdentity().getIdentityString().c_str());

		port->getTxLock();
		req.sendPort(port, sourcePortIdentity);
		GPTP_LOG_DEBUG("*** Sent Delay Request message");
		port->putTxLock();
		port->setLastDelayReq(req);
	}

#ifdef APTP
	PTPMessageSync *sync = port->getLastSync();
	port->setLastSync(NULL);
	delete sync;

	GPTP_LOG_VERBOSE("PTPMessageSync::processMessage  before setLastSync");
	port->setLastSync(this);
	GPTP_LOG_VERBOSE("PTPMessageSync::processMessage  after setLastSync");

	port->HaveDelayResp(false);
	if (!port->FollowupAhead())
	{
		port->HaveFollowup(false);
	}
	port->FollowupAhead(false);
	_gc = false;
#else
	if (flags[PTP_ASSIST_BYTE] & (0x1<<PTP_ASSIST_BIT))
	{
		port->setLastSync(this);
		_gc = false;
	}
	else
	{
		GPTP_LOG_ERROR("PTP assist flag is not set, discarding invalid sync");
		_gc = true;
	}
#endif

	MaybePerformCalculations(port);	
}

 PTPMessageFollowUp::PTPMessageFollowUp(IEEE1588Port * port):PTPMessageCommon
    (port)
{
	messageType = FOLLOWUP_MESSAGE;	/* This is an event message */
	control = FOLLOWUP;

	logMeanMessageInterval = port->getSyncInterval();

	return;
}

PTPMessageFollowUp& PTPMessageFollowUp::assign(const PTPMessageFollowUp& other)
{
	if (this != &other)
	{
		PTPMessageCommon::assign(other);
		preciseOriginTimestamp = other.preciseOriginTimestamp;
		tlv = other.tlv;
	}
	return *this;
}

void PTPMessageFollowUp::sendPort(IEEE1588Port * port,
				  std::shared_ptr<PortIdentity> destIdentity)
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

	// Compute correction field
        Timestamp egressTime = port->getClock()->getTime();
	int64_t residenceTime = TIMESTAMP_TO_NS(egressTime) - TIMESTAMP_TO_NS(preciseOriginTimestamp);
	static const int32_t kTwoToThe16th = 65536;
	correctionField = residenceTime * kTwoToThe16th;

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

	MulticastType broadcastType = 
#ifdef APTP
	MCAST_NONE;
#else
	MCAST_OTHER;
#endif

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, broadcastType, destIdentity);

	port->incCounter_ieee8021AsPortStatTxFollowUpCount();

	return;
}

void PTPMessageFollowUp::processMessage(IEEE1588Port * port)
{
	if (*(port->getPortIdentity()) == *getPortIdentity())
	{
		// Ignore messages from self
		return;
	}

	uint64_t delay;
	Timestamp sync_arrival;
	Timestamp system_time(0, 0, 0);
	Timestamp device_time(0, 0, 0);
	Timestamp masterTime(0, 0, 0);

	signed long long scalar_offset;

	int64_t convertedCorrection = correctionField >> 16;

	FrequencyRatio local_clock_adjustment;
	FrequencyRatio local_system_freq_offset;
	FrequencyRatio master_local_freq_offset;
	int64_t correction;
	int32_t scaledLastGmFreqChange = 0;
	scaledNs scaledLastGmPhaseChange;

	GPTP_LOG_DEBUG("Processing a follow-up message");

	GPTP_LOG_VERBOSE("Followup clock id:%s", 
	 sourcePortIdentity->getClockIdentity().getIdentityString().c_str());

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

	FrequencyRatio adjMaster = 0.0;
	Timestamp correctedMasterEventTimestamp;

	std::shared_ptr<PortIdentity> sync_id;
	PTPMessageSync *sync = port->getLastSync();
	if (sync == nullptr) {
		GPTP_LOG_ERROR("Received Follow Up but there is no sync message");
		return;
	}
	sync->getPortIdentity(sync_id);

#ifndef APTP
	if (sync->getSequenceId() != sequenceId || 
	 (sourcePortIdentity != nullptr && sync_id != nullptr &&
	 *sync_id != *sourcePortIdentity))
	{
		unsigned int cnt = 0;

		if( !port->incWrongSeqIDCounter(&cnt) )
		{
			port->becomeMaster( true );
			port->setWrongSeqIDCounter(0);
		}

		GPTP_LOG_ERROR("Received Follow Up %d times but cannot find "
		 "corresponding Sync", cnt);
		goto done;
	}
#endif

	sync_arrival = sync->getTimestamp();

	if( !port->getLinkDelay(&delay) ) 
	{
		GPTP_LOG_ERROR("Error getting link delay.");
		goto done;
	}

	master_local_freq_offset  =  tlv.getRateOffset();
	master_local_freq_offset /= 1ULL << 41;
	master_local_freq_offset += 1.0;

	master_local_freq_offset /= port->getPeerRateOffset();
	//correctionField /= 1 << 16;
	
	GPTP_LOG_VERBOSE("Followup  sync_arrival:%Ld", TIMESTAMP_TO_NS(sync_arrival));
	GPTP_LOG_VERBOSE("Followup  master_local_freq_offset:%Le", master_local_freq_offset);
	
	correction = (int64_t)((delay * master_local_freq_offset) + convertedCorrection);

	GPTP_LOG_VERBOSE("Followup  correction:%Ld", correction);

	masterTime = preciseOriginTimestamp;

	GPTP_LOG_VERBOSE("Followup  masterTime (raw):%Ld", TIMESTAMP_TO_NS(masterTime));

	if (correction > 0)
	   TIMESTAMP_ADD_NS(masterTime, correction);
	else 
		TIMESTAMP_SUB_NS(masterTime, -correction);

	GPTP_LOG_VERBOSE("Followup  masterTime (adj):%Ld", TIMESTAMP_TO_NS(masterTime));

	scalar_offset  = TIMESTAMP_TO_NS(sync_arrival);
	scalar_offset -= TIMESTAMP_TO_NS(masterTime);

	port->MasterOffset(scalar_offset);
   GPTP_LOG_VERBOSE("Followup  scalar_offset:%Ld", scalar_offset);

	GPTP_LOG_VERBOSE("Followup Correction Field: %Ld,  convertedCorrection: %Ld"
	 " Link Delay: %lu", correctionField, convertedCorrection, delay);
	GPTP_LOG_VERBOSE("FollowUp Scalar = %lld", scalar_offset);

	/* Otherwise synchronize clock with approximate time from Sync message */
	uint32_t local_clock, nominal_clock_rate;
	uint32_t device_sync_time_offset;

	port->getDeviceTime(system_time, device_time, local_clock, nominal_clock_rate);
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

	// <correctedMasterEventTimestamp> =
	// <preciseOriginTimestamp> + <meanPathDelay> + correctionField of Sync message +
	// correctionField of Follow_Up message.
	// Note that the correctionField of the followup message is added to the 
	// preciseOriginTimestamp above.

	adjMaster = TIMESTAMP_TO_NS(masterTime) + 
	 port->meanPathDelay() + (sync->getCorrectionField() >> 16);

	correctedMasterEventTimestamp.set64(uint64_t(adjMaster));

	port->AdjustedTime(TIMESTAMP_TO_NS(masterTime));

	// GPTP_LOG_INFO("PTPMessageFollowUp::processMessage  preciseOriginTimestamp.seconds_ls:%d", preciseOriginTimestamp.seconds_ls);
	// GPTP_LOG_INFO("PTPMessageFollowUp::processMessage  preciseOriginTimestamp.seconds_ms:%d", preciseOriginTimestamp.seconds_ms);
	// GPTP_LOG_INFO("PTPMessageFollowUp::processMessage  preciseOriginTimestamp.nanoseconds:%d", preciseOriginTimestamp.nanoseconds);

	// GPTP_LOG_INFO("PTPMessageFollowUp::processMessage  correctedMasterEventTimestamp.seconds_ls:%d", correctedMasterEventTimestamp.seconds_ls);
	// GPTP_LOG_INFO("PTPMessageFollowUp::processMessage  correctedMasterEventTimestamp.seconds_ms:%d", correctedMasterEventTimestamp.seconds_ms);
	// GPTP_LOG_INFO("PTPMessageFollowUp::processMessage  correctedMasterEventTimestamp.nanoseconds:%d", correctedMasterEventTimestamp.nanoseconds);

	local_clock_adjustment = port->getClock()->calcMasterLocalClockRateDifference(
	 masterTime, sync_arrival);

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
			port->getClock()->calcLocalSystemClockRateDifference(device_time, system_time);
		TIMESTAMP_SUB_NS(system_time,
		 (uint64_t)(((FrequencyRatio) device_sync_time_offset)/local_system_freq_offset));

		signed long long local_system_offset =
			TIMESTAMP_TO_NS(system_time) - TIMESTAMP_TO_NS(sync_arrival);

		port->getClock()->setMasterOffset(port, scalar_offset, sync_arrival,
		 local_clock_adjustment, local_system_offset, system_time,
		 local_system_freq_offset, port->getSyncCount(), port->getPdelayCount(),
		 port->getPortState(), port->getAsCapable(), port->AdrRegSocketPort());

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

	MaybePerformCalculations(port);

done:
	_gc = true;
#ifndef APTP
	port->setLastSync(NULL);
	delete sync;
#endif	
}

PTPMessageDelayReq::PTPMessageDelayReq(IEEE1588Port * port):
	 PTPMessageCommon(port)
{
	logMeanMessageInterval = 0;
	control = MESSAGE_OTHER;
	messageType = DELAY_REQ_MESSAGE;
	sequenceId = port->getNextDelaySequenceId();
}

PTPMessageDelayReq& PTPMessageDelayReq::assign(const PTPMessageDelayReq& other)
{
	if (this != &other)
	{
		PTPMessageCommon::assign(other);
		originTimestamp = other.originTimestamp;
	}
	return *this;
}

void PTPMessageDelayReq::processMessage(IEEE1588Port * port)
{
	if (*(port->getPortIdentity()) == *getPortIdentity())
	{
		// Ignore messages from self
		return;
	}

   if (V2_E2E == port->getDelayMechanism() && 
   	PTP_MASTER == port->getPortState()) 
   {
      port->incCounter_ieee8021AsPortStatRxDelayRequest();

		PTPMessageDelayResp resp(port);

		GPTP_LOG_DEBUG("Process Delay Request SeqId: %u\t", sequenceId);

		GPTP_LOG_VERBOSE("PTPMessageDelayReq::processMessage   1  sourcePortIdentity is %s",
		 (sourcePortIdentity ? "NOT NULL" : "NULL"));

		std::shared_ptr<PortIdentity> portId = port->getPortIdentity();
		
		resp.RequestingPortIdentity(sourcePortIdentity);
		resp.setPortIdentity(portId);
		resp.setRequestReceiptTimestamp(_timestamp);
		resp.setSequenceId(sequenceId);
		port->getTxLock();
		resp.sendPort(port, sourcePortIdentity);

		GPTP_LOG_DEBUG("*** Sent Delay Response message");
		port->putTxLock();

	}
}

void PTPMessageDelayReq::sendPort(IEEE1588Port * port,
				      std::shared_ptr<PortIdentity> destIdentity)
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

	// originTimeStamp
	originTimestamp = port->getClock()->getTime();
	setTimestamp(originTimestamp);

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

	MulticastType broadcastType = 
#ifdef APTP
	MCAST_NONE;
#else
	MCAST_OTHER;
#endif

	port->sendEventPort(PTP_ETHERTYPE, buf_t, messageLength, broadcastType, destIdentity);
	port->incCounter_ieee8021AsPortStatTxDelayRequest();

	return;
}

#ifdef APTP
// Overload of the base method to inject the Apple Vendor PTP Profile specific
// values.
void PTPMessageDelayReq::buildCommonHeader(uint8_t * buf)
{
	unsigned char tspec_msg_t;
    /*TODO: Message type assumes value sbetween 0x0 and 0xD (its an enumeration).
     * So I am not sure why we are adding 0x10 to it
     */
	tspec_msg_t = messageType | 0x10;
	// Per Apple Vendor PTP Profile 2017 (See setDelayRequest)
	correctionField = 0;
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

	// Per Apple Vendor PTP Profile 2017 - force two way and unicast
	flags[0] |= 6;
	memcpy(buf + PTP_COMMON_HDR_FLAGS(PTP_COMMON_HDR_OFFSET), &flags,
	       PTP_FLAGS_LENGTH);
	memcpy(buf + PTP_COMMON_HDR_CORRECTION(PTP_COMMON_HDR_OFFSET),
	       &correctionField_BE, sizeof(correctionField));

	// Per Apple Vendor PTP Profile 2017 (See setDelayRequest)
	sourcePortIdentity->getClockIdentityString
	  ((uint8_t *) buf+
	   PTP_COMMON_HDR_SOURCE_CLOCK_ID(PTP_COMMON_HDR_OFFSET));
	sourcePortIdentity->getPortNumberNO
	  ((uint16_t *) (buf + PTP_COMMON_HDR_SOURCE_PORT_ID
			 (PTP_COMMON_HDR_OFFSET)));

	// Per Apple Vendor PTP Profile 2017 (See setDelayRequest)
	GPTP_LOG_VERBOSE("PTPMessageDelayReq::buildCommonHeader Sending Sequence Id: %u", sequenceId);
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
#endif // ifdef APTP

PTPMessageDelayResp::PTPMessageDelayResp(IEEE1588Port * port):
 PTPMessageCommon(port)
{
	logMeanMessageInterval = 0;
	control = MESSAGE_OTHER;
	messageType = DELAY_RESP_MESSAGE;
	port->getPortIdentity(requestingPortIdentity);
}

PTPMessageDelayResp::~PTPMessageDelayResp()
{
	//delete requestingPortIdentity;
}

PTPMessageDelayResp& 
PTPMessageDelayResp::assign(const PTPMessageDelayResp& other)
{
	if (&other != this)
	{
		PTPMessageCommon::assign(other);
		requestReceiptTimestamp = other.requestReceiptTimestamp;
		if (other.requestingPortIdentity != nullptr)
		{
			requestingPortIdentity = other.requestingPortIdentity;
		}
	}
	return *this;
}

void PTPMessageDelayResp::processMessage(IEEE1588Port * port)
{
	if (*(port->getPortIdentity()) == *getPortIdentity())
	{
		// Ignore messages from self
		GPTP_LOG_VERBOSE("Ignoring Delay Response message from self.");
		return;
	}

	GPTP_LOG_DEBUG("*** Delay Response processMessage  delayMech:%d",port->getDelayMechanism());

	MaybePerformCalculations(port);
}

void PTPMessageDelayResp::sendPort(IEEE1588Port * port,
 std::shared_ptr<PortIdentity> destIdentity)
{
	uint8_t buf_t[256];
	uint8_t *buf_ptr = buf_t + port->getPayloadOffset();
	unsigned char tspec_msg_t = 0x0;
	Timestamp receiveTimeStamp_BE;
	memset(buf_t, 0, 256);
	// Create packet in buf
	// Copy in common header

	messageLength = PTP_COMMON_HDR_LENGTH + PTP_DELAY_RESP_LENGTH;
	tspec_msg_t |= messageType & 0xF;

	fEgressTime = port->getClock()->getTime();

	buildCommonHeader(buf_ptr);

	// receiveTimeStamp
	Timestamp receiveTimestamp = port->getClock()->getTime();
	receiveTimeStamp_BE.seconds_ms = PLAT_htons(receiveTimestamp.seconds_ms);
	receiveTimeStamp_BE.seconds_ls = PLAT_htonl(receiveTimestamp.seconds_ls);
	receiveTimeStamp_BE.nanoseconds =
	    PLAT_htonl(receiveTimestamp.nanoseconds);
	
	// Copy in v2 sync specific fields
	memcpy(buf_ptr + PTP_DELAY_RESP_SEC_MS(PTP_DELAY_RESP_OFFSET),
	       &(receiveTimeStamp_BE.seconds_ms),
	       sizeof(receiveTimestamp.seconds_ms));
	memcpy(buf_ptr + PTP_DELAY_RESP_SEC_LS(PTP_DELAY_RESP_OFFSET),
	       &(receiveTimeStamp_BE.seconds_ls),
	       sizeof(receiveTimestamp.seconds_ls));
	memcpy(buf_ptr + PTP_DELAY_RESP_NSEC(PTP_DELAY_RESP_OFFSET),
	       &(receiveTimeStamp_BE.nanoseconds),
	       sizeof(receiveTimestamp.nanoseconds));

   GPTP_LOG_VERBOSE("requestingPortIdentity: %s",requestingPortIdentity->getClockIdentity().getIdentityString().c_str());
   GPTP_LOG_VERBOSE("sourcePortIdentity: %s",sourcePortIdentity->getClockIdentity().getIdentityString().c_str());
	requestingPortIdentity->getClockIdentityString
	  (buf_ptr + PTP_DELAY_RESP_REQ_CLOCK_ID
	   (PTP_DELAY_RESP_OFFSET));
	requestingPortIdentity->getPortNumberNO
		((uint16_t *)
		 (buf_ptr + PTP_DELAY_RESP_REQ_PORT_ID
		  (PTP_DELAY_RESP_OFFSET)));

	GPTP_LOG_VERBOSE("PTPMessageDelayResp::sendPort   destIdentity is %s", (destIdentity ? "NOT NULL" : "NULL"));

	MulticastType broadcastType = 
#ifdef APTP
	MCAST_NONE;
#else
	MCAST_OTHER;
#endif

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, broadcastType, destIdentity);
	port->incCounter_ieee8021AsPortStatTxSyncCount();

	return;
}

#ifdef APTP
// Overload of the base method to inject the Apple Vendor PTP Profile specific
// values.
void PTPMessageDelayResp::buildCommonHeader(uint8_t * buf)
{
	unsigned char tspec_msg_t;
    /*TODO: Message type assumes value sbetween 0x0 and 0xD (its an enumeration).
     * So I am not sure why we are adding 0x10 to it
     */
	tspec_msg_t = messageType | 0x10;

	// Compute correction field
	int64_t residenceTime = TIMESTAMP_TO_NS(fEgressTime) - TIMESTAMP_TO_NS(requestReceiptTimestamp);

	static const int32_t kTwoToThe16th = 65536;
	correctionField = residenceTime * kTwoToThe16th;
	int64_t correctionField_BE = PLAT_htonll(correctionField);
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

	// Per Apple Vendor PTP Profile 2017 (See setDelayReponse)
	sourcePortIdentity->getClockIdentityString
	  ((uint8_t *) buf+
	   PTP_COMMON_HDR_SOURCE_CLOCK_ID(PTP_COMMON_HDR_OFFSET));
	sourcePortIdentity->getPortNumberNO
	  ((uint16_t *) (buf + PTP_COMMON_HDR_SOURCE_PORT_ID
			 (PTP_COMMON_HDR_OFFSET)));

	// Per Apple Vendor PTP Profile 2017 (See setDelayRequest)
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
#endif // ifdef APTP

PTPMessagePathDelayReq::PTPMessagePathDelayReq(IEEE1588Port * port):
 PTPMessageCommon (port)
{
	logMeanMessageInterval = 0;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_REQ_MESSAGE;
	sequenceId = port->getNextPDelaySequenceId();
	return;
}

void PTPMessagePathDelayReq::processMessage(IEEE1588Port * port)
{
	OSTimer *timer = port->getTimerFactory()->createTimer();
	std::shared_ptr<PortIdentity> resp_fwup_id;
	std::shared_ptr<PortIdentity> requestingPortIdentity_p;
	PTPMessagePathDelayResp *resp;
	std::shared_ptr<PortIdentity> resp_id;
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
	resp->setPortIdentity(resp_id);
	resp->setSequenceId(sequenceId);

	GPTP_LOG_DEBUG("Process PDelay Request SeqId: %u\t", sequenceId);

#ifdef DEBUG
	for (int n = 0; n < PTP_CLOCK_IDENTITY_LENGTH; ++n) {
		GPTP_LOG_VERBOSE("%c", resp_id.clockIdentity[n]);
	}
#endif

	this->getPortIdentity(requestingPortIdentity_p);
	resp->setRequestingPortIdentity(requestingPortIdentity_p);
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
	resp_fwup->setPortIdentity(resp_fwup_id);
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

void PTPMessagePathDelayReq::sendPort(IEEE1588Port * port,
				      std::shared_ptr<PortIdentity> destIdentity)
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

 PTPMessagePathDelayResp::PTPMessagePathDelayResp(IEEE1588Port * port) :
	 PTPMessageCommon(port)
{
    /*TODO: Why 0x7F?*/
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_RESP_MESSAGE;
	versionPTP = GPTP_VERSION;
	requestingPortIdentity = std::make_shared<PortIdentity>();

	flags[PTP_ASSIST_BYTE] |= (0x1 << PTP_ASSIST_BIT);

	return;
}

PTPMessagePathDelayResp::~PTPMessagePathDelayResp()
{
	//delete requestingPortIdentity;
}

void PTPMessagePathDelayResp::processMessage(IEEE1588Port * port)
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

	std::shared_ptr<PortIdentity> resp_id;
	std::shared_ptr<PortIdentity> oldresp_id;
	uint16_t resp_port_number;
	uint16_t oldresp_port_number;

	PTPMessagePathDelayResp *old_pdelay_resp = port->getLastPDelayResp();
	if( old_pdelay_resp == NULL ) {
		goto bypass_verify_duplicate;
	}

	old_pdelay_resp->getPortIdentity(oldresp_id);
	oldresp_id->getPortNumber(&oldresp_port_number);
	getPortIdentity(resp_id);
	resp_id->getPortNumber(&resp_port_number);

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

void PTPMessagePathDelayResp::sendPort(IEEE1588Port * port,
				       std::shared_ptr<PortIdentity> destIdentity)
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
(std::shared_ptr<PortIdentity> identity)
{
	requestingPortIdentity = identity;
}

void PTPMessagePathDelayResp::getRequestingPortIdentity
(std::shared_ptr<PortIdentity> identity)
{
	identity = requestingPortIdentity;
}

PTPMessagePathDelayRespFollowUp::PTPMessagePathDelayRespFollowUp
(IEEE1588Port * port) : PTPMessageCommon (port)
{
	logMeanMessageInterval = 0x7F;
	control = MESSAGE_OTHER;
	messageType = PATH_DELAY_FOLLOWUP_MESSAGE;
	versionPTP = GPTP_VERSION;
	requestingPortIdentity = std::make_shared<PortIdentity>();

	return;
}

PTPMessagePathDelayRespFollowUp::~PTPMessagePathDelayRespFollowUp()
{
	//delete requestingPortIdentity;
}

#define US_PER_SEC 1000000
void PTPMessagePathDelayRespFollowUp::processMessage(IEEE1588Port * port)
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

	std::shared_ptr<PortIdentity> req_id;
	std::shared_ptr<PortIdentity> resp_id;
	std::shared_ptr<PortIdentity> fup_sourcePortIdentity;
	std::shared_ptr<PortIdentity> resp_sourcePortIdentity;
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

	req->getPortIdentity(req_id);
	resp->getRequestingPortIdentity(resp_id);
	req_clkId = req_id->getClockIdentity();
	resp_clkId = resp_id->getClockIdentity();
	resp_id->getPortNumber(&resp_port_number);
	requestingPortIdentity->getPortNumber(&req_port_number);
	resp->getPortIdentity(resp_sourcePortIdentity);
	getPortIdentity(fup_sourcePortIdentity);

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
	if ( *fup_sourcePortIdentity != *resp_sourcePortIdentity ) {
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

void PTPMessagePathDelayRespFollowUp::sendPort(IEEE1588Port * port,
					       std::shared_ptr<PortIdentity> destIdentity)
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
(std::shared_ptr<PortIdentity> identity)
{
	requestingPortIdentity = identity;
}


 PTPMessageSignalling::PTPMessageSignalling(void)
{
}

 PTPMessageSignalling::PTPMessageSignalling(IEEE1588Port * port) : PTPMessageCommon(port)
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

void PTPMessageSignalling::sendPort(IEEE1588Port * port, 
	std::shared_ptr<PortIdentity> destIdentity)
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

	MulticastType broadcastType = 
#ifdef APTP
	MCAST_NONE;
#else
	MCAST_OTHER;
#endif

	port->sendGeneralPort(PTP_ETHERTYPE, buf_t, messageLength, broadcastType, destIdentity);
}

void PTPMessageSignalling::processMessage(IEEE1588Port * port)
{
	GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage");

	GPTP_LOG_STATUS("Signalling Link Delay Interval: %d", tlv.getLinkDelayInterval());
	GPTP_LOG_STATUS("Signalling Sync Interval: %d", tlv.getTimeSyncInterval());
	GPTP_LOG_STATUS("Signalling Announce Interval: %d", tlv.getAnnounceInterval());

#ifndef APTP
	long long unsigned int waitTime;

	char linkDelayInterval = tlv.getLinkDelayInterval();
	char timeSyncInterval = tlv.getTimeSyncInterval();
	char announceInterval = tlv.getAnnounceInterval();

	if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   linkDelayInterval == sigMsgInterval_Initial");
		port->setInitPDelayInterval();

		waitTime = ((long long) (pow((double)2, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		// TODO: No send functionality needs to be implemented.
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   linkDelayInterval == sigMsgInterval_NoSend");
		GPTP_LOG_WARNING("Signal received to stop sending pDelay messages: Not implemented");
	}
	else if (linkDelayInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		// Nothing to do
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   linkDelayInterval == sigMsgInterval_NoChange");
	}
	else {
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   else (linkDelayInterval)");
#ifndef APTP
		port->setPDelayInterval(linkDelayInterval);
		waitTime = ((long long) (pow((double)2, port->getPDelayInterval()) *  1000000000.0));
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
		port->startPDelayIntervalTimer(waitTime);
#endif
	}

	if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_Initial) {
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   timeSyncInterval == sigMsgInterval_Initial");
		port->setInitSyncInterval();

		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
#ifndef APTP
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
#endif		
		port->startSyncIntervalTimer(waitTime);
		port->SyncIntervalTimeoutExpireCount(0);
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoSend) {
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   timeSyncInterval == sigMsgInterval_NoSend");
		// TODO: No send functionality needs to be implemented.
		GPTP_LOG_WARNING("Signal received to stop sending Sync messages: Not implemented");
	}
	else if (timeSyncInterval == PTPMessageSignalling::sigMsgInterval_NoChange) {
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   timeSyncInterval == sigMsgInterval_NoChange");
		// Nothing to do
	}
	else {
		port->setSyncInterval(timeSyncInterval);
		waitTime = ((long long) (pow((double)2, port->getSyncInterval()) *  1000000000.0));
		GPTP_LOG_VERBOSE("PTPMessageSignalling::processMessage   else  timeSyncInterval   waitTime:%d", waitTime);
#ifndef APTP
		waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
#endif		
		port->startSyncIntervalTimer(waitTime);
		port->SyncIntervalTimeoutExpireCount(0);
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
#ifndef APTP
			waitTime = waitTime > EVENT_TIMER_GRANULARITY ? waitTime : EVENT_TIMER_GRANULARITY;
#endif
			port->startAnnounceIntervalTimer(waitTime);
		}
	}
#endif	
}
