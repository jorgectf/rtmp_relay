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
#include "PullReceiver.h"
#include "PushReceiver.h"

namespace relay
{
    class Application;

    class Server
    {
        const std::string name = "Server";
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

        void getInfo(std::string& str, ReportType reportType) const;

    private:
        void handleAccept(cppsocket::Socket& clientSocket);

        const uint64_t id;

        cppsocket::Network& network;
        cppsocket::Acceptor socket;
        const float pingInterval;
        const std::vector<ApplicationDescriptor> applicationDescriptors;

        std::unique_ptr<PullReceiver> pullReceiver;
        std::vector<std::unique_ptr<PushReceiver>> pushReceivers;
    };
}
