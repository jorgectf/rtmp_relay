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

namespace relay
{
    class Server;

    class Receiver
    {
        const std::string name = "Receiver";
    public:
        Receiver(cppsocket::Socket& pSocket, const std::string& pApplication, const std::shared_ptr<Server>& pServer);
        ~Receiver();
        
        Receiver(const Receiver&) = delete;
        Receiver& operator=(const Receiver&) = delete;
        
        Receiver(Receiver&& other) = delete;
        Receiver& operator=(Receiver&& other) = delete;
        
        void update();
        
        bool getPacket(std::vector<uint8_t>& packet);
        
        bool isConnected() const { return socket.isReady(); }

        void printInfo() const;
        
    protected:
        void handleRead(const std::vector<uint8_t>& newData);
        void handleClose();

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

        std::string application;
        std::string streamName;

        std::weak_ptr<Server> server;

        amf0::Node metaData;
    };
}
