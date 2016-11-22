//
//  rtmp_relay
//

#pragma once

#include "Socket.h"
#include "Relay.h"

namespace relay
{
    class HTTPClient
    {
    public:
        HTTPClient(cppsocket::Network& aNetwork,
                   cppsocket::Socket& aSocket,
                   Relay& aRelay);

    private:
        cppsocket::Network& network;
        cppsocket::Socket& socket;
        Relay& relay;
    };
}
