//
//  rtmp_relay
//

#include <iostream>
#include "Input.h"

Input::Input(Network& network, Socket socket):
    _network(network), _socket(std::move(socket))
{
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
    _socket.startRead();
}

Input::~Input()
{
    
}

Input::Input(Input&& other):
    _network(other._network),
    _socket(std::move(other._socket)),
    _data(std::move(other._data))
{
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
}

Input& Input::operator=(Input&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
    
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

void Input::handleRead(const std::vector<char>& data)
{
    _data.insert(_data.end(), data.begin(), data.end());
    
    std::cout << "Got " << std::to_string(data.size()) << " bytes" << std::endl;
    
    if (_state == State::UNINITIALIZED)
    {
        _socket.send(data);
        _state = State::VERSION_SENT;
    }
}

void Input::handleClose()
{
    std::cout << "Input disconnect!" << std::endl;
}
