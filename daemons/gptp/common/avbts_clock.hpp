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

#ifndef AVBTS_CLOCK_HPP
#define AVBTS_CLOCK_HPP

#include <stdint.h>
#include <ieee1588.hpp>
#include <avbts_port.hpp>
#include <avbts_ostimerq.hpp>
#include <avbts_osipc.hpp>

/**@file*/

#define EVENT_TIMER_GRANULARITY 5000000		/*!< Event timer granularity*/

#define INTEGRAL 0.0024				/*!< PI controller integral factor*/
#define PROPORTIONAL 1.0			/*!< PI controller proportional factor*/
#define UPPER_FREQ_LIMIT  250.0		/*!< Upper frequency limit */
#define LOWER_FREQ_LIMIT -250.0		/*!< Lower frequency limit */

#define UPPER_LIMIT_PPM 250
#define LOWER_LIMIT_PPM -250
#define PPM_OFFSET_TO_RATIO(ppm) ((ppm) / ((FrequencyRatio)US_PER_SEC) + 1)

/* This is the threshold in ns for which frequency adjustments will be made */
#define PHASE_ERROR_THRESHOLD (1000000000)

/* This is the maximum count of phase error, outside of the threshold before
   adjustment is performed */
#define PHASE_ERROR_MAX_COUNT (6)


/**
 * @brief Provides the clock quality abstraction.
 * Represents the quality of the clock
 * Defined at IEEE 802.1AS-2011
 * Clause 6.3.3.8
 */
struct ClockQuality {
	unsigned char cq_class;				/*!< Clock Class - Clause 8.6.2.2
										  Denotes the tracebility of the synchronized time
										  distributed by a clock master when it is grandmaster. */
	unsigned char clockAccuracy; 		/*!< Clock Accuracy - clause 8.6.2.3.
										  Indicates the expected time accuracy of
										  a clock master.*/
	int16_t offsetScaledLogVariance;	/*!< ::Offset Scaled log variance - Clause 8.6.2.4.
										  Is the scaled, offset representation
										  of an estimate of the PTP variance. The
										  PTP variance characterizes the
										  precision and frequency stability of the clock
										  master. The PTP variance is the square of
										  PTPDEV (See B.1.3.2). */
};

/**
 * @brief Provides the 1588 clock interface
 */
class IEEE1588Clock {
private:
	ClockIdentity clock_identity;
	ClockQuality clock_quality;
	unsigned char priority1;
	unsigned char priority2;
	bool initializable;
	bool is_boundary_clock;
	bool two_step_clock;
	unsigned char domain_number;
	uint16_t number_ports;
	uint16_t number_foreign_records;
	bool slave_only;
	int16_t current_utc_offset;
	bool leap_59;
	bool leap_61;
	uint16_t epoch_number;
	uint16_t steps_removed;
	int64_t offset_from_master;
	Timestamp one_way_delay;
	PortIdentity parent_identity;
	ClockIdentity grandmaster_clock_identity;
	ClockQuality grandmaster_clock_quality;
	unsigned char grandmaster_priority1;
	unsigned char grandmaster_priority2;
	bool grandmaster_is_boundary_clock;
	uint8_t time_source;

	ClockIdentity LastEBestIdentity;
	bool _syntonize;
	bool _new_syntonization_set_point;
	float _ppm;
	int _phase_error_violation;

	IEEE1588Port *port_list[MAX_PORTS];

	static Timestamp start_time;
	Timestamp last_sync_time;

	bool _master_local_freq_offset_init;
	Timestamp _prev_master_time;
	Timestamp _prev_sync_time;

	bool _local_system_freq_offset_init;
	Timestamp _prev_local_time;
	Timestamp _prev_system_time;

	HWTimestamper *_timestamper;

	OS_IPC *ipc;

	OSTimerQueue *timerq;

	bool forceOrdinarySlave;
	FrequencyRatio _master_local_freq_offset;
	FrequencyRatio _local_system_freq_offset;

    /**
     * @brief fup info stores information of the last time
     * the base time has changed. When that happens
     * the information from fup_status is copied over
     * fup_info. The follow-up sendPort method is
     * supposed to get the information from fup_info
     * prior to sending a message out.
     */
    FollowUpTLV *fup_info;

    /**
     * @brief fup status has the instantaneous info
     */
    FollowUpTLV *fup_status;

    OSLock *timerq_lock;
public:
  /**
   * @brief Instantiates a IEEE 1588 Clock
   * @param forceOrdinarySlave Forces it to be an ordinary slave
   * @param syntonize if TRUE, clock will syntonize to the master clock
   * @param priority1 It is used in the execution of BCMA. See IEEE 802.1AS Clause 10.3
   * @param timestamper [in] Provides an object for hardware timestamp
   * @param timerq_factory [in] Provides a factory object for creating timer queues (managing events)
   * @param ipc [in] Inter process communication object
   * @param lock_factory [in] Provides a factory object for creating locking a locking mechanism
   */
  IEEE1588Clock
	  (bool forceOrdinarySlave, bool syntonize, uint8_t priority1,
	   HWTimestamper *timestamper, OSTimerQueueFactory * timerq_factory,
	   OS_IPC * ipc, OSLockFactory *lock_factory );

  /*
   * Destroys the IEEE 1588 clock entity
   */
  ~IEEE1588Clock(void);

  /**
   * @brief  Updates the frequencyRatio information
   * @param  buf [out] Stores the serialized clock quality state
   * @param  count [inout] Provides the size of buffer. Its decremented internally
   * @return TRUE in case of success, FALSE when the count should be updated with the right size.
   */
  bool serializeState( void *buf, long *count );

  /**
   * @brief  Restores the frequencyRatio with the serialized input buffer data
   * @param  buf [in] serialized frequencyRatio information
   * @param  count [inout] Size of buffer. It is incremented internally
   * @return TRUE in case of success, FALSE otherwise.
   */
  bool restoreSerializedState( void *buf, long *count );

  /**
   * @brief  Gets the current time from system clock
   * @return System time
   */
  Timestamp getTime(void);

  /**
   * @brief  Gets the timestamp from hardware (Deprecated)
   * @return Hardware timestamp
   */
  Timestamp getPreciseTime(void);

  /**
   * @brief  Compares the 1588 Clock to the grandmaster clock
   * @param  msg [in] PTP announce message
   * @return TRUE if the 1588 clock
   */
  bool isBetterThan(PTPMessageAnnounce * msg);

  /**
   * @brief  Gets the Last Best clock identity
   * @return clock identity
   */
  ClockIdentity getLastEBestIdentity( void ) {
	  return LastEBestIdentity;
  }

  /**
   * @brief  Sets the last Best clock identity
   * @param  id ClockIdentity object to be set
   * @return void
   */
  void setLastEBestIdentity( ClockIdentity id ) {
	  LastEBestIdentity = id;
	  return;
  }

  /**
   * @brief  Sets clock identity by id
   * @param  id [id] Clock identity (as an octet array)
   * @return void
   */
  void setClockIdentity(char *id) {
	  clock_identity.set((uint8_t *) id);
  }

  /**
   * @brief  Set clock id based on the link layer address. Clock id is 8 octets
   * long whereas link layer address is 6 octets long and it is turned into a
   * clock identity as per the 802.1AS standard described in clause 8.5.2.2
   * @param  addr Link layer address
   * @return void
   */
  void setClockIdentity(LinkLayerAddress * addr) {
	  clock_identity.set(addr);
  }

  /**
   * @brief  Gets the domain number
   * @return domain number
   */
  unsigned char getDomain(void) {
	  return domain_number;
  }

  /**
   * @brief  Gets grandmaster clock ID
   * @return GM clock ID
   */
  ClockIdentity getGrandmasterClockIdentity(void) {
	  return grandmaster_clock_identity;
  }

  /**
   * @brief  Sets a new GM clock ID
   * @param  id New id
   * @return void
   */
  void setGrandmasterClockIdentity(ClockIdentity id) {
	  if (id != grandmaster_clock_identity) {
		  GPTP_LOG_STATUS("New Grandmaster \"%s\" (previous \"%s\")", id.getIdentityString().c_str(), grandmaster_clock_identity.getIdentityString().c_str());
		  grandmaster_clock_identity = id;
	  }
  }

  /**
   * @brief  Gets grandmaster clock quality object
   * @return Clock quality
   */
  ClockQuality getGrandmasterClockQuality(void) {
	  return grandmaster_clock_quality;
  }

  /**
   * @brief  Sets grandmaster clock quality
   * @param  clock_quality ClockQuality object to be set
   * @return void
   */
  void setGrandmasterClockQuality( ClockQuality clock_quality ) {
	  grandmaster_clock_quality = clock_quality;
  }

  /**
   * @brief  Gets the IEEE 1588 Clock quality
   * @return ClockQuality
   */
  ClockQuality getClockQuality(void) {
	  return clock_quality;
  }

  /**
   * @brief  Gets grandmaster priority1 attribute (IEEE 802.1AS clause 10.5.3.2.2)
   * @return Grandmaster priority1
   */
  unsigned char getGrandmasterPriority1(void) {
	  return grandmaster_priority1;
  }

  /**
   * @brief  Gets grandmaster priotity2 attribute (IEEE 802.1AS clause 10.5.3.2.4)
   * @return Grandmaster priority2
   */
  unsigned char getGrandmasterPriority2(void) {
	  return grandmaster_priority2;
  }

  /**
   * @brief  Sets grandmaster's priority1 attribute (IEEE 802.1AS clause 10.5.3.2.2)
   * @param  priority1 value to be set
   * @return void
   */
  void setGrandmasterPriority1( unsigned char priority1 ) {
	  grandmaster_priority1 = priority1;
  }

  /**
   * @brief  Sets grandmaster's priority2 attribute (IEEE 802.1AS clause 10.5.3.2.4)
   * @param  priority2 Value to be set
   * @return void
   */
  void setGrandmasterPriority2( unsigned char priority2 ) {
	  grandmaster_priority2 = priority2;
  }

  /**
   * @brief  Gets master steps removed (IEEE 802.1AS clause 10.3.3)
   * @return steps removed value
   */
  uint16_t getMasterStepsRemoved(void) {
	  return steps_removed;
  }

  /**
   * @brief  Gets the currentUtcOffset attribute (IEEE 802.1AS clause 10.3.8.9)
   * @return currentUtcOffset
   */
  uint16_t getCurrentUtcOffset(void) {
	  return current_utc_offset;
  }

  /**
   * @brief  Gets the TimeSource attribute (IEEE 802.1AS-2011 clause 10.3.8.10)
   * @return TimeSource
   */
  uint8_t getTimeSource(void) {
	  return time_source;
  }

  /**
   * @brief  Gets IEEE1588Clock priority1 value (IEEE 802.1AS clause 8.6.2.1)
   * @return Priority1 value
   */
  unsigned char getPriority1(void) {
	  return priority1;
  }

  /**
   * @brief  Gets IEEE1588Clock priority2 attribute (IEEE 802.1AS clause 8.6.2.5)
   * @return Priority2 value
   */
  unsigned char getPriority2(void) {
	  return priority2;
  }

  /**
   * @brief  Gets nextPortId value
   * @return The remaining value from the division of current number of ports by
   * (maximum number of ports + 1) + 1
   */
  uint16_t getNextPortId(void) {
	  return (number_ports++ % (MAX_PORTS + 1)) + 1;
  }

  /**
   * @brief  Sets the follow up info internal object. The fup_info object contains
   * the last updated information when the frequency or phase have changed
   * @param  fup Pointer to the FolloUpTLV object.
   * @return void
   */
  void setFUPInfo(FollowUpTLV *fup)
  {
      fup_info = fup;
  }

  /**
   * @brief  Gets the fup_info pointer
   * @return fup_info pointer
   */
  FollowUpTLV *getFUPInfo(void)
  {
      return fup_info;
  }

  /**
   * @brief  Sets the follow up status internal object. The fup_status object contains
   * information about the current frequency/phase aqcuired through the received
   * follow up messages
   * @param  fup Pointer to the FollowUpTLV object
   * @return void
   */
  void setFUPStatus(FollowUpTLV *fup)
  {
      fup_status = fup;
  }

  /**
   * @brief  Gets the fup_status pointer
   * @return fup_status pointer
   */
  FollowUpTLV *getFUPStatus(void)
  {
      return fup_status;
  }

  /**
   * @brief Updates the follow up info internal object with the current clock source time
   * status values. This method should be called whenever the clockSource entity time
   * base changes (IEEE 802.1AS clause 9.2)
   * @return void
   */
  void updateFUPInfo(void)
  {
      fup_info->incrementGMTimeBaseIndicator();
      fup_info->setScaledLastGmFreqChange(fup_status->getScaledLastGmFreqChange());
      fup_info->setScaledLastGmPhaseChange(fup_status->getScaledLastGmPhaseChange());
  }

  /**
   * @brief  Registers a new IEEE1588 port
   * @param  port  [in] IEEE1588port instance
   * @param  index Port's index
   * @return void
   */
  void registerPort(IEEE1588Port * port, uint16_t index) {
	  if (index < MAX_PORTS) {
		  port_list[index - 1] = port;
	  }
	  ++number_ports;
  }

  /**
   * @brief  Gets the current port list instance
   * @param  count [out] Number of ports
   * @param  ports [out] Pointer to the port list
   * @return
   */
  void getPortList(int &count, IEEE1588Port ** &ports) {
	  ports = this->port_list;
	  count = number_ports;
	  return;
  }

  /**
   * @brief  Gets current system time
   * @return Instance of a Timestamp object
   */
  static Timestamp getSystemTime(void);

  /**
   * @brief  Add a new event to the timer queue
   * @param  target IEEE1588Port target
   * @param  e Event to be added
   * @param  time_ns Time in nanoseconds
   */
  void addEventTimer
	  ( IEEE1588Port * target, Event e, unsigned long long time_ns );

  /**
   * @brief  Deletes an event from the timer queue
   * @param  target Target port to remove the event from
   * @param  e Event to be removed
   * @return void
   */
  void deleteEventTimer(IEEE1588Port * target, Event e);

  /**
   * @brief  Adds an event to the timer queue using a lock
   * @param  target IEEE1588Port target
   * @param  e Event to be added
   * @param  time_ns current time in nanoseconds
   * @return void
   */
  void addEventTimerLocked
	  ( IEEE1588Port * target, Event e, unsigned long long time_ns );

  /**
   * @brief  Deletes and event from the timer queue using a lock
   * @param  target Target port to remove the event from
   * @param  e Event to be deleted
   * @return
   */
  void deleteEventTimerLocked(IEEE1588Port * target, Event e);

  /**
   * @brief  Calculates the master to local clock rate difference
   * @param  master_time Master time
   * @param  sync_time Local time
   * @return The offset in ppt (parts per trillion)
   */
  FrequencyRatio calcMasterLocalClockRateDifference
	  ( Timestamp master_time, Timestamp sync_time );

  /**
   * @brief  Calculates the local to system clock rate difference
   * @param  local_time Local time
   * @param  system_time System time
   * @return The offset in ppt (parts per trillion)
   */
  FrequencyRatio calcLocalSystemClockRateDifference
	  ( Timestamp local_time, Timestamp system_time );

  /**
   * @brief  Sets the master offset, sintonyze and adjusts the frequency offset
   * @param  master_local_offset Master to local phase offset
   * @param  local_time Local time
   * @param  master_local_freq_offset Master to local frequency offset
   * @param  local_system_offset Local time to system time phase offset
   * @param  system_time System time
   * @param  local_system_freq_offset Local to system frequency offset
   * @param  sync_count Sync messages count
   * @param  pdelay_count PDelay messages count
   * @param  port_state PortState instance
   * @param  asCapable asCapable flag
   */
  void setMasterOffset
	  ( IEEE1588Port * port, int64_t master_local_offset, Timestamp local_time,
		FrequencyRatio master_local_freq_offset,
		int64_t local_system_offset,
		Timestamp system_time,
		FrequencyRatio local_system_freq_offset,
		unsigned sync_count,
        unsigned pdelay_count,
        PortState port_state,
        bool asCapable );

  /**
   * @brief  Get the IEEE1588Clock identity value
   * @return clock identity
   */
  ClockIdentity getClockIdentity() {
	  return clock_identity;
  }

  /**
   * @brief  Sets a flag that will allow syntonization during setMasterOffset calls
   * @return void
   */
  void newSyntonizationSetPoint() {
	  _new_syntonization_set_point = true;
  }

  /**
   * @brief  Restart PDelays on all ports
   * @return void
   */
  void restartPDelayAll() {
	  int number_ports, i, j = 0;
	  IEEE1588Port **ports;

	  getPortList( number_ports, ports );

	  for( i = 0; i < number_ports; ++i ) {
		  while( ports[j] == NULL ) ++j;
		  ports[j]->restartPDelay();
	  }
  }

  /**
   * @brief  Gets all TX locks
   * @return void
   */
  int getTxLockAll() {
	  int number_ports, i, j = 0;
	  IEEE1588Port **ports;

	  getPortList( number_ports, ports );

	  for( i = 0; i < number_ports; ++i ) {
		  while( ports[j] == NULL ) ++j;
		  if( ports[j]->getTxLock() == false ) {
			  return false;
		  }
	  }

	  return true;
  }

  /**
   * @brief  Release all TX locks
   * @return void
   */
  int putTxLockAll() {
	  int number_ports, i, j = 0;
	  IEEE1588Port **ports;

	  getPortList( number_ports, ports );

	  for( i = 0; i < number_ports; ++i ) {
		  while( ports[j] == NULL ) ++j;
		  if( ports[j]->putTxLock() == false ) {
			  return false;
		  }
	  }

	  return true;
  }


  /**
   * @brief  Declares a friend instance of tick_handler method
   * @param  sig Signal
   * @return void
   */
  friend void tick_handler(int sig);

  /**
   * @brief  Gets the timer queue lock
   * @return OSLockResult structure
   */
  OSLockResult getTimerQLock() {
	  return timerq_lock->lock();
  }

  /**
   * @brief  Releases the timer queue lock
   * @return OSLockResult structure
   */
  OSLockResult putTimerQLock() {
	  return timerq_lock->unlock();
  }

  /**
   * @brief  Gets a pointer to the timer queue lock object
   * @return OSLock instance
   */
  OSLock *timerQLock() {
	  return timerq_lock;
  }
};

void tick_handler(int sig);

#endif
