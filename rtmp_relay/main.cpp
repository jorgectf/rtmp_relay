//
//  rtmp_relay
//

#include "Relay.h"

int main(int argc, const char * argv[])
{
    Relay relay;
    
    if (!relay.init())
    {
        fprintf(stderr, "Failed to init relay\n");
        return 1;
    }
    
    relay.run();
    
    // TODO: open reading sockets
    // TODO: open writing socket
    // TODO: make handshake
    // TODO: read incoming data and push to outgoing
    
    return 0;
}
