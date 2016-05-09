//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Input.h"
#include "Constants.h"
#include "Amf0.h"
#include "Utils.h"

Input::Input(Network& pNetwork, Socket pSocket):
    network(pNetwork), socket(std::move(pSocket)), generator(rd())
{
    socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    socket.setCloseCallback(std::bind(&Input::handleClose, this));
    socket.startRead();
}

Input::~Input()
{
    
}

Input::Input(Input&& other):
    network(other.network),
    socket(std::move(other.socket)),
    data(std::move(other.data)),
    state(other.state),
    inChunkSize(other.inChunkSize),
    outChunkSize(other.outChunkSize),
    generator(std::move(other.generator)),
    timestamp(other.timestamp)
{
    other.state = rtmp::State::UNINITIALIZED;
    other.inChunkSize = 128;
    other.outChunkSize = 128;
    other.timestamp = 0;
    
    socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    socket.setCloseCallback(std::bind(&Input::handleClose, this));
}

Input& Input::operator=(Input&& other)
{
    socket = std::move(other.socket);
    data = std::move(other.data);
    state = other.state;
    inChunkSize = other.inChunkSize;
    outChunkSize = other.outChunkSize;
    generator = std::move(other.generator);
    timestamp = other.timestamp;
    
    other.state = rtmp::State::UNINITIALIZED;
    other.inChunkSize = 128;
    other.outChunkSize = 128;
    other.timestamp = 0;
    
    socket.setReadCallback(std::bind(&Input::handleRead, this, std::placeholders::_1));
    socket.setCloseCallback(std::bind(&Input::handleClose, this));
    
    return *this;
}

void Input::update()
{
    
}

bool Input::getPacket(std::vector<uint8_t>& packet)
{
    if (data.size())
    {
        packet = data;
        return true;
    }
    
    return false;
}

void Input::handleRead(const std::vector<uint8_t>& newData)
{
    data.insert(data.end(), newData.begin(), newData.end());

#ifdef DEBUG
    std::cout << "Input got " << std::to_string(newData.size()) << " bytes" << std::endl;
#endif
    
    uint32_t offset = 0;
    
    while (offset < data.size())
    {
        if (state == rtmp::State::UNINITIALIZED)
        {
            if (data.size() - offset >= sizeof(uint8_t))
            {
                // C0
                uint8_t version = static_cast<uint8_t>(*(data.data() + offset));
                offset += sizeof(version);

#ifdef DEBUG
                std::cout << "Got version " << static_cast<uint32_t>(version) << std::endl;
#endif
                
                if (version != 0x03)
                {
                    std::cerr << "Unsuported version" << std::endl;
                    socket.close();
                    break;
                }
                
                // S0
                std::vector<uint8_t> reply;
                reply.push_back(RTMP_VERSION);
                socket.send(reply);
                
                state = rtmp::State::VERSION_SENT;
            }
            else
            {
                break;
            }
        }
        else if (state == rtmp::State::VERSION_SENT)
        {
            if (data.size() - offset >= sizeof(rtmp::Challange))
            {
                // C1
                rtmp::Challange* challange = reinterpret_cast<rtmp::Challange*>(data.data() + offset);
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
                    replyChallange.randomBytes[i] = std::uniform_int_distribution<uint8_t>{0, 255}(generator);
                }
                
                std::vector<uint8_t> reply;
                reply.insert(reply.begin(),
                             reinterpret_cast<uint8_t*>(&replyChallange),
                             reinterpret_cast<uint8_t*>(&replyChallange) + sizeof(replyChallange));
                socket.send(reply);
                
                // S2
                rtmp::Ack ack;
                ack.time = challange->time;
                ack.time2 = static_cast<uint32_t>(time(nullptr));
                memcpy(ack.randomBytes, challange->randomBytes, sizeof(ack.randomBytes));
                
                std::vector<uint8_t> ackData;
                ackData.insert(ackData.begin(),
                               reinterpret_cast<uint8_t*>(&ack),
                               reinterpret_cast<uint8_t*>(&ack) + sizeof(ack));
                socket.send(ackData);
                
                state = rtmp::State::ACK_SENT;
            }
            else
            {
                break;
            }
        }
        else  if (state == rtmp::State::ACK_SENT)
        {
            if (data.size() - offset >= sizeof(rtmp::Ack))
            {
                // C2
                rtmp::Ack* ack = reinterpret_cast<rtmp::Ack*>(data.data() + offset);
                offset += sizeof(*ack);

#ifdef DEBUG
                std::cout << "Got Ack message, time: " << ack->time << ", time2: " << ack->time2 << std::endl;
                std::cout << "Handshake done" << std::endl;
#endif
                
                state = rtmp::State::HANDSHAKE_DONE;
            }
            else
            {
                break;
            }
        }
        else if (state == rtmp::State::HANDSHAKE_DONE)
        {
            rtmp::Packet packet;
            
            uint32_t ret = rtmp::decodePacket(data, offset, inChunkSize, packet);

            if (ret > 0)
            {
#ifdef DEBUG
                std::cout << "Total packet size: " << ret << std::endl;
#endif
                
                offset += ret;

                handlePacket(packet);
            }
            else
            {
                break;
            }
        }
    }
    
    data.erase(data.begin(), data.begin() + offset);

#ifdef DEBUG
    std::cout << "Remaining data " << data.size() << std::endl;
#endif
}

void Input::handleClose()
{
    std::cout << "Input disconnected" << std::endl;
}

bool Input::handlePacket(const rtmp::Packet& packet)
{
    timestamp = packet.header.timestamp;

    switch (packet.header.messageType)
    {
        case rtmp::MessageType::SET_CHUNK_SIZE:
        {
            uint32_t offset = 0;

            uint32_t ret = decodeInt(packet.data, offset, 4, inChunkSize);

            if (ret == 0)
            {
                return false;
            }

#ifdef DEBUG
            std::cout << "Chunk size: " << inChunkSize << std::endl;
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
            
            amf0::Node transactionId;

            ret = transactionId.decode(packet.data, offset);

            if (ret == 0)
            {
                return false;
            }

            offset += ret;

#ifdef DEBUG
            std::cout << "Transaction ID: ";
            transactionId.dump();
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
                sendConnectResult(transactionId.asDouble());
                sendBWDone();
            }
            else if (command.asString() == "_checkbw")
            {
                sendCheckBWResult(transactionId.asDouble());
            }
            else if (command.asString() == "createStream")
            {
                sendCreateStreamResult(transactionId.asDouble());
            }
            else if (command.asString() == "releaseStream")
            {
                sendReleaseStreamResult(transactionId.asDouble());
            }
            else if (command.asString() == "FCPublish")
            {
                sendOnFCPublish();
            }
            else if (command.asString() == "_error")
            {
                auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                if (i != invokes.end())
                {
                    std::cerr << i->second << " error" << std::endl;

                    invokes.erase(i);
                }
            }
            else if (command.asString() == "_result")
            {
                auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                if (i != invokes.end())
                {
                    std::cerr << i->second << " result" << std::endl;

                    invokes.erase(i);
                }
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

    encodeInt(packet.data, 4, serverBandwidth);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending SERVER_BANDWIDTH" << std::endl;
#endif

    socket.send(buffer);
}

void Input::sendClientBandwidth()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::NETWORK;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::CLIENT_BANDWIDTH;

    encodeInt(packet.data, 4, serverBandwidth);
    encodeInt(packet.data, 1, 2); // dynamic

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending CLIENT_BANDWIDTH" << std::endl;
#endif

    socket.send(buffer);
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
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending PING" << std::endl;
#endif

    socket.send(buffer);
}

void Input::sendSetChunkSize()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::TWELVE_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::SET_CHUNK_SIZE;
    packet.header.messageStreamId = 0;

    encodeInt(packet.data, 4, outChunkSize);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending SET_CHUNK_SIZE" << std::endl;
#endif

    socket.send(buffer);
}

void Input::sendConnectResult(double transactionId)
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("_result");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = transactionId;
    transactionIdNode.encode(packet.data);

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
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

    socket.send(buffer);
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

    amf0::Node transactionIdNode = static_cast<double>(++invokeId);
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    amf0::Node argument2 = 0.0;
    argument2.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

    socket.send(buffer);

    invokes[invokeId] = commandName.asString();
}

void Input::sendCheckBWResult(double transactionId)
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("_result");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = transactionId;
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

    socket.send(buffer);
}

void Input::sendCreateStreamResult(double transactionId)
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("_result");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = transactionId;
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    amf0::Node argument2 = 1.0;
    argument2.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

    socket.send(buffer);
}

void Input::sendReleaseStreamResult(double transactionId)
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("_result");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = transactionId;
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

    socket.send(buffer);
}

void Input::sendOnFCPublish()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("onFCPublish");
    commandName.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE onFCPublish" << std::endl;
#endif

    socket.send(buffer);
}

void Input::startPlaying()
{

}
