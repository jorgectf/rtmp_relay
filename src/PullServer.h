//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <string>
#include <set>
#include <memory>
#include "Acceptor.h"
#include "PullSender.h"

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
    
    class PullServer
    {
        const std::string name = "PullServer";
    public:
        PullServer(cppsocket::Network& aNetwork,
                   const std::string& aApplication,
                   const std::string& aOverrideStreamName,
                   const std::string& aAddress,
                   bool videoOutput,
                   bool audioOutput,
                   bool dataOutput,
                   const std::set<std::string>& aMetaDataBlacklist,
                   float aPingInterval);

        void update(float delta);

        void createStream(const std::string& newStreamName);
        void deleteStream();
        void unpublishStream();

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node& newMetaData);
        void sendTextData(uint64_t timestamp, const amf0::Node& textData);

        void getInfo(std::string& str, ReportType reportType) const;
        
    private:
        void handleAccept(cppsocket::Socket& clientSocket);

        const uint64_t id;
        
        cppsocket::Network& network;
        cppsocket::Acceptor socket;

        const std::string application;
        const std::string overrideStreamName;
        const std::string address;
        const bool videoStream;
        const bool audioStream;
        const bool dataStream;
        std::set<std::string> metaDataBlacklist;

        std::string streamName;
        const float pingInterval;

        std::vector<std::unique_ptr<PullSender>> pullSenders;

        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf0::Node metaData;
    };
}
