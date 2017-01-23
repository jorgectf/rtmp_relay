//
//  rtmp_relay
//

#pragma once

#include <random>
#include <vector>
#include <string>
#include <set>
#include "Socket.h"
#include "RTMP.h"
#include "Amf0.h"
#include "Status.h"

namespace relay
{
    struct PullDescriptor
    {
        std::string overrideStreamName;
        std::string address;
        bool videoOutput;
        bool audioOutput;
        bool dataOutput;
        std::set<std::string> metaDataBlacklist;
        float pingInterval;
    };
    
    class PullSender
    {
        const std::string name = "PullSender";
    public:
        PullSender(cppsocket::Socket& aSocket,
                   const std::string& aApplication,
                   const PullDescriptor& pullDescriptor);

        PullSender(const PullSender&) = delete;
        PullSender& operator=(const PullSender&) = delete;

        PullSender(PullSender&& other) = delete;
        PullSender& operator=(PullSender&& other) = delete;

        void update(float delta);

        bool isConnected() const { return socket.isReady(); }

        void createStream(const std::string& newStreamName);
        void deleteStream();
        void unpublishStream();

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node& metaData);
        void sendTextData(uint64_t timestamp, const amf0::Node& textData);

        void getInfo(std::string& str, ReportType reportType) const;
        
    private:
        void sendAudioHeader();
        void sendVideoHeader();
        void sendMetaData();

        void sendAudioData(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideoData(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        
        void handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket&);

        bool handlePacket(const rtmp::Packet& packet);

        void sendServerBandwidth();
        void sendClientBandwidth();
        void sendPing();
        void sendSetChunkSize();

        void sendConnectResult(double transactionId);
        void sendBWDone();
        void sendCheckBWResult(double transactionId);
        void sendCreateStreamResult(double transactionId);
        void sendReleaseStreamResult(double transactionId);

        void sendPlayStatus(double transactionId);
        void sendStopStatus(double transactionId);

        bool connect(const std::string& applicationName);
        bool play(const std::string& stream);

        const uint64_t id;
        
        std::random_device rd;
        std::mt19937 generator;

        cppsocket::Socket socket;

        const std::string application;
        const std::string overrideStreamName;
        const bool videoStream;
        const bool audioStream;
        const bool dataStream;
        std::set<std::string> metaDataBlacklist;

        std::vector<uint8_t> data;

        rtmp::State state = rtmp::State::UNINITIALIZED;

        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;
        uint32_t serverBandwidth = 2500000;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;

        bool connected = false;
        std::string streamName;
        const float pingInterval;
        float timeSincePing = 0.0f;

        bool streaming = false;

        std::vector<uint8_t> audioHeader;
        bool audioHeaderSent = false;
        std::vector<uint8_t> videoHeader;
        bool videoHeaderSent = false;
        bool videoFrameSent = false;

        amf0::Node metaData;
        bool metaDataSent = false;
    };
}
