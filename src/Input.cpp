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

Input::Input(Input&& other):
    _network(other._network),
    _socket(std::move(other._socket)),
    _data(std::move(other._data))
{
    
}

Input& Input::operator=(Input&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    
    return *this;
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
    if (_data.size())
    {
        packet = _data;
        return true;
    }
    
    return false;
}
