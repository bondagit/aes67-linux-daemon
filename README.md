# AES67 Linux Daemon

AES67 Linux Daemon is a Linux implementation of AES67 interoperability standard used to distribute and synchronize real time audio over Ethernet.
See [https://en.wikipedia.org/wiki/AES67](https://en.wikipedia.org/wiki/AES67) for additional info.

# Status

![Daemon tests status](https://github.com/bondagit/aes67-linux-daemon/actions/workflows/daemon-tests.yml/badge.svg?branch=master)

![WebUI release status](https://github.com/bondagit/aes67-linux-daemon/actions/workflows/release.yml/badge.svg)

# Introduction

The daemon is a Linux process that uses the [Merging Technologies ALSA RAVENNA/AES67 Driver](https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm/src/master) to handle PTP synchronization and RTP streams and exposes a REST interface for configuration and status monitoring.

The **ALSA AES67 Driver** implements a virtual ALSA audio device that can be configured using _Sources_ and _Sinks_ and it's clocked using the PTP clock.
A _Source_ reads audio samples from the ALSA playback device and sends RTP packets to a configured multicast or unicast address.
A _Sink_ receives RTP packets from a specific multicast or unicast address and writes them to the ALSA capture device.

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
* control and configuration of up to 64 multicast and unicast sources and sinks using the ALSA RAVENNA/AES67 driver via netlink
* session handling and SDP parsing and creation
* HTTP REST API for the daemon control and configuration
* SAP sources discovery, update and advertisement compatible with AES67 standard
* mDNS sources discovery and advertisement (using Linux Avahi) compatible with Ravenna standard
* RTSP client and server to retrieve, return and update SDP files via DESCRIBE and ANNOUNCE methods according to Ravenna standard
* IGMP handling for SAP, PTP and RTP sessions
* Integration with systemd watchdog monitoring (from daemon release v1.6)

See the [README](daemon/README.md) file in this directory for additional information about the AES67 daemon configuration and the daemon HTTP REST API.

The directory also contains the daemon tests in the [tests](daemon/tests) subdirectory.

Daemon tests can be executed via a Docker container by using a fake version of the daemon driver manager.
This was implemented to perfom automated execution of the tests via a GitHub workflow.

To build the Docker image for the daemon regression tests run:

      docker build --progress=plain -f ./Dockerfile.daemon_tests -t aes67-daemon-tests  .

To run the tests:
      
      docker run aes67-daemon-tests
      

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
The [aes67-daemon branch of ravenna-alsa-lkm repository](https://github.com/bondagit/ravenna-alsa-lkm/tree/aes67-daemon) contains the ALSA RAVENNA/AES67 module code plus all the patches applied to it.

 The ALSA RAVENNA/AES67 kernel module is responsible for:

* registering as an ALSA driver
* generating and receiving RTP audio packets
* PTP slave operations and PTP driven interrupt loop
* netlink communication between user and kernel

 The following patches have been applied to the original module:

* patch to make the PTP slave status change from locked to unlocked if no announcement messages are received from the Master for more than 5 seconds (from driver version v1.6). See [issue 87](https://github.com/bondagit/aes67-linux-daemon/issues/87).
* patch to fix issue causing the usage of wrong Sources and Sinks buffer offsets in case of reconfiguration or daemon restart (from driver version v1.5). See [issue 55](https://github.com/bondagit/aes67-linux-daemon/issues/55).
* patch to enable the configuration of smaller ALSA buffer sizes to reduce end-to-end latency up to 8ms (from driver version v1.4). See [platform latency test](#latency) and See [issue 53](https://github.com/bondagit/aes67-linux-daemon/issues/53)
* patch to remove unsupported non-interleved access from driver capabilities. This enables compatibility with JACK audio (from driver version v1.3)
* patch to enable the usage of a PTP master clock on the daemon host (from driver version v1.1)
* patch to rework the driver PCM interface and to simplify and unify handling of read-write interlaved and memory mapped access modes
* patch to remove user space transfer handling from convert functions
* patch to add support for AM824 (L32) codec
* patch to initialize playback and capture buffers
* patch to enable mono channel support to playback and capture ALSA devices
* patch to enable direct PCM transfer mode instead of indirect to enable the use of ALSA plugins
* patch to enable independent playback and capture interrupt startup
* patch to disable UDP checksum check for PTP packets
* patch to enable support for ARM 32bit and 64bit platforms
* patch to enable the network loopback device
* patch to compile with Linux Kernel v5

See [ALSA RAVENNA/AES67 Driver README](https://github.com/bondagit/aes67-linux-daemon/blob/master/README.md) for additional information about the Merging Technologies module and for proper Linux Kernel configuration and tuning.

### [systemd](systemd) directory ###

This directory contains systemd configuration files for the daemon.

The daemon integrates with systemd watchdog.

To enable it recompile the daemon with the CMake option _-DWITH_SYSTEMD=ON_

You can install the daemon under _systemd_ by using the script [systemd/install.sh](install.sh):

    cd systemd
    sudo ./install.sh

Before starting the daemon edit _/etc/daemon.conf_ and make sure the _interface_name_ parameter is set to your ethernet interface.

To start the daemon use:

     sudo systemctl start aes67-daemon

To stop it use:

     sudo systemctl stop aes67-daemon


The daemon requires the _MergingRavennaALSA_ module to run.

You can usally install the module using the following commands:

    cd 3rdparty/ravenna-alsa-lkm/driver
    sudo make modules_install

If this doesn't work because you miss kernel certificate follow the instructions at: 
[No OpenSSL sign-file signing_key.pem](https://superuser.com/questions/1214116/no-openssl-sign-file-signing-key-pem-leads-to-error-while-loading-kernel-modules)

Finally use the command to load the modules:

    sudo depmod -a


### [test](test) directory ###

This directory contains the files used to run the daemon platform compatibility test on the network loopback interface. The [test](#test) is described below.

## Prerequisite ##
<a name="prerequisite"></a>
The daemon and the test have been verified starting from **Ubuntu 18.04** distro onwards for **ARMv7** and **x86** platforms using:

* Linux kernel version >= 4.10.x
* GCC  version >= 7.x / clang >= 6.x (C++17 support required)
* cmake version >= 3.7
* boost libraries version >= 1.65
* Avahi service discovery (if enabled) >= 0.7

The following platforms have been used for testing:

A NanoPi NEO2 with Allwinner H5 Quad-core 64-bit Cortex A53 processor.
See [Armbian NanoPi NEO2 ](https://www.armbian.com/nanopi-neo-2/) for additional information about how to setup Ubuntu on this board.

A Mini PC N40 with Intel® Celeron® Processor N4020 , 2 Cores/2 Threads (4M Cache, up to 2.80 GHz).
See [Minisforum N40 Mini PC](https://store.minisforum.com/products/minisforum-n40-mini-pc) and [how to Install Ubuntu on a fanless Mini PC](https://www.youtube.com/watch?v=2djTPJ02xK0).

The [ubuntu-packages.sh](ubuntu-packages.sh) script can be used to install all the packages required to compile and run the AES67 daemon, and the [platform compatibility test](#test).

**_Important_** CPU scaling events could affect daemon streams causing unexpected distortions, see [CPU scaling events and scripts notes](#notes).

**_Important_** Starting from Linux kernel 5.10.x onwards a change in a kernel parameter is required to fix a problem with round robin scheduler causing the latency test to fail, see [Real Time Scheduler Throttling](#notes).

**_Important_** _PulseAudio_ must be disabled or uninstalled for the daemon to work properly, see [PulseAudio and scripts notes](#notes).

## How to build ##
Make sure you have all the required packages installed, see [prerequisite](#prerequisite).
To compile the AES67 daemon and the WebUI you can use the [build.sh](build.sh) script, see [script notes](#notes).
The script performs the following operations:

* checkout, patch and build the Merging Technologies ALSA RAVENNA/AES67 module
* checkout the cpp-httplib
* build and deploy the WebUI
* build the AES67 daemon

## Run the platform compatibility test ##
<a name="test"></a>
Before attempting to use the AES67 daemon on a specific host or board it's highly recommended to run the platform test.
The platform test runs a playback and recording of an RTP session on the loopback network device using the ALSA RAVENNA/AES67 modules and checks that no audio samples get corrupted or lost.
This test can be executed using the [run\_test.sh](run_test.sh) script. See [script notes](#notes).

The script allows a user to test a specific configuration and it can be used to ensure that the daemon will be able to operate smoothly with such config on the current platform.

      Usage run_test.sh sample_format sample_rate channels duration
           sample_format can be one of S16_LE, S24_3LE, S32_LE
           sample_rate can be one of 44100, 48000, 96000, 192000, 384000
           channels can be one of 2, 4, 6, up to 64
           duration is in the range 1 to 10 minutes

For example to test the typical AES67 configuration run:

      ./run_test.sh S24_3LE 48000 2 5

The test performs the following operations:

* check that all the required executables are available
* stop the running daemon instances and remove the ALSA RAVENNA/AES67 module
* compile the test tools under the test folder
* validates the input parameters, prepare the raw input file to be played *./test/test.raw* and the configuration files under the test folder
* stop PulseAudio (if installed). This opens and keeps busy the ALSA playback and capture devices causing problems. See [PulseAudio](#notes).
* install the ALSA RAVENNA/AES67 module
* start the ptp4l as master clock on the network loopback interface
* start the AES67 daemon and create a source and a sink according to the status file created in the test directory
* wait for the Ravenna driver PTP slave to synchronize
* start recording on the configured ALSA sink for the specified period tof time to the raw file *./test/sink_test.raw*
* start playing the test file created *./test/test.raw* on the configured ALSA source
* wait for the recording and the playback to complete
* check that the recorded file contains the expected audio samples sequence
* terminate ptp4l and the AES67 daemon
* print the test result that can be either *OK* or *failed at (location)*


If the test result is OK it means that the selected configuration can run smoothly on your platform.

A 64 channels configuration was succesfully tested on the _Mini PC_ with Intel Celeron N4020 processor.

A 24 channels configuration was succesfully tested on the _NanoPi NEO2 board_.

If the test reports a failure you may try to stop all the possible additional loads running on the host and repeat it.
If after this the test fails systematically it means you cannot achieve a good reliability with the specified configuration.
In this case you may try to configure a different driver timer basic tick period in the daemon configuration file (parameter *tic\_frame\_size\_at\_1fs* in *test/daemon.conf*).
By default this parameter is set to 48 (1ms latency) and the valid range is from 48 to 480 with steps of 48.
Note that higher values of this parameter (values above 48) lead to higher packets processing latency and this breaks the compatibility with certain devices.

## Run the platform latency test ##
<a name="latency"></a>
The platform latency test can be used to measure the minimum end-to-end latency that can be achieved on a specific platform.

The test is based on the same setup used for the platform compatibility test where a _Sink_ is configured to receive audio samples from a _Source_
 both running on the same device using the network loopback interface.
 
A test application plays audio samples on the RAVENNA playback device and measures the time till the samples are received on the RAVENNA capture device.

The test setup and the end-to-end latency measured are shown in the picture below:

![image](https://user-images.githubusercontent.com/56439183/144990290-c5c94697-b631-4af0-981a-dfea5fc21e9a.png)

The test can be executed using the [run\_latency\_test.sh](run_latency_test.sh) script. See [script notes](#notes).

The script allows a user to test the latency on a specific configuration and it can be used to ensure that a specific ALSA buffer size can be used:

      Usage run_latency_test.sh sample_format sample_rate channels duration frames
           sample_format can be one of S16_LE, S24_3LE, S32_LE
           sample_rate can be one of 44100, 48000, 96000, 192000, 384000
           channels can be one of 2, 4, 6, up to 64
           duration of the test in seconds
           frames buffer size in frames

The specified buffer size in frames starts from _tic_frame_size_at_1fs_ * 2 (128 by default) in steps of _tic_frame_size_at_1fs_.

For example, to test the typical AES67 configuration for 1 minute and a buffer size of 128 frames run:

      ./run_latency_test.sh S24_3LE 48000 2 60 128
      
If no underrun errors occurred during the test the requested buffer size can be used and the end-to-end latency measured is printed at the end:

       Trying latency 128 frames, 2666.667us, 2.666667ms (375.0000Hz)
       Success
       Playback:
       *** frames = 2880128 ***
         state       : RUNNING
         trigger_time: 157745.455411
         tstamp      : 0.000000
         delay       : 128
         avail       : 0
         avail_max   : 64
       Capture:
       *** frames = 2880000 ***
         state       : RUNNING
         trigger_time: 157745.455415
         tstamp      : 0.000000
         delay       : 0
         avail       : 0
         avail_max   : 64
       Maximum read: 64 frames
       Maximum read latency: 1333.333us, 1.333333ms (750.0000Hz)
       Playback time = 157745.455411, Record time = 157745.455415, diff = -4
       End to end latency: 7.997 msecs
       Terminating processes ...
       daemon exiting with code: 0

The previous test was run on a _NanoPi NEO2 board_ with Ubuntu distro.

A 64 channels configuration was succesfully tested on the _Mini PC_ with Intel Celeron N4020 processor.

A 24 channels configuration was succesfully tested on the _NanoPi NEO2 board_.

In case underrun happened the status reported is:

         state       : XRUN
      
and the specified buffer size cannot be used.

**_Important_** Starting from Linux kernel 5.10.x onwards a change in a kernel parameter is required to fix a problem with round robin scheduler causing the latency test to fail, see [Real Time Scheduler Throttling](#notes).

## Run the daemon tests ##
To run daemon tests install the ALSA RAVENNA/AES67 kernel module with:

      sudo insmod 3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko

setup the kernel parameters with:

      sudo sysctl -w net/ipv4/igmp_max_memberships=66

make sure that no instances of the aes67-daemon are running, enter the [tests](daemon/tests) subdirectory and run:

      ./daemon-test -p

## Notes ##
<a name="notes"></a>

* All the scripts in this repository are provided as a reference to help setting up the system and run the platform compatibility test.
  They have been tested starting from **Ubuntu 18.04** distros onwards.
* Starting from Linux kernel 5.10.x onwards a change in a kernel parameter is required to fix a problem with round robin scheduler causing the latency test to fail, see [issue 79](https://github.com/bondagit/aes67-linux-daemon/issues/96) and alsa-lib [issue-285](https://github.com/alsa-project/alsa-lib/issues/285).

  Set the total bandwidth available to all real-time tasks to 100% of the CPU with:

      sysctl -w kernel.sched_rt_runtime_us=1000000

  By default this value is set to 95%.

* CPU scaling events could have an impact on daemon streams causing unexpected distortion for a few seconds, see [issue 96](https://github.com/bondagit/aes67-linux-daemon/issues/96).
Before running the daemon make sure you disable CPU scaling events:

  Check if CPU scaling is enabled with:

      cat /proc/sys/kernel/perf_cpu_time_max_percent

  If result is not 0, (it's usually set to 25) set it to 0 with:

      sudo sysctl -w kernel.perf_cpu_time_max_percent=0

  You may also want to review the current CPU scaling governor with (cpu0 in this case):

      cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

  See https://www.kernel.org/doc/Documentation/cpu-freq/governors.txt for dditional info.
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
