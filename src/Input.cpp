//
//  rtmp_relay
//

#include <iostream>
#include "Input.h"

Input::Input(Network& network, Socket socket):
    _network(network), _socket(std::move(socket))
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
