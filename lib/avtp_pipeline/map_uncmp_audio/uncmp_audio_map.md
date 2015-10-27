Uncompressed audio Mapping {#uncmp_audio_map}
==========================

# Description

Uncompressed audio mapping module conforming to AVB 61883-6 encapsulation.

# Mapping module configuration parameters

Name                | Description
--------------------|---------------------------
map_nv_item_count   |The number of media queue elements to hold.
map_nv_tx_rate or map_nv_tx_interval | Transmit interval in frames per second. \
                     0 = default for talker class. <br> The transmit rate for  \
                     the mapping module should be set according to the sample  \
                     rate, so that the transmit interval meets the suggested   \
                     values in 1722a <ul><li>sample rate which are multiple of \
                     8000Hz <ul><li>8000 for class <b>A</b></li><li>4000 for   \
                     class <b>B</b></li></ul></li><li>sample rate which are    \
                     multiple of 44100Hz<ul><li>7350 for class <b>A</b></li>   \
                     <li>3675 for class <b>B</b></li></ul></li></ul>
map_nv_packing_factor|How many frames of audio to accept in one media queue item
map_nv_audio_mcr     |Media clock recovery,<ul><li>0 - No Media Clock Recovery \
                      default option</li><li>1 - MCR done using AVTP timestamps\
                      </li><li>2 - MCR using Clock Reference Stream</li></ul>

# Notes

There are additional parameters that have to be set by intf module during
configuration process to make everything calulated properly inside mapping.
Those variables have to be set before *map_gen_init_cb* is being called.
One of the approaches might be setting them during reading of the ini
configuration and setting of the interface parameters.

Fields of a structure media_q_pub_map_uncmp_audio_info_t that have to be set
during interface configuration:
Name               | Description
-------------------|----------------------------
audioRate          |Rate of the audio @ref avb_audio_rate_t
audioType          |How the data is organized - what is the data type of       \
                    samples @ref avb_audio_type_t
audioBitDepth      |What is the bit depth of audio @ref avb_audio_bit_depth_t
audioChannels      |How many channels there are @ref avb_audio_channels_t
sparseMode         |Timestamping mode @ref avb_audio_sparse_mode_t \
                    (not used in this mapping module)

Below you can find description of how to set up those variables in interfaces
* [wav file interface](@ref wav_file_intf)
* [alsa interface](@ref alsa_intf)

**Note**: If one of those fields will not be set, mapping module will not
configure media queue correctly.

**Note**: Both the talker and listner must be configured with matching sample
parameters. If the received data does not match the configured
parameters on the listener, the stream will still be setup and data
will still flow - but no audio will be played.
