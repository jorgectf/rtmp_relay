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
    Output(Network& pNetwork);
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
    void handleRead(const std::vector<uint8_t>& newData);
    void handleClose();

    bool handlePacket(const rtmp::Packet& packet);

    void sendConnect();
    void sendSetChunkSize();
    void sendCheckBW();
    
    Network& network;
    Socket socket;
    
    std::vector<uint8_t> data;
    
    rtmp::State state = rtmp::State::UNINITIALIZED;
    
    uint32_t inChunkSize = 128;
    uint32_t outChunkSize = 128;
    
    std::random_device rd;
    std::mt19937 generator;
};
