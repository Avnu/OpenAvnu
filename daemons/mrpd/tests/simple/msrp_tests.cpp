/******************************************************************************

  Copyright (c) 2014, AudioScience, Inc.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the AudioScience, Inc nor the names of its
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

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#ifdef __linux__
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#else
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#define PRIu64       "I64u"
#define PRIx64       "I64x"
#endif

#include "CppUTest/TestHarness.h"

extern "C"
{

#include "mrp_doubles.h"
#include "mrp.h"
#include "msrp.h"
#include "parse.h"

    extern struct msrp_database *MSRP_db;

}

// Various parameters used by MMRP, MVRP and MSRP
// (Note: Defined here in an effort to make it easier to
//        understand examples below when it comes time
//        to build commands in real code!)
#define STREAM_DA                "010203040506"
#define STREAM_ID                "DEADBEEFBADFCA11"
#define VLAN_ID                  "0002"
#define TSPEC_MAX_FRAME_SIZE     "576"
#define TSPEC_MAX_FRAME_INTERVAL "8000"
#define PRIORITY_AND_RANK        "96"
#define ACCUMULATED_LATENCY      "1000"
#define SR_CLASS_ID              "6"
#define SR_CLASS_PRIORITY        "3"
#define BRIDGE_ID                "BADC0FFEEC0FFEE0"
#define FAILURE_CODE             "1"

#define ST_PLUS_PLUS "S++:S=" STREAM_ID \
                     ",A=" STREAM_DA \
                     ",V=" VLAN_ID \
                     ",Z=" TSPEC_MAX_FRAME_SIZE \
                     ",I=" TSPEC_MAX_FRAME_INTERVAL \
                     ",P=" PRIORITY_AND_RANK \
                     ",L=" ACCUMULATED_LATENCY

static struct sockaddr_in client;

static void uint64_to_id(uint64_t v, uint8_t *id)
{
    int i;

    uint8_t *p = (uint8_t *)&v;
    for (i = 0; i < 8; i++)
    {
        int shift = (7 - i) * 8;
        id[i] = (uint8_t) (v >> shift);
    }
}

static int event_counts_per_type(int the_type, int the_event)
{
	return test_state.msrp_event_counts_per_type[MSRP_TYPE_IDX(the_type)][MSRP_EVENT_IDX(the_event)];
}


TEST_GROUP(MsrpTestGroup)
{
    void setup()
    {
        mrpd_reset();
        msrp_init(1);
    }

    void teardown()
    {
        msrp_reset();
        mrpd_reset();
    }
};

/*
 * This test registers a TalkerAdv using the same string
 * interface that a client would use and verifies the TalkerAdv
 * is registered in the MSRP database.
 */
TEST(MsrpTestGroup, RegisterTalkerAdv)
{
    struct msrp_attribute a_ref;
    struct msrp_attribute *a_msrp = NULL;
    int err_index = 0;
    int parse_status = 0;
    char id_substring[] = "S" PARSE_ASSIGN STREAM_ID;
    char cmd_string[] = ST_PLUS_PLUS;

    CHECK(MSRP_db != NULL);

    /* here we fill in a_ref struct with target values */
    uint64_to_id(0xDEADBEEFBADFCA11ull, a_ref.attribute.talk_listen.StreamID);
    a_ref.type = MSRP_TALKER_ADV_TYPE;

    /* use string interface to get MSRP to create TalkerAdv attrib in it's database */
    msrp_recv_cmd(cmd_string, sizeof(cmd_string), &client);

    /* lookup the created attrib */
    a_msrp = msrp_lookup(&a_ref);
    CHECK(a_msrp != NULL);

	/* check for NEW event */
	LONGS_EQUAL(1, event_counts_per_type(MSRP_TALKER_ADV_TYPE, MRP_EVENT_NEW));
}

/*
 * This test registers a TalkerAdv using the same string
 * interface that a client would use and verifies that after
 * the LVA timer triggers, a packet is emitted and that all
 * attributes scheduled for transmission have been sent.
 */
TEST(MsrpTestGroup, TxLVA_TalkerAdv_clear_tx_flag)
{
    struct msrp_attribute *attrib;
    int count = 128;
    int tx_flag_count = 0;
	char cmd_string[] = "S++:S=" STREAM_ID ",A=" STREAM_DA ",V=" VLAN_ID
		",Z=" TSPEC_MAX_FRAME_SIZE ",I=" TSPEC_MAX_FRAME_INTERVAL
		",P=" PRIORITY_AND_RANK ",L=" ACCUMULATED_LATENCY;

	msrp_recv_cmd(cmd_string, sizeof(cmd_string), &client);

    /*
	 * Generate a LVA event.
	 * This will cause a tx flag to be set for the attribute and then cleared when
	 * the attribute is encoded into a PDU.
	 */
    msrp_event(MRP_EVENT_LVATIMER, NULL);

    /* verify that all tx flags are zero by scanning the attribute list */
    attrib = MSRP_db->attrib_list;
    while (NULL != attrib)
    {
        tx_flag_count += attrib->applicant.tx;
        attrib = attrib->next;
    }
    CHECK(mrpd_send_packet_count() > 0);
    LONGS_EQUAL(0, tx_flag_count);
    LONGS_EQUAL(1, event_counts_per_type(MSRP_TALKER_ADV_TYPE, MRP_EVENT_NEW));
}

/*
 * This test registers a TalkerFailed using the same string
 * interface that a client would use and verifies that after
 * the LVA timer triggers, a packet is emitted and that all
 * attributes scheduled for transmission have been sent.
 */
TEST(MsrpTestGroup, TxLVA_TalkerFailed_clear_tx_flag)
{
	struct msrp_attribute *attrib;
	int tx_flag_count = 0;
	char cmd_string[] = "S++:S=" STREAM_ID ",A=" STREAM_DA ",V=" VLAN_ID
		",Z=" TSPEC_MAX_FRAME_SIZE ",I=" TSPEC_MAX_FRAME_INTERVAL
		",P=" PRIORITY_AND_RANK ",L=" ACCUMULATED_LATENCY
		",B=" BRIDGE_ID ",C=" FAILURE_CODE;

	/* declare single TalkerFailed */
	msrp_recv_cmd(cmd_string, strlen(cmd_string) + 1, &client);

	/*
	* Generate a LVA event.
	* This will cause a tx flag to be set for the attribute and then cleared when
	* the attribute is encoded into a PDU.
	*/
	msrp_event(MRP_EVENT_LVATIMER, NULL);

	/* verify that all tx flags are zero by scanning the attribute list */
	attrib = MSRP_db->attrib_list;
	while (NULL != attrib)
	{
		tx_flag_count += attrib->applicant.tx;
		attrib = attrib->next;
	}
	CHECK(mrpd_send_packet_count() > 0);
	LONGS_EQUAL(0, tx_flag_count);
	LONGS_EQUAL(1, event_counts_per_type(MSRP_TALKER_FAILED_TYPE, MRP_EVENT_NEW));
}

/*
 * This test registers a Listener using the same string
 * interface that a client would use and verifies that after
 * the LVA timer triggers, a packet is emitted and that all
 * attributes scheduled for transmission have been sent.
 */
TEST(MsrpTestGroup, TxLVA_Listener_clear_tx_flag)
{
	struct msrp_attribute *attrib;
	int tx_flag_count = 0;
	char cmd_string[] = "S+L:L=" STREAM_ID ",D=2";

	/* declare single Listener */
	msrp_recv_cmd(cmd_string, sizeof(cmd_string), &client);

	/*
	* Generate a LVA event.
	* This will cause a tx flag to be set for the attribute and then cleared when
	* the attribute is encoded into a PDU.
	*/
	msrp_event(MRP_EVENT_LVATIMER, NULL);

	/* verify that all tx flags are zero by scanning the attribute list */
	attrib = MSRP_db->attrib_list;
	while (NULL != attrib)
	{
		tx_flag_count += attrib->applicant.tx;
		attrib = attrib->next;
	}
	CHECK(mrpd_send_packet_count() > 0);
	LONGS_EQUAL(0, tx_flag_count);
	LONGS_EQUAL(1, event_counts_per_type(MSRP_LISTENER_TYPE, MRP_EVENT_NEW));
}

/*
 * This test registers a Domain using the same string
 * interface that a client would use and verifies that after
 * the LVA timer triggers, a packet is emitted and that all
 * attributes scheduled for transmission have been sent.
 */
TEST(MsrpTestGroup, TxLVA_Domain_clear_tx_flag)
{
	struct msrp_attribute *attrib;
	int tx_flag_count = 0;
	char cmd_string[] = "S+D:C=" SR_CLASS_ID
		",P=" SR_CLASS_PRIORITY
		",V=" VLAN_ID;

	/* declare single Domain */
	msrp_recv_cmd(cmd_string, sizeof(cmd_string), &client);

	/*
	* Generate a LVA event.
	* This will cause a tx flag to be set for the attribute and then cleared when
	* the attribute is encoded into a PDU.
	*/
	msrp_event(MRP_EVENT_LVATIMER, NULL);

	/* verify that all tx flags are zero by scanning the attribute list */
	attrib = MSRP_db->attrib_list;
	while (NULL != attrib)
	{
		tx_flag_count += attrib->applicant.tx;
		attrib = attrib->next;
	}
	CHECK(mrpd_send_packet_count() > 0);
	LONGS_EQUAL(0, tx_flag_count);
	LONGS_EQUAL(1, event_counts_per_type(MSRP_DOMAIN_TYPE, MRP_EVENT_JOIN));
}

/*
 * This test registers 64 TalkerAdvertises using the same string
 * interface that a client would use and verifies that after
 * the LVA timer triggers, a packet is emitted and that all
 * attributes scheduled for transmission have been sent.
 */
TEST(MsrpTestGroup, TxLVA_TalkerAdv_count_64)
{
	struct msrp_attribute *attrib;
	char cmd_string[128];
	uint64_t id = 0xbadc0ffeeull;
	uint64_t da = 0xdeadbeefull;
	int count = 64;
	int tx_flag_count = 0;
	int i;

	/* declare count TalkerAdv */
	for (i = 0; i < count; i++)
	{
		snprintf(cmd_string, sizeof(cmd_string),
			"S++:S=%" PRIx64 ",A=%" PRIx64 ",V=" VLAN_ID ",Z=" TSPEC_MAX_FRAME_SIZE
			",I=" TSPEC_MAX_FRAME_INTERVAL ",P=" PRIORITY_AND_RANK ",L=" ACCUMULATED_LATENCY,
			id, da);
		msrp_recv_cmd(cmd_string, strlen(cmd_string) + 1, &client);
		/* add 2 to prevent vectorizing */
		id += 2;
		da += 2;
	}

	/* generate a LVA */
	msrp_event(MRP_EVENT_LVATIMER, NULL);

	/* verify that all tx flags are zero */
	attrib = MSRP_db->attrib_list;
	while (NULL != attrib)
	{
		tx_flag_count += attrib->applicant.tx;
		attrib = attrib->next;
	}
	CHECK(mrpd_send_packet_count() > 0);
	CHECK_EQUAL(0, tx_flag_count);
	LONGS_EQUAL(count, event_counts_per_type(MSRP_TALKER_ADV_TYPE, MRP_EVENT_NEW));
}
