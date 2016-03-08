//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Noncopyable.h"

class Network;

class Socket: public Noncopyable
{
    friend Network;
public:
    Socket(Network& network);
    virtual ~Socket();
    
    bool connect(const std::string& address, uint16_t port = 0);
    bool connect(uint32_t ipAddress, uint16_t port);
    
    bool send(std::vector<char> buffer);
    
    int getSocket() const { return _socket; }
    
    bool setBlocking(bool blocking);
    
    bool isConnecting() const { return _connecting; }
    bool isReady() const { return _ready; }
    
protected:
    virtual bool read();
    virtual bool write();
    
    Network& _network;
    
    int _socket = 0;
    
    std::vector<char> _data;
    uint32_t _dataSize;
    
    bool _connecting = false;
    bool _ready = false;
};
