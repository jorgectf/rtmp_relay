//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Noncopyable.h"

class Input: public Noncopyable
{
public:
    Input();
    ~Input();
    
    bool init(int serverSocket);
    
    bool read();
    
    bool getPacket(std::vector<char>& packet);
    
    int getSocket() const { return _socket; }
    
private:
    int _socket = 0;
    bool _closed = false;
    
    std::vector<char> _data;
    uint32_t _dataSize = 0;
};
