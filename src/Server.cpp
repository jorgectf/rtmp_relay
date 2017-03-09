//
//  rtmp_relay
//

#include <algorithm>
#include "Server.h"
#include "Relay.h"

using namespace cppsocket;

namespace relay
{
    Server::Server(Relay& aRelay,
                   cppsocket::Network& aNetwork):
        relay(aRelay),
        network(aNetwork)
    {
    }

    void Server::start(const std::vector<Connection::Description>& aConnectionDescriptions)
    {
        connectionDescriptions = aConnectionDescriptions;

        for (const Connection::Description& connectionDescription : connectionDescriptions)
        {
            if (connectionDescription.type == Connection::Type::CLIENT &&
                connectionDescription.streamType == Connection::StreamType::INPUT)
            {
                Socket socket(network);

                std::unique_ptr<Connection> newConnection(new Connection(relay,
                                                                         socket,
                                                                         connectionDescription));

                newConnection->createStream(connectionDescription.applicationName,
                                            connectionDescription.streamName);

                newConnection->connect();

                connections.push_back(std::move(newConnection));
            }
        }
    }

    void Server::update(float delta)
    {
        for (const auto& connection : connections)
        {
            connection->update(delta);
        }
    }

    void Server::startStreaming(Connection& connection)
    {
        inputConnection = &connection;
        streaming = true;

        for (Connection* outputConnection : outputConnections)
        {
            outputConnection->createStream(inputConnection->getApplicationName(),
                                           inputConnection->getStreamName());
        }

        for (const Connection::Description& connectionDescription : connectionDescriptions)
        {
            if (connectionDescription.type == Connection::Type::CLIENT &&
                connectionDescription.streamType == Connection::StreamType::OUTPUT)
            {
                Socket socket(network);

                std::unique_ptr<Connection> newConnection(new Connection(relay,
                                                                         socket,
                                                                         connectionDescription));

                newConnection->createStream(inputConnection->getApplicationName(),
                                            inputConnection->getStreamName());

                newConnection->connect();

                connections.push_back(std::move(newConnection));
            }
        }
    }

    void Server::stopStreaming(Connection& connection)
    {
        streaming = false;

        if (&connection == inputConnection)
        {
            for (Connection* outputConnection : outputConnections)
            {
                outputConnection->deleteStream();
                outputConnection->unpublishStream();
            }

            connections.clear();

            inputConnection = nullptr;
        }
    }

    void Server::startReceiving(Connection& connection)
    {
        auto i = std::find(outputConnections.begin(), outputConnections.end(), &connection);

        if (i == outputConnections.end())
        {
            outputConnections.push_back(&connection);

            if (streaming)
            {
                connection.createStream(inputConnection->getApplicationName(),
                                        inputConnection->getStreamName());

                if (!videoHeader.empty()) connection.sendVideoData(0, videoHeader);
                if (!audioHeader.empty()) connection.sendAudioData(0, videoHeader);
                if (metaData.getMarker() != amf0::Marker::Unknown) connection.sendMetaData(metaData);
            }
        }
    }

    void Server::stopReceiving(Connection& connection)
    {
        auto outputIterator = std::find(outputConnections.begin(), outputConnections.end(), &connection);

        if (outputIterator != outputConnections.end())
        {
            outputConnections.erase(outputIterator);
        }

        removeConnection(connection);
        relay.removeConnection(connection);
    }

    void Server::removeConnection(Connection& connection)
    {
        auto connectionIterator = std::find_if(connections.begin(), connections.end(),
                                               [&connection](const std::unique_ptr<Connection>& currentConnection) {
                                                   return currentConnection.get() == &connection;
                                               });

        if (connectionIterator != connections.end())
        {
            connections.erase(connectionIterator);
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
