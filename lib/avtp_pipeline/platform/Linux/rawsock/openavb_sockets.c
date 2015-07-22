/*************************************************************************************************************
Copyright (c) 2012-2013, Symphony Teleca Corporation, a Harman International Industries, Incorporated company
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

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "stc_avb_platform.h"

#include "stc_avb_ether_hal.h"
#include "stc_qmgr.h"
#include "stc_sockets.h"
#include "stc_avb_tasks.h"
#include "stc_avb_types.h"
#include "stc_array.h"
#include "stc_queue.h"
#include "stc_debug.h"

#define	AVB_LOG_COMPONENT	"OSAL Socket Layer"
#include "stc_avb_pub.h"
#include "stc_avb_log.h"

#define SOCK_VALID (1<<0)
#define SOCK_BIND (1<<1)
#define SOCK_MULTICAST (1<<2)

#define VLAN_TYPE_NS 0x0081

STC_CODE_MODULE_PRI

THREAD_TYPE(socketRXThread);
THREAD_DEFINITON(socketRXThread);

THREAD_TYPE(socketTXThread);
THREAD_DEFINITON(socketTXThread);

STC_DATA_PRI COMPILER_ALIGNED(8) static MUTEX_HANDLE_ALT(gSocketMutex);
#define SOCKET_LOCK() MUTEX_LOCK_ALT(gSocketMutex)
#define SOCKET_UNLOCK() MUTEX_UNLOCK_ALT(gSocketMutex)

STC_DATA_PRI COMPILER_ALIGNED(8) static MUTEX_HANDLE_ALT(gSocketTxMutex);
#define SOCKET_TX_LOCK() MUTEX_LOCK_ALT(gSocketTxMutex)
#define SOCKET_TX_UNLOCK() MUTEX_UNLOCK_ALT(gSocketTxMutex)

typedef struct {
	int flags;
	uint16_t protocol;						// in network format
	int mark;
	uint8_t multicastAddr[6];
	bool bAvtpSubtype;						// Filter based on AvtpSubType
	uint8_t avtpSubtype; 					// AVTP subtype to filter on Including the CD indicator
	bool bPollMode;							// TRUE is socket RX using Poll FD
	bool bRxSignalMode;						// TRUE to send signal on RX packet
	bool bRx;								// Socket used for RX (used for filtering rx packets)
	bool bTx;								// Socket used for TX (used for filtering tx packets)
	STC_POLLFD_DEF pollFd;	
	
	thread_type *pTask;						// Direct task resume via Task Manager
	rxSockBufStruct *rxSockBuf;
	stc_queue_t rxQueue;
	stc_queue_t txQueue;					// Currently not used
	int sockId;
} socket_t;

// Array of sockets
STC_DATA_PRI COMPILER_ALIGNED(8) stc_array_t socketArray;

thread_type *pSocketRXTask = NULL;
thread_type *pSocketTXTask = NULL;

static inline socket_t *findSocket(int sk)
{
	return stcArrayDataIdx(socketArray, sk);
}

void socketRXDirectAVTP(void)
{
	U32 frameSize;
	U8 *pRxBuf = halGetRxBufAVTP(&frameSize);
	while (pRxBuf) {
		// If AVB isn't running yet toss AVTP packets		  
		if (bAVBRunning) {
			ethFrameStruct *ethFrame;
			ethFrame = (ethFrameStruct *)pRxBuf;

			U32 iter;
			stc_array_elem_t elem;
			elem = stcArrayIterFirstAlt(socketArray, &iter);
			while (elem) {
				socket_t *sock = stcArrayData(elem);

				if (sock &&
					(sock->flags & SOCK_BIND) &&
					(sock->bRx ) &&
					((memcmp((void *)sock->multicastAddr, ethFrame->destAddr, 6) == 0)))
				{
					stc_queue_elem_t elem = stcQueueHeadLock(sock->rxQueue);
					if (elem) {
						eth_frame_t *pFrame = stcQueueData(elem);
						pFrame->length = frameSize;
						stc_safe_memcpy(pFrame->data, pRxBuf, frameSize);
						stcQueueHeadPush(sock->rxQueue);

						if (sock->bRxSignalMode) {
							TASK_MGR_SIGNAL(sock->pTask);
						}
						else {
							if (stcQueueGetElemCount(sock->rxQueue) >= (stcQueueGetQueueSize(sock->rxQueue) >> 1)) {
								// rx buffer 1/2 full start signalling anyway
								TASK_MGR_SIGNAL(sock->pTask);
							}
						}
					}
				}

				elem = stcArrayIterNextAlt(socketArray, &iter);
			}
		}
		// Read next frame			
		pRxBuf = halGetRxBufAVTP(&frameSize);
	}
}

// Socket RX Task 
static void socketRXThreadFn(void *p_arg)
{
	pSocketRXTask = TASK_MGR_TASKDATA(socketRXThread);
	stcTaskMgrAdd(pSocketRXTask);
	
	while (bAVBRunning) {
		// The HAL Ethernet driver is the only way this task will wakeup.
		// NOTE: Using a semaphore here is costly therefore task suspend is used in combination with TaskMgr.
		TASK_MGR_SUSPEND(pSocketRXTask);

		U32 frameSize;
		bool bPtpCBUsed;
		U8 *pRxBuf = halGetRxBufAVB(&frameSize, &bPtpCBUsed);
		while (pRxBuf || bPtpCBUsed) {
			if (pRxBuf) {
				ethFrameStruct *ethFrame;
				U16 ethernetType;
				U16 typeVlan;
			
				ethFrame = (ethFrameStruct *)pRxBuf;
				typeVlan = ntohs(*(U16 *)(&ethFrame->payload + 2));
				ethernetType = ntohs(ethFrame->type);
			
				U32 iter;
				stc_array_elem_t elem;
				elem = stcArrayIterFirstAlt(socketArray, &iter);
				while (elem) {
					socket_t *sock = stcArrayData(elem);

					if (sock &&
						(sock->flags & SOCK_BIND) &&
						(sock->bRx ) &&
						(((ethernetType == VLAN_TYPE_NS) && (sock->protocol == typeVlan)) || (ethernetType == sock->protocol)) &&
						(((sock->flags & SOCK_MULTICAST) == 0) || (memcmp((void *)sock->multicastAddr, ethFrame->destAddr, 6) == 0)) &&
						((sock->bAvtpSubtype == FALSE) || (pRxBuf[18] == sock->avtpSubtype)))
					 {
						stc_queue_elem_t elem = stcQueueHeadLock(sock->rxQueue);
						if (elem) {
							eth_frame_t *pFrame = stcQueueData(elem);
							pFrame->length = frameSize;
							stc_safe_memcpy(pFrame->data, pRxBuf, frameSize);
							stcQueueHeadPush(sock->rxQueue);

							if (sock->bRxSignalMode) {
								if (sock->bPollMode) {
									STC_POLLFD_SIG(&sock->pollFd);
								}
								else {
									TASK_MGR_SIGNAL(sock->pTask);
								}
							}
							else {
								if (stcQueueGetElemCount(sock->rxQueue) >= (stcQueueGetQueueSize(sock->rxQueue) >> 1)) {
									// rx buffer 1/2 full start signalling anyway
									if (sock->bPollMode) {
										STC_POLLFD_SIG(&sock->pollFd);
									}
									else {
										TASK_MGR_SIGNAL(sock->pTask);
									}
								}							  
							}
						}
						else {
							// TODO: This output causes lockup. Why?
							// AVB_LOG_ERROR("out of socket RX Queue buffers");
						}
					}

					elem = stcArrayIterNextAlt(socketArray, &iter);
				}
			}

			// Read next frame			
			pRxBuf = halGetRxBufAVB(&frameSize, &bPtpCBUsed);
		}
	}
}

// Socket TX Task : Currently not used
#if 0
static void socketTXThreadFn(void *p_arg)
{
	pSocketTXTask = TASK_MGR_TASKDATA(socketTXThread);
	stcTaskMgrAdd(pSocketTXTask);
  
	while (bAVBRunning) {
		TASK_MGR_SUSPEND(pSocketTXTask);
		
		// TODO: This algorithm for sending is minimalistic and inefficient and will need to be revisited
		//  when more than 1 stream is supported.
		// Send as many frames as the Hal Ether will take
		U32 iter;
		stc_array_elem_t sockElem;
		sockElem = stcArrayIterFirstAlt(socketArray, &iter);
		while (sockElem) {
			socket_t *sock = stcArrayData(sockElem);

			stc_queue_elem_t elem = stcQueueTailLock(sock->txQueue);
			while (elem) {
				eth_frame_t *pFrame = stcQueueData(elem);
				
				if (!halSendTxBuffer(pFrame->data, pFrame->length, TC_AVB_MARK_CLASS(sock->mark))) {
					// No buffers were available. Release the queue item and process next time.
					stcQueueTailUnlock(sock->txQueue);
					break;	
				}
				
				stcQueueTailPull(sock->txQueue);
				elem = stcQueueTailLock(sock->txQueue);
			}
			
			sockElem = stcArrayIterNextAlt(socketArray, &iter);
		}
	}
}
#endif

bool osalInitSockets(U32 maxSockets)
{
  	// Check parameter
	if (!maxSockets)  
		return FALSE;
	
	// Create array to hold sockets	
	socketArray = stcArrayNewArray(sizeof(socket_t));
	if (!socketArray)
		return FALSE;
	if (!stcArraySetInitSize(socketArray, maxSockets))
		return FALSE;

	MUTEX_CREATE_ALT(gSocketMutex);
	MUTEX_CREATE_ALT(gSocketTxMutex);
	
	// Start the Rx Parser Task
	{
		bool errResult;
		THREAD_CREATE(socketRXThread, socketRXThread, NULL, socketRXThreadFn, NULL);
		THREAD_CHECK_ERROR(socketRXThread, "Task create failed", errResult);
		if (errResult);		// Already reported
	}

	// Start the Tx Parser Task : Currently not used
#if 0	
	{
		bool errResult;
		THREAD_CREATE(socketTXThread, socketTXThread, NULL, socketTXThreadFn, NULL);
		THREAD_CHECK_ERROR(socketTXThread, "Task create failed", errResult);
		if (errResult);		// Already reported
	}
#endif	
	
	return TRUE;
}

int osalSocket(int domain, int type, int protocol)
{
	int retSk = -1;

	// only support PF_PACKET and SOCK_RAW
	if ((domain == PF_PACKET) && (type == SOCK_RAW)) {
		SOCKET_LOCK();

		stc_array_elem_t elem = stcArrayNew(socketArray);
		if (elem) {
			socket_t *newSocket = stcArrayData(elem);
			newSocket->sockId = stcArrayGetIdx(elem);
			newSocket->flags = SOCK_VALID;						// socket is valid
			newSocket->protocol = ntohs((uint16_t)protocol);	// store protocol in network format
			newSocket->bRxSignalMode = TRUE;

			newSocket->pTask = stcTaskMgrGetCurrentTask();
	
			retSk = newSocket->sockId;
		}
		else {
			AVB_LOG_ERROR("Too many sockets");
		}
		SOCKET_UNLOCK();
	}

	return retSk;
}

int osalSetsockopt(int sk, int level, int optname, const void *optval, socklen_t optlen)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock && (optval != NULL)) {
		if ((level == SOL_SOCKET) && (optname == SO_MARK) && (optlen == sizeof(int))) {
			sock->mark = *(int *)optval;
			retSk = sk;
		}
		else if ((level == SOL_SOCKET) && (optname == PACKET_ADD_MEMBERSHIP) && (optlen == 6)) {
			stc_safe_memcpy(sock->multicastAddr, optval, 6);
			sock->flags |= SOCK_MULTICAST;
			if (halEtherAddMulticast(sock->multicastAddr, TRUE))
				retSk = sk;
		}
		else if ((level == SOL_SOCKET) && (optname == PACKET_DROP_MEMBERSHIP)) {
			if (halEtherAddMulticast(sock->multicastAddr, FALSE))
				retSk = sk;
			memset(sock->multicastAddr, 0, 6);
			sock->flags &= ~SOCK_MULTICAST;
		}
	}

	return retSk;
}

int osalSetAvtpSubtype(int sk, uint8_t avtpSubtype)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock) {
		sock->bAvtpSubtype = TRUE;
		sock->avtpSubtype = avtpSubtype;
		retSk = sk;
	}

	return retSk;
}

int osalEnablePoll(int sk)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock) {
		sock->bPollMode = TRUE;
		STC_POLLFD_INIT(&sock->pollFd);
		retSk = sk;
	}

	return retSk;
}


int osalSetRxMode(int sk, int bufCount, int bufSize)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock) {
		sock->bRx = TRUE;
		
		sock->rxQueue = stcQueueNewQueue(sizeof(eth_frame_t) + bufSize, bufCount);
		if (!sock->rxQueue) {
			AVB_LOG_ERROR("Out of memory creating socket rxQueue");
		}
		else {
			retSk = sk;
		}
	}

	return retSk;
}

int osalSetRxSignalMode(int sk, bool rxSignalMode)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock) {
		sock->bRxSignalMode = rxSignalMode;
	}

	return retSk;
}


int osalSetTxMode(int sk, int bufCount, int bufSize)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock) {
		sock->bTx = TRUE;

		sock->txQueue = stcQueueNewQueue(sizeof(eth_frame_t) + bufSize, bufCount);
		if (!sock->txQueue) {
			AVB_LOG_ERROR("Out of memory creating socket txQueue");
		}
		else {
			retSk = sk;
		}
	}

	return retSk;
}

int osalBind(int sk)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock) {
		sock->flags |= SOCK_BIND;
		retSk = sk;
	}

	return retSk;
}

int osalCloseSocket(int sk)
{
	int retSk = -1;
	socket_t *sock = findSocket(sk);

	if (sock) {
		SOCKET_LOCK();

		// check to see if there is a multicastAddr, if so do a halEtherAddMulticast(del)
		if (sock->flags & SOCK_MULTICAST) {
			// delete multicast addr
			if (!halEtherAddMulticast(sock->multicastAddr, 0)) {
				AVB_LOG_ERROR("Sock Close: HAL delete multicast");
			}
		}

		STC_POLLFD_DESTROY(&sock->pollFd);

		if (sock->bRx) {
			stcQueueDeleteQueue(sock->rxQueue);
		}
		if (sock->bTx) {
			stcQueueDeleteQueue(sock->txQueue);
		}

		stc_array_elem_t elem = stcArrayIdx(socketArray, sk);
		if (elem) {
			stcArrayDelete(socketArray, elem);
		}

		SOCKET_UNLOCK();
		
		retSk = sk;
	}

	return retSk;
}

STC_POLLFD_PTR osalGetSocketPollFd(int sk)
{
	socket_t *sock = findSocket(sk);

	if (sock) {
		return &sock->pollFd;
	}
	return NULL;
}


int osalGetRxSockLevel(int sk)
{
	int retLevel = 0;
	socket_t *sock = findSocket(sk);

	if (sock) {
		retLevel = stcQueueGetElemCount(sock->rxQueue);
	}
	
	return retLevel;
}

int osalGetTxSockLevel(int sk)
{
	int retLevel = 0;
	socket_t *sock = findSocket(sk);

	if (sock) {
		retLevel = stcQueueGetElemCount(sock->txQueue);
	}
	
	return retLevel;
}

// Get the next Rx buffer locking it. Null if none available
eth_frame_t *osalSocketGetRxBuf(int sk)
{
	socket_t *sock = findSocket(sk);

	if (sock) {
		stc_queue_elem_t elem = stcQueueTailLock(sock->rxQueue);
		if (elem) {
			return (eth_frame_t *)stcQueueData(elem);
		}
	}
	
	return NULL;
}

// Release (unlock) the Rx buffer for a socket keeping it in the queue. Returns TRUE on success
bool osalSocketRelRxBuf(int sk)
{
	socket_t *sock = findSocket(sk);

	if (sock) {
		stcQueueTailUnlock(sock->rxQueue);
		return TRUE;
	}
	return FALSE;	
}

// Pulls the Rx buffer off the queue. Returns TRUE on success.
bool osalSocketPullRxBuf(int sk)
{
	socket_t *sock = findSocket(sk);

	if (sock) {
		stcQueueTailPull(sock->rxQueue);
		return TRUE;
	}
	return FALSE;
}

// Get the next Tx buffer locking it. Null if none available
eth_frame_t *osalSocketGetTxBuf(int sk)
{
	socket_t *sock = findSocket(sk);

	if (sock) {
		stc_queue_elem_t elem = stcQueueHeadLock(sock->txQueue);
		if (elem) {
			eth_frame_t *pEthFrame = stcQueueData(elem);
			pEthFrame->length = stcQueueGetElemSize(sock->txQueue);
			return pEthFrame;
		}
	}
	
	return NULL;
}

// Release (unlock) the Tx buffer. Returns TRUE on success
bool osalSocketRelTxBuf(int sk)
{
	socket_t *sock = findSocket(sk);

	if (sock) {
		stcQueueHeadUnlock(sock->txQueue);
		return TRUE;
	}
	return FALSE;
}

// Pushes the Tx buffer onto the queue. Returns TRUE on success.
bool osalSocketPushTxBuf(int sk)
{
	socket_t *sock = findSocket(sk);

	if (sock) {
		stcQueueHeadPush(sock->txQueue);
		 
		SOCKET_TX_LOCK();
		stc_queue_elem_t elem = stcQueueTailLock(sock->txQueue);
		while (elem) {
			eth_frame_t *pFrame = stcQueueData(elem);
			
			if (!halSendTxBuffer(pFrame->data, pFrame->length, TC_AVB_MARK_CLASS(sock->mark))) {
				// No buffers were available. Release the queue item and process next time.
				stcQueueTailUnlock(sock->txQueue);
				break;	
			}
			
			stcQueueTailPull(sock->txQueue);
			elem = stcQueueTailLock(sock->txQueue);
		}
		SOCKET_TX_UNLOCK();
		
		return TRUE;
	}
	return FALSE;
}
