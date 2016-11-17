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
        Status(cppsocket::Network& aNetwork, Relay& aRelay, const std::string& address);

        Status(const Status&) = delete;
        Status& operator=(const Status&) = delete;

        Status(Status&& other) = delete;
        Status& operator=(Status&& other) = delete;

    private:
        void handleAccept(cppsocket::Socket& clientSocket);
        void handleRead(cppsocket::Socket& clientSocket, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket& clientSocket);

        cppsocket::Network& network;
        cppsocket::Acceptor socket;
        Relay& relay;

        std::vector<cppsocket::Socket> clients;
    };
}
