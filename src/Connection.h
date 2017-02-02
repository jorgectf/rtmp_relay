//
//  rtmp_relay
//

#pragma once

#include <random>
#include <map>
#include "Socket.h"
#include "Connector.h"
#include "RTMP.h"
#include "Amf0.h"

namespace relay
{
    class Relay;
    
    class Connection
    {
        const std::string name = "Connection";
    public:
        enum class ConnectionType
        {
            PUSH,
            PULL
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

        Connection(Relay& aRelay, cppsocket::Socket& aSocket, ConnectionType aConnectionType);
        Connection(Relay& aRelay, cppsocket::Socket& client);
        Connection(Relay& aRelay, cppsocket::Connector& connector);

        void update();

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

        void sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData);

        void sendPlay();
        void sendPlayStatus(double transactionId);
        void sendStop();
        void sendStopStatus(double transactionId);

        Relay& relay;
        const uint64_t id;

        std::random_device rd;
        std::mt19937 generator;
        
        ConnectionType connectionType;
        State state;
        cppsocket::Socket& socket;

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

        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf0::Node metaData;

        float timeSinceMeasure = 0.0f;
        uint64_t currentAudioBytes = 0;
        uint64_t currentVideoBytes = 0;
        uint64_t audioRate = 0;
        uint64_t videoRate = 0;
    };
}
