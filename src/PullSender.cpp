//
//  rtmp_relay
//

#include "PullSender.h"
#include "Constants.h"
#include "Log.h"

using namespace cppsocket;

namespace relay
{
    PullSender::PullSender(cppsocket::Socket& aSocket,
                           const std::string& aApplication,
                           const std::string& aOverrideStreamName,
                           bool videoOutput,
                           bool audioOutput,
                           bool dataOutput,
                           const std::set<std::string>& aMetaDataBlacklist):
        socket(aSocket),
        generator(rd()),
        application(aApplication),
        overrideStreamName(aOverrideStreamName),
        videoStream(videoOutput),
        audioStream(audioOutput),
        dataStream(dataOutput),
        metaDataBlacklist(aMetaDataBlacklist)
    {
        if (!socket.setBlocking(false))
        {
            Log(Log::Level::ERR) << "[" << name << "] " << "Failed to set socket non-blocking";
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
    }

    void PullSender::handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData)
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
            if (socket.isReady())
            {
                Log(Log::Level::ERR) << "[" << name << "] " << "Reading outside of the buffer, buffer size: " << static_cast<uint32_t>(data.size()) << ", data size: " << offset;
            }
            
            data.clear();
        }
        else
        {
            data.erase(data.begin(), data.begin() + offset);
            
            Log(Log::Level::ALL) << "[" << name << "] " << "Remaining data " << data.size();
        }
    }

    void PullSender::handleClose(cppsocket::Socket&)
    {
        Log(Log::Level::INFO) << "[" << name << "] " << "Input from " << ipToString(socket.getIPAddress()) << ":" << socket.getPort() << " disconnected";
    }

    bool PullSender::handlePacket(const rtmp::Packet& packet)
    {
        switch (packet.messageType)
        {
            default:
            {
                Log(Log::Level::ERR) << "[" << name << "] " << "Unhandled message: " << static_cast<uint32_t>(packet.messageType);
                break;
            }
        }

        return true;
    }
}
