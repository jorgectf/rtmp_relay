//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "Noncopyable.h"

class Server: public Noncopyable
{
public:
    Server(uint16_t port, const std::vector<std::string>& pushUrls);
    ~Server();
    
    void update();
};
