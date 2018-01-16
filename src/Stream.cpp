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
        idString = "[ST:" + std::to_string(id) + " " + applicationName + "/" + streamName + "] ";

        Log(Log::Level::INFO) << idString << "Create";
    }

    Stream::~Stream()
    {
        Log(Log::Level::INFO) << idString << "Delete";
    }

    void Stream::getStats(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str += "    Stream[" + std::to_string(id) + "]: " + applicationName + "/" + streamName + "\n";
                break;
            }
            case ReportType::HTML:
            {
                str += "<b>Stream[" + std::to_string(id) + "]: " + applicationName + "/" + streamName + "</b>";
                break;
            }
            case ReportType::JSON:
            {
                str += "{\"id\": " + std::to_string(id) + ", \"applicationName\":\"" + applicationName + "\", \"streamName\":\"" + streamName + "\", \"connections\": [";
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
        for (auto o : connections)
        {
            o->close(true);
        }
        server.cleanup();
    }

    void Stream::start(relay::Connection &connection)
    {
        if (closed) return;

        Log() << idString << "Stream start " << connection.getIdString();
        if (connection.getDirection() == Connection::Direction::INPUT)
        {
            if (!inputConnection)
            {
                inputConnection = &connection;
                streaming = true;
            }

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
        else if (connection.getDirection() == Connection::Direction::OUTPUT)
        {
            if (!inputConnection && !inputConnectionCreated)
            {
                for (const Endpoint& endpoint : server.getEndpoints())
                {
                    if (endpoint.connectionType == Connection::Type::CLIENT &&
                        endpoint.direction == Connection::Direction::INPUT &&
                        !endpoint.isNameKnown())
                    {
                        auto ic = server.createConnection(*this, endpoint);
                        ic->connect();
                        inputConnectionCreated = true;

                        connections.push_back(ic);
                    }
                }
            }
    
            auto i = std::find(outputConnections.begin(), outputConnections.end(), &connection);
    
            if (i == outputConnections.end())
            {
                outputConnections.push_back(&connection);
            }

            if (streaming)
            {
                connection.setStream(this);

                if (!videoHeader.empty()) connection.sendVideoHeader(videoHeader);
                if (!audioHeader.empty()) connection.sendAudioHeader(audioHeader);
                if (metaData.getType() != amf::Node::Type::Unknown) connection.sendMetaData(metaData);
            }
        }
        else
        {
            Log(Log::Level::ERR) << "Stream start direction not set";
        }
    }

    void Stream::stop(relay::Connection &connection)
    {
        if (closed) return;

        Log() << idString << "Stream stop " << connection.getIdString();
        if (&connection == inputConnection)
        {
            streaming = false;
            if (inputConnection->getType() == Connection::Type::HOST)
            {
                inputConnection = nullptr;
            }

            // close all output client connections
            for (auto it = connections.begin(); it != connections.end();)
            {
                auto con = *it;
                if (con->getType() == Connection::Type::CLIENT && con->getDirection() == Connection::Direction::OUTPUT)
                {
                    it = connections.erase(it);

                    auto ci = std::find(outputConnections.begin(), outputConnections.end(), con);
                    if (ci != outputConnections.end()) outputConnections.erase(ci);

                    con->close(true);
                }
                else
                {
                    it++;
                }
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

    void Stream::getConnections(std::map<Connection*, Stream*>& cons)
    {
        if (inputConnection) cons[inputConnection] = this;

        for (const auto& c : outputConnections)
        {
            cons[c] = this;
        }
        for (const auto& c : connections)
        {
            cons[c] = this;
        }
    }
}
