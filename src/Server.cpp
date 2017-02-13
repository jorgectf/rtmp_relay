//
//  rtmp_relay
//

#include "Server.h"

namespace relay
{
    Server::Server(const Server::Description& aDescription):
        description(aDescription)
    {
    }

    void Server::createStream(const std::string& newStreamName)
    {
        for (Connection* connection : connections)
        {
            connection->createStream(newStreamName);
        }
    }

    void Server::deleteStream()
    {
        for (Connection* connection : connections)
        {
            connection->deleteStream();
        }
    }

    void Server::unpublishStream()
    {
        for (Connection* connection : connections)
        {
            connection->unpublishStream();
        }
    }

    void Server::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;

        for (Connection* connection : connections)
        {
            connection->sendAudioData(0, headerData);
        }
    }

    void Server::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        videoHeader = headerData;

        for (Connection* connection : connections)
        {
            connection->sendVideoData(0, headerData);
        }
    }

    void Server::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (Connection* connection : connections)
        {
            connection->sendAudioData(timestamp, audioData);
        }
    }

    void Server::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (Connection* connection : connections)
        {
            connection->sendVideoData(timestamp, videoData);
        }
    }

    void Server::sendMetaData(const amf0::Node& newMetaData)
    {
        metaData = newMetaData;

        for (Connection* connection : connections)
        {
            connection->sendMetaData(metaData);
        }
    }

    void Server::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        for (Connection* connection : connections)
        {
            connection->sendTextData(timestamp, textData);
        }
    }

}
