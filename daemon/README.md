# AES67 Daemon #

AES67 daemon uses the Merging Technologies device driver (MergingRavennaALSA.ko) to implement basic AES67 functionalities. See [ALSA RAVENNA/AES67 Driver README](https://bitbucket.org/MergingTechnologies/ravenna-alsa-lkm/src/master/README.md) for additional information.

The daemon is responsible for:

* communication and configuration of the device driver
* provide an HTTP REST API for the daemon control and configuration
* session handling and SDP parsing and creation
* SAP discovery protocol implementation 
* IGMP handling for SAP and RTP sessions

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

## HTTP REST API structures ##

### JSON Config<a name="config"></a> ###

Example

    {
      "interface_name": "lo",
      "http_port": 8080,
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
      "sap_interval": 30,
      "mac_addr": "01:00:5e:01:00:01",
      "ip_addr": "127.0.0.1"
    }

where:

> **interface\_name**
> JSON string specifying the network interface used by the daemon and the driver for both the RTP, PTP, SAP and HTTP traffic.

> **http\_port**
> JSON number specifying the HTTP port number used by the embedded web server in the daemon implementing the REST interface.

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

> **rtp_port**
> JSON number specifying the RTP port used by the sources.

> **ptp\_domain**
> JSON number specifying the PTP clock domain of the master clock the driver will attempt to synchronize to.

> **ptp\_dscp**
> JSON number specifying the IP DSCP used in IPv4 header for PTP traffic.
> Valid values are 48 (CS6) and 46 (EF).

> **sample\_rate**
> JSON number specifying the default sample rate.
> Valid values are 44100Hz, 48000Hz and 96000Hz.

> **playout\_delay**
> JSON number specifying the default safety playout delay at 1FS in samples.

> **tic\_frame\_size\_at\_1fs**
> JSON number specifying the RTP frame size at 1FS in samples.

> **max\_tic\_frame\_size**
> JSON number specifying the max tick frame size.     
> In case of a high value of *tic_frame_size_at_1fs*, this must be set to 8192.

> **sap\_interval**
> JSON number specifying the SAP interval in seconds to use. Use 0 for automatic and RFC compliant interval. Default is 30secs.

> **mac\_addr**
> JSON string specifying the MAC address of the specified network device.
> **_NOTE:_** This parameter is read-only and cannot be set. The server will determine the MAC address of the network device at startup time.

> **ip\_addr**
> JSON string specifying the IP address of the specified network device.
> **_NOTE:_** This parameter is read-only and cannot be set. The server will determine the IP address of the network device at startup time and will monitor it periodically.

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

### JSON RTP source<a name="rtp-source"></a> ###

Example:

    {
      "enabled": true,
      "name": "ALSA Source 0",
      "io": "Audio Device",
      "codec": "L16",
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
> Valid values are L16 and L24.

> **max\_sample\_per\_packet**
> JSON number specifying the max number of samples contained in one RTP packet.
> Valid values are 12, 16, 18, 96, 192.

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
> JSON string specifying the HTTP URL of the source SDP file. This parameter is mandatory if **use\_sdp** is false.

> **sdp**
> JSON string specifying the SDP of the source. This parameter is mandatory if **use\_sdp** is true.
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

