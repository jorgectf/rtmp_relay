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
    class PullSender
    {
        const std::string name = "Push";
    public:
        PullSender(cppsocket::Socket& aSocket,
                   const std::string& aApplication,
                   const std::string& aOverrideStreamName,
                   bool videoOutput,
                   bool audioOutput,
                   bool dataOutput,
                   const std::set<std::string>& aMetaDataBlacklist);

        void update(float delta);

        bool isConnected() const { return socket.isReady(); }

    private:
        void handleRead(cppsocket::Socket&, const std::vector<uint8_t>& newData);
        void handleClose(cppsocket::Socket&);

        bool handlePacket(const rtmp::Packet& packet);
        
        cppsocket::Socket& socket;

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

        std::random_device rd;
        std::mt19937 generator;

        uint32_t invokeId = 0;
        std::map<uint32_t, std::string> invokes;

        uint32_t streamId = 0;

        std::map<uint32_t, rtmp::Header> receivedPackets;
        std::map<uint32_t, rtmp::Header> sentPackets;
    };
}
