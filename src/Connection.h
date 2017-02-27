//
//  rtmp_relay
//

#pragma once

#include <random>
#include <map>
#include <set>
#include "Socket.h"
#include "RTMP.h"
#include "Amf0.h"
#include "Status.h"

namespace relay
{
    class Relay;
    class Server;

    class Connection
    {
        const std::string name = "Connection";
    public:
        enum class Type
        {
            HOST,
            CLIENT
        };

        struct Description
        {
            Type type;
            std::vector<std::pair<uint32_t, uint16_t>> addresses;
            float connectionTimeout = 5.0f;
            float reconnectInterval = 5.0f;
            uint32_t reconnectCount = 0;
            float pingInterval = 0.0f;
        };

        enum class StreamType
        {
            NONE,
            INPUT,
            OUTPUT
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
                   cppsocket::Socket& client,
                   float aPingInterval);
        Connection(Relay& aRelay,
                   cppsocket::Socket& connector,
                   const std::pair<uint32_t, uint16_t>& aAddress,
                   float aConnectionTimeout,
                   float aReconnectInterval,
                   StreamType aStreamType,
                   Server& aServer,
                   const std::string& aApplicationName,
                   const std::string& aStreamName);
        ~Connection();

        Type getType() const { return type; }
        StreamType getStreamType() const { return streamType; }
        const std::string& getApplicationName() const { return applicationName; }
        const std::string& getStreamName() const { return streamName; }

        void update(float delta);

        void getInfo(std::string& str, ReportType reportType) const;

        void createStream(const std::string& newStreamName);
        void deleteStream();
        void unpublishStream();

        void sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node metaData);
        void sendTextData(uint64_t timestamp, const amf0::Node& textData);

    private:
        void handleConnect(cppsocket::Socket&);
        void handleConnectError(cppsocket::Socket&);
        void handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket&);

        bool handlePacket(const rtmp::Packet& packet);

        void sendServerBandwidth();
        void sendClientBandwidth();
        void sendPing();
        void sendSetChunkSize();

        void sendBWDone();
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
        void sendPublish();
        void sendPublishStatus(double transactionId);

        void sendPlay();
        void sendPlayStatus(double transactionId);
        void sendStop();
        void sendStopStatus(double transactionId);

        Relay& relay;
        const uint64_t id;

        std::random_device rd;
        std::mt19937 generator;
        
        Type type;
        State state;
        std::pair<uint32_t, uint16_t> address;
        float pingInterval = 5.0f;
        float connectionTimeout = 5.0f;
        float reconnectInterval = 5.0f;
        cppsocket::Socket socket;

        float timeSincePing = 0.0f;
        float timeSinceConnect = 0.0f;

        std::vector<uint8_t> data;

        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;
        uint32_t serverBandwidth = 2500000;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        StreamType streamType = StreamType::NONE;
        std::string applicationName;
        std::string streamName;
        std::string overrideApplicationName;
        std::string overrideStreamName;
        bool connected = false;

        float timeSinceMeasure = 0.0f;
        uint64_t currentAudioBytes = 0;
        uint64_t currentVideoBytes = 0;
        uint64_t audioRate = 0;
        uint64_t videoRate = 0;

        Server* server = nullptr;
        std::set<std::string> metaDataBlacklist;

    };
}
