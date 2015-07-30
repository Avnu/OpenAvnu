/*************************************************************************************************************
Copyright (c) 2012-2015, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
 
1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS LISTED "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS LISTED BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
Attributions: The inih library portion of the source code is licensed from 
Brush Technology and Ben Hoyt - Copyright (c) 2009, Brush Technology and Copyright (c) 2009, Ben Hoyt. 
Complete license and copyright information can be found at 
https://github.com/benhoyt/inih/commit/74d2ca064fb293bc60a77b0bd068075b293cf175.
*************************************************************************************************************/

#include "openavb_types.h"
#include "openavb_result_codes.h"

openavbRC openavbUtilRCRecord(openavbRC rc)
{
	return rc;
}

char *openavbUtilRCResultToString(openavbRC rc)
{
	if (IS_OPENAVB_SUCCESS(rc))						return "Success";
	else											return "Failure";
}

char *openavbUtilRCModuleToString(openavbRC rc)
{
	switch (rc & OPENAVB_RC_MODULE_MASK) {
		case OPENAVB_MODULE_GLOBAL:					return "AVB";
		case OPENAVB_MODULE_GPTP: 					return "gPTP";
		case OPENAVB_MODULE_SRP:						return "SRP";
		case OPENAVB_MODULE_AVTP:					return "AVTP";
		case OPENAVB_MODULE_AVTP_TIME:				return "AVTP TIME";
		case OPENAVB_MODULE_AVDECC:					return "AVDECC";
		default:									return "Unknown";
	}
}

char *openavbUtilRCCodeToString(openavbRC rc) 
{
	// General result codes
	switch (rc & OPENAVB_RC_CODE_MASK) {
		case OPENAVB_RC_GENERIC:							return "";
		case OPENAVB_RC_RAWSOCK_OPEN:					return "Failed to open rawsock";
		case OPENAVB_RC_OUT_OF_MEMORY:					return "Out of memory";
		case OPENAVB_RC_INVALID_ARGUMENT:				return "Invalid function argument";
		case OPENAVB_RC_FAILED_TO_OPEN:					return "Failed to open";
		// No Default
	}

	// gPTP Result Codes
	if ((rc & OPENAVB_RC_MODULE_MASK) == OPENAVB_MODULE_GPTP) {
		switch (rc & OPENAVB_RC_CODE_MASK) {
			case OPENAVBPTP_RC_GENERIC:						return "";
			case OPENAVBPTP_RC_SHARED_MEMORY_OPEN:  		return "Failed to open shared memory file";
			case OPENAVBPTP_RC_SHARED_MEMORY_TRANC: 		return "Failed to truncate shared memory file";
			case OPENAVBPTP_RC_SHARED_MEMORY_MMAP:  		return "Failed to memory map shared memory file";
			case OPENAVBPTP_RC_SHARED_MEMORY_ENTRY: 		return "Failed to locate matching shared memory item";
			case OPENAVBPTP_RC_SHARED_MEMORY_UPDATE:		return "Failed to update shared memory item";
			case OPENAVBPTP_RC_PTP_DEV_OPEN:				return "Failed to open ptp device";
			case OPENAVBPTP_RC_PTP_DEV_CLOCKID: 			return "Failed to obtain ptp device clock ID";
			case OPENAVBPTP_RC_SOCK_OPEN:  					return "Failed to open socket";
			case OPENAVBPTP_RC_SOCK_NET_INTERFACE:  		return "Unable to obtain network interface";
			case OPENAVBPTP_RC_SOCK_DEVICE_INDEX:   		return "Unable to obtain socket device index";
			case OPENAVBPTP_RC_SOCK_REUSE: 					return "Unable to reuse socket";
			case OPENAVBPTP_RC_SOCK_BIND: 					return "Unable to bind socket";
			case OPENAVBPTP_RC_SOCK_TIMESTAMP: 				return "Hardware timestamping not supported";
			case OPENAVBPTP_RC_SOCK_LINK_DOWN:  			return "Socket network link not active";
			case OPENAVBPTP_RC_TIMER_CREATE:   				return "Failed to create timer(s)";
			case OPENAVBPTP_RC_SIGNAL_HANDLER: 				return "Failed to create signal handler";
			case OPENAVBPTP_RC_CONFIG_FILE_OPEN:			return "Failed to open configuration file";
			case OPENAVBPTP_RC_CONFIG_FILE_READ:			return "Failed to read configuration file";
			case OPENAVBPTP_RC_CONFIG_FILE_DATA:			return "Invalid data encountered in configuration file";
			case OPENAVBPTP_RC_CONFIG_FILE_WRITE:   		return "Failed to write configuration file";
			case OPENAVBPTP_RC_NEW_CONFIG_FILE_WRITE:		return "SUCCESSFULLY wrote recreated configuration file";
			case OPENAVBPTP_RC_CLOCK_GET_TIME:  			return "Failed to obtain time";
			case OPENAVBPTP_RC_SEND_FAIL:  					return "Failed to send packet";
			case OPENAVBPTP_RC_SEND_SHORT: 					return "Failed to send complete packet";
			case OPENAVBPTP_RC_SOCK_ADD_MULTI:  			return "Failed to set multicast address";
			// No Default
		}
	}


	// SRP Result Codes
	if ((rc & OPENAVB_RC_MODULE_MASK) == OPENAVB_MODULE_SRP) {
		switch (rc & OPENAVB_RC_CODE_MASK) {
			case OPENAVBSRP_RC_GENERIC:						return "";
			case OPENAVBSRP_RC_ALREADY_INIT:   				return "Already initialized";
			case OPENAVBSRP_RC_SOCK_OPEN: 					return "Failed to open socket";
			case OPENAVBSRP_RC_THREAD_CREATE:  				return "Failed to create thread";
			case OPENAVBSRP_RC_BAD_CLASS:  					return "Attempt to register / access stream with a bad class index";
			case OPENAVBSRP_RC_REG_NULL_HDL:				return "Attempt to register a stream with a null handle";
			case OPENAVBSRP_RC_REREG: 						return "Attempt to [re]register an exisiting stream Id";
			case OPENAVBSRP_RC_NO_MEMORY: 					return "Out of memory";
			case OPENAVBSRP_RC_NO_BANDWIDTH:  				return "Insufficient bandwidth";
			case OPENAVBSRP_RC_SEND:  						return "Failed to send packet";
			case OPENAVBSRP_RC_DEREG_NO_XST:				return "Attempt to deregister a non-existent stream";
			case OPENAVBSRP_RC_DETACH_NO_XST:   			return "Attempt to detach from non-existent stream";
			case OPENAVBSRP_RC_INVALID_SUBTYPE: 			return "Invalid listener declaration subtype";
			case OPENAVBSRP_RC_FRAME_BUFFER:  				return "Failed to get a transmit frame buffer";
			case OPENAVBSRP_RC_SHARED_MEM_MMAP: 			return "Failed to mmap openavb_gPTP shared memory";
			case OPENAVBSRP_RC_SHARED_MEM_OPEN: 			return "Failed to open openavb_gPTP shared memory";
			case OPENAVBSRP_RC_BAD_VERSION:					return "AVB core stack version mismatch endpoint / srp vs gptp";
			// No Default
		}
	}

	// AVTP Result Codes
	if ((rc & OPENAVB_RC_MODULE_MASK) == OPENAVB_MODULE_AVTP) {
		switch (rc & OPENAVB_RC_CODE_MASK) {
			case OPENAVBAVTP_RC_GENERIC:					return "";
			case OPENAVBAVTP_RC_TX_PACKET_NOT_READY:		return "Transmit packet not ready";
			case OPENAVBAVTP_RC_MAPPING_CB_NOT_SET:			return "Mapping callback structure not set";
			case OPENAVBAVTP_RC_INTERFACE_CB_NOT_SET:		return "Interface callback structure not set";
			case OPENAVBAVTP_RC_NO_FRAMES_PROCESSED:		return "No frames processed";
			case OPENAVBAVTP_RC_INVALID_AVTP_VERSION:		return "Invalid AVTP version configured";
			case OPENAVBAVTP_RC_IGNORING_STREAMID:			return "Ignoring unexpected streamID";
			case OPENAVBAVTP_RC_IGNORING_CONTROL_PACKET:	return "Ignoring unexpected AVTP control packet";
			case OPENAVBAVTP_RC_PARSING_FRAME_HEADER:		return "Parsing frame header";
			// No Default
		}
	}

	// AVTP Time Result Codes
	if ((rc & OPENAVB_RC_MODULE_MASK) == OPENAVB_MODULE_AVTP_TIME) {
		switch (rc & OPENAVB_RC_CODE_MASK) {
			case OPENAVBAVTPTIME_RC_GENERIC:				return "";
			case OPENAVBAVTPTIME_RC_PTP_TIME_DESCRIPTOR:	return "PTP file descriptor not available";
			case OPENAVBAVTPTIME_RC_OPEN_SRC_PTP_NOT_AVAIL:	return "Open source PTP configured but not available in this build";
			case OPENAVBAVTPTIME_RC_GET_TIME_ERROR:			return "PTP get time error";
			case OPENAVBAVTPTIME_RC_OPENAVB_PTP_NOT_AVAIL:		return "OPENAVB PTP configured but not available in this build";
			case OPENAVBAVTPTIME_RC_INVALID_PTP_TIME:		return "Invalid avtp_time_t";
			// No Default
		}
	}

	// AVDECC Result Codes
	if ((rc & OPENAVB_RC_MODULE_MASK) == OPENAVB_MODULE_AVDECC) {
		switch (rc & OPENAVB_RC_CODE_MASK) {
			case OPENAVBAVDECC_RC_GENERIC:					return "";
			case OPENAVBAVDECC_RC_BUFFER_TOO_SMALL:			return "Buffer size is too small";
			case OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING:		return "The Entity Model has not been created";
			case OPENAVBAVDECC_RC_INVALID_CONFIG_IDX:		return "Referenced an invalid configuration descriptor index";
			case OPENAVBAVDECC_RC_PARSING_MAC_ADDRESS:		return "Parsing Mac Address";
			case OPENAVBAVDECC_RC_UNKNOWN_DESCRIPTOR:		return "Unknown descriptor";
			// No Default

		}
	}

	return "";
}


