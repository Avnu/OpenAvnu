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


#ifndef DUMMY_HPP
#define DUMMY_HPP

#include <packet.hpp>
#include <stdint.h>

typedef int ADAPTER_ID;
typedef enum {
	TIMINGMSMT_CONFIRM_EVENT,
	TIMINGMSMT_INDICATION_EVENT,
	CORRELATED_TIME_EVENT,
} EVENT_ID;

typedef uint32_t WifiTimestamp;
typedef uint64_t SystemTimestamp;

typedef struct {
	uint8_t dialog_token;
	WifiTimestamp request;
	WifiTimestamp ack;
} Dialog, *P_Dialog;

typedef struct {
	uint8_t element_id;
	uint8_t length;
	uint8_t data[];
} PTPContext, *P_PTPContext;

typedef struct {
	uint8_t peer_address[ETHER_ADDR_OCTETS];
	Dialog current_dialog;
	Dialog prev_dialog;
	PTPContext ptp_context;
} TmOnIndication, *P_TmOnIndication;

typedef struct {
	uint8_t peer_address[ETHER_ADDR_OCTETS];
	Dialog current_dialog;
} TmOnConfirm, *P_TmOnConfirm;

typedef struct {
	WifiTimestamp device_time;
	SystemTimestamp system_time;
} CorrelatedTime, *P_CorrelatedTime;

typedef struct {
	uint8_t local_address[ETHER_ADDR_OCTETS];
	ADAPTER_ID id;
} Adapter, *P_Adapter;

typedef struct {
	size_t count;
	Adapter *adapter;
} AdapterList, *P_AdapterList;

void GetAdapterList(P_AdapterList list);

typedef struct {
	TIMINGMSMT_CALLBACK fn;
	void *context;
} TimingMsmtCallback, *P_TimingMsmtCallback;

typedef void (*TIMINGMSMT_CALLBACK)(EVENT_ID event, void *data, void *context);

void GetAdapterList(P_AdapterList list);
void RegisterTimingMsmtCallback(P_TimingMsmtCallback callback);
void WifiTimingMeasurementRequest(ADAPTER_ID hAdapter, uint8_t *peer_address, uint8_t dialog_token, P_Dialog prev_dialog, P_PTPContext context);
void WifiCorrelatedTimestampRequest(ADAPTER_ID hAdapter);

#endif/*DUMMY_HPP*/