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
        id(Relay::nextId()),
        network(aNetwork)
    {
    }

    Stream* Server::findStream(Connection::StreamType type,
                               const std::string& aApplicationName,
                               const std::string& aStreamName) const
    {
        for (auto i = streams.begin(); i != streams.end(); ++i)
        {
            if ((*i)->getType() == type &&
                (*i)->getApplicationName() == aApplicationName &&
                (*i)->getStreamName() == aStreamName)
            {
                return i->get();
            }
        }

        return nullptr;
    }

    Stream* Server::createStream(Connection::StreamType type,
                                 const std::string& aApplicationName,
                                 const std::string& aStreamName)
    {
        std::unique_ptr<Stream> stream(new Stream(type, aApplicationName, aStreamName));
        Stream* streamPtr = stream.get();
        streams.push_back(std::move(stream));

        return streamPtr;
    }

    void Server::releaseStream(Stream* stream)
    {
        for (auto i = streams.begin(); i != streams.end();)
        {
            if (i->get() == stream)
            {
                i = streams.erase(i);
            }
            else
            {
                ++i;
            }
        }
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

    void Server::getStats(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                for (const auto& connection : connections)
                {
                    connection->getStats(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                for (const auto& connection : connections)
                {
                    connection->getStats(str, reportType);
                }
                break;
            }
            case ReportType::JSON:
            {
                bool first = true;

                for (const auto& connection : connections)
                {
                    if (!first) str += ",";
                    first = false;
                    connection->getStats(str, reportType);
                }
                break;
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

            outputConnections.clear();
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
                if (metaData.getType() != amf::Node::Type::Unknown) connection.sendMetaData(metaData);
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

    void Server::sendMetaData(const amf::Node& newMetaData)
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

    void Server::sendTextData(uint64_t timestamp, const amf::Node& textData)
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
