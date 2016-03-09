//
//  rtmp_relay
//

#include <iostream>
#include "Input.h"

static const uint8_t VERSION = 3;

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
    
    while (true)
    {
        if (_state == State::UNINITIALIZED)
        {
            if (_data.size() >= sizeof(Version))
            {
                Version* version = (Version*)&_data[0];
                _data.erase(_data.begin(), _data.begin() + sizeof(Version));
                std::cout << "Got version " << version->version << std::endl;
                
                std::vector<char> versionData;
                versionData.push_back(VERSION);
                _socket.send(versionData);
                
                _state = State::VERSION_SENT;
            }
            else
            {
                break;
            }
        }
        else if (_state == State::VERSION_SENT)
        {
            if (_data.size() >= sizeof(Ack))
            {
                Ack* ack = (Ack*)&_data[0];
                _data.erase(_data.begin(), _data.begin() + sizeof(Ack));
                
                std::cout << "Got Ack, time: " << ack->time << ", zero: " << ack->zero << std::endl;
            }
            else
            {
                break;
            }
        }
        else
        {
            break;
        }
    }
}

void Input::handleClose()
{
    std::cout << "Input disconnect!" << std::endl;
}
