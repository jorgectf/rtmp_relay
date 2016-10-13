//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Sender.h"
#include "Constants.h"
#include "RTMP.h"
#include "Amf0.h"
#include "Log.h"
#include "Network.h"
#include "Utils.h"

using namespace cppsocket;

namespace relay
{
    Sender::Sender(Network& pNetwork,
                   const std::string& pApplication,
                   const std::vector<std::string>& pAddresses,
                   bool videoOutput,
                   bool audioOutput,
                   bool dataOutput,
                   const std::set<std::string>& pMetaDataBlacklist,
                   float pConnectionTimeout,
                   float pReconnectInterval,
                   uint32_t pReconnectCount):
        generator(rd()),
        network(pNetwork),
        socket(network),
        application(pApplication),
        addresses(pAddresses),
        videoStream(videoOutput),
        audioStream(audioOutput),
        dataStream(dataOutput),
        metaDataBlacklist(pMetaDataBlacklist),
        connectionTimeout(pConnectionTimeout),
        reconnectInterval(pReconnectInterval),
        reconnectCount(pReconnectCount)
    {
        if (!socket.setBlocking(false))
        {
            Log(Log::Level::ERR) << "[" << name << "] " << "Failed to set socket non-blocking";
        }
        
        socket.setConnectTimeout(connectionTimeout);
        socket.setConnectCallback(std::bind(&Sender::handleConnect, this));
        socket.setReadCallback(std::bind(&Sender::handleRead, this, std::placeholders::_1));
        socket.setCloseCallback(std::bind(&Sender::handleClose, this));

        if (!videoOutput)
        {
            metaDataBlacklist.insert("width");
            metaDataBlacklist.insert("height");
            metaDataBlacklist.insert("videodatarate");
            metaDataBlacklist.insert("framerate");
            metaDataBlacklist.insert("videocodecid");
            metaDataBlacklist.insert("fps");
            metaDataBlacklist.insert("gopsize");
        }

        if (!audioOutput)
        {
            metaDataBlacklist.insert("audiodatarate");
            metaDataBlacklist.insert("audiosamplerate");
            metaDataBlacklist.insert("audiosamplesize");
            metaDataBlacklist.insert("stereo");
            metaDataBlacklist.insert("audiocodecid");
        }
    }

    Sender::~Sender()
    {
        
    }

    void Sender::reset()
    {
        socket.close();
        data.clear();

        state = rtmp::State::UNINITIALIZED;
        inChunkSize = 128;
        outChunkSize = 128;
        connected = false;
        streamName.clear();
        streaming = false;
        invokeId = 0;
        invokes.clear();
    }

    bool Sender::connect()
    {
        reset();
        active = true;

        timeSinceConnect = 0.0f;

        if (addresses.empty())
        {
            Log(Log::Level::ERR) << "[" << name << "] " << "No addresses to connect to";
            return false;
        }

        if (connectCount >= reconnectCount)
        {
            connectCount = 0;
            ++addressIndex;
        }

        if (addressIndex >= addresses.size())
        {
            addressIndex = 0;
        }

        std::string address = addresses[addressIndex];

        if (!socket.connect(address))
        {
            return false;
        }

        ++connectCount;

        return true;
    }

    void Sender::disconnect()
    {
        Log(Log::Level::ERR) << "[" << name << "] " << "Disconnecting";
        reset();
        active = false;
        addressIndex = 0;
        connectCount = 0;
    }

    void Sender::update(float delta)
    {
        if (active && !socket.isReady())
        {
            timeSinceConnect += delta;

            if (timeSinceConnect >= reconnectInterval)
            {
                connect();
            }
        }
    }

    void Sender::handleConnect()
    {
        Log(Log::Level::INFO) << "[" << name << "] " << "Connected";

        std::vector<uint8_t> version;
        version.push_back(RTMP_VERSION);
        socket.send(version);
        
        rtmp::Challenge challenge;
        challenge.time = 0;
        memcpy(challenge.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));
        
        for (size_t i = 0; i < sizeof(challenge.randomBytes); ++i)
        {
            uint32_t randomValue = std::uniform_int_distribution<uint32_t>{ 0, 255 }(generator);

            challenge.randomBytes[i] = static_cast<uint8_t>(randomValue);
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

        Log(Log::Level::ALL) << "[" << name << "] " << "Got " << std::to_string(newData.size()) << " bytes";
        
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

                    Log(Log::Level::ALL) << "[" << name << "] " << "Got version " << static_cast<uint32_t>(version);
                    
                    if (version != 0x03)
                    {
                        Log(Log::Level::ERR) << "[" << name << "] " << "Unsuported version, disconnecting";
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
                if (data.size() - offset >= sizeof(rtmp::Challenge))
                {
                    // S1
                    rtmp::Challenge* challenge = reinterpret_cast<rtmp::Challenge*>(data.data() + offset);
                    offset += sizeof(*challenge);

                    Log(Log::Level::ALL) << "[" << name << "] " << "Got challenge message, time: " << challenge->time <<
                        ", version: " << static_cast<uint32_t>(challenge->version[0]) << "." <<
                        static_cast<uint32_t>(challenge->version[1]) << "." <<
                        static_cast<uint32_t>(challenge->version[2]) << "." <<
                        static_cast<uint32_t>(challenge->version[3]);
                    
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

                    Log(Log::Level::ALL) << "[" << name << "] " << "Got Ack message, time: " << ack->time <<
                        ", version: " << static_cast<uint32_t>(ack->version[0]) << "." <<
                        static_cast<uint32_t>(ack->version[1]) << "." <<
                        static_cast<uint32_t>(ack->version[2]) << "." <<
                        static_cast<uint32_t>(ack->version[3]);
                    Log(Log::Level::ALL) << "[" << name << "] " << "Handshake done";
                    
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
                    Log(Log::Level::ALL) << "[" << name << "] " << "Total packet size: " << ret;

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
            Log(Log::Level::WARN) << "[" << name << "] " << "Reading outside of the buffer, buffer size: " << static_cast<uint32_t>(data.size()) << ", data size: " << offset;
        }

        data.erase(data.begin(), data.begin() + offset);

        Log(Log::Level::ALL) << "[" << name << "] " << "Remaining data " << data.size();
    }

    void Sender::handleClose()
    {
        if (active)
        {
            connect();
        }
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

                Log(Log::Level::ALL) << "[" << name << "] " << "Chunk size: " << inChunkSize;

                sendSetChunkSize();

                break;
            }

            case rtmp::MessageType::BYTES_READ:
            {
                uint32_t offset = 0;
                uint32_t bytesRead;

                uint32_t ret = decodeInt(packet.data, offset, 4, bytesRead);

                if (ret == 0)
                {
                    return false;
                }

                Log(Log::Level::ALL) << "[" << name << "] " << "Bytes read: " << bytesRead;

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

                Log(Log::Level::ALL) << "[" << name << "] " << "Ping type: " << pingType << ", param: " << param;

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

                Log(Log::Level::ALL) << "[" << name << "] " << "Server bandwidth: " << bandwidth;

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

                Log(Log::Level::ALL) << "[" << name << "] " << "Client bandwidth: " << bandwidth << ", type: " << static_cast<uint32_t>(type);
                
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

                Log(Log::Level::ALL) << "[" << name << "] " << "INVOKE command: ";
                command.dump();

                amf0::Node transactionId;

                ret = transactionId.decode(packet.data, offset);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                Log(Log::Level::ALL) << "[" << name << "] " << "Transaction ID: ";
                transactionId.dump();

                amf0::Node argument1;

                if ((ret = argument1.decode(packet.data, offset)) > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << name << "] " << "Argument 1: ";
                    argument1.dump();
                }

                amf0::Node argument2;

                if ((ret = argument2.decode(packet.data, offset)) > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << name << "] " << "Argument 2: ";
                    argument2.dump();
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
                        Log(Log::Level::ALL) << "[" << name << "] " << i->second << " error";

                        invokes.erase(i);
                    }
                }
                else if (command.asString() == "_result")
                {
                    auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                    if (i != invokes.end())
                    {
                        Log(Log::Level::ALL) << "[" << name << "] " << i->second << " result";
                        
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
                Log(Log::Level::ERR) << "[" << name << "] " << "Unhandled message: " << static_cast<uint32_t>(packet.messageType);
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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending SET_CHUNK_SIZE";

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Sender::printInfo() const
    {
        Log log(Log::Level::INFO);
        log << "\t[" << name << "] "<< (socket.isReady() ? "Connected" : "Not connected") << " to: " << ipToString(socket.getIPAddress()) << ":" << socket.getPort() << ", state: ";

        switch (state)
        {
            case rtmp::State::UNINITIALIZED: log << "UNINITIALIZED"; break;
            case rtmp::State::VERSION_RECEIVED: log << "VERSION_RECEIVED"; break;
            case rtmp::State::VERSION_SENT: log << "VERSION_SENT"; break;
            case rtmp::State::ACK_SENT: log << "ACK_SENT"; break;
            case rtmp::State::HANDSHAKE_DONE: log << "HANDSHAKE_DONE"; break;
        }
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

            amf0::Node filteredMetaData = amf0::Marker::ECMAArray;

            for (auto value : metaData.asMap())
            {
                /// not in the blacklist
                if (metaDataBlacklist.find(value.first) == metaDataBlacklist.end())
                {
                    filteredMetaData[value.first] = value.second;
                }
            }

            amf0::Node argument2 = filteredMetaData;
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
