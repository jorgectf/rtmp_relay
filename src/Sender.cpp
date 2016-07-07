//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include <unistd.h>
#include "Sender.h"
#include "Constants.h"
#include "RTMP.h"
#include "Amf0.h"
#include "Utils.h"

namespace relay
{
    Sender::Sender(Network& pNetwork, const std::string& pApplication, const std::string& pAddress, bool videoOutput, bool audioOutput, bool dataOutput):
        generator(rd()),
        network(pNetwork),
        socket(network),
        application(pApplication),
        address(pAddress),
        videoStream(videoOutput),
        audioStream(audioOutput),
        dataStream(dataOutput)
    {
        socket.setConnectCallback(std::bind(&Sender::handleConnect, this));
        socket.setReadCallback(std::bind(&Sender::handleRead, this, std::placeholders::_1));
        socket.setCloseCallback(std::bind(&Sender::handleClose, this));
    }

    Sender::~Sender()
    {
        
    }

    bool Sender::connect()
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

    void Sender::disconnect()
    {
        std::cerr << "Disconnecting sender" << std::endl;
        socket.disconnect();
        streaming = false;
    }

    void Sender::update()
    {
        
    }

    void Sender::handleConnect()
    {
        std::cout << "Sender connected" << std::endl;

        std::vector<uint8_t> version;
        version.push_back(RTMP_VERSION);
        socket.send(version);
        
        rtmp::Challenge challenge;
        challenge.time = 0;
        memcpy(challenge.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
        
        for (size_t i = 0; i < sizeof(challenge.randomBytes); ++i)
        {
            challenge.randomBytes[i] = std::uniform_int_distribution<uint8_t>{0, 255}(generator);
        }
        
        std::vector<uint8_t> challengeMessage;
        challengeMessage.insert(challengeMessage.begin(),
                                reinterpret_cast<uint8_t*>(&challenge),
                                reinterpret_cast<uint8_t*>(&challenge) + sizeof(challenge));
        socket.send(challengeMessage);
        
        state = rtmp::State::VERSION_SENT;
    }

    bool Sender::sendPacket(const std::vector<uint8_t>& packet)
    {
        socket.send(packet);
        
        return true;
    }

    void Sender::handleRead(const std::vector<uint8_t>& newData)
    {
        data.insert(data.end(), newData.begin(), newData.end());

#ifdef DEBUG
        std::cout << "Sender got " << std::to_string(newData.size()) << " bytes" << std::endl;
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
                        std::cerr << "Unsuported version, disconnecting sender" << std::endl;
                        socket.disconnect();
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
                if (data.size() - offset >= sizeof(rtmp::Challenge))
                {
                    // S1
                    rtmp::Challenge* challenge = reinterpret_cast<rtmp::Challenge*>(data.data() + offset);
                    offset += sizeof(*challenge);

#ifdef DEBUG
                    std::cout << "Got challenge message, time: " << challenge->time <<
                        ", version: " << static_cast<uint32_t>(challenge->version[0]) << "." <<
                        static_cast<uint32_t>(challenge->version[1]) << "." <<
                        static_cast<uint32_t>(challenge->version[2]) << "." <<
                        static_cast<uint32_t>(challenge->version[3]) << std::endl;
#endif
                    
                    // C2
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
            else if (state == rtmp::State::ACK_SENT)
            {
                if (data.size() - offset >= sizeof(rtmp::Ack))
                {
                    // S2
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

    void Sender::handleClose()
    {
        
    }

    bool Sender::handlePacket(const rtmp::Packet& packet)
    {
        switch (packet.messageType)
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

            case rtmp::MessageType::NOTIFY:
            {
                break;
            }

            case rtmp::MessageType::AUDIO_PACKET:
            {
                // ignore this, sender should not receive audio data
                break;
            }

            case rtmp::MessageType::VIDEO_PACKET:
            {
                // ignore this, sender should not receive video data
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
                std::cout << "INVOKE command: ";
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
                else if (command.asString() == "onFCPublish")
                {
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
                            connected = true;
                            if (!streamName.empty())
                            {
                                sendReleaseStream();
                                sendFCPublish();
                                sendCreateStream();
                            }
                        }
                        else if (i->second == "releaseStream")
                        {
                        }
                        else if (i->second == "createStream")
                        {
                            streamId = static_cast<uint32_t>(argument2.asDouble());
                            sendPublish();

                            streaming = true;
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

    void Sender::sendConnect()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.messageStreamId = 0;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("connect");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1;
        argument1["app"] = application;
        argument1["flashVer"] = std::string("FMLE/3.0 (compatible; Lavf57.5.0)");
        argument1["tcUrl"] = std::string("rtmp://127.0.0.1:") + std::to_string(socket.getPort()) + "/" + application;
        argument1["type"] = std::string("nonprivate");
        argument1.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::sendSetChunkSize()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::SET_CHUNK_SIZE;

        encodeInt(packet.data, 4, outChunkSize);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending SET_CHUNK_SIZE" << std::endl;
#endif

        socket.send(buffer);
    }

    void Sender::sendCheckBW()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("_checkbw");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::sendCreateStream()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("createStream");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::sendReleaseStream()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("releaseStream");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2 = streamName;
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::sendDeleteStream()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("deleteStream");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2 = 1.0;
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::sendFCPublish()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("FCPublish");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2 = streamName;
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::sendFCUnpublish()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("FCUnpublish");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2 = streamName;
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::sendPublish()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SOURCE;
        packet.messageStreamId = streamId;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("publish");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2 = streamName;
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

#ifdef DEBUG
        std::cout << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId << std::endl;
#endif

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::printInfo() const
    {
        std::cout << "\tSender " << (socket.isReady() ? "" : "not ") << "connected with: " << ipToString(socket.getIPAddress()) << ":" << socket.getPort() << ", state: ";

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

    void Sender::createStream(const std::string& newStreamName)
    {
        streamName = newStreamName;

        if (connected && !streamName.empty())
        {
            sendReleaseStream();
            sendFCPublish();
            sendCreateStream();
        }
    }

    void Sender::deleteStream()
    {
        if (connected && !streamName.empty())
        {
            sendDeleteStream();
        }

        streaming = false;
    }

    void Sender::unpublishStream()
    {
        if (connected && !streamName.empty())
        {
            sendFCUnpublish();
        }

        streaming = false;
    }

    void Sender::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        if (streaming && audioStream)
        {
            rtmp::Packet packet;
            packet.channel = rtmp::Channel::AUDIO;
            packet.messageStreamId = 1;
            packet.timestamp = timestamp;
            packet.messageType = rtmp::MessageType::AUDIO_PACKET;

            packet.data = audioData;

            std::vector<uint8_t> buffer;
            encodePacket(buffer, outChunkSize, packet, sentPackets);

            socket.send(buffer);
        }
    }

    void Sender::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        if (streaming && videoStream)
        {
            rtmp::Packet packet;
            packet.channel = rtmp::Channel::VIDEO;
            packet.messageStreamId = 1;
            packet.timestamp = timestamp;
            packet.messageType = rtmp::MessageType::VIDEO_PACKET;

            packet.data = videoData;

            std::vector<uint8_t> buffer;
            encodePacket(buffer, outChunkSize, packet, sentPackets);

            socket.send(buffer);
        }
    }

    void Sender::sendMetaData(const amf0::Node& metaData)
    {
        if (streaming)
        {
            rtmp::Packet packet;
            packet.channel = rtmp::Channel::AUDIO;
            packet.messageStreamId = 1;
            packet.timestamp = 0;
            packet.messageType = rtmp::MessageType::NOTIFY;

            amf0::Node commandName = std::string("@setDataFrame");
            commandName.encode(packet.data);

            amf0::Node argument1 = std::string("onMetaData");
            argument1.encode(packet.data);

            amf0::Node argument2 = metaData;
            argument2.encode(packet.data);

            std::vector<uint8_t> buffer;
            encodePacket(buffer, outChunkSize, packet, sentPackets);

            socket.send(buffer);
        }
    }

    void Sender::sendTextData(const amf0::Node& textData)
    {
        if (streaming && dataStream)
        {
            rtmp::Packet packet;
            packet.channel = rtmp::Channel::AUDIO;
            packet.messageStreamId = 1;
            packet.timestamp = 0;
            packet.messageType = rtmp::MessageType::NOTIFY;

            amf0::Node commandName = std::string("onTextData");
            commandName.encode(packet.data);

            amf0::Node argument1 = textData;
            argument1.encode(packet.data);

            amf0::Node argument2 = 0.0;
            argument2.encode(packet.data);

            std::vector<uint8_t> buffer;
            encodePacket(buffer, outChunkSize, packet, sentPackets);
            
            socket.send(buffer);
        }
    }
}
