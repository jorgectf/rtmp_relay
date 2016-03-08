//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Socket.h"

class Input
{
public:
    Input() = default;
    Input(Network& network, Socket socket);
    ~Input();
    
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    
    Input(Input&& other);
    Input& operator=(Input&& other);
    
    void update();
    
    bool getPacket(std::vector<char>& packet);
    
protected:
    void handleRead(const std::vector<char>& data, bool error);
    
    Network& _network;
    Socket _socket;
    
    std::vector<char> _data;
};
