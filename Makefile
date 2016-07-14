CC=g++
CPPFLAGS=-std=c++11 -Wall -I external/rapidjson/include -I external/cppsocket -o $(BINDIR)/$@
LDFLAGS=
SRC=src/Amf0.cpp \
	src/main.cpp \
	src/Receiver.cpp \
	src/Relay.cpp \
	src/RTMP.cpp \
	src/Sender.cpp \
	src/Server.cpp \
	src/Utils.cpp \
	external/Acceptor.cpp \
	external/Connector.cpp \
	external/Network.cpp \
	external/Socket.cpp
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
