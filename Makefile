CXXFLAGS=-c -std=c++11 -Wall -I external/cppsocket -I external/yaml-cpp/include
LDFLAGS=

SOURCES=src/Amf0.cpp \
	src/Application.cpp \
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
OBJECTS=$(SOURCES:.cpp=.o)

BINDIR=./bin
EXECUTABLE=rtmp_relay

all: directories $(SOURCES) $(EXECUTABLE)

debug: CXXFLAGS+=-DDEBUG -g
debug: directories $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(OBJECTS) $(LDFLAGS) -o $(BINDIR)/$@

.cpp.o:
	$(CXX) $(CXXFLAGS) $< -o $@

prefix=/usr/bin

install:
	install -m 0755 $(BINDIR)/$(EXECUTABLE) $(prefix)

.PHONY: install

uninstall:
	rm -f $(prefix)/$(EXECUTABLE)

.PHONY: uninstall

clean:
	rm -rf src/*.o external/cppsocket/*.o external/yaml-cpp/src/*.o $(BINDIR)/$(EXECUTABLE) $(BINDIR)

.PHONY: clean

directories: ${BINDIR}

.PHONY: directories

${BINDIR}: 
	mkdir -p ${BINDIR}
