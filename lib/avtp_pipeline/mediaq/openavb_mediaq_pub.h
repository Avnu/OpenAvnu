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
* HEADER SUMMARY : Circular queue for passing data between interfaces
*  and mappers.
*/

#ifndef OPENAVB_MEDIA_Q_PUB_H
#define OPENAVB_MEDIA_Q_PUB_H 1

#include "openavb_types_pub.h"
#include "openavb_avtp_time_pub.h"

/** \file
 * Media Queue.
 * Circular queue for passing data between interfaces and mappers.
 */

/** Media Queue Item structure.
 */
typedef struct {
	/// In a talker process this is the capture time. In a listener process this
	/// is the presentation time (AVTP timestamp).
	avtp_time_t *pAvtpTime;

	/// The structure of this memory will be defined per mapper in a public
	/// header. This is the common data payload format
	///  that mappers and interfaces will share.
	void *pPubData;

	/// Read index. User managed. It will be reset to zero when the item is
	/// pushed and pulled.
	U32 readIdx;

	/// Length of data in item.
	U32 dataLen;

	/// Size of the data item
	U32 itemSize;

	/// Flag indicating mediaQ item has been taken by a call to openavbMediaQTailItemTake()
	bool taken;

	/// Public extra map data
	void *pPubMapData;

	/// For use internally by mapping modules. Often may not be used.
	void *pPvtMapData;	

	/// For use internally by the interface. Often may not be used.
	void *pPvtIntfData;
} media_q_item_t;

/** Media Queue structure.
 * The media queue is the conduit between interface modules and mapping modules.
 * It is internally implemented as a circular FIFO container.
 * \see \ref working_with_mediaq
  */
typedef struct {
	///////////////////////////
	// Shared properties
	///////////////////////////

	/// Common name for mapping data format. Set by mapping modules and used by
	/// interface modules to validate
	///  the media queue data format compatibility.
	char *pMediaQDataFormat;

	/// Defined per mapper in the public header.
	///  The structure of this memory area will be used only the mapper module
	///  and interface module.
	void *pPubMapInfo;

	///////////////////////////
	// Private properties
	/// \privatesection
	///////////////////////////

	/// For use internally in the media queue
	void *pPvtMediaQInfo;

	/// For use internally by the mapper
	void *pPvtMapInfo;

	/// For use internally by the interface
	void *pPvtIntfInfo;
} media_q_t;

/** Create a media queue.
 *
 * Allocate a media queue structure. Only mapping modules will use this call.
 *
 * \return A pointer to a media queue structure. NULL if the creation fails
 */
media_q_t* openavbMediaQCreate();

/** Enable thread safe access for this media queue.
 *
 * In the default case a media queue is only accessed from a single thread and
 * therefore multi-threaded synchronication isn't needed. In situations where a
 * media queue can be accessed from multiple threads calling this function will
 * enable mutex protection on the head and tail related functions. Once enabled
 * for a media queue it can not be disabled.
 *
 * \param pMediaQ A pointer to the media_q_t structure
 */
void openavbMediaQThreadSafeOn(media_q_t *pMediaQ);

/** Set size of  media queue.
 *
 * Pre-allocate all the items for the media queue. Once allocated the item
 * storage will be reused as items are added and removed from the queue. Only
 * mapping modules will use this call. This must be called before using the
 * MediaQ.
 *
 * \param pMediaQ A pointer to the media_q_t structure
 * \param itemCount Maximum number of items that the queue will hold. These are
 *        pre-allocated
 * \param itemSize The pre-allocated size of a media queue item
 * \return TRUE on success or FALSE on failure
 *
 * \warning This must be called before using the MediaQ
 */
bool openavbMediaQSetSize(media_q_t *pMediaQ, int itemCount, int itemSize);

/** Alloc item map data.
 *
 * Items in the media queue may also have per-item data that is managed by the
 * mapping modules. This function allows mapping modules to specify this
 * storage.
 * Only mapping modules will use this call. This must be called before using the
 * media queue.
 *
 * \param pMediaQ A pointer to the media_q_t structure
 * \param itemPubMapSize The size of the public (shared) per-items data that
 *        will be allocated. Typically this is the size of a structure that is
 *        declared in a public header file associated with the mapping module.
 * \param itemPvtMapSize The size of the private per-items data that will be
 *        allocated. The structure of this area will not be shared outside of
 *        the mapping module
 * \return TRUE on success or FALSE on failure
 *
 * \warning This must be called before using the MediaQ
 */
bool openavbMediaQAllocItemMapData(media_q_t *pMediaQ, int itemPubMapSize, int itemPvtMapSize);

/** Alloc item interface data.
 *
 * Items in the media queue may also have per-item data that is managed by the
 * interface modules. This function allows interface modules to specify this
 * storage. This must be called before using the media queue.
 *
 * \param pMediaQ A pointer to the media_q_t structure
 * \param itemIntfSize The size of the per-items data to allocate for use by the
 *        interface module
 * \return TRUE on success or FALSE on failure
 *
 * \warning This must be called before using the MediaQ
 */
bool openavbMediaQAllocItemIntfData(media_q_t *pMediaQ, int itemIntfSize);

/** Destroy the queue.
 *
 * The media queue passed in will be deleted. This includes all allocated memory
 * both for mapping modules and interface modules. Only mapping modules will use
 * this call.
 *
 * \param pMediaQ A pointer to the media_q_t structure
 * \return TRUE on success or FALSE on failure
 */
bool openavbMediaQDelete(media_q_t *pMediaQ);

/** Sets the maximum latency expected.
 *
 * The maximum latency will be set. This value is used by the media queue to
 * help determine if a media queue item is ready to be released to the listener
 * interface module for presentation. Typically the mapping module will call
 * this function with a max latency value derived from the max_latency
 * configuration value.
 *
 * \param pMediaQ A pointer to the media_q_t structure
 * \param maxLatencyUsec The maximum latency.
 */
void openavbMediaQSetMaxLatency(media_q_t *pMediaQ, U32 maxLatencyUsec);

/** Sets the maximum stale tail.
 *
 * Used to purge media queue items that are too old.
 *
 * \param pMediaQ A pointer to the media_q_t structure
 * \param maxStaleTailUsec tail element purge threshold in microseconds
 */
void openavbMediaQSetMaxStaleTail(media_q_t *pMediaQ, U32 maxStaleTailUsec);

/** Get pointer to the head item and lock it.
 *
 * Get the storage location for the next item that can be added to the circle
 * queue. Once the function is called the item will remained locked until
 * unlocked or pushed. The lock remains valid across callbacks. An interface
 * module will use this function when running as a talker to add a new media
 * element to the queue thereby making it available to the mapping module.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \return A pointer to a media queue item. Returns NULL if head item storage
 *         isn't available.
 */
media_q_item_t *openavbMediaQHeadLock(media_q_t *pMediaQ);

/** Unlock the head item.
 *
 * Unlock a locked media queue item from the head of the queue. The item will
 * not become available for use in the queue and the data will not be cleared.
 * Subsequent calls to openavbMediaQHeadLock  will return the same item storage
 * with the same data values. An interface module will use this function when
 * running as a talker when it must release a previously locked media queue head
 * item.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 */
void openavbMediaQHeadUnlock(media_q_t *pMediaQ);

/** Unlock the head item and make it available.
 *
 * Unlock a locked media queue item from the head of the queue and make it
 * available for use in the queue to be accessed with the tail function calls.
 * An interface module will use this function when running as a talker after it
 * has locked the head item and added data to the item storage area.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \return Returns TRUE on success or FALSE on failure.
 */
bool openavbMediaQHeadPush(media_q_t *pMediaQ);

/** Get pointer to the tail item and lock it.
 *
 * Lock the next available tail item in the media queue. Available is based on
 * the timestamp that is associated with the item when it was a placed into the
 * media queue. The interface module running on a listener uses this function
 * to access the data items place into the media queue by the mapping module.
 * At some point after this function call the item must be unlocked with either
 * openavbMediaQTailUnlockor openavbMediaQTailPull on the same callback or a subsequent
 * callback.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \param ignoreTimestamp If TRUE ignore the tail item timestamp making the tail
 *        item immediately available.
 * \return A pointer to a media queue item. Returns NULL if a tail item isn't
 *         available.
 */
media_q_item_t* openavbMediaQTailLock(media_q_t *pMediaQ, bool ignoreTimestamp);

/** Unlock the tail item without removing it from the queue.
 *
 * Unlock a media queue item that was previously locked with openavbMediaQTailLock.
 * The item will not be removed from the tail of the media queue.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 */
void openavbMediaQTailUnlock(media_q_t *pMediaQ);

/** Unlock the tail item and remove it from the queue.
 *
 * Unlock a media queue item that was previously locked with openavbMediaQTailLock
 * and remove it from the media queue.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \return Returns TRUE on success or FALSE on failure.
 */
bool openavbMediaQTailPull(media_q_t *pMediaQ);

/** Take ownership from the MediaQ of an item.
 *
 * Take ownership from the MediaQ of an item previously locked
 * with openavbMediaQTailLock. Will advance the tail. Used in place
 * of openavbMediaQTailPull()
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \param pItem MediaQ item to take ownership of.
 * \return Returns TRUE on success or FALSE on failure.
 */
bool openavbMediaQTailItemTake(media_q_t *pMediaQ, media_q_item_t* pItem);

/** Give itme ownership back to the MediaQ.
 *
 * Give ownership back to the MediaQ of an item previously taken
 * with openavbMediaQTailItemTake()
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \param pItem MediaQ item to give back tot he MediaA.
 * \return Returns TRUE on success or FALSE on failure.
 */
bool openavbMediaQTailItemGive(media_q_t *pMediaQ, media_q_item_t* pItem);

/** Get microseconds until tail is ready.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \param pUsecTill An output parameter that is set with the number of
 *        microseconds until the tail item will be available.
 * \return Return FALSE if greater than 5 seconds otherwise TRUE.
 */
bool openavbMediaQUsecTillTail(media_q_t *pMediaQ, U32 *pUsecTill);

/** Check if the number of bytes are available.
 *
 * Checks were the given media queue contains bytes, returns true if it does
 * false otherwise.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \param bytes Number of bytes expected in media queue
 * \param ignoreTimestamp Ignore timestamp for byte accumulation.
 * \return TRUE if bytes are available in pMediaQ; FALSE if bytes not available
 *        in pMediaQ.
 */
bool openavbMediaQIsAvailableBytes(media_q_t *pMediaQ, U32 bytes, bool ignoreTimestamp);

/** Count number of available MediaQ items.
 *
 * Count the number of available MediaQ items. 
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \param ignoreTimestamp Ignore timestamp for byte accumulation.
 * \return The number of available MediaA items.
 */
U32 openavbMediaQCountItems(media_q_t *pMediaQ, bool ignoreTimestamp);

/** Check if there are any ready MediaQ items.
 *
 * Check if there are any ready MediaQ items.
 *
 * \param pMediaQ A pointer to the media_q_t structure.
 * \param ignoreTimestamp Ignore timestamp for checking
 * \return TRUE if there is at least 1 MediaQ item available
 * otherwise FALSE.
 */
bool openavbMediaQAnyReadyItems(media_q_t *pMediaQ, bool ignoreTimestamp);

#endif  // OPENAVB_MEDIA_Q_PUB_H
