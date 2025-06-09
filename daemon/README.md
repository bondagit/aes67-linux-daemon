# AES67 Daemon #

AES67 daemon uses the Merging Technologies device driver (MergingRavennaALSA.ko) to implement basic AES67 functionalities. See [ALSA RAVENNA/AES67 Driver README](https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm/src/master/README.md) for additional information.

The daemon is responsible for:

* communication and configuration of the ALSA RAVENNA/AES67 device driver
* control and configuration of up to 64 sources and sinks using the ALSA RAVENNA/AES67 driver via netlink
* session handling and SDP parsing and creation
* HTTP REST API for the daemon control and configuration
* SAP sources discovery and advertisement compatible with AES67 standard
* mDNS sources discovery and advertisement (using Linux Avahi) compatible with Ravenna standard
* RTSP client and server to retrieve, return and update SDP files via DESCRIBE and ANNOUNCE methods according to Ravenna standard
* IGMP handling for SAP, PTP and RTP sessions
* automatic update of Sinks based on discovered mDNS/SAP remote sources


## Configuration file ##

The daemon uses a JSON file to store the configuration parameters.    
The config file must be specified at startup time and it gets updated when new params are set via the REST interface.   
See [JSON config params](#config) for additional info on the configuration parameters.    
If a status file is specified in the daemon's configuration the server will load it at startup ad will save it at termination.    
The status file contains all the configured sources and sinks (streams).    
See [JSON streams](#rtp-streams) for additional info on the status file format and its parameters.    

## HTTP REST API ##

The daemon implements a REST API interface to configure and control the driver.    
All operations returns HTTP *200* status code in case of success and HTTP *4xx* or *5xx* status code in case of failure.    
In case of failure the server returns a **text/plain** content type with the category and a description of the error occurred.    
**_NOTE:_** At present the embedded HTTP server doesn't implement neither HTTPS nor user authentication.

### Get Daemon Version ###
* **URL** /api/version
* **Method** GET
* **URL Params** none
* **Body Type** application/json
* **Body** [Version params](#version)

### Get Daemon Configuration ###
* **URL** /api/config    
* **Method** GET    
* **URL Params** none    
* **Body Type** application/json    
* **Body** [Config params](#config)

### Set Daemon Configuration ###
* **URL** /api/config    
* **Method** POST    
* **URL Params** none    
* **Body Type** application/json    
* **Body** [Config params](#config)

### Get PTP Configuration ###
* **URL** /api/ptp/config    
* **Method** GET    
* **URL Params** none    
* **Body Type** application/json    
* **Body** [PTP Config params](#ptp-config)

### Set PTP Configuration ###
* **URL** /api/ptp/config
* **Method** POST
* **URL Params** none
* **Body Type** application/json    
* **Body** [PTP Config params](#ptp-config)

### Get PTP Status ###
* **URL** /api/ptp/status    
* **Method** GET    
* **URL Params** none    
* **Body Type** application/json    
* **Body** [PTP Status params](#ptp-status)

### Add RTP Source ###
* **Description** add or update the RTP source specified by the *id*    
* **URL** /api/source/:id    
* **Method** PUT    
* **URL Params** id=[integer in the range (0-63)]     
* **Body Type** application/json    
* **Body** [RTP Source params](#rtp-source)

### Remove RTP Source ###
* **Description** remove the RTP sink specified by the *id*    
* **URL** /api/source/:id    
* **Method** DELETE    
* **URL Params** id=[integer in the range (0-63)]     
* **Body** none    

### Get RTP Source SDP file ###
* **Description** retrieve the SDP of the source specified by *id*    
* **URL** /api/source/sdp/:id    
* **Method** GET    
* **URL Params** id=[integer in the range (0-63)]     
* **Body Type** application/sdp    
* **Body** [Example SDP file for a source](#rtp-source-sdp)

### Add RTP Sink ###
* **Description** add or update the RTP sink specified by the *id*    
* **URL** /api/sink/:id    
* **Method** PUT    
* **URL Params** id=[integer in the range (0-63)]     
* **Body Type** application/json    
* **Body** [RTP Sink params](#rtp-sink)

### Remove RTP Sink ###
* **Description** remove the RTP sink specified by *id*   
* **URL** /api/sink/:id    
* **Method** DELETE    
* **URL Params** id=[integer in the range (0-63)]    
* **Body** none    

### Get RTP Sink status ###
* **Description** retrieve the status of the sink specified by *id*
* **URL** /api/sink/status/:id    
* **Method** GET    
* **URL Params** id=[integer in the range (0-63)]    
* **Body Type** application/json    
* **Body** [RTP Sink status params](#rtp-sink-status)

### Get all configured RTP Sources ###
* **URL** /api/sources    
* **Method** GET    
* **URL Params** none    
* **Body type** application/json    
* **Body** [RTP Sources params](#rtp-sources)

### Get all configured RTP Sinks ###
* **URL** /api/sinks    
* **Method** GET    
* **URL Params** none    
* **Body type** application/json    
* **Body** [RTP Sinks params](#rtp-sinks)

### Get all configured RTP Sources and Sinks (Streams) ###
* **URL** /api/streams    
* **Method** GET    
* **URL Params** none    
* **Body type** application/json    
* **Body** [RTP Streams params](#rtp-streams)

### Get remote RTP Sources ###
* **Description** retrieve the remote sources collected via SAP, via mDNS or both
* **URL** /api/browse/sources/[all|mdns|sap]
* **Method** GET    
* **URL Params** all=[all sources], mdns=[mDNS sources only], sap=[sap sources only]    
* **Body type** application/json    
* **Body** [RTP Remote Sources params](#rtp-remote-sources)

### Get streamer info for a Sink ###
* **Description** retrieve the streamer info for the specified Sink
* **URL** /api/streamer/info/:id
* **Method** GET
* **URL Params** id=[integer in the range (0-63)]
* **Body Type** application/json
* **Body** [Streamer info params](#streamer-info)

### Get streamer AAC audio file ###
* **Description** retrieve the AAC audio frames for the specified Sink and file id
* **URL** /api/streamer/streamer/:sinkId/:fileId
* **Method** GET
* **URL Params** sinkId=[integer in the range (0-63)], fileId=[integer in the range (0-*streamer_files_num*)]
* **HTTP headers** the headers _X-File-Count_, _X-File-Current-Id_, _X-File-Start-Id_ return the current global file count, the current file id and the start file id for the file returned
* **Body Type** audio/aac
* **Body** Binary body containing ADTS AAC LC audio frames


## HTTP REST API structures ##

### JSON Version<a name="version"></a> ###

Example
    {
      "version:" "bondagit-2.0"
    }

where:

> **version**
> JSON string specifying the daemon version.

### JSON Config<a name="config"></a> ###

Example

    {
      "interface_name": "lo",
      "http_addr": "127.0.0.1",
      "http_port": 8080,
      "rtsp_port": 8854,
      "log_severity": 2,
      "syslog_proto": "none",
      "syslog_server": "255.255.255.254:1234",
      "rtp_mcast_base": "239.2.0.1",
      "status_file": "./status.json",
      "rtp_port": "5004",
      "ptp_domain": 0,
      "ptp_dscp": 46,
      "playout_delay": 0,
      "frame_size_at_1fs": 192,
      "sample_rate": 44100,
      "max_tic_frame_size": 1024,
      "sap_mcast_addr": "239.255.255.255",
      "sap_interval": 30,
      "mac_addr": "01:00:5e:01:00:01",
      "ip_addr": "127.0.0.1",
      "node_id": "AES67 daemon d9aca383",
      "custom_node_id": "",
      "ptp_status_script": "./scripts/ptp_status.sh",
      "auto_sinks_update": true,
      "streamer_enabled": false,
      "streamer_channels": 8,
      "streamer_files_num": 6,
      "streamer_file_duration": 1,
      "streamer_player_buffer_files_num": 1,
      "transcriber_enabled": true,
      "transcriber_channels": 4,
	  "transcriber_files_num": 4,
      "transcriber_file_duration": 5,
      "transcriber_model": "../3rdparty/whisper.cpp/models/ggml-base.en.bin",
      "transcriber_language": "en",
      "transcriber_openvino_device": "CPU"
    }

where:

> **interface\_name**
> JSON string specifying the network interface used by the daemon and the driver for both the RTP, PTP, SAP and HTTP traffic.

> **http\_port**
> JSON number specifying the HTTP port number used by the web server in the daemon implementing the REST interface.

> **rtsp\_port**
> JSON number specifying the RTSP port number used by the RTSP server in the daemon.

> **log\_severity**
> JSON integer specifying the process log severity level (0 to 5).    
> All traces major or equal to the specified level are enabled. (0=trace, 1=debug, 2=info, 3=warning, 4=error, 5=fatal).

> **syslog\_proto**
> JSON string specifying the syslog protocol used for logging.    
> This can be an empty string to log to the local syslog, "udp" to send syslog messages 
> to a remote server or "none" to disable the syslog logging.
> When "none" is used the client writes the logs to the standard output.

> **syslog\_server**
> JSON string specifying the syslog server address used for logging.

> **status\_file**
> JSON string specifying the file that will contain the sessions status.    
> The file is loaded when the daemon starts and is saved when the daemon exits.

> **rtp\_mcast\_base**
> JSON string specifying the default base RTP IPv4 multicast address used by a source.    
> The specific multicast RTP address is the base address plus the source id number.    
> For example if the base address is 239.2.0.1 and source id is 1 the RTP source address used is 239.2.0.2.

> **rtp\_port**
> JSON number specifying the RTP port used by the sources.

> **ptp\_domain**
> JSON number specifying the PTP clock domain of the master clock the driver will attempt to synchronize to.

> **ptp\_dscp**
> JSON number specifying the IP DSCP used in IPv4 header for PTP traffic.
> Valid values are 48 (CS6) and 46 (EF).

> **sample\_rate**
> JSON number specifying the default sample rate.
> Valid values are 44100Hz, 48000Hz, 88200Hz, 96000Hz, 192000Hz and 384000Hz.

> **playout\_delay**
> JSON number specifying the default safety playout delay at 1FS in samples.

> **tic\_frame\_size\_at\_1fs**
> JSON number specifying the TIC frame size at 1FS in samples, valid range is from 32 to 192 samples.
> This global setting is used to determine the driver base timer period. For example with a value of 192 samples this period is set to 4ms and the outgoing RTP packets are scheduled for being sent every 4ms resulting on an average latency greater than 4ms.

> **max\_tic\_frame\_size**
> JSON number specifying the max tick frame size. This is currently set to 1024.

> **sap\_mcast\_addr**
> JSON string specifying the SAP multicast address used for both announcing local sources and browsing remote sources.    
> By default and according to SAP RFC this address is 224.2.127.254, but many devices use 239.255.255.255.

> **sap\_interval**
> JSON number specifying the SAP interval in seconds to use. Use 0 for automatic and RFC compliant interval. Default is 30secs.

> **mac\_addr**
> JSON string specifying the MAC address of the specified network device.
> **_NOTE:_** This parameter is read-only and cannot be set. The server will determine the MAC address of the network device at startup time.

> **ip\_addr**
> JSON string specifying the IP address of the specified network device.
> This parameter can be set to specify the prefferred IP address to use. 
> In case such address is not valid, the server will determine the IP address of the network device at startup time and will monitor it periodically.

> **http\_addr**
> JSON string specifying the alternate IP address used for the daemon HTTP interface. 
> If this address is specified the HTTP interface will bind to this IP instead of the one specified by the *ip_addr* parameter.

> **mdns\_enabled**
> JSON boolean specifying whether the mDNS discovery is enabled or disabled.

> **node\_id**
> JSON string specifying the unique node identifier used to identify mDNS, SAP and SDP services announced by the daemon.
> **_NOTE:_** This parameter is read-only and cannot be set. The server will determine the node id at startup time.

> **auto\_sinks\_update**
> JSON boolean specifying whether to enable or disable the automatic update of the configured Sinks.
> When enabled the daemon will automatically update the configured Sinks according to the discovered remote sources via SAP and mDNS/RTSP updates. The SDP Originator (o=) is used to match a Sink with the remote source/s.

> **custom\_node\_id**
> JSON string specifying a custom node identifier used to identify mDNS, SAP and SDP services announced by the daemon. 
> When this parameter is empty the *node_id* is automatically generated by the daemon based on the current IP address.
> **_NOTE:_** When not empty, it is responsibility of the user to specify a unique node id to avoid name clashes on the LAN.

> **ptp\_status\_script**
> JSON string specifying the path to the script executed in background when the PTP slave clock status changes.
> The PTP clock status is passed as first parameter to the script and it can be *unlocked*, *locking* or *locked*.

> **streamer\_enabled**
> JSON boolean specifying whether the HTTP Streamer is enabled or disabled.
> Once activated, the HTTP Streamer starts capturing samples for number of channels specified by *streamer_channels* starting from channel 0, then it splits them into *streamer_files_num* files of a *streamer_file_duration* duration for each configured Sink and it serves them via HTTP.

> **streamer\_channels**
> JSON number specifying the number of channels captured by the HTTP Streamer starting from channel 0, 8 by default.

> **streamer\_files\_num**
> JSON number specifying the number of files into which the stream gets split.

> **streamer\_file\_duration**
> JSON number specifying the maximum duration of each streamer file in seconds.

> **streamer\_player\_buffer\_files\_num**
> JSON number specifying the player buffer in number of files.

> **transcriber\_enabled**
> JSON boolean Enables or disables transcription for all Sinks. Set to true to enable.

> **transcriber\_channels**
> JSON number specifying the number of audio channels captured by the ALSA capture thread.

> **transcriber\_files\_num**
> JSON number specifying the number of buffers in the rotating audio buffers pool.

> **transcriber\_file\_duration**: 
> JSON number specifying the duration (in seconds) of each audio buffer.

> **transcriber\_model**: 
> JSON string specifying the path to the Whisper model file. The default is the base English model.

> **transcriber\_language**: 
> JSON string specifying the language setting for Whisper. Default is English "en".

> **transcriber\_openvino\_device**: 
> JSON string specifying the OpenVINO device for transcription inference, if supported by the current model. Default is "CPU".

### JSON PTP Config<a name="ptp-config"></a> ###

Example

    {
      "domain": 0,
      "dscp": 46
    }

where:

> **domain**
> JSON number specifying the PTP clock domain of the master clock the driver will attempt to synchronize to. 

> **dscp**
> JSON number specifying the IP DSCP used in IPv4 header for PTP traffic.   
> Valid values are 48 (CS6) and 46 (EF).

### JSON PTP Status<a name="ptp-status"></a> ###

Example

    {
      "status": "unlocked",
      "gmid": "00-00-00-FF-FE-00-00-00",
      "jitter": 0
    }

where:

> **status**
> JSON string specifying the PTP slave status. This can be *unlocked*, *locking* or *locked*.

> **gmid**
> JSON string specifying GrandMaster clock ID we are currently synchronized to.

> **jitter**
> JSON number specifying the measured PTP packet delay jitter.

### JSON RTP source<a name="rtp-source"></a> ###

Example:

    {
      "enabled": true,
      "name": "ALSA Source 0",
      "io": "Audio Device",
      "codec": "L16",
      "address": "",
      "max_samples_per_packet": 48,
      "ttl": 15,
      "payload_type": 98,
      "dscp": 34,
      "refclk_ptp_traceable": true,
      "map": [0, 1]
    }

where:

> **enabled**
> JSON bool specifying whether the source is enabled and will be announced or not. 

> **name**
> JSON string specifying the source name.

> **io**
> JSON string specifying the IO name.

> **codec**
> JSON string specifying codec in use.
> Valid values are L16, L24 and AM824 (L32).

> **address**
> JSON string specifying the destination address to use.
> If this field contains a valid address it's used instead of the default multicast address for the source.
> In case an unicast address is provided this must be of an host currently active on the local network.

> **max\_sample\_per\_packet**
> JSON number specifying the max number of samples contained in one RTP packet.    
> Valid values are 12, 16, 48, 96, 192.        
> See the table below for correspondent max RTP packet duration: 
> |     | 44.1kHz  | 48kHz   | 96kHz   |
> |-----|----------|---------|---------|
> | 12  | 272µs    | 250µs   | 125µs   |
> | 16  | 363µs    | 333µs   | 166µs   |
> | 48  | 1.088ms  | 1ms     | 500µs   |
> | 96  | 2.177ms  | 2ms     | 1ms     |
> | 192 | 4.353ms  | 4ms     | 2ms     |

> **ttl**
> JSON number specifying RTP packets Time To Live.

> **payload\_type**
> JSON number specifying RTP Payload Type.

> **dscp**
> JSON number specifying the IP DSCP used in IPv4 header for RTP traffic.
> Valid values are 46 (EF), 34 (AF41), 26 (AF31), 0 (BE).

> **refclk\_ptp\_traceable**
> JSON boolean specifying whether the PTP reference clock is traceable or not. 
> A reference clock source is traceable if it is known to be delivering traceable time.

> **map**
> JSON array of integers specifying the mapping between the RTP source and the ALSA playback device channels used during playback. The length of this map determines the number of channels of the source.

### RTP source SDP<a name="rtp-source-sdp"></a> ###

Example:

    v=0
    o=- 0 0 IN IP4 127.0.0.1
    s=ALSA Source 0
    c=IN IP4 239.1.0.1/15
    t=0 0
    a=clock-domain:PTPv2 0
    m=audio 5004 RTP/AVP 98
    c=IN IP4 239.1.0.1/15
    a=rtpmap:98 L16/44100/2
    a=sync-time:0
    a=framecount:48
    a=ptime:1.08843537415
    a=mediaclk:direct=0
    a=ts-refclk:ptp=traceable
    a=recvonly

### JSON RTP sink<a name="rtp-sink"></a> ###

Example:

    {
      "name": "ALSA Sink 0",
      "io": "Audio Device",
      "delay": 576,
      "use_sdp": false,
      "source": "http://127.0.0.1:8080/api/source/sdp/0",
      "sdp": "v=0\no=- 0 0 IN IP4 127.0.0.1\ns=ALSA Source 0\nc=IN IP4 239.1.0.1/15\nt=0 0\na=clock-domain:PTPv2 0\nm=audio 5004 RTP/AVP 98\nc=IN IP4 239.1.0.1/15\na=rtpmap:98 L16/44100/2\na=sync-time:0\na=framecount:48\na=ptime:1.08843537415\na=mediaclk:direct=0\na=ts-refclk:ptp=traceable\na=recvonly\n",
      "ignore_refclk_gmid": false,
      "map": [0, 1]
    }

where:

> **name**
> JSON string specifying the source name.

> **io**
> JSON string specifying the IO name.

> **delay**
> JSON number specifying the playout delay of the sink in samples.    
> **_NOTE:_** The specified delay cannot be less than the source max samples size announced by the SDP file.

> **use\_sdp**
> JSON boolean specifying whether the source SDP file is fetched from the HTTP URL specified in the **source** parameter or the SDP in the **sdp** parameter is used.

> **source**
> JSON string specifying the URL of the source SDP file. At present HTTP and RTSP protocols are supported.
> This parameter is only used if **use\_sdp** is false. Even when not used it must be specified and an empty string can be provided.

> **sdp**
> JSON string specifying the SDP of the source.
> This parameter is only used if **use\_sdp** is true. Even when not used it must be specified and an empty string can be provided. 
> See [example SDP file for a source](#rtp-source-sdp)

> **ignore\_refclk\_gmid**
> JSON boolean specifying whether the grand master reference clock ID specified in the SDP file of the source must be compared with the master reference clock to which the current PTP slave clock is syncronized.

> **map**
> JSON array of integers specifying the mapping between the RTP sink and the ALSA capture device channels used during recording. The length of this map determines the number of channels of the sink.

### JSON RTP sink status<a name="rtp-sink-status"></a> ###

Example:

    {   
      "sink_flags":
      { 
        "rtp_seq_id_error": false, 
        "rtp_ssrc_error": false, 
        "rtp_payload_type_error": false, 
        "rtp_sac_error": false, 
        "receiving_rtp_packet": false, 
        "some_muted": false, 
        "all_muted": false, 
        "muted": true
      },
      "sink_min_time": 0
}

where:

> **sink\_flags**
> JSON object containing a set of flags reporting the RTP sink status.    

>    - **rtp\_seq\_id\_error** JSON boolean specifying whether a wrong RTP sequence was detected.    

>    - **rtp\_ssrc\_error** JSON boolean specifying whether a wrong RTP source is contributing to the incoming stream.    

>    - **rtp\_payload\_type\_error** JSON boolean specifying whether a wrong payload type was received.    

>    - **rtp\_sac\_error** JSON boolean specifying whether a packet with a wrong RTP timestamp was received.

>    - **receiving\_rtp\_packet** JSON boolean specifying whether the sink is currently receiving RTP packets from the source.   

>    - **some\_muted** JSON boolean (not used)

>    - **all\_muted** JSON boolean (not used)

>    - **muted** JSON boolean specifying whether the sink is currently muted.

> **sink\_min\_time** JSON number specifying the minimum source RTP packet arrival time.    

### JSON RTP Sources<a name="rtp-sources"></a> ###

Example:

    {
      "sources": [
      {
        "id": 0,
        "enabled": true,
        "name": "ALSA Source 0",
        "io": "Audio Device",
        "max_samples_per_packet": 48,
        "codec": "L16",
        "ttl": 15,
        "payload_type": 98,
        "dscp": 34,
        "refclk_ptp_traceable": true,
        "map": [ 0, 1 ]
      }  ]
   }

where:

> **sources**
> JSON array of the configured sources. 
> Every source is identified by the JSON number **id** (in the range 0 - 63). 
> See [RTP Source params](#rtp-source) for all the other parameters.

### JSON RTP Sinks<a name="rtp-sinks"></a> ###

Example:

    {
      "sinks": [
      {
        "id": 0,
        "name": "ALSA Sink 0",
        "io": "Audio Device",
        "use_sdp": true,
        "source": "http://127.0.0.1:8080/api/source/sdp/0",
        "sdp": "v=0\no=- 0 0 IN IP4 127.0.0.1\ns=ALSA Source 0\nc=IN IP4 239.1.0.1/15\nt=0 0\na=clock-domain:PTPv2 0\nm=audio 5004 RTP/AVP 98\nc=IN IP4 239.1.0.1/15\na=rtpmap:98 L16/44100/2\na=sync-time:0\na=framecount:48\na=ptime:1.08843537415\na=mediaclk:direct=0\na=ts-refclk:ptp=traceable\na=recvonly\n",
        "delay": 576,
        "ignore_refclk_gmid": false,
        "map": [ 0, 1 ]
      }  ]
   }

where:

> **sinks**
> JSON array of the configured sinks. 
> Every sink is identified by the JSON number **id** (in the range 0 - 63). 
> See [RTP Sink params](#rtp-sink) for all the other parameters.

### JSON RTP Streams<a name="rtp-streams"></a> ###

Example:

    {
      "sources": [
      {
        "id": 0,
        "enabled": true,
        "name": "ALSA Source 0",
        "io": "Audio Device",
        "max_samples_per_packet": 48,
        "codec": "L16",
        "ttl": 15,
        "payload_type": 98,
        "dscp": 34,
        "refclk_ptp_traceable": true,
        "map": [ 0, 1 ]
      }  ],
      "sinks": [
      {
        "id": 0,
        "name": "ALSA Sink 0",
        "io": "Audio Device",
        "use_sdp": true,
        "source": "http://127.0.0.1:8080/api/source/sdp/0",
        "sdp": "v=0\no=- 0 0 IN IP4 127.0.0.1\ns=ALSA Source 0\nc=IN IP4 239.1.0.1/15\nt=0 0\na=clock-domain:PTPv2 0\nm=audio 5004 RTP/AVP 98\nc=IN IP4 239.1.0.1/15\na=rtpmap:98 L16/44100/2\na=sync-time:0\na=framecount:48\na=ptime:1.08843537415\na=mediaclk:direct=0\na=ts-refclk:ptp=traceable\na=recvonly\n",
        "delay": 576,
        "ignore_refclk_gmid": false,
        "map": [ 0, 1 ]
      }  ]
    }

where:

> **sources**
> JSON array of the configured sources. 
> Every source is identified by the JSON number **id** (in the range 0 - 63). 
> See [RTP Source params](#rtp-source) for all the other parameters.

> **sinks**
> JSON array of the configured sinks. 
> Every sink is identified by the JSON number **id** (in the range 0 - 63). 
> See [RTP Sink params](#rtp-sink) for all the other parameters.

### JSON Remote Sources<a name="rtp-remote-sources"></a> ###

Example:

    {
      "remote_sources": [
      {
        "source": "SAP",
        "id": "d00000a611d",
        "name": "ALSA Source 2",
        "domain": "",
        "address": "10.0.0.13",
        "sdp": "v=0\no=- 2 0 IN IP4 10.0.0.13\ns=ALSA Source 2\nc=IN IP4 239.1.0.3/15\nt=0 0\na=clock-domain:PTPv2 0\nm=audio 5004 RTP/AVP 98\nc=IN IP4 239.1.0.3/15\na=rtpmap:98 L16/48000/2\na=sync-time:0\na=framecount:48\na=ptime:1\na=mediaclk:direct=0\na=ts-refclk:ptp=IEEE1588-2008:00-10-4B-FF-FE-7A-87-FC:0\na=recvonly\n",
        "last_seen": 2768,
        "announce_period": 30 
      }, 
      {
        "source": "SAP",
        "id": "d00000a8dd5",
        "name": "ALSA Source 1",
        "domain": "",
        "address": "10.0.0.13",
        "sdp": "v=0\no=- 1 0 IN IP4 10.0.0.13\ns=ALSA Source 1\nc=IN IP4 239.1.0.2/15\nt=0 0\na=clock-domain:PTPv2 0\nm=audio 5004 RTP/AVP 98\nc=IN IP4 239.1.0.2/15\na=rtpmap:98 L16/48000/2\na=sync-time:0\na=framecount:48\na=ptime:1\na=mediaclk:direct=0\na=ts-refclk:ptp=IEEE1588-2008:00-10-4B-FF-FE-7A-87-FC:0\na=recvonly\n",
        "last_seen": 2768,
        "announce_period": 30 
      }  ]
    }


where:

> **remote\_sources**
> JSON array of the remote sources collected so far.
> Every source is identified by the unique JSON string **id**.

> **source**
> JSON string specifying the protocol used to collect the remote source.

> **id**
> JSON string specifying the remote source unique id.

> **name**
> JSON string specifying the remote source name announced.

> **doamin**
> JSON string specifying the remote source domain announced.
**_NOTE:_** This field is only populated for mDNS sources.

> **address**
> JSON string specifying the remote source address announced.

> **sdp**
> JSON string specifying the remote source SDP.

> **last\_seen**
> JSON number specifying the last time the source was announced.
> This time is expressed in seconds since the source was announced.

> **announce_period**
> JSON number specifying the meausured period in seconds between the last source announcements.
> A remote source is automatically removed if it doesn't get announced for **announce\_period** x 10 seconds.

### JSON Streamer info<a name="streamer-info"></a> ###

Example:

    {
       "status": 0,
       "file_duration": 1,
       "files_num": 8,
       "player_buffer_files_num": 1,
       "start_file_id": 3,
       "current_file_id": 0,
       "channels": 2,
       "format": "s16",
       "rate": 48000
    }

where:

> **status**
> JSON number containing the streamer status code.
> Status is 0 in case the streamer is able to provide the audio samples, othrewise the specific error code is returned.
> 0 - OK
> 1 - PTP clock not locked
> 2 - Channel/s not captured
> 3 - Buffering
> 4 - Streamer not enabled
> 5 - Invalid Sink
> 6 - Cannot retrieve Sink

> **file_duration_sec**
> JSON number specifying the duration of each file.

> **files_num**
> JSON number specifying the number of files. The streamer will use these files as a circular buffer.

> **start_file_id**
> JSON number specifying the file id to use to start the playback.

> **current_file_id**
> JSON number specifying the file id that is beeing created by the daemon.

> **player_buffer_files_num**
> JSON number specifying the number of files to use for buffering.

> **channels**
> JSON number specifying the number of channels of the stream.

> **format**
> JSON string specifying the PCM encoding of the AAC compressed stream.

> **rate**
> JSON number specifying the sample rate of the stream.
