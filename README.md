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

* *listen* – the address server is listening to
* *pingInterval* – client ping interval in seconds
* *servers* – application object (can be multiple for each server)
  * *inputs* – array of input destinations
    * *applicationName* – name of the application (optional if server should route all applications)
    * *listen*: the address server is listening to (optional)
    * *addresses* – list of addresses to push to (optional)
    * *video* – flag that indicates wether to forward video stream
    * *audio* – flag that indicates wether to forward audio stream
    * *data* – flag that indicates wether to forward data stream
    * *metaDataBlacklist* – list of metadata fields that should not be forwarded
    * *pingInterval* – client ping interval in seconds
    * *connectionTimeout* – how long should the attempt to connect last
    * *reconnectInterval* – the interval of reconnection
    * *reconnectCount* – amount of connect attempts
  * *outputs* – array of output destinations
    * *overrideApplicationName* – name of the output application name
    * *overrideStreamName* – string to override the stream name with
    * *listen*: the address server is listening to (optional)
    * *addresses* – list of addresses to push to (optional)
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
* ${streamName} – name of the application

*overrideStream* name can have the following tokens:

* ${id} – id of the sender
* ${streamName} – name of the source stream
* ${applicationName} – name of the application
* ${ipAddress} – IP address of the destination
* ${port} – destination port

Optionally you can add a web status page with "statusPage" object, which has the following attribute:
* *listen* – the address of the web status page

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
      - applicationName: "app/name"
        overrideApplicationName: "${name}_2"
        address: "127.0.0.1:2200"
        type: "push"
        output:
          - streamName: "name"
            overrideStreamName: "test_${name}"
            address: "10.0.1.1:1935"
            type: "push"
            video: true
            audio: true
            connectionTimeout: 5.0
            reconnectInterval: 5.0
            reconnectCount: 3
      - applicationName: "casino/roulette"
        address: "127.0.0.1:2200"
        type: "pull"
        output:
          - address: [ "10.0.1.2:1935", "10.0.1.3:1935" ]
            type: "push"
            video: true
            audio: true
            connectionTimeout: 5.0
            reconnectInterval: 5.0
            reconnectCount: 3
          - address: "0.0.0.0:1935"
            type: "pull"
            video: true
            audio: true
            pingInterval: 60.0