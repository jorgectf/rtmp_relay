//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Socket.h"

class Acceptor: Socket
{
public:
    Acceptor(Network& network);
    virtual ~Acceptor();
    
    bool startAccept(uint16_t port);
    
protected:
    virtual bool read();
    
    uint16_t _port = 0;
    
    std::vector<int> _sockets;
};
