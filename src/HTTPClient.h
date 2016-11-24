//
//  rtmp_relay
//

#pragma once

#include "Socket.h"

namespace relay
{
    class Relay;

    class HTTPClient
    {
    public:
        HTTPClient(cppsocket::Network& aNetwork,
                   cppsocket::Socket& aSocket,
                   Relay& aRelay);

        bool isConnected() const { return socket.isReady(); }
        
    private:
        void handleRead(cppsocket::Socket& clientSocket, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket& clientSocket);

        cppsocket::Network& network;
        cppsocket::Socket socket;
        Relay& relay;
    };
}
