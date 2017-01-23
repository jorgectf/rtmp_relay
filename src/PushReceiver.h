//
//  rtmp_relay
//

#pragma once

#include <random>
#include <vector>
#include <map>
#include <memory>
#include "Socket.h"
#include "RTMP.h"
#include "Amf0.h"
#include "Application.h"

namespace relay
{
    class Server;

    class PushReceiver
    {
        const std::string name = "Receiver";
    public:
        PushReceiver(cppsocket::Network& aNetwork,
                     cppsocket::Socket& aSocket,
                     float aPingInterval,
                     const std::vector<ApplicationDescriptor>& aApplicationDescriptors);

        void reset();

        PushReceiver(const PushReceiver&) = delete;
        PushReceiver& operator=(const PushReceiver&) = delete;

        PushReceiver(PushReceiver&& other) = delete;
        PushReceiver& operator=(PushReceiver&& other) = delete;

        void update(float delta);

        bool getPacket(std::vector<uint8_t>& packet);

        bool isConnected() const { return socket.isReady(); }

        void getInfo(std::string& str, ReportType reportType) const;

    private:
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
        void sendOnFCPublish();
        void sendPublishStatus(double transactionId);

        bool connect(const std::string& applicationName);

        const uint64_t id;

        cppsocket::Network& network;
        cppsocket::Socket socket;

        std::vector<uint8_t> data;

        rtmp::State state = rtmp::State::UNINITIALIZED;

        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;
        uint32_t serverBandwidth = 2500000;

        std::random_device rd;
        std::mt19937 generator;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;

        std::string streamName;
        const float pingInterval;
        float timeSincePing = 0.0f;

        bool streaming = false;

        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf0::Node metaData;

        std::vector<ApplicationDescriptor> applicationDescriptors;
        std::unique_ptr<Application> application;

        float timeSinceMeasure = 0.0f;
        uint64_t currentAudioBytes = 0;
        uint64_t currentVideoBytes = 0;
        uint64_t audioRate = 0;
        uint64_t videoRate = 0;
    };
}
