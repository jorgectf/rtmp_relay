//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include <unistd.h>
#include "Output.h"
#include "Constants.h"

Output::Output(Network& network):
    _network(network), _socket(_network), _generator(_rd())
{
    _socket.setReadCallback(std::bind(&Output::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Output::handleClose, this));
}

Output::~Output()
{
    
}

Output::Output(Output&& other):
    _network(other._network),
    _socket(std::move(other._socket)),
    _data(std::move(other._data)),
    _state(other._state),
    _generator(std::move(other._generator))
{
    other._state = State::UNINITIALIZED;
    
    _socket.setReadCallback(std::bind(&Output::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Output::handleClose, this));
}

Output& Output::operator=(Output&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    _state = other._state;
    _generator = std::move(other._generator);
    
    other._state = State::UNINITIALIZED;
    
    _socket.setReadCallback(std::bind(&Output::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Output::handleClose, this));
    
    return *this;
}

bool Output::init(const std::string& address)
{
    if (!_socket.setBlocking(false))
    {
        std::cerr << "Failed to set socket non-blocking" << std::endl;
        return false;
    }
    
    if (!_socket.connect(address))
    {
        return false;
    }
    
    // TODO: make handshake
    
    return true;
}

void Output::update()
{
    
}

void Output::connected()
{
    std::cout << "Connected" << std::endl;

    std::vector<uint8_t> version;
    version.push_back(RTMP_VERSION);
    _socket.send(version);
    
    Challange challange;
    challange.time = 0;
    memcpy(challange.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
    
    for (size_t i = 0; i < sizeof(challange.randomBytes); ++i)
    {
        challange.randomBytes[i] = static_cast<uint8_t>(_generator());
    }
    
    std::vector<uint8_t> challangeMessage;
    challangeMessage.insert(challangeMessage.begin(),
                            reinterpret_cast<uint8_t*>(&challange),
                            reinterpret_cast<uint8_t*>(&challange) + sizeof(challange));
    _socket.send(challangeMessage);
    
    _state = State::VERSION_SENT;
}

bool Output::sendPacket(const std::vector<uint8_t>& packet)
{
    _socket.send(packet);
    
    return true;
}

void Output::handleRead(const std::vector<uint8_t>& data)
{
    _data.insert(_data.end(), data.begin(), data.end());
    
    std::cout << "Output got " << std::to_string(data.size()) << " bytes" << std::endl;
    
    while (!_data.empty())
    {
        if (_state == State::VERSION_SENT)
        {
            if (_data.size() >= sizeof(uint8_t))
            {
                // S0
                uint8_t version = static_cast<uint8_t>(*_data.data());
                _data.erase(_data.begin(), _data.begin() + sizeof(version));
                std::cout << "Got version " << version << std::endl;
                
                if (version != 0x03)
                {
                    std::cerr << "Unsuported version" << std::endl;
                    _socket.close();
                    break;
                }
                
                _state = State::VERSION_RECEIVED;
            }
            else
            {
                break;
            }
        }
        else if (_state == State::VERSION_RECEIVED)
        {
            if (_data.size() >= sizeof(Challange))
            {
                // S1
                Challange* challange = (Challange*)_data.data();
                _data.erase(_data.begin(), _data.begin() + sizeof(*challange));
                std::cout << "Got Challange message, time: " << challange->time << ", version: " << static_cast<uint32_t>(challange->version[0]) << "." <<
                static_cast<uint32_t>(challange->version[1]) << "." <<
                static_cast<uint32_t>(challange->version[2]) << "." <<
                static_cast<uint32_t>(challange->version[3]) << std::endl;
                
                // C2
                Ack ack;
                ack.time = challange->time;
                ack.time2 = static_cast<uint32_t>(time(nullptr));
                memcpy(ack.randomBytes, challange->randomBytes, sizeof(ack.randomBytes));
                
                std::vector<uint8_t> ackData;
                ackData.insert(ackData.begin(),
                               reinterpret_cast<uint8_t*>(&ack),
                               reinterpret_cast<uint8_t*>(&ack) + sizeof(ack));
                _socket.send(ackData);
                
                _state = State::ACK_SENT;
            }
        }
        else if (_state == State::ACK_SENT)
        {
            if (_data.size() > sizeof(Ack))
            {
                // S2
                Ack* ack = (Ack*)_data.data();
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
            // receive subscribe
            break;
        }
    }
}

void Output::handleClose()
{
    
}
