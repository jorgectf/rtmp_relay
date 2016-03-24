//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Input.h"
#include "Constants.h"
#include "Amf0.h"

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
    _state(other._state),
    _chunkSize(other._chunkSize),
    _generator(std::move(other._generator))
{
    other._state = rtmp::State::UNINITIALIZED;
    other._chunkSize = 128;
    
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
}

Input& Input::operator=(Input&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    _state = other._state;
    _chunkSize = other._chunkSize;
    _generator = std::move(other._generator);
    
    other._state = rtmp::State::UNINITIALIZED;
    other._chunkSize = 128;
    
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
    
    std::cout << "Input got " << std::to_string(data.size()) << " bytes" << std::endl;
    
    uint32_t offset = 0;
    
    while (offset < _data.size())
    {
        if (_state == rtmp::State::UNINITIALIZED)
        {
            if (_data.size() - offset >= sizeof(uint8_t))
            {
                // C0
                uint8_t version = static_cast<uint8_t>(*(_data.data() + offset));
                offset += sizeof(version);
                std::cout << "Got version " << version << std::endl;
                
                if (version != 0x03)
                {
                    std::cerr << "Unsuported version" << std::endl;
                    _socket.close();
                    break;
                }
                
                // S0
                std::vector<uint8_t> reply;
                reply.push_back(RTMP_VERSION);
                _socket.send(reply);
                
                _state = rtmp::State::VERSION_SENT;
            }
            else
            {
                break;
            }
        }
        else if (_state == rtmp::State::VERSION_SENT)
        {
            if (_data.size() - offset >= sizeof(rtmp::Challange))
            {
                // C1
                rtmp::Challange* challange = reinterpret_cast<rtmp::Challange*>(_data.data() + offset);
                offset += sizeof(*challange);
                
                std::cout << "Got Challange message, time: " << challange->time <<
                    ", version: " << static_cast<uint32_t>(challange->version[0]) << "." <<
                    static_cast<uint32_t>(challange->version[1]) << "." <<
                    static_cast<uint32_t>(challange->version[2]) << "." <<
                    static_cast<uint32_t>(challange->version[3]) << std::endl;
                
                // S1
                rtmp::Challange replyChallange;
                replyChallange.time = 0;
                memcpy(replyChallange.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
                
                for (size_t i = 0; i < sizeof(replyChallange.randomBytes); ++i)
                {
                    replyChallange.randomBytes[i] = static_cast<uint8_t>(_generator());
                }
                
                std::vector<uint8_t> reply;
                reply.insert(reply.begin(),
                             reinterpret_cast<uint8_t*>(&replyChallange),
                             reinterpret_cast<uint8_t*>(&replyChallange) + sizeof(replyChallange));
                _socket.send(reply);
                
                // S2
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
        else  if (_state == rtmp::State::ACK_SENT)
        {
            if (_data.size() - offset >= sizeof(rtmp::Ack))
            {
                // C2
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
            // TODO: send subscribe
            rtmp::Packet packet;
            
            uint32_t ret = rtmp::decodePacket(data, offset, _chunkSize, packet);
            
            if (ret > 0)
            {
                offset += ret;

                handlePacket(packet);
            }
            else
            {
                break;
            }
        }
    }
    
    _data.erase(_data.begin(), _data.begin() + offset);
}

void Input::handleClose()
{
    std::cout << "Input disconnected" << std::endl;
}

bool Input::handlePacket(const rtmp::Packet& packet)
{
    switch (packet.header.messageType)
    {
        case rtmp::MessageType::SET_CHUNK_SIZE:
        {
            // TODO: set chunk size
            break;
        }

        case rtmp::MessageType::PING:
        {
            // TODO: handle ping
            break;
        }

        case rtmp::MessageType::SERVER_BANDWIDTH:
        {
            break;
        }

        case rtmp::MessageType::CLIENT_BANDWIDTH:
        {
            break;
        }

        case rtmp::MessageType::AUDIO_PACKET:
        {
            // TODO: forward audio packet
            break;
        }

        case rtmp::MessageType::VIDEO_PACKET:
        {
            // TODO: forward video packet
            break;
        }

        case rtmp::MessageType::AMF3_COMMAND:
        {
            std::cerr << "AMF3 commands are not supported" << std::endl;
            break;
        }

        case rtmp::MessageType::INVOKE:
        case rtmp::MessageType::AMF0_COMMAND:
        {
            uint32_t offset = 0;

            amf0::Node command;

            uint32_t ret = command.parseBuffer(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

            std::cout << "Command: " << command.asString() << std::endl;

            std::cout << "Offset: " << offset << std::endl;
            
            amf0::Node streamId;

            ret = streamId.parseBuffer(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

            std::cout << "Stream ID: " << streamId.asDouble() << std::endl;

            std::cout << "Offset: " << offset << std::endl;

            amf0::Node argument;

            ret = streamId.parseBuffer(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

            std::cout << "Argument: " << "Object" << std::endl;
        }

        default:
            std::cerr << "Unhandled message" << std::endl;
    }

    return true;
}
