//
//  rtmp_relay
//

#pragma once

#include <map>
#include <set>
#include "Socket.hpp"
#include "RTMP.hpp"
#include "Amf.hpp"
#include "MersanneTwister.hpp"
#include "Status.hpp"
#include "Stream.hpp"
#include "Utils.hpp"

namespace relay
{
    class Relay;
    class Server;
    class Stream;
    struct Endpoint;

    class Connection
    {
        const std::string name = "Connection";
    public:
        enum class Type
        {
            HOST,
            CLIENT
        };

        enum class State
        {
            UNINITIALIZED = 0,
            VERSION_RECEIVED = 1,
            VERSION_SENT = 2,
            ACK_SENT = 3,
            HANDSHAKE_DONE = 4
        };

        Connection(Relay& aRelay, cppsocket::Socket& aSocket, Type aType);
        Connection(Relay& aRelay,
                   cppsocket::Socket& client);
        Connection(Relay& aRelay,
                   cppsocket::Socket& connector,
                   Stream& aStream,
                   const Endpoint& endpoint);

        Connection(const Connection&) = delete;
        Connection(Connection&&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection& operator=(Connection&&) = delete;

        ~Connection();

        void close();

        uint64_t getId() const { return id; }
        Type getType() const { return type; }
        Stream::Type getStreamType() const { return streamType; }
        const std::string& getApplicationName() const { return applicationName; }
        const std::string& getStreamName() const { return streamName; }

        bool isClosed() const;

        void update(float delta);

        void getStats(std::string& str, ReportType reportType) const;

        void connect();

        void setStream(Stream* aStream);
        void removeStream();
        void unpublishStream();

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& frameData);
        void sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& frameData, VideoFrameType frameType);
        void sendMetaData(const amf::Node& newMetaData);
        void sendTextData(uint64_t timestamp, const amf::Node& textData);

    private:
        void reset();

        void handleConnect(cppsocket::Socket&);
        void handleConnectError(cppsocket::Socket&);
        void handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket&);

        bool handlePacket(const rtmp::Packet& packet);

        void sendServerBandwidth();
        void sendClientBandwidth();
        void sendUserControl(rtmp::UserControlType userControlType, uint64_t timestamp = 0, uint32_t parameter1 = 0, uint32_t parameter2 = 0);
        void sendSetChunkSize();

        void sendOnBWDone();
        void sendCheckBW();
        void sendCheckBWResult(double transactionId);

        void sendConnect();
        void sendConnectResult(double transactionId);

        void sendCreateStream();
        void sendCreateStreamResult(double transactionId);
        void sendReleaseStream();
        void sendReleaseStreamResult(double transactionId);
        void sendDeleteStream();

        void sendFCPublish();
        void sendOnFCPublish();
        void sendFCUnpublish();
        void sendOnFCUnpublish();

        void sendFCSubscribe();
        void sendOnFCSubscribe();
        void sendFCUnsubscribe();
        void sendOnFCUnubscribe();

        void sendPublish();
        void sendPublishStatus(double transactionId);
        void sendUnublishStatus(double transactionId);

        void sendGetStreamLength();
        void sendGetStreamLengthResult(double transactionId);
        void sendPlay();
        void sendPlayStatus(double transactionId);
        void sendStop();
        void sendStopStatus(double transactionId);

        void sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData);

        Relay& relay;
        const uint64_t id;

        MersanneTwister& mersanneTwister;
        
        Type type;
        State state = State::UNINITIALIZED;
        std::vector<std::pair<uint32_t, uint16_t>> ipAddresses;
        std::vector<std::string> addresses;
        float connectionTimeout = 5.0f;
        float reconnectInterval = 5.0f;
        uint32_t reconnectCount = 0;
        float pingInterval = 60.0f;
        uint32_t bufferSize = 3000;
        cppsocket::Socket socket;

        float timeSincePing = 0.0f;
        float timeSinceConnect = 0.0f;
        uint32_t connectCount = 0;
        uint32_t addressIndex = 0;

        std::vector<uint8_t> data;

        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;
        uint32_t serverBandwidth = 2500000;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        Stream::Type streamType = Stream::Type::NONE;
        std::string applicationName;
        std::string streamName;
        std::string overrideApplicationName;
        std::string overrideStreamName;
        bool connected = false;

        bool videoStream = true;
        bool audioStream = true;
        bool dataStream = true;

        bool videoFrameSent = false;
        float timeSinceMeasure = 0.0f;
        uint64_t currentAudioBytes = 0;
        uint64_t currentVideoBytes = 0;
        uint64_t audioRate = 0;
        uint64_t videoRate = 0;

        Stream* stream = nullptr;
        amf::Node metaData;
        std::set<std::string> metaDataBlacklist;

        amf::Version amfVersion = amf::Version::AMF0;
    };
}