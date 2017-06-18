//
//  rtmp_relay
//

#include "Connection.h"
#include "Relay.h"
#include "Server.h"
#include "Constants.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    Connection::Connection(Relay& aRelay, cppsocket::Socket& aSocket, Type aType):
        relay(aRelay),
        id(Relay::nextId()),
        mersanneTwister(relay.getMersanneTwister()),
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
        socket.startRead();
    }

    Connection::Connection(Relay& aRelay,
                           cppsocket::Socket& connector,
                           const Description& description):
        Connection(aRelay, connector, Type::CLIENT)
    {
        addresses = description.addresses;
        ipAddresses = description.ipAddresses;
        connectionTimeout = description.connectionTimeout;
        reconnectInterval = description.reconnectInterval;
        reconnectCount = description.reconnectCount;
        bufferSize = description.bufferSize;
        videoStream = description.videoStream;
        audioStream = description.audioStream;
        dataStream = description.dataStream;
        streamType = description.streamType;
        server = description.server;
        applicationName = description.applicationName;
        streamName = description.streamName;
        overrideApplicationName = description.overrideApplicationName;
        overrideStreamName = description.overrideStreamName;
        amfVersion = description.amfVersion;
        metaDataBlacklist = description.metaDataBlacklist;

        socket.setConnectTimeout(connectionTimeout);
        socket.setConnectCallback(std::bind(&Connection::handleConnect, this, std::placeholders::_1));
        socket.setConnectErrorCallback(std::bind(&Connection::handleConnectError, this, std::placeholders::_1));
    }

    Connection::~Connection()
    {
        if (server)
        {
            server->stopReceiving(*this);
            server->stopStreaming(*this);
        }
    }

    void Connection::close()
    {
        socket.close();
        reset();
    }

    void Connection::reset()
    {
        state = State::UNINITIALIZED;
        data.clear();
        timeSincePing = 0.0f;
        timeSinceConnect = 0.0f;
        inChunkSize = 128;
        outChunkSize = 128;
        serverBandwidth = 2500000;
        receivedPackets.clear();
        sentPackets.clear();
        invokeId = 0;
        invokes.clear();
        streamId = 0;

        connected = false;
        videoFrameSent = false;
        metaData = amf::Node::Type::Unknown;

        if (server)
        {
            server->stopReceiving(*this);
            server->stopStreaming(*this);

            if (type != Type::CLIENT)
            {
                videoStream = true;
                audioStream = true;
                dataStream = true;

                streamType = StreamType::NONE;
                applicationName.clear();
                streamName.clear();
                overrideApplicationName.clear();
                overrideStreamName.clear();
                metaDataBlacklist.clear();
                server = nullptr;
            }
        }

        // disconnect all host connections
        if (type == Type::HOST)
        {
            streamType = StreamType::NONE;
        }
    }

    bool Connection::isClosed() const
    {
        return type == Type::HOST && !socket.isReady(); // host connections are closed if the client disconnected
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
                    sendUserControl(rtmp::UserControlType::PING);
                }
            }
        }
        else if (type == Type::CLIENT)
        {
            if (socket.isReady() && state == State::HANDSHAKE_DONE)
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
                        socket.connect(ipAddresses[addressIndex].first, ipAddresses[addressIndex].second);
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

    void Connection::getStats(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "\t[" + std::to_string(id) + ", " + name + "], " +
                    "name: " + streamName + ", " +
                    "application: " + applicationName + ", " +
                    "status: " + (socket.isReady() ? "connected" : "not connected") + ", " +
                    "address: " + ipToString(socket.getRemoteIPAddress()) + ":" + std::to_string(socket.getRemotePort()) + ", " +
                    "connection: ";

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

                str += ", stream: ";

                switch (streamType)
                {
                    case StreamType::NONE: str += "NONE"; break;
                    case StreamType::INPUT: str += "INPUT"; break;
                    case StreamType::OUTPUT: str += "OUTPUT"; break;
                }

                if (server) str += ", server: " + std::to_string(server->getId());

                if (metaData.getType() == amf::Node::Type::Dictionary ||
                    metaData.getType() == amf::Node::Type::Object)
                {
                    str += ", metadata: ";
                    bool first = true;

                    for (const std::pair<std::string, amf::Node>& value : metaData.asMap())
                    {
                        if (!first) str += ", ";
                        first = false;
                        str += value.first + " = " + value.second.toString();
                    }
                }

                str += "\n";

                break;
            }
            case ReportType::HTML:
            {
                str += "<tr><td>" + std::to_string(id) +"</td><td>" + streamName + "</td>" +
                    "<td>" + applicationName + "</td>" +
                    "<td>" + (socket.isReady() ? "Connected" : "Not connected") + "</td><td>" + ipToString(socket.getRemoteIPAddress()) + ":" + std::to_string(socket.getRemotePort()) + "</td><td>";

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

                str += "</td><td>" + (server ? std::to_string(server->getId()) : "") + "</td><td>";

                if (metaData.getType() == amf::Node::Type::Dictionary ||
                    metaData.getType() == amf::Node::Type::Object)
                {
                    bool first = true;

                    for (const std::pair<std::string, amf::Node>& value : metaData.asMap())
                    {
                        if (!first) str += "<br/>";
                        first = false;
                        str += value.first + " = " + value.second.toString();
                    }
                }

                str += "</td></tr>";
                break;
            }
            case ReportType::JSON:
            {
                str += "{\"id\":" + std::to_string(id) + "," +
                    "\"name\":\"" + streamName + "\","
                    "\"application\":\"" + applicationName + "\"," +
                    "\"status\":" + (socket.isReady() ? "\"connected\"" : "\"not connected\"") + "," +
                    "\"address\":\"" + ipToString(socket.getRemoteIPAddress()) + ":" + std::to_string(socket.getRemotePort()) + "\"," +
                    "\"connection\":";

                switch (type)
                {
                    case Type::HOST: str += "\"HOST\""; break;
                    case Type::CLIENT: str += "\"CLIENT\""; break;
                }

                str += ",\"state\":";

                switch (state)
                {
                    case State::UNINITIALIZED: str += "\"UNINITIALIZED\""; break;
                    case State::VERSION_RECEIVED: str += "\"VERSION_RECEIVED\""; break;
                    case State::VERSION_SENT: str += "\"VERSION_SENT\""; break;
                    case State::ACK_SENT: str += "\"ACK_SENT\","; break;
                    case State::HANDSHAKE_DONE: str += "\"HANDSHAKE_DONE\""; break;
                }

                str += ",\"stream\":";

                switch (streamType)
                {
                    case StreamType::NONE: str += "\"NONE\""; break;
                    case StreamType::INPUT: str += "\"INPUT\""; break;
                    case StreamType::OUTPUT: str += "\"OUTPUT\""; break;
                }

                if (server) str += ",\"serverId\":" + std::to_string(server->getId());

                if (metaData.getType() == amf::Node::Type::Dictionary ||
                    metaData.getType() == amf::Node::Type::Object)
                {
                    str += ",\"metaData\":{";
                    bool first = true;

                    for (const std::pair<std::string, amf::Node>& value : metaData.asMap())
                    {
                        if (!first) str += ", ";
                        first = false;
                        if (value.second.getType() == amf::Node::Type::Boolean)
                        {
                            str += "\"" + escapeString(value.first) + "\":" + (value.second.asBool() ? "true" : "false");
                        }
                        else if (value.second.isNumber())
                        {
                            str += "\"" + escapeString(value.first) + "\":" + value.second.toString();
                        }
                        else
                        {
                            str += "\"" + escapeString(value.first) + "\":\"" + escapeString(value.second.toString()) + "\"";
                        }
                    }

                    str += "}";
                }

                str += "}";
                break;
            }
        }
    }

    void Connection::connect()
    {
        if (addressIndex < addresses.size())
        {
            socket.connect(ipAddresses[addressIndex].first, ipAddresses[addressIndex].second);
        }
    }

    void Connection::handleConnect(cppsocket::Socket&)
    {
        // handshake
        if (type == Type::CLIENT)
        {
            Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Connected to " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort();

            // C0
            std::vector<uint8_t> version;
            version.push_back(RTMP_VERSION);
            socket.send(version);

            Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending version message " << RTMP_VERSION;

            // C1
            rtmp::Challenge challenge;
            challenge.time = 0;
            std::copy(RTMP_SERVER_VERSION, RTMP_SERVER_VERSION + sizeof(RTMP_SERVER_VERSION), challenge.version);

            for (size_t i = 0; i < sizeof(challenge.randomBytes); ++i)
            {
                uint32_t randomValue = mersanneTwister.extractU32() % 255;

                challenge.randomBytes[i] = static_cast<uint8_t>(randomValue);
            }

            std::vector<uint8_t> challengeMessage;
            challengeMessage.insert(challengeMessage.begin(),
                                    reinterpret_cast<uint8_t*>(&challenge),
                                    reinterpret_cast<uint8_t*>(&challenge) + sizeof(challenge));
            socket.send(challengeMessage);

            Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending challenge message";

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
                            close();
                            break;
                        }

                        // S0
                        std::vector<uint8_t> reply;
                        reply.push_back(RTMP_VERSION);
                        socket.send(reply);
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending reply version " << RTMP_VERSION;

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
                            uint32_t randomValue = mersanneTwister.extractU32() % 255;
                            replyChallenge.randomBytes[i] = static_cast<uint8_t>(randomValue);
                        }

                        std::vector<uint8_t> reply;
                        reply.insert(reply.begin(),
                                     reinterpret_cast<uint8_t*>(&replyChallenge),
                                     reinterpret_cast<uint8_t*>(&replyChallenge) + sizeof(replyChallenge));
                        socket.send(reply);

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending challange reply message";

                        // S2
                        rtmp::Ack ack;
                        ack.time = challenge->time;
                        std::copy(challenge->version, challenge->version + sizeof(ack.version), ack.version);
                        std::copy(challenge->randomBytes, challenge->randomBytes + sizeof(ack.randomBytes), ack.randomBytes);

                        std::vector<uint8_t> ackData(reinterpret_cast<uint8_t*>(&ack),
                                                     reinterpret_cast<uint8_t*>(&ack) + sizeof(ack));
                        socket.send(ackData);

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending Ack message";

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

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got Ack reply message, time: " << ack->time <<
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

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got reply version " << static_cast<uint32_t>(version);

                        if (version != 0x03)
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Unsuported version, disconnecting";
                            close();
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

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got challenge reply message, time: " << challenge->time <<
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

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending Ack message";

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

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Got Ack reply message, time: " << ack->time <<
                            ", version: " << static_cast<uint32_t>(ack->version[0]) << "." <<
                            static_cast<uint32_t>(ack->version[1]) << "." <<
                            static_cast<uint32_t>(ack->version[2]) << "." <<
                            static_cast<uint32_t>(ack->version[3]);
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Handshake done";
                        
                        state = State::HANDSHAKE_DONE;

                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Connecting to application " << applicationName;

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
        reset();

        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Client at " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " disconnected";
    }

    bool Connection::handlePacket(const rtmp::Packet& packet)
    {
        switch (packet.messageType)
        {
            case rtmp::MessageType::SET_CHUNK_SIZE:
            {
                uint32_t offset = 0;

                uint32_t ret = decodeIntBE(packet.data, offset, 4, inChunkSize);

                if (ret == 0)
                {
                    return false;
                }

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Received SET_CHUNK_SIZE, parameter: " << inChunkSize;

                if (type == Type::CLIENT)
                {
                    sendSetChunkSize();
                }

                break;
            }

            case rtmp::MessageType::ABORT:
            {
                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Received ABORT";
                break;
            }

            case rtmp::MessageType::BYTES_READ:
            {
                uint32_t offset = 0;
                uint32_t bytesRead;

                uint32_t ret = decodeIntBE(packet.data, offset, 4, bytesRead);

                if (ret == 0)
                {
                    return false;
                }

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Received BYTES_READ, parameter: " << bytesRead;

                break;
            }

            case rtmp::MessageType::USER_CONTROL:
            {
                uint32_t offset = 0;

                uint16_t argument;
                uint32_t ret = decodeIntBE(packet.data, offset, 2, argument);

                rtmp::UserControlType userControlType = static_cast<rtmp::UserControlType>(argument);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                uint32_t param;
                ret = decodeIntBE(packet.data, offset, 4, param);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                {
                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] " << "Received PING, type: ";

                    switch (userControlType)
                    {
                        case rtmp::UserControlType::CLEAR_STREAM: log << "CLEAR_STREAM"; break;
                        case rtmp::UserControlType::CLEAR_BUFFER: log << "CLEAR_BUFFER"; break;
                        case rtmp::UserControlType::CLIENT_BUFFER_TIME: log << "CLIENT_BUFFER_TIME"; break;
                        case rtmp::UserControlType::RESET_STREAM: log << "RESET_STREAM"; break;
                        case rtmp::UserControlType::PING: log << "PING"; break;
                        case rtmp::UserControlType::PONG: log << "PONG"; break;
                    }

                    log << ", param: " << param;
                }

                if (userControlType == rtmp::UserControlType::PING)
                {
                    sendUserControl(rtmp::UserControlType::PONG, packet.timestamp);
                }

                break;
            }

            case rtmp::MessageType::SERVER_BANDWIDTH:
            {
                uint32_t offset = 0;

                uint32_t bandwidth;
                uint32_t ret = decodeIntBE(packet.data, offset, 4, bandwidth);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Received SERVER_BANDWIDTH, parameter: " << bandwidth;

                break;
            }

            case rtmp::MessageType::CLIENT_BANDWIDTH:
            {
                uint32_t offset = 0;

                uint32_t bandwidth;
                uint32_t ret = decodeIntBE(packet.data, offset, 4, bandwidth);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                uint8_t bandwidthType;
                ret = decodeIntBE(packet.data, offset, 1, bandwidthType);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Received CLIENT_BANDWIDTH, parameter: " << bandwidth << ", type: " << bandwidthType;

                break;
            }

            case rtmp::MessageType::AMF0_DATA:
            case rtmp::MessageType::AMF3_DATA:
            {
                uint32_t offset = 0;
                uint32_t ret;

                if (packet.messageType == rtmp::MessageType::AMF3_DATA)
                {
                    uint8_t header;
                    ret = decodeIntBE(packet.data, offset, sizeof(uint8_t), header);

                    if (ret == 0)
                    {
                        return false;
                    }

                    offset += ret;

                    // no documentation states what this byte means, but it usually is 0
                    if (header != 0)
                    {
                        return false;
                    }
                }

                // only input can receive notify packets
                if (streamType == StreamType::INPUT)
                {
                    amf::Node command;

                    ret = command.decode(amf::Version::AMF0, packet.data, offset);

                    if (ret == 0)
                    {
                        return false;
                    }

                    offset += ret;

                    {
                        Log log(Log::Level::ALL);
                        log << "[" << id << ", " << name << "] " << "Received NOTIFY, command: ";
                        command.dump(log);
                    }

                    amf::Node argument1;

                    if ((ret = argument1.decode(amf::Version::AMF0, packet.data, offset))  > 0)
                    {
                        offset += ret;

                        Log log(Log::Level::ALL);
                        log << "[" << id << ", " << name << "] " << "Argument 1: ";
                        argument1.dump(log);
                    }

                    amf::Node argument2;

                    if ((ret = argument2.decode(amf::Version::AMF0, packet.data, offset)) > 0)
                    {
                        offset += ret;

                        Log log(Log::Level::ALL);
                        log << "[" << id << ", " << name << "] " << "Argument 2: ";
                        argument2.dump(log);
                    }

                    if (command.asString() == "@setDataFrame" &&
                        argument1.asString() == "onMetaData" &&
                        (argument2.getType() == amf::Node::Type::Dictionary ||
                         argument2.getType() == amf::Node::Type::Object))
                    {
                        metaData = argument2;

                        Log(Log::Level::ALL) << "Audio codec: " << getAudioCodec(static_cast<AudioCodec>(metaData["audiocodecid"].asUInt32()));
                        Log(Log::Level::ALL) << "Video codec: " << getVideoCodec(static_cast<VideoCodec>(metaData["videocodecid"].asUInt32()));

                        // forward notify packet
                        if (server) server->sendMetaData(metaData);
                    }
                    else if (command.asString() == "onMetaData" &&
                             (argument1.getType() == amf::Node::Type::Dictionary ||
                              argument1.getType() == amf::Node::Type::Object))
                    {
                        metaData = argument1;

                        Log(Log::Level::ALL) << "Audio codec: " << getAudioCodec(static_cast<AudioCodec>(metaData["audiocodecid"].asUInt32()));
                        Log(Log::Level::ALL) << "Video codec: " << getVideoCodec(static_cast<VideoCodec>(metaData["videocodecid"].asUInt32()));

                        // forward notify packet
                        if (server) server->sendMetaData(metaData);
                    }
                    else if (command.asString() == "onTextData")
                    {
                        if (server) server->sendTextData(packet.timestamp, argument1);
                    }
                }
                else
                {
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent notify packet to sender, disconnecting";
                    close();
                    return false;
                }
                break;
            }

            case rtmp::MessageType::AUDIO_PACKET:
            {
                // only input can receive audio packets
                if (streamType == StreamType::INPUT)
                {
                    {
                        Log log(Log::Level::ALL);
                        log << "[" << id << ", " << name << "] " << "Received AUDIO_PACKET";
                        if (isCodecHeader(packet.data)) log << "(header)";
                    }

                    currentAudioBytes += packet.data.size();

                    if (isCodecHeader(packet.data))
                    {
                        uint8_t format = packet.data[0];
                        AudioCodec codec = static_cast<AudioCodec>((format & 0xf0) >> 4);
                        uint32_t channels = (format & 0x01) + 1;
                        uint32_t sampleSize = (format & 0x02) ? 2 : 1;
                        Log(Log::Level::ALL) << "Codec: " << getAudioCodec(codec) << ", channels: " << channels << ", sampleSize: " << sampleSize * 8;

                        if (server) server->sendAudioHeader(packet.data);
                    }
                    else
                    {
                        // forward audio packet
                        if (server) server->sendAudioFrame(packet.timestamp, packet.data);
                    }
                }
                else
                {
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent audio packet to sender, disconnecting";
                    close();
                    return false;
                }
                break;
            }

            case rtmp::MessageType::VIDEO_PACKET:
            {
                // only input can receive video packets
                if (streamType == StreamType::INPUT)
                {
                    VideoFrameType frameType = getVideoFrameType(packet.data);

                    {
                        Log log(Log::Level::ALL);
                        log << "[" << id << ", " << name << "] " << "Received VIDEO_PACKET";

                        if (isCodecHeader(packet.data))
                        {
                            log << "(header)";
                        }

                        switch (frameType)
                        {
                            case VideoFrameType::KEY: log << "(key frame)"; break;
                            case VideoFrameType::INTER: log << "(inter frame)"; break;
                            case VideoFrameType::DISPOSABLE: log << "(disposable frame)"; break;
                            case VideoFrameType::GENERATED_KEY: log << "(generated key frame)"; break;
                            case VideoFrameType::VIDEO_INFO: log << "(video info)"; break;
                            default: log << "(unknown frame)"; break;
                        }
                    }

                    currentVideoBytes += packet.data.size();

                    if (isCodecHeader(packet.data))
                    {
                        uint8_t format = packet.data[0];
                        VideoCodec codec = static_cast<VideoCodec>(format & 0x0f);
                        Log(Log::Level::ALL) << "Codec: " << getVideoCodec(codec);

                        if (server && frameType == VideoFrameType::KEY) server->sendVideoHeader(packet.data);
                        // do nothing if frameType is VideoFrameType::VIDEO_INFO
                    }
                    else
                    {
                        // forward video packet
                        if (server) server->sendVideoFrame(packet.timestamp, packet.data, frameType);
                    }
                }
                else
                {
                    Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Client sent video packet to sender, disconnecting";
                    close();
                    return false;
                }
                break;
            }

            case rtmp::MessageType::AMF0_INVOKE:
            case rtmp::MessageType::AMF3_INVOKE:
            {
                uint32_t offset = 0;
                uint32_t ret;

                if (packet.messageType == rtmp::MessageType::AMF3_INVOKE)
                {
                    uint8_t header;
                    ret = decodeIntBE(packet.data, offset, sizeof(uint8_t), header);

                    if (ret == 0)
                    {
                        return false;
                    }

                    offset += ret;

                    // no documentation states what this byte means, but it usually is 0
                    if (header != 0)
                    {
                        return false;
                    }
                }

                amf::Node command;

                ret = command.decode(amf::Version::AMF0, packet.data, offset);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                {
                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] " << "Received INVOKE, command: ";
                    command.dump(log);
                }

                amf::Node transactionId;

                ret = transactionId.decode(amf::Version::AMF0, packet.data, offset);

                if (ret == 0)
                {
                    return false;
                }

                offset += ret;

                {
                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] " << "Transaction ID: ";
                    transactionId.dump(log);
                }

                amf::Node argument1;

                if ((ret = argument1.decode(amf::Version::AMF0, packet.data, offset)) > 0)
                {
                    offset += ret;

                    Log log(Log::Level::ALL);
                    log << "[" << id << ", " << name << "] " << "Argument 1: ";
                    argument1.dump(log);
                }

                if (command.asString() == "connect")
                {
                    if (type == Type::HOST)
                    {
                        applicationName = argument1["app"].asString();

                        if (argument1.hasElement("objectEncoding"))
                        {
                            amfVersion = (argument1["objectEncoding"].asDouble() == 3.0) ? amf::Version::AMF3 : amf::Version::AMF0;
                        }

                        sendServerBandwidth();
                        sendClientBandwidth();
                        sendUserControl(rtmp::UserControlType::CLEAR_STREAM);
                        sendSetChunkSize();
                        sendConnectResult(transactionId.asDouble());
                        sendOnBWDone();

                        connected = true;

                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " sent connect, application: \"" << argument1["app"].asString() << "\"";

#ifdef DEBUG
                        Log log(Log::Level::ALL);
                        log << "Connect argument: ";
                        argument1.dump(log);
#endif
                    }
                    else
                    {
                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Invalid message (\"connect\") received, disconnecting";
                        close();
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
                        close();
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
                        close();
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
                        close();
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
                        close();
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
                        close();
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
                        close();
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
                        videoFrameSent = false;

                        if (server)
                        {
                            server->stopReceiving(*this);
                            server->stopStreaming(*this);

                            if (type != Type::CLIENT)
                            {
                                server = nullptr;
                            }
                        }

                        sendOnFCUnpublish();

                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " unpublished stream \"" << streamName << "\"";

                        if (type != Type::CLIENT)
                        {
                            streamName.clear();
                        }
                    }
                    else
                    {
                        // this is not a receiver
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"FCUnpublish\") received, disconnecting";
                        close();
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
                        close();
                        return false;
                    }
                }
                else if (command.asString() == "FCSubscribe")
                {
                    if (streamType == StreamType::NONE ||
                        streamType == StreamType::OUTPUT)
                    {
                        sendOnFCSubscribe();
                    }
                    else if (streamType == StreamType::INPUT)
                    {
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"FCSubscribe\") received, disconnecting";
                        close();
                        return false;
                    }
                }
                else if (command.asString() == "onFCSubscribe")
                {
                }
                else if (command.asString() == "publish")
                {
                    if (streamType == StreamType::NONE ||
                        streamType == StreamType::INPUT)
                    {
                        streamType = StreamType::INPUT;

                        amf::Node argument2;

                        if ((ret = argument2.decode(amf::Version::AMF0, packet.data, offset)) > 0)
                        {
                            offset += ret;

                            Log log(Log::Level::ALL);
                            log << "[" << id << ", " << name << "] " << "Argument 2: ";
                            argument2.dump(log);
                        }

                        streamName = argument2.asString();

                        const Description* connectionDescription = relay.getConnectionDescription(std::make_pair(socket.getLocalIPAddress(), socket.getLocalPort()), streamType, applicationName, streamName);

                        if (connectionDescription)
                        {
                            sendUserControl(rtmp::UserControlType::CLEAR_STREAM);
                            sendPublishStatus(transactionId.asDouble());

                            server = connectionDescription->server;
                            pingInterval = connectionDescription->pingInterval;

                            server->startStreaming(*this);

                            Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " published stream \"" << streamName << "\"";
                        }
                        else
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid stream \"" << applicationName << "/" << streamName << "\", disconnecting";
                            close();
                            return false;
                        }
                    }
                    else if (streamType == StreamType::OUTPUT)
                    {
                        // this is not a receiver
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"publish\") received, disconnecting";
                        close();
                        return false;
                    }
                }
                else if (command.asString() == "unpublish")
                {
                    if (streamType == StreamType::INPUT)
                    {
                        streamType = StreamType::NONE;
                        videoFrameSent = false;

                        if (server)
                        {
                            server->stopReceiving(*this);
                            server->stopStreaming(*this);

                            if (type != Type::CLIENT)
                            {
                                server = nullptr;
                            }
                        }

                        sendUnublishStatus(transactionId.asDouble());

                        Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Input from " << ipToString(socket.getRemoteIPAddress()) << ":" << socket.getRemotePort() << " unpublished stream \"" << streamName << "\"";

                        if (type != Type::CLIENT)
                        {
                            streamName.clear();
                        }
                    }
                    else
                    {
                        // this is not a receiver
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"FCUnpublish\") received, disconnecting";
                        close();
                        return false;
                    }
                }
                else if (command.asString() == "play")
                {
                    if (streamType == StreamType::NONE ||
                        streamType == StreamType::OUTPUT)
                    {
                        streamType = StreamType::OUTPUT;

                        amf::Node argument2;

                        if ((ret = argument2.decode(amf::Version::AMF0, packet.data, offset)) > 0)
                        {
                            offset += ret;

                            Log log(Log::Level::ALL);
                            log << "[" << id << ", " << name << "] " << "Argument 2: ";
                            argument2.dump(log);
                        }

                        streamName = argument2.asString();

                        const Description* connectionDescription = relay.getConnectionDescription(std::make_pair(socket.getLocalIPAddress(), socket.getLocalPort()), streamType, applicationName, streamName);

                        if (connectionDescription)
                        {
                            sendUserControl(rtmp::UserControlType::CLEAR_STREAM);
                            sendPlayStatus(transactionId.asDouble());

                            server = connectionDescription->server;
                            server->startReceiving(*this);
                            pingInterval = connectionDescription->pingInterval;
                            videoStream = connectionDescription->videoStream;
                            audioStream = connectionDescription->audioStream;
                            dataStream = connectionDescription->dataStream;
                            metaDataBlacklist = connectionDescription->metaDataBlacklist;
                        }
                        else
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid stream \"" << applicationName << "/" << streamName << "\", disconnecting";
                            close();
                            return false;
                        }
                    }
                    else
                    {
                        // this is not a sender
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"play\") received, disconnecting";
                        close();
                        return false;
                    }
                }
                else if (command.asString() == "getStreamLength")
                {
                    if (streamType == StreamType::NONE ||
                        streamType == StreamType::INPUT)
                    {
                        sendGetStreamLengthResult(transactionId.asDouble());
                    }
                    else
                    {
                        // this is not a sender
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"getStreamLength\") received, disconnecting";
                        close();
                        return false;
                    }
                }
                else if (command.asString() == "stop")
                {
                    if (streamType == StreamType::OUTPUT)
                    {
                        streamType = StreamType::NONE;
                        videoFrameSent = false;
                        sendStopStatus(transactionId.asDouble());
                    }
                    else
                    {
                        // this is not a sender
                        Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Invalid message (\"stop\") received, disconnecting";
                        close();
                        return false;
                    }
                }
                else if (command.asString() == "onStatus")
                {
                    amf::Node argument2;

                    if ((ret = argument2.decode((packet.messageType == rtmp::MessageType::AMF3_INVOKE) ? amf::Version::AMF3 : amf::Version::AMF0, packet.data, offset)) > 0)
                    {
                        offset += ret;

                        Log log(Log::Level::ALL);
                        log << "[" << id << ", " << name << "] " << "Argument 2: ";
                        argument2.dump(log);
                    }

                    if (argument2["code"].asString() == "NetStream.Publish.Start")
                    {
                        if (streamType == StreamType::OUTPUT)
                        {
                            server->startReceiving(*this);
                        }
                        else
                        {
                            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Wrong status (\"NetStream.Publish.Start\") received, disconnecting";
                            close();
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
                            close();
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
                                if (streamType == StreamType::OUTPUT)
                                {
                                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Publishing stream " << streamName;

                                    sendReleaseStream();
                                    sendFCPublish();
                                }
                                else if (streamType == StreamType::INPUT)
                                {
                                    Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Subscribing to stream " << streamName;

                                    sendFCSubscribe();
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
                            amf::Node argument2;

                            if ((ret = argument2.decode(amf::Version::AMF0, packet.data, offset)) > 0)
                            {
                                offset += ret;

                                Log log(Log::Level::ALL);
                                log << "[" << id << ", " << name << "] " << "Argument 2: ";
                                argument2.dump(log);
                            }

                            streamId = static_cast<uint32_t>(argument2.asDouble());

                            if (streamType == StreamType::INPUT)
                            {
                                sendGetStreamLength();
                                sendPlay();
                                sendUserControl(rtmp::UserControlType::CLIENT_BUFFER_TIME, 0, streamId, bufferSize);
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
                        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Invalid _result received, transaction ID: " << static_cast<uint32_t>(transactionId.asDouble());
                    }
                }
                break;
            }

            case rtmp::MessageType::AMF0_SHARED_OBJECT:
            case rtmp::MessageType::AMF3_SHARED_OBJECT:
            {
                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Received shared object";
                break;
            }

            case rtmp::MessageType::AGGREGATE:
            {
                Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Received aggregated messages";
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
            if (applicationName.empty())
            {
                applicationName = newApplicationName;
            }
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
            if (streamName.empty())
            {
                streamName = newStreamName;
            }
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

            if (type != Type::CLIENT)
            {
                server = nullptr;
            }
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

        encodeIntBE(packet.data, 4, serverBandwidth);

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

        encodeIntBE(packet.data, 4, serverBandwidth);
        encodeIntBE(packet.data, 1, 2); // dynamic

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending CLIENT_BANDWIDTH";

        socket.send(buffer);
    }

    void Connection::sendUserControl(rtmp::UserControlType userControlType, uint64_t timestamp, uint32_t parameter1, uint32_t parameter2)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::NETWORK;
        packet.timestamp = timestamp;
        packet.messageType = rtmp::MessageType::USER_CONTROL;

        encodeIntBE(packet.data, 2, static_cast<uint16_t>(userControlType));
        encodeIntBE(packet.data, 4, parameter1); // parameter 1
        if (parameter2 != 0) encodeIntBE(packet.data, 4, parameter2); // parameter 2

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log log(Log::Level::ALL);
        log << "[" << id << ", " << name << "] " << "Sending USER_CONTROL of type: ";

        switch (userControlType)
        {
            case rtmp::UserControlType::CLEAR_STREAM: log << "CLEAR_STREAM"; break;
            case rtmp::UserControlType::CLEAR_BUFFER: log << "CLEAR_BUFFER"; break;
            case rtmp::UserControlType::CLIENT_BUFFER_TIME: log << "CLIENT_BUFFER_TIME"; break;
            case rtmp::UserControlType::RESET_STREAM: log << "RESET_STREAM"; break;
            case rtmp::UserControlType::PING: log << "PING"; break;
            case rtmp::UserControlType::PONG: log << "PONG"; break;
        }

        log << ", parameter 1: " << parameter1;
        if (parameter2 != 0) log << ", parameter 2: " << parameter2;

        socket.send(buffer);
    }

    void Connection::sendSetChunkSize()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;
        packet.messageType = rtmp::MessageType::SET_CHUNK_SIZE;

        encodeIntBE(packet.data, 4, outChunkSize);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onBWDone");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = 0.0;
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("_checkbw");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("_result");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("createStream");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("_result");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        ++streamId;
        if (streamId == 0 || streamId == 2) // streams 0 and 2 are reserved
        {
            ++streamId;
        }

        amf::Node argument2 = static_cast<double>(streamId);
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("releaseStream");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("_result");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("deleteStream");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = static_cast<double>(streamId);
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("connect");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1;
        argument1["app"] = applicationName;
        argument1["type"] = std::string("nonprivate");
        argument1["flashVer"] = std::string("FMLE/3.0 (compatible; Lavf56.16.0)");
        argument1["tcUrl"] = "rtmp://" + addresses[addressIndex] + "/" + applicationName;
        argument1["objectEncoding"] = (amfVersion == amf::Version::AMF3) ? 3.0 : 0.0;

        argument1.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("_result");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1;
        argument1["fmsVer"] = std::string("FMS/3,5,7,7009");
        argument1["capabilities"] = 31.0;
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2;
        argument2["level"] = std::string("status");
        argument2["code"] = std::string("NetConnection.Connect.Success");
        argument2["description"] = std::string("Connection succeeded.");
        argument2["objectEncoding"] = (amfVersion == amf::Version::AMF3) ? 3.0 : 0.0;

        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("FCPublish");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onFCPublish");
        commandName.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("FCUnpublish");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onFCUnpublish");
        commandName.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendFCSubscribe()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("FCSubscribe");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendOnFCSubscribe()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onFCSubscribe");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2;
        argument2["clientid"] = std::string("Lavf57.1.0");
        argument2["code"] = std::string("NetStream.Play.Start");
        argument2["description"] = "Subscribed to " + streamName;
        argument2["level"] = std::string("status");
        argument2.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();

        socket.send(buffer);
    }

    void Connection::sendFCUnsubscribe()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("FCUnsubscribe");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString() << ", transaction ID: " << invokeId;

        socket.send(buffer);

        invokes[invokeId] = commandName.asString();
    }

    void Connection::sendOnFCUnubscribe()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onFCUnsubscribe");
        commandName.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("publish");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

        amf::Node argument3 = std::string("live");
        argument3.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onStatus");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2;
        argument2["clientid"] = std::string("Lavf57.1.0");
        argument2["code"] = std::string("NetStream.Publish.Start");
        argument2["description"] = streamName + " is now published";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode((amfVersion == amf::Version::AMF3) ? amf::Version::AMF3 : amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onStatus");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2;
        argument2["clientid"] = std::string("Lavf57.1.0");
        argument2["code"] = std::string("NetStream.Unpublish.Success");
        argument2["description"] = streamName + " stopped publishing";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    void Connection::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        sendAudioData(0, headerData);
    }

    void Connection::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        sendVideoData(0, headerData);

        // TODO: send video info
    }

    void Connection::sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& frameData)
    {
        sendAudioData(timestamp, frameData);
    }

    void Connection::sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& frameData, VideoFrameType frameType)
    {
        if (videoStream &&
            (videoFrameSent || frameType == VideoFrameType::KEY))
        {
            videoFrameSent = true;
            sendVideoData(timestamp, frameData);
        }
    }

    void Connection::sendMetaData(const amf::Node& newMetaData)
    {
        if (newMetaData.getType() == amf::Node::Type::Dictionary ||
            newMetaData.getType() == amf::Node::Type::Object)
        {
            metaData = amf::Node::Type::Dictionary;

            for (const std::pair<std::string, amf::Node>& value : newMetaData.asMap())
            {
                // not in the blacklist
                if (metaDataBlacklist.find(value.first) != metaDataBlacklist.end()) continue;

                // don't send audio meta data if audio stream is disabled
                if (!audioStream && (value.first == "audiocodecid" ||
                                     value.first == "audiodatarate")) continue;

                // don't send video meta data if video stream is disabled
                if (!videoStream && (value.first == "fps" ||
                                     value.first == "framerate" ||
                                     value.first == "gopsize" ||
                                     value.first == "level" ||
                                     value.first == "profile" ||
                                     value.first == "videocodecid" ||
                                     value.first == "videodatarate")) continue;

                metaData[value.first] = value.second;
            }

            rtmp::Packet packet;
            packet.channel = rtmp::Channel::AUDIO;
            packet.messageStreamId = streamId;
            packet.timestamp = 0;

            if (amfVersion == amf::Version::AMF0)
            {
                packet.messageType = rtmp::MessageType::AMF0_DATA;
            }
            else if (amfVersion == amf::Version::AMF3)
            {
                packet.messageType = rtmp::MessageType::AMF3_DATA;
                packet.data.push_back(0); // using AMF0
            }

            amf::Node commandName = std::string("@setDataFrame");
            commandName.encode(amf::Version::AMF0, packet.data);

            amf::Node argument1 = std::string("onMetaData");
            argument1.encode(amf::Version::AMF0, packet.data);

            amf::Node argument2 = metaData;
            argument2.encode(amf::Version::AMF0, packet.data);

            std::vector<uint8_t> buffer;
            encodePacket(buffer, outChunkSize, packet, sentPackets);

            {
                Log log(Log::Level::ALL);
                log << "[" << id << ", " << name << "] " << "Sending meta data " << commandName.asString() << ": ";
                argument2.dump(log);
            }

            socket.send(buffer);
        }
    }

    void Connection::sendTextData(uint64_t timestamp, const amf::Node& textData)
    {
        if (dataStream)
        {
            rtmp::Packet packet;
            packet.channel = rtmp::Channel::AUDIO;
            packet.messageStreamId = streamId;
            packet.timestamp = timestamp;

            if (amfVersion == amf::Version::AMF0)
            {
                packet.messageType = rtmp::MessageType::AMF0_DATA;
            }
            else if (amfVersion == amf::Version::AMF3)
            {
                packet.messageType = rtmp::MessageType::AMF3_DATA;
                packet.data.push_back(0); // using AMF0
            }

            amf::Node commandName = std::string("onTextData");
            commandName.encode(amf::Version::AMF0, packet.data);

            amf::Node argument1 = textData;
            argument1.encode(amf::Version::AMF0, packet.data);

            std::vector<uint8_t> buffer;
            encodePacket(buffer, outChunkSize, packet, sentPackets);

            {
                Log log(Log::Level::ALL);
                log << "[" << id << ", " << name << "] " << "Sending text data: ";
                argument1.dump(log);
            }
            
            socket.send(buffer);
        }
    }

    void Connection::sendGetStreamLength()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("getStreamLength");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    void Connection::sendGetStreamLengthResult(double transactionId)
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.timestamp = 0;

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("_result");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = 0.0;
        argument2.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    void Connection::sendPlay()
    {
        rtmp::Packet packet;
        packet.channel = rtmp::Channel::SYSTEM;
        packet.messageStreamId = streamId;
        packet.timestamp = 0;

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("play");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onStatus");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2;
        argument2["clientid"] = std::string("Lavf57.1.0");
        argument2["code"] = std::string("NetStream.Play.Start");
        argument2["description"] = streamName + " is now playing";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("stop");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = static_cast<double>(++invokeId);
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2 = streamName;
        argument2.encode(amf::Version::AMF0, packet.data);

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

        if (amfVersion == amf::Version::AMF0)
        {
            packet.messageType = rtmp::MessageType::AMF0_INVOKE;
        }
        else if (amfVersion == amf::Version::AMF3)
        {
            packet.messageType = rtmp::MessageType::AMF3_INVOKE;
            packet.data.push_back(0); // using AMF0
        }

        amf::Node commandName = std::string("onStatus");
        commandName.encode(amf::Version::AMF0, packet.data);

        amf::Node transactionIdNode = transactionId;
        transactionIdNode.encode(amf::Version::AMF0, packet.data);

        amf::Node argument1(amf::Node::Type::Null);
        argument1.encode(amf::Version::AMF0, packet.data);

        amf::Node argument2;
        argument2["clientid"] = std::string("Lavf57.1.0");
        argument2["code"] = std::string("NetStream.Play.Stop");
        argument2["description"] = streamName + " is now stopped";
        argument2["details"] = streamName;
        argument2["level"] = std::string("status");
        argument2.encode(amf::Version::AMF0, packet.data);

        std::vector<uint8_t> buffer;
        encodePacket(buffer, outChunkSize, packet, sentPackets);

        Log(Log::Level::ALL) << "[" << id << ", " << name << "] " << "Sending INVOKE " << commandName.asString();
        
        socket.send(buffer);
    }

    void Connection::sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        if (audioStream)
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
    }

    void Connection::sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        if (videoStream)
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
    }
}
