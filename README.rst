
.. image:: avnu_logo.png

|

.. image:: https://travis-ci.org/AVnu/Open-AVB.svg?branch=open-avb-next
   :target: https://travis-ci.org/AVnu/Open-AVB

Open AVB
========

The Open AVB project is sponsored by the AVnu Alliance.

The intent is to provide components for building AVB systems. The repository
contains primarily network building block components - drivers, libraries,
\example applications  and daemon source code - required to build an AVB system.
It is planned to eventually include the various packet encapsulation types,
protocol discovery daemons, libraries to convert media clocks to AVB clocks
and vice versa), and drivers.

This repository does not include all components required to build a full 
production AVB system (e.g. a turnkey solution to stream stored or live audio 
or video content). Some simple example applications are provided which 
illustrate the flow - but a professional AVB system requires a full media stack 
- including audio and video inputs and outputs, media processing elements, and 
various graphical user interfaces. Various companies provide such integrated 
solutions.

For more information about AVB, see also the AVnu Alliance webpage at
www.avnu.org.

BACKGROUND
===========

Intel created the Open AVB repository to encourage collaborative source code 
development for AVB technology enabling. By publishing the source code, our 
intent is to encourage standardization, stability and inter-operability between 
multiple vendors. This repository - created by the Intel LAN Access Division - 
is open for contributions from other vendors. 

LICENSING AND CONTRIBUTION GUIDELINES
======================================
To the extent possible, content is licensed under BSD licensing terms. Linux 
kernel mode components are provided under a GPLv2 license. The specific license 
information is included in the various directories to eliminate confusion. We 
encourage you to review the ‘LICENSE’ file included in the head of the 
various subdirectories for details.

Third party submissions are welcomed. Our intent for third party content 
contributions is to enable derivative products with minimal licensing 
entanglements. Practically speaking, this means we would enforce (a) an 
original-source attestation for any contributed content, and (b) rejecting 
patches with GPL content into existing “BSD” licensed components. Third 
party copyrights can be included provided they do not narrow the licensing 
terms of an existing component.

Prior to accepting a commit, Intel may perform scans using third-party tools 
to identify suspected hits of GPL code. Intel may also perform vulnerability 
scans of patches in an attempt to find various coding errors such as memory 
leaks, buffer overflows and usage of uninitialized variables. The submitter 
will be asked to correct any detected issues prior to a commit. Owners
of submitted third-party content are free to apply changes without supervision
by Intel.

The Open AVB project has a development mailing list. To subscribe, visit
https://lists.sourceforge.net/lists/listinfo/open-avb-devel to sign up.

GIT SUBMODULES
==============

After checking out the Open-AVB git repository submodules should be
configured by going::

    git submodule init
    git submodule update

CMAKE
=====

cmake is a cross-platform build system generator. cmake build files are
currently available to build mrpd in Windows and linux and CppUTest unit
tests for mrpd. The recommended usage for cmake is to create an out-of-tree
directory for cmake output of generated makefiles or MSVC project files.

Starting from the Open-AVB dir, one would go::

    mkdir tmp
    cd tmp
    cmake .. -G "Unix Makefiles"
    make

to build in Open-AVB/tmp

RELATED OPEN SOURCE PROJECTS
============================

AVDECC
------
Jeff Koftinoff maintains a repository of AVDECC example open 
source code. AVDECC is a management layer, similar to SNMP MIB formats, 
which enables remote devices to detect, enumerate and configure AVB-related 
devices based on their standardized management properties.

+ https://github.com/jdkoftinoff/jdksavdecc-c

AudioScience has created a 1722.1 C++ controller library which builds on jdkadvecc-c.

+ https://github.com/audioscience/avdecc-lib

XMOS
----
XMOS is a semiconductor company providing a reference design for AVB 
endpoints in pro audio and automotive. XMOS endpoint source code is open source 
and available on Github - https://github.com/xcore/sw_avb

