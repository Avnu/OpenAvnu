WAV file interface {#wav_file_intf}
==================

# Description

WAV file interface module.

This interface module is narrowly focused to read a common wav file format
and send the data samples to mapping modules.

<br>
# Interface module configuration parameters

Name                      | Description
--------------------------|---------------------------
intf_nv_file_name         |Name of the file to be read
intf_nv_file_name_rx      |Name of wav file where received data will be stored
intf_nv_audio_rate        |Audio rate, numberic values defined by              \
                           @ref avb_audio_rate_t
intf_nv_audio_bit_depth   |Bit depth of audio, numeric values defined by       \
                           @ref avb_audio_bit_depth_t
intf_nv_audio_channels    |Number of audio channels, numeric values should be  \
                           within range of values in @ref avb_audio_channels_t
intf_nv_number_of_data_bytes|Size of sample data counted in bytes. This size   \
                             should be equal to Subchunk2Size field in wav file\
                             to be transferred. The data is printed out by     \
                             talker when started (INFO: Number of data bytes)

<br>
# Notes

There are some parameters that have to be set during configuration of this
interface module and before configuring mapping:
* [AAF audio mapping](@ref aaf_audio_map)
* [Uncompressed audio mapping](@ref uncmp_audio_map)

These parameters can be set in either:
* in the ini file (or available configuration system), so values will be parsed 
in the intf_cfg_cb function, 
* in the interface module initialization function where valid values can be 
assigned directly 

Values assigned in the intf_cfg_cb function will override any values set in the 
initialization function. 

