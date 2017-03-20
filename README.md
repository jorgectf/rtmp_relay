# RTMP relay v0.8

[![Build Status](https://api.travis-ci.org/elnormous/rtmp_relay.svg?branch=master)](https://travis-ci.org/elnormous/rtmp_relay) [![Build Status](https://ci.appveyor.com/api/projects/status/9axwxwyf99dcr11d?svg=true)](https://ci.appveyor.com/project/elnormous/rtmp_relay)

Relays RTMP streams to multiple endpoints.

# Usage

RTMP relay uses cppsocket and yaml-cpp submodules. The following command has to be run after cloning the RTMP relay:

```
$ git submodule update --init
```

To compile the RTMP relay, just run "make" in the root directory. To launch it, run "./bin/rtmp_relay --config config.yaml". You can optionally pass "--daemon" to run it as a daemon or "--kill-daemon" to kill it.

# Configuration

RTMP relay configuration files are YAML-based. It must start with servers array. Each server has following attributes:

* *connections* – array of connection descriptors
  * *applicationName* – for input stream this is the filter of incoming stream application names, for output stream this is the name of the application (optional)
  * *streamName* – for input stream this is the filter of incoming stream names, for output stream this is the name of the stream (optional)
  * *overrideApplicationName* – name of the output application name
  * *overrideStreamName* – string to override the stream name with
  * *type* – type of connection (client or host)
  * *stream* – type of stream (input or output)
  * *addresses* – list of addresses to connect to (for client connections) or listen to (for server connections)
  * *video* – flag that indicates wether to forward video stream
  * *audio* – flag that indicates wether to forward audio stream
  * *data* – flag that indicates wether to forward data stream
  * *metaDataBlacklist* – list of metadata fields that should not be forwarded
  * *pingInterval* – client ping interval in seconds
  * *connectionTimeout* – how long should the attempt to connect last
  * *reconnectInterval* – the interval of reconnection
  * *reconnectCount* – amount of connect attempts

*overrideApplicationName* can have the following tokens:

* ${id} – id of the application
* ${applicationName} – name of the application
* ${streamName} – name of the stream
* ${ipAddress} – IP address of the destination
* ${port} – destination port

*overrideStream* name can have the following tokens:

* ${id} – id of the sender
* ${applicationName} – name of the application
* ${streamName} – name of the source stream
* ${ipAddress} – IP address of the destination
* ${port} – destination port

Optionally you can add a web status page with "statusPage" object, which has the following attribute:
* *address* – the address of the web status page

Status page can be accessed in the following addresses:
* &lt;server address&gt;/stats – HTML output
* &lt;server address&gt;/stats.html – HTML output
* &lt;server address&gt;/stats.json – JSON output
* &lt;server address&gt;/stats.txt – text output

To configure logging, you can add "log" object to the config file. It has the following attributes
* *level* – the log threshold level (0 for no logs and 4 for all logs)
* *syslogIdent* – identification to be passed to openlog (on *NIX only)
* *syslogFacility* – facility to be passed to openlog (on *NIX only)

Example configuration:

    log:
        level: 4
        syslogIdent: relay
        syslogFacility: "LOG_LOCAL3"
    statusPage:
        address: "0.0.0.0:80"
    servers:
      - connections:
          - address: [ "0.0.0.0:13004" ]
            type: "host"
            stream: "input"
            applicationName: "app/name"
            video: true
            audio: true
          - address: [ "52.19.130.93:1935" ]
            type: "client"
            stream: "output"
            applicationName: "app/name"
            streamName: "stream_name"
            video: true
            audio: true
            pingInterval: 60.0
          - address: [ "127.0.0.1:13005" ]
            type: "client"
            stream: "output"
            applicationName: "app/name"
            streamName: "stream_name"
            overrideStreamName: "test_${name}"
            video: true
            audio: true
            pingInterval: 60.0
            connectionTimeout: 5.0
            reconnectInterval: 5.0
            reconnectCount: 3