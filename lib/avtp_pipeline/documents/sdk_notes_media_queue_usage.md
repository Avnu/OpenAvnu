Media Queue Usage    {#sdk_notes_media_queue_usage}
=================

# Description

This section presents the work flow required for working with the Media Queue. 
Please note that most of the described steps are performed inside the AVB stack 
and are hidden from interface module implemented. There are also some 
simplifications to make description as straightforward as possible. 

<br>
# Workflow

The following work flow steps will be described.
* [Starting](@ref media_queue_usage_start) - Initialization common for talker
and listener streams
* [Talker specific](@ref media_queue_usage_talker) - Talker specific usage
* [Listener specific](@ref media_queue_usage_listener) - Listener specific usage
* [Stopping](@ref media_queue_usage_stop) - Stopping procedure common for talker
and listener streams
* [Rules](@ref media_queue_usage_rules) - general rules to follow while using 
the Media Queue 

<br>
Starting {#media_queue_usage_start}
========

First the Media Queue has to be created and initialized with correct data. Below 
are the function calls needed for proper creation and initialization. Note: 
these calls are initiated from the AVTP module. 

* @ref openavbMediaQCreate - data is allocated, queue is initialized, internal data
structures are prepared
* @ref openavbMediaQSetMaxStaleTail - sets maximum stale in microseconds before data
is being purged
* The Interface module initialization function for this media queue is called 
with media queue as a parameter, those functions 
  * Set internal interface module parameters
  * Initialize Media Queue
    * Creates Media Queue Interface Module Private Data
	* Fills the private data structure with information needed by interface 
	  module 
* Stream configuration values are processed. This actually configuration data 
may come from .ini files or internally set within the AVB host application. 
* Mapping module init function openavb_map_cb_t::map_gen_init_cb is being called 
during which several steps (media queue parameters **have to** be set) 
  * Media Queue Items count is set
  * Media Queue Items are allocated
* Interface module init function openavb_intf_cb_t::intf_gen_init_cb function is 
called 

Now the listener/talker stream is running. See next steps below for details of 
Media Queue interaction for the talker and listener. 

<br>
Talker specific flow  {#media_queue_usage_talker}
====================

As a talker, an interface module is responsible for writing data to Media Queue, 
so important steps are: 

* @ref openavbMediaQHeadLock function for getting the head of the media queue and 
marking it as locked 
* @ref openavbMediaQHeadUnlock function which releases head. Any subsequent calls to
@ref openavbMediaQHeadLock will return the same MediaQueueItem
* @ref openavbMediaQHeadPush function unlocks head previously taken by the Lock 
function and informs that work on the current Item is finished so that next call 
to @ref openavbMediaQHeadLock will return next Media Queue Item. Call of this 
function makes the item accessible via the @ref openavbMediaQTailLock function. This 
function additionally unlocks the head so the @ref openavbMediaQHeadUnlock call is 
not needed 

<br>
Listener specific flow    {#media_queue_usage_listener}
======================

As a listener, an interface module works on Media Queue tail elements. Data in 
those items is being written by the mapping module. These are the key Media 
Queue functions for the listener functionality in an interface module: 

* @ref openavbMediaQTailLock gets an item from the tail and allows working on
the current tail item of the Media Queue
* @ref openavbMediaQTailUnlock unlocks the tail element in MediaQueue which means
that interface module has stopped working on it for now but processing of
current tail element will be continued later
* @ref openavbMediaQTailPull unlocks the tail element and removes it from the Media 
Queue. This means that the interface module has finished processing of current 
tail element and it can be rewritten again by new data. This function 
additionally unlocks tail, so it is not necessary to call @ref 
openavbMediaQTailUnlock. 

<br>
Stopping     {#media_queue_usage_stop}
========

During the stopping process following action are taken

* openavb_intf_cb_t::intf_gen_end_cb is called
* openavb_map_cb_t::map_gen_end_cb is called
* Media Queue is deleted using the @ref openavbMediaQDelete function which frees
the memory taken by all internal structures of Media Queue

<br>
Guidelines    {#media_queue_usage_rules}
==========

* Calls to **Lock** functions must always be paired with their **Unlock** 
counterparts to avoid problems while working with MediaQueue. Push and Pull 
functions additional result in an unlock. 
* The current implementation Media Queue allows accessing two elements at the 
same time - one is head and second the tail 
* The number of Media Queue items and their sizes depends on configuration for 
the stream. The main driving factor is to make the data accessible for the next 
element that follows interface module. The size of the Media Queue item is a 
factor of the According to this Media Queue number of size of AVTP stream 
encapsulation payload multiplied by the packing factor. The number of Media 
Queue items required if different for a talker and listener. The talker usually 
needs minimal Media Queue items. However, the listener must have enough Media 
Queue items to buffer data until the AVTP presentation time. This is based on 
the maximum transit time for the SR Class in use. ring decoding 
* If there are not enough Media Queue items warnings will be logged to the 
implemented logging system for the port for debugging purposes. 

More implementation details might be find in @ref openavb_mediaq_pub.h
