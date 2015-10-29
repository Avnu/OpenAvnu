Notes on the AVTP Audio Format mapping module:

Implements AAF (or "AVTP Audio Format") as described in IEEE-1722a (Draft 11).

Works with the ALSA interface and the WAV file interface.

The transmit rate (map_nv_tx_rate) for the mapping module
should be set according to the sample rate, so that the transmit
interval meets the suggested values in 1722a:

For audio sample rates which are a multiple of  8000hz: use 8000 for class A, 4000 for class B
For audio sample rates which are a multiple of 44100hz: use 7350 for class A, 3675 for class B

AAF supports the following ALSA sample parameters:

- sample rate:
  intf_nv_audio_rate = 8, 16, 32, 44.1, 48, 88.2, 96, 176.4 or 192Khz
- signed integer, unsigned integer or floating point samples
  intf_nv_audio_type = int, uint or float
- bit depth
  intf_nv_audio_bit_depth = 16, 24 or 32 for integers; 32 for float
- number of audio channels
  intf_nv_audio_channels = 1 - 8
- sample endian-ness
  intf_nv_audio_end = big or little

The ALSA library on various platforms will only support certain
combinations of the sample parameters. Typically, only sample rate and
bit depth need to be configured, and the other parameters may be left
to their defaults.

The WAV file interface will send data that matches the encoding of the
WAV file.

Both the talker and listner must be configured with matching sample
parameters. If the received data does not match the configured
parameters on the listener, the stream will still be setup and data
will still flow - but no audio will be played.

Sample talker and listener ini files are provided in aaf_talker.ini
and aaf_listener.ini.

