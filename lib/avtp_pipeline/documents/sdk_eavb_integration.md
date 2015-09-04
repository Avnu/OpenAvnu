EAVB Integration Guide {#sdk_integration}
======================

Introduction {#sdk_integration_introduction}
============
Integrating the OPENAVB AVB stack into a larger solution can be conceptually divided 
into 2 parts. The first is the configuration and start up of the AVB components 
such as gPTP and AVTP. The second part is the configuration of the streams that 
will be active. The details of stream configuration is described in another 
section. 

<br>
AVTP Control {#sdk_integration_avtp_control}
============
Only a handful of functions are needed to control the AVTP component. This is
done indirectly when opening, running and closing talkers and listeners. The
general flow is:
1. openavbTLInitialize() is called to initialize the openavb_tl module.
2. openavbTLOpen() is called one or more times to load talkers and listeners.
3. openavbTLConfigure() is called for each talker or listener.
4. openavbTLRun() is called for each talker and listener to start their stream.
5. openavbTLClose() is called for each talker and listener to stop the stream and
   close.

<br>
Host Application Integration {#sdk_integration_host_app}
============================
Controlling the non-AVTP components of the AVB stack are platform dependent. 
Release notes for the specific port should be referenced for those details. 


