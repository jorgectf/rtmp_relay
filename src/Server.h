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
        struct InputDescription
        {
            Connection::Description connectionDescription;
            bool video = true;
            bool audio = true;
            bool data = true;
            std::string applicationName;
            std::string streamName;
        };

        struct OutputDescription
        {
            Connection::Description connectionDescription;
            bool video = true;
            bool audio = true;
            bool data = true;
            std::string overrideApplicationName;
            std::string overrideStreamName;
        };

        struct Description
        {
            std::vector<InputDescription> inputDescriptions;
            std::vector<OutputDescription> outputDescriptions;
        };

        Server(const Server::Description& aDescription);

        const Server::Description& getDescription() const { return description; }

        void createStream(const std::string& newStreamName);
        void deleteStream();
        void unpublishStream();

        void sendAudioHeader(const std::vector<uint8_t>& headerData);
        void sendVideoHeader(const std::vector<uint8_t>& headerData);
        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node& newMetaData);
        void sendTextData(uint64_t timestamp, const amf0::Node& textData);

    private:
        Server::Description description;

        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf0::Node metaData;

        std::vector<Connection*> connections;
    };
}
