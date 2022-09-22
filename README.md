# RTMP relay v1.0

[![Build Status](https://api.travis-ci.org/elnormous/rtmp_relay.svg?branch=master)](https://travis-ci.org/elnormous/rtmp_relay) [![Build Status](https://ci.appveyor.com/api/projects/status/9axwxwyf99dcr11d?svg=true)](https://ci.appveyor.com/project/elnormous/rtmp_relay)

Relays RTMP streams to multiple endpoints.

# Usage

RTMP relay uses yaml-cpp submodule. The following command has to be run after cloning the RTMP relay:

```
$ git submodule update --init
```

To compile the RTMP relay, just run "make" in the root directory.
You can pass these arguments to rtmp_relay (located in the bin directory):

* *--config <config_file>* – path to config file
* *--daemon* – run RTMP relay as daemon
* *--kill-daemon* – kill the daemon
* *--reload-config* – reload the daemon's configuration
* *--help* – print the documentation

# Docker build
Check out submodules the same way as for a normal build, then run `docker-compose build`. This will result in a local image named `evo-rtmp-relay:latest`.

# Configuration

RTMP relay configuration files are YAML-based. It must start with servers array. Each server has following attributes:

* *endpoints* – array of endpoint descriptors
  * *applicationName* – for host streams this is the filter of incoming stream application names (can contain a regex), for client streams this is the name of the application (optional)
  * *streamName* – for host streams this is the filter of incoming stream names (can contain a regex), for client streams this is the name of the stream (optional)
  * *type* – type of connection (client or host)
  * *direction* – direction of the stream (input or output)
  * *addresses* – list of addresses to connect to (for client connections) or listen to (for server connections)
  * *video* – flag that indicates whether to forward video stream (default value is true)
  * *audio* – flag that indicates whether to forward audio stream (default value is true)
  * *data* – flag that indicates whether to forward data stream (default value is true)
  * *metaDataBlacklist* – list of metadata fields that should not be forwarded
  * *connectionTimeout* – how long should the attempt to connect last (default value is 5.0)
  * *reconnectInterval* – the interval of reconnection (default value is 5.0)
  * *reconnectCount* – amount of connect attempts (0 to reconnect forever)
  * *pingInterval* – client ping interval in seconds (default value is 60.0)
  * *bufferSize* – size of the client buffer for input streams (default value is 3000)
  * *amfVersion* – AMF version (for client connections) to use for communication (default value is 0)

*applicationName* can have the following tokens:

* {id} – id of the application
* {applicationName} – name of the application
* {streamName} – name of the stream
* {ipAddress} – IP address of the destination
* {port} – destination port

*streamName* name can have the following tokens:

* {id} – id of the sender
* {applicationName} – name of the application
* {streamName} – name of the source stream
* {ipAddress} – IP address of the destination
* {port} – destination port

Optionally you can add a web status page with "statusPage" object, which has the following attribute:
* *address* – the address of the web status page

Status page can be accessed in the following addresses:
* &lt;server address&gt;/stats – HTML output
* &lt;server address&gt;/stats.html – HTML output
* &lt;server address&gt;/stats.json – JSON output
* &lt;server address&gt;/stats.txt – text output

To configure logging, you can add "log" object to the config file. It has the following attributes
* *level* – the log threshold level (0 for no logs and 4 for all logs)
* *syslogEnabled* – should the syslog be used (default value is true) (on *NIX only)
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
      - endpoints:
          - address: [ "0.0.0.0:13004" ]
            type: "host"
            direction: "input"
            applicationName: "app/name"
            video: true
            audio: true
          - address: [ "52.19.130.93:1935" ]
            type: "client"
            direction: "output"
            applicationName: "app/name"
            streamName: "stream_name"
            video: true
            audio: true
            pingInterval: 60.0
          - address: [ "127.0.0.1:13005" ]
            type: "client"
            direction: "output"
            applicationName: "app/name"
            streamName: "{streamName}_2"
            video: true
            audio: true
            pingInterval: 60.0
            connectionTimeout: 5.0
            reconnectInterval: 5.0
            reconnectCount: 3
