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
    virtual ~Input();
    
    bool init(int serverSocket);
    
    bool readData();
    
    int getSocket() const { return _socket; }
    
private:
    int _socket;
    
    std::vector<char> _data;
    uint32_t _dataSize = 0;
};
