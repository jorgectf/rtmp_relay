//
//  rtmp_relay
//

#pragma once

#include "Acceptor.h"
#include "Relay.h"

namespace relay
{
    class Status
    {
    public:
        Status(cppsocket::Network& aNetwork, Relay& aRelay);

    private:
        cppsocket::Network& network;
        cppsocket::Acceptor socket;
        Relay& relay;
    };
}
