CC=g++
CPPFLAGS=-std=c++11 -Wall -I external/rapidjson/include -o $(BINDIR)/$@
LDFLAGS=
SRC=$(wildcard src/*.cpp)
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
	rm -rf src/*.o $(BINDIR)/rtmp_relay $(BINDIR)

directories: ${BINDIR}

${BINDIR}: 
	mkdir -p ${BINDIR}
