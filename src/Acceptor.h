//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Socket.h"

class Acceptor: Socket
{
public:
    Acceptor() = delete;
    Acceptor(Network& network, int sock = 0);
    virtual ~Acceptor();
    
    Acceptor(Acceptor&& other);
    Acceptor& operator=(Acceptor&& other);
    
    bool startAccept(uint16_t port);
    
protected:
    virtual bool read();
    
    uint16_t _port = 0;
    
    std::vector<int> _clientSockets;
};
