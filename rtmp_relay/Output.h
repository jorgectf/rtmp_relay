//
//  rtmp_relay
//

#pragma once

class Output
{
public:
    Output();
    
    int getSocket() const { return _socket; }
    
private:
    int _socket;
};