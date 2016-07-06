//
//  rtmp_relay
//

#pragma once

#include <random>
#include <string>
#include <vector>
#include <map>
#include "Socket.h"
#include "RTMP.h"
#include "Amf0.h"

namespace relay
{
    class Sender
    {
    public:
        Sender(Network& pNetwork, const std::string& pApplication, const std::string& pAddress, bool videoOutput, bool audioOutput, bool dataOutput);
        ~Sender();
        
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;
        
        Sender(Sender&& other) = delete;
        Sender& operator=(Sender&& other) = delete;
        
        bool connect();
        void disconnect();

        void update();
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
        
        Network& network;
        Socket socket;
        std::string application;
        std::string address;
        bool videoStream = false;
        bool audioStream = false;
        bool dataStream = false;
        
        std::vector<uint8_t> data;
        
        rtmp::State state = rtmp::State::UNINITIALIZED;
        
        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;

        bool connected = false;
        std::string streamName;

        bool streaming = false;
    };
}
