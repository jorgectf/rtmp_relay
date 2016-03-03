//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include "Noncopyable.h"

class Server: public Noncopyable
{
public:
    Server(uint16_t port, const std::string& outputUrl, uint16_t outputPort);
    ~Server();
    
    void update();
};
