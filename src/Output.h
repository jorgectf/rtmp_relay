//
//  rtmp_relay
//

#pragma once

#include <random>
#include <string>
#include <vector>
#include "Socket.h"
#include "RTMP.h"

class Output
{
public:
    Output(Network& network);
    ~Output();
    
    Output(const Output&) = delete;
    Output& operator=(const Output&) = delete;
    
    Output(Output&& other);
    Output& operator=(Output&& other);
    
    bool init(const std::string& address);
    void update();
    void handleConnect();
    
    bool sendPacket(const std::vector<uint8_t>& packet);
    
private:
    void handleRead(const std::vector<uint8_t>& data);
    void handleClose();
    
    Network& _network;
    Socket _socket;
    
    std::vector<uint8_t> _data;
    
    rtmp::State _state = rtmp::State::UNINITIALIZED;
    
    uint32_t _chunkSize = 128;
    
    std::random_device _rd;
    std::mt19937 _generator;
};
