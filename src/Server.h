//
//  rtmp_relay
//

#pragma once

#include <vector>
#include "Connection.h"
#include "Stream.h"

namespace relay
{
    class Server
    {
    public:
        Server(Relay& aRelay, cppsocket::Network& aNetwork);

        uint64_t getId() const { return id; }

        Stream* findStream(Connection::StreamType type,
                           const std::string& applicationName,
                           const std::string& streamName) const;
        Stream* createStream(Connection::StreamType type,
                             const std::string& applicationName,
                             const std::string& streamName);
        void releaseStream(Stream* stream);

        void start(const std::vector<Connection::Description>& aConnectionDescriptions);

        void update(float delta);
        void getStats(std::string& str, ReportType reportType) const;

        void startStreaming(Connection& connection);
        void stopStreaming(Connection& connection);

        void startReceiving(Connection& connection);
        void stopReceiving(Connection& connection);

        const std::vector<Connection::Description>& getConnectionDescriptions() const { return connectionDescriptions; }

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& videoData, VideoFrameType frameType);
        void sendMetaData(const amf::Node& newMetaData);
        void sendTextData(uint64_t timestamp, const amf::Node& textData);

    private:
        Relay& relay;
        const uint64_t id;

        cppsocket::Network& network;
        std::vector<Connection::Description> connectionDescriptions;

        Connection* inputConnection = nullptr;
        std::vector<Connection*> outputConnections;

        bool streaming = false;
        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf::Node metaData;

        std::vector<std::unique_ptr<Stream>> streams;
        std::vector<std::unique_ptr<Connection>> connections;
    };
}
