//
//  rtmp_relay
//

#include <algorithm>
#include "Stream.h"
#include "Connection.h"
#include "Relay.h"
#include "Server.h"

using namespace cppsocket;

namespace relay
{
    Stream::Stream(Relay& aRelay,
                   cppsocket::Network& aNetwork,
                   Server& aServer,
                   Type aType,
                   const std::string& aApplicationName,
                   const std::string& aStreamName):
        id(Relay::nextId()),
        relay(aRelay),
        network(aNetwork),
        server(aServer),
        type(aType),
        applicationName(aApplicationName),
        streamName(aStreamName)
    {
    }

    void Stream::getStats(std::string& str, ReportType reportType) const
    {
        // TODO: implement
    }

    void Stream::startStreaming(Connection& connection)
    {
        inputConnection = &connection;
        streaming = true;

        for (Connection* outputConnection : outputConnections)
        {
            outputConnection->createStream(inputConnection->getApplicationName(),
                                           inputConnection->getStreamName());
        }

        for (const Connection::Description& connectionDescription : server.getConnectionDescriptions())
        {
            if (connectionDescription.connectionType == Connection::Type::CLIENT &&
                connectionDescription.streamType == Type::OUTPUT)
            {
                Socket socket(network);

                std::unique_ptr<Connection> newConnection(new Connection(relay,
                                                                         socket,
                                                                         connectionDescription));

                newConnection->createStream(inputConnection->getApplicationName(),
                                            inputConnection->getStreamName());

                newConnection->connect();

                //connections.push_back(std::move(newConnection));
            }
        }
    }

    void Stream::stopStreaming(Connection& connection)
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

    void Stream::startReceiving(Connection& connection)
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

    void Stream::stopReceiving(Connection& connection)
    {
        auto outputIterator = std::find(outputConnections.begin(), outputConnections.end(), &connection);

        if (outputIterator != outputConnections.end())
        {
            outputConnections.erase(outputIterator);
        }
    }

    void Stream::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Type::OUTPUT)
            {
                outputConnection->sendAudioHeader(headerData);
            }
        }
    }

    void Stream::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        videoHeader = headerData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Type::OUTPUT)
            {
                outputConnection->sendVideoHeader(headerData);
            }
        }
    }

    void Stream::sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Type::OUTPUT)
            {
                outputConnection->sendAudioFrame(timestamp, audioData);
            }
        }
    }

    void Stream::sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& videoData, VideoFrameType frameType)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Type::OUTPUT)
            {
                outputConnection->sendVideoFrame(timestamp, videoData, frameType);
            }
        }
    }

    void Stream::sendMetaData(const amf::Node& newMetaData)
    {
        metaData = newMetaData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Type::OUTPUT)
            {
                outputConnection->sendMetaData(metaData);
            }
        }
    }

    void Stream::sendTextData(uint64_t timestamp, const amf::Node& textData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getStreamType() == Type::OUTPUT)
            {
                outputConnection->sendTextData(timestamp, textData);
            }
        }
    }
}
