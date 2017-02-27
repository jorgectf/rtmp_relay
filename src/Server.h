//
//  rtmp_relay
//

#pragma once

#include "Connection.h"

namespace relay
{
    class Server
    {
    public:
        struct Description
        {
            std::vector<Connection::Description> inputDescriptions;
            std::vector<Connection::Description> outputDescriptions;
        };

        Server(Relay& aRelay,
               cppsocket::Network& aNetwork,
               const Server::Description& aDescription);

        void startStreaming(Connection& connection);
        void stopStreaming(Connection& connection);

        void startReceiving(Connection& connection);
        void stopReceiving(Connection& connection);

        const Server::Description& getDescription() const { return description; }

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node& newMetaData);
        void sendTextData(uint64_t timestamp, const amf0::Node& textData);

    private:
        Relay& relay;
        cppsocket::Network& network;
        Server::Description description;

        Connection* inputConnection = nullptr;
        std::vector<Connection*> outputConnections;

        std::string applicationName;
        std::string streamName;

        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf0::Node metaData;

        std::vector<std::unique_ptr<Connection>> connections;
    };
}
