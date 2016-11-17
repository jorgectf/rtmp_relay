//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include "Receiver.h"
#include "Server.h"
#include "Constants.h"
#include "Amf0.h"
#include "Network.h"
#include "Log.h"
#include "Utils.h"

using namespace cppsocket;

namespace relay
{
    Receiver::Receiver(cppsocket::Network& aNetwork,
                       Socket& aSocket,
                       float aPingInterval,
                       const std::vector<ApplicationDescriptor>& aApplicationDescriptors):
        network(aNetwork), socket(std::move(aSocket)), generator(rd()), pingInterval(aPingInterval), applicationDescriptors(aApplicationDescriptors)
    {
        if (!socket.setBlocking(false))
        {
            Log(Log::Level::ERR) << "[" << name << "] " << "Failed to set socket non-blocking";
        }

        socket.setReadCallback(std::bind(&Receiver::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        socket.setCloseCallback(std::bind(&Receiver::handleClose, this, std::placeholders::_1));
        socket.startRead();
    }

    void Receiver::reset()
    {
        socket.close();
        data.clear();

        receivedPackets.clear();
        sentPackets.clear();

        state = rtmp::State::UNINITIALIZED;
        inChunkSize = 128;
        outChunkSize = 128;
        streamName.clear();
        invokeId = 0;
        invokes.clear();
        receivedPackets.clear();
        sentPackets.clear();
        audioHeader.clear();
        videoHeader.clear();
        metaData = amf0::Node();
        timeSincePing = 0.0f;
    }

    void Receiver::update(float delta)
    {
        if (application) application->update(delta);

        if (socket.isReady() && pingInterval > 0.0f)
        {
            timeSincePing += delta;

            while (timeSincePing >= pingInterval)
            {
                timeSincePing -= pingInterval;
                sendPing();
            }
        }
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

    void Receiver::handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData)
    {
        data.insert(data.end(), newData.begin(), newData.end());

        Log(Log::Level::ALL) << "[" << name << "] " << "Got " << std::to_string(newData.size()) << " bytes";

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

                    Log(Log::Level::ALL) << "[" << name << "] " << "Got version " << static_cast<uint32_t>(version);

                    if (version != 0x03)
                    {
                        Log(Log::Level::ERR) << "[" << name << "] " << "Unsuported version, disconnecting";
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

                    Log(Log::Level::ALL) << "[" << name << "] " << "Got challenge message, time: " << challenge->time <<
                        ", version: " << static_cast<uint32_t>(challenge->version[0]) << "." <<
                        static_cast<uint32_t>(challenge->version[1]) << "." <<
                        static_cast<uint32_t>(challenge->version[2]) << "." <<
                        static_cast<uint32_t>(challenge->version[3]);

                    // S1
                    rtmp::Challenge replyChallenge;
                    replyChallenge.time = 0;
                    memcpy(replyChallenge.version, RTMP_SERVER_VERSION, sizeof(RTMP_SERVER_VERSION));

                    for (size_t i = 0; i < sizeof(replyChallenge.randomBytes); ++i)
                    {
                        uint32_t randomValue = std::uniform_int_distribution<uint32_t>{ 0, 255 }(generator);
                        replyChallenge.randomBytes[i] = static_cast<uint8_t>(randomValue);
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

                    Log(Log::Level::ALL) << "[" << name << "] " << "Got Ack message, time: " << ack->time <<
                        ", version: " << static_cast<uint32_t>(ack->version[0]) << "." <<
                        static_cast<uint32_t>(ack->version[1]) << "." <<
                        static_cast<uint32_t>(ack->version[2]) << "." <<
                        static_cast<uint32_t>(ack->version[3]);
                    Log(Log::Level::ALL) << "[" << name << "] " << "Handshake done";

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
            Log(Log::Level::INFO) << "[" << name << "] " << "Reading outside of the buffer, buffer size: " << static_cast<uint32_t>(data.size()) << ", data size: " << offset;
        }

        data.erase(data.begin(), data.begin() + offset);

        Log(Log::Level::ALL) << "[" << name << "] " << "Remaining data " << data.size();
    }

    void Receiver::handleClose(cppsocket::Socket&)
    {
        if (application)
        {
            application->unpublishStream();
            application->deleteStream();
        }

        reset();

        Log(Log::Level::INFO) << "[" << name << "] " << "Input disconnected";
    }

    bool Receiver::handlePacket(const rtmp::Packet& packet)
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
                uint32_t offset = 0;

                amf0::Node command;

                uint32_t ret = command.decode(packet.data, offset);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                Log(Log::Level::ALL) << "[" << name << "] " << "NOTIFY command: ";

                {
                    Log log(Log::Level::ALL);
                    command.dump(log);
                }

                amf0::Node argument1;

                if ((ret = argument1.decode(packet.data, offset))  > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << name << "] " << "Argument 1: ";

                    Log log(Log::Level::ALL);
                    argument1.dump(log);
                }

                amf0::Node argument2;

                if ((ret = argument2.decode(packet.data, offset)) > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << name << "] " << "Argument 2: ";

                    Log log(Log::Level::ALL);
                    argument2.dump(log);
                }

                if (command.asString() == "@setDataFrame" && argument1.asString() == "onMetaData")
                {
                    metaData = argument2;

                    Log(Log::Level::ALL) << "Audio codec: " << getAudioCodec(static_cast<uint32_t>(argument2["audiocodecid"].asDouble()));
                    Log(Log::Level::ALL) << "Video codec: " << getVideoCodec(static_cast<uint32_t>(argument2["videocodecid"].asDouble()));

                    // forward notify packet
                    if (application) application->sendMetaData(metaData);
                }
                else if (command.asString() == "onTextData")
                {
                    if (application) application->sendTextData(packet.timestamp, argument1);
                }
                break;
            }

            case rtmp::MessageType::AUDIO_PACKET:
            {
                Log log(Log::Level::ALL);
                log << "[" << name << "] " << "Audio packet";
                if (isCodecHeader(packet.data)) log << "(header)";

                if (isCodecHeader(packet.data))
                {
                    audioHeader = packet.data;
                    if (application) application->sendAudioHeader(audioHeader);
                }
                else
                {
                    // forward audio packet
                    if (application) application->sendAudio(packet.timestamp, packet.data);
                }
                break;
            }

            case rtmp::MessageType::VIDEO_PACKET:
            {
                Log log(Log::Level::ALL);
                log << "[" << name << "] " << "Video packet: ";
                switch (getVideoFrameType(packet.data))
                {
                    case VideoFrameType::KEY: log << "key frame"; break;
                    case VideoFrameType::INTER: log << "inter frame"; break;
                    case VideoFrameType::DISPOSABLE: log << "disposable frame"; break;
                    default: log << "unknown frame"; break;
                }

                if (isCodecHeader(packet.data)) log << "(header)";

                if (isCodecHeader(packet.data))
                {
                    videoHeader = packet.data;
                    if (application) application->sendVideoHeader(videoHeader);
                }
                else
                {
                    // forward video packet
                    if (application) application->sendVideo(packet.timestamp, packet.data);
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

                Log(Log::Level::ALL) << "[" << name << "] " << "INVOKE command: ";

                {
                    Log log(Log::Level::ALL);
                    command.dump(log);
                }

                amf0::Node transactionId;

                ret = transactionId.decode(packet.data, offset);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                Log(Log::Level::ALL) << "[" << name << "] " << "Transaction ID: ";

                {
                    Log log(Log::Level::ALL);
                    transactionId.dump(log);
                }

                amf0::Node argument1;

                if ((ret = argument1.decode(packet.data, offset))  > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << name << "] " << "Argument 1: ";

                    Log log(Log::Level::ALL);
                    argument1.dump(log);
                }

                amf0::Node argument2;

                if ((ret = argument2.decode(packet.data, offset)) > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << name << "] " << "Argument 2: ";

                    Log log(Log::Level::ALL);
                    argument2.dump(log);
                }

                if (command.asString() == "connect")
                {
                    if (!connect(argument1["app"].asString()))
                    {
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
                    if (application) application->deleteStream();
                }
                else if (command.asString() == "FCPublish")
                {
                    sendOnFCPublish();
                    streamName = argument2.asString();
                    if (application) application->createStream(streamName);
                }
                else if (command.asString() == "FCUnpublish")
                {
                    if (application) application->unpublishStream();
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
                        Log(Log::Level::ERR) << i->second << " error";

                        invokes.erase(i);
                    }
                }
                else if (command.asString() == "_result")
                {
                    auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                    if (i != invokes.end())
                    {
                        Log(Log::Level::ERR) << i->second << " result";

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

    void Receiver::sendServerBandwidth()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::NETWORK;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::SERVER_BANDWIDTH;
        packet.messageStreamId = 0;

        encodeInt(packet.data, 4, serverBandwidth);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending SERVER_BANDWIDTH";

        socket.send(buffer);
    }

    void Receiver::sendClientBandwidth()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::NETWORK;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::CLIENT_BANDWIDTH;

        encodeInt(packet.data, 4, serverBandwidth);
        encodeInt(packet.data, 1, 2); // dynamic

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending CLIENT_BANDWIDTH";

        socket.send(buffer);
    }

    void Receiver::sendPing()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::NETWORK;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::PING;

        encodeInt(packet.data, 2, 0); // ping type
        encodeInt(packet.data, 4, 0); // ping param

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending PING";

        socket.send(buffer);
    }

    void Receiver::sendSetChunkSize()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::SET_CHUNK_SIZE;
        packet.messageStreamId = 0;

        encodeInt(packet.data, 4, outChunkSize);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending SET_CHUNK_SIZE";

        socket.send(buffer);
    }

    void Receiver::sendConnectResult(double transactionId)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Receiver::sendBWDone()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Receiver::sendCheckBWResult(double transactionId)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("_result");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = transactionId;
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Receiver::sendCreateStreamResult(double transactionId)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Receiver::sendReleaseStreamResult(double transactionId)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("_result");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = transactionId;
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Receiver::sendOnFCPublish()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("onFCPublish");
        commandName.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Receiver::sendPublishStatus(double transactionId)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

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

        Log(Log::Level::ALL) << "[" << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Receiver::printInfo() const
    {
        Log log(Log::Level::INFO);
        log << "\t[" << name << "] " << (socket.isReady() ? "Connected" : "Not connected") << " to: " << ipToString(socket.getIPAddress()) << ":" << socket.getPort() << ", state: ";

        switch (state)
        {
            case rtmp::State::UNINITIALIZED: log << "UNINITIALIZED"; break;
            case rtmp::State::VERSION_RECEIVED: log << "VERSION_RECEIVED"; break;
            case rtmp::State::VERSION_SENT: log << "VERSION_SENT"; break;
            case rtmp::State::ACK_SENT: log << "ACK_SENT"; break;
            case rtmp::State::HANDSHAKE_DONE: log << "HANDSHAKE_DONE"; break;
        }

        log << ", name: " << streamName;

        if (application)
        {
            application->printInfo();
        }
    }

    void Receiver::getInfo(std::string& str) const
    {
        str += "<tr><td>" + streamName + "</td><td>" + (socket.isReady() ? "Connected" : "Not connected") + "</td><td>" + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort()) + "</td><td>";

        switch (state)
        {
            case rtmp::State::UNINITIALIZED: str += "UNINITIALIZED"; break;
            case rtmp::State::VERSION_RECEIVED: str += "VERSION_RECEIVED"; break;
            case rtmp::State::VERSION_SENT: str += "VERSION_SENT"; break;
            case rtmp::State::ACK_SENT: str += "ACK_SENT"; break;
            case rtmp::State::HANDSHAKE_DONE: str += "HANDSHAKE_DONE"; break;
        }

        str += "</td></tr>";

        if (application)
        {
            application->getInfo(str);
        }
    }

    bool Receiver::connect(const std::string& applicationName)
    {
        for (const ApplicationDescriptor& applicationDescriptor : applicationDescriptors)
        {
            if (applicationDescriptor.name.empty() ||
                applicationDescriptor.name == applicationName)
            {
                application.reset(new Application(network, applicationDescriptor, applicationName));

                return true;
            }
        }

        Log(Log::Level::ERR) << "[" << name << "] " << "Wrong application";

        // failed to connect
        return false;
    }
}
