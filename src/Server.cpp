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

    void Server::addConnection(Connection& connection)
    {
        auto i = std::find(connections.begin(), connections.end(), &connection);

        if (i == connections.end())
        {
            connections.push_back(&connection);

            if (connection.getStreamType() == Connection::StreamType::INPUT)
            {
                for (Connection* currentConnection : connections)
                {
                    if (currentConnection->getStreamType() == Connection::StreamType::OUTPUT)
                    {
                        currentConnection->createStream(connection.getStreamName());
                    }
                }

                // TODO: create all push connections
            }
        }
    }

    void Server::removeConnection(Connection& connection)
    {
        auto i = std::find(connections.begin(), connections.end(), &connection);

        if (i != connections.end())
        {
            connections.erase(i);

            if (connection.getStreamType() == Connection::StreamType::INPUT)
            {
                for (Connection* currentConnection : connections)
                {
                    if (currentConnection->getStreamType() == Connection::StreamType::OUTPUT)
                    {
                        currentConnection->deleteStream();
                        currentConnection->unpublishStream();
                    }
                }

                // TODO: release all push connections
            }
        }
    }

    void Server::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;

        for (Connection* connection : connections)
        {
            if (connection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                connection->sendAudioData(0, headerData);
            }
        }
    }

    void Server::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        videoHeader = headerData;

        for (Connection* connection : connections)
        {
            if (connection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                connection->sendVideoData(0, headerData);
            }
        }
    }

    void Server::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (Connection* connection : connections)
        {
            if (connection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                connection->sendAudioData(timestamp, audioData);
            }
        }
    }

    void Server::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (Connection* connection : connections)
        {
            if (connection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                connection->sendVideoData(timestamp, videoData);
            }
        }
    }

    void Server::sendMetaData(const amf0::Node& newMetaData)
    {
        metaData = newMetaData;

        for (Connection* connection : connections)
        {
            if (connection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                connection->sendMetaData(metaData);
            }
        }
    }

    void Server::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        for (Connection* connection : connections)
        {
            if (connection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                connection->sendTextData(timestamp, textData);
            }
        }
    }
}
