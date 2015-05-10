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

#include "pdu.h"
#include "avtp.h"

ssize_t avtp_common_control_header_read(struct avtp_common_control_header *p,
					void const *base,
					ssize_t pos, size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, AVTP_COMMON_CONTROL_HEADER_LEN);
	if (r >= 0) {
		p->cd = avtp_common_control_header_get_cd(base, pos);
		p->subtype = avtp_common_control_header_get_subtype(base, pos);
		p->sv = avtp_common_control_header_get_sv(base, pos);
		p->version = avtp_common_control_header_get_version(base, pos);
		p->control_data =
		    avtp_common_control_header_get_control_data(base, pos);
		p->status = avtp_common_control_header_get_status(base, pos);
		p->control_data_length =
		    avtp_common_control_header_get_control_data_length(base,
								       pos);
		p->stream_id =
		    avtp_common_control_header_get_stream_id(base, pos);
	}
	return r;
}

ssize_t avtp_common_control_header_write(struct avtp_common_control_header const
					 *p, void *base, ssize_t pos,
					 size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, AVTP_COMMON_CONTROL_HEADER_LEN);
	if (r >= 0) {
		avtp_common_control_header_set_cd(p->cd, base, pos);
		avtp_common_control_header_set_subtype(p->subtype, base, pos);
		avtp_common_control_header_set_sv(p->sv, base, pos);
		avtp_common_control_header_set_version(p->version, base, pos);
		avtp_common_control_header_set_control_data(p->control_data,
							    base, pos);
		avtp_common_control_header_set_status(p->status, base, pos);
		avtp_common_control_header_set_control_data_length
		    (p->control_data_length, base, pos);
		avtp_common_control_header_set_stream_id(p->stream_id, base,
							 pos);
	}
	return r;
}
