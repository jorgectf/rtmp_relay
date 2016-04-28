//
//  rtmp_relay
//

#pragma once

#include <random>
#include <vector>
#include "Socket.h"
#include "RTMP.h"

class Input
{
public:
    Input(Network& pNetwork, Socket pSocket);
    ~Input();
    
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    
    Input(Input&& other);
    Input& operator=(Input&& other);
    
    void update();
    
    bool getPacket(std::vector<uint8_t>& packet);
    
    bool isConnected() const { return socket.isReady(); }
    
protected:
    void handleRead(const std::vector<uint8_t>& newData);
    void handleClose();

    bool handlePacket(const rtmp::Packet& packet);

    void sendServerBandwidth();
    void sendClientBandwidth();
    void sendPing();
    void sendSetChunkSize();

    void sendConnectResult();
    void sendBWDone();
    void sendCheckBWResult();

    void startPlaying();
    
    Network& network;
    Socket socket;
    
    std::vector<uint8_t> data;
    
    rtmp::State state = rtmp::State::UNINITIALIZED;
    
    uint32_t inChunkSize = 128;
    uint32_t outChunkSize = 128;
    uint32_t serverBandwidth = 2500000;
    
    std::random_device rd;
    std::mt19937 generator;

    uint32_t timestamp;
};
