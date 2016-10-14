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

namespace relay
{
    class Sender
    {
        const std::string name = "Sender";
    public:
        Sender(cppsocket::Network& pNetwork,
               const std::string& pApplication,
               const std::string& newOverrideStreamName,
               const std::vector<std::string>& pAddresses,
               bool videoOutput,
               bool audioOutput,
               bool dataOutput,
               const std::set<std::string>& pMetaDataBlacklist,
               float pConnectionTimeout,
               float pReconnectInterval,
               uint32_t pReconnectCount);
        ~Sender();
        
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;
        
        Sender(Sender&& other) = delete;
        Sender& operator=(Sender&& other) = delete;
        
        bool connect();
        void disconnect();

        void update(float delta);
        void handleConnect();
        
        bool sendPacket(const std::vector<uint8_t>& packet);

        void printInfo() const;

        void createStream(const std::string& newStreamName);
        void deleteStream();
        void unpublishStream();
        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node& metaData);
        void sendTextData(const amf0::Node& textData);
        
    private:
        void reset();
        void handleRead(const std::vector<uint8_t>& newData);
        void handleClose();

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
    };
}
