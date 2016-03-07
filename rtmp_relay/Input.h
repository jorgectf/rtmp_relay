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
    
    bool readPacket(std::vector<char>& packet);
    
    bool isClosed() const { return _closed; }
    
    int getSocket() const { return _socket; }
    
private:
    int _socket = 0;
    bool _closed = false;
    
    std::vector<char> _data;
    uint32_t _dataSize = 0;
};
