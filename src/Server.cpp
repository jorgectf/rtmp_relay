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
        for (const Connection* connection : connections)
        {
            //connection->createStream(newStreamName);
        }
    }

    void Server::deleteStream()
    {
        for (const Connection* connection : connections)
        {
            //connection->deleteStream();
        }
    }

    void Server::unpublishStream()
    {
        for (const Connection* connection : connections)
        {
            //connection->unpublishStream();
        }
    }

    void Server::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        for (const Connection* connection : connections)
        {
            //connection->sendAudioHeader(headerData);
        }
    }

    void Server::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        for (const Connection* connection : connections)
        {
            //connection->sendVideoHeader(headerData);
        }
    }

    void Server::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (const Connection* connection : connections)
        {
            //connection->sendAudio(timestamp, audioData);
        }
    }

    void Server::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (const Connection* connection : connections)
        {
            //connection->sendVideo(timestamp, videoData);
        }
    }

    void Server::sendMetaData(const amf0::Node& metaData)
    {
        for (const Connection* connection : connections)
        {
            //connection->sendMetaData(metaData);
        }
    }

    void Server::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        for (const Connection* connection : connections)
        {
            //connection->sendTextData(timestamp, textData);
        }
    }

}
