//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Socket.h"
#include "State.h"

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
    
    bool sendPacket(const std::vector<uint8_t>& packet);
    
private:
    void handleRead(const std::vector<uint8_t>& data);
    void handleClose();
    
    Network& _network;
    Socket _socket;
};
