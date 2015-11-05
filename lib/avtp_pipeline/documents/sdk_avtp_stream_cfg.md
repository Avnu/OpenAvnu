AVTP Stream (Talker/Listener) Configuration {#sdk_avtp_stream_cfg}
===========================================

Overview {#sdk_avtp_stream_cfg_overview}
========

The configuration of streams (talkers and listeners) is controlled via the 
structure @ref openavb_tl_cfg_t that is passed to the @ref openavbTLConfigure function. 
There are 3 major sections within the configuration structure. The general 
talker / listener (AVTP stream) section, the mapping module section and the 
interface module section. The general section has settings used by the 
talker/listener module directly. The mapping module section has settings 
specific to the mapping module being used for the stream and the interface 
module section has settings specific to the interface module being used for the 
stream. 

How the @ref openavb_tl_cfg_t structure gets set is platform dependent. For the 
Linux reference implementation reading from .ini files is supported which in 
turn fills this structure. For RTOSes the stream configuration structure is 
usually set directly via code in the AVB host application module. Therefore the 
use of .ini files is a layer above the what the core AVB stack uses. The Release 
Notes for the AVB port should be referenced with regards to where the 
configuration values can be set. 


Here is a sample configuration structure initialization for a H.264 listener.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void h264_SampleListenerCfg(openavb_tl_cfg_t *cfg)
{
	// Clear our the configuration structure to ensure defaults (0)
	memset(cfg, 0, sizeof(openavb_tl_cfg_t));

	// This must be set to the multicast address that is used 
	U8 multicastStrmAddr[] = { 0x91, 0xE0, 0xF0, 0x00, 0xFE, 0x00 };

	///////////////////
	// TL (Talker / Listener) Configuration
	///////////////////

	// Identify the role of the stream (talker or listener)
	// must be set to AVB_ROLE_LISTENER or AVB_ROLE_TALKER
	cfg->role = AVB_ROLE_LISTENER;

	memcpy(cfg->stream_addr.buffer.ether_addr_octet, multicastStrmAddr, ETH_ALEN);
	cfg->stream_addr.mac = &cfg->stream_addr.buffer;

	// max_interval_frames: The maximum number of packets that will be sent during 
	// an observation interval. This is only used on the talker.
	// cfg->max_interval_frames = 1;

	// max_transit_usec: Allows manually specifying a maximum transit time. 
	// On the talker this value is added to the PTP walltime to create the AVTP Timestamp.
	// On the listener this value is used to validate an expected valid timestamp range.
	// Note: For the listener the map_nv_item_count value must be set large enough to 
	// allow buffering at least as many AVTP packets that can be transmitted  during this 
	// max transit time.
	cfg->max_transit_usec = 2000;

	// max_transmit_deficit_usec: Allows setting the maximum packet transmit rate deficit that will
	// be recovered when a talker falls behind. This is only used on a talker side. When a talker
	// can not keep up with the specified transmit rate it builds up a deficit and will attempt to 
	// make up for this deficit by sending more packets. There is normally some variability in the 
	// transmit rate because of other demands on the system so this is expected. However, without this
	// bounding value the deficit could grew too large in cases such where more streams are started 
	// than the system can support and when the number of streams is reduced the remaining streams 
	// will attempt to recover this deficit by sending packets at a higher rate. This can cause a problem
	// at the listener side and significantly delay the recovery time before media playback will return 
	// to normal. Typically this value can be set to the expected buffer size (in usec) that listeners are 
	// expected to be buffering. For low latency solutions this is normally a small value. For non-live 
	// media playback such as video playback the listener side buffers can often be large enough to held many
	// seconds of data.
	// cfg->max_transmit_deficit_usec = 2000;

	// internal_latency: Allows mannually specifying an internal latency time. This is used
	// only on the talker.
	//	cfg->internal_latency = 0;

	// The number of microseconds a media queue items can be passed the presentation time at which
	// point it will be purged.
	cfg->max_stale = 500;

	// number of intervals to handle at once (talker)
	// cfg->batch_factor = 1;

	// report_seconds: How often to output stats. Defaults to 10 seconds. 0 turns off the stats. 
	cfg->report_seconds = 0;

	// sr_class: A talker only setting. Values are either SR_CLASS_A or SR_CLASS_B.
	// cfg->sr_class = SR_CLASS_A;

	// raw_tx_buffers: The number of raw socket transmit buffers.
	// cfg->raw_tx_buffers = 40;

	// raw_rx_buffers: The number of raw socket receive buffers.
	cfg->raw_rx_buffers = 40;

	// tx_blocking_in_intf : A talker only option. When set packet pacing is expected to be handled in the interface module.
	// Commonly there will be a matching interface module configuration item that needs to be set.
	// cfg->tx_blocking_in_intf = FALSE;

	///////////////////
	// The remaining configuration items vary depending on the mapping module and interface module being used.
	// These configuration values are populated as name value pairs.
	///////////////////

	// Configuration index counter.
	int cfgIdx = 0;

	///////////////////
	// Mapping module
	///////////////////
	// map_nv_item_count: The number of media queue elements to hold.
	cfg->libCfgNames[cfgIdx] = "map_nv_item_count";
	cfg->libCfgValues[cfgIdx++] = "10";

	// map_nv_tx_rate: Transmit rate. Typically this is set to match the SR Class. 8000 for A and 4000 for B
	// cfg->libCfgNames[cfgIdx] = "map_nv_tx_rate";
	// cfg->libCfgValues[cfgIdx++] = "8000";

	// map_nv_max_payload_size: This is the max RTP payload size. See RFC 6184 for details, 
       // 1412 is the default size
	cfg->libCfgNames[cfgIdx] = "map_nv_max_payload_size";
	cfg->libCfgValues[cfgIdx++] = "1412";
	
	///////////////////
	// Interface module
	///////////////////

	// intf_nv_blocking_tx_callback : A talker only option. When set packet pacing is expected to be handled in the interface module.
	// Commonly the TL configuration option tx_blocking_in_intf needs to be set to match this.
	// cfg->libCfgNames[cfgIdx] = "intf_nv_blocking_tx_callback";
	// cfg->libCfgValues[cfgIdx++] = "1";

	// intf_nv_ignore_timestamp : If set the listener will ignore the timestamp on media queue items.
	cfg->libCfgNames[cfgIdx] = "intf_nv_ignore_timestamp";
	cfg->libCfgValues[cfgIdx++] = "0";

	cfg->nLibCfgItems = cfgIdx;

	///////////////////
	// Mapping and interface modules main initialization entry points
	///////////////////

	// pMapInitFn : Mapping module initialization function. (openavb_map_initialize_fn_t)
	cfg->osalCfg.pMapInitFn = openavbMapH264Initialize;

	// pIntfInitFn : Interface module initialization function. (openavb_intf_initialize_fn_t)
	cfg->osalCfg.pIntfInitFn = openavbIntfJ6Video;
} 

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

<br>
Common Stream Configuration {#sdk_avtp_stream_cfg_common}
===========================
These are common stream configuration values. 

Name                | Description
--------------------|------------
role                |Sets the process as a talker or listener. Valid values are\
                     *talker* or *listener*.
stream_addr         |Used on the listener and should be set to the mac address \
                     of the talker.
stream_uid          |The unique stream ID. The talker and listener must both   \
                     have this set the same.
dest_addr           |Destination multicast address for the stream.<br>         \
                     If using                                                  \
                     <ul><li><b>with MAAP</b> - dynamic destination addresses  \
                     are generated automatically by the talker and passed to   \
                     the listener, and don't need to be configured.</li>       \
                     <li><b>without MAAP</b>, locally administered (static)    \
                     addresses must be configured. Those addresses are in the  \
                     range of: 91:E0:F0:00:FE:00 - 91:E0:F0:00:FE:FF. Typically\
                     use :00 for the first stream, :01 for the second, etc.    \
                     </li></ul>                                                \
                     If <b>SRP</b>                                             \
                     <ul><li><b>is being</b> used the static destination       \
                     address only needs to be set in the talker;</li>          \
                     <li><b>is not being</b> used the destination address need \
                     to be set (to the same value) in both the talker and      \
                     listener.</li></ul>                                       \
                     The destination is a multicast address, not a real MAC    \
                     address, so it does not match the talker or listener's    \
                     interface MAC. There are several pools of those addresses \
                     for use by AVTP defined in 1722.
max_interval_frames |The maximum number of packets that will be sent during an \
                     observation interval. This is only used on the talker.
max_frame_size      |Maximum size of the frame
sr_class            |A talker only setting. Values are either A or B. If not   \
                     set an internal default is used.
sr_rank             |A talker only setting. If not set an internal default is  \
                     used.
max_transit_usec    |Allows manually specifying a maximum transit time.        \
                     <ul><li><b>On the talker</b> this value is added to the   \
                     PTP walltime to create the AVTP Timestamp.</li>           \
                     <li><b>On the listener</b> this value is used to validate \
                     an expected valid timestamp range.</li></ul>              \
                     <b>Note:</b> For the listener the map_nv_item_count value  \
                     must be set large enough to allow buffering at least as   \
                     many AVTP packets that can be transmitted during this max \
                     transit time.
max_transmit_deficit_usec |Allows setting the maximum packet transmit rate     \
                     deficit that will be recovered when a talker falls behind.\
                     <p>When a talker can not keep up with the specified       \
                     transmit rate it builds up a deficit and will attempt to  \
                     make up for this deficit by sending more packets. There is\
                     normally some variability in the transmit rate because of \
                     other demands on the system so this is expected. However, \
                     without this bounding value the deficit could grew too    \
                     large in cases such where more streams are started than   \
                     the system can support and when the number of streams is  \
                     reduced the remaining streams will attempt to recover this\
                     deficit by sending packets at a higher rate. This can     \
                     cause a problem at the listener side and significantly    \
                     delay the recovery time before media playback will return \
                     to normal.</p>                                            \
                     <p>Typically this value can be set to the expected buffer \
                     size (in usec) that listeners are expected to be          \
                     buffering.<br>                                            \
                     For low latency solutions this is normally a small value. \
                     For non-live media playback such as video playback the    \
                     listener side buffers can often be large enough to held   \
                     many seconds of data.</p>                                 \
                     <b>Note:</b> This is only used on a talker side.
internal_latency    |Allows mannually specifying an internal latency time. This\
                     is used only on the talker.
max_stale           |The number of microseconds beyond the presentation time   \
                     that media queue items will be purged because they are too\
                     old (past the presentation time).<br>                     \
                     This is only used on listener end stations.               \
                     <p><b>Note:</b> needing to purge old media queue items is \
                     often a sign of some other problem.<br>                   \
                     For example: a delay at stream startup before incoming    \
                     packets are ready to be processed by the media sink.<br>  \
                     If this deficit in processing or purging the old (stale)  \
                     packets is not handled, syncing multiple listeners will be\
                     problematic.</p>
raw_tx_buffers      |The number of raw socket transmit buffers. Typically 4 - 8\
                     are good values. This is only used by the talker. If not  \
                     set internal defaults are used.
raw_rx_buffers      |The number of raw socket receive buffers. Typically 50 -  \
                     100 are good values. This is only used by the listener.   \
                     If not set internal defaults are used.
report_seconds      |How often to output stats. Defaults to 10 seconds. 0 turns\
                     off the stats.
tx_blocking_in_intf |The interface module will block until data is available.  \
                     This is a talker only configuration value and not all interface modules support it.     
pMapInitFn          |Pointer to the mapping module initialization function.    \
                     Since this is a pointer to a function addresss is it not  \
		     directly set in platforms that use a .ini file. 
IntfInitFn          |Pointer to the interface module initialization function.  \
                     Since this is a pointer to a function addresss is it not  \
		     directly set in platforms that use a .ini file. 


<br>
# Platform Specific Stream Configuration Values
Some platform ports have unique configuration values. 

## Linux

Name                      | Description
--------------------------|---------------------------
map_lib                   |The name of the library file (commonly a .so file) that   \
                           implements the Initialize function.<br>                   \
                           Comment out the map_lib name and link in the .c file to   \
                           the openavb_tl executable to embed the mapper directly into   \
                           the executable unit. There is no need to change anything  \
                           else. The Initialize function will still be dynamically   \
                           linked in.
map_fn                    |The name of the initialize function in the mapper
intf_lib                  | The name of the library file (commonly a .so file) \
                            that implements the Initialize function.<br>       \
                            Comment out the intf_lib name and link in the .c   \
                            file to the openavb_tl executable to embed the         \
                            interface directly into the executable unit.<br>   \
                            There is no need to change anything else.          \
                            The Initialize function will still be dynamically  \
                            linked in
intf_fn                   | The name of the initialize function in the interface


<br>
Example Interface / Mapping Combinations {#sdk_avtp_stream_cfg_combinations}
========================================
Below are a few interface / mapping module combinations. Notice that a single 
interface module may work with mutliple mapping modules. Additionally some 
mappings may work with multiple interface modules. 

interface module            | mapping module        | description
----------------------------|-----------------------|------------
[echo](@ref echo_host_intf) |[pipe](@ref pipe_map)  |Demonstration interface   \
                                                     used mostly for           \
                                                     verification and testing  \
                                                     purposes.
[alsa](@ref alsa_intf)      |[uncmp_audio](@ref uncmp_audio_map)|Audio         \
                                                     interface created for     \
                                                     demonstration on Linux.   \
                                                     Can be used to play       \
                                                     captured (line in, mic)   \
                                                     audio stream via EAVB
[alsa](@ref alsa_intf)      |[aaf_audio](@ref aaf_audio_map)|Audio             \
                                                     interface created for     \
                                                     demonstration on Linux.   \
                                                     Can be used to play       \
                                                     captured (line in, mic)   \
                                                     audio stream via EAVB
[wav_file](@ref wav_file_intf)|[uncmp_audio](@ref uncmp_audio_map)|            \
                                                     Configuration for playing \
                                                     wave file via EAVB


<br>
Interface and Mapping Module Configuration {#sdk_avtp_stream_cfg_intf_map}
==========================================

Each interface module and mapping module has unique configuration values. 
Details of these configuration values can be found in the reference pages for 
each module. 

- Reference: AVTP Mapping Modules 
	- [1722 AAF (aaf_audio)](@ref aaf_audio_map)
	- [Control (ctrl)](@ref ctrl_map)
	- [Motion JPEG (mjpeg)](@ref mjpeg_map)
	- [MPEG2 TS (mpeg2ts)](@ref mpeg2ts_map)
	- [NULL (null)](@ref null_map)
	- [PIPE (pipe)](@ref pipe_map)
	- [61883-6 (uncmp_audio)](@ref uncmp_audio_map)
- Reference: AVTP Interface Module
	- [Control (ctrl)](@ref ctrl_intf)
	- [Echo (echo)](@ref echo_host_intf)
	- [Null (null)](@ref null_host_intf)
	- [Viewer (viewer)](@ref viewer_intf)
- Reference: AVTP Interface Module Linux Specific
	- [ALSA (alsa)](@ref alsa_intf)
	- [MJPEG GST (mjpeg_gstreamer)](@ref mjpeg_gst_intf)
	- [MPEG2 TS File (mpeg2ts_file)](@ref mpeg2ts_file_intf)
	- [MPEG2 TS GST (mpeg2ts_gstreamer)](@ref mpeg2ts_gst_intf)
	- [WAV File (wav_file)](@ref wav_file_intf)


