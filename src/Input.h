//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Noncopyable.h"
#include "Socket.h"

class Input: public Noncopyable
{
public:
    Input(Network& network);
    ~Input();
    
    bool init(int serverSocket);
    void update();
    
    bool getPacket(std::vector<char>& packet);
    
private:
    Network& _network;
    Socket _socket;
    
    std::vector<char> _data;
    uint32_t _dataSize = 0;
};
