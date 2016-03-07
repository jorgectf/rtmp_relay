CC=g++
CFLAGS=-std=c++11 -Wall -I external/rapidjson/include
SDIR=src

all:
	$(CC) $(CFLAGS) $(SDIR)/main.cpp $(SDIR)/Input.cpp $(SDIR)/Output.cpp $(SDIR)/Relay.cpp $(SDIR)/Server.cpp $(SDIR)/Utils.cpp -o rtmp

clean:
	rm *o rtmp
