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

#ifndef AVB_SCHED_H
#define AVB_SCHED_H

// Macros to map stream/class into 16-bit fwmark
// Bottom 8 bits are used for stream index (allows 32 streams/class)
// Upper  8 bits are used for class index
//
// The fwmark is attached to the socket, and the kernel uses it to
// steer AVTP frames into the correct queues.
//
// If we ever want more than 256 classes (won't happen) or 256 streams
// per class (very unlikely) we'll need to shift the boundary between
// the bits used for the class idex and those used for stream idx.
//
// If the combination of the two needs more than 16 bits, we'll have
// to use something other than the FWMARK to communicate with the kernel.
//
#define TC_AVB_CLASS_SHIFT		8
#define TC_AVB_STREAM_MASK 		((1 << TC_AVB_CLASS_SHIFT) - 1)
#define TC_AVB_MARK_CLASS(M) 	(((M) >> TC_AVB_CLASS_SHIFT) - 1)
#define TC_AVB_MARK_STREAM(M) 	((M)  & TC_AVB_STREAM_MASK)
#define TC_AVB_MARK(C,S)		(((C + 1) << TC_AVB_CLASS_SHIFT) | (TC_AVB_STREAM_MASK & (S)))

#ifndef AVB_CLASS_LABEL
#define AVB_CLASS_LABEL(C) ('A'+C)
#endif

#define AVB_ENABLE_HWQ_PER_CLASS 1

// Modes for qdisc shaping
//
typedef enum {
	// Disable FQTSS
	AVB_SHAPER_DISABLED,
	// Drop all frames
	AVB_SHAPER_DROP_ALL,
	// Treat everything as non-SR traffic
	AVB_SHAPER_ALL_NONSR,
	// Drop all AVB stream frames
	AVB_SHAPER_DROP_SR,
	// Shaping done in SW
	// - Credit-based shaping per-class (in software)
	// - Priority between classes
	// - WRR for streams within each class
	AVB_SHAPER_SW,
#ifdef AVB_ENABLE_HWQ_PER_CLASS
	// Shaping in HW w/TXQ per AVB class
	// - Each AVB class gets its own txq w/shaping in HW
	// - SW does round-robin among classes to keep all TX queues fed
	// - SW does WRR for streams within each class
	AVB_SHAPER_HWQ_PER_CLASS,
#endif
#ifdef AVB_ENABLE_HWQ_PER_STREAM
	// Shaping in HW w/TXQ per AVB stream
	// - Each AVB stream gets its own txq w/shaping in HW
	// - SW does round-robin among streams to keep all TX queues fed
	AVB_SHAPER_HWQ_PER_STREAM
#endif
} avb_shaper_mode;

/* Options/Stats for AVB qdisc
 *  (information passed back/forth between kernel and userland)
 */
struct tc_avb_qopt {
	__u16	limit;			// number of packets that may be queued in qdisc
	__u16	num_classes;	// number of SR classes
	__u16	num_streams;	// number of streams per SR class
	__u16	linkBytesPerSec;  // link speed
	__u8	mode;			// shaping mode (HW or SW)
};

/* Options/Stats for AVB streams
 *  (information passed back/forth between kernel and userland)
 */
struct tc_avb_sopt {
	__u32	mark;				// mark associated with stream
	__u32	streamBytesPerSec;	// reserved bandwith (non-zero for established stream)
	__u16	maxIntervalFrames;	// max frames per interval
	__u16	maxFrameSize;		// max frame size
	__u32	nDropped, nQueued, nSent;
};

enum {
	TCA_AVB_UNSPEC,
	TCA_AVB_QOPT,
	TCA_AVB_SOPT,
	__TCA_AVB_MAX,
};

#define TCA_AVB_MAX (__TCA_AVB_MAX - 1)\

#define AVB_CLASS_A 2
#define AVB_CLASS_B 1
#define AVB_CLASS_NR 0

#endif
