//
//  rtmp_relay
//

#pragma once

#include <random>
#include "Socket.h"

namespace relay
{
    class Connection
    {
        const std::string name = "Connection";
    public:
        enum class Type
        {
            PUSH,
            PULL
        };

        enum class State
        {
            UNINITIALIZED = 0,
            VERSION_RECEIVED = 1,
            VERSION_SENT = 2,
            ACK_SENT = 3,
            HANDSHAKE_DONE = 4
        };

        Connection(cppsocket::Socket& aSocket, Type aType);

        void update();

    private:
        const uint64_t id;

        std::random_device rd;
        std::mt19937 generator;
        
        Type type;
        State state;
        cppsocket::Socket& socket;
    };
}
