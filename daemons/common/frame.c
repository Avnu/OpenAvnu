
/*
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
*/

#include "frame.h"
#include "pdu.h"

ssize_t frame_read( struct frame *p, void const *base, ssize_t pos, size_t len )
{
    ssize_t r = pdu_validate_range( pos, len, 14 );
    if ( r >= 0 )
    {
        size_t payload_offset;
        uint16_t tag;

        p->dest_address = pdu_eui48_get( base, pos + FRAME_HEADER_DA_OFFSET );
        p->src_address = pdu_eui48_get( base, pos + FRAME_HEADER_SA_OFFSET );
        tag = pdu_uint16_get( base, pos + FRAME_HEADER_ETHERTYPE_OFFSET );

        if ( tag == FRAME_TAG_ETHERTYPE )
        {
            payload_offset = FRAME_PAYLOAD_OFFSET + FRAME_TAG_LEN;
            p->tpid = tag;
            r = pdu_validate_range( pos, len, pos + payload_offset );
            if ( r >= 0 )
            {
                uint16_t tci = pdu_uint16_get( base, pos + FRAME_HEADER_TAG_OFFSET );

                p->pcp = ( tci >> 13 ) & 0x7;
                p->dei = ( tci >> 12 ) & 1;
                p->vid = tci & 0xfff;

                p->ethertype = pdu_uint16_get( base, pos + FRAME_HEADER_ETHERTYPE_OFFSET + FRAME_TAG_LEN );
            }
        }
        else
        {
            payload_offset = FRAME_PAYLOAD_OFFSET;
            p->tpid = 0;
            p->dei = 0;
            p->pcp = 0;
            p->vid = 0;

            p->ethertype = tag;
        }

        if ( r >= 0 )
        {
            p->length = ( uint16_t )( len - payload_offset );
            memcpy( p->payload, ( (uint8_t *)base ) + pos + payload_offset, p->length );
            r = len;
        }
    }
    return r;
}

ssize_t frame_write( struct frame const *p, void *base, ssize_t pos, size_t len )
{
    ssize_t r = pdu_validate_range( pos, len, FRAME_HEADER_LEN );
    if ( r >= 0 )
    {
        size_t payload_offset;

        pdu_eui48_set( p->dest_address, base, pos + FRAME_HEADER_DA_OFFSET );
        pdu_eui48_set( p->src_address, base, pos + FRAME_HEADER_SA_OFFSET );
        if ( p->tpid == FRAME_TAG_ETHERTYPE )
        {
            payload_offset = FRAME_PAYLOAD_OFFSET + FRAME_TAG_LEN;
            r = pdu_validate_range( pos, len, payload_offset );
            if ( r >= 0 )
            {
                uint16_t tci = ( ( p->pcp & 0x7 ) << 13 ) | ( ( p->dei & 1 ) << 12 ) | ( ( p->vid ) & 0xfff );
                pdu_uint16_set( p->tpid, base, pos + FRAME_HEADER_TAG_OFFSET );
                pdu_uint16_set( tci, base, pos + FRAME_HEADER_TAG_OFFSET + FRAME_TAG_LEN );
                pdu_uint16_set( p->ethertype, base, pos + FRAME_HEADER_ETHERTYPE_OFFSET + FRAME_TAG_LEN );
            }
        }
        else
        {
            payload_offset = FRAME_PAYLOAD_OFFSET;
            pdu_uint16_set( p->ethertype, base, pos + FRAME_HEADER_ETHERTYPE_OFFSET );
        }

        r = pdu_validate_range( pos, len, pos + payload_offset + p->length );
        if ( r >= 0 )
        {
            memcpy( ( (uint8_t *)base ) + pos + payload_offset, p->payload, p->length );
        }
    }

    return r;
}

void frame_init(frame *p)
{
    p->time = 0;
    pdu_eui48_init( &p->dest_address );
    pdu_eui48_init( &p->src_address );
    p->ethertype = 0;
    p->length = 0;
    p->tpid = 0;
    p->pcp = 0;
    p->dei = 0;
    p->vid = 0;
    memset( p->payload, 0, sizeof( p->payload ) );
}
