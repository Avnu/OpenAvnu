AVTP Interface Module Development {#sdk_avtp_interface_module_dev}
=================================

Introduction {#sdk_avtp_intf_module_introduction} 
============
Interface modules are the components that sit between the core AVB stack and the 
platform device drivers that supply access to media hardware. 

<br>
The Plug-in Architecture {#sdk_avtp_intf_module_plugin} 
========================
The OPENAVB AVB stack has a plug-in architecture for the AVTP implementation in the
form of interface modules. This allows for easily hooking into the AVB stack to
extend it for interfacing to media specific hardware.

Mapping modules, which are mentioned through-out these guides, are also
plug-ins. These implement the various AVTP encapsulations and require a deep
understanding of AVTP and therefore are developed internally by OPENAVB.

Interface modules are discovered at runtime using configuration information
that is passed to the openavbTLOpen() function. Below is an example of a section of
the configuration information with interface module values.

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// pIntfInitFn : Interface module initialization function.
// openavb_intf_initialize_fn_t)
cfg->osalCfg.pIntfInitFn = openavbIntfJ6Video;

///////////////////
// Interface module
///////////////////

// intf_nv_blocking_tx_callback : A talker only option. When set packet pacing
// is expected to be handled in the interface module.
// Commonly the TL configuration option tx_blocking_in_intf needs to be set
// to match this.
// cfg->libCfgNames[cfgIdx] = "intf_nv_blocking_tx_callback";
// cfg->libCfgValues[cfgIdx++] = "1";

// intf_nv_ignore_timestamp : If set the listener will ignore the timestamp
// on media queue items.
cfg->libCfgNames[cfgIdx] = "intf_nv_ignore_timestamp";
cfg->libCfgValues[cfgIdx++] = "0";

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The function pointer variable `pIntfInitFn` points to the interface module
initialization function.

When the talker/listener module calls the interface module initialization
function it passes in a callback structure as a parameter that must get filled
with the required callback function addresses by the interface module.

After initialization the talker/listener module will call directly into the
interface module via these callbacks. See the API reference section for details
and examples of these callbacks.

The callback functions in an interface module, aside from the initialization
call, receives a pointer to a media queue structure. This structure is
self-contained, similar to an object in C++, in that it includes a function
callback list for the interface module to call into the media queue to access
and work with the media queue.

<br>
Task / Thread Model {#sdk_avtp_intf_module_task} 
=================
In the general case the task / threading (hereafter referred to as task) model 
for interface modules is very simple. All callbacks will occur on the same task 
and therefore there are no synchronization concerns. However, access to media 
sources and media sinks can have demanding timing and processing requirements 
and it may be necessary for an interface module to have complete control over an 
execution task to ensure timely pulling or pushing data into the media hardware 
or driver. In those cases the interface module may find it a benefit to create 
its own task or tasks. 

When an interface module has its own task the point of synchronization between
the callback thread and the interface module private thread would be the
sharing of a media queue item pointer. When a head or tail item is locked a
pointer to that media queue item is returned. At that point the data area for
that item remains locked and can be used by another task beyond the scope of
the callback into the interface module. The media queue item will remain locked
until unlocked (or pushed or pulled). The media queue itself can't be locked,
it can only be accessed within the scope of a callback into the interface
module.

<br>
Building an Interface Module {#sdk_avtp_intf_module_building} 
============================
There should be minimal dependencies for creation of an interface module. The
interface module can be built as a static library and include all the required
callbacks as defined in the reference section of this document.

Access to all functionality in the AVB stack from an interface module is also
handled via callbacks that are available to the interface module in the media
queue structure it receives as a parameter on its own callbacks.

<br>
Interface Module in a Talker {#sdk_avtp_intf_module_talker} 
============================
An interface module when used in a talker pulls data from a media source and
pushes it onto the media queue in the format expected by the mapping module
configured for that stream.

<br>
Interface Module in a Listener {#sdk_avtp_intf_module_listener} 
==============================
When called from a listener it pulls data from the media queue and pushes it to
the media sink for presentation.

<br>
Working With the Media Queue {#working_with_mediaq}
============================
The media queue is the conduit between interface modules and mapping modules.
The media queue is the only entry point for interface modules to call into the
AVB stack and allows for pushing or pulling media data though it as well as
access to time functionality in the AVTP time structure.

The media queue is internally implemented as a circular FIFO container. The
format of the data for items in the media queue is defined by the mapping
module being used in the talker/listener. For some mapping modules the data
format of an item will match the defined AVB encapsulation. For example the
MPEG2-TS (61883-4) mapping module data format of media queue items are simple
192 byte MPEG2-TS source packets. Each media queue item contains one or more
MPEG2-TS source packet. The actual MPEG2-TS mapping may combine multiple source
packets into one AVTP packet but that is hidden from the interface module.

The media queue functions are accessed as API calls.

For example, the openavbMediaQHeadLock() function is called as:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    openavbMediaQHeadLock(pMediaQ);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The pMediaQ pointer is passed to all of the interface module callbacks.

Access to the queue items is accomplished with the Head functions for pushing
data into the media queue. These are used by interface modules running in a
talker. To read items from the media queue the Tail functions are used by
interface modules running in a listener.

At any one time only 2 items are accessible in the media queue. These are the
head item and tail item. Care must be taken to always match up the
openavbMediaQHeadLock() with a openavbMediaQHeadUnlock() or openavbMediaQHeadPush()
functions. The same applies that the openavbMediaQTailLock() must be paired with
either a openavbMediaQTailPull() or openavbMediaQTailUnlock() functions. This means
a new item can not be added to the media queue until a openavbMediaQHeadPush()
is called and a new item can not be pulled from the media queue until 
openavbMediaQTailPull() is called.

For a detailed work flow please visit 
[Media Queue Usage](@ref sdk_notes_media_queue_usage)

<br>
Timestamps {#sdk_avtp_intf_module_timestamps} 
==========
For interface modules often the only requirement for timestamps is to assign a
PTP time to the items added to the media queue when running as a talker. For
example:
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    openavbAvtpTimeSetToWallTime(pMediaQItem->pAvtpTime);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
In most cases the mapping modules and media queue take care of the rest of the
requirements for AVTP.

[Source code](@ref openavb_intf_echo.c)
