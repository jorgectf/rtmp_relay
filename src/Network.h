//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <memory>
#include "Noncopyable.h"
#include "Socket.h"

class Network: public Noncopyable
{
    friend Socket;
    
public:
    Network();
    
    bool update();
    
protected:
    void addSocket(Socket& socket);
    void removeSocket(Socket& socket);
    
    std::vector<std::reference_wrapper<Socket>> _sockets;
};
