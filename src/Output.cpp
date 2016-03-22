//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include <unistd.h>
#include "Output.h"
#include "Constants.h"
#include "RTMP.h"

Output::Output(Network& network):
    _network(network), _socket(_network), _generator(_rd())
{
    _socket.setConnectCallback(std::bind(&Output::handleConnect, this));
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
    _chunkSize(other._chunkSize),
    _generator(std::move(other._generator))
{
    other._state = rtmp::State::UNINITIALIZED;
    other._chunkSize = 128;
    
    _socket.setConnectCallback(std::bind(&Output::handleConnect, this));
    _socket.setReadCallback(std::bind(&Output::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Output::handleClose, this));
}

Output& Output::operator=(Output&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    _state = other._state;
    _chunkSize = other._chunkSize;
    _generator = std::move(other._generator);
    
    other._state = rtmp::State::UNINITIALIZED;
    other._chunkSize = 128;
    
    _socket.setConnectCallback(std::bind(&Output::handleConnect, this));
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

void Output::handleConnect()
{
    std::cout << "Connected" << std::endl;

    std::vector<uint8_t> version;
    version.push_back(RTMP_VERSION);
    _socket.send(version);
    
    rtmp::Challange challange;
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
    
    _state = rtmp::State::VERSION_SENT;
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
    
    uint32_t offset = 0;
    
    while (offset < _data.size())
    {
        if (_state == rtmp::State::VERSION_SENT)
        {
            if (_data.size() - offset >= sizeof(uint8_t))
            {
                // S0
                uint8_t version = static_cast<uint8_t>(*_data.data() + offset);
                offset += sizeof(version);
                std::cout << "Got version " << version << std::endl;
                
                if (version != 0x03)
                {
                    std::cerr << "Unsuported version" << std::endl;
                    _socket.close();
                    break;
                }
                
                _state = rtmp::State::VERSION_RECEIVED;
            }
            else
            {
                break;
            }
        }
        else if (_state == rtmp::State::VERSION_RECEIVED)
        {
            if (_data.size() - offset >= sizeof(rtmp::Challange))
            {
                // S1
                rtmp::Challange* challange = reinterpret_cast<rtmp::Challange*>(_data.data() + offset);
                offset += sizeof(*challange);
                
                std::cout << "Got Challange message, time: " << challange->time <<
                    ", version: " << static_cast<uint32_t>(challange->version[0]) << "." <<
                    static_cast<uint32_t>(challange->version[1]) << "." <<
                    static_cast<uint32_t>(challange->version[2]) << "." <<
                    static_cast<uint32_t>(challange->version[3]) << std::endl;
                
                // C2
                rtmp::Ack ack;
                ack.time = challange->time;
                ack.time2 = static_cast<uint32_t>(time(nullptr));
                memcpy(ack.randomBytes, challange->randomBytes, sizeof(ack.randomBytes));
                
                std::vector<uint8_t> ackData;
                ackData.insert(ackData.begin(),
                               reinterpret_cast<uint8_t*>(&ack),
                               reinterpret_cast<uint8_t*>(&ack) + sizeof(ack));
                _socket.send(ackData);
                
                _state = rtmp::State::ACK_SENT;
            }
            else
            {
                break;
            }
        }
        else if (_state == rtmp::State::ACK_SENT)
        {
            if (_data.size() - offset >= sizeof(rtmp::Ack))
            {
                // S2
                rtmp::Ack* ack = reinterpret_cast<rtmp::Ack*>(_data.data() + offset);
                offset += sizeof(*ack);
                
                std::cout << "Got Ack message, time: " << ack->time << ", time2: " << ack->time2 << std::endl;
                
                std::cout << "Handshake done" << std::endl;
                
                _state = rtmp::State::HANDSHAKE_DONE;
            }
            else
            {
                break;
            }
        }
        else if (_state == rtmp::State::HANDSHAKE_DONE)
        {
            // TODO: receive subscribe
            rtmp::Packet packet;
            
            uint32_t ret = rtmp::decodePacket(data, offset, _chunkSize, packet);
            
            if (ret > 0)
            {
                offset += ret;
            }
            else
            {
                break;
            }
        }
    }
    
    _data.erase(_data.begin(), _data.begin() + offset);
}

void Output::handleClose()
{
    
}
