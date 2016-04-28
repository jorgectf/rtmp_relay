//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <memory>
#include "Network.h"
#include "Socket.h"

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
    
private:
    Network network;
    std::vector<Server> servers;
};
