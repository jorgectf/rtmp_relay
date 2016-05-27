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

namespace relay
{
    class Sender
    {
    public:
        Sender(Network& pNetwork, const std::string& pApplication);
        ~Sender();
        
        Sender(const Sender&) = delete;
        Sender& operator=(const Sender&) = delete;
        
        Sender(Sender&& other) = delete;
        Sender& operator=(Sender&& other) = delete;
        
        bool init(const std::string& address);
        void update();
        void handleConnect();
        
        bool sendPacket(const std::vector<uint8_t>& packet);

        void printInfo() const;

        void createStream(const std::string& newStreamName);
        
    private:
        void handleRead(const std::vector<uint8_t>& newData);
        void handleClose();

        bool handlePacket(const rtmp::Packet& packet);

        void sendConnect();
        void sendSetChunkSize();
        void sendCheckBW();

        void sendCreateStream();
        void sendReleaseStream();
        void sendFCPublish();
        void sendPublish();
        
        Network& network;
        Socket socket;
        
        std::vector<uint8_t> data;
        
        rtmp::State state = rtmp::State::UNINITIALIZED;
        
        uint32_t inChunkSize = 128;
        uint32_t outChunkSize = 128;
        
        std::random_device rd;
        std::mt19937 generator;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        std::map<rtmp::Channel, rtmp::Header> previousPackets;

        bool connected = false;
        std::string application;
        std::string streamName;
    };
}
