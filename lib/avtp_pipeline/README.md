# STC AVTP Pipeline Contribution Notes

## General Status
- Consider the AVTP Pipeline a work in progress.
- Integrated with OpenAVB:
    - gPTP
    - MSRP
    - igb direct for packet TX.
    - igb credit based shaper
    - build system
- Not yet integreated with OpenAVB:
    - MAAP
- Credit-based shaper algorithm is not used for individual streams.
- Testing of various mappings and benchmarking has been performed. Look at the end of this file for benchmark results.
- Documentation and doc generation has not been fully updated.

## Building Current OpenAVB
### Tool chain and libraries
- Ubuntu 14.04
- Install dependencies for OpenAVB ($ sudo apt-get install ...)
    - $ sudo apt-get install build-essential
    - $ sudo apt-get install libpcap-dev
    - $ sudo apt-get install libpci-dev
    - $ sudo apt-get install libsndfile1-dev
    - $ sudo apt-get install libjack-dev
    - $ sudo apt-get install linux-headers-generic
        - linux-headers (same version as the kernel you're building for)

- Install dependencies for AVTP pipeline
    - $ sudo apt-get install libglib2.0-dev
    - $ sudo apt-get install libgstreamer0.10-dev
    - $ sudo apt-get install libgstreamer-plugins-base0.10-dev
    - $ sudo apt-get install libasound2-dev 

### Building everything
- Building from the repo root
- $ ARCH=I210 make all

### Building just AVTP pipeline.
- $ make avtp_pipeline

Binaries will be installed in lib/avtp_pipeline/build/bin.

### Building AVTP pipeline without SRP.
- $ AVB_FEATURE_ENDPOINT=0 make avtp_pipeline

Make sure to call `make avtp_pipeline_clean` before.


### Building AVTP pipeline documentation
- $ make avtp_pipeline_doc

## Running OpenAVB daemons
- Helper scripts in the repo root.
- `$ sudo ./run_igb.sh eth1`
    - Load the igb driver. Supply the interface name (ethx) as parameter.
- `$ sudo ./run_gptp.sh eth1`
    - Start gptp daemon. Supply the interface name (ethx) as parameter.
- `$ sudo ./run_srp.sh eth1`
    - Start msrp daemon. Supply the interface name (ethx) as parameter.

## Running OpenAVB simple talker example
- `$ sudo ./run_simple_talker.sh eth1`
    - Run the current OpenAVB simple talker example.  Supply the interface name (ethx) as parameter.

## Running STC Echo Talker
- `$ sudo ./run_echo_talker.sh eth1`
    - Run the AVTP Echo talker test stream. Supply the interface name (ethx) as parameter.

## Running STC Echo Listener
- `$ sudo ./run_echo_listener.sh eth1`
    - Run the AVTP Echo talker test stream. Supply the interface name (ethx) as parameter.


## Benchmark results

All test done on DELL Optiplex 755 with Intel Core 2 Duo CPU E8400 @ 3.00GHz 

| Type    | Comment      | Class | Streams | CPU Talker | CPU Listener |
|:-------:| -------------|:-----:| -------:| ----------:| ------------:|
| 61883-6 | 48kHz stereo |   A   |       1 |        3.2%|          4.9%|
|         |              |       |       2 |       22.8%|          9.3%|
|         |              |       |       4 |       28.2%|         20.3%|
|         |              |       |       8 |       39.7%|         35.0%|
|         |              |       |      16 |       60.8%|         73.1%|
| aaf     | 48kHz stereo |   A   |       1 |        3.4%|          4.8%|
|         |              |       |       2 |       22.7%|          8.9%|
|         |              |       |       4 |       28.7%|         19.4%|
|         |              |       |       8 |       40.5%|         35.7%|
|         |              |       |      16 |       63.0%|         74.8%|
| h264    | 4675 Kbps vbr|   A   |       1 |        3.2%|         27.0%|
|         |              |       |       2 |        6.4%|         59.8%|
|         |              |       |       4 |       10.5%|        108.1%|
|         |              |       |       6 |       15.6%|        149.9%|
| mpeg2ts | 1405 Kbps vbr|   A   |       1 |        2.5%|         14.7%|
|         |              |       |       2 |        5.0%|         40.9%|
|         |              |       |       4 |        7.6%|         86.3%|
|         |              |       |       6 |       10.7%|        123.5%|
| mjpeg   | live camera  |   A   |       1 |       11.5%|         10.2%|
| mjpeg   | videotestsrc |   A   |       1 |       10.0%|          6.4%|
|         |              |       |       2 |       21.4%|         13.0%|
|         |              |       |       4 |       40.5%|         26.9%|
|         |              |       |       6 |       57.7%|         42.0%|
|         |              |   B   |      12 |      113.0%|         79.0%|

## More examples

Below are examples of AVTP pipeline usage with various stream types (mappings). These commands were used for generating benchmark results above. AVTP pipeline was compiled without SRP support (`AVB_FEATURE_ENDPOINT=0 make avtp_pipeline`).

	IFNAME=eth0
	STREAMS=7
	CLASS=A
	RATE=8000
	TRANSIT_USEC=2000
	REPORT=0 # statistics display interval in seconds (0 turns off statistics)

	# AAF talker
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad aaf_file_talker.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,map_nv_sparse_mode=0,intf_nv_file_name=test01.wav,report_seconds=$REPORT
	# AAF listener
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad aaf_listener_auto.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,map_nv_sparse_mode=0,intf_nv_audio_endian=big,report_seconds=$REPORT

	# 61883-6 talker
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad wav_file_talker.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,intf_nv_file_name=test01.wav,report_seconds=$REPORT
	# 61883-6 listener
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad alsa_listener.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,report_seconds=$REPORT

	# H264 talker
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad h264_gst_talker.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,report_seconds=$REPORT
	# H264 listener
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad h264_gst_listener.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,report_seconds=$REPORT

	# MJPEG talker
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad mjpeg_gst_talker.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,report_seconds=$REPORT
	# MJPEG listener
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad mjpeg_gst_listener.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,report_seconds=$REPORT

	# MPEG2TS talker
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad mpeg2ts_file_talker.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,report_seconds=$REPORT
	# MPEG2TS listener
	sudo ./openavb_harness -I $IFNAME -s $STREAMS -d 0 -a a0:36:9f:2d:01:ad mpeg2ts_gst_listener.ini,sr_class=$CLASS,map_nv_tx_rate=$RATE,max_transit_usec=$TRANSIT_USEC,report_seconds=$REPORT
