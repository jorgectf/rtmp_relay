//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <memory>
#include "Network.h"
#include "Socket.h"

namespace relay
{
    class Server;

    class Relay
    {
    public:
        Relay();
        ~Relay();
        
        Relay(const Relay&) = delete;
        Relay& operator=(const Relay&) = delete;
        
        Relay(Relay&&) = delete;
        Relay& operator=(Relay&&) = delete;
        
        bool init(const std::string& config);
        
        void run();

        void printInfo() const;
        
    private:
        cppsocket::Network network;
        std::vector<std::shared_ptr<Server>> servers;
        uint64_t previousTime;
    };
}
