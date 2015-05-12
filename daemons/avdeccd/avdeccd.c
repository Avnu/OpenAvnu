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
           uint8_t const *buf, uint16_t len);

void entity_state_init( struct entity_state *entity );


/**
 * @brief found_interface
 *
 * Called whenever an ethernet capable network port interface is
 * found
 *
 * @param iter the network port iterator that owns this network port
 * @param port_info the network port
 */
static void found_interface(
        uv_rawpkt_network_port_iterator_t *iter,
        uv_rawpkt_network_port_t *port_info );

/**
 * @brief removed_interface
 *
 * Called whenever an ethernet capable network port interface
 * was removed
 *
 * @param iter the network port iterator that owns this network port
 * @param port_info the network port
 */
static void removed_interface(
        uv_rawpkt_network_port_iterator_t *iter,
        uv_rawpkt_network_port_t *port_info );

/**
 * @brief The port_context_s struct
 *
 * Local structure to keep track of a linked list of uv_rawpkt_t objects
 * for one protocol type and any additional protocol state required for them.
 *
 * These are kept in a doubly linked list so that the collection
 * of protocol handlers can forward messages between network ports if
 * necessary
 */
struct port_context_s
{
    uv_rawpkt_t rawpkt;
    int frame_received_count;
    int link_status_updated_count;
    int link_status;
    struct port_context_s *next;
    struct port_context_s *prev;
};
typedef struct port_context_s port_context_t;

/**
 * @brief portcontext_first The first entry of the port_context_t object list
 */
port_context_t *portcontext_first = 0;

/**
 * @brief portcontext_last The last entry of the port_context_t object list
 */
port_context_t *portcontext_last = 0;

/**
 * @brief received_packet callback for any received ethernet frames
 *
 * @param rawpkt The uv_rawpkt_t object that received the ethernet frames
 * @param nread The number of ethernet frames received
 * @param buf The list of nread uv_buf_t's containing the ethernet frames
 */
static void received_ethernet_frame( uv_rawpkt_t *rawpkt,
                                     ssize_t nread,
                                     const uv_buf_t *buf );

/**
 * @brief
 *
 * @param rawpkt The uv_rawpkt object that had a link status event
 * @param link_status The current link status: 1=link up, 0=link down
 */
static void link_status_updated( uv_rawpkt_t *rawpkt, int link_status );


/**
 * @brief rawpkt_closed callback that is called whenever a uv_rawpkt is closed
 *
 * @param handle The pointer to the uv_rawpkt_t as a uv_handle_t
 */
static void rawpkt_closed( uv_handle_t *handle );

/**
 * @brief finish called by a signal handler when SIGINT occurs
 *
 *
 * @param handle The sigint_t handler
 * @param sig The signal number
 */
static void finish( uv_signal_t *handle, int sig );


static void rawpkt_closed( uv_handle_t *handle )
{
    uv_rawpkt_t *rawpkt = (uv_rawpkt_t *)handle;
    port_context_t *context = (port_context_t *)rawpkt->data;

    printf( "Closed : %s after receiving %d frames. link changed %d times and is currently %d\n",
            rawpkt->owner_network_port->device_name,
            context->frame_received_count,
            context->link_status_updated_count,
            context->link_status );
    fflush(stdout);
    /*
     * Remove the associated port context
     */
    if( context->prev )
    {
        context->prev->next = context->next;
    }
    if( context->next )
    {
        context->next->prev = context->prev;
    }
    if( portcontext_first == context )
    {
        portcontext_first = context->next;
    }
    if( portcontext_last == context )
    {
        portcontext_last = context->prev;
    }
    free(context);
}

static void received_ethernet_frame( uv_rawpkt_t *rawpkt,
                             ssize_t nread,
                             const uv_buf_t *buf )
{
    int bufnum=0;
    port_context_t *context = (port_context_t *)rawpkt->data;
    const uint8_t *mac = rawpkt->owner_network_port->mac;

    /*
     * Count and print all network packets for this protocol handler
     */

    if( nread>0 )
    {
        context->frame_received_count++;
#if 1
        printf("\nFrame #%8d On: %02X:%02X:%02X:%02X:%02X:%02X :",
               context->frame_received_count,
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

        for( bufnum=0; bufnum<nread; ++bufnum )
        {
            size_t i;
            uint8_t *p = (uint8_t *)buf[bufnum].base;

            for( i=0; i<buf[bufnum].len; ++i )
            {
                printf("%02X ",(uint16_t)p[i]);
            }
            printf( "\n" );
        }
#endif
    }
    else
    {
        /*
         * Notify user that the socket was closed
         */
        printf("\nEnd of connection on: %02X:%02X:%02X:%02X:%02X:%02X :",
               mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    fflush(stdout);
}


static void link_status_updated( uv_rawpkt_t *rawpkt, int link_status )
{
    port_context_t *context = (port_context_t *)rawpkt->data;
    const uint8_t *mac = rawpkt->owner_network_port->mac;

    context->link_status = link_status;
    context->link_status_updated_count++;
    printf("\nLink status updated: %02X:%02X:%02X:%02X:%02X:%02X : %d count %d\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], context->link_status, context->link_status_updated_count );
}


static void found_interface( uv_rawpkt_network_port_iterator_t *iter,
                             uv_rawpkt_network_port_t *network_port )
{
    port_context_t *context=0;

    /**
     * Tell the user that a new network port was found
     */
    printf( "Found  : %s: %s %02X:%02X:%02X:%02X:%02X:%02X\n",
            network_port->device_name,
            network_port->device_description,
            network_port->mac[0],
            network_port->mac[1],
            network_port->mac[2],
            network_port->mac[3],
            network_port->mac[4],
            network_port->mac[5] );
    fflush(stdout);

    {
        /*
         * Allocate a new port_context_t
         */
        context=calloc(sizeof(port_context_t),1);
        if( context )
        {
            int status=-1;

            /*
             * Initialize the new uv_rawpkt_t object, attached
             * to the same loop as the iterator used
             */
            status = uv_rawpkt_init(iter->loop,&context->rawpkt);

            /*
             * Point the rawpkt.data pointer to the port_context object
             */

            context->rawpkt.data = (void *)context;

            if( status>=0 )
            {
                /*
                 * Open the uv_rawpkt_t object to listen on this
                 * network port for any IPv4 Ethertype messages
                 */
                uint16_t ethertype=AVTP_ETHERTYPE;
                status = uv_rawpkt_open(
                            &context->rawpkt,
                            network_port,
                            128,
                            1,
                            1,
                            &ethertype,
                            rawpkt_closed
                            );
            }

            if( status>=0 )
            {
                /*
                 * Start receiving ethernet frames, using the
                 * received_ethernet_frame callback
                 */
                uv_rawpkt_recv_start(
                            &context->rawpkt,
                            received_ethernet_frame );
                uv_rawpkt_link_status_start(
                            &context->rawpkt,
                            link_status_updated );
            }

            if( status>=0 )
            {
                /*
                 * Add this new port context object to the linked list
                 */
                if( portcontext_first == 0 )
                {
                    portcontext_first=context;
                    portcontext_last=context;
                }
                else
                {
                    context->next = 0;
                    context->prev = portcontext_last;
                    portcontext_last->next = context;
                    portcontext_last = context;
                }
            }

            if( status<0 )
            {
                free(context);
            }
        }
    }
}

static void removed_interface( uv_rawpkt_network_port_iterator_t *iter,
                               uv_rawpkt_network_port_t *port_info )
{
    /**
     * Tell the user that a new network port was removed
     */
    printf( "Removed: %s: %s %02X:%02X:%02X:%02X:%02X:%02X\n",
            port_info->device_name,
            port_info->device_description,
            port_info->mac[0],
            port_info->mac[1],
            port_info->mac[2],
            port_info->mac[3],
            port_info->mac[4],
            port_info->mac[5] );
    fflush(stdout);


}

static void finish( uv_signal_t *handle, int sig )
{
    uv_rawpkt_network_port_iterator_t *network_port_iterator =
            (uv_rawpkt_network_port_iterator_t *)handle->data;
    /*
     * Stop listening for the signal
     */
    uv_signal_stop(handle);

    /*
     * Stop the network port iterator
     */
    uv_rawpkt_network_port_iterator_stop( network_port_iterator );

    /*
     * Close the network port iterator and all owned network_ports
     * and all of the futher owned uv_rawpkt_t objects
     */
    uv_rawpkt_network_port_iterator_close( network_port_iterator, 0 );

}


void frame_send(struct adp_adv *self, void *context,
           uint8_t const *buf, uint16_t len)
{
    struct entity_state *entity = (struct entity_state *)context;
    /* TODO: find appropriate uvrawpkt network port to send on */
}

void entity_state_init( struct entity_state *entity )
{
    adp_adv_init(&entity->advertiser,entity,frame_send,0);

    eui64_init_from_uint64( &entity->entity_id, 0x70B3d5edc0000002UL );
    eui64_init_from_uint64( &entity->entity_model_id, 0x70B3d5edc0000003UL );

    /* TODO: initialize entity_state */
}

int main(int argc, char **argv)
{
    uv_loop_t *loop = uv_default_loop();
    uv_signal_t sigint_handle;
    uv_signal_t sigterm_handle;
    uv_rawpkt_network_port_iterator_t rawpkt_iter;

    entity_state_init(&the_entity);

    uv_rawpkt_network_port_iterator_init(loop,&rawpkt_iter);

    uv_rawpkt_network_port_iterator_start(
                &rawpkt_iter,
                found_interface,
                removed_interface);

    uv_signal_init(loop,&sigint_handle);
    sigint_handle.data = &rawpkt_iter;

    uv_signal_init(loop,&sigterm_handle);
    sigterm_handle.data = &rawpkt_iter;

    uv_signal_start(&sigint_handle,finish,SIGTERM);
    uv_signal_start(&sigint_handle,finish,SIGINT);

    uv_run( loop, UV_RUN_DEFAULT );

    return 0;
}
