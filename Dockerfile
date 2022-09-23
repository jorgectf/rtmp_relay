FROM centos:7

RUN yum update -y && \
	yum install -y clang make gcc-c++ gdb && \
	yum clean all && \
	mkdir /app

# This can be overriden to use a custom file. Must be mounted into /app/config though.
ENV STREAM_RELAY_CFGFILE=stream-relay.yaml

WORKDIR /build
COPY external /build/external
COPY src /build/src
COPY Makefile /build

RUN make clean all CXX=clang++

# Verify that it can run
RUN ./bin/stream_relay --version

RUN cp bin/stream_relay /app/stream_relay

# User should mount /app/config/ with a rtmp-relay.yaml file to run with default entrypoint
VOLUME [ "/app/config" ]

WORKDIR /app
ENTRYPOINT ./stream_relay --config /app/config/${STREAM_RELAY_CFGFILE}

