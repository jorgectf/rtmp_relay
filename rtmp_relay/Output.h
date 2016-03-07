//
//  rtmp_relay
//

#pragma once

#include <string>
#include "Noncopyable.h"

class Output: public Noncopyable
{
public:
    Output();
    ~Output();
    
    bool init(const std::string& address);
    
    int getSocket() const { return _socket; }
    
private:
    int _socket = 0;
};
