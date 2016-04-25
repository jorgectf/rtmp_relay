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
    Input(Network& network, Socket socket);
    ~Input();
    
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    
    Input(Input&& other);
    Input& operator=(Input&& other);
    
    void update();
    
    bool getPacket(std::vector<uint8_t>& packet);
    
    bool isConnected() const { return _socket.isReady(); }
    
protected:
    void handleRead(const std::vector<uint8_t>& data);
    void handleClose();

    bool handlePacket(const rtmp::Packet& packet);

    void sendPing();
    void sendResult();
    void sendBWDone();
    void startPlaying(const std::string filename);
    
    Network& _network;
    Socket _socket;
    
    std::vector<uint8_t> _data;
    
    rtmp::State _state = rtmp::State::UNINITIALIZED;
    
    uint32_t _chunkSize = 128;
    
    std::random_device _rd;
    std::mt19937 _generator;

    uint32_t _messageStreamId = 0;
    uint32_t _timestamp;
};
