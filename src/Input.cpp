//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Input.h"
#include "Constants.h"
#include "Amf0.h"
#include "Utils.h"

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
    _inChunkSize(other._inChunkSize),
    _outChunkSize(other._outChunkSize),
    _generator(std::move(other._generator)),
    _timestamp(other._timestamp)
{
    other._state = rtmp::State::UNINITIALIZED;
    other._inChunkSize = 128;
    other._outChunkSize = 128;
    other._timestamp = 0;
    
    _socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    _socket.setCloseCallback(std::bind(&Input::handleClose, this));
}

Input& Input::operator=(Input&& other)
{
    _socket = std::move(other._socket);
    _data = std::move(other._data);
    _state = other._state;
    _inChunkSize = other._inChunkSize;
    _outChunkSize = other._outChunkSize;
    _generator = std::move(other._generator);
    _timestamp = other._timestamp;
    
    other._state = rtmp::State::UNINITIALIZED;
    other._inChunkSize = 128;
    other._outChunkSize = 128;
    other._timestamp = 0;
    
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

#ifdef DEBUG
    std::cout << "Input got " << std::to_string(data.size()) << " bytes" << std::endl;
#endif
    
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

#ifdef DEBUG
                std::cout << "Got version " << static_cast<uint32_t>(version) << std::endl;
#endif
                
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

#ifdef DEBUG
                std::cout << "Got Challange message, time: " << challange->time <<
                    ", version: " << static_cast<uint32_t>(challange->version[0]) << "." <<
                    static_cast<uint32_t>(challange->version[1]) << "." <<
                    static_cast<uint32_t>(challange->version[2]) << "." <<
                    static_cast<uint32_t>(challange->version[3]) << std::endl;
#endif
                
                // S1
                rtmp::Challange replyChallange;
                replyChallange.time = 0;
                memcpy(replyChallange.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
                
                for (size_t i = 0; i < sizeof(replyChallange.randomBytes); ++i)
                {
                    replyChallange.randomBytes[i] = std::uniform_int_distribution<uint8_t>{0, 255}(_generator);
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

#ifdef DEBUG
                std::cout << "Got Ack message, time: " << ack->time << ", time2: " << ack->time2 << std::endl;
                std::cout << "Handshake done" << std::endl;
#endif
                
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
            
            uint32_t ret = rtmp::decodePacket(data, offset, _inChunkSize, packet);

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
    _timestamp = packet.header.timestamp;

    switch (packet.header.messageType)
    {
        case rtmp::MessageType::SET_CHUNK_SIZE:
        {
            uint32_t offset = 0;

            uint32_t ret = decodeInt(packet.data, offset, 4, _inChunkSize);

            if (ret == 0)
            {
                return false;
            }

#ifdef DEBUG
            std::cout << "Chunk size: " << _inChunkSize << std::endl;
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

        case rtmp::MessageType::INVOKE:
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
            std::cout << "Command: ";
            command.dump();
#endif
            
            amf0::Node streamId;

            ret = streamId.decode(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Stream ID: ";
            streamId.dump();
#endif

            amf0::Node argument1;

            if ((ret = argument1.decode(packet.data, offset))  > 0)
            {
                offset += ret;

#ifdef DEBUG
                std::cout << "Argument 1: ";
                argument1.dump();
#endif
            }

            amf0::Node argument2;

            if ((ret = argument2.decode(packet.data, offset)) > 0)
            {
                offset += ret;
            
#ifdef DEBUG
                std::cout << "Argument 2: ";
                argument2.dump();
#endif
            }

            if (command.asString() == "connect")
            {
                sendServerBandwidth();
                sendClientBandwidth();
                sendPing();
                sendSetChunkSize();
                sendConnectResult();
                sendBWDone();
            }
            else if (command.asString() == "_checkbw")
            {
                sendCheckBWResult();
            }
            else if (command.asString() == "publish")
            {
                //startPlaying("casino/blackjack/wallclock_test_med");
                //startPlaying("wallclock_test_med");
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

void Input::sendServerBandwidth()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::TWELVE_BYTE;
    packet.header.channel = rtmp::Channel::NETWORK;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::SERVER_BANDWIDTH;
    packet.header.messageStreamId = 0;

    encodeInt(packet.data, 4, _serverBandwidth);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, _outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending SERVER_BANDWIDTH" << std::endl;
#endif

    _socket.send(buffer);
}

void Input::sendClientBandwidth()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::NETWORK;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::CLIENT_BANDWIDTH;

    encodeInt(packet.data, 4, _serverBandwidth);
    encodeInt(packet.data, 1, 2); // dynamic

    std::vector<uint8_t> buffer;
    encodePacket(buffer, _outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending CLIENT_BANDWIDTH" << std::endl;
#endif

    _socket.send(buffer);
}

void Input::sendPing()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::NETWORK;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::PING;

    encodeInt(packet.data, 2, 0); // ping type
    encodeInt(packet.data, 4, 0); // ping param

    std::vector<uint8_t> buffer;
    encodePacket(buffer, _outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending PING" << std::endl;
#endif

    _socket.send(buffer);
}

void Input::sendSetChunkSize()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::TWELVE_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::SET_CHUNK_SIZE;
    packet.header.messageStreamId = 0;

    encodeInt(packet.data, 4, _outChunkSize);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, _outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending SET_CHUNK_SIZE" << std::endl;
#endif

    _socket.send(buffer);
}

void Input::sendConnectResult()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("_result");
    commandName.encode(packet.data);

    amf0::Node streamId = 0.0;
    streamId.encode(packet.data);

    amf0::Node argument1;
    argument1["fmsVer"] = std::string("FMS/3,0,1,123");
    argument1["capabilities"] = 31.0;
    argument1.encode(packet.data);

    amf0::Node argument2;
    argument2["level"] = std::string("status");
    argument2["code"] = std::string("NetConnection.Connect.Success");
    argument2["description"] = std::string("Connection succeeded.");
    argument2["objectEncoding"] = 0.0;
    argument2.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, _outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE _result" << std::endl;
#endif

    _socket.send(buffer);
}

void Input::sendBWDone()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("onBWDone");
    commandName.encode(packet.data);

    amf0::Node streamId = 0.0;
    streamId.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    amf0::Node argument2 = 0.0;
    argument2.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, _outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE onBWDone" << std::endl;
#endif

    _socket.send(buffer);
}

void Input::sendCheckBWResult()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("_result");
    commandName.encode(packet.data);

    amf0::Node streamId = 0.0;
    streamId.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, _outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE _result" << std::endl;
#endif

    _socket.send(buffer);
}

void Input::startPlaying()
{

}
