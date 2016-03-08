//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <functional>
#include "Socket.h"

class Acceptor: Socket
{
public:
    Acceptor() = delete;
    Acceptor(Network& network, int socketFd = -1);
    virtual ~Acceptor();
    
    Acceptor(Acceptor&& other);
    Acceptor& operator=(Acceptor&& other);
    
    bool startAccept(uint16_t port);
    void setAcceptCallback(const std::function<void(Socket)>& acceptCallback);
    
protected:
    virtual bool read();
    
    uint16_t _port = 0;
    
    std::function<void(Socket)> _acceptCallback;
};
