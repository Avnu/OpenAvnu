/****************************************************************************
  Copyright (c) 2015, J.D. Koftinoff Software, Ltd.
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of J.D. Koftinoff Software, Ltd. nor the names of its
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

#ifndef FRAME_H_
#define FRAME_H_

#include "eui48.h"

/** \addtogroup frame raw ethernet frame */
/*@{*/

typedef uint64_t timestamp_in_microseconds;

#define FRAME_HEADER_DA_OFFSET (0)
#define FRAME_HEADER_SA_OFFSET (6)
#define FRAME_HEADER_TAG_OFFSET (12)
#define FRAME_TAG_LEN (4)
#define FRAME_HEADER_ETHERTYPE_OFFSET (12)
#define FRAME_HEADER_LEN (14)
#define FRAME_PAYLOAD_OFFSET (14)
#define FRAME_ETHERTYPE_LEN (2)
#define FRAME_TAG_ETHERTYPE (0x8100)

#ifndef FRAME_MAX_PAYLOAD_SIZE
#define FRAME_MAX_PAYLOAD_SIZE (1500)
#endif

struct frame {
	timestamp_in_microseconds time;
	struct eui48 dest_address;
	struct eui48 src_address;
	uint16_t ethertype;
	uint16_t tpid;
	uint16_t pcp : 3;
	uint16_t dei : 1;
	uint16_t vid : 12;
	uint16_t length;
	uint8_t payload[FRAME_MAX_PAYLOAD_SIZE];
};

void frame_init(struct frame *p);

int frame_read(struct frame *p, void const *base, int pos, int len);
int frame_write(struct frame const *p, void *base, int pos, int len);

/*@}*/

/** frame sender class */
struct frame_sender {
	void (*terminate)(struct frame_sender *);
	void (*send)(struct frame_sender *, struct frame const *frame);
};

/*@}*/

#endif /* FRAME_H_ */
