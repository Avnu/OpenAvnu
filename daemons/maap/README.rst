OpenAvnu MAAP
=============

.. contents::
..
   1  Introduction
   2  MAAP Server and Client
     2.1  Implementation Details
     2.2  Linux Specific
     2.3  Windows Specific
     2.4  Tests
     2.5  Known Issues and Future Enhancements
   3  API Documentation
   4  Client/Server Protocol
     4.1  Commands
     4.2  Notifications

Introduction
------------

Multicast MAC addresses or locally administered unicast addresses are required
by AVTP for the transmission of media streams. Because AVTP runs directly on a
layer 2 transport, there is no existing protocol to allocate multicast MAC
addresses dynamically. MAAP is designed to provide a way to allocate dynamically
the multicast MAC addresses needed by AVTP. MAAP is not designed to allocate locally
administered unicast addresses.

A block of Multicast MAC addresses has been reserved for the use of AVTP. These
addresses are listed in MAAP Dynamic Allocation Pool below shown. These
addresses are available for dynamic allocation by MAAP: ``91:E0:F0:00:00:00`` –
``91:E0:F0:00:FD:FF``.

When you initialize the MAAP server, it will use the reserved range by default,
but you can override this if your network administration has set aside a
different range for dynamic allocation to streams.

MAAP Server and Client
----------------------

Implementation Details
++++++++++++++++++++++

This implementation provides a binary that runs in two modes, *client* and
*server*. A single server instance manages the MAAP protocol for a single
network interface. The server, which must have sufficient access rights, listens
for raw network frames matching the MAAP destination MAC address. It also opens
a local stream socket and listens for client connections. If the server is not
asked to daemonize itself, it will also listen for commands from the console.

The client opens a socket connection to the server, after which it can send
commands to the server and receive notifications from it. The binary version of
this communication protocol is outlined in the ``maap_iface.h`` file in the
``common`` source directory. Commands and notifications on the console (of
either the server or client binary) operate in the plain text version of the
protocol; the client binary translates these to the binary version before
sending them over the socket to the server.

**Note:** Socket clients may send plain text commands directly to the server,
but currently the server will only send binary notifications back to socket
clients. If you build a client into your application, you should include
``maap_iface.h`` and use the binary protocol.

Linux Specific
++++++++++++++

The MAAP programs can be built in several ways.

First, the top-level makefile of the OpenAvnu repository will build them as part
of the ``daemons_all`` and ``maap`` targets.

Second, the makefile can be used directly from the build directory::

  $ cd daemons/maap/linux/build
  $ make

There is also support for building with ``cmake``, both from the top-level
``CMakeLists.txt`` of the OpenAvnu repository and from the ``CMakeLists.txt`` of
the ``maap`` directory. The ``cmake`` build rules include testing targets for
automatically running unit tests.

Command Line Usage::

    maap_daemon [ -c | -i interface_name [-d log_file] ] [-p port_num]

Command Line Options:

	-c  Run as a client (sends commands to the daemon)
	-i  Run as a server monitoring *interface_name*
	-d  Daemonize the server and log to *log_file*
	-p  Specify the control port to connect to (client) or
	    listen to (server).  The default *port_num* is ``15364``.

When running the ``maap_daemon`` binary, you select the client mode with the
``-c`` flag or server mode with ``-i interface_name``. For either case, the ``-p
port_num`` option will allow changing the client/server communication port. The
client and server must have the same port selected to communicate.

Without the ``-d log_file`` option, the server will stay in the foreground. It
will accept plain text commands from ``stdin`` and write plain text
notifications and log messages to ``stdout``. It will also listen for and accept
socket clients.

With the ``-d log_file`` option, the server will *daemonize*, i.e. it will
disassociate itself from the process environment in which it was launched and
run in the background. Its ``stdin`` and ``stdout`` will go to ``/dev/null`` and
its ``stderr`` will go to the file named by *log_file*. In this mode, it can
only be controlled via socket clients.

With the ``-c`` option, the binary runs in *client* mode. It opens a local
socket on *port_num* to the server process and listens on ``stdin`` for plain
text commands. The commands a translated to the binary protocol and sent to the
server. When the server sends notifications, they are translated from binary to
plain text and they are printed to ``stdout``.

Windows Specific
++++++++++++++++

A Windows build of the ``common`` code and unit tests has been implemented, but
platform-specific client and server code still needs to be written.

The Windows build must be performed with ``cmake``, either from the top-level
``CMakeLists.txt`` of the OpenAvnu repository or from the ``CMakeLists.txt`` of
the ``maap`` directory. The ``cmake`` build rules include testing targets for
automatically running unit tests.

Tests
+++++

The code includes unit tests and an integration test program for testing basic
protocol operation. The unit tests are found in the ``tests`` subdirectory, and
they are integrated into the ``cmake`` builds. Some support code for the unit
tests can be found in the files matching the pattern ``test/maap_*_dummy.*``.

A stress test for the interval tree library is also built and run by the
``cmake`` test rules; the code for this is in ``test/test_intervals.c``.

The integration test program is in the ``test/maap_test.c`` file. This will
listen on an interface using the *pcap* library and run a series of scripted
interactions against a MAAP implementation on the other side of the network
interface. This code is currently Linux-only, and is built by the Linux
makefile.

Known Issues and Future Enhancements
++++++++++++++++++++++++++++++++++++

- The server currently only tracks its own reservations. Tracking the
  ``ANNOUNCE`` messages of other servers on the network would allow us to rule
  out overlapping request ranges without sending ``PROBE`` messages.

- The server always sends binary protocol notifications to socket clients;
  sending plain text notifications when commands are sent in plain text would
  allow using ``telnet`` or ``netcat`` for command line clients instead of the
  ``-c`` flag to the binary.

- The Windows platform-specific code is incomplete and nonfunctional.

API Documentation
-----------------

The ``doc`` directory contains a ``CMakeLists.txt`` file that, when included,
adds the custom target ``doc`` when the build option ``BUILD_DOCUMENTATION`` is
set. It is not currently included from any higher-level ``CMakeLists.txt``.

A manual documentation build can be achieved by executing the following command
from the ``doc`` directory::

  $ doxygen Doxyfile.in

The resulting html documentation will be in the ``build`` subdirectory.

Client/Server Protocol
----------------------

Clients communicate with the server over a binary protocol based on *commands*,
which are sent from the client to the server; and *notifications*, which are
sent from the server to the client. This section gives an overview of how the
commands and responses work; for code-level detail on constructing or
interpreting them, see the ``common/maap_iface.h`` file.

The supplied client and server binaries also support a plain text version of the
protocol for testing at the command line; the command *kinds* will be described
below with their plain text names instead of ther *enum* symbols for the sake of
readability and to provide some reference for command line usage. When an
invalid command is given on the plain text console interface, the following
usage statement will be given::

  init [<range_base> <range_size>] - Initialize the MAAP daemon to recognize
      the specified range of addresses.  If not specified, it uses
      range_base=0x91e0f0000000, range_size=0xfe00.
  reserve [<addr_base>] <addr_size> - Reserve a range of addresses of size
      <addr_size> in the initialized range.  If <addr_base> is specified,
      that address base will be attempted first.
  release <id> - Release the range of addresses with identifier ID
  status <id> - Get the range of addresses associated with identifier ID
  exit - Shutdown the MAAP daemon

Commands
++++++++

The commands accepted by the server are identified by the ``kind`` field of the
``Maap_Cmd`` structure, and the fields ``id``, ``start``, and ``count`` contain
command parameters. The exact meaning of the fields depends on the command, but
``start`` typically identifies a base MAC address, ``count`` determines the size
of the range that begins with the address in ``start``, and ``id`` is used to
reference an existing range allocation.

The following are the command parameter fields:

``kind``
  This field holds the command kind identifier, which are described below.

``start``
  This field holds an unsigned 64-bit integer, which should contain a MAC
  address converted to a native integer with the first-to-transmit bytes of the
  address in the most significant bits of the 48-bit integer and then
  zero-extended to 64-bits; e.g. the default MAAP range uses a ``start``
  parameter of ``0x000091E0F0000000ULL`` for the MAC address
  ``91:E0:F0:00:00:00``. This way of interpreting an address as an integer
  corresponds to the ordering rule that SRP uses to determine contiguous ranges.

``count``
  This field holds an unsigned 32-bit integer, which represents the number of
  addresses in a range. The default MAAP range uses ``0x0000FE00UL`` as the
  count, which means that the range extends from ``91:E0:F0:00:00:00`` to
  ``91:E0:F0:00:FD:FF``.

``id``
  This field holds a signed 32-bit integer, but all current commands expect a
  positive value.

In the plain text interface to the protocol, numbers are parsed via the
``strtol`` family with ``base`` parameter ``16`` for ``start`` and ``0`` for the
others. This means that for ``base``, all digits are interpreted as hexadecimal
digits and the ``"0x"`` prefix is optional. For the other parameters, digits are
parsed as decimal digits unless the ``"0"`` prefix for octal or the ``"0x"``
prefix for hexadecimal are used.

The following are the current set of command kinds:

``init``
  This command must be run before any ranges can be reserved. It accepts
  ``start`` and ``count`` parameters, but default values will be supplied if the
  plain text interface is being used and no parameters are given. When
  initialization is complete, a notification will be sent. Initialization only
  needs to happen once per start of the server.

``reserve``
  This command requests the server to reserve a range of multicast MAC addresses
  from the range specified by the ``init`` command, which must have been
  previously executed. It accepts an optional ``start`` value, which will cause
  it to use that address as the initial request base, and a mandatory ``count``
  parameter for the number of contiguous addresses to reserve. If ``start`` is
  not supplied, a random one will be chosen. When the server receives a valid
  request for reservation, it immediately sends a notification to indicate it is
  querying. That notification and all further notifications about the status of
  the request will include the reservation ``id``, which can be used in other
  commands to identify it.

``release``
  This command requests that the server release the reservation, or to stop
  attempting to acquire it if it has not yet completed the reservation process.
  The only parameter is ``id``, which is the identifier given in response to a
  ``reserve`` command. A notification will be sent by the server when the
  release is completed.

``status``
  This command asks the server to supply the ``start`` and ``count`` values
  associated with a particular reservation. The only parameter is ``id``, which
  is the identifier given in response to a ``reserve`` command. A notification
  with the requested information will be sent by the server if the ``id`` value
  corresponds to an active reservation being managed by the server.

``exit``
  This command asks the server to shut itself down and exit. It takes no
  parameters. Note that this stops the entire server, not just the current
  connection to the server.

Notifications
+++++++++++++

Notifications are sent asynchronously by the server to clients to inform them
about relevant changes to the server's state. The ``kind`` field of the
``Maap_Notify`` structure identifies the kind of notification, the ``result``
field contains a general status code, and the ``id``, ``start``, and ``count``
fields contain extra notification-specific data.

The following are the notification fields:

``kind``
  This field determines the kind of notification; the different kinds are listed
  below.

``id``
  This field contains a signed 32-bit integer; if positive, it represents the
  identifer of a reservation range. There are no current uses for negative
  values in the protocol.

``start``
  This field holds an unsigned 64-bit integer, which should contain a MAC
  address converted to a native integer with the first-to-transmit bytes of the
  address in the most significant bits of the 48-bit integer and then
  zero-extended to 64-bits; e.g. the default MAAP range uses a ``start``
  parameter of ``0x000091E0F0000000ULL`` for the MAC address
  ``91:E0:F0:00:00:00``. This way of interpreting an address as an integer
  corresponds to the ordering rule that SRP uses to determine contiguous ranges.

``count``
  This field holds an unsigned 32-bit integer, which represents the number of
  addresses in a range. The default MAAP range uses ``0x0000FE00UL`` as the
  count, which means that the range extends from ``91:E0:F0:00:00:00`` to
  ``91:E0:F0:00:FD:FF``.

``result``
  This field contains a general result code, which may either indicate no error
  occurred or specify which kind of error occurred. The different result codes
  are listed below after the kinds of notifications.

The following are the kinds of notifications:

``initialized``
  This kind of notification is sent in response to receiving an initialization
  request from a client. The ``start`` and ``count`` fields will contain the
  range which the MAAP server has been initialized to use for reservations. The
  ``result`` field will indicate whether the server had been previously
  initialized before the latest request or not.

``acquiring``
  This kind of notification is sent immediately in response to a ``reserve``
  command. If ``result`` does not indicate an error, then ``start`` and
  ``count`` will indicate the address range that the server is currently
  attempting to reserve. These should *not* be used for streams yet, as a
  conflict may yet be detected that could force a different range selection.
  Most importantly, the ``id`` field will contain the identifier that must be
  used by the client to refer to this reservation in ``release`` or ``status``
  commands.

``acquired``
  This kind of notification is sent to indicate either successful or
  unsuccessful completion of a particular ``reserve`` command that has gone
  through the ``acquiring`` phase. If there is no error indicated in ``result``,
  then ``start`` and ``count`` contain the reserved address range that can now
  be used for streams. The ``id`` field will contain the identifier that must be
  used by the client to refer to this reservation in ``release`` or ``status``
  commands.

``released``
  This kind of notification indicates that the reservation identified in a
  ``release`` command has now been released. The ``start`` and ``count`` fields
  contain the address range that the reservation previously held; they must no
  longer be used for streams. The ``id`` field holds the identifer that was
  associated with this reservation; it is no longer valid as an identifier.

``status``
  This kind of notification describes the reservation identified in a ``status``
  command without changing it. The ``id`` field holds the identifier that was
  given to the ``status`` command. The ``start`` and ``count`` fields describe
  the range of addresses held by the reservation.

``yielded``
  This kind of notification is not sent in response to a command, but when the
  network protocol forced the server to yield a previously-acquired address
  range. The ``id`` field has the same value as when the server sent the
  ``acquiring`` and ``acquired`` notifications for the range, and the actual
  range values are held in ``start`` and ``count``. Any usage of the addresses
  in the yielded range must be immediately stopped. Unless the ``status`` code
  indicates an error, the server will attempt to reserve a new range with the
  same size. This new range (if successfully acquired) will use the same ``id``
  field value.

The following are the result codes:

``none``
  This value indicates that there were no errors associated with this
  notification.

``requires initialization``
  This value indicates that the server has not yet been initialized.

``already initialized``
  This value indicates that the server was already initialized when the latest
  ``init`` command was received.

``reserve not available``
  This value indicates that no block of addresses of the requested size was
  available. A smaller request may be necessary.

``release invalid id``
  This value indicates that a ``release`` command was given with an invalid
  ``id`` field.

``out of memory``
  This value indicates that the request could not be completed because the
  server is out of memory.

``internal``
  This value indicates that an unspecified internal server error occurred.

