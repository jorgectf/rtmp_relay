//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Receiver.h"
#include "Server.h"
#include "Constants.h"
#include "Amf0.h"
#include "Utils.h"

namespace relay
{
    Receiver::Receiver(Network& pNetwork, Socket pSocket, const std::string& pApplication, const std::shared_ptr<Server>& pServer):
        network(pNetwork), socket(std::move(pSocket)), generator(rd()), application(pApplication), server(pServer)
    {
        socket.setReadCallback(std::bind(&Receiver::handleRead, this, std::placeholders::_1));
        socket.setCloseCallback(std::bind(&Receiver::handleClose, this));
        socket.startRead();
    }

    Receiver::~Receiver()
    {
        
    }

    void Receiver::update()
    {
        
    }

    bool Receiver::getPacket(std::vector<uint8_t>& packet)
    {
        if (data.size())
        {
            packet = data;
            return true;
        }
        
        return false;
    }

    void Receiver::handleRead(const std::vector<uint8_t>& newData)
    {
        data.insert(data.end(), newData.begin(), newData.end());

#ifdef DEBUG
        std::cout << "Receiver got " << std::to_string(newData.size()) << " bytes" << std::endl;
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
                if (data.size() - offset >= sizeof(rtmp::Challenge))
                {
                    // C1
                    rtmp::Challenge* challenge = reinterpret_cast<rtmp::Challenge*>(data.data() + offset);
                    offset += sizeof(*challenge);

#ifdef DEBUG
                    std::cout << "Got challenge message, time: " << challenge->time <<
                        ", version: " << static_cast<uint32_t>(challenge->version[0]) << "." <<
                        static_cast<uint32_t>(challenge->version[1]) << "." <<
                        static_cast<uint32_t>(challenge->version[2]) << "." <<
                        static_cast<uint32_t>(challenge->version[3]) << std::endl;
#endif
                    
                    // S1
                    rtmp::Challenge replyChallenge;
                    replyChallenge.time = 0;
                    memcpy(replyChallenge.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
                    
                    for (size_t i = 0; i < sizeof(replyChallenge.randomBytes); ++i)
                    {
                        replyChallenge.randomBytes[i] = std::uniform_int_distribution<uint8_t>{0, 255}(generator);
                    }
                    
                    std::vector<uint8_t> reply;
                    reply.insert(reply.begin(),
                                 reinterpret_cast<uint8_t*>(&replyChallenge),
                                 reinterpret_cast<uint8_t*>(&replyChallenge) + sizeof(replyChallenge));
                    socket.send(reply);
                    
                    // S2
                    rtmp::Ack ack;
                    ack.time = challenge->time;
                    memcpy(ack.version, challenge->version, sizeof(ack.version));
                    memcpy(ack.randomBytes, challenge->randomBytes, sizeof(ack.randomBytes));
                    
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
                    std::cout << "Got Ack message, time: " << ack->time <<
                        ", version: " << static_cast<uint32_t>(ack->version[0]) << "." <<
                        static_cast<uint32_t>(ack->version[1]) << "." <<
                        static_cast<uint32_t>(ack->version[2]) << "." <<
                        static_cast<uint32_t>(ack->version[3]) << std::endl;
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
                
                uint32_t ret = rtmp::decodePacket(data, offset, inChunkSize, packet, receivedPackets);

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

        if (offset > data.size())
        {
            std::cout << "Reading outside of the buffer, buffer size: " << static_cast<uint32_t>(data.size()) << ", data size: " << offset << std::endl;
        }

        data.erase(data.begin(), data.begin() + offset);

#ifdef DEBUG
        std::cout << "Remaining data " << data.size() << std::endl;
#endif
    }

    void Receiver::handleClose()
    {
        std::cout << "Input disconnected" << std::endl;
    }

    bool Receiver::handlePacket(const rtmp::Packet& packet)
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

            case rtmp::MessageType::NOTIFY:
            {
                // forward notify packet
                if (auto localServer = server.lock())
                {
                    localServer->sendPacket(packet);
                }
                break;
            }

            case rtmp::MessageType::AUDIO_PACKET:
            {
                // forward audio packet
                if (auto localServer = server.lock())
                {
                    localServer->sendPacket(packet);
                }
                break;
            }

            case rtmp::MessageType::VIDEO_PACKET:
            {
                // forward video packet
                if (auto localServer = server.lock())
                {
                    localServer->sendPacket(packet);
                }
                break;
            }

            case rtmp::MessageType::METADATA:
            {
                // forward meta data packet
                if (auto localServer = server.lock())
                {
                    localServer->sendPacket(packet);
                }
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
                    if (argument1["app"].asString() != application)
                    {
                        std::cerr << "Wrong application" << std::endl;
                        socket.close();
                        return false;
                    }

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
                else if (command.asString() == "deleteStream")
                {
                    if (auto localServer = server.lock())
                    {
                        localServer->deleteStream();
                    }
                }
                else if (command.asString() == "FCPublish")
                {
                    sendOnFCPublish();
                    streamName = argument2.asString();
                    if (auto localServer = server.lock())
                    {
                        localServer->createStream(streamName);
                    }
                }
                else if (command.asString() == "FCUnpublish")
                {
                }
                else if (command.asString() == "publish")
                {
                    sendPing();
                    sendPublishStatus(transactionId.asDouble());
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

    void Receiver::sendServerBandwidth()
    {
        rtmp::Packet packet;
        packet.header.channel = rtmp::Channel::NETWORK;
        packet.header.timestamp = 0;
        packet.header.messageType = rtmp::MessageType::SERVER_BANDWIDTH;
        packet.header.messageStreamId = 0;

        encodeInt(packet.data, 4, serverBandwidth);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending SERVER_BANDWIDTH" << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendClientBandwidth()
    {
        rtmp::Packet packet;
        packet.header.channel = rtmp::Channel::NETWORK;
        packet.header.timestamp = 0;
        packet.header.messageType = rtmp::MessageType::CLIENT_BANDWIDTH;

        encodeInt(packet.data, 4, serverBandwidth);
        encodeInt(packet.data, 1, 2); // dynamic

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending CLIENT_BANDWIDTH" << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendPing()
    {
        rtmp::Packet packet;
        packet.header.channel = rtmp::Channel::NETWORK;
        packet.header.timestamp = 0;
        packet.header.messageType = rtmp::MessageType::PING;

        encodeInt(packet.data, 2, 0); // ping type
        encodeInt(packet.data, 4, 0); // ping param

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending PING" << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendSetChunkSize()
    {
        rtmp::Packet packet;
        packet.header.channel = rtmp::Channel::SYSTEM;
        packet.header.timestamp = 0;
        packet.header.messageType = rtmp::MessageType::SET_CHUNK_SIZE;
        packet.header.messageStreamId = 0;

        encodeInt(packet.data, 4, outChunkSize);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending SET_CHUNK_SIZE" << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendConnectResult(double transactionId)
    {
        rtmp::Packet packet;
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
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendBWDone()
    {
        rtmp::Packet packet;
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
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Receiver::sendCheckBWResult(double transactionId)
    {
        rtmp::Packet packet;
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
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendCreateStreamResult(double transactionId)
    {
        rtmp::Packet packet;
        packet.header.channel = rtmp::Channel::SYSTEM;
        packet.header.timestamp = 0;
        packet.header.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("_result");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = transactionId;
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        ++streamId;
        if (streamId == 0 || streamId == 2)
        {
            ++streamId; // Values 0 and 2 are reserved
        }

        amf0::Node argument2 = static_cast<double>(streamId);
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendReleaseStreamResult(double transactionId)
    {
        rtmp::Packet packet;
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
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendOnFCPublish()
    {
        rtmp::Packet packet;
        packet.header.channel = rtmp::Channel::SYSTEM;
        packet.header.timestamp = 0;
        packet.header.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("onFCPublish");
        commandName.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::sendPublishStatus(double transactionId)
    {
        rtmp::Packet packet;
        packet.header.channel = rtmp::Channel::SYSTEM;
        packet.header.timestamp = 0;
        packet.header.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("onStatus");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = transactionId;
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2;
        argument2["clientid"] = std::string("Lavf57.1.0");
        argument2["code"] = std::string("NetStream.Publish.Start");
        argument2["description"] = std::string("wallclock_test_med is now published");
        argument2["details"] = std::string("wallclock_test_med");
        argument2["level"] = std::string("status");
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << std::endl;
#endif

        socket.send(buffer);
    }

    void Receiver::printInfo() const
    {
        std::cout << "\tReceiver " << (socket.isReady() ? "" : "not ") << "connected to: " << ipToString(socket.getIPAddress()) << ":" << socket.getPort() << ", state: ";

        switch (state)
        {
            case rtmp::State::UNINITIALIZED: std::cout << "UNINITIALIZED"; break;
            case rtmp::State::VERSION_RECEIVED: std::cout << "VERSION_RECEIVED"; break;
            case rtmp::State::VERSION_SENT: std::cout << "VERSION_SENT"; break;
            case rtmp::State::ACK_SENT: std::cout << "ACK_SENT"; break;
            case rtmp::State::HANDSHAKE_DONE: std::cout << "HANDSHAKE_DONE"; break;
        }

        std::cout << std::endl;
    }
}
