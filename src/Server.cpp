//
//  rtmp_relay
//

#include <algorithm>
#include "Server.h"

namespace relay
{
    Server::Server(const Server::Description& aDescription):
        description(aDescription)
    {
    }

    void Server::startStreaming(Connection& connection)
    {
        inputConnection = &connection;

        for (Connection* outputConnection : outputConnections)
        {
            outputConnection->createStream(inputConnection->getStreamName());
        }

        // TODO: create all push connections
    }

    void Server::stopStreaming(Connection& connection)
    {
        if (&connection == inputConnection)
        {
            for (Connection* outputConnection : outputConnections)
            {
                outputConnection->deleteStream();
                outputConnection->unpublishStream();
            }

            // TODO: release all push connections
        }
    }

    void Server::startReceiving(Connection& connection)
    {
        auto i = std::find(outputConnections.begin(), outputConnections.end(), &connection);

        if (i == outputConnections.end())
        {
            outputConnections.push_back(&connection);
        }
    }

    void Server::stopReceiving(Connection& connection)
    {
        auto i = std::find(outputConnections.begin(), outputConnections.end(), &connection);

        if (i != outputConnections.end())
        {
            outputConnections.erase(i);
        }
    }

    void Server::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendAudioData(0, headerData);
            }
        }
    }

    void Server::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        videoHeader = headerData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendVideoData(0, headerData);
            }
        }
    }

    void Server::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendAudioData(timestamp, audioData);
            }
        }
    }

    void Server::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendVideoData(timestamp, videoData);
            }
        }
    }

    void Server::sendMetaData(const amf0::Node& newMetaData)
    {
        metaData = newMetaData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendMetaData(metaData);
            }
        }
    }

    void Server::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendTextData(timestamp, textData);
            }
        }
    }
}
