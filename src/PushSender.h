//
//  rtmp_relay
//

#pragma once

#include <random>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "Connector.h"
#include "RTMP.h"
#include "Amf0.h"
#include "Status.h"

namespace relay
{
    struct PushDescriptor
    {
        std::string overrideStreamName;
        std::vector<std::string> addresses;
        bool videoOutput;
        bool audioOutput;
        bool dataOutput;
        std::set<std::string> metaDataBlacklist;
        float connectionTimeout;
        float reconnectInterval;
        uint32_t reconnectCount;
    };

    class PushSender
    {
        const std::string name = "PushSender";
    public:
        PushSender(cppsocket::Network& aNetwork,
                   const std::string& aApplication,
                   const std::string& aOverrideStreamName,
                   const std::vector<std::string>& aAddresses,
                   bool videoOutput,
                   bool audioOutput,
                   bool dataOutput,
                   const std::set<std::string>& aMetaDataBlacklist,
                   float aConnectionTimeout,
                   float aReconnectInterval,
                   uint32_t aReconnectCount);

        PushSender(const PushSender&) = delete;
        PushSender& operator=(const PushSender&) = delete;

        PushSender(PushSender&& other) = delete;
        PushSender& operator=(PushSender&& other) = delete;

        bool connect();
        void disconnect();

        void update(float delta);

        bool sendPacket(const std::vector<uint8_t>& packet);

        void getInfo(std::string& str, ReportType reportType) const;

        void createStream(const std::string& newStreamName);
        void deleteStream();
        void unpublishStream();

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);

        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendTextData(uint64_t timestamp, const amf0::Node& textData);

        void sendMetaData(const amf0::Node& newMetaData);

    private:
        void sendAudioHeader();
        void sendVideoHeader();
        void sendMetaData();

        void sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData);

        void reset();
        void handleConnect();
        void handleConnectError();
        void handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket&);

        bool handlePacket(const rtmp::Packet& packet);

        void sendConnect();
        void sendSetChunkSize();
        void sendCheckBW();

        void sendCreateStream();
        void sendReleaseStream();
        void sendDeleteStream();

        void sendFCPublish();
        void sendFCUnpublish();
        void sendPublish();

        std::random_device rd;
        std::mt19937 generator;

        cppsocket::Network& network;
        cppsocket::Connector socket;
        const std::string application;
        const std::string overrideStreamName;

        const std::vector<std::string> addresses;
        uint32_t addressIndex = 0;
        uint32_t connectCount = 0;

        const bool videoStream = false;
        const bool audioStream = false;
        const bool dataStream = false;
        std::set<std::string> metaDataBlacklist;
        const float connectionTimeout;
        const float reconnectInterval;
        const uint32_t reconnectCount;

        std::vector<uint8_t> data;

        rtmp::State state = rtmp::State::UNINITIALIZED;

        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;

        bool active = false;
        bool connected = false;
        std::string streamName;

        bool streaming = false;
        float timeSinceConnect = 0.0f;
        float timeSinceHandshake = 0.0f;

        std::vector<uint8_t> audioHeader;
        bool audioHeaderSent = false;
        std::vector<uint8_t> videoHeader;
        bool videoHeaderSent = false;
        bool videoFrameSent = false;

        amf0::Node metaData;
        bool metaDataSent = false;
    };
}
