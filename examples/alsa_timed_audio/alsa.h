#ifndef ALSA_H
#define ALSA_H

#include <audio_output.h>

struct isaudk_output *
isaudk_get_default_alsa_output();

struct isaudk_output *
alsa_open_output( char *devname );

struct isaudk_input *
isaudk_get_default_alsa_input();

struct isaudk_input *
alsa_open_input( char *devname );

#endif/*ALSA_H*/
