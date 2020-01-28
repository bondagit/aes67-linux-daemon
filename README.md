# AES67 Linux Daemon 

## License ##

AES67 daemon and the WebUI are licensed under [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.en.html).

The daemon uses the following open source:

* **Merging Technologies ALSA RAVENNA/AES67 Driver** licensed under [GNU GPL](https://www.gnu.org/licenses/gpl-3.0.en.html).
* **cpp-httplib** licensed under the [MIT License](https://github.com/yhirose/cpp-httplib/blob/master/LICENSE)

## Repository content ##

### [daemon](daemon) directory ###

This directory contains the AES67 daemon source code.     
The daemon can be cross-compiled for multiple platforms and implements the following functionalities:

* communication and configuration with the ALSA RAVENNA/AES67 driver via netlink  
* HTTP REST API for control and configuration
* session handling and SDP parsing and creation
* SAP discovery protocol implementation
* IGMP handling for SAP and RTP sessions

The directory also contains the daemon regression tests in the [tests](daemon/tests) subdirectory.  To run daemon tests install the ALSA RAVENNA/AES67 kernel module enter the [tests](daemon/tests) subdirectory and run *./daemon-test -l all*    

See the [README](daemon/README.md) file in this directory for additional information about the AES67 daemon configuration and the HTTP REST API.

### [webui](webui) directory ###

This directory contains the AES67 daemon WebUI configuration implemented using React.    
With the WebUI a user can do the following operations:

* change the daemon configuration, this causes a daemon restart
* edit PTP clock slave configuration and monitor PTP slave status
* add and edit RTP Sources
* add, edit and monitor RTP Sinks

### [3rdparty](3rdparty) directory ###

This directory used to download the 3rdparty open source used by the daemon.    
The **patches** subdirectory contains patches applied to the ALSA RAVENNA/AES67 module to compile with the Linux Kernel 5.x and to enable operations on the network loopback device (for testing purposes).

 The ALSA RAVENNA/AES67 kernel module is responsible for:

* registering as an ALSA driver
* generating and receiving RTP audio packets
* PTP slave operations and PTP driven interrupt loop
* netlink communication between user and kernel

See [ALSA RAVENNA/AES67 Driver README](https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm/src/master/README.md) for additional information about the Merging Technologies module and for proper Linux Kernel configuration and tuning.

### [demo](demo) directory ###

This directory contains a the daemon configuration and status files used to run a short demo on the network loopback device. The [demo](#demo) is described below.

## Prerequisite ##
The daemon has been tested on **Ubuntu 18.04** and **19.10** using:

* Linux kernel version >= 5.0.0 
* GCC  version >= 7.4 / clang >= 6.0.0 (C++17 support required)
* cmake version >= 3.10.2
* node version >= 8.10.0
* mpm version >= 3.5.2
* boost libraries version >= 1.65.1

The [ubuntu-packages.sh](ubuntu-packages.sh) script can be used to install all the packages required to compile and run the AES67 daemon, the daemon tests and the [demo](#demo). See [script notes](#notes).
 
## How to build ##
To compile the AES67 daemon and the WebUI use the [build.sh](build.sh) script. See [script notes](#notes).    
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
* install the ALSA RAVENNA/AES67 module
* start the ptp4l as master clock on the network loopback device
* start the AES67 daemon and creates a source and a sink according to the status file in the demo directory
* open a browser on the daemon PTP status page
* wait for the Ravenna driver PTP slave to synchronize
* start recording on the configured ALSA sink for 60 seconds to the wave file in *./demo/sink_test.wav*
* start playing a test sound on the configured ALSA source
* wait for the recording to complete and terminate ptp4l and the AES67 daemon

## Notes ##
<a name="notes"></a>
* All the scripts in this repository are provided as a reference to help setting up the system and run a simple demo.    
  They have been tested on **Ubuntu 18.04** and **19.10** distros only.

