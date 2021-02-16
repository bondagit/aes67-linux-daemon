# AES67 Linux Daemon 

AES67 Linux Daemon is a Linux implementation of AES67 interoperability standard used to distribute and synchronize real time audio over Ethernet.    
See [https://en.wikipedia.org/wiki/AES67](https://en.wikipedia.org/wiki/AES67) for additional info.    

# Introduction

The daemon is a Linux process that uses the [Merging Technologies ALSA RAVENNA/AES67 Driver](https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm/src/master) to handle PTP synchronization and RTP streams and exposes a REST interface for configuration and status monitoring.     

The **ALSA AES67 Driver** implements a virtual ALSA audio device that can be configured using _Sources_ and _Sinks_ and it's clocked using the PTP clock.    
A _Source_ reads audio samples from the ALSA playback device and sends RTP packets to a configured multicast address.    
A _Sink_ receives RTP packets from a specific multicast address and writes them to the ALSA capture device.    

A user can use the ALSA capture device to receive synchronized incoming audio samples from an RTP stream and the ALSA playback device to send synchronized audio samples to an RTP stream.    
The binding between a _Source_ and the ALSA playback device is determined by the channels used during the playback and the configured _Source_ channels map. The binding between a _Sink_ and the ALSA capture device is determined by the channels used while recoding and the configured _Sink_ channels map.    

The driver handles the PTP and RTP packets processing and acts as a PTP clock slave to synchronize with a master clock on the specified PTP domain.  All the configured _Sources_ and _Sinks_ are synchronized using the same PTP clock.    

The daemon communicates with the driver for control, configuration and status monitoring only by using _netlink_ sockets.    
The daemon implements a REST interface to configure and monitor the _Sources_, the _Sinks_ and PTP slave. See [README](daemon/README.md) for additional info.
It also implements SAP sources discovery and advertisement compatible with AES67 standard and mDNS sources discovery and advertisement compatible with Ravenna standard.    

A WebUI is provided to allow daemon and driver configuration and monitoring. The WebUI uses the daemon REST API and exposes all the supported configuration paramaters for the daemon, the PTP slave clock, the _Sources_ and the _Sinks_. The WebUI can also be used to monitor the PTP slave status and the _Sinks_ status and to browse the remote SAP and mDNS sources.

## License ##

AES67 daemon and the WebUI are licensed under [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.en.html).

The daemon uses the following open source:

* **Merging Technologies ALSA RAVENNA/AES67 Driver** licensed under [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.en.html).
* **cpp-httplib** licensed under the [MIT License](https://github.com/yhirose/cpp-httplib/blob/master/LICENSE)
* **Avahi common & client libraries** licensed under the [LGPL License](https://github.com/lathiat/avahi/blob/master/LICENSE)
* **Boost libraries** licensed under the [Boost Software License](https://www.boost.org/LICENSE_1_0.txt)

## Devices and interoperability tests ##
See [Devices and interoperability tests with the AES67 daemon](DEVICES.md)

## AES67 USB Receiver and Transmitter ##
See [Use your board as AES67 USB Receiver and Transmitter](USB_GADGET.md)

## Repository content ##

### [daemon](daemon) directory ###

This directory contains the AES67 daemon source code.     
The daemon can be cross-compiled for multiple platforms and implements the following functionalities:

* communication and configuration of the ALSA RAVENNA/AES67 device driver
* control and configuration of up to 64 sources and sinks using the ALSA RAVENNA/AES67 driver via netlink
* session handling and SDP parsing and creation
* HTTP REST API for the daemon control and configuration
* SAP sources discovery and advertisement compatible with AES67 standard
* mDNS sources discovery and advertisement (using Linux Avahi) compatible with Ravenna standard
* RTSP client and server to retrieve, return and update SDP files via DESCRIBE and ANNOUNCE methods according to Ravenna standard
* IGMP handling for SAP, PTP and RTP sessions

The directory also contains the daemon regression tests in the [tests](daemon/tests) subdirectory.  
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

### [test](test) directory ###

This directory contains the files used to run the daemon platform compatibility test on the network loopback device. The [test](#test) is described below.

## Prerequisite ##
<a name="prerequisite"></a>
The daemon and the test have been tested with **Ubuntu 18.04** distro on **ARMv7** and with **Ubuntu 18.04, 19.10 and 20.04** distros on **x86** using:

* Linux kernel version >= 4.10.x
* GCC  version >= 7.x / clang >= 6.x (C++17 support required)
* cmake version >= 3.7
* node version >= 8.10.0
* npm version >= 3.5.2
* boost libraries version >= 1.65
* Avahi service discovery (if enabled) >= 0.7

The following ARM platform have been used for testing:

The BeagleBone® Black board with ARM Cortex-A8 32-Bit processor.
See [Ubuntu 18.04 on BeagleBone® Black](https://elinux.org/BeagleBoardUbuntu) for additional information about how to setup Ubuntu on this board.

The NanoPi NEO2 with Allwinner H5, Quad-core 64-bit high-performance Cortex A53 processor.
See [Armbian NanoPi NEO2 ](https://www.armbian.com/nanopi-neo-2/) for additional information about how to setup Ubuntu on this board.

The [ubuntu-packages.sh](ubuntu-packages.sh) script can be used to install all the packages required to compile and run the AES67 daemon, and the [platform compatibility test](#test).
**_Important_** _PulseAudio_ must be disabled or uninstalled for the daemon to work properly, see [PulseAudio and scripts notes](#notes).
 
## How to build ##
Make sure you have all the required packages installed, see [prerequisite](#prerequisite).    
To compile the AES67 daemon and the WebUI you can use the [build.sh](build.sh) script, see [script notes](#notes).        
The script performs the following operations:    

* checkout, patch and build the Merging Technologies ALSA RAVENNA/AES67 module
* checkout the cpp-httplib
* build and deploy the WebUI
* build the AES67 daemon

## Run the regression tests ##
To run daemon regression tests install the ALSA RAVENNA/AES67 kernel module with:    

      sudo insmod 3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko

setup the kernel parameters with:

      sudo sysctl -w net/ipv4/igmp_max_memberships=66

make sure that no instances of the aes67-daemon are running, enter the [tests](daemon/tests) subdirectory and run:

      ./daemon-test -p

**_NOTE:_** when running regression tests make sure that no other Ravenna mDNS sources are advertised on the network because this will affect the results. Regression tests run on loopback interface but Avahi ignores the interface parameter set and will forward to the daemon the sources found on all network interfaces.

## Run the platform compatibility test ##
<a name="test"></a>
Before attempting to use the AES67 daemon on a specific host or board it's highly recommended to run the platform test.
These tests can be executed using the [run\_test.sh](run_test.sh) script. See [script notes](#notes).

The script allows a user to test a specific configuration and it can be used to ensure that the daemon will be able to operate smoothly with such config on the platform.

      Usage run_test.sh sample_format sample_rate channels duration
           sample_format can be one of S16_LE, S24_3LE, S32_LE
           sample_rate can be one of 44100, 48000, 96000
           channels can be one of 1, 2, 4
           duration is in the range 1 to 10 minutes

For example to test the typical AES67 configuration run:

      ./run_test.sh S24_3LE 48000 2 5 

The test performs the following operations:

* check that all the required executables are available
* stop the running daemon instances and remove the ALSA RAVENNA/AES67 module
* compile the test tools under the test folder
* validatesthe input parametersi, prepare the raw input file to be played *./test/test.raw* and the configuration files under the test folder
* stop PulseAudio (if installed). This opens and keeps busy the ALSA playback and capture devices causing problems. See [PulseAudio](#notes).
* install the ALSA RAVENNA/AES67 module
* start the ptp4l as master clock on the network loopback device
* start the AES67 daemon and create a source and a sink according to the status file created in the test directory
* wait for the Ravenna driver PTP slave to synchronize
* start recording on the configured ALSA sink for the specified period tof time to the raw file *./test/sink_test.raw*
* start playing the test file created *./test/test.raw* on the configured ALSA source
* wait for the recording and the playback to complete 
* check that the recorded file contains the expected audio samples sequence
* terminate ptp4l and the AES67 daemon
* print the test result that can be either *OK" or *failed at {location}*

If the test result is OK it means that the selected configuration can possibly run smoothly on your platform.

If the test reports a failure you may try to stop all the possible additional loads running on the host and repeat it.
If after this the test fails systematically it means you cannot achieve a good reliability with the specified configuration.
In this case you may try to configure a difference dirver timer basic tick period in the daemon configuration file (parameter *tic\_frame\_size\_at\_1fs* in *test/daemon.conf*).

By default this parameter is set to 48 (1ms latency) and the valid range is from 48 to 480 with steps of 48. Higher values of this parameter lead to higher packets processing latency and this breaks the compatibility with certain devices.

## Notes ##
<a name="notes"></a>

* All the scripts in this repository are provided as a reference to help setting up the system and run the platform compatibility test.
  They have been tested on **Ubuntu 18.04 19.10 20.04** distros.
* **PulseAudio** can create instability problems.    
Before running the daemon verify that PulseAudio is not running with:  

      ps ax | grep pulseaudio

  In case it's running try to execute the following script to stop it:

      daemon/scripts/disable_pulseaudio.sh

  If after this the process is still alive consider one of these two solutions and reboot the system afterwards:    
  * Uninstall it completely with:    

        sudo apt-get remove pulseaudio

  * Disable it by renaming the executable with:    
    
         sudo mv /usr/bin/pulseaudio /usr/bin/_pulseaudio

  Other methods to disable PulseAudio may fail and just killing it is not enough since it gets immediately re-spawned.
