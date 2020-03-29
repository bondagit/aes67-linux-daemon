# AES67 Linux Daemon 

AES67 Linux Daemon is a Linux implementation of AES67 interoperability standard used to distribute and synchronize real time audio over Ethernet.
See https://en.wikipedia.org/wiki/AES67 for additional info.

## License ##

AES67 daemon and the WebUI are licensed under [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.en.html).

The daemon uses the following open source:

* **Merging Technologies ALSA RAVENNA/AES67 Driver** licensed under [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.en.html).
* **cpp-httplib** licensed under the [MIT License](https://github.com/yhirose/cpp-httplib/blob/master/LICENSE)
* **Avahi common & client libraries** licensed under the [LGPL License](https://github.com/lathiat/avahi/blob/master/LICENSE)
* **Boost libraries** licensed under the [Boost Software License](https://www.boost.org/LICENSE_1_0.txt)

## Repository content ##

### [daemon](daemon) directory ###

This directory contains the AES67 daemon source code.     
The daemon can be cross-compiled for multiple platforms and implements the following functionalities:

* control and configuration of up to 64 sources and sinks using the ALSA RAVENNA/AES67 driver via netlink
* session handling and SDP parsing and creation
* HTTP REST API for control and configuration
* SAP discovery protocol and SAP browser
* mDNS sources discovery (using Avahi) and SDP transfer via RTSP
* IGMP handling for SAP, RTP and PTP multicast traffic

The directory also contains the daemon regression tests in the [tests](daemon/tests) subdirectory.  To run daemon tests install the ALSA RAVENNA/AES67 kernel module enter the [tests](daemon/tests) subdirectory and run *./daemon-test -l all*    

See the [README](daemon/README.md) file in this directory for additional information about the AES67 daemon configuration and the HTTP REST API.

### [webui](webui) directory ###

This directory contains the AES67 daemon WebUI configuration implemented using React.    
With the WebUI a user can do the following operations:

* change the daemon configuration, this causes a daemon restart
* edit PTP clock slave configuration and monitor PTP slave status
* add and edit RTP Sources
* add, edit and monitor RTP Sinks
* browser remote SAP and mDNS RTP sources

### [3rdparty](3rdparty) directory ###

This directory is used to download the 3rdparty open source.
The [patches](3rdparty/patches) subdirectory contains patches applied to the ALSA RAVENNA/AES67 module to compile with the Linux Kernel 5.x and on ARMv7 platforms and to enable operations on the network loopback device (for testing purposes).

 The ALSA RAVENNA/AES67 kernel module is responsible for:

* registering as an ALSA driver
* generating and receiving RTP audio packets
* PTP slave operations and PTP driven interrupt loop
* netlink communication between user and kernel

See [ALSA RAVENNA/AES67 Driver README](https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm/src/master/README.md) for additional information about the Merging Technologies module and for proper Linux Kernel configuration and tuning.

### [demo](demo) directory ###

This directory contains a the daemon configuration and status files used to run a short demo on the network loopback device. The [demo](#demo) is described below.

## Prerequisite ##
<a name="prerequisite"></a>
The daemon and the demo have been tested with **Ubuntu 18.04** distro on **x86/ARMv7** and with **Ubuntu 19.10** distro on **x86** using:

* Linux kernel version >= 4.14.x
* GCC  version >= 7.4 / clang >= 6.0.0 (C++17 support required, clang is required to compile on ARMv7)
* cmake version >= 3.10.2
* node version >= 8.10.0
* npm version >= 3.5.2
* boost libraries version >= 1.65.1
* Avahi service discovery (if enabled) >= 0.7

The BeagleBone® Black board with ARM Cortex-A8 32-Bit processor was used for testing on ARMv7.
See [Ubuntu 18.04 on BeagleBone® Black](https://elinux.org/BeagleBoardUbuntu) for additional information about how to setup Ubuntu on this board.

The [ubuntu-packages.sh](ubuntu-packages.sh) script can be used to install all the packages required to compile and run the AES67 daemon, the daemon tests and the [demo](#demo). See [PulseAudio and scripts notes](#notes).
 
## How to build ##
Make sure you have all the required packages installed, see [prerequisite](#prerequisite).    
To compile the AES67 daemon and the WebUI you can use the [build.sh](build.sh) script, see [script notes](#notes).        
The script performs the following operations:    

* checkout, patch and build the Merging Technologies ALSA RAVENNA/AES67 module
* checkout the cpp-httplib
* build and deploy the WebUI
* build the AES67 daemon

## Run the Demo ##
<a name="demo"></a>
To run a simple demo use the [run\_demo.sh](run_demo.sh) script. See [script notes](#notes).

The demo performs the following operations:

* setup system parameters
* stop PulseAudio (if installed). This uses and keeps busy the ALSA playback and capture devices causing instability problems. See [PulseAudio](#notes).
* install the ALSA RAVENNA/AES67 module
* start the ptp4l as master clock on the network loopback device
* start the AES67 daemon and creates a source and a sink according to the status file in the demo directory
* open a browser on the daemon PTP status page
* wait for the Ravenna driver PTP slave to synchronize
* start recording on the configured ALSA sink for 60 seconds to the wave file in *./demo/sink_test.wav*
* start playing a test sound on the configured ALSA source
* wait for the recording to complete and terminate ptp4l and the AES67 daemon

## Interoperability tests ##
To run interoperability tests using the [Hasseb audio over Ethernet receiver](http://hasseb.fi/shop2/index.php?route=product/product&product_id=62) follow these steps:

* open the daemon configuration file *daemon.conf* and change the following parameters:
  * set network interface name to your Ethernet card, e.g.: *"interface\_name": "eth0"*
  * set default sample rate to 48Khz: *"sample\_rate": 48000*
* verify that PulseAdio is not running. See [PulseAudio](#notes).
* install the ALSA RAVENNA/AES67 module with:     
    *sudo insmod 3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko*
* run the daemon using the new configuration file:     
    *aes67-daemon -c daemon.conf*
* open the Daemon WebUi *http://[address:8080]* and do the following:
  * go to Config tab and verify that the sample rate is set to 48KHz
  * go to Sources tab and add a new Source using the plus button, set Codec to L24 and press the Submit button
  * go to Sources tab and click on the info icon on the right of the newly created source and copy the SDP file
* open the Hasseb WebUI and do the following:
  * deselect the "PTP slave only" checkbox to enable PTP master on Hasseb device
  * select the "Add SDP file manually" checkbox and copy the previous Source SDP into the SDP field
  * press the Submit button
* return to the daemon WebUI, click on the PTP tab and wait for the "PTP Status" to report "locked"
* open a shell on the Linux host and start the playback on the ravenna ALSA device. For example to playback a test sound use: *speaker-test -D plughw:RAVENNA -r 48000 -c 2 -t sine*

## Notes ##
<a name="notes"></a>

* All the scripts in this repository are provided as a reference to help setting up the system and run a simple demo.    
  They have been tested on **Ubuntu 18.04** and **19.10** distros only.    
* PulseAudio can create instability problems.    
Before running the daemon verify that PulseAudio is not running with  *ps ax | grep pulseaudio*     
In case it's running try to execute the script *daemon/scripts/disable_pulseaudio.sh* to stop it. If after this the process is still alive consider renaming the executable with *sudo mv /usr/bin/pulseaudio /usr/bin/_pulseaudio* and reboot the system.

