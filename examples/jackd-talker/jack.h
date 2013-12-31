#ifndef _AVB_JACK_H
#define _AVB_JACK_H

#define DEFAULT_RINGBUFFER_SIZE 32768

/* Prototypes */
jack_client_t* init_jack(void);
void stop_jack(jack_client_t* client);

#endif /* _AVB_JACK_H */
