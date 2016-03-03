//
//  rtmp_relay
//

#include <ctime>
#include "Relay.h"
#include "Server.h"

Relay::Relay()
{
    
}

Relay::~Relay()
{
    for (Server* server : _servers)
    {
        delete server;
    }
}

void Relay::run()
{
    const timespec sleepTime = { 0, 10000000 };
    
    while (true)
    {
        for (Server* server : _servers)
        {
            server->update();
        }
        
        nanosleep(&sleepTime, nullptr);
    }
}
