OpenAvnu Traffic Shaping Daemon
===============================

.. contents::
..
   1  Introduction
   2  Future Updates

Introduction
------------

The shaper daemon is an interface to use the tc (Traffic Control) command to
configure the kernel traffic shaping with the Hierarchy Token Bucket.  While
tc could be called directly, using the daemon allows for a simpler interface
and keeps track of the current traffic shaping configurations in use.

Future Updates
--------------

- Verify that tc is installed
- Verify that the kernel is configured for traffic shaping

