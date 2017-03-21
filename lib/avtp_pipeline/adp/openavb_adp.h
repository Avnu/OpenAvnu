/*
 ******************************************************************
 * COPYRIGHT � Symphony Teleca
 *----------------------------------------------------------------*
 * MODULE : ADP - AVDECC Discovery Protocol Internal Interface
 *
 * PROGRAMMER : Ken Carlino (Triple Play Integration)
 * DATE :       18-Nov-2013
 * VERSION :    1.0
 *
 *----------------------------------------------------------------*
 *
 * MODULE SUMMARY : Internal Interface for the 1722.1 (AVDECC) discovery protocol
 *
 *----------------------------------------------------------------*
 *
 * MODIFICATION RECORDS
 *
 ******************************************************************
 */

#ifndef OPENAVB_ADP_H
#define OPENAVB_ADP_H 1

#include "openavb_avdecc.h"
#include "openavb_adp_pub.h"

#define OPENAVB_ADP_AVTP_SUBTYPE (0x7a)

typedef struct {
	U8 cd;
	U8 subtype;
	U8 sv;
	U8 version;
	U8 message_type;			// Redefined from control data
	U8 valid_time;				// Redefined from status
	U16 control_data_length;
	U8 entity_id[8];			// Redefined from stream_id
} openavb_adp_control_header_t;

typedef struct {
	U8 entity_model_id[8];
	U32 entity_capabilities;
	U16 talker_stream_sources;
	U16 talker_capabilities;
	U16 listener_stream_sinks;
	U16 listener_capabilities;
	U32 controller_capabilities;
	U32 available_index;
	U8 gptp_grandmaster_id[8];
	U8 gptp_domain_number;
	U8 reserved0[3];
	U16 identify_control_index;
	U16 interface_index;
	U8 association_id[8];
	U8 reserved1[4];
} openavb_adp_data_unit_t;

typedef struct {
	openavb_adp_control_header_t header;
	openavb_adp_data_unit_t pdu;
} openavb_adp_entity_info_t;

// IEEE Std 1722.1-2013 clause 6.2.3.
typedef struct {
	U64 currentTime;
	openavb_adp_entity_info_t entityInfo;
} openavb_adp_sm_global_vars_t;


openavbRC openavbAdpStart(void);
void openavbAdpStop(void);

openavbRC openavbAdpHaveTL(bool bHaveTL);

#endif // OPENAVB_ADP_H
