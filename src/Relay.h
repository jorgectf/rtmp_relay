//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <memory>
#include "Noncopyable.h"
#include "Network.h"
#include "Socket.h"

class Server;

class Relay: public Noncopyable
{
public:
    Relay();
    ~Relay();
    
    bool init(const std::string& config);
    
    void run();
    
private:
    Network _network;
    std::vector<std::unique_ptr<Server>> _servers;
};
