/*
Copyright (c) 2015, Jeff Koftinoff
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the {organization} nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "avdeccd.h"

struct entity_state
{
    struct adp_adv advertiser;
    struct eui64 entity_id;
    struct eui64 entity_model_id;

    int socket_fd;
} the_entity;

void frame_send(struct adp_adv *self, void *context,
           uint8_t const *buf, uint16_t len)
{
    struct entity_state *entity = (struct entity_state *)context;
}

void entity_state_init( struct entity_state *entity, const char *network_port )
{
    adp_adv_init(&entity->advertiser,entity,frame_send,0);

    eui64_init_from_uint64( &entity->entity_id, 0x70B3d5edc0000002UL );
    eui64_init_from_uint64( &entity->entity_model_id, 0x70B3d5edc0000003UL );


}

int main(int argc, char **argv)
{
    const char *network_port="en0";
    if( argc>1 )
    {
        network_port = argv[1];
    }
    entity_state_init(&the_entity,network_port);

    return 0;
}
