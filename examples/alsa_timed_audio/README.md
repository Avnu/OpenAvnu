<h1>
Introduction
</h1>
The Intel Synchronized Audio Toolkit consists of several pieces of sample
code that wrap ALSA timing primitives. It contains building blocks for a time
aware mixer and capture module. It uses the OpenAVB time sync daemon for
the 802.1AS protocol.

There are several Python scripts that use this example code creating a simple
demo that measures distance between two devices

<h1>
Prerequisites
</h1>
- **libsndfile** development package/source
- **Doxygen**
- **pdxlatex** - usually part of **TeX Live** package

<h1>
Building
</h1>
<h2>
Build sample applications
</h2>
Checkout the Open-AVB submodule:
<pre>
git submodule update --init
</pre>
In the support directory run:
<pre>
make
</pre>
This generates the time sync daemon and helper application for the ranging
demo that finds a "chirp" signal given an audio file

In the top level directory run:
<pre>
make
</pre>
<h2>
Build the documentation
</h2>
In the top level directory run:
<pre>
make docs
</pre>
Produces <i>documentation.pdf</i> in the top level directory.
<h1>
Running the demo
</h1>
In the support/RangingDemo directory run:
<pre>
./RangingDemo <-p|-r> -d \<peer IP address\> -i \<local interface name\>
</pre>

One machine should use the -p flag to play audio, the other machine should
use the -r flag, to record audio.

<h1>
Example applications
</h1>
<p>@subpage play_at</p>
<p>@subpage record_at</p>
<p>@subpage sys_to_net_time</p>
<p>@subpage net_to_sys_time</p>
