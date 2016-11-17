//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <memory>
#include "Acceptor.h"
#include "Receiver.h"

namespace relay
{
    class Application;

    class Server
    {
    public:
        Server(cppsocket::Network& aNetwork,
               const std::string& address,
               float aPingInterval,
               const std::vector<ApplicationDescriptor>& aApplicationDescriptors);

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        Server(Server&& other) = delete;
        Server& operator=(Server&& other) = delete;

        void update(float delta);

        void printInfo() const;
        void getInfo(std::string& str) const;

    protected:
        void handleAccept(cppsocket::Socket& clientSocket);

        cppsocket::Network& network;
        cppsocket::Acceptor socket;
        const float pingInterval;
        const std::vector<ApplicationDescriptor> applicationDescriptors;

        std::vector<std::unique_ptr<Receiver>> receivers;
    };
}
