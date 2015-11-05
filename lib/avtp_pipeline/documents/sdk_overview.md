EAVB SDK Overview {#sdk_overview}
=================


Introduction {#sdk_overview_introduction}
============
Symphony Teleca Corporation (OPENAVB) has developed a software Protocol Stack to 
support Ethernet Audio Video Bridging (EAVB or AVB) on a variety of platforms. 
The software includes explicit Operating System and Hardware Abstraction Layers 
(OSAL and HAL) to facilitate rapid and efficient porting of the generic stack to 
specific platforms. The complete OPENAVB EAVB implementation, and its role within an 
overall EAVB system are discussed in OPENAVB?s Ethernet Audio Video Bridging (AVB) 
High Level Specification (OPENAVB13-01059). 

The OPENAVB AVB stack is designed to work on both General Purpose OSes (GPOS), such 
as Linux, as well as Real-Time OSes (RTOS). There are some difference in 
concepts and terms between a GPOS and RTOS, for example threads vs tasks, or the 
concept of multiple processes. For the purposes of the SDK guides the terms 
thread and task are used interchangeably. 

The guides in this SDK focus on integrating the OPENAVB AVB stack in an application, 
developing interface module components and configuration AVB streams. These 
guides are separated into four sections: 

- [EAVB SDK Overview](@ref sdk_overview)
- [EAVB Integration](@ref sdk_integration)
- [AVTP Interface Module Development](@ref sdk_avtp_interface_module_dev)
- [AVTP Stream Configuration](@ref sdk_avtp_stream_cfg)

This overview section will cover general architecture.

<br>
Glossary {#sdk_overview_glossary}
========
**AAF:** AVTP Audio Format

**AVB:** Audio Video Bridging (used interchangeably with EAVB)

**AVB Stack:** The primary functionality that implements the various AVB 
protocols 

**AVTP:** Audio Video Transport Protocol

**EAVB:** Ethernet Audio Video Bridging (used interchangeably with AVB)

**EMAC:** Ethernet Media Access Control

**FQTSS:** Forwarding and Queuing enhancements for Time Sensitive Streams

**GPOS:** General purpose OS

**gPTP:** generalized Precision Time Protocol

**HAL:** Hardware Abstraction Layer

**Listener:** Receives an AVTP stream, unpacks it and pushes it to a media sink 
for playback. It consists of a dedicated task and uses functionality in the AVTP 
module, one mapping module and one interface module. 

**IEEE:** Institute of Electrical and Electronics Engineers

**Interface Module:** An AVTP component that when called from the talker pulls 
data from a media source and pushes it onto the media queue in the format 
expected by the mapping module running on the talker. When called from a 
listener it pulls data from the media queue and pushes it to the media sink for 
playback.

**ISR:** Interrupt Service Routine

**MAAP:** Multicast Address Allocation Protocol

**MAC:** Media Access Control

**Mapping Module:** An AVTP component that when called from the talker pulls 
data from the media queue and packages it into the AVTP data payload according 
to a specific AVB encapsulation. When called from a listener it unpacks the AVTP 
data payload according to the specific AVB encapsulation and pushes it on to the 
media queue.

**Media Queue:** A circular FIFO container used to opaquely pass media data
blocks between interface modules and mapping modules. This is the only way
interface modules and mapping modules communicate.

**MJPEG:** Motion Joint Photographic Expert Group

**MPEG2-TS:** Motion Picture Expert Group (version 2) Transport Stream

**OS:** Operating System

**OSAL:** Operating System Abstraction Layer

**SDK:** Software Development Kit

**SR:** Stream Reservation

**RTOS:** Real-time OS

**SRP:** Stream Reservation Protocol

**OPENAVB:** Symphony Teleca Corporation

**Stream:** A series of AVTP data packets

**Talker:** Takes an audio or video source and transmits it as an AVTP stream on 
the AVB network. It consists of a dedicated task and uses functionality in the 
AVTP module, one mapping module and one interface module. 

<br>
Architecture {#sdk_overview_architecture}
============

The complete OPENAVB EAVB implementation and its role within an overall EAVB system 
are discussed in OPENAVB?s Ethernet Audio Video Bridging (AVB) High Level 
Specification (OPENAVB13-01059). 

What follows in this section are details that more directly effect the use and 
understanding of the SDK. 

<br>
Below is the component diagram of the Core AVB stack. Notice that the interface 
modules are not shown as part of the formal core stack but instead the 
interfaces that they use in the core stack are shown. This is also true for the 
interfaces the host application will use (TL APIs). 

@image html   Core_AVB.png "Core AVB"
@image latex  Core_AVB.png "Core AVB" width=15cm

<br>
General Purpose OSes will generally have the core AVB stack split across 
multiple processes. Whereas in an RTOS everything sits within a single execution 
image. Here is a process diagram of the components split across processes in the 
Linux reference implementation. 

@image html  fig1.png "AVB Components"
@image latex fig1.png "AVB Components" width=15cm

As shown above the AVTP component of AVB is present in the application task.
The libopenavbAVBStack library gets initialized and loaded via the APIs exposed and
documented here. This static library implements that AVTP functionality as well
as controlling talker and listener initialization and life cycle. The PTP task
initialization is handled during stack initialization.

<br>
The common AVTP stream data flow is shown here.

@image html   AVTP_Data_Flow.png "AVTP Data Flow"
@image latex  AVTP_Data_Flow.png "AVPT Data Flow" width=15cm

<br>
The diagram below shows an audio stream flowing through the AVB system on the 
Linux platform. Typically the talker and listener will be in different tasks and 
may use different interfaces. 

@image html  fig2.png "AVB Audio Stream Flow"
@image latex fig2.png "AVB Audio Stream Flow" width=15cm

<br>
Understanding the stream life-cycle is important for use of this SDK. Below are 
sequence diagrams that show the component interaction for key stream use cases. 

@image html   Stream_Initialize.png "Stream Initialization"
@image latex  Stream_Initialize.png "Stream Initialization" width=15cm

<br>
@image html   Talker_Stream_Data_Flow.png "Talker Stream Data Flow"
@image latex  Talker_Stream_Data_Flow.png "Talker Stream Data Flow" width=15cm

<br>
@image html   Listener_Stream_Data_Flow.png "Listener Stream Data Flow"
@image latex  Listener_Stream_Data_Flow.png "Listener Stream Data Flow" width=15cm

<br>
Integration, Extension and Configuration {#sdk_overview_integration}
========================================
Three distinct uses of the SDK are as follows.

**Integration:** This refers to the integration of the OPENAVB AVB stack into an 
existing or new application. This is also sometimes referred to as the hosting 
application. For details related to this see [EAVB Integration](@ref 
sdk_integration) 

**AVTP Interface Module Development:** In order for the AVB stack to be used it 
must have interaction with media data sources and sinks on the platform. This is 
accomplished with Interface modules. The AVB stack ported to a platform may 
include some interface modules already but typically new interface modules are 
needed for the specific hardware and drivers being targeted. See [AVTP Interface 
Module Development](@ref sdk_avtp_interface_module_dev) for more details. 

**Configuration:** Configuration can be split into 2 distinct area. Hosting 
application configuration and stream configuration. Hosting application 
configuration is port specifici and therefore not covered in detail in this SDK. 
However, in the configuration guide section of the SDK some host application 
configuration example are provided as a point of reference. The other primary 
area of configuration is related to stream. See the section [AVTP Stream 
Configuration](@ref sdk_avtp_stream_cfg) for more details. 

<br>
Platform Specific Considerations {#sdk_overview_platform}
================================
The OPENAVB AVB stack is designed for portability covering both general purpose OSes 
and real-time OSes. The base public APIs of the SDK do not change depending on 
the port. However, some elements of working with the SDK do change depending on 
the port of the OPENAVB stack. See the release notes for the specific port for 
details that may be beyond the common SDK usage. 

## Threads / Tasks
Within the SDK guides the terms threads and tasks are used interchangeability 
and for the purposes of the SDK they can be considered the same. 

## Processes
Some platform OSes support multiple processes. The OPENAVB stack for a particular 
port may make use of multiple processes. For example in the Linux reference 
implementation gPTP resided in a separate process. Additionally in this 
reference implementation the talk / listener end-station functionality for each 
stream can be in a single process or split across multiple processes. Whereas in 
a RTOS there isn't typically the concept of multile processes. 

## Configuration Overview
Configuration of both the host application as well as the streams can vary based 
on platform capabilities. For example on configuration information may come from 
.ini files on the device and in other platforms there may not be a file system 
in which case configuration may be set at build time. 

## Startup
See the port specific Release Notes for the details on AVB start up. 

## Libraries
The AVB core stack commonly is build as a static library and linked with a 
hosting application. Interface modules may be built differently depending on the 
platform. In some cases they my be dynamic libraries and other cases they may be 
static libraries. It is also possible that the interface modules will be built 
with the host application or as part of the AVB core library. 

