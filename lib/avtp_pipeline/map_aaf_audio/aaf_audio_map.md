AAF audio Mapping {#aaf_audio_map}
=================

# Description

Implements AAF (or "AVTP Audio Format") as described in IEEE-1722a.

Works with the ALSA interface and the WAV file interface.

# Mapping module configuration parameters

Name                | Description
--------------------|---------------------------
map_nv_item_count   |The number of Media Queue items to hold.
map_nv_tx_rate or map_nv_tx_interval | Transmit interval in frames per second. \
                     0 = default for talker class. <br> The transmit rate for  \
                     the mapping module should be set according to the sample  \
                     rate, so that the transmit interval meets the suggested   \
                     values in 1722a <ul><li>sample rate which are multiple of \
                     8000Hz <ul><li>8000 for class <b>A</b></li><li>4000 for   \
                     class <b>B</b></li></ul></li><li>sample rate which are    \
                     multiple of 44100Hz<ul><li>7350 for class <b>A</b></li>   \
                     <li>3675 for class <b>B</b></li></ul></li></ul>
map_nv_packing_factor|How many AVTP packets worth of audio data to accept in one Media Queue item

<br>
# Notes

There are additional parameters that have to be set by the interface module 
during the configuration process to ensure all sizes and rates calculate 
properly inside the mapping module. Those variables have to be set before 
*map_gen_init_cb* is being called. Commonly these additional values are set 
during the interface module configuration which does occur before the 
gen_init_cb. 

These are the fields of the @ref media_q_pub_map_uncmp_audio_info_t structure 
that have to be set during the interface module configuration: 

Name               | Description
-------------------|----------------------------
audioRate          |Rate of the audio @ref avb_audio_rate_t
audioType          |How the data is organized - what is the data type of       \
                    samples @ref avb_audio_type_t
audioBitDepth      |What is the bit depth of audio @ref avb_audio_bit_depth_t
audioChannels      |How many channels there are @ref avb_audio_channels_t
sparseMode         |Timestamping mode @ref avb_audio_sparse_mode_t

Below you can find description of how to set up those variables in interfaces
* [wav file interface](@ref wav_file_intf)
* [alsa interface](@ref alsa_intf)

**Note**: If any of these fields are not set correct the mapping module will not
configure the Media Queue correctly.

**Note**: Both the talker and listener must be configured with matching audio 
parameters. If the received data on the listener does not match the configured
parameters the stream will still be started and data
will still flow but no audio will be played.
