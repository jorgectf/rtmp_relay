//
//  rtmp_relay
//

#include "Connection.h"
#include "Relay.h"
#include "Constants.h"
#include "Amf0.h"
#include "Utils.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    Connection::Connection(cppsocket::Socket& aSocket, Type aType):
        id(Relay::nextId()),
        generator(rd()),
        socket(aSocket),
        type(aType)
    {
        if (!socket.setBlocking(false))
        {
            Log(Log::Level::ERR) << "[" << id << ", " << name << "] " << "Failed to set socket non-blocking";
        }

        // TODO: implement
        // socket.setConnectTimeout(connectionTimeout);
        //socket.setConnectCallback(std::bind(&Connection::handleConnect, this, std::placeholders::_1));
        //socket.setConnectErrorCallback(std::bind(&Connection::handleConnectError, this, std::placeholders::_1));
        socket.setReadCallback(std::bind(&Connection::handleRead, this, std::placeholders::_1, std::placeholders::_2));
        socket.setCloseCallback(std::bind(&Connection::handleClose, this, std::placeholders::_1));

        // handshake
        if (type == Type::PUSH)
        {
            Log(Log::Level::INFO) << "[" << id << ", " << name << "] " << "Connected to " << ipToString(socket.getIPAddress()) << ":" << socket.getPort();

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

    void Connection::update()
    {
    }

    void Connection::handleConnect(cppsocket::Socket&)
    {
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
            else if (type == Type::PULL)
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
            else if (type == Type::PUSH)
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

                        // TODO: implement
                        //timeSinceHandshake = 0.0f;
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

                        // TODO: implement
                        //timeSinceHandshake = 0.0f;
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

                        // TODO: implement
                        //sendConnect();

                        // TODO: implement
                        //timeSinceHandshake = 0.0f;
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

                if (command.asString() == "onBWDone")
                {
                    // TODO: implement
                    //sendCheckBW();
                }
                else if (command.asString() == "onFCPublish")
                {
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

                        // TODO: implement
                        /*if (i->second == "connect")
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
                            
                            sendMetaData();
                            sendAudioHeader();
                            sendVideoHeader();
                        }*/
                        
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
}
