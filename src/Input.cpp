//
//  rtmp_relay
//

#include <iostream>
#include "Input.h"

static const uint32_t BUFFER_SIZE = 65536;

Input::Input(Network& network):
    _network(network), _socket(_network)
{
    
}

Input::~Input()
{
    
}

bool Input::init(int serverSocket)
{
    _data.resize(BUFFER_SIZE);
    
    return true;
}

void Input::update()
{
    
}

bool Input::getPacket(std::vector<char>& packet)
{
    if (_dataSize)
    {
        packet.resize(_dataSize);
        std::copy(_data.begin(), _data.begin() + _dataSize, packet.begin());
        _dataSize = 0;
        return true;
    }
    
    return false;
}
