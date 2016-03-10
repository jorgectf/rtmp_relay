//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Input.h"

static const uint8_t VERSION = 3;

Input::Input(Network& network, Socket socket):
    _network(network), _socket(std::move(socket)), _generator(_rd())
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
    _data(std::move(other._data)),
    _generator(std::move(other._generator))
{
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
}

Input& Input::operator=(Input&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    _generator = std::move(other._generator);
    
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
    
    return *this;
}

void Input::update()
{
    
}

bool Input::getPacket(std::vector<uint8_t>& packet)
{
    if (_data.size())
    {
        packet = _data;
        return true;
    }
    
    return false;
}

void Input::handleRead(const std::vector<uint8_t>& data)
{
    _data.insert(_data.end(), data.begin(), data.end());
    
    std::cout << "Got " << std::to_string(data.size()) << " bytes" << std::endl;
    
    while (true)
    {
        if (_state == State::UNINITIALIZED)
        {
            if (_data.size() >= sizeof(uint8_t))
            {
                // C0
                uint8_t version = static_cast<uint8_t>(_data[0]);
                _data.erase(_data.begin(), _data.begin() + sizeof(version));
                std::cout << "Got version " << version << std::endl;
                
                // S0
                std::vector<uint8_t> reply;
                reply.push_back(VERSION);
                _socket.send(reply);
                
                _state = State::VERSION_SENT;
            }
            else
            {
                break;
            }
        }
        else if (_state == State::VERSION_SENT)
        {
            if (_data.size() >= sizeof(Init))
            {
                // C1
                Init* init = (Init*)&_data[0];
                _data.erase(_data.begin(), _data.begin() + sizeof(*init));
                std::cout << "Got Init message, time: " << init->time << ", zero: " << init->zero << std::endl;
                
                // S1
                Init replyInit;
                replyInit.time = 123;
                replyInit.zero = 0;
                
                for (size_t i = 0; i < sizeof(replyInit.randomBytes); ++i)
                {
                    replyInit.randomBytes[i] = static_cast<uint8_t>(_generator() % 255);
                }
                
                std::vector<uint8_t> reply;
                reply.insert(reply.begin(),
                             reinterpret_cast<uint8_t*>(&replyInit),
                             reinterpret_cast<uint8_t*>(&replyInit) + sizeof(replyInit));
                _socket.send(reply);
                
                // S2
                Ack ack;
                ack.time = init->time;
                ack.time2 = static_cast<uint32_t>(time(nullptr));
                memcpy(ack.randomBytes, init->randomBytes, sizeof(ack.randomBytes));
                
                std::vector<uint8_t> ackData;
                ackData.insert(ackData.begin(),
                               reinterpret_cast<uint8_t*>(&ack),
                               reinterpret_cast<uint8_t*>(&ack) + sizeof(ack));
                _socket.send(ackData);
                
                _state = State::ACK_SENT;
            }
            else
            {
                break;
            }
        }
        else  if (_state == State::ACK_SENT)
        {
            if (_data.size() > sizeof(Ack))
            {
                // C2
                Ack* ack = (Ack*)&_data[0];
                _data.erase(_data.begin(), _data.begin() + sizeof(*ack));
                
                std::cout << "Got Ack message, time: " << ack->time << ", time2: " << ack->time2 << std::endl;
                
                std::cout << "Handshake done" << std::endl;
                
                _state = State::HANDSHAKE_DONE;
            }
            else
            {
                break;
            }
        }
        else if (_state == State::HANDSHAKE_DONE)
        {
            // handle packets
            break;
        }
    }
}

void Input::handleClose()
{
    std::cout << "Input disconnect!" << std::endl;
}
