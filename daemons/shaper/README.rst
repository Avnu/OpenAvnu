OpenAvnu Traffic Shaping Daemon
===============================

.. contents::
..
   1  Introduction
   2  Support
   3  Future Updates

Introduction
------------

The shaper daemon is an interface to use the tc (Traffic Control) command to
configure the kernel traffic shaping with the Hierarchy Token Bucket.  While
tc could be called directly, using the daemon allows for a simpler interface
and keeps track of the current traffic shaping configurations in use.

Support
-------

To enable the Shaper daemon support for AVTP Pipeline, edit the endpoint.ini
file and uncomment the port=15365 line of the shaper section.

The Shaper daemon does not work (i.e. no shaping occurs) if the Intel IGB
support is loaded for the adapter being used.  You may also need to compile
the AVTP Pipeline with PLATFORM_TOOLCHAIN=generic to not include the IGB
features.

Future Updates
--------------

- Have the daemon verify that tc is installed
- Have the daemon verify that the kernel is configured to support Hierarchy
  Token Bucket traffic shaping
- Add a method to interlace frames from multiple streams of the same class
  (perhaps using multiple layers of queues)
- Add updates to support IEEE 802.1Qcc configurable classes

