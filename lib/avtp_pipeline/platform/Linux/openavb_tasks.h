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

#ifndef _EAVBTASKS_H
#define _EAVBTASKS_H

#define THREAD_STACK_SIZE 									65536

///////////////////////////
// Platform code Tasks values
///////////////////////////

///////////////////////////
// Common code Tasks values
///////////////////////////

//task rcvThread. SRP
#define rcvThread_THREAD_STK_SIZE			    			32768

//task txThread. SRP
#define txThread_THREAD_STK_SIZE		    				32768

//task maapThread
#define maapThread_THREAD_STK_SIZE	    					16384

//task loggingThread
#define loggingThread_THREAD_STK_SIZE    					THREAD_STACK_SIZE

//task TLThread Used for both Talker and Listener threads
#define TLThread_THREAD_STK_SIZE    						THREAD_STACK_SIZE

//task TalkerThread
#define talkerThread_THREAD_STK_SIZE						THREAD_STACK_SIZE

//task ListenerThread
#define listenerThread_THREAD_STK_SIZE 						THREAD_STACK_SIZE

//task openavbAecpSMEntityModelEntityThread
#define openavbAecpSMEntityModelEntityThread_THREAD_STK_SIZE   	THREAD_STACK_SIZE

//task openavbAdpSmAdvertiseEntityThread
#define openavbAdpSmAdvertiseEntityThread_THREAD_STK_SIZE   	THREAD_STACK_SIZE

//task openavbAcmpSmListenerThread
#define openavbAcmpSmListenerThread_THREAD_STK_SIZE   			THREAD_STACK_SIZE

//task openavbAecpMessageRxThread
#define openavbAecpMessageRxThread_THREAD_STK_SIZE   			THREAD_STACK_SIZE

//task openavbAdpMessageRxThread
#define openavbAdpMessageRxThread_THREAD_STK_SIZE   			THREAD_STACK_SIZE

//task openavbAdpSmAdvertiseInterfaceThread
#define openavbAdpSmAdvertiseInterfaceThread_THREAD_STK_SIZE   	THREAD_STACK_SIZE

//task openavbAcmpMessageRxThread
#define openavbAcmpMessageRxThread_THREAD_STK_SIZE   			THREAD_STACK_SIZE

//task openavbAcmpSmTalkerThread
#define openavbAcmpSmTalkerThread_THREAD_STK_SIZE   			THREAD_STACK_SIZE

//task openavbAcmpSmTalkerThread
#define openavbAcmpSmTalkerThread_THREAD_STK_SIZE   			THREAD_STACK_SIZE















#endif // _EAVBTASKS_H
