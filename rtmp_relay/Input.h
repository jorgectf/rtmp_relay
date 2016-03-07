//
//  rtmp_relay
//

#pragma once

#include "Noncopyable.h"

class Input: public Noncopyable
{
public:
    Input();
    virtual ~Input();
    
    bool init(int serverSocket);
    
    int getSocket() const { return _socket; }
    
private:
    int _socket;
};