//
//  rtmp_relay
//

#pragma once

#include "Noncopyable.h"

class Output: public Noncopyable
{
public:
    Output();
    
    int getSocket() const { return _socket; }
    
private:
    int _socket;
};
