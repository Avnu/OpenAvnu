Tone Generator Interface {#tonegen_intf}
========================

# Description

This interface module is a talker only and is used to generate an audio 
tone for testing purposes. It is designed to work with the AAF (AVTP 
Audio Format) mapping but could be quickly adjusted to work with the 
61883-6 mapping module as well. 

# Interface module configuration parameters

Name                         | Description
-----------------------------|---------------------------
intf_nv_tone_hz              | The frequency of the generated tone
intf_nv_on_off_interval_msec | How many millisecs to turn on and off the tone
intf_nv_melody_string        | A simple melody to play. When a melody string is \
                               set the on/off tone hz is not used. The string   \
                               holds the notes in a 2 octave range with the use \
                               of letters A B C D E F G a b c d e f g followed  \
                               by a duration of the note. For example the full  \
                               scale is A4B4C4D4E4F4G4a4b4c4d4e4f4g4.   
intf_nv_audio_rate           | Audio rate, numberic values defined by           \
                               @ref avb_audio_rate_t
intf_nv_audio_bit_depth      | Bit depth of audio, numeric values defined by    \
                               @ref avb_audio_bit_depth_t
intf_nv_audio_channels       | Number of audio channels, numeric values should  \
                               be within range of values in                     \
                               @ref avb_audio_channels_t

