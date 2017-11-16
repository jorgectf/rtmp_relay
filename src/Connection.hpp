//
//  rtmp_relay
//

#pragma once

#include <map>
#include <set>
#include "Socket.hpp"
#include "RTMP.hpp"
#include "Amf.hpp"
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
        enum class Direction
        {
            NONE,
            INPUT,
            OUTPUT
        };

        enum class Type
        {
            HOST, // connection to RTMP relay
            CLIENT // connection to different host
        };

        enum class State
        {
            UNINITIALIZED = 0,
            VERSION_RECEIVED = 1,
            VERSION_SENT = 2,
            ACK_SENT = 3,
            HANDSHAKE_DONE = 4
        };

        Connection(Relay& aRelay,
                   cppsocket::Socket& client);
        Connection(Relay& aRelay,
                   Stream& aStream,
                   const Endpoint& aEndpoint);

        Connection(const Connection&) = delete;
        Connection(Connection&&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection& operator=(Connection&&) = delete;

        ~Connection();

        void close(bool forceClose = false);
        void reset();

        uint64_t getId() const { return id; }
        Type getType() const { return type; }
        Direction getDirection() const { return direction; }
        const std::string& getApplicationName() const { return applicationName; }
        const std::string& getStreamName() const { return streamName; }

        bool isClosed() const;
        bool isConnected() { return connected; }

        void update(float delta);

        void getStats(std::string& str, ReportType reportType) const;

        void connect();

        void setStream(Stream* aStream);
        Stream* getStream() { return stream; }
        void unpublishStream();

        bool sendAudioHeader(const std::vector<uint8_t>& headerData);
        bool sendVideoHeader(const std::vector<uint8_t>& headerData);
        bool sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& frameData);
        bool sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& frameData, VideoFrameType frameType);
        bool sendMetaData(const amf::Node& newMetaData);
        bool sendTextData(uint64_t timestamp, const amf::Node& textData);

        bool isDependable();

    private:
        void resolveStreamName();
        void updateIdString();

        void handleConnect(cppsocket::Socket&);
        void handleConnectError(cppsocket::Socket&);
        void handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket&);

        bool handlePacket(const rtmp::Packet& packet);

        bool sendServerBandwidth();
        bool sendClientBandwidth();
        bool sendUserControl(rtmp::UserControlType userControlType, uint64_t timestamp = 0, uint32_t parameter1 = 0, uint32_t parameter2 = 0);
        bool sendSetChunkSize();

        bool sendOnBWDone();
        bool sendCheckBW();
        bool sendCheckBWResult(double transactionId);

        bool sendConnect();
        bool sendConnectResult(double transactionId);

        bool sendCreateStream();
        bool sendCreateStreamResult(double transactionId);
        bool sendReleaseStream();
        bool sendReleaseStreamResult(double transactionId);
        bool sendDeleteStream();

        bool sendFCPublish();
        bool sendOnFCPublish();
        bool sendFCUnpublish();
        bool sendOnFCUnpublish();

        bool sendFCSubscribe();
        bool sendOnFCSubscribe();
        bool sendFCUnsubscribe();
        bool sendOnFCUnubscribe();

        bool sendPublish();
        bool sendPublishStatus(double transactionId);
        bool sendUnublishStatus(double transactionId);

        bool sendGetStreamLength();
        bool sendGetStreamLengthResult(double transactionId);
        bool sendPlay();
        bool sendPlayStatus(double transactionId);
        bool sendStop();
        bool sendStopStatus(double transactionId);

        bool sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        bool sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData);

        Relay& relay;
        const uint64_t id;

        Type type;
        State state = State::UNINITIALIZED;
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

        Connection::Direction direction = Connection::Direction::NONE;
        std::string applicationName;
        std::string streamName;
        bool connected = false;
        bool closed = false;
        bool streaming = false;

        bool videoFrameSent = false;
        float timeSinceMeasure = 0.0f;
        uint64_t currentAudioBytes = 0;
        uint64_t currentVideoBytes = 0;
        uint64_t audioRate = 0;
        uint64_t videoRate = 0;

        const Endpoint* endpoint = nullptr;
        Stream* stream = nullptr;
        amf::Node metaData;

        amf::Version amfVersion = amf::Version::AMF0;

        std::string idString;
    };
}
