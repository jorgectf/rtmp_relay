FROM centos:7

RUN yum update -y && \
	yum install -y clang make gcc-c++ gdb && \
	yum clean all && \
	mkdir /app

WORKDIR /build
COPY external /build/external
COPY src /build/src
COPY Makefile /build

RUN make clean all CXX=clang++

# Verify that it can run
RUN ./bin/rtmp_relay --version

RUN cp bin/rtmp_relay /app/rtmp_relay

# User should mount /app/config/ with a rtmp-relay.yaml file to run with default entrypoint
VOLUME [ "/app/config" ]

WORKDIR /app
ENTRYPOINT ["./rtmp_relay", "--config", "/app/config/rtmp-relay.yaml"]

