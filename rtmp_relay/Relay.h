//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Noncopyable.h"

class Server;

class Relay: public Noncopyable
{
public:
    Relay();
    ~Relay();
    
    bool init();
    
    void run();
    
private:
    std::vector<std::unique_ptr<Server>> _servers;
};
