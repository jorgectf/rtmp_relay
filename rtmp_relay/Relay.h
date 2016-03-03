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
    
    void run();
    
private:
    std::vector<Server*> _servers;
};
