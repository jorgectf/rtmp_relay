//
//  rtmp_relay
//

#pragma once

#include "Socket.h"

namespace relay
{
    class Connection
    {
    public:
        Connection(cppsocket::Socket& aSocket);

        void update();

    private:
        cppsocket::Socket& socket;
    };
}
