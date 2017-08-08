//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Amf.hpp"
#include "Socket.hpp"
#include "Status.hpp"
#include "Utils.hpp"

namespace relay
{
    class Relay;
    class Server;
    class Connection;

    class Stream
    {
    public:
        enum class Type
        {
            NONE,
            INPUT,
            OUTPUT
        };

        Stream(Relay& aRelay,
               cppsocket::Network& aNetwork,
               Server& aServer,
               Type aType,
               const std::string& aApplicationName,
               const std::string& aStreamName);

        Stream(const Stream&) = delete;
        Stream(Stream&&) = delete;
        Stream& operator=(const Stream&) = delete;
        Stream& operator=(Stream&&) = delete;

        virtual ~Stream();

        Server& getServer() { return server; }
        Type getType() const { return type; }
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

        Type type;
        std::string applicationName;
        std::string streamName;

        Connection* inputConnection = nullptr;
        std::vector<Connection*> outputConnections;

        bool streaming = false;
        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf::Node metaData;

        std::vector<std::unique_ptr<Connection>> connections;
    };
}
