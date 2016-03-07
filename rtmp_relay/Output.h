//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Noncopyable.h"

class Output: public Noncopyable
{
public:
    Output();
    ~Output();
    
    bool init(const std::string& address);
    void connected();
    
    bool sendPacket(const std::vector<char>& packet);
    
    bool isConnected() const { return _connected; }
    int getSocket() const { return _socket; }
    
private:
    int _socket = 0;
    bool _connected = false;
};
