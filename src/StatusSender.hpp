//
//  rtmp_relay
//

#pragma once

#include "Socket.hpp"

namespace relay
{
    class Relay;

    class StatusSender
    {
    public:
        StatusSender(cppsocket::Network& aNetwork,
                     cppsocket::Socket& aSocket,
                     Relay& aRelay);

        StatusSender(const StatusSender&) = delete;
        StatusSender(StatusSender&&) = delete;
        StatusSender& operator=(const StatusSender&) = delete;
        StatusSender& operator=(StatusSender&&) = delete;

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
