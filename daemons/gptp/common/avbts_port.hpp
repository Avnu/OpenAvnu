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

#ifndef AVBTS_PORT_HPP
#define AVBTS_PORT_HPP

#include <ieee1588.hpp>
#include <avbts_message.hpp>
#include <avbap_message.hpp>

#include <avbts_ostimer.hpp>
#include <avbts_oslock.hpp>
#include <avbts_osnet.hpp>
#include <avbts_osthread.hpp>
#include <avbts_oscondition.hpp>
#include <ipcdef.hpp>
#include <gptp_log.hpp>

#include <stdint.h>

#include <map>
#include <list>

/**@file*/

#define GPTP_MULTICAST 0x0180C200000EULL		/*!< GPTP multicast adddress */
#define PDELAY_MULTICAST GPTP_MULTICAST			/*!< PDELAY Multicast value */
#define OTHER_MULTICAST GPTP_MULTICAST			/*!< OTHER multicast value */
#define TEST_STATUS_MULTICAST 0x011BC50AC000ULL	/*!< AVnu Automotive profile test status msg Multicast value */

#define PDELAY_RESP_RECEIPT_TIMEOUT_MULTIPLIER 3	/*!< PDelay timeout multiplier*/
#define SYNC_RECEIPT_TIMEOUT_MULTIPLIER 3			/*!< Sync receipt timeout multiplier*/
#define ANNOUNCE_RECEIPT_TIMEOUT_MULTIPLIER 3		/*!< Announce receipt timeout multiplier*/

#define LOG2_INTERVAL_INVALID -127	/* Simple out of range Log base 2 value used for Sync and PDelay msg internvals */

/**
 * @brief PortType enumeration. Selects between delay request-response (E2E) mechanism
 * or PTPV1 or PTPV2 P2P (peer delay) mechanism.
 */
typedef enum {
	V1,
	V2_E2E,
	V2_P2P
} PortType;

/**
 * @brief PortIdentity interface
 * Defined at IEEE 802.1AS Clause 8.5.2
 */
class PortIdentity {
private:
	ClockIdentity clock_id;
	uint16_t portNumber;
public:
	/**
	 * @brief Default Constructor
	 */
	PortIdentity() { };

	/**
	 * @brief  Constructs PortIdentity interface.
	 * @param  clock_id Clock ID value as defined at IEEE 802.1AS Clause 8.5.2.2
	 * @param  portNumber Port Number
	 */
	PortIdentity(uint8_t * clock_id, uint16_t * portNumber) {
		this->portNumber = *portNumber;
		this->portNumber = PLAT_ntohs(this->portNumber);
		this->clock_id.set(clock_id);
	}

	/**
	 * @brief  Implements the operator '!=' overloading method. Compares clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value differs from the object's PortIdentity value. FALSE otherwise.
	 */
	bool operator!=(const PortIdentity & cmp) const {
		return
			!(this->clock_id == cmp.clock_id) ||
			this->portNumber != cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '==' overloading method. Compares clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value equals to the object's PortIdentity value. FALSE otherwise.
	 */
	bool operator==(const PortIdentity & cmp)const {
		return
			this->clock_id == cmp.clock_id &&
			this->portNumber == cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '<' overloading method. Compares clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value is lower than the object's PortIdentity value. FALSE otherwise.
	 */
	bool operator<(const PortIdentity & cmp)const {
		return
			this->clock_id < cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber < cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Implements the operator '>' overloading method. Compares clock_id and portNumber.
	 * @param  cmp Constant PortIdentity value to be compared against.
	 * @return TRUE if the comparison value is greater than the object's PortIdentity value. FALSE otherwise.
	 */
	bool operator>(const PortIdentity & cmp)const {
		return
			this->clock_id > cmp.clock_id ?
			true : this->clock_id == cmp.clock_id &&
			this->portNumber > cmp.portNumber ? true : false;
	}

	/**
	 * @brief  Gets the ClockIdentity string
	 * @param  id [out] Pointer to an array of octets.
	 * @return void
	 */
	void getClockIdentityString(uint8_t *id) {
		clock_id.getIdentityString(id);
	}

	/**
	 * @brief  Sets the ClockIdentity.
	 * @param  clock_id Clock Identity to be set.
	 * @return void
	 */
	void setClockIdentity(ClockIdentity clock_id) {
		this->clock_id = clock_id;
	}

	/**
	 * @brief  Gets the clockIdentity value
	 * @return A copy of Clock identity value.
	 */
	ClockIdentity getClockIdentity( void ) {
		return this->clock_id;
	}

	/**
	 * @brief  Gets the port number following the network byte order, i.e. Big-Endian.
	 * @param  id [out] Port number
	 * @return void
	 */
	void getPortNumberNO(uint16_t * id) {	// Network byte order
		uint16_t portNumberNO = PLAT_htons(portNumber);
		*id = portNumberNO;
	}

	/**
	 * @brief  Gets the port number in the host byte order, which can be either Big-Endian
	 * or Little-Endian, depending on the processor where it is running.
	 * @param  id Port number
	 * @return void
	 */
	void getPortNumber(uint16_t * id) {	// Host byte order
		*id = portNumber;
	}

	/**
	 * @brief  Sets the Port number
	 * @param  id [in] Port number
	 * @return void
	 */
	void setPortNumber(uint16_t * id) {
		portNumber = *id;
	}
};

/**
 * @brief Provides a map for the identityMap member of IEEE1588Port class
 */
typedef std::map < PortIdentity, LinkLayerAddress > IdentityMap_t;

/**
 * @brief Structure for initializing the IEEE1588 class
 */
typedef struct {
	/* clock IEEE1588Clock instance */
	IEEE1588Clock * clock;

	/* index Interface index */
	uint16_t index;

	/* forceSlave Forces port to be slave  */
	bool forceSlave;

	/* timestamper Hardware timestamper instance */
	HWTimestamper * timestamper;

	/* offset  Initial clock offset */
	int32_t offset;

	/* net_label Network label */
	InterfaceLabel * net_label;

	/* automotive_profile set the AVnu automotive profile */
	bool automotive_profile;

	/* Set to true if the port is the grandmaster. Used for fixed GM in the the AVnu automotive profile */
	bool isGM;

	/* Set to true if the port is the grandmaster. Used for fixed GM in the the AVnu automotive profile */
	bool testMode;

	/* gPTP 10.2.4.4 */
	char initialLogSyncInterval;

	/* gPTP 11.5.2.2 */
	char initialLogPdelayReqInterval;

	/* CDS 6.2.1.5 */
	char operLogPdelayReqInterval;

	/* CDS 6.2.1.6 */
	char operLogSyncInterval;

	/* condition_factory OSConditionFactory instance */
	OSConditionFactory * condition_factory;

	/* thread_factory OSThreadFactory instance */
	OSThreadFactory * thread_factory;

	/* timer_factory OSTimerFactory instance */
	OSTimerFactory * timer_factory;

	/* lock_factory OSLockFactory instance */
	OSLockFactory * lock_factory;
} IEEE1588PortInit_t;


/**
 * @brief Structure for IEE1588Port Counters
 */
typedef struct {
	int32_t ieee8021AsPortStatRxSyncCount;
	int32_t ieee8021AsPortStatRxFollowUpCount;
	int32_t ieee8021AsPortStatRxPdelayRequest;
	int32_t ieee8021AsPortStatRxPdelayResponse;
	int32_t ieee8021AsPortStatRxPdelayResponseFollowUp;
	int32_t ieee8021AsPortStatRxAnnounce;
	int32_t ieee8021AsPortStatRxPTPPacketDiscard;
	int32_t ieee8021AsPortStatRxSyncReceiptTimeouts;
	int32_t ieee8021AsPortStatAnnounceReceiptTimeouts;
	int32_t ieee8021AsPortStatPdelayAllowedLostResponsesExceeded;
	int32_t ieee8021AsPortStatTxSyncCount;
	int32_t ieee8021AsPortStatTxFollowUpCount;
	int32_t ieee8021AsPortStatTxPdelayRequest;
	int32_t ieee8021AsPortStatTxPdelayResponse;
	int32_t ieee8021AsPortStatTxPdelayResponseFollowUp;
	int32_t ieee8021AsPortStatTxAnnounce;
} IEEE1588PortCounters_t;


/**
 * @brief Provides the IEEE 1588 port interface
 */
class IEEE1588Port {
	static LinkLayerAddress other_multicast;
	static LinkLayerAddress pdelay_multicast;
	static LinkLayerAddress test_status_multicast;

	PortIdentity port_identity;
	/* directly connected node */
	PortIdentity peer_identity;

	OSNetworkInterface *net_iface;
	LinkLayerAddress local_addr;
	int link_delay[4];

	/* Port Status */
	unsigned sync_count;  // 0 for master, ++ for each sync receive as slave
	// set to 0 when asCapable is false, increment for each pdelay recvd
	unsigned pdelay_count;

	/* Port Configuration */
	unsigned char delay_mechanism;
	PortState port_state;
	char log_mean_unicast_sync_interval;
	char log_mean_sync_interval;
	char log_mean_announce_interval;
	char log_min_mean_delay_req_interval;
	char log_min_mean_pdelay_req_interval;
	bool burst_enabled;
	static const int64_t ONE_WAY_DELAY_DEFAULT = 3600000000000;
	static const int64_t INVALID_LINKDELAY = 3600000000000;
	static const int64_t NEIGHBOR_PROP_DELAY_THRESH = 800;
	static const unsigned int DEFAULT_SYNC_RECEIPT_THRESH = 5;
	static const unsigned int DUPLICATE_RESP_THRESH = 3;

	unsigned int duplicate_resp_counter;
	uint16_t last_invalid_seqid;

	/* Signed value allows this to be negative result because of inaccurate
	   timestamp */
	int64_t one_way_delay;
	int64_t neighbor_prop_delay_thresh;

	/*Sync threshold*/
	unsigned int sync_receipt_thresh;
	unsigned int wrongSeqIDCounter;

	/* Implementation Specific data/methods */
	IEEE1588Clock *clock;

	bool _syntonize;

	bool asCapable;

	/* Automotive Profile : Static variables */
	// port_state : already defined as port_state
	bool isGM;
	bool testMode;
	// asCapable : already defined as asCapable
	char initialLogPdelayReqInterval;
	char initialLogSyncInterval;
	char operLogPdelayReqInterval;
	char operLogSyncInterval;
	bool automotive_profile;

	// Test Status variables
	uint32_t linkUpCount;
	uint32_t linkDownCount;
	StationState_t stationState;


	/* Automotive Profile : Persistant variables */
	// neighborPropDelay : defined as one_way_delay ??
	// rateRatio : Optional and didn't fine this variable. Will proceed without writing it.
	// neighborRateRatio : defined as _peer_rate_offset ??

	// Automotive Profile AVB SYNC state indicator. > 0 will inditate valid AVB SYNC state
	uint32_t avbSyncState;

	int32_t *rate_offset_array;
	uint32_t rate_offset_array_size;
	uint32_t rate_offset_count;
	uint32_t rate_offset_index;

	FrequencyRatio _peer_rate_offset;
	Timestamp _peer_offset_ts_theirs;
	Timestamp _peer_offset_ts_mine;
	bool _peer_offset_init;

	int32_t _initial_clock_offset;
	int32_t _current_clock_offset;

	PTPMessageAnnounce *qualified_announce;

	uint16_t announce_sequence_id;
	uint16_t signal_sequence_id;
	uint16_t sync_sequence_id;

	uint16_t pdelay_sequence_id;
	PTPMessagePathDelayReq *last_pdelay_req;
	PTPMessagePathDelayResp *last_pdelay_resp;
	PTPMessagePathDelayRespFollowUp *last_pdelay_resp_fwup;

	/* Network socket description
	   physical interface number that object represents */
	uint16_t ifindex;

	IdentityMap_t identity_map;

	PTPMessageSync *last_sync;

	OSThread *listening_thread;

	OSThread *link_thread;

	OSCondition *port_ready_condition;

	OSLock *pdelay_rx_lock;
	OSLock *port_tx_lock;

	OSLock *syncReceiptTimerLock;
	OSLock *syncIntervalTimerLock;
	OSLock *announceIntervalTimerLock;
	OSLock *pDelayIntervalTimerLock;

	OSThreadFactory *thread_factory;
	OSTimerFactory *timer_factory;

	HWTimestamper *_hw_timestamper;

	net_result port_send
	(uint16_t etherType, uint8_t * buf, int size, MulticastType mcast_type,
	 PortIdentity * destIdentity, bool timestamp);

	InterfaceLabel *net_label;

	OSLockFactory *lock_factory;
	OSConditionFactory *condition_factory;

	bool pdelay_started;
	bool pdelay_halted;
	bool sync_rate_interval_timer_started;

	uint16_t lastGmTimeBaseIndicator;

	IEEE1588PortCounters_t counters;

 public:
	bool forceSlave;	//!< Forces port to be slave. Added for testing.

	/**
	 * @brief  Serializes (i.e. copy over buf pointer) the information from
	 * the variables (in that order):
	 *  - asCapable;
	 *  - Port Sate;
	 *  - Link Delay;
	 *  - Neighbor Rate Ratio
	 * @param  buf [out] Buffer where to put the results.
	 * @param  count [inout] Length of buffer. It contains maximum length to be written
	 * when the function is called, and the value is decremented by the same amount the
	 * buf size increases.
	 * @return TRUE if it has successfully written to buf all the values or if buf is NULL.
	 * FALSE if count should be updated with the right size.
	 */
	bool serializeState( void *buf, long *count );

	/**
	 * @brief  Restores the serialized state from the buffer. Copies the information from buffer
	 * to the variables (in that order):
	 *  - asCapable;
	 *  - Port State;
	 *  - Link Delay;
	 *  - Neighbor Rate Ratio
	 * @param  buf Buffer containing the serialized state.
	 * @param  count Buffer lenght. It is decremented by the same size of the variables that are
	 * being copied.
	 * @return TRUE if everything was copied successfully, FALSE otherwise.
	 */
	bool restoreSerializedState( void *buf, long *count );

	/**
	 * @brief  Switches port to a gPTP master
	 * @param  annc If TRUE, starts announce event timer.
	 * @return void
	 */
	void becomeMaster( bool annc );

	/**
	 * @brief  Switches port to a gPTP slave.
	 * @param  restart_syntonization if TRUE, restarts the syntonization
	 * @return void
	 */
	void becomeSlave( bool restart_syntonization );

	/**
	 * @brief  Starts pDelay event timer.
	 * @return void
	 */
	void startPDelay();

	/**
	 * @brief Stops PDelay event timer
	 * @return void
	 */
	void stopPDelay();

	/**
	 * @brief Enable/Disable PDelay Request messages
	 * @param hlt True to HALT (stop sending), False to resume (start sending).
	 */
	void haltPdelay(bool hlt)
	{
		pdelay_halted = hlt;
	}

	/**
	 * @brief Get the status of pdelayHalted condition.
	 * @return True PdelayRequest halted. False when PDelay Request is running
	 */
	bool pdelayHalted(void)
	{
		return pdelay_halted;
	}

	/**
	 * @brief  Starts Sync Rate Interval event timer. Used for the
	 *         Automotive Profile.
	 * @return void
	 */
	void startSyncRateIntervalTimer();

	/**
	 * @brief  Starts announce event timer
	 * @return void
	 */
	void startAnnounce();

	/**
	 * @brief  Starts pDelay event timer if not yet started.
	 * @return void
	 */
	void syncDone() {
		GPTP_LOG_VERBOSE("Sync complete");

		if (automotive_profile && port_state == PTP_SLAVE) {
			if (avbSyncState > 0) {
				avbSyncState--;
				if (avbSyncState == 0) {
					setStationState(STATION_STATE_AVB_SYNC);
					if (testMode) {
						APMessageTestStatus *testStatusMsg = new APMessageTestStatus(this);
						if (testStatusMsg) {
							testStatusMsg->sendPort(this);
							delete testStatusMsg;
						}
					}
				}
			}
		}

		if (automotive_profile) {
			if (!sync_rate_interval_timer_started) {
				if (log_mean_sync_interval != operLogSyncInterval) {
					startSyncRateIntervalTimer();
				}
			}
		}

		if( !pdelay_started ) {
			startPDelay();
		}
	}

	/**
	 * @brief  Gets a pointer to timer_factory object
	 * @return timer_factory pointer
	 */
	OSTimerFactory *getTimerFactory() {
		return timer_factory;
	}

	/**
	 * @brief  Restart PDelay
	 * @return void
	 */
	void restartPDelay() {
		_peer_offset_init = false;
	}

	/**
	 * @brief  Sets asCapable flag
	 * @param  ascap flag to be set. If FALSE, marks peer_offset_init as false.
	 * @return void
	 */
	void setAsCapable(bool ascap) {
		if (ascap != asCapable) {
			GPTP_LOG_STATUS("AsCapable: %s",
					ascap == true ? "Enabled" : "Disabled");
		}
		if(!ascap){
			_peer_offset_init = false;
		}
		asCapable = ascap;
	}

	/**
	 * @brief  Gets the asCapable flag
	 * @return asCapable flag.
	 */
	bool getAsCapable() { return( asCapable ); }

	/**
	 * @brief  Gets the AVnu automotive profile flag
	 * @return automotive_profile flag
	 */
	bool getAutomotiveProfile() { return( automotive_profile ); }

	/**
	 * @brief Destroys a IEEE1588Port
	 */
	~IEEE1588Port();

	/**
	 * @brief  Creates the IEEE1588Port interface. Will be
	 *         deprecated when the Windows platform suports
	 *         Automotive profile.
	 * @param  clock IEEE1588Clock instance
	 * @param  index Interface index
	 * @param  forceSlave Forces port to be slave
	 * @param  timestamper Hardware timestamper instance
	 * @param  offset  Initial clock offset
	 * @param  net_label Network label
	 * @param  condition_factory OSConditionFactory instance
	 * @param  thread_factory OSThreadFactory instance
	 * @param  timer_factory OSTimerFactory instance
	 * @param  lock_factory OSLockFactory instance
	 */
	IEEE1588Port
	(IEEE1588Clock * clock, uint16_t index,
	 bool forceSlave,
	 HWTimestamper * timestamper,
	 int32_t offset, InterfaceLabel * net_label,
	 OSConditionFactory * condition_factory,
	 OSThreadFactory * thread_factory,
	 OSTimerFactory * timer_factory,
	 OSLockFactory * lock_factory);

	/**
	 * @brief  Creates the IEEE1588Port interface.
	 * @param  init IEEE1588PortInit initialization parameters
	 */
	IEEE1588Port(IEEE1588PortInit_t *portInit);

	/**
	 * @brief  Initializes the port. Creates network interface, initializes
	 * hardware timestamper and create OS locks conditions
	 * @return FALSE if error during building the interface. TRUE if success
	 */
	bool init_port(int delay[4]);

	/**
	 * @brief  Currently doesnt do anything. Just returns.
	 * @return void
	 */
	void recoverPort(void);

	/**
	 * @brief Watch for link up and down events.
	 * @return Its an infinite loop. Returns NULL in case of error.
	 */
	void *watchNetLink(void);

	/**
	 * @brief Receives messages from the network interface
	 * @return Its an infinite loop. Returns NULL in case of error.
	 */
	void *openPort(IEEE1588Port *port);

	/**
	 * @brief Get the payload offset inside a packet
	 * @return 0
	 */
	unsigned getPayloadOffset();

	/**
	 * @brief  Sends and event to a IEEE1588 port. It includes timestamp
	 * @param  buf [in] Pointer to the data buffer
	 * @param  len Size of the message
	 * @param  mcast_type Enumeration MulticastType (pdlay, none or other). Depracated.
	 * @param  destIdentity Destination port identity
	 * @return void
	 */
	void sendEventPort
	(uint16_t etherType, uint8_t * buf, int len, MulticastType mcast_type,
	 PortIdentity * destIdentity);

	/**
	 * @brief Sends a general message to a port. No timestamps
	 * @param buf [in] Pointer to the data buffer
	 * @param len Size of the message
	 * @param mcast_type Enumeration MulticastType (pdelay, none or other). Depracated.
	 * @param destIdentity Destination port identity
	 * @return void
	 */
	void sendGeneralPort
	(uint16_t etherType, uint8_t * buf, int len, MulticastType mcast_type,
	 PortIdentity * destIdentity);

	/**
	 * @brief  Process all events for a IEEE1588Port
	 * @param  e Event to be processed
	 * @return void
	 */
	void processEvent(Event e);

	/**
	 * @brief  Gets the "best" announce
	 * @return Pointer to PTPMessageAnnounce
	 */
	PTPMessageAnnounce *calculateERBest(void);

	/**
	 * @brief  Adds a foreign master.
	 * @param  msg [in] PTP announce message
	 * @return void
	 * @todo Currently not implemented
	 */
	void addForeignMaster(PTPMessageAnnounce * msg);

	/**
	 * @brief  Remove a foreign master.
	 * @param  msg [in] PTP announce message
	 * @return void
	 * @todo Currently not implemented
	 */
	void removeForeignMaster(PTPMessageAnnounce * msg);

	/**
	 * @brief  Remove all foreign masters.
	 * @return void
	 * @todo Currently not implemented
	 */
	void removeForeignMasterAll(void);


	/**
	 * @brief  Adds a new qualified announce the port. IEEE 802.1AS Clause 10.3.10.2
	 * @param  msg PTP announce message
	 * @return void
	 */
	void addQualifiedAnnounce(PTPMessageAnnounce * msg) {
		if( qualified_announce != NULL ) delete qualified_announce;
		qualified_announce = msg;
	}
	/**
	 * @brief  Gets the local_addr
	 * @return LinkLayerAddress
	 */
	LinkLayerAddress *getLocalAddr(void) {
		return &local_addr;
	}

	/**
	 * @brief  Gets the sync interval value
	 * @return Sync Interval
	 */
	char getSyncInterval(void) {
		return log_mean_sync_interval;
	}

	/**
	 * @brief  Sets the sync interval value
	 * @param  val time interval
	 * @return none
	 */
	void setSyncInterval(char val) {
		log_mean_sync_interval = val;;
	}

	/**
	 * @brief  Sets the sync interval back to initial value
	 * @return none
	 */
	void setInitSyncInterval(void) {
		log_mean_sync_interval = initialLogSyncInterval;;
	}

	/**
	 * @brief  Start sync receipt timer
	 * @param  waitTime time interval
	 * @return none
	 */
	void startSyncReceiptTimer(long long unsigned int waitTime);

	/**
	 * @brief  Stop sync receipt timer
	 * @return none
	 */
	void stopSyncReceiptTimer(void);

	/**
	 * @brief  Start sync interval timer
	 * @param  waitTime time interval in nanoseconds
	 * @return none
	 */
	void startSyncIntervalTimer(long long unsigned int waitTime);

	/**
	 * @brief  Gets the announce interval
	 * @return Announce interval
	 */
	char getAnnounceInterval(void) {
		return log_mean_announce_interval;
	}

	/**
	 * @brief  Sets the announce interval
	 * @param  val time interval
	 * @return none
	 */
	void setAnnounceInterval(char val) {
		log_mean_announce_interval = val;
	}

	/**
	 * @brief  Start announce interval timer
	 * @param  waitTime time interval
	 * @return none
	 */
	void startAnnounceIntervalTimer(long long unsigned int waitTime);

	/**
	 * @brief  Gets the pDelay minimum interval
	 * @return PDelay interval
	 */
	char getPDelayInterval(void) {
		return log_min_mean_pdelay_req_interval;
	}

	/**
	 * @brief  Sets the pDelay minimum interval
	 * @param  val time interval
	 * @return none
	 */
	void setPDelayInterval(char val) {
		log_min_mean_pdelay_req_interval = val;
	}

	/**
	 * @brief  Sets the pDelay minimum interval back to initial
	 *         value
	 * @return none */
	void setInitPDelayInterval(void) {
		log_min_mean_pdelay_req_interval = initialLogPdelayReqInterval;
	}

	/**
	 * @brief  Start pDelay interval timer
	 * @param  waitTime time interval
	 * @return none
	 */
	void startPDelayIntervalTimer(long long unsigned int waitTime);

	/**
	 * @brief  Gets the portState information
	 * @return PortState
	 */
	PortState getPortState(void) {
		return port_state;
	}

	/**
	 * @brief Sets the PortState
	 * @param state value to be set
	 * @return void
	 */
	void setPortState( PortState state ) {
		port_state = state;
	}

	/**
	 * @brief  Gets port identity
	 * @param  identity [out] Reference to PortIdentity
	 * @return void
	 */
	void getPortIdentity(PortIdentity & identity) {
		identity = this->port_identity;
	}

	/**
	 * @brief  Gets the burst_enabled flag
	 * @return burst_enabled
	 */
	bool burstEnabled(void) {
		return burst_enabled;
	}

	/**
	 * @brief  Increments announce sequence id and returns
	 * @return Next announce sequence id.
	 */
	uint16_t getNextAnnounceSequenceId(void) {
		return announce_sequence_id++;
	}

	/**
	 * @brief  Increments signal sequence id and returns
	 * @return Next signal sequence id.
	 */
	uint16_t getNextSignalSequenceId(void) {
		return signal_sequence_id++;
	}

	/**
	 * @brief  Increments sync sequence ID and returns
	 * @return Next synce sequence id.
	 */
	uint16_t getNextSyncSequenceId(void) {
		return sync_sequence_id++;
	}

	/**
	 * @brief  Increments PDelay sequence ID and returns.
	 * @return Next PDelay sequence id.
	 */
	uint16_t getNextPDelaySequenceId(void) {
		return pdelay_sequence_id++;
	}

	/**
	 * @brief  Gets last sync sequence number from parent
	 * @return Parent last sync sequence number
	 * @todo Not currently implemented.
	 */
	uint16_t getParentLastSyncSequenceNumber(void);

	/**
	 * @brief  Sets last sync sequence number from parent
	 * @param  num Sequence number
	 * @return void
	 * @todo Currently not implemented.
	 */
	void setParentLastSyncSequenceNumber(uint16_t num);

	/**
	 * @brief  Gets a pointer to IEEE1588Clock
	 * @return Pointer to clock
	 */
	IEEE1588Clock *getClock(void);

	/**
	 * @brief  Sets last sync ptp message
	 * @param  msg [in] PTP sync message
	 * @return void
	 */
	void setLastSync(PTPMessageSync * msg) {
		last_sync = msg;
	}

	/**
	 * @brief  Gets last sync message
	 * @return PTPMessageSync last sync
	 */
	PTPMessageSync *getLastSync(void) {
		return last_sync;
	}

	/**
	 * @brief  Locks PDelay RX
	 * @return TRUE if acquired the lock. FALSE otherwise
	 */
	bool getPDelayRxLock() {
		return pdelay_rx_lock->lock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Do a trylock on the PDelay RX
	 * @return TRUE if acquired the lock. FALSE otherwise.
	 */
	bool tryPDelayRxLock() {
		return pdelay_rx_lock->trylock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Unlocks PDelay RX.
	 * @return TRUE if success. FALSE otherwise
	 */
	bool putPDelayRxLock() {
		return pdelay_rx_lock->unlock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Locks the TX port
	 * @return TRUE if success. FALSE otherwise.
	 */
	bool getTxLock() {
		return port_tx_lock->lock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Unlocks the port TX.
	 * @return TRUE if success. FALSE otherwise.
	 */
	bool putTxLock() {
		return port_tx_lock->unlock() == oslock_ok ? true : false;
	}

	/**
	 * @brief  Gets the hardware timestamper version
	 * @return HW timestamper version
	 */
	int getTimestampVersion() {
		return _hw_timestamper->getVersion();
	}

	/**
	 * @brief  Sets the last_pdelay_req message
	 * @param  msg [in] PTPMessagePathDelayReq message to set
	 * @return void
	 */
	void setLastPDelayReq(PTPMessagePathDelayReq * msg) {
		last_pdelay_req = msg;
	}

	/**
	 * @brief  Gets the last PTPMessagePathDelayReq message
	 * @return Last pdelay request
	 */
	PTPMessagePathDelayReq *getLastPDelayReq(void) {
		return last_pdelay_req;
	}

	/**
	 * @brief  Sets the last PTPMessagePathDelayResp message
	 * @param  msg [in] Last pdelay response
	 * @return void
	 */
	void setLastPDelayResp(PTPMessagePathDelayResp * msg) {
		last_pdelay_resp = msg;
	}

	/**
	 * @brief  Gets the last PTPMessagePathDelayResp message
	 * @return Last pdelay response
	 */
	PTPMessagePathDelayResp *getLastPDelayResp(void) {
		return last_pdelay_resp;
	}

	/**
	 * @brief  Sets the last PTPMessagePathDelayRespFollowUp message
	 * @param  msg [in] last pdelay response follow up
	 * @return void
	 */
	void setLastPDelayRespFollowUp(PTPMessagePathDelayRespFollowUp * msg) {
		last_pdelay_resp_fwup = msg;
	}

	/**
	 * @brief  Gets the last PTPMessagePathDelayRespFollowUp message
	 * @return last pdelay response follow up
	 */
	PTPMessagePathDelayRespFollowUp *getLastPDelayRespFollowUp(void) {
		return last_pdelay_resp_fwup;
	}

	/**
	 * @brief  Gets the Peer rate offset. Used to calculate neighbor rate ratio.
	 * @return FrequencyRatio peer rate offset
	 */
	FrequencyRatio getPeerRateOffset(void) {
		return _peer_rate_offset;
	}

	/**
	 * @brief  Sets the peer rate offset. Used to calculate neighbor rate ratio.
	 * @param  offset Offset to be set
	 * @return void
	 */
	void setPeerRateOffset( FrequencyRatio offset ) {
		_peer_rate_offset = offset;
	}

	/**
	 * @brief  Sets peer offset timestamps
	 * @param  mine Local timestamps
	 * @param  theirs Remote timestamps
	 * @return void
	 */
	void setPeerOffset(Timestamp mine, Timestamp theirs) {
		_peer_offset_ts_mine = mine;
		_peer_offset_ts_theirs = theirs;
		_peer_offset_init = true;
	}

	/**
	 * @brief  Gets peer offset timestamps
	 * @param  mine [out] Reference to local timestamps
	 * @param  theirs [out] Reference to remote timestamps
	 * @return TRUE if peer offset has already been initialized. FALSE otherwise.
	 */
	bool getPeerOffset(Timestamp & mine, Timestamp & theirs) {
		mine = _peer_offset_ts_mine;
		theirs = _peer_offset_ts_theirs;
		return _peer_offset_init;
	}

	/**
	 * @brief  Adjusts the clock frequency.
	 * @param  freq_offset Frequency offset
	 * @return TRUE if adjusted. FALSE otherwise.
	 */
	bool _adjustClockRate( FrequencyRatio freq_offset ) {
		if( _hw_timestamper ) {
			return _hw_timestamper->HWTimestamper_adjclockrate((float) freq_offset );
		}
		return false;
	}

	/**
	 * @brief  Adjusts the clock frequency.
	 * @param  freq_offset Frequency offset
	 * @return TRUE if adjusted. FALSE otherwise.
	 */
	bool adjustClockRate( FrequencyRatio freq_offset ) {
		return _adjustClockRate( freq_offset );
	}

	/**
	 * @brief  Gets extended error message from hardware timestamper
	 * @param  msg [out] Extended error message
	 * @return void
	 */
	void getExtendedError(char *msg) {
		if (_hw_timestamper) {
			_hw_timestamper->HWTimestamper_get_extderror(msg);
		} else {
			*msg = '\0';
		}
	}

	/**
	 * @brief Initializes the hwtimestamper
	 */
	void timestamper_init(void);

	/**
	 * @brief Resets the hwtimestamper
	 */
	void timestamper_reset(void);


	/**
	 * @brief  Gets RX timestamp based on port identity
	 * @param  sourcePortIdentity [in] Source port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [out] RX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the rx lock.
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getRxTimestamp
	(PortIdentity * sourcePortIdentity, PTPMessageId messageId,
	 Timestamp & timestamp, unsigned &counter_value, bool last);

	/**
	 * @brief  Gets TX timestamp based on port identity
	 * @param  sourcePortIdentity [in] Source port identity
	 * @param  sequenceId Sequence ID
	 * @param  timestamp [out] TX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the TX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getTxTimestamp
	(PortIdentity * sourcePortIdentity, PTPMessageId messageId,
	 Timestamp & timestamp, unsigned &counter_value, bool last);

	/**
	 * @brief  Gets TX timestamp based on PTP message
	 * @param  msg PTPMessageCommon message
	 * @param  timestamp [out] TX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the TX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getTxTimestamp
	(PTPMessageCommon * msg, Timestamp & timestamp, unsigned &counter_value,
	 bool last);

	/**
	 * @brief  Gets RX timestamp based on PTP message
	 * @param  msg PTPMessageCommon message
	 * @param  timestamp [out] RX timestamp
	 * @param  counter_value [out] timestamp count value
	 * @param  last If true, removes the RX lock
	 * @return GPTP_EC_SUCCESS if no error, GPTP_EC_FAILURE if error and GPTP_EC_EAGAIN to try again.
	 */
	int getRxTimestamp
	(PTPMessageCommon * msg, Timestamp & timestamp, unsigned &counter_value,
	 bool last);

	/**
	 * @brief  Get the cross timestamping information.
	 * The gPTP subsystem uses these samples to calculate
	 * ratios which can be used to translate or extrapolate
	 * one clock into another clock reference. The gPTP service
	 * uses these supplied cross timestamps to perform internal
	 * rate estimation and conversion functions.
	 * @param  system_time [out] System time
	 * @param  device_time [out] Device time
	 * @param  local_clock [out] Local clock
	 * @param  nominal_clock_rate [out] Nominal clock rate
	 * @return True in case of success. FALSE in case of error
	 */
	void getDeviceTime
	(Timestamp & system_time, Timestamp & device_time, uint32_t & local_clock,
	 uint32_t & nominal_clock_rate);

	/**
	 * @brief  Gets the link delay information.
	 * @return one way delay if delay > 0, or zero otherwise.
	 */
	uint64_t getLinkDelay(void) {
		return one_way_delay > 0LL ? one_way_delay : 0LL;
	}

	/**
	 * @brief  Gets the link delay information.
	 * @param  [in] delay Pointer to the delay information
	 * @return True if valid, false if invalid
	 */
	bool getLinkDelay(uint64_t *delay) {
		if(delay == NULL) {
			return false;
		}
		*delay = getLinkDelay();
		return *delay < INVALID_LINKDELAY;
	}

	/**
	 * @brief  Sets link delay information.
	 * Signed value allows this to be negative result because
	 * of inaccurate timestamps.
	 * @param  delay Link delay
	 * @return True if one_way_delay is lower or equal than neighbor propagation delay threshold
	 *         False otherwise
	 */
	bool setLinkDelay(int64_t delay) {
		one_way_delay = delay;
		int64_t abs_delay = (one_way_delay < 0 ? -one_way_delay : one_way_delay);

		if (testMode) {
			GPTP_LOG_STATUS("Link delay: %d", delay);
		}

		return (abs_delay <= neighbor_prop_delay_thresh);
	}

	/**
	 * @brief  Sets the internal variabl sync_receipt_thresh, which is the
	 * flag that monitors the amount of wrong syncs enabled before switching
	 * the ptp to master.
	 * @param  th Threshold to be set
	 * @return void
	 */
	void setSyncReceiptThresh(unsigned int th)
	{
		sync_receipt_thresh = th;
	}

	/**
	 * @brief  Gets the internal variabl sync_receipt_thresh, which is the
	 * flag that monitors the amount of wrong syncs enabled before switching
	 * the ptp to master.
	 * @return sync_receipt_thresh value
	 */
	unsigned int getSyncReceiptThresh(void)
	{
		return sync_receipt_thresh;
	}

	/**
	 * @brief  Sets the wrongSeqIDCounter variable
	 * @param  cnt Value to be set
	 * @return void
	 */
	void setWrongSeqIDCounter(unsigned int cnt)
	{
		wrongSeqIDCounter = cnt;
	}

	/**
	 * @brief  Gets the wrongSeqIDCounter value
	 * @param  [out] cnt Pointer to the counter value. It must be valid
	 * @return TRUE if ok and lower than the syncReceiptThreshold value. FALSE otherwise
	 */
	bool getWrongSeqIDCounter(unsigned int *cnt)
	{
		if( cnt == NULL )
		{
			return false;
		}
		*cnt = wrongSeqIDCounter;

		return( *cnt < getSyncReceiptThresh() );
	}

	/**
	 * @brief  Increments the wrongSeqIDCounter value
	 * @param  [out] cnt Pointer to the counter value. Must be valid
	 * @return TRUE if incremented value is lower than the syncReceiptThreshold. FALSE otherwise.
	 */
	bool incWrongSeqIDCounter(unsigned int *cnt)
	{
		if( getAsCapable() )
		{
			wrongSeqIDCounter++;
		}
		bool ret = wrongSeqIDCounter < getSyncReceiptThresh();

		if( cnt != NULL)
		{
			*cnt = wrongSeqIDCounter;
		}

		return ret;
	}

	/**
	 * @brief Sets the value of last duplicated SeqID
	 * @param seqid Value to set
	 * @return void
	 */
	void setLastInvalidSeqID(uint16_t seqid)
	{
		last_invalid_seqid = seqid;
	}

	/**
	 * @brief  Get the value of last invalid seqID
	 * @return Last invalid seq id
	 */
	uint16_t getLastInvalidSeqID(void)
	{
		return last_invalid_seqid;
	}

	/**
	 * @brief  Sets the duplicate pdelay_resp counter.
	 * @param  cnt Value to be set
	 */
	void setDuplicateRespCounter(unsigned int cnt)
	{
		duplicate_resp_counter = cnt;
	}

	/**
	 * @brief  Gets the current value of pdelay_resp duplicate messages counter
	 * @return Counter value
	 */
	unsigned int getDuplicateRespCounter(void)
	{
		return duplicate_resp_counter;
	}

	/**
	 * @brief  Increment the duplicate PDelayResp message counter
	 * @return True if it equals the threshold, False otherwise
	 */
	bool incrementDuplicateRespCounter(void)
	{
		return ++duplicate_resp_counter == DUPLICATE_RESP_THRESH;
	}

	/**
	 * @brief  Sets the neighbor propagation delay threshold
	 * @param  delay Delay in nanoseconds
	 * @return void
	 */
	void setNeighPropDelayThresh(int64_t delay) {
		neighbor_prop_delay_thresh = delay;
	}

	/**
	 * @brief  Changes the port state
	 * @param  state Current state
	 * @param  changed_external_master TRUE if external master has changed, FALSE otherwise
	 * @return void
	 */
	void recommendState(PortState state, bool changed_external_master);

	/**
	 * @brief  Maps socket addr to the remote link layer address
	 * @param  destIdentity [in] PortIdentity remote
	 * @param  remote [in] remote link layer address
	 * @return void
	 */
	void mapSocketAddr
	(PortIdentity * destIdentity, LinkLayerAddress * remote);

	/**
	 * @brief  Adds New sock addr map
	 * @param  destIdentity [in] PortIdentity remote
	 * @param  remote [in] remote link layer address
	 * @return void
	 */
	void addSockAddrMap
	(PortIdentity * destIdentity, LinkLayerAddress * remote);

	/**
	 * @brief  Sets current pdelay count value.
	 * @param  cnt [in] pdelay count value
	 * @return void
	 */
	void setPdelayCount(unsigned int cnt) {
		pdelay_count = cnt;
	}

	/**
	 * @brief  Increments Pdelay count
	 * @return void
	 */
	void incPdelayCount() {
		++pdelay_count;
	}

	/**
	 * @brief  Gets current pdelay count value. It is set to zero
	 * when asCapable is false.
	 * @return pdelay count
	 */
	unsigned getPdelayCount() {
		return pdelay_count;
	}

	/**
	 * @brief  Sets current sync count value.
	 * @param  cnt [in] sync count value
	 * @return void
	 */
	void setSyncCount(unsigned int cnt) {
		sync_count = cnt;
	}

	/**
	 * @brief  Increments sync count
	 * @return void
	 */
	void incSyncCount() {
		++sync_count;
	}

	/**
	 * @brief  Gets current sync count value. It is set to zero
	 * when master and incremented at each sync received for slave.
	 * @return sync count
	 */
	unsigned getSyncCount() {
		return sync_count;
	}

	/**
	 * @brief  Gets link up count
	 * @return Link up  count
	 */
	uint32_t getLinkUpCount() {
		return linkUpCount;
	}

	/**
	 * @brief  Gets link down count
	 * @return Link down count
	 */
	uint32_t getLinkDownCount() {
		return linkDownCount;
	}

	/**
	 * @brief  Sets the Station State for the Test Status message 
	 * @param  StationState_t [in] The station state  
	 * @return none
	 */
	void setStationState(StationState_t _stationState) {
		stationState = _stationState;
		if (stationState == STATION_STATE_ETHERNET_READY) {
			GPTP_LOG_STATUS("AVnu AP Status : STATION_STATE_ETHERNET_READY");
		}
		else if (stationState == STATION_STATE_AVB_SYNC) {
			GPTP_LOG_STATUS("AVnu AP Status : STATION_STATE_AVB_SYNC");
		}
		else if (stationState == STATION_STATE_AVB_MEDIA_READY) {
			GPTP_LOG_STATUS("AVnu AP Status : STATION_STATE_AVB_MEDIA_READY");
		}
	}

	/**
	 * @brief  Gets the Station State for the Test Status
	 *         message
	 * @return station state
	 */
	StationState_t getStationState() {
		return stationState;
	}


	/**
	 * @brief  Gets the lastGmTimeBaseIndicator
	 * @return uint16 of the lastGmTimeBaseIndicator
	 */
	uint16_t getLastGmTimeBaseIndicator(void) {
		return lastGmTimeBaseIndicator;
	}

	/**
	 * @brief  Sets the lastGmTimeBaseIndicator
	 * @param  gmTimeBaseIndicator from last Follow up message
	 * @return void
	 */
	void setLastGmTimeBaseIndicator(uint16_t gmTimeBaseIndicator) {
		lastGmTimeBaseIndicator = gmTimeBaseIndicator;
	}

	/**
	 * @brief  Gets the testMode
	 * @return bool of the test mode value
	 */
	bool getTestMode(void) {
		return testMode;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxSyncCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxSyncCount(void) {
		counters.ieee8021AsPortStatRxSyncCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxFollowUpCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxFollowUpCount(void) {
		counters.ieee8021AsPortStatRxFollowUpCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayRequest
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayRequest(void) {
		counters.ieee8021AsPortStatRxPdelayRequest++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayResponse
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayResponse(void) {
		counters.ieee8021AsPortStatRxPdelayResponse++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPdelayResponseFollowUp
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPdelayResponseFollowUp(void) {
		counters.ieee8021AsPortStatRxPdelayResponseFollowUp++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxAnnounce
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxAnnounce(void) {
		counters.ieee8021AsPortStatRxAnnounce++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxPTPPacketDiscard
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxPTPPacketDiscard(void) {
		counters.ieee8021AsPortStatRxPTPPacketDiscard++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatRxSyncReceiptTimeouts
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatRxSyncReceiptTimeouts(void) {
		counters.ieee8021AsPortStatRxSyncReceiptTimeouts++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatAnnounceReceiptTimeouts
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatAnnounceReceiptTimeouts(void) {
		counters.ieee8021AsPortStatAnnounceReceiptTimeouts++;
	}


	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatPdelayAllowedLostResponsesExceeded
	 * @return void
	 */
	// TODO: Not called
	void incCounter_ieee8021AsPortStatPdelayAllowedLostResponsesExceeded(void) {
		counters.ieee8021AsPortStatPdelayAllowedLostResponsesExceeded++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxSyncCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxSyncCount(void) {
		counters.ieee8021AsPortStatTxSyncCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxFollowUpCount
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxFollowUpCount(void) {
		counters.ieee8021AsPortStatTxFollowUpCount++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayRequest
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayRequest(void) {
		counters.ieee8021AsPortStatTxPdelayRequest++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayResponse
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayResponse(void) {
		counters.ieee8021AsPortStatTxPdelayResponse++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxPdelayResponseFollowUp
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxPdelayResponseFollowUp(void) {
		counters.ieee8021AsPortStatTxPdelayResponseFollowUp++;
	}

	/**
	 * @brief  Increment IEEE Port counter:
	 *         ieee8021AsPortStatTxAnnounce
	 * @return void
	 */
	void incCounter_ieee8021AsPortStatTxAnnounce(void) {
		counters.ieee8021AsPortStatTxAnnounce++;
	}

	/**
	 * @brief  Logs port counters
	 * @return void
	 */
	void logIEEEPortCounters(void) {
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxSyncCount : %u", counters.ieee8021AsPortStatRxSyncCount);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxFollowUpCount : %u", counters.ieee8021AsPortStatRxFollowUpCount);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxPdelayRequest : %u", counters.ieee8021AsPortStatRxPdelayRequest);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxPdelayResponse : %u", counters.ieee8021AsPortStatRxPdelayResponse);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxPdelayResponseFollowUp : %u", counters.ieee8021AsPortStatRxPdelayResponseFollowUp);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxAnnounce : %u", counters.ieee8021AsPortStatRxAnnounce);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxPTPPacketDiscard : %u", counters.ieee8021AsPortStatRxPTPPacketDiscard);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatRxSyncReceiptTimeouts : %u", counters.ieee8021AsPortStatRxSyncReceiptTimeouts);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatAnnounceReceiptTimeouts : %u", counters.ieee8021AsPortStatAnnounceReceiptTimeouts);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatPdelayAllowedLostResponsesExceeded : %u", counters.ieee8021AsPortStatPdelayAllowedLostResponsesExceeded);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatTxSyncCount : %u", counters.ieee8021AsPortStatTxSyncCount);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatTxFollowUpCount : %u", counters.ieee8021AsPortStatTxFollowUpCount);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatTxPdelayRequest : %u", counters.ieee8021AsPortStatTxPdelayRequest);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatTxPdelayResponse : %u", counters.ieee8021AsPortStatTxPdelayResponse);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatTxPdelayResponseFollowUp : %u", counters.ieee8021AsPortStatTxPdelayResponseFollowUp);
		GPTP_LOG_STATUS("IEEE Port Counter ieee8021AsPortStatTxAnnounce : %u", counters.ieee8021AsPortStatTxAnnounce);
	}
};

#endif
