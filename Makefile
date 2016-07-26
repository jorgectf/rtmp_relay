CC=g++
CPPFLAGS=-std=c++11 -Wall -I external/cppsocket -I external/yaml-cpp/include -o $(BINDIR)/$@
LDFLAGS=
SRC=src/Amf0.cpp \
	src/main.cpp \
	src/Receiver.cpp \
	src/Relay.cpp \
	src/RTMP.cpp \
	src/Sender.cpp \
	src/Server.cpp \
	src/Utils.cpp \
	external/cppsocket/Acceptor.cpp \
	external/cppsocket/Connector.cpp \
	external/cppsocket/Network.cpp \
	external/cppsocket/Socket.cpp \
	external/yaml-cpp/src/binary.cpp \
	external/yaml-cpp/src/convert.cpp \
	external/yaml-cpp/src/directives.cpp \
	external/yaml-cpp/src/emit.cpp \
	external/yaml-cpp/src/emitfromevents.cpp \
	external/yaml-cpp/src/emitter.cpp \
	external/yaml-cpp/src/emitterstate.cpp \
	external/yaml-cpp/src/emitterutils.cpp \
	external/yaml-cpp/src/exp.cpp \
	external/yaml-cpp/src/memory.cpp \
	external/yaml-cpp/src/node_data.cpp \
	external/yaml-cpp/src/node.cpp \
	external/yaml-cpp/src/nodebuilder.cpp \
	external/yaml-cpp/src/nodeevents.cpp \
	external/yaml-cpp/src/null.cpp \
	external/yaml-cpp/src/ostream_wrapper.cpp \
	external/yaml-cpp/src/parse.cpp \
	external/yaml-cpp/src/parser.cpp \
	external/yaml-cpp/src/regex_yaml.cpp \
	external/yaml-cpp/src/scanner.cpp \
	external/yaml-cpp/src/scanscalar.cpp \
	external/yaml-cpp/src/scantag.cpp \
	external/yaml-cpp/src/scantoken.cpp \
	external/yaml-cpp/src/simplekey.cpp \
	external/yaml-cpp/src/singledocparser.cpp \
	external/yaml-cpp/src/stream.cpp \
	external/yaml-cpp/src/tag.cpp
OBJ=$(SRC:.cpp=.o)
BINDIR := ./bin

all: directories rtmp_relay

debug: CXXFLAGS += -DDEBUG -g
debug: CCFLAGS += -DDEBUG -g
debug: directories rtmp_relay

rtmp_relay: $(OBJ)
	$(CC) $(CFLAGS) $^ -o $(BINDIR)/$@

.PHONY: clean

clean:
	rm -rf src/*.o external/*.o $(BINDIR)/rtmp_relay $(BINDIR)

directories: ${BINDIR}

${BINDIR}: 
	mkdir -p ${BINDIR}
