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
        for (auto i = connections.begin(); i != connections.end();)
        {
            const std::unique_ptr<Connection>& connection = *i;

            connection->update(delta);

            if (connection->isClosed())
            {
                i = connections.erase(i);
            }
            else
            {
                ++i;
            }
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

                if (!videoHeader.empty()) connection.sendVideoHeader(videoHeader);
                if (!audioHeader.empty()) connection.sendAudioHeader(audioHeader);
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
    }

    void Server::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendAudioHeader(headerData);
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
                outputConnection->sendVideoHeader(headerData);
            }
        }
    }

    void Server::sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendAudioFrame(timestamp, audioData);
            }
        }
    }

    void Server::sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& videoData, VideoFrameType frameType)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Connection::StreamType::OUTPUT)
            {
                outputConnection->sendVideoFrame(timestamp, videoData, frameType);
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
