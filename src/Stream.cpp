//
//  rtmp_relay
//

#include <algorithm>
#include "Stream.hpp"
#include "Connection.hpp"
#include "Relay.hpp"
#include "Server.hpp"

using namespace cppsocket;

namespace relay
{
    Stream::Stream(Server& aServer,
                   Type aType,
                   const std::string& aApplicationName,
                   const std::string& aStreamName):
        id(Relay::nextId()),
        server(aServer),
        type(aType),
        applicationName(aApplicationName),
        streamName(aStreamName)
    {
    }

    Stream::~Stream()
    {
        if (inputConnection) inputConnection->removeStream();

        for (Connection* outputConnection : outputConnections)
        {
            outputConnection->removeStream();
            outputConnection->unpublishStream();
        }

        for (Connection* connection : connections)
        {
            server.deleteConnection(connection);
        }
    }

    void Stream::getStats(std::string& str, ReportType reportType) const
    {
        // TODO: implement
    }

    void Stream::startStreaming(Connection& connection)
    {
        if (!inputConnection)
        {
            inputConnection = &connection;
            streaming = true;

            for (Connection* outputConnection : outputConnections)
            {
                outputConnection->setStream(this);
            }

            for (const Endpoint& endpoint : server.getEndpoints())
            {
                if (endpoint.connectionType == Connection::Type::CLIENT &&
                    endpoint.streamType == Type::OUTPUT)
                {
                    Connection* newConnection = server.createConnection(*this, endpoint);
                    newConnection->connect();

                    connections.push_back(newConnection);
                }
            }
        }
    }

    void Stream::stopStreaming(Connection& connection)
    {
        if (&connection == inputConnection)
        {
            streaming = false;

            for (Connection* outputConnection : outputConnections)
            {
                outputConnection->removeStream();
                outputConnection->unpublishStream();
            }

            outputConnections.clear();
            inputConnection = nullptr;
        }
    }

    void Stream::startReceiving(Connection& connection)
    {
        if (!inputConnection)
        {
            for (const Endpoint& endpoint : server.getEndpoints())
            {
                if (endpoint.connectionType == Connection::Type::CLIENT &&
                    endpoint.streamType == Type::INPUT)
                {
                    auto ic = server.createConnection(*this, endpoint);
                    ic->connect();
                }
            }
        }

        auto i = std::find(outputConnections.begin(), outputConnections.end(), &connection);

        if (i == outputConnections.end())
        {
            outputConnections.push_back(&connection);

            if (streaming)
            {
                connection.setStream(this);

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
