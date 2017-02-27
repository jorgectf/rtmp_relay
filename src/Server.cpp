//
//  rtmp_relay
//

#include <algorithm>
#include "Server.h"

using namespace cppsocket;

namespace relay
{
    Server::Server(Relay& aRelay,
                   cppsocket::Network& aNetwork,
                   const Server::Description& aDescription):
        relay(aRelay),
        network(aNetwork),
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

        for (const Connection::Description& outputDescription : description.outputDescriptions)
        {
            Socket socket(network);

            std::unique_ptr<Connection> newConnection(new Connection(relay,
                                                                     socket,
                                                                     outputDescription.addresses,
                                                                     outputDescription.connectionTimeout,
                                                                     outputDescription.reconnectInterval,
                                                                     outputDescription.reconnectCount,
                                                                     Connection::StreamType::OUTPUT,
                                                                     *this,
                                                                     outputDescription.applicationName,
                                                                     outputDescription.streamName,
                                                                     outputDescription.overrideApplicationName,
                                                                     outputDescription.overrideStreamName));

            connections.push_back(std::move(newConnection));
        }
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

            connections.clear();
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
