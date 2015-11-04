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

/*
* MODULE SUMMARY : Common types and defines for result codes
*/

#ifndef AVB_RESULT_CODES_H
#define AVB_RESULT_CODES_H 1

// Result Code (success or error code)
//   First (left) byte:
//     bits 0 thru 6 reserved;
//     bit 7 (rightmost) - 0 indicates success, 1 indicates failure.
//   Second Byte:
//     indicates specific software module - see enum openavbModules below
//   Third & Forth (right) Bytes;
//     defined independently by each module to indicate specific failure (or success)
typedef U32 openavbRC;

// Example usage:
// 	openavbRC foo()
//  {
// 		.
// 		.
// 		return AVB_RC(OPENAVB_AVTP_FAILURE | OPENAVB_RC_OUT_OF_MEMORY)
// 	}
//
//	openavbRC result;
//	result = foo();
//	if (IS_OPENAVB_SUCCESS(result)) {
//		...
//	}

#define OPENAVB_RC_MODULE_MASK		0x00FF0000
#define OPENAVB_RC_CODE_MASK		0x0000FFFF

#define OPENAVB_SUCCESS  0x00000000
#define OPENAVB_FAILURE  0x01000000

// 2nd byte only
enum openavbModules {
	OPENAVB_MODULE_GLOBAL					= 0x00010000,
	OPENAVB_MODULE_GPTP						= 0x00020000,
	OPENAVB_MODULE_SRP						= 0x00030000,
	OPENAVB_MODULE_AVTP						= 0x00040000,
	OPENAVB_MODULE_AVTP_TIME					= 0x00050000,
	OPENAVB_MODULE_AVDECC					= 0x00060000,
};

// When adding result codes be sure to update the openavbUtilRCCodeToString() function in openavb_result_codes.c

enum openavbCommonResultCodes {
	OPENAVB_RC_GENERIC						= 0x1000,
	OPENAVB_RC_RAWSOCK_OPEN					= 0x1001, // Failed to open rawsock
	OPENAVB_RC_OUT_OF_MEMORY					= 0x1002, // Out of memory
	OPENAVB_RC_INVALID_ARGUMENT				= 0x1003, // Invalid function argument
	OPENAVB_RC_FAILED_TO_OPEN				= 0x1004, // Failed to open
};                                  		

enum openavbPtpResultCodes {            		
	OPENAVBPTP_RC_GENERIC						= 0x0000,
	OPENAVBPTP_RC_SHARED_MEMORY_OPEN  			= 0x0001, // Failed to open shared memory file
	OPENAVBPTP_RC_SHARED_MEMORY_TRANC 			= 0x0002, // Failed to truncate shared memory file
	OPENAVBPTP_RC_SHARED_MEMORY_MMAP  			= 0x0003, // Failed to memory map shared memory file
	OPENAVBPTP_RC_SHARED_MEMORY_ENTRY 			= 0x0004, // Failed to locate matching shared memory item
	OPENAVBPTP_RC_SHARED_MEMORY_UPDATE			= 0x0005, // Failed to update shared memory item
	OPENAVBPTP_RC_PTP_DEV_OPEN					= 0x0006, // Failed to open ptp device
	OPENAVBPTP_RC_PTP_DEV_CLOCKID 				= 0x0007, // Failed to obtain ptp device clock ID
	OPENAVBPTP_RC_SOCK_OPEN  					= 0x0008, // Failed to open socket
	OPENAVBPTP_RC_SOCK_NET_INTERFACE  			= 0x0009, // Unable to obtain network interface
	OPENAVBPTP_RC_SOCK_DEVICE_INDEX				= 0x0010, // Unable to obtain socket device index										  //
	OPENAVBPTP_RC_SOCK_REUSE 					= 0x0011, // unable to reuse socket
	OPENAVBPTP_RC_SOCK_BIND  					= 0x0012, // Unable to bind socket
	OPENAVBPTP_RC_SOCK_TIMESTAMP 				= 0x0013, // Hardware timestamping not supported
	OPENAVBPTP_RC_SOCK_LINK_DOWN  				= 0x0014, // Socket network link not active
	OPENAVBPTP_RC_TIMER_CREATE   				= 0x0015, // Failed to create timer(s)
	OPENAVBPTP_RC_SIGNAL_HANDLER 				= 0x0016, // Failed to create signal handler
	OPENAVBPTP_RC_CONFIG_FILE_OPEN				= 0x0017, // Failed to open configuration file
	OPENAVBPTP_RC_CONFIG_FILE_READ				= 0x0018, // Failed to read configuration file
	OPENAVBPTP_RC_CONFIG_FILE_DATA				= 0x0019, // Invalid data encountered in configuration file
	OPENAVBPTP_RC_CONFIG_FILE_WRITE   			= 0x0020, // Failed to write configuration file
	OPENAVBPTP_RC_NEW_CONFIG_FILE_WRITE			= 0x0021, // SUCCESSFULLY wrote recreated configuration file
	OPENAVBPTP_RC_CLOCK_GET_TIME  				= 0x0022, // Failed to obtain time
	OPENAVBPTP_RC_SEND_FAIL  					= 0x0023, // Failed to send packet
	OPENAVBPTP_RC_SEND_SHORT 					= 0x0024, // Failed to send complete packet
	OPENAVBPTP_RC_SOCK_ADD_MULTI  				= 0x0025, // Failed to set multicast address
	OPENAVBPTP_RC_TX_TIMESTAMP_FAIL				= 0x0026, // Failed to get egress timestamp
};                                  		

enum openavbSrpResultCodes {            		
	OPENAVBSRP_RC_GENERIC						= 0x0000,
	OPENAVBSRP_RC_ALREADY_INIT   				= 0x0001, // Already initialized
	OPENAVBSRP_RC_SOCK_OPEN  					= 0x0002, // Failed to open socket
	OPENAVBSRP_RC_THREAD_CREATE  				= 0x0003, // Failed to create thread
	OPENAVBSRP_RC_BAD_CLASS  					= 0x0004, // Attempt to register / access stream with a bad class index
	OPENAVBSRP_RC_REG_NULL_HDL					= 0x0005, // Attempt to register a stream with a null handle
	OPENAVBSRP_RC_REREG 						= 0x0006, // Attempt to [re]register an exisiting stream Id
	OPENAVBSRP_RC_NO_MEMORY 					= 0x0007, // Out of memory
	OPENAVBSRP_RC_NO_BANDWIDTH  				= 0x0008, // Insufficient bandwidth
	OPENAVBSRP_RC_SEND  						= 0x0009, // Failed to send packet
	OPENAVBSRP_RC_DEREG_NO_XST					= 0x0010, // Attempt to deregister a non-existent stream
	OPENAVBSRP_RC_DETACH_NO_XST   				= 0x0011, // Attempt to detach from non-existent stream
	OPENAVBSRP_RC_INVALID_SUBTYPE  				= 0x0012, // Invalid listener declaration subtype
	OPENAVBSRP_RC_FRAME_BUFFER  				= 0x0013, // Failed to get a transmit frame buffer
	OPENAVBSRP_RC_SHARED_MEM_MMAP 				= 0x0014, // Failed to mmap openavb_gPTP shared memory
	OPENAVBSRP_RC_SHARED_MEM_OPEN 				= 0x0015, // Failed to open openavb_gPTP shared memory
	OPENAVBSRP_RC_BAD_VERSION					= 0x0016, // AVB core stack version mismatch endpoint / srp vs gptp
	OPENAVBSRP_RC_SOCK_JN_MULTI                 = 0x0017, // Failed to join multicast group for the Nearest Bridge group address
	OPENAVBSRP_RC_SOCK_ADDR                     = 0x0018, // Failed to get our own mac address
	OPENAVBSRP_RC_MUTEX_INIT                    = 0x0019, // Failed to create / initialize mutex
	OPENAVBSRP_RC_SOCK_TX_HDR                   = 0x0020, // Failed to set Ethernet frame transmit header
// limited to two bytes; max 0xffff 		
};                                  		

enum openavbAVTPResultCodes {           		
	OPENAVBAVTP_RC_GENERIC						= 0x0000,
	OPENAVBAVTP_RC_TX_PACKET_NOT_READY			= 0x0001, // Transmit packet not ready
	OPENAVBAVTP_RC_MAPPING_CB_NOT_SET			= 0x0002, // Mapping callback structure not set
	OPENAVBAVTP_RC_INTERFACE_CB_NOT_SET			= 0x0003, // Interface callback structure not set
	OPENAVBAVTP_RC_NO_FRAMES_PROCESSED			= 0x0004, // No frames processed
	OPENAVBAVTP_RC_INVALID_AVTP_VERSION			= 0x0005, // Invalid AVTP version configured
	OPENAVBAVTP_RC_IGNORING_STREAMID			= 0x0006, // Ignoring unexpected streamID
	OPENAVBAVTP_RC_IGNORING_CONTROL_PACKET		= 0x0007, // Ignoring unexpected AVTP control packet
	OPENAVBAVTP_RC_PARSING_FRAME_HEADER			= 0x0008, // Parsing frame header
};                                  		

enum openavbAVTPTimeResultCodes {       		
	OPENAVBAVTPTIME_RC_GENERIC					= 0x0000,
	OPENAVBAVTPTIME_RC_PTP_TIME_DESCRIPTOR		= 0x0001, // PTP file descriptor not available
	OPENAVBAVTPTIME_RC_OPEN_SRC_PTP_NOT_AVAIL	= 0x0002, // Open source PTP configured but not available in this build
	OPENAVBAVTPTIME_RC_GET_TIME_ERROR			= 0x0003, // PTP get time error
	OPENAVBAVTPTIME_RC_OPENAVB_PTP_NOT_AVAIL		= 0x0004, // OPENAVB PTP configured but not available in this build
	OPENAVBAVTPTIME_RC_INVALID_PTP_TIME			= 0x0005, // Invalid avtp_time_t
};                                  		

enum openavbAVDECCResultCodes {           		
	OPENAVBAVDECC_RC_GENERIC					= 0x0000,
	OPENAVBAVDECC_RC_BUFFER_TOO_SMALL			= 0x0001, // Buffer size is too small
	OPENAVBAVDECC_RC_ENTITY_MODEL_MISSING		= 0x0002, // The Entity Model has not been created
	OPENAVBAVDECC_RC_INVALID_CONFIG_IDX			= 0x0003, // Referenced an invalid configuration descriptor index
	OPENAVBAVDECC_RC_PARSING_MAC_ADDRESS		= 0x0004, // Parsing Mac Address
	OPENAVBAVDECC_RC_UNKNOWN_DESCRIPTOR			= 0x0005, // Unknown descriptor
};                                  		


#define OPENAVB_PTP_SUCCESS			(OPENAVB_SUCCESS | OPENAVB_MODULE_GPTP)
#define OPENAVB_PTP_FAILURE			(OPENAVB_FAILURE | OPENAVB_MODULE_GPTP)

#define OPENAVB_SRP_SUCCESS			(OPENAVB_SUCCESS | OPENAVB_MODULE_SRP)
#define OPENAVB_SRP_FAILURE			(OPENAVB_FAILURE | OPENAVB_MODULE_SRP)

#define OPENAVB_AVTP_SUCCESS		(OPENAVB_SUCCESS | OPENAVB_MODULE_AVTP)
#define OPENAVB_AVTP_FAILURE		(OPENAVB_FAILURE | OPENAVB_MODULE_AVTP)

#define OPENAVB_AVTP_TIME_SUCCESS	(OPENAVB_SUCCESS | OPENAVB_MODULE_AVTP_TIME)
#define OPENAVB_AVTP_TIME_FAILURE	(OPENAVB_FAILURE | OPENAVB_MODULE_AVTP_TIME)

#define OPENAVB_AVDECC_SUCCESS		(OPENAVB_SUCCESS | OPENAVB_MODULE_AVDECC)
#define OPENAVB_AVDECC_FAILURE		(OPENAVB_FAILURE | OPENAVB_MODULE_AVDECC)


// Helper functions
openavbRC openavbUtilRCRecord(openavbRC rc);
char* openavbUtilRCResultToString(openavbRC rc);
char* openavbUtilRCModuleToString(openavbRC rc);
char* openavbUtilRCCodeToString(openavbRC rc);

// Helper macros
#define IS_OPENAVB_SUCCESS(_rc_) 				(!((_rc_) & 0x01000000))
#define IS_OPENAVB_FAILURE(_rc_)   				((_rc_) & 0x01000000)
#define AVB_RC(_rc_)							(openavbUtilRCRecord(_rc_))
#define AVB_RC_RET(_rc_)						return (_rc_)
#define AVB_RC_MSG(_rc_)						openavbUtilRCCodeToString(_rc_)
#define AVB_RC_LOG(_rc_)						AVB_LOGF_ERROR("%s", openavbUtilRCCodeToString(_rc_))
#define AVB_RC_LOG_RET(_rc_)					AVB_LOGF_ERROR("%s", openavbUtilRCCodeToString(_rc_)); return (_rc_)
#define AVB_RC_LOG_TRACE_RET(_rc_, _trace_)		AVB_LOGF_ERROR("%s", openavbUtilRCCodeToString(_rc_)); AVB_TRACE_EXIT(_trace_); return (_rc_)
#define AVB_RC_TRACE_RET(_rc_, _trace_)			AVB_TRACE_EXIT(_trace_); return (_rc_)
#define AVB_RC_FAIL_RET(_rc_)					if (IS_OPENAVB_FAILURE(_rc_)) return (_rc_)
#define AVB_RC_FAIL_TRACE_RET(_rc_, _trace_)	if (IS_OPENAVB_FAILURE(_rc_)) {AVB_TRACE_EXIT(_trace_); return (_rc_);}

#endif // AVB_RESULT_CODES_H
