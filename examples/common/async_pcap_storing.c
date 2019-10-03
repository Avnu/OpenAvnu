#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pcap.h>
#include <unistd.h>
#include <sys/time.h> 
#include "async_pcap_storing.h"

struct storing_packet {
    void *mem;
    uint32_t size;
    uint32_t used_size;
    uint32_t real_size;
    uint64_t ts;
    struct storing_packet *next;
};

struct pcap_store_control {
    struct storing_packet *free;
    struct storing_packet *busy;
    struct storing_packet *last_busy;
    pcap_dumper_t *pd;
    pthread_t thread_id;
    pthread_mutex_t free_lock;
    pthread_mutex_t busy_lock;
    uint32_t stop_signal;
    uint32_t stop_confirm;
};

static void release_packets(struct storing_packet *packet)
{
    struct storing_packet *next;
    while( packet ) {
        next = packet->next;
        if( packet->mem ) {
            free(packet->mem);
        }
        free(packet);
        packet = next;
    }
}

static struct storing_packet *alloc_packet(uint32_t size)
{
    struct storing_packet *packet = malloc(sizeof(*packet));
    if( packet ) {
        packet->next = NULL;
        packet->size = size;
        packet->used_size = 0;
        packet->real_size = 0;
        packet->ts = 0;
        packet->mem = malloc(size);
        if( !packet->mem ) {
            free(packet);
            packet = NULL;
        }
    }
    return packet;
}

static void dump_in_file(void *context, struct storing_packet *store_packet)
{
    struct pcap_store_control *ctrl = (struct pcap_store_control *)context;
    while( store_packet ) {
        if( store_packet->real_size ) {
            struct pcap_pkthdr pkt_hdr;
            pkt_hdr.ts.tv_sec = store_packet->ts / 1000000000;
            pkt_hdr.ts.tv_usec = (store_packet->ts - 1000000000 * pkt_hdr.ts.tv_sec); // / 1000;
            pkt_hdr.caplen = store_packet->used_size;
            pkt_hdr.len = store_packet->real_size;
            pcap_dump((u_char *)ctrl->pd, &pkt_hdr, store_packet->mem);
            store_packet->used_size = 0;
            store_packet->real_size = 0;
            //printf("Store packet %p in file\n", store_packet);
        }
        store_packet = store_packet->next;
    }
}

static void *store_thread(void *context)
{
    int res;
    struct pcap_store_control *ctrl = (struct pcap_store_control *)context;
    struct storing_packet *store_packet = NULL;
    ctrl->stop_confirm = 0;
    while( !ctrl->stop_signal ) {
        if( store_packet ) {
            res = pthread_mutex_trylock(&ctrl->free_lock);
            if( res == EBUSY ) {
                usleep(1);
                continue;
            } else if ( res ) {
                printf("Mutex error: %s!", strerror(errno));
                break;
            }
            store_packet->next = ctrl->free;
            ctrl->free = store_packet;
            pthread_mutex_unlock(&ctrl->free_lock);
            store_packet = NULL;
        }

        if( !ctrl->busy ) {
            usleep(1);
            continue;
        }

        res = pthread_mutex_trylock(&ctrl->busy_lock);
        if( res == EBUSY ) {
            usleep(1);
            continue;
        } else if ( res ) {
            printf("Mutex error: %s!", strerror(errno));
            break;
        }
        store_packet = ctrl->busy;
        ctrl->busy = ctrl->busy->next;
        if( ctrl->last_busy == store_packet ) {
            ctrl->last_busy = ctrl->busy;
        }
        pthread_mutex_unlock(&ctrl->busy_lock);
        store_packet->next = NULL;
        dump_in_file(context, store_packet);
    }
    //printf("Thread exit. Busy packets: %p\n", ctrl->busy);
    if( store_packet ) {
        release_packets(store_packet);
    }

    ctrl->stop_confirm = 1;
    return NULL;
}

int async_pcap_store_packet(void *context, void *buf, uint32_t size, uint64_t ts)
{
    struct pcap_store_control *ctrl = (struct pcap_store_control *)context;
    struct storing_packet *store_packet = NULL;
    int i, res = 0;
    if( ctrl->free ) {
        for( i = 100; i; i--) {
            res = pthread_mutex_trylock(&ctrl->free_lock);
            if( !res ) {
                break;
            } else if ( res !=EBUSY ) {
                printf("Mutex error: %s!", strerror(errno));
                break;
            }
            usleep(1);
        }
    } else {
        res = ENOMEM;
    }

    if( res ) {
        store_packet = alloc_packet(size);
    } else {
        store_packet = ctrl->free;
        ctrl->free = store_packet->next;
        pthread_mutex_unlock(&ctrl->free_lock);
        store_packet->next = NULL;
    }

    //printf("Add packet. %p. Res %x\n", store_packet, res);
    if( !store_packet ) {
        return -ENOMEM;
    }

    store_packet->used_size = size < store_packet->size ? size : store_packet->size;
    store_packet->real_size = size;
    memcpy(store_packet->mem, buf, store_packet->used_size);
    store_packet->ts = ts;

    for( i = 1000; i; i--) {
        res = pthread_mutex_trylock(&ctrl->busy_lock);
        if( !res ) {
            break;
        } else if ( res !=EBUSY ) {
            printf("Mutex error: %s!", strerror(errno));
            break;
        }
        usleep(1);
    }
    if( !res ) {
        if( ctrl->last_busy ) {
            ctrl->last_busy->next = store_packet;
        } else {
            ctrl->busy = store_packet;
        }
        ctrl->last_busy = store_packet;
        pthread_mutex_unlock(&ctrl->busy_lock);
    } else {
        printf("Loose packet! ts %lu\n", ts);
        release_packets(store_packet);
    }
    return res;
}

void async_pcap_release_context(void *context)
{
    struct pcap_store_control *ctrl = (struct pcap_store_control *)context;
    if( ctrl ) {
        int i;
        ctrl->stop_signal = 1;
        for(i = 100; i && !ctrl->stop_confirm; i-- ) {
            usleep(1);
            continue;
        };

        if( !ctrl->stop_confirm ) {
            void *ret;
            pthread_cancel(ctrl->thread_id);
            pthread_join(ctrl->thread_id, &ret);
            printf("Cannot close thread correctly! Cancel it! Result: %p.", ret);
        }

        if( ctrl->busy ) {
            dump_in_file(context, ctrl->busy);
            release_packets(ctrl->busy);
            ctrl->busy = NULL;
            ctrl->last_busy = NULL;
        }

        if( ctrl->pd ) {
            pcap_dump_close(ctrl->pd);
            ctrl->pd = NULL;
        }

        if( ctrl->free ) {
            release_packets(ctrl->free);
            ctrl->free = NULL;
        }
        pthread_mutex_destroy(&ctrl->busy_lock);
        pthread_mutex_destroy(&ctrl->free_lock);
        free(ctrl);
    }
}

int async_pcap_initialize_context(char *file_name, uint32_t packet_count, uint32_t packet_size, void **context)
{
    int res;
    struct pcap_store_control *ctrl;
    pcap_t *pcap;

    if( !context || !packet_count || !packet_size ) {
        return -EINVAL;
    }

    ctrl = (struct pcap_store_control *)malloc(sizeof(*ctrl));
    if( !ctrl ) {
        return -ENOMEM;
    }
    ctrl->free = NULL;
    ctrl->busy = NULL;
    ctrl->stop_signal = 0;
    ctrl->stop_confirm = 1;
    ctrl->last_busy = NULL;

    if( res = pthread_mutex_init(&ctrl->busy_lock, NULL) ) {
        free(ctrl);
        return res;
    }

    if( res = pthread_mutex_init(&ctrl->free_lock, NULL) ) {
        pthread_mutex_destroy(&ctrl->busy_lock);
        free(ctrl);
        return res;
    }

    pcap = pcap_open_dead_with_tstamp_precision(DLT_EN10MB, packet_size, PCAP_TSTAMP_PRECISION_NANO);
    ctrl->pd = pcap_dump_open(pcap, file_name);
    if( ctrl->pd == NULL )  {
        async_pcap_release_context(ctrl);
        printf("PCAP Dump open error! %s\n", pcap_geterr(pcap));
        return -ENFILE;
    }

    while( --packet_count ) {
        struct storing_packet *new_pkt = alloc_packet(packet_size);
        if( !new_pkt ) {
            printf("Cannot allocate memory for packets! Rest of packets: %d. Packet size %d\n", packet_count, packet_size);
            break;
        }
        new_pkt->next = ctrl->free;
        ctrl->free = new_pkt;
    }

    if( packet_count > 0 ) {
        async_pcap_release_context(ctrl);
        printf("Cannot allocate memory! Packet count %d. Error: %s\n", packet_count, strerror(res));
        return -ENOMEM;
    }

    res = pthread_create(&ctrl->thread_id, NULL, &store_thread, ctrl);
    if( res )
    {
        printf("Cannot create thread! %s\n", strerror(res));
        async_pcap_release_context(ctrl);
        return res;
    }
    for( res = 100; res && ctrl->stop_confirm; res-- ) {
        usleep(100);
    }

    if( ctrl->stop_confirm ) {
        printf("Thread is not run! Exit.");
        async_pcap_release_context(ctrl);
        return res;
    }

    *context = ctrl;
    return 0;
}
//EOF
