//
//  rtmp_relay
//

#include "PullSender.h"
#include "Relay.h"
#include "Constants.h"
#include "Log.h"
#include "Utils.h"

using namespace cppsocket;

namespace relay
{
    PullSender::PullSender(cppsocket::Socket& aSocket,
                           const std::string& aApplication,
                           const std::string& aOverrideStreamName,
                           bool videoOutput,
                           bool audioOutput,
                           bool dataOutput,
                           const std::set<std::string>& aMetaDataBlacklist,
                           float aPingInterval):
        id(Relay::nextId()),
        socket(std::move(aSocket)),
        application(aApplication),
        overrideStreamName(aOverrideStreamName),
        videoStream(videoOutput),
        audioStream(audioOutput),
        dataStream(dataOutput),
        metaDataBlacklist(aMetaDataBlacklist),
        generator(rd()),
        pingInterval(aPingInterval)
    {
        if (!socket.setBlocking(false))
        {
            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Failed to set socket non-blocking";
        }

        socket.setReadCallback(std::bind(&PullSender::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        socket.setCloseCallback(std::bind(&PullSender::handleClose, this, std::placeholders::_1));

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

    void PullSender::update(float delta)
    {
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

    void PullSender::handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData)
    {
        data.insert(data.end(), newData.begin(), newData.end());

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got " << std::to_string(newData.size()) << " bytes";

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

                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got version " << static_cast<uint32_t>(version);

                    if (version != 0x03)
                    {
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Unsuported version, disconnecting";
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

                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got challenge message, time: " << challenge->time <<
                    ", version: " << static_cast<uint32_t>(challenge->version[0]) << "." <<
                    static_cast<uint32_t>(challenge->version[1]) << "." <<
                    static_cast<uint32_t>(challenge->version[2]) << "." <<
                    static_cast<uint32_t>(challenge->version[3]);

                    // S1
                    rtmp::Challenge replyChallenge;
                    replyChallenge.time = 0;
                    std::copy(RTMP_SERVER_VERSION, RTMP_SERVER_VERSION + sizeof(RTMP_SERVER_VERSION), replyChallenge.version);

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
                    std::copy(challenge->version, challenge->version + sizeof(ack.version), ack.version);
                    std::copy(challenge->randomBytes, challenge->randomBytes + sizeof(ack.randomBytes), ack.randomBytes);

                    std::vector<uint8_t> ackData(reinterpret_cast<uint8_t*>(&ack),
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

                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got Ack message, time: " << ack->time <<
                    ", version: " << static_cast<uint32_t>(ack->version[0]) << "." <<
                    static_cast<uint32_t>(ack->version[1]) << "." <<
                    static_cast<uint32_t>(ack->version[2]) << "." <<
                    static_cast<uint32_t>(ack->version[3]);
                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Handshake done";

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
                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Total packet size: " << ret;

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
            if (socket.isReady())
            {
                Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Reading outside of the buffer, buffer size: " << static_cast<uint32_t>(data.size()) << ", data size: " << offset;
            }
            
            data.clear();
        }
        else
        {
            data.erase(data.begin(), data.begin() + offset);
            
            Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Remaining data " << data.size();
        }
    }

    void PullSender::handleClose(cppsocket::Socket&)
    {
        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getIPAddress()) << ":" << socket.getPort() << " disconnected";
    }

    bool PullSender::handlePacket(const rtmp::Packet& packet)
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

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Chunk size: " << inChunkSize;

                //sendSetChunkSize();

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

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Bytes read: " << bytesRead;

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

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Ping type: " << pingType << ", param: " << param;

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

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Server bandwidth: " << bandwidth;

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

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Client bandwidth: " << bandwidth << ", type: " << static_cast<uint32_t>(type);

                break;
            }

            case rtmp::MessageType::NOTIFY:
            {
                break;
            }

            case rtmp::MessageType::AUDIO_PACKET:
            {
                // ignore this, audio data should not be received
                break;
            }

            case rtmp::MessageType::VIDEO_PACKET:
            {
                // ignore this, video data should not be received
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

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "INVOKE command: ";

                {
                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] ";
                    command.dump(log);
                }

                amf0::Node transactionId;

                ret = transactionId.decode(packet.data, offset);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Transaction ID: ";

                {
                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] ";
                    transactionId.dump(log);
                }

                amf0::Node argument1;

                if ((ret = argument1.decode(packet.data, offset)) > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Argument 1: ";

                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] ";
                    argument1.dump(log);
                }

                amf0::Node argument2;

                if ((ret = argument2.decode(packet.data, offset)) > 0)
                {
                    offset += ret;

                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Argument 2: ";

                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] ";
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

                    connected = true;

                    Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getIPAddress()) << ":" << socket.getPort() << " sent connect, application: \"" << argument1["app"].asString() << "\"";
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
                    // do nothing
                }
                else if (command.asString() == "FCPublish")
                {
                    // this is not a receiver
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent FCPublish to sender, disconnecting";

                    socket.close();
                    return false;
                }
                else if (command.asString() == "FCUnpublish")
                {
                    // this is not a receiver
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent FCUnpublish to sender, disconnecting";

                    socket.close();
                    return false;
                }
                else if (command.asString() == "publish")
                {
                    // this is not a receiver
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent publish to sender, disconnecting";

                    socket.close();
                    return false;
                }
                else if (command.asString() == "getStreamLength")
                {
                    // TODO: implement for streams we know length of
                }
                else if (command.asString() == "play")
                {
                    if (!play(argument2.asString()))
                    {
                        socket.close();
                        return false;
                    }

                    streaming = true;
                    sendPlayStatus(transactionId.asDouble());

                    sendMetaData();
                    sendAudioHeader();
                    sendVideoHeader();
                }
                else if (command.asString() == "stop")
                {
                    streaming = false;
                    sendStopStatus(transactionId.asDouble());
                }
                else if (command.asString() == "_error")
                {
                    auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                    if (i != invokes.end())
                    {
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << i->second << " error";

                        invokes.erase(i);
                    }
                }
                else if (command.asString() == "_result")
                {
                    auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                    if (i != invokes.end())
                    {
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << i->second << " result";
                        
                        invokes.erase(i);
                    }
                }
                break;
            }
                
            default:
            {
                Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Unhandled message: " << static_cast<uint32_t>(packet.messageType);
                break;
            }
        }
        
        return true;
    }

    void PullSender::createStream(const std::string& newStreamName)
    {
        if (overrideStreamName.empty())
        {
            streamName = newStreamName;
        }
        else
        {
            std::map<std::string, std::string> tokens = {
                {"id", std::to_string(id)},
                {"streamName", newStreamName},
                {"applicationName", application},
                {"ipAddress", cppsocket::ipToString(socket.getIPAddress())},
                {"port", std::to_string(socket.getPort())}
            };

            streamName = overrideStreamName;
            replaceTokens(streamName, tokens);
        }
    }

    void PullSender::deleteStream()
    {
        if (connected && !streamName.empty())
        {
            //sendDeleteStream();
        }

        streaming = false;
    }

    void PullSender::unpublishStream()
    {
        if (connected && !streamName.empty())
        {
            //sendFCUnpublish();
        }

        streaming = false;
    }

    void PullSender::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;
        audioHeaderSent = false;

        sendAudioHeader();
    }

    void PullSender::sendAudioHeader()
    {
        if (streaming && !audioHeader.empty() && audioStream)
        {
            sendAudioData(0, audioHeader);
            audioHeaderSent = true;
        }
    }

    void PullSender::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        videoHeader = headerData;
        videoHeaderSent = false;

        sendVideoHeader();
    }

    void PullSender::sendVideoHeader()
    {
        if (streaming && !videoHeader.empty() && videoStream)
        {
            sendVideoData(0, videoHeader);
            videoHeaderSent = true;
        }
    }

    void PullSender::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        if (streaming && audioStream)
        {
            sendAudioData(timestamp, audioData);
        }
    }

    void PullSender::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        if (streaming && videoStream)
        {
            if (videoFrameSent || getVideoFrameType(videoData) == VideoFrameType::KEY)
            {
                sendVideoData(timestamp, videoData);
                videoFrameSent = true;
            }
        }
    }

    void PullSender::sendMetaData(const amf0::Node& newMetaData)
    {
        metaData = newMetaData;
        metaDataSent = false;

        sendMetaData();
    }

    void PullSender::sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::AUDIO;
        packet.messageStreamId = streamId;
        packet.timestamp = timestamp;
        packet.messageType = rtmp::MessageType::AUDIO_PACKET;

        packet.data = audioData;

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending audio packet";

        socket.send(buffer);
    }

    void PullSender::sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::VIDEO;
        packet.messageStreamId = streamId;
        packet.timestamp = timestamp;
        packet.messageType = rtmp::MessageType::VIDEO_PACKET;

        packet.data = videoData;

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending video packet";

        socket.send(buffer);
    }

    void PullSender::sendMetaData()
    {
        if (streaming && metaData.getMarker() != amf0::Marker::Unknown)
        {
            rtmp::Packet packet;
            packet.channel = rtmp::Channel::AUDIO;
            packet.messageStreamId = streamId;
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

            Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending meta data " << commandName.asString() << ":";

            Log log(Log::Level::ALL);
            log << "[" << id << ", " << name << "] ";
            argument2.dump(log);

            socket.send(buffer);

            metaDataSent = true;
        }
    }

    void PullSender::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        if (streaming && dataStream)
        {
            rtmp::Packet packet;
            packet.channel = rtmp::Channel::AUDIO;
            packet.messageStreamId = streamId;
            packet.timestamp = timestamp;
            packet.messageType = rtmp::MessageType::NOTIFY;

            amf0::Node commandName = std::string("onTextData");
            commandName.encode(packet.data);

            amf0::Node argument1 = textData;
            argument1.encode(packet.data);

            std::vector<uint8_t> buffer;
            encodePacket(buffer, outChunkSize, packet, sentPackets);
            
            Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending text data";
            
            Log log(Log::Level::ALL);
            log << "[" << id << ", " << name << "] ";
            argument1.dump(log);
            
            socket.send(buffer);
        }
    }

    void PullSender::sendServerBandwidth()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::NETWORK;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::SERVER_BANDWIDTH;

        encodeInt(packet.data, 4, serverBandwidth);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending SERVER_BANDWIDTH";

        socket.send(buffer);
    }

    void PullSender::sendClientBandwidth()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::NETWORK;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::CLIENT_BANDWIDTH;

        encodeInt(packet.data, 4, serverBandwidth);
        encodeInt(packet.data, 1, 2); // dynamic

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending CLIENT_BANDWIDTH";

        socket.send(buffer);
    }

    void PullSender::sendPing()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::NETWORK;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::PING;

        encodeInt(packet.data, 2, 0); // ping type
        encodeInt(packet.data, 4, 0); // ping param

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending PING";

        socket.send(buffer);
    }

    void PullSender::sendSetChunkSize()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::SET_CHUNK_SIZE;

        encodeInt(packet.data, 4, outChunkSize);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending SET_CHUNK_SIZE";

        socket.send(buffer);
    }

    void PullSender::sendConnectResult(double transactionId)
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void PullSender::sendBWDone()
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void PullSender::sendCheckBWResult(double transactionId)
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void PullSender::sendCreateStreamResult(double transactionId)
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void PullSender::sendReleaseStreamResult(double transactionId)
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void PullSender::sendPlayStatus(double transactionId)
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
        argument2["code"] = std::string("NetStream.Play.Start");
        argument2["description"] = streamName + " is now published";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    void PullSender::sendStopStatus(double transactionId)
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
        argument2["code"] = std::string("NetStream.Play.Stop");
        argument2["description"] = streamName + " is now stopped";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    bool PullSender::connect(const std::string& applicationName)
    {
        if (applicationName == application)
        {
            return true;
        }
        
        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Wrong application";
        
        // failed to connect
        return false;
    }

    bool PullSender::play(const std::string& stream)
    {
        if (stream == streamName)
        {
            return true;
        }

        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Wrong stream";

        // failed to connect
        return false;
    }

    void PullSender::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "\t[" + std::to_string(id) + ", " + name + "] " + (socket.isReady() ? "Connected" : "Not connected") + " to: " + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort()) + ", state: ";

                switch (state)
                {
                    case rtmp::State::UNINITIALIZED: str += "UNINITIALIZED"; break;
                    case rtmp::State::VERSION_RECEIVED: str += "VERSION_RECEIVED"; break;
                    case rtmp::State::VERSION_SENT: str += "VERSION_SENT"; break;
                    case rtmp::State::ACK_SENT: str += "ACK_SENT"; break;
                    case rtmp::State::HANDSHAKE_DONE: str += "HANDSHAKE_DONE"; break;
                }
                str += ", name: " + streamName + "\n";
                break;
            }
            case ReportType::HTML:
            {
                str += "<tr><td>" + std::to_string(id) +"</td><td>" + streamName + "</td><td>" + (socket.isReady() ? "Connected" : "Not connected") + "</td><td>" + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort()) + "</td><td>";

                switch (state)
                {
                    case rtmp::State::UNINITIALIZED: str += "UNINITIALIZED"; break;
                    case rtmp::State::VERSION_RECEIVED: str += "VERSION_RECEIVED"; break;
                    case rtmp::State::VERSION_SENT: str += "VERSION_SENT"; break;
                    case rtmp::State::ACK_SENT: str += "ACK_SENT"; break;
                    case rtmp::State::HANDSHAKE_DONE: str += "HANDSHAKE_DONE"; break;
                }

                str += "</td></tr>";
                break;
            }
            case ReportType::JSON:
            {
                str += "{\"id\":" + std::to_string(id) + ",\"name\":\"" + streamName + "\"," +
                "\"connected\":" + (socket.isReady() ? "true" : "false") + "," +
                "\"address\":\"" + ipToString(socket.getIPAddress()) + ":" + std::to_string(socket.getPort()) + "\"," +
                "\"status\":";

                switch (state)
                {
                    case rtmp::State::UNINITIALIZED: str += "\"UNINITIALIZED\""; break;
                    case rtmp::State::VERSION_RECEIVED: str += "\"VERSION_RECEIVED\""; break;
                    case rtmp::State::VERSION_SENT: str += "\"VERSION_SENT\""; break;
                    case rtmp::State::ACK_SENT: str += "\"ACK_SENT\","; break;
                    case rtmp::State::HANDSHAKE_DONE: str += "\"HANDSHAKE_DONE\""; break;
                }

                str += "}";
                break;
            }
        }
    }
}
