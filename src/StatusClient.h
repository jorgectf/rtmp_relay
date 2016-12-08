//
//  rtmp_relay
//

#pragma once

#include "Socket.h"

namespace relay
{
    class Relay;

    class StatusClient
    {
    public:
        StatusClient(cppsocket::Network& aNetwork,
                     cppsocket::Socket& aSocket,
                     Relay& aRelay);

        bool isConnected() const { return socket.isReady(); }
        
    private:
        void handleRead(cppsocket::Socket& clientSocket, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket& clientSocket);

        void sendReport();
        void sendError();

        cppsocket::Network& network;
        cppsocket::Socket socket;
        Relay& relay;

        std::vector<uint8_t> data;

        std::string startLine;
        std::vector<std::string> headers;
    };
}
