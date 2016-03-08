//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Noncopyable.h"
#include "Socket.h"

class Output: public Noncopyable
{
public:
    Output(Network& network);
    ~Output();
    
    bool init(const std::string& address);
    void update();
    void connected();
    
    bool sendPacket(const std::vector<char>& packet);
    
private:
    Network& _network;
    Socket _socket;
};
