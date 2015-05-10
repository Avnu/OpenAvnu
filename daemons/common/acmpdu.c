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

#include <stdlib.h>
#include "acmpdu.h"

static inline ssize_t acmpdu_read(struct acmpdu *p, void const *base,
				  ssize_t pos, size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, ACMPDU_LEN);
	if (r >= 0) {
		acmpdu_common_control_header_read(&p->header, base, pos, len);
		p->controller_entity_id =
		    acmpdu_get_controller_entity_id(base, pos);
		p->talker_entity_id = acmpdu_get_talker_entity_id(base, pos);
		p->listener_entity_id =
		    acmpdu_get_listener_entity_id(base, pos);
		p->talker_unique_id = acmpdu_get_talker_unique_id(base, pos);
		p->listener_unique_id =
		    acmpdu_get_listener_unique_id(base, pos);
		p->stream_dest_mac = acmpdu_get_stream_dest_mac(base, pos);
		p->connection_count = acmpdu_get_connection_count(base, pos);
		p->sequence_id = acmpdu_get_sequence_id(base, pos);
		p->flags = acmpdu_get_flags(base, pos);
		p->stream_vlan_id = acmpdu_get_stream_vlan_id(base, pos);
		p->reserved = acmpdu_get_reserved(base, pos);
	}
	return r;
}

static inline ssize_t acmpdu_write(struct acmpdu const *p, void *base,
				   size_t pos, size_t len)
{
	ssize_t r = pdu_validate_range(pos, len, ACMPDU_LEN);
	if (r >= 0) {
		acmpdu_common_control_header_write(&p->header, base, pos, len);
		acmpdu_set_controller_entity_id(p->controller_entity_id, base,
						pos);
		acmpdu_set_talker_entity_id(p->talker_entity_id, base, pos);
		acmpdu_set_listener_entity_id(p->listener_entity_id, base, pos);
		acmpdu_set_talker_unique_id(p->talker_unique_id, base, pos);
		acmpdu_set_listener_unique_id(p->listener_unique_id, base, pos);
		acmpdu_set_stream_dest_mac(p->stream_dest_mac, base, pos);
		acmpdu_set_connection_count(p->connection_count, base, pos);
		acmpdu_set_sequence_id(p->sequence_id, base, pos);
		acmpdu_set_flags(p->flags, base, pos);
		acmpdu_set_stream_vlan_id(p->stream_vlan_id, base, pos);
		acmpdu_set_reserved(p->reserved, base, pos);
	}
	return r;
}
