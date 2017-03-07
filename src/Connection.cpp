//
//  rtmp_relay
//

#include "Connection.h"
#include "Relay.h"
#include "Server.h"
#include "Constants.h"
#include "Utils.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    Connection::Connection(Relay& aRelay, cppsocket::Socket& aSocket, Type aType):
        relay(aRelay),
        id(Relay::nextId()),
        generator(rd()),
        type(aType),
        socket(std::move(aSocket))
    {
        if (!socket.setBlocking(false))
        {
            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Failed to set socket non-blocking";
        }

        socket.setReadCallback(std::bind(&Connection::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        socket.setCloseCallback(std::bind(&Connection::handleClose, this, std::placeholders::_1));
    }

    Connection::Connection(Relay& aRelay,
                           cppsocket::Socket& client):
        Connection(aRelay, client, Type::HOST)
    {
        socket.setBlocking(false);
        socket.startRead();
    }

    Connection::Connection(Relay& aRelay,
                           cppsocket::Socket& connector,
                           const Description& description):
        Connection(aRelay, connector, Type::CLIENT)
    {
        // TODO: implement stream name passing from server
        socket.setBlocking(false);

        addresses = description.addresses;
        connectionTimeout = description.connectionTimeout;
        reconnectInterval = description.reconnectInterval;
        reconnectCount = description.reconnectCount;
        streamType = description.streamType;
        server = description.server;
        overrideApplicationName = description.overrideApplicationName;
        overrideStreamName = description.overrideStreamName;

        if (overrideApplicationName.empty())
        {
            applicationName = description.applicationName;
        }
        else
        {
            std::map<std::string, std::string> tokens = {
                {"id", std::to_string(id)},
                {"applicationName", description.applicationName},
                {"ipAddress", cppsocket::ipToString(socket.getRemoteIPAddress())},
                {"port", std::to_string(socket.getRemotePort())}
            };

            applicationName = overrideApplicationName;
            replaceTokens(applicationName, tokens);
        }

        if (overrideStreamName.empty())
        {
            streamName = description.streamName;
        }
        else
        {
            std::map<std::string, std::string> tokens = {
                {"id", std::to_string(id)},
                {"streamName", description.streamName},
                {"applicationName", description.applicationName},
                {"ipAddress", cppsocket::ipToString(socket.getRemoteIPAddress())},
                {"port", std::to_string(socket.getRemotePort())}
            };

            streamName = overrideStreamName;
            replaceTokens(streamName, tokens);
        }

        if (!addresses.empty())
        {
            socket.setConnectTimeout(connectionTimeout);
            socket.setConnectCallback(std::bind(&Connection::handleConnect, this, std::placeholders::_1));
            socket.setConnectErrorCallback(std::bind(&Connection::handleConnectError, this, std::placeholders::_1));
            socket.connect(addresses[0].first, addresses[0].second);
        }
    }

    Connection::~Connection()
    {
        if (server)
        {
            server->stopReceiving(*this);
            server->stopStreaming(*this);
        }
    }

    void Connection::update(float delta)
    {
        if (type == Type::HOST)
        {
            if (connected && pingInterval > 0.0f)
            {
                timeSincePing += delta;

                if (timeSincePing >= pingInterval)
                {
                    timeSincePing = 0.0f;
                    sendPing();
                }
            }
        }
        else if (type == Type::CLIENT)
        {
            if (socket.isReady() && state != State::HANDSHAKE_DONE)
            {
                timeSinceConnect = 0.0f;
            }
            else
            {
                timeSinceConnect += delta;

                if (timeSinceConnect >= reconnectInterval)
                {
                    timeSinceConnect = 0.0f;
                    state = State::UNINITIALIZED;

                    if (connectCount >= reconnectCount)
                    {
                        connectCount = 0;
                        ++addressIndex;
                    }

                    if (addressIndex >= addresses.size())
                    {
                        addressIndex = 0;
                    }

                    if (addressIndex < addresses.size())
                    {
                        socket.connect(addresses[addressIndex].first, addresses[addressIndex].second);
                    }
                }
            }
        }

        timeSinceMeasure += delta;

        if (timeSinceMeasure >= 1.0f)
        {
            timeSinceMeasure = 0.0f;
            audioRate = currentAudioBytes;
            videoRate = currentVideoBytes;

            currentAudioBytes = 0;
            currentVideoBytes = 0;
        }
    }

    void Connection::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "\t[" + std::to_string(id) + ", " + name + "] " + (socket.isReady() ? "Connected" : "Not connected") +
                    " to: " + ipToString(socket.getRemoteIPAddress()) + ":" + std::to_string(socket.getRemotePort()) +
                    ", connection type: ";

                switch (type)
                {
                    case Type::HOST: str += "HOST"; break;
                    case Type::CLIENT: str += "CLIENT"; break;
                }

                str += ", state: ";

                switch (state)
                {
                    case State::UNINITIALIZED: str += "UNINITIALIZED"; break;
                    case State::VERSION_RECEIVED: str += "VERSION_RECEIVED"; break;
                    case State::VERSION_SENT: str += "VERSION_SENT"; break;
                    case State::ACK_SENT: str += "ACK_SENT"; break;
                    case State::HANDSHAKE_DONE: str += "HANDSHAKE_DONE"; break;
                }
                str += ", application: " + applicationName +
                    ", name: " + streamName;

                str += ", stream type: ";

                switch (streamType)
                {
                    case StreamType::NONE: str += "NONE"; break;
                    case StreamType::INPUT: str += "INPUT"; break;
                    case StreamType::OUTPUT: str += "OUTPUT"; break;
                }

                str += "\n";

                break;
            }
            case ReportType::HTML:
            {
                str += "<tr><td>" + std::to_string(id) +"</td><td>" + streamName + "</td><td>" + (socket.isReady() ? "Connected" : "Not connected") + "</td><td>" + ipToString(socket.getRemoteIPAddress()) + ":" + std::to_string(socket.getRemotePort()) + "</td><td>";

                switch (type)
                {
                    case Type::HOST: str += "HOST"; break;
                    case Type::CLIENT: str += "CLIENT"; break;
                }

                str += "</td><td>";

                switch (state)
                {
                    case State::UNINITIALIZED: str += "UNINITIALIZED"; break;
                    case State::VERSION_RECEIVED: str += "VERSION_RECEIVED"; break;
                    case State::VERSION_SENT: str += "VERSION_SENT"; break;
                    case State::ACK_SENT: str += "ACK_SENT"; break;
                    case State::HANDSHAKE_DONE: str += "HANDSHAKE_DONE"; break;
                }

                str += "</td><td>";

                switch (streamType)
                {
                    case StreamType::NONE: str += "NONE"; break;
                    case StreamType::INPUT: str += "INPUT"; break;
                    case StreamType::OUTPUT: str += "OUTPUT"; break;
                }

                str += "</td></tr>";
                break;
            }
            case ReportType::JSON:
            {
                str += "{\"id\":" + std::to_string(id)  +",\"name\":\"" + streamName + "\"," +
                    "\"connected\":" + (socket.isReady() ? "true" : "false") + "," +
                    "\"address\":\"" + ipToString(socket.getRemoteIPAddress()) + ":" + std::to_string(socket.getRemotePort()) + "\"," +
                    "\"connectionType:\"";

                switch (type)
                {
                    case Type::HOST: str += "\"HOST\""; break;
                    case Type::CLIENT: str += "\"CLIENT\""; break;
                }

                str += ",\"status\":";

                switch (state)
                {
                    case State::UNINITIALIZED: str += "\"UNINITIALIZED\""; break;
                    case State::VERSION_RECEIVED: str += "\"VERSION_RECEIVED\""; break;
                    case State::VERSION_SENT: str += "\"VERSION_SENT\""; break;
                    case State::ACK_SENT: str += "\"ACK_SENT\","; break;
                    case State::HANDSHAKE_DONE: str += "\"HANDSHAKE_DONE\""; break;
                }

                str += ",\"streamType\":";

                switch (streamType)
                {
                    case StreamType::NONE: str += "\"NONE\""; break;
                    case StreamType::INPUT: str += "\"INPUT\""; break;
                    case StreamType::OUTPUT: str += "\"OUTPUT\""; break;
                }

                str += "}";
                break;
            }
        }
    }

    void Connection::handleConnect(cppsocket::Socket&)
    {
        // handshake
        if (type == Type::CLIENT)
        {
            Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Connected to " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort();

            std::vector<uint8_t> version;
            version.push_back(RTMP_VERSION);
            socket.send(version);

            rtmp::Challenge challenge;
            challenge.time = 0;
            std::copy(RTMP_SERVER_VERSION, RTMP_SERVER_VERSION + sizeof(RTMP_SERVER_VERSION), challenge.version);

            for (size_t i = 0; i < sizeof(challenge.randomBytes); ++i)
            {
                uint32_t randomValue = std::uniform_int_distribution<uint32_t>{0, 255}(generator);

                challenge.randomBytes[i] = static_cast<uint8_t>(randomValue);
            }

            std::vector<uint8_t> challengeMessage;
            challengeMessage.insert(challengeMessage.begin(),
                                    reinterpret_cast<uint8_t*>(&challenge),
                                    reinterpret_cast<uint8_t*>(&challenge) + sizeof(challenge));
            socket.send(challengeMessage);

            state = State::VERSION_SENT;
        }
    }

    void Connection::handleConnectError(cppsocket::Socket&)
    {
    }

    void Connection::handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData)
    {
        data.insert(data.end(), newData.begin(), newData.end());

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got " << std::to_string(newData.size()) << " bytes";

        uint32_t offset = 0;

        while (offset < data.size())
        {
            if (state == State::HANDSHAKE_DONE)
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
            else if (type == Type::HOST)
            {
                if (state == State::UNINITIALIZED)
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

                        state = State::VERSION_SENT;
                    }
                    else
                    {
                        break;
                    }
                }
                else if (state == State::VERSION_SENT)
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
                            uint32_t randomValue = std::uniform_int_distribution<uint32_t>{0, 255}(generator);
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

                        state = State::ACK_SENT;
                    }
                    else
                    {
                        break;
                    }
                }
                else  if (state == State::ACK_SENT)
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

                        state = State::HANDSHAKE_DONE;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else if (type == Type::CLIENT)
            {
                if (state == State::VERSION_SENT)
                {
                    if (data.size() - offset >= sizeof(uint8_t))
                    {
                        // S0
                        uint8_t version = static_cast<uint8_t>(*data.data() + offset);
                        offset += sizeof(version);

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got version " << static_cast<uint32_t>(version);

                        if (version != 0x03)
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Unsuported version, disconnecting";
                            socket.close();
                            break;
                        }

                        state = State::VERSION_RECEIVED;
                    }
                    else
                    {
                        break;
                    }
                }
                else if (state == State::VERSION_RECEIVED)
                {
                    if (data.size() - offset >= sizeof(rtmp::Challenge))
                    {
                        // S1
                        rtmp::Challenge* challenge = reinterpret_cast<rtmp::Challenge*>(data.data() + offset);
                        offset += sizeof(*challenge);

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got challenge message, time: " << challenge->time <<
                        ", version: " << static_cast<uint32_t>(challenge->version[0]) << "." <<
                        static_cast<uint32_t>(challenge->version[1]) << "." <<
                        static_cast<uint32_t>(challenge->version[2]) << "." <<
                        static_cast<uint32_t>(challenge->version[3]);

                        // C2
                        rtmp::Ack ack;
                        ack.time = challenge->time;
                        std::copy(challenge->version, challenge->version + sizeof(ack.version), ack.version);
                        std::copy(challenge->randomBytes, challenge->randomBytes + sizeof(ack.randomBytes), ack.randomBytes);

                        std::vector<uint8_t> ackData(reinterpret_cast<uint8_t*>(&ack),
                                                     reinterpret_cast<uint8_t*>(&ack) + sizeof(ack));
                        socket.send(ackData);

                        state = State::ACK_SENT;
                    }
                    else
                    {
                        break;
                    }
                }
                else if (state == State::ACK_SENT)
                {
                    if (data.size() - offset >= sizeof(rtmp::Ack))
                    {
                        // S2
                        rtmp::Ack* ack = reinterpret_cast<rtmp::Ack*>(data.data() + offset);
                        offset += sizeof(*ack);

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got Ack message, time: " << ack->time <<
                        ", version: " << static_cast<uint32_t>(ack->version[0]) << "." <<
                        static_cast<uint32_t>(ack->version[1]) << "." <<
                        static_cast<uint32_t>(ack->version[2]) << "." <<
                        static_cast<uint32_t>(ack->version[3]);
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Handshake done";
                        
                        state = State::HANDSHAKE_DONE;

                        sendConnect();
                    }
                    else
                    {
                        break;
                    }
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

    void Connection::handleClose(cppsocket::Socket&)
    {
        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " disconnected";
    }

    bool Connection::handlePacket(const rtmp::Packet& packet)
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

                if (type == Type::CLIENT)
                {
                    sendSetChunkSize();
                }

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

                uint8_t bandwidthType;
                ret = decodeInt(packet.data, offset, 1, bandwidthType);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Client bandwidth: " << bandwidth << ", type: " << bandwidthType;

                break;
            }

            case rtmp::MessageType::NOTIFY:
            {
                // only input can receive notify packets
                if (streamType == StreamType::INPUT)
                {
                    uint32_t offset = 0;

                    amf0::Node command;

                    uint32_t ret = command.decode(packet.data, offset);

                    if (ret == 0)
                    {
                        return false;
                    }

                    offset += ret;

                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "NOTIFY command: ";

                    {
                        Log log(Log::Level::ALL);
                        log << "[" << id << ", " << name << "] ";
                        command.dump(log);
                    }

                    amf0::Node argument1;

                    if ((ret = argument1.decode(packet.data, offset))  > 0)
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

                    if (command.asString() == "@setDataFrame" && argument1.asString() == "onMetaData")
                    {
                        Log(Log::Level::ALL) << "Audio codec: " << getAudioCodec(static_cast<uint32_t>(argument2["audiocodecid"].asDouble()));
                        Log(Log::Level::ALL) << "Video codec: " << getVideoCodec(static_cast<uint32_t>(argument2["videocodecid"].asDouble()));

                        // forward notify packet
                        if (server) server->sendMetaData(argument2);
                    }
                    else if (command.asString() == "onMetaData")
                    {
                        Log(Log::Level::ALL) << "Audio codec: " << getAudioCodec(static_cast<uint32_t>(argument1["audiocodecid"].asDouble()));
                        Log(Log::Level::ALL) << "Video codec: " << getVideoCodec(static_cast<uint32_t>(argument1["videocodecid"].asDouble()));

                        // forward notify packet
                        if (server) server->sendMetaData(argument2);
                    }
                    else if (command.asString() == "onTextData")
                    {
                        if (server) server->sendTextData(packet.timestamp, argument1);
                    }
                }
                else
                {
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent notify packet to sender, disconnecting";
                    socket.close();
                    return false;
                }
                break;
            }

            case rtmp::MessageType::AUDIO_PACKET:
            {
                // only input can receive audio packets
                if (streamType == StreamType::INPUT)
                {
                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] " << "Audio packet";
                    if (isCodecHeader(packet.data)) log << "(header)";

                    currentAudioBytes += packet.data.size();

                    if (packet.data.empty() && isCodecHeader(packet.data))
                    {
                        if (server) server->sendAudioHeader(packet.data);
                    }
                    else
                    {
                        // forward audio packet
                        if (server) server->sendAudio(packet.timestamp, packet.data);
                    }
                }
                else
                {
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent audio packet to sender, disconnecting";
                    socket.close();
                    return false;
                }
                break;
            }

            case rtmp::MessageType::VIDEO_PACKET:
            {
                // only input can receive video packets
                if (streamType == StreamType::INPUT)
                {
                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] " << "Video packet: ";
                    switch (getVideoFrameType(packet.data))
                    {
                        case VideoFrameType::KEY: log << "key frame"; break;
                        case VideoFrameType::INTER: log << "inter frame"; break;
                        case VideoFrameType::DISPOSABLE: log << "disposable frame"; break;
                        default: log << "unknown frame"; break;
                    }

                    if (isCodecHeader(packet.data)) log << "(header)";

                    currentVideoBytes += packet.data.size();

                    if (packet.data.empty() && isCodecHeader(packet.data))
                    {
                        if (server) server->sendVideoHeader(packet.data);
                    }
                    else
                    {
                        // forward video packet
                        if (server) server->sendVideo(packet.timestamp, packet.data);
                    }
                }
                else
                {
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent video packet to sender, disconnecting";
                    socket.close();
                    return false;
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
                    if (type == Type::HOST)
                    {
                        applicationName = argument1["app"].asString();

                        sendServerBandwidth();
                        sendClientBandwidth();
                        sendPing();
                        sendSetChunkSize();
                        sendConnectResult(transactionId.asDouble());
                        sendOnBWDone();

                        connected = true;

                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " sent connect, application: \"" << argument1["app"].asString() << "\"";
                    }
                    else
                    {
                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Invalid message (\"connect\") received, disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "onBWDone")
                {
                    if (type == Type::CLIENT)
                    {
                        sendCheckBW();
                    }
                    else
                    {
                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Invalid message (\"onBWDone\"), disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "_checkbw")
                {
                    if (type == Type::HOST)
                    {
                        sendCheckBWResult(transactionId.asDouble());
                    }
                    else
                    {
                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Invalid message (\"_checkbw\"), disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "createStream")
                {
                    if (type == Type::HOST)
                    {
                        sendCreateStreamResult(transactionId.asDouble());
                    }
                    else
                    {
                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Invalid message (\"createStream\"), disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "releaseStream")
                {
                    if (type == Type::HOST)
                    {
                        sendReleaseStreamResult(transactionId.asDouble());
                    }
                    else
                    {
                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Invalid message (\"releaseStream\"), disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "deleteStream")
                {
                    if (type == Type::HOST)
                    {
                        if (server)
                        {
                            server->stopReceiving(*this);
                            server->stopStreaming(*this);
                            server = nullptr;
                        }
                    }
                    else
                    {
                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Invalid message (\"deleteStream\"), disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "FCPublish")
                {
                    if (streamType == StreamType::NONE ||
                        streamType == StreamType::INPUT)
                    {
                        sendOnFCPublish();
                    }
                    else if (streamType == StreamType::OUTPUT)
                    {
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"FCPublish\") received, disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "onFCPublish")
                {
                }
                else if (command.asString() == "FCUnpublish")
                {
                    if (streamType == StreamType::INPUT)
                    {
                        streamType = StreamType::NONE;

                        if (server)
                        {
                            server->stopReceiving(*this);
                            server->stopStreaming(*this);
                            server = nullptr;
                        }

                        sendOnFCUnpublish();

                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " unpublished stream \"" << streamName << "\"";

                        streamName.clear();
                    }
                    else
                    {
                        // this is not a receiver
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"FCUnpublish\") received, disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "onFCUnpublish")
                {
                    if (streamType == StreamType::INPUT)
                    {
                        // Do nothing
                    }
                    else
                    {
                        // this is not a receiver
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"onFCUnpublish\") received, disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "publish")
                {
                    if (streamType == StreamType::NONE ||
                        streamType == StreamType::INPUT)
                    {
                        streamType = StreamType::INPUT;
                        streamName = argument2.asString();

                        const Description* connectionDescription = relay.getConnectionDescription(std::make_pair(socket.getLocalIPAddress(), socket.getLocalPort()), streamType, applicationName, streamName);

                        if (connectionDescription)
                        {
                            sendPing();
                            sendPublishStatus(transactionId.asDouble());

                            server = connectionDescription->server;
                            pingInterval = connectionDescription->pingInterval;

                            server->startStreaming(*this);

                            Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " published stream \"" << streamName << "\"";
                        }
                        else
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid stream \"" << applicationName << "/" << streamName << "\", disconnecting";
                            socket.close();
                            return false;
                        }
                    }
                    else if (streamType == StreamType::OUTPUT)
                    {
                        // this is not a receiver
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"publish\") received, disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "unpublish")
                {
                    if (streamType == StreamType::INPUT)
                    {
                        streamType = StreamType::NONE;

                        if (server)
                        {
                            server->stopReceiving(*this);
                            server->stopStreaming(*this);
                            server = nullptr;
                        }

                        sendUnublishStatus(transactionId.asDouble());

                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " unpublished stream \"" << streamName << "\"";

                        streamName.clear();
                    }
                    else
                    {
                        // this is not a receiver
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"FCUnpublish\") received, disconnecting";
                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "play")
                {
                    if (streamType == StreamType::NONE)
                    {
                        streamType = StreamType::OUTPUT;
                        streamName = argument2.asString();

                        const Description* connectionDescription = relay.getConnectionDescription(std::make_pair(socket.getLocalIPAddress(), socket.getLocalPort()), streamType, applicationName, streamName);

                        if (connectionDescription)
                        {
                            sendPlayStatus(transactionId.asDouble());

                            server = connectionDescription->server;
                            server->startReceiving(*this);
                            pingInterval = connectionDescription->pingInterval;
                        }
                        else
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid stream \"" << applicationName << "/" << streamName << "\", disconnecting";
                            socket.close();
                            return false;
                        }
                    }
                    else
                    {
                        // this is not a sender
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"play\") received, disconnecting";

                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "getStreamLength")
                {
                    if (streamType == StreamType::INPUT)
                    {
                        // ignore this
                    }
                    else
                    {
                        // this is not a sender
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"getStreamLength\") received, disconnecting";

                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "stop")
                {
                    if (streamType == StreamType::OUTPUT)
                    {
                        streamType = StreamType::NONE;
                        sendStopStatus(transactionId.asDouble());
                    }
                    else
                    {
                        // this is not a sender
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"stop\") received, disconnecting";

                        socket.close();
                        return false;
                    }
                }
                else if (command.asString() == "onStatus")
                {
                    if (argument2["code"].asString() == "NetStream.Publish.Start")
                    {
                        if (streamType == StreamType::OUTPUT)
                        {
                            server->startReceiving(*this);
                        }
                        else
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Wrong status (\"NetStream.Publish.Start\") received, disconnecting";

                            socket.close();
                            return false;
                        }
                    }
                    else if (argument2["code"].asString() == "NetStream.Play.Start")
                    {
                        if (streamType == StreamType::INPUT)
                        {
                            server->startStreaming(*this);
                        }
                        else
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Wrong status (\"NetStream.Play.Start\") received, disconnecting";

                            socket.close();
                            return false;
                        }
                    }

                }
                else if (command.asString() == "_error")
                {
                    auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                    if (i != invokes.end())
                    {
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << i->second << " error";

                        invokes.erase(i);
                    }
                    else
                    {
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << i->second << "Invalid _error received";
                    }
                }
                else if (command.asString() == "_result")
                {
                    auto i = invokes.find(static_cast<uint32_t>(transactionId.asDouble()));

                    if (i != invokes.end())
                    {
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << i->second << " result";

                        if (i->second == "connect")
                        {
                            connected = true;

                            if (!streamName.empty())
                            {
                                sendReleaseStream();

                                if (streamType == StreamType::OUTPUT)
                                {
                                    sendFCPublish();
                                }

                                sendCreateStream();
                            }
                        }
                        else if (i->second == "_checkbw")
                        {
                        }
                        else if (i->second == "releaseStream")
                        {
                        }
                        else if (i->second == "createStream")
                        {
                            streamId = static_cast<uint32_t>(argument2.asDouble());

                            if (streamType == StreamType::INPUT)
                            {
                                sendPlay();
                            }
                            else if (streamType == StreamType::OUTPUT)
                            {
                                sendPublish();
                            }

                            Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Created stream " << streamId;
                        }
                        else if (i->second == "deleteStream")
                        {
                        }

                        invokes.erase(i);
                    }
                    else
                    {
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << i->second << "Invalid _result received";
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

    void Connection::createStream(const std::string& newApplicationName,
                                  const std::string& newStreamName)
    {
        if (overrideApplicationName.empty())
        {
            applicationName = newApplicationName;
        }
        else
        {
            std::map<std::string, std::string> tokens = {
                {"id", std::to_string(id)},
                {"streamName", newStreamName},
                {"applicationName", newApplicationName},
                {"ipAddress", cppsocket::ipToString(socket.getRemoteIPAddress())},
                {"port", std::to_string(socket.getRemotePort())}
            };

            applicationName = overrideApplicationName;
            replaceTokens(applicationName, tokens);
        }

        if (overrideStreamName.empty())
        {
            streamName = newStreamName;
        }
        else
        {
            std::map<std::string, std::string> tokens = {
                {"id", std::to_string(id)},
                {"streamName", newStreamName},
                {"applicationName", newApplicationName},
                {"ipAddress", cppsocket::ipToString(socket.getRemoteIPAddress())},
                {"port", std::to_string(socket.getRemotePort())}
            };

            streamName = overrideStreamName;
            replaceTokens(streamName, tokens);
        }
    }

    void Connection::deleteStream()
    {
        if (connected && server)
        {
            server->stopReceiving(*this);
            server->stopStreaming(*this);
            server = nullptr;
        }
    }

    void Connection::unpublishStream()
    {
        if (connected)
        {
            sendFCUnpublish();
        }
    }

    void Connection::sendServerBandwidth()
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

    void Connection::sendClientBandwidth()
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

    void Connection::sendPing()
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

    void Connection::sendSetChunkSize()
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

    void Connection::sendOnBWDone()
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

    void Connection::sendCheckBW()
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendCheckBWResult(double transactionId)
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

    void Connection::sendCreateStream()
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendCreateStreamResult(double transactionId)
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
        if (streamId == 0 || streamId == 2) // streams 0 and 2 are reserved
        {
            ++streamId;
        }

        amf0::Node argument2 = static_cast<double>(streamId);
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendReleaseStream()
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendReleaseStreamResult(double transactionId)
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

    void Connection::sendDeleteStream()
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

        amf0::Node argument2 = static_cast<double>(streamId);
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;
        
        socket.send(buffer);
        
        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendConnect()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("connect");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1;
        argument1["app"] = applicationName;
        argument1["flashVer"] = std::string("FMLE/3.0 (compatible; Lavf57.5.0)");
        argument1["tcUrl"] = std::string("rtmp://127.0.0.1:") + std::to_string(socket.getRemotePort()) + "/" + applicationName;
        argument1["type"] = std::string("nonprivate");
        argument1.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendConnectResult(double transactionId)
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

    void Connection::sendFCPublish()
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendOnFCPublish()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("onFCPublish");
        commandName.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendFCUnpublish()
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendOnFCUnpublish()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("onFCUnpublish");
        commandName.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendPublish()
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

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();

        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Published stream \"" << streamName << "\" (ID: " << streamId << ") to " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort();
    }

    void Connection::sendPublishStatus(double transactionId)
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
        argument2["description"] = streamName + " is now published";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    void Connection::sendUnublishStatus(double transactionId)
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
        argument2["code"] = std::string("NetStream.Unpublish.Success");
        argument2["description"] = streamName + " stopped publishing";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    void Connection::sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData)
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

    void Connection::sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData)
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

    void Connection::sendMetaData(const amf0::Node metaData)
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
    }

    void Connection::sendTextData(uint64_t timestamp, const amf0::Node& textData)
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

    void Connection::sendPlay()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("play");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2 = streamName;
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendPlayStatus(double transactionId)
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
        argument2["description"] = streamName + " is now playing";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendStop()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::INVOKE;

        amf0::Node commandName = std::string("stop");
        commandName.encode(packet.data);

        amf0::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(packet.data);

        amf0::Node argument1(amf0::Marker::Null);
        argument1.encode(packet.data);

        amf0::Node argument2 = streamName;
        argument2.encode(packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendStopStatus(double transactionId)
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
}
