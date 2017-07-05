//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Connection.h"
#include "Amf.h"

namespace relay
{
    class Server;

    class Stream
    {
    public:
        Stream(Relay& aRelay,
               cppsocket::Network& aNetwork,
               Server& aServer,
               Connection::StreamType aType,
               const std::string& aApplicationName,
               const std::string& aStreamName);

        Connection::StreamType getType() const { return type; }
        const std::string& getApplicationName() const { return applicationName; }
        const std::string& getStreamName() const { return streamName; }

        void getStats(std::string& str, ReportType reportType) const;

        void startStreaming(Connection& connection);
        void stopStreaming(Connection& connection);

        void startReceiving(Connection& connection);
        void stopReceiving(Connection& connection);

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& videoData, VideoFrameType frameType);
        void sendMetaData(const amf::Node& newMetaData);
        void sendTextData(uint64_t timestamp, const amf::Node& textData);

    private:
        const uint64_t id;

        Relay& relay;
        cppsocket::Network& network;
        Server& server;

        Connection::StreamType type;
        std::string applicationName;
        std::string streamName;

        Connection* inputConnection = nullptr;
        std::vector<Connection*> outputConnections;

        bool streaming = false;
        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf::Node metaData;

        std::vector<Connection> connections;
    };
}
