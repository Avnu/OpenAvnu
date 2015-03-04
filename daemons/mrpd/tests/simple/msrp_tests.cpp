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
#include "msrp_tests.h"
#include "mrp.h"
#include "msrp.h"
#include "parse.h"
#include "eui64set.h"

/* Most MSRP commands operate on the global DB */
extern struct msrp_database *MSRP_db;

void msrp_event_observer(int event, struct msrp_attribute *attr);
char *msrp_attrib_type_string(int t);
char *mrp_event_string(int e);

}

/* This is from a live capture; it contains several messages with Mt
* and JoinMt events for Talker Advertise, Listener, and Domain
* VectorAttributes. */
static unsigned char pkt2[] = {
	0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e, /* Destination MAC */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0x58, /* Source MAC */
	0x22, 0xea,                         /* Ethertype */

	0x00,         /* Protocol Version */

	/* Message Start */
	0x01,         /* Attribute Type - Talker Advertise */
	0x19,         /* Attribute FirstValue Length */
	0x00, 0xc4,   /* Attribute ListLength */

	/* Vector Header */
	0x00, 0x01,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0x4d, 0x00, 0x00, /* ID 0x000fd700234d0000 */
	0x91, 0xe0, 0xf0, 0x00, 0xb7, 0x1a, 0x00, 0x00,
	0x00, 0x38, 0x00, 0x01, 0x60, 0x00, 0x02, 0x1f,
	0xd8,
	/* ThreePackedEvents */
	0x90,

	/* Vector Header */
	0x00, 0x13,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0x4d, 0x00, 0x03,
	0x91, 0xe0, 0xf0, 0x00, 0xb7, 0x1d, 0x00, 0x00,
	0x00, 0x38, 0x00, 0x01, 0x70, 0x00, 0x02, 0x1f,
	0xd8,
	/* ThreePackedEvents */
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0x90,

	/* Vector Header */
	0x00, 0x0d,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0x58, 0x00, 0x01,
	0x91, 0xe0, 0xf0, 0x00, 0x88, 0x3d, 0x00, 0x00,
	0x00, 0x38, 0x00, 0x01, 0x70, 0x00, 0x00, 0x01,
	0xf4,
	/* ThreePackedEvents */
	0x81, 0x81, 0x81, 0x81, 0x6c,

	/* Vector Header */
	0x00, 0x13,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0xba, 0x00, 0x03,
	0x91, 0xe0, 0xf0, 0x00, 0x4c, 0x40, 0x00, 0x00,
	0x00, 0x38, 0x00, 0x01, 0x70, 0x00, 0x02, 0x1f,
	0xd8,
	/* ThreePackedEvents */
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0x90,

	/* Vector Header */
	0x00, 0x06,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x01, 0xa0, 0x05, 0x00, 0x01,
	0x91, 0xe0, 0xf0, 0x00, 0xdd, 0xd1, 0x00, 0x00,
	0x00, 0x38, 0x00, 0x01, 0x73, 0x00, 0x02, 0x1f,
	0xd8,
	/* ThreePackedEvents */
	0xac, 0xac,

	/* Vector Header */
	0x00, 0x1c,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x01, 0xa0, 0x16, 0x00, 0x03,
	0x91, 0xe0, 0xf0, 0x00, 0x6f, 0xdc, 0x00, 0x00,
	0x00, 0x38, 0x00, 0x01, 0x70, 0x00, 0x02, 0x1f,
	0xd8,
	/* ThreePackedEvents */
	0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac, 0xac,
	0xac, 0x90,

	0x00, 0x00, /* EndMark */

	/* Message Start */
	0x03,         /* Attribute Type - Listener */
	0x08,         /* Attribute FirstValue Length */
	0x00, 0x63,   /* Attribute ListLength */

	/* Vector Header */
	0x00, 0x01,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0x4d, 0x01, 0x00,
	/* ThreePackedEvents */
	0x6c,
	/* FourPackedEvents */
	0x80,

	/* Vector Header */
	0x00, 0x13,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0x4d, 0x02, 0x03,
	/* ThreePackedEvents */
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x6c,
	/* FourPackedEvents */
	0xaa, 0xaa, 0xaa, 0xaa, 0xa8,

	/* Vector Header */
	0x00, 0x13,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x00, 0x23, 0xba, 0x003, 0x03,
	/* ThreePackedEvents */
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x6c,
	/* FourPackedEvents */
	0xaa, 0xaa, 0xaa, 0xaa, 0xa8,

	/* Vector Header */
	0x00, 0x06,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x01, 0xa0, 0x05, 0x04, 0x01,
	/* ThreePackedEvents */
	0x81, 0x81,
	/* FourPackedEvents */
	0xaa, 0xa0,

	/* Vector Header */
	0x00, 0x1c,
	/* FirstValue */
	0x00, 0x0f, 0xd7, 0x01, 0xa0, 0x16, 0x05, 0x03,
	/* ThreePackedEvents */
	0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81,
	0x81, 0x6c,
	/* FourPackedEvents */
	0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,

	0x00, 0x00, /* EndMark */

	/* Message Start */
	0x04,          /* Attribute Type - Domain */
	0x04,          /* Attribute FirstValue Length */
	0x00, 0x09,    /* Attribute ListLength */

	/* Vector Header */
	0x00, 0x01,
	/* FirstValue */
	0x06, 0x03, 0x00, 0x02,
	/* ThreePackedEvents */
	0x6c,

	0x00, 0x00, /* EndMark */

	0x00, 0x00  /* EndMark */
};



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

int msrp_tests_event_counts_per_type(int the_type, int the_event)
{
	return test_state.msrp_event_counts_per_type[MSRP_TYPE_IDX(the_type)][MSRP_EVENT_IDX(the_event)];
}

int msrp_tests_cmd_ok(const char *zstr)
{
	int status;

	status = (zstr[0] != 'E') && (zstr[1] != 'R');

		return status;
}

TEST_GROUP(MsrpTestGroup)
{
    void setup()
    {
        mrpd_reset();
        msrp_init(1, MSRP_INTERESTING_STREAM_ID_COUNT, 0);
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
    eui64_write(a_ref.attribute.talk_listen.StreamID, 0xDEADBEEFBADFCA11ull);
    a_ref.type = MSRP_TALKER_ADV_TYPE;

    /* use string interface to get MSRP to create TalkerAdv attrib in it's database */
    msrp_recv_cmd(cmd_string, sizeof(cmd_string), &client);
    CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));

    /* lookup the created attrib */
    a_msrp = msrp_lookup(&a_ref);
    CHECK(a_msrp != NULL);

    /* check for NEW event */
    LONGS_EQUAL(1, msrp_tests_event_counts_per_type(MSRP_TALKER_ADV_TYPE, MRP_EVENT_NEW));
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
	CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));

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
    LONGS_EQUAL(1, msrp_tests_event_counts_per_type(MSRP_TALKER_ADV_TYPE, MRP_EVENT_NEW));
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
	CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));

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
	LONGS_EQUAL(1, msrp_tests_event_counts_per_type(MSRP_TALKER_FAILED_TYPE, MRP_EVENT_NEW));
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
	CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));

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
	LONGS_EQUAL(1, msrp_tests_event_counts_per_type(MSRP_LISTENER_TYPE, MRP_EVENT_NEW));
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
	CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));

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
	LONGS_EQUAL(1, msrp_tests_event_counts_per_type(MSRP_DOMAIN_TYPE, MRP_EVENT_JOIN));
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
		CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));
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
	LONGS_EQUAL(count, msrp_tests_event_counts_per_type(MSRP_TALKER_ADV_TYPE, MRP_EVENT_NEW));
}

/*
 * Without pruning enabled, more than one client is supported.
 */
TEST(MsrpTestGroup, Multiple_Clients)
{
	static struct sockaddr_in client1;
	static struct sockaddr_in client2;

	memset(&client1, 0, sizeof(client1));
	memset(&client2, 1, sizeof(client2));

	/* no error returned for first client */
	msrp_recv_cmd("S??", strlen("S??") + 1, &client1);
	CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));

	/* no error returned for second client */
	msrp_recv_cmd("S??", strlen("S??") + 1, &client2);
	CHECK(msrp_tests_cmd_ok(test_state.ctl_msg_data));
}


/**********************************************************************************/

/*
 * This test enables interesting StreamID checking that will prune
 * all "uninteresting" IDs from the MSRP attribute database.
 */
TEST(MsrpTestGroup, Prune_Uninteresting_TA)
{
	struct msrp_attribute a_ref;
	struct msrp_attribute *attrib;
	int tx_flag_count = 0;
	int rv;

	/* enable pruning */
	msrp_recv_cmd("I+P", strlen("I+P") + 1, &client);
	LONGS_EQUAL(0, msrp_interesting_id_count());

	/* here we fill in a_ref struct with target values, ID comes from inspection of pkt2 */
	eui64_write(a_ref.attribute.talk_listen.StreamID, 0x000fd700234d0000ull);
	a_ref.type = MSRP_TALKER_ADV_TYPE;

	memcpy(test_state.rx_PDU, pkt2, sizeof pkt2);
	test_state.rx_PDU_len = sizeof pkt2;
	rv = msrp_recv_msg();
	LONGS_EQUAL(0, rv);

	/* lookup the created attrib (it should not be present) */
	attrib = msrp_lookup(&a_ref);
	CHECK(attrib == NULL);

	/* no interesting stream ids */
	LONGS_EQUAL(0, msrp_interesting_id_count());
}

/*
 * This test enables interesting StreamID checking that will prune
 * all "uninteresting" IDs from the attribute database. The id
 * of a single TalkerAdvertise is marked as interesting.
 */
TEST(MsrpTestGroup, Prune_Uninteresting_Except_One_TA)
{
	struct msrp_attribute a_ref;
	struct msrp_attribute *attrib;
	uint64_t id = 0x000fd70023580001; /* see pkt2 at top of this file */
	char cmd_string[128];
	int tx_flag_count = 0;
	int rv;

	/* enable pruning */
	msrp_recv_cmd("I+P", strlen("I+P") + 1, &client);
	LONGS_EQUAL(0, msrp_interesting_id_count());

	snprintf(cmd_string, sizeof(cmd_string), "I+S:S=%" PRIx64, id);
	msrp_recv_cmd(cmd_string, strlen(cmd_string) + 1, &client);
	LONGS_EQUAL(1, msrp_interesting_id_count());

	/* here we fill in a_ref struct with target values */
	eui64_write(a_ref.attribute.talk_listen.StreamID, id);
	a_ref.type = MSRP_TALKER_ADV_TYPE;

	memcpy(test_state.rx_PDU, pkt2, sizeof pkt2);
	test_state.rx_PDU_len = sizeof pkt2;
	//test_state.msrp_observe = msrp_event_observer;
	rv = msrp_recv_msg();
	LONGS_EQUAL(0, rv);

	/* lookup the created attrib (it should be present) */
	attrib = msrp_lookup(&a_ref);
	CHECK(attrib != NULL);
}

/*
 * This test enables interesting StreamID checking that will prune
 * all "uninteresting" IDs from the attribute database. With
 * pruning enabled, make sure incoming listener attributes are
 * processed.
 */
TEST(MsrpTestGroup, Prune_Uninteresting_Except_Any_Listener)
{
	struct msrp_attribute a_ref;
	struct msrp_attribute *attrib;
	uint64_t id = 0x000fd700234d0203; /* see pkt2 at top of this file */
	char cmd_string[128];
	int tx_flag_count = 0;
	int rv;

	/* enable pruning */
	msrp_recv_cmd("I+P", strlen("I+P") + 1, &client);

	/* here we fill in a_ref struct with target values */
	eui64_write(a_ref.attribute.talk_listen.StreamID, id);
	a_ref.type = MSRP_LISTENER_TYPE;

	memcpy(test_state.rx_PDU, pkt2, sizeof pkt2);
	test_state.rx_PDU_len = sizeof pkt2;
	//test_state.msrp_observe = msrp_event_observer;
	rv = msrp_recv_msg();
	LONGS_EQUAL(0, rv);

	/* lookup the created attrib (it should be present) */
	attrib = msrp_lookup(&a_ref);
	CHECK(attrib != NULL);
}

/*
 * This test enables interesting StreamID checking that will prune
 * all "uninteresting" IDs from the attribute database. With
 * pruning enabled, a registered listener attribute should cause
 * the matching TalkerAdv to be processed correctly.
 */
TEST(MsrpTestGroup, Prune_Uninteresting_Except_New_Listener)
{
	struct msrp_attribute a_ref;
	struct msrp_attribute *attrib;
	uint64_t id = 0x000fd70023580001; /* see pkt2 at top of this file */
	char cmd_string[128];
	int tx_flag_count = 0;
	int rv;

	/* enable pruning */
	msrp_recv_cmd("I+P", strlen("I+P") + 1, &client);

	/* declare a listener attribute */
	snprintf(cmd_string, sizeof(cmd_string), "S+L:L=%016" PRIx64 ",D=2", id);
	msrp_recv_cmd(cmd_string, strlen(cmd_string) + 1, &client);

	memcpy(test_state.rx_PDU, pkt2, sizeof pkt2);
	test_state.rx_PDU_len = sizeof pkt2;
	//test_state.msrp_observe = msrp_event_observer;
	rv = msrp_recv_msg();
	LONGS_EQUAL(0, rv);

	/* lookup the created attrib (it should be present) */
	eui64_write(a_ref.attribute.talk_listen.StreamID, id);
	a_ref.type = MSRP_TALKER_ADV_TYPE;
	attrib = msrp_lookup(&a_ref);
	CHECK(attrib != NULL);
}

/*
 * This test checks behaviour when interesting StreamId
 * checking is enabled after some number of talker attributes
 * are present in the MSRP database.
 *
 * The action of enabling StreamID checking causes all TA and TF
 * to be deleted from the MSRP database if and only if there is
 * no matching listener attribute. The interesting StreamID database
 * is completely deleted/emptied. The delete operation is silent
 * in that there is no client notification or LVA sent.
 */

// Remove option completely - make it a runtime option.
// Make it a daemon startup option.

//TEST(MsrpTestGroup, Prune_Uninteresting_LateEnable)


/*
 * This test checks behaviour when pruning is enabled and a
 * talker stream that was interesting is now marked as not
 * interesting. In this case there is no matching listener
 * attribute.
 *
 * Because there is not matching listener attribute, the
 * talker attribute (TA of TF) is deleted from the MRSP
 * database. The ID is all deleted from the interesting IDs
 * database. The delete is silent and there is no client
 * notification.
 */
// Multi-client cases should be be run without "pruning"
// enabled.
// "I+S:ID"
//TEST(MsrpTestGroup, Prune_Uninteresting_Disable_With_TA)

/*
 * This test checks behaviour when pruning is enabled and a
 * talker streamID that was previously marked as interesting
 * is now marked as not interesting. In this case there is a
 * matching listener attribute.
 *
 * Because there is a matching listener attribute, the StreamID
 * is deleted from the interesting StreamID database. Talker attribute
 * registrations remain.
 */
//TEST(MsrpTestGroup, Prune_Uninteresting_Disable_With_TA_and_Listener)



/*
 * This test checks behaviour when pruning is enabled and then
 * disabled. And pdu with TA is processed.
 *
 * After enabling and disabling streamID checking default operation
 * conditions should prevail and any TA or TF should be recorded in
 * the MSRP database.
 */
// Delete. Enabling pruning is a runtime option only.
//TEST(MsrpTestGroup, Prune_Uninteresting_Enable_Disable)
