# Devices and interoperability tests #

This document describes the interoperability tests carried out using the AES67 daemon and provides a guide to execute them.

Before starting make sure that the [AES67 daemon basic setup](#daemon_setup) is done.

The following devices have been tested:

* Audinate AVIOUSB Dante receiver, see [Dante receivers](#dante_avio_receiver)
* Audinate AVIOUSB and AVIOAI2 Dante trasmitters, see [Dante transmitters](#dante_avio_transmitter)
* [Hasseb audio over Ethernet receiver](#hasseb_receiver)


## AES67 daemon basic setup ##
<a name="daemon_setup"></a>
Before running any interoperability test configure the AES67 daemon with the following instructions:

* open the daemon configuration file *daemon.conf* and change the following parameters:
  * set network interface name to your Ethernet card: *"interface\_name": "eth0"*
  * set default sample rate to 48Khz: *"sample\_rate": 48000*
  * set TIC frame size @1FS (samples) to 48 samples: *"tic\_frame\_size\_at\_1fs": 48*
* verify that PulseAdio is not running. See [PulseAudio](README.md#notes).
* install the ALSA RAVENNA/AES67 module with:     

      sudo insmod 3rdparty/ravenna-alsa-lkm/driver/MergingRavennaALSA.ko

* run the daemon using the new configuration file:     
         
      aes67-daemon -c daemon.conf

* open the Daemon WebUI *http://[address:8080]* and do the following:
  * go to Config tab and verify that the sample rate is set to 48KHz and the TIC frame size @1FS is set to 48 samples
  * go to Sources tab and add a new Source using the plus button
    * set Codec to L24 
    * set Max samples per packet to 48 samples
    * press the Submit button

## Dante receivers ##
<a name="dante_avio_receiver"></a>
To run interoperability tests with a Dante receiver follow these steps. Tests were done using a Dante AVIOUBS device.

* make sure [AES67 daemon basic setup](#daemon_setup) is done
* download and install the [Dante Controller](https://www.audinate.com/products/software/dante-controller) 
* connect the Dante receiver to the network
* open the Dante Controller application, select the Routing dialog and wait for the Dante receiver to show up. Make sure this device or another on the network is acting as PTP clock master
* wait for a daemon Source to show up in the Routing dialog
* using the Routing Dialog connect the Daemon channels to the Dante receiver channels with the desired configuration, prohibition icons should show up
* go to the daemon WebUI, click on the PTP tab and wait for the "PTP Status" to report "locked"
* open a shell on the Linux host and start the playback on the RAVENNA ALSA device. For example to playback a test sound use:

          speaker-test -D plughw:RAVENNA -r 48000 -c 2 -t sine

* alternatively start the playback of a file:

          aplay -D plughw:RAVENNA -r 48000 -c 2 -f S24_3LE test.wav

* return to the Dante Controller application, select the Routing dialog and verify that prohibition icons get replaced with green icons to indicate that the audio flow is active

## Dante transmitters ##
<a name="dante_avio_transmitter"></a>
To run interoperability tests using a Dante transmitter follow these steps. Tests were done using Dante AVIOUBS and AVIOAI2 devices.

* make sure [AES67 daemon basic setup](#daemon_setup) is done
* download and install the [Dante Controller](https://www.audinate.com/products/software/dante-controller)
* connect the Dante transmitter to the network
* open the Dante Controller application, select the Routing dialog and wait for the Dante transmitter to show up. Make sure this device or another on the network is acting as PTP clock master
* in the Dante Controller wait for a daemon Source to show up in the Routing dialog
* in the Dante Controller go to Device view and select Dante transmitter, go to the AES67 Config tab and select "AES67 Mode" to Enabled. This may require a reboot of the Dante device
* go to the daemon WebUI, click on the Browser tab and wait for the Dante transmitter to show up as remote SAP source
* on the daemon WebUI select the Sinks tab, click on the plus icon to add a new Sink, mark the "Use SDP" flag and select the Dante transmitter SAP source
* open a shell on the Linux host and start the recording on the RAVENA ALSA device. For example:

          arecord -D plughw:RAVENNA -c 2 -f S24_3LE -r 48000 -t wav sink.wav

## Hasseb audio over Ethernet receiver ##
<a name="hasseb_receiver"></a>
To run interoperability tests using the [Hasseb audio over Ethernet receiver](http://hasseb.fi/shop2/index.php?route=product/product&product_id=62) follow these steps:

* make sure [AES67 daemon basic setup](#daemon_setup) is done
* make sure that the daemon mDNS support is enabled
* open the Hasseb WebUI and do the following:
  * deselect the "PTP slave only" checkbox to enable PTP master on Hasseb device
  * wait for the daemon source to show up in the "Stream name" drop down list and select it
  * press the Submit button
* go to the daemon WebUI, click on the PTP tab and wait for the "PTP Status" to report "locked"
* open a shell on the Linux host and start the playback on the RAVENNA ALSA device. For example to playback a test sound use:

          speaker-test -D plughw:RAVENNA -r 48000 -c 2 -t sine

* alternatively start the playback of a file:

          aplay -D plughw:RAVENNA -r 48000 -c 2 -f S24_3LE test.wav


