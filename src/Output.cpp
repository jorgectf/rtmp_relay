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

Output::Output(Network& pNetwork):
    network(pNetwork), socket(pNetwork), generator(rd())
{
    socket.setConnectCallback(std::bind(&Output::handleConnect, this));
    socket.setReadCallback(std::bind(&Output::handleRead, this, std::placeholders::_1));
    socket.setCloseCallback(std::bind(&Output::handleClose, this));
}

Output::~Output()
{
    
}

Output::Output(Output&& other):
    network(other.network),
    socket(std::move(other.socket)),
    data(std::move(other.data)),
    state(other.state),
    inChunkSize(other.inChunkSize),
    outChunkSize(other.outChunkSize),
    generator(std::move(other.generator))
{
    other.state = rtmp::State::UNINITIALIZED;
    other.inChunkSize = 128;
    other.outChunkSize = 128;
    
    socket.setConnectCallback(std::bind(&Output::handleConnect, this));
    socket.setReadCallback(std::bind(&Output::handleRead, this, std::placeholders::_1));
    socket.setCloseCallback(std::bind(&Output::handleClose, this));
}

Output& Output::operator=(Output&& other)
{
    socket = std::move(other.socket);
    data = std::move(other.data);
    state = other.state;
    inChunkSize = other.inChunkSize;
    outChunkSize = other.outChunkSize;
    generator = std::move(other.generator);
    
    other.state = rtmp::State::UNINITIALIZED;
    other.inChunkSize = 128;
    other.outChunkSize = 128;
    
    socket.setConnectCallback(std::bind(&Output::handleConnect, this));
    socket.setReadCallback(std::bind(&Output::handleRead, this, std::placeholders::_1));
    socket.setCloseCallback(std::bind(&Output::handleClose, this));
    
    return *this;
}

bool Output::init(const std::string& address)
{
    if (!socket.setBlocking(false))
    {
        std::cerr << "Failed to set socket non-blocking" << std::endl;
        return false;
    }
    
    if (!socket.connect(address))
    {
        return false;
    }
    
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
    socket.send(version);
    
    rtmp::Challange challange;
    challange.time = 0;
    memcpy(challange.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
    
    for (size_t i = 0; i < sizeof(challange.randomBytes); ++i)
    {
        challange.randomBytes[i] = std::uniform_int_distribution<uint8_t>{0, 255}(generator);
    }
    
    std::vector<uint8_t> challangeMessage;
    challangeMessage.insert(challangeMessage.begin(),
                            reinterpret_cast<uint8_t*>(&challange),
                            reinterpret_cast<uint8_t*>(&challange) + sizeof(challange));
    socket.send(challangeMessage);
    
    state = rtmp::State::VERSION_SENT;
}

bool Output::sendPacket(const std::vector<uint8_t>& packet)
{
    socket.send(packet);
    
    return true;
}

void Output::handleRead(const std::vector<uint8_t>& newData)
{
    data.insert(data.end(), newData.begin(), newData.end());

#ifdef DEBUG
    std::cout << "Output got " << std::to_string(newData.size()) << " bytes" << std::endl;
#endif
    
    uint32_t offset = 0;
    
    while (offset < data.size())
    {
        if (state == rtmp::State::VERSION_SENT)
        {
            if (data.size() - offset >= sizeof(uint8_t))
            {
                // S0
                uint8_t version = static_cast<uint8_t>(*data.data() + offset);
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
                
                state = rtmp::State::VERSION_RECEIVED;
            }
            else
            {
                break;
            }
        }
        else if (state == rtmp::State::VERSION_RECEIVED)
        {
            if (data.size() - offset >= sizeof(rtmp::Challange))
            {
                // S1
                rtmp::Challange* challange = reinterpret_cast<rtmp::Challange*>(data.data() + offset);
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
                socket.send(ackData);
                
                state = rtmp::State::ACK_SENT;
            }
            else
            {
                break;
            }
        }
        else if (state == rtmp::State::ACK_SENT)
        {
            if (data.size() - offset >= sizeof(rtmp::Ack))
            {
                // S2
                rtmp::Ack* ack = reinterpret_cast<rtmp::Ack*>(data.data() + offset);
                offset += sizeof(*ack);

#ifdef DEBUG
                std::cout << "Got Ack message, time: " << ack->time << ", time2: " << ack->time2 << std::endl;
                std::cout << "Handshake done" << std::endl;
#endif
                
                state = rtmp::State::HANDSHAKE_DONE;

                sendConnect();
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
}

void Output::handleClose()
{
    
}

bool Output::handlePacket(const rtmp::Packet& packet)
{
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
            sendSetChunkSize();

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

            if ((ret = argument1.decode(packet.data, offset)) > 0)
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

            if (command.asString() == "onBWDone")
            {
                sendCheckBW();
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
                    
                    if (i->second == "connect")
                    {
                        sendReleaseStream();
                        sendFCPublish();
                        sendCreateStream();
                    }
                    else if (i->second == "releaseStream")
                    {
                    }
                    else if (i->second == "createStream")
                    {
                    }

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

void Output::sendConnect()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::TWELVE_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.messageStreamId = 0;
    packet.header.timestamp = 0; //packet.header.timestamp;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("connect");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = static_cast<double>(++invokeId);
    transactionIdNode.encode(packet.data);

    amf0::Node argument1;
    argument1["app"] = std::string("casino/blackjack");
    argument1["flashVer"] = std::string("FMLE/3.0 (compatible; Lavf57.5.0)");
    argument1["tcUrl"] = std::string("rtmp://127.0.0.1:2200/casino/blackjack");
    argument1["type"] = std::string("nonprivate");
    argument1.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

    socket.send(buffer);

    invokes[invokeId] = commandName.asString();
}

void Output::sendSetChunkSize()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::SET_CHUNK_SIZE;

    encodeInt(packet.data, 4, outChunkSize);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending SET_CHUNK_SIZE" << std::endl;
#endif

    socket.send(buffer);
}

void Output::sendCheckBW()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("_checkbw");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = static_cast<double>(++invokeId);
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

    socket.send(buffer);

    invokes[invokeId] = commandName.asString();
}

void Output::sendCreateStream()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("createStream");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = static_cast<double>(++invokeId);
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

    socket.send(buffer);

    invokes[invokeId] = commandName.asString();
}

void Output::sendReleaseStream()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("releaseStream");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = static_cast<double>(++invokeId);
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    amf0::Node argument2 = std::string("wallclock_test_med");
    argument2.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

    socket.send(buffer);

    invokes[invokeId] = commandName.asString();
}

void Output::sendFCPublish()
{
    rtmp::Packet packet;
    packet.header.type = rtmp::Header::Type::EIGHT_BYTE;
    packet.header.channel = rtmp::Channel::SYSTEM;
    packet.header.timestamp = 0;
    packet.header.messageType = rtmp::MessageType::INVOKE;

    amf0::Node commandName = std::string("FCPublish");
    commandName.encode(packet.data);

    amf0::Node transactionIdNode = static_cast<double>(++invokeId);
    transactionIdNode.encode(packet.data);

    amf0::Node argument1(amf0::Marker::Null);
    argument1.encode(packet.data);

    amf0::Node argument2 = std::string("wallclock_test_med");
    argument2.encode(packet.data);

    std::vector<uint8_t> buffer;
    encodePacket(buffer, outChunkSize, packet);

#ifdef DEBUG
    std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

    socket.send(buffer);

    invokes[invokeId] = commandName.asString();
}
