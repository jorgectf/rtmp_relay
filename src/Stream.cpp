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
                   const std::string& aApplicationName,
                   const std::string& aStreamName):
        id(Relay::nextId()),
        server(aServer),
        applicationName(aApplicationName),
        streamName(aStreamName)
    {
        Log(Log::Level::INFO) << "[" << id << ", Stream] " << " Create " << applicationName << "/" << streamName;
    }

    Stream::~Stream()
    {
        Log(Log::Level::INFO) << "[" << id << ", Stream] " << " Delete " << applicationName << "/" << streamName;
    }

    void Stream::getStats(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "    Stream[" + std::to_string(id) + "]: " + applicationName + " / " + streamName + "\n";

//                if (inputConnection) inputConnection->getStats(str, reportType);
//                for (const auto& c : outputConnections)
//                {
//                    c->getStats(str, reportType);
//                }
                break;
            }
            case ReportType::HTML:
            {
                if (inputConnection) inputConnection->getStats(str, reportType);
                for (const auto& c : outputConnections)
                {
                    c->getStats(str, reportType);
                }

                break;
            }
            case ReportType::JSON:
            {
                str += "{\"id\": " + std::to_string(id) + ", \"applicationName\":\"" + applicationName + "\", \"streamName\":\"" + streamName + "\", \"connections\": [";

                bool first = true;

                if (inputConnection)
                {
                    inputConnection->getStats(str, reportType);
                    first = false;
                }
                for (const auto& c : outputConnections)
                {
                    if (!first) str += ",";
                    first = false;
                    c->getStats(str, reportType);
                }

                str += "]}";
            }
        }
    }

    bool Stream::hasDependableConnections()
    {
        bool hasDependables = (inputConnection ? inputConnection->isDependable() : false);
        for (const auto& c : outputConnections)
        {
            hasDependables |= c->isDependable();
        }

        return hasDependables;
    }

    void Stream::close()
    {
        closed = true;
        if (inputConnection) inputConnection->close(true);
        for (auto o : outputConnections)
        {
            o->close(true);
        }
        server.cleanup();
    }

    void Stream::start(relay::Connection &connection)
    {
        Log() << "Stream start";
        if (connection.getDirection() == Connection::Direction::INPUT)
        {
            if (!inputConnection)
            {
                inputConnection = &connection;
                streaming = true;

                for (const Endpoint& endpoint : server.getEndpoints())
                {
                    if (endpoint.connectionType == Connection::Type::CLIENT &&
                        endpoint.direction == Connection::Direction::OUTPUT)
                    {
                        Connection* newConnection = server.createConnection(*this, endpoint);
                        newConnection->connect();

                        connections.push_back(newConnection);
                    }
                }
            }
        }
        else if (connection.getDirection() == Connection::Direction::OUTPUT)
        {
            if (!inputConnection)
            {
                for (const Endpoint& endpoint : server.getEndpoints())
                {
                    if (endpoint.connectionType == Connection::Type::CLIENT &&
                        endpoint.direction == Connection::Direction::INPUT &&
                        !endpoint.isNameKnown())
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
        else
        {
            Log(Log::Level::ERR) << "Stream start direction not set";
        }
    }

    void Stream::stop(relay::Connection &connection)
    {
        if (&connection == inputConnection)
        {
            streaming = false;
            if (inputConnection->getType() == Connection::Type::HOST)
            {
                inputConnection = nullptr;
            }
        }
        else
        {
            if (connection.getType() == Connection::Type::HOST)
            {
                auto outputIterator = std::find(outputConnections.begin(), outputConnections.end(), &connection);

                if (outputIterator != outputConnections.end())
                {
                    outputConnections.erase(outputIterator);
                }
            }
        }

        if (!hasDependableConnections())
        {
            close();
        }
    }

    void Stream::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        audioHeader = headerData;

        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getDirection() == Connection::Direction::OUTPUT)
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
            if (outputConnection->getDirection() == Connection::Direction::OUTPUT)
            {
                outputConnection->sendVideoHeader(headerData);
            }
        }
    }

    void Stream::sendAudioFrame(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getDirection() == Connection::Direction::OUTPUT)
            {
                outputConnection->sendAudioFrame(timestamp, audioData);
            }
        }
    }

    void Stream::sendVideoFrame(uint64_t timestamp, const std::vector<uint8_t>& videoData, VideoFrameType frameType)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getDirection() == Connection::Direction::OUTPUT)
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
            if (outputConnection->getDirection() == Connection::Direction::OUTPUT)
            {
                outputConnection->sendMetaData(metaData);
            }
        }
    }

    void Stream::sendTextData(uint64_t timestamp, const amf::Node& textData)
    {
        for (Connection* outputConnection : outputConnections)
        {
            if (outputConnection->getDirection() == Connection::Direction::OUTPUT)
            {
                outputConnection->sendTextData(timestamp, textData);
            }
        }
    }
}
