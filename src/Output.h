//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Socket.h"

class Output
{
public:
    Output() = default;
    Output(Network& network);
    ~Output();
    
    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;
    
    Output(Output&& other);
    Output& operator=(Output&& other);
    
    bool init(const std::string& address);
    void update();
    void connected();
    
    bool sendPacket(const std::vector<char>& packet);
    
private:
    Network& _network;
    Socket _socket;
};
