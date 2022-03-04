# AES67 USB Receiver and Transmitter #

This document describes how to use the AES67 Daemon on a board to operate as AES67 USB Receiver and Transmitter.

The board doesn't need to have an audio codec but a USB client or OTG connector that is able to operate in device mode.

Eventually you will connect your card to your PC ( *Windows* / *Linux* / *MAC* ) via USB and use the PC as a player or recorder and the card to relay audio from USB to Ethernet and vice versa.

## Kernel configuration ##
<a name="kernel_config"></a>
You need to enable support for USB gadget and compile and enable the USB Audio Gadget 1.0 module of the Linux Kernel. 
For this make sure you have the following kernel config options enabled:

      CONFIG_USB_F_UAC1=m
      CONFIG_USB_CONFIGFS_F_UAC1=y
      CONFIG_GADGET_UAC1=y
In the Kernel menu config the location of these option is:

      -> Device Drivers 
        -> USB support (USB_SUPPORT [=y])
          -> USB Gadget Support (USB_GADGET [=y])
Transfer the new kernel and modules to the board. 
Also make sure your board is properly configured to use the USB connector in device mode.

**_NOTE:_** The reason for configuring USB Audio Gadget 1.0 instead of 2.0 is that 2.0 does not currently work properly with *Windows*.

## Install the USB Audio Gadget 1.0 ##
<a name="g_audio_install"></a>
Install the Audio Gadget module with:

      sudo modprobe g_audio

If the negotiation with the connected PC is succefull on the board a new virtual audio card shows up.
To verify run:

      cat /proc/asound/card
The expected output is:

       0 [UAC1Gadget     ]: UAC1_Gadget - UAC1_Gadget
                            UAC1_Gadget 0
By default this new audio codec works with a sample rate of 48Khz, 2 channels and sample format *S16_LE*.

On the PC new audio input and output devices show up. 
The name assigned to these depend on the OS.
On *Windows* a new speaker and microphone named *AC interface* get configured.

**_NOTE:_**
If the USB gadget module loads but the USB audio device doesn't show up check in the kernel ring buffer for the following message:

    udc-core: couldn't find an available UDC - added [g_audio] to list of pending drivers
    
This means you have other USB gadgets currently active (for example g_serial).
To proceed disable the automatic load of these modules and configure *g_audio* instead.

## AES67 USB Transmitter ##
<a name="usb_transmitter"></a>
Before starting make sure that the [AES67 daemon basic setup](DEVICES.md#daemon_setup) is done.

On the board run the following command to relay the audio from the USB to the AES67 Source:

      sudo nice -n -10 alsaloop -c 2 -r 48000 -f S16_LE -C plughw:UAC1Gadget -P plughw:RAVENNA
On the PC start the audio playback on the new device.

**_NOTE:_** The *ALSA RAVENNA driver* will perform the format conversion from *S16\_LE* to the *Source* format.

## AES67 USB Receiver ##
<a name="usb_receiver"></a>
Before starting make sure that the [AES67 daemon basic setup](DEVICES.md#daemon_setup) is done. 
You also need to configure a new stereo *Sink* on the daemon.

On the board run the following command to relay the audio from AES67 Sink to the USB:

      sudo nice -n -10 alsaloop -c 2 -r 48000 -f S16_LE -C plughw:RAVENNA -P plughw:UAC1Gadget
On the PC start the audio recording on the new device.

**_NOTE:_** The *ALSA RAVENNA driver* will perform the format conversion from *S16\_LE* to the *Sink* format.
