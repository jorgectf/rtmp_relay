//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include <unistd.h>
#include "Output.h"
#include "Constants.h"
#include "RTMP.h"
#include "Amf0.h"
#include "Utils.h"

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
    std::cout << "Output connected" << std::endl;

    std::vector<uint8_t> version;
    version.push_back(RTMP_VERSION);
    _socket.send(version);
    
    rtmp::Challange challange;
    challange.time = 0;
    memcpy(challange.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
    
    for (size_t i = 0; i < sizeof(challange.randomBytes); ++i)
    {
        challange.randomBytes[i] = std::uniform_int_distribution<uint8_t>{0, 255}(_generator);
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

#ifdef DEBUG
    std::cout << "Output got " << std::to_string(data.size()) << " bytes" << std::endl;
#endif
    
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

#ifdef DEBUG
                std::cout << "Got version " << version << std::endl;
#endif
                
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

#ifdef DEBUG
                std::cout << "Got Challange message, time: " << challange->time <<
                    ", version: " << static_cast<uint32_t>(challange->version[0]) << "." <<
                    static_cast<uint32_t>(challange->version[1]) << "." <<
                    static_cast<uint32_t>(challange->version[2]) << "." <<
                    static_cast<uint32_t>(challange->version[3]) << std::endl;
#endif
                
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

#ifdef DEBUG
                std::cout << "Got Ack message, time: " << ack->time << ", time2: " << ack->time2 << std::endl;
                std::cout << "Handshake done" << std::endl;
#endif
                
                _state = rtmp::State::HANDSHAKE_DONE;

                sendConnect();
            }
            else
            {
                break;
            }
        }
        else if (_state == rtmp::State::HANDSHAKE_DONE)
        {
            rtmp::Packet packet;
            
            uint32_t ret = rtmp::decodePacket(_data, offset, _chunkSize, packet);
            
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

void Output::handleClose()
{
    
}

bool Output::handlePacket(const rtmp::Packet& packet)
{
#ifdef DEBUG
    std::cout << "Message Type: " << static_cast<uint32_t>(packet.header.messageType) << std::endl;
#endif

    switch (packet.header.messageType)
    {
        case rtmp::MessageType::SET_CHUNK_SIZE:
        {
            uint32_t ret = decodeInt(packet.data, 0, 4, _chunkSize);

            if (ret == 0)
            {
                return false;
            }

#ifdef DEBUG
            std::cout << "Chunk size: " << _chunkSize << std::endl;
#endif

            break;
        }

        case rtmp::MessageType::PING:
        {
            uint32_t offset = 0;

            uint16_t pingType;
            uint32_t ret = decodeInt(packet.data, offset, 2, pingType);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

            uint32_t param;
            ret = decodeInt(packet.data, offset, 4, param);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Ping type: " << pingType << ", param: " << param << std::endl;
#endif
            break;
        }

        case rtmp::MessageType::SERVER_BANDWIDTH:
        {
            uint32_t offset = 0;

            uint32_t bandwidth;
            uint32_t ret = decodeInt(packet.data, offset, 4, bandwidth);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Server bandwidth: " << bandwidth << std::endl;
#endif

            break;
        }

        case rtmp::MessageType::CLIENT_BANDWIDTH:
        {
            uint32_t offset = 0;

            uint32_t bandwidth;
            uint32_t ret = decodeInt(packet.data, offset, 4, bandwidth);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

            uint8_t type;
            ret = decodeInt(packet.data, offset, 1, type);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Client bandwidth: " << bandwidth << ", type: " << static_cast<uint32_t>(type) << std::endl;
#endif
            
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

            uint32_t ret = command.decode(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Command: " << command.asString() << std::endl;
#endif

            amf0::Node streamId;

            ret = streamId.decode(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Stream ID: " << streamId.asDouble() << std::endl;
#endif

            amf0::Node argument;

            ret = argument.decode(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Argument: ";
            argument.dump();
#endif

            if (command.asString() == "connect")
            {
            }
            else if (command.asString() == "publish")
            {
            }
            break;
        }
            
        default:
        {
            std::cerr << "Unhandled message" << std::endl;
            break;
        }
    }

    return true;
}

void Output::sendConnect()
{
    rtmp::Packet resultPacket;

    resultPacket.header.type = rtmp::Header::Type::TWELVE_BYTE;
    resultPacket.header.channel = rtmp::Channel::SYSTEM;
    resultPacket.header.messageStreamId = 0;
    resultPacket.header.timestamp = 0; //packet.header.timestamp;
    resultPacket.header.messageType = rtmp::MessageType::AMF0_COMMAND;

    amf0::Node commandName = std::string("connect");
    commandName.encode(resultPacket.data);

    amf0::Node resultStreamId = static_cast<double>(0);
    resultStreamId.encode(resultPacket.data);

    amf0::Node replyStatus;
    replyStatus["app"] = std::string("casino/blackjack");
    replyStatus["flashVer"] = std::string("FMLE/3.0 (compatible; Lavf57.5.0)");
    replyStatus["tcUrl"] = std::string("rtmp://127.0.0.1:2200/casino/blackjack");
    replyStatus["type"] = std::string("nonprivate");
    replyStatus.encode(resultPacket.data);

    resultPacket.header.length = static_cast<uint32_t>(resultPacket.data.size());

    std::vector<uint8_t> result;
    encodePacket(result, _chunkSize, resultPacket);

    _socket.send(result);
}
