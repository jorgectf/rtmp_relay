//
//  rtmp_relay
//

#include <ctime>
#include <memory>
#include <algorithm>
#include <functional>
#include <iostream>
#include <chrono>
#include <thread>
#include "yaml-cpp/yaml.h"
#include "Log.h"
#include "Relay.h"
#include "Status.h"
#include "Connection.h"

using namespace cppsocket;

namespace relay
{
    uint64_t Relay::currentId = 0;

    Relay::Relay(Network& aNetwork):
    network(aNetwork), mersanneTwister(static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()))
    {
        previousTime = std::chrono::steady_clock::now();
    }

    bool Relay::init(const std::string& config)
    {
        servers.clear();
        connections.clear();
        status.reset();

        YAML::Node document;

        try
        {
            document = YAML::LoadFile(config);
        }
        catch (YAML::BadFile)
        {
            Log(Log::Level::ERR) << "Failed to open " << config;
            return false;
        }
        catch (YAML::ParserException e)
        {
            Log(Log::Level::ERR) << "Failed to parse " << config << ", " << e.msg << " on line " << e.mark.line << " column " << e.mark.column;
            return false;
        }

        if (document["log"])
        {
            const YAML::Node& logObject = document["log"];

            if (logObject["level"])
            {
                Log::threshold = static_cast<Log::Level>(logObject["level"].as<uint32_t>());
            }

#ifndef _WIN32
            if (logObject["syslogEnabled"])
            {
                Log::syslogEnabled = logObject["syslogEnabled"].as<bool>();
            }

            if (logObject["syslogIdent"])
            {
                syslogIdent = logObject["syslogIdent"].as<std::string>();
            }

            if (logObject["syslogFacility"])
            {
                std::string facility = logObject["syslogFacility"].as<std::string>();

                if (facility == "LOG_USER") syslogFacility = LOG_USER;
                else if (facility == "LOG_LOCAL0") syslogFacility = LOG_LOCAL0;
                else if (facility == "LOG_LOCAL1") syslogFacility = LOG_LOCAL1;
                else if (facility == "LOG_LOCAL2") syslogFacility = LOG_LOCAL2;
                else if (facility == "LOG_LOCAL3") syslogFacility = LOG_LOCAL3;
                else if (facility == "LOG_LOCAL4") syslogFacility = LOG_LOCAL4;
                else if (facility == "LOG_LOCAL5") syslogFacility = LOG_LOCAL5;
                else if (facility == "LOG_LOCAL6") syslogFacility = LOG_LOCAL6;
                else if (facility == "LOG_LOCAL7") syslogFacility = LOG_LOCAL7;
            }
#endif
        }

        openLog();

        if (document["statusPage"])
        {
            const YAML::Node& statusPageObject = document["statusPage"];

            if (statusPageObject["address"])
            {
                status.reset(new Status(network, *this, statusPageObject["address"].as<std::string>()));
            }
        }

        std::set<std::string> listenAddresses;

        const YAML::Node& serversArray = document["servers"];

        for (size_t serverIndex = 0; serverIndex < serversArray.size(); ++serverIndex)
        {
            std::vector<Endpoint> endpoints;
            
            const YAML::Node& serverObject = serversArray[serverIndex];

            if (serverObject["endpoints"])
            {
                const YAML::Node& endpointsArray = serverObject["endpoints"];

                for (size_t endpointIndex = 0; endpointIndex < endpointsArray.size(); ++endpointIndex)
                {
                    const YAML::Node& endpointObject = endpointsArray[endpointIndex];

                    Endpoint endpoint;

                    if (endpointObject["type"].as<std::string>() == "host") endpoint.connectionType = Connection::Type::HOST;
                    else if (endpointObject["type"].as<std::string>() == "client") endpoint.connectionType = Connection::Type::CLIENT;

                    if (endpointObject["stream"].as<std::string>() == "input") endpoint.streamType = Stream::Type::INPUT;
                    else if (endpointObject["stream"].as<std::string>() == "output") endpoint.streamType = Stream::Type::OUTPUT;

                    if (endpointObject["address"].IsSequence())
                    {
                        const YAML::Node& addressArray = endpointObject["address"];

                        for (size_t addressIndex = 0; addressIndex < addressArray.size(); ++addressIndex)
                        {
                            std::string address = addressArray[addressIndex].as<std::string>();
                            std::pair<uint32_t, uint16_t> addr;
                            if (!Socket::getAddress(address, addr))
                            {
                                return false;
                            }

                            endpoint.ipAddresses.push_back(std::make_pair(addr.first, addr.second));
                            endpoint.addresses.push_back(address);

                            if (endpoint.connectionType == Connection::Type::HOST)
                            {
                                listenAddresses.insert(address);
                            }
                        }
                    }
                    else
                    {
                        std::string address = endpointObject["address"].as<std::string>();
                        std::pair<uint32_t, uint16_t> addr;
                        if (!Socket::getAddress(address, addr))
                        {
                            return false;
                        }

                        endpoint.ipAddresses.push_back(std::make_pair(addr.first, addr.second));
                        endpoint.addresses.push_back(address);
                    }

                    if (endpointObject["connectionTimeout"]) endpoint.connectionTimeout = endpointObject["connectionTimeout"].as<float>();
                    if (endpointObject["reconnectInterval"]) endpoint.reconnectInterval = endpointObject["reconnectInterval"].as<float>();
                    if (endpointObject["reconnectCount"]) endpoint.reconnectCount = endpointObject["reconnectCount"].as<uint32_t>();
                    if (endpointObject["pingInterval"]) endpoint.pingInterval = endpointObject["pingInterval"].as<float>();
                    if (endpointObject["bufferSize"]) endpoint.bufferSize = endpointObject["bufferSize"].as<uint32_t>();

                    if (endpointObject["applicationName"]) endpoint.applicationName = endpointObject["applicationName"].as<std::string>();
                    if (endpointObject["streamName"]) endpoint.streamName = endpointObject["streamName"].as<std::string>();
                    if (endpointObject["overrideApplicationName"]) endpoint.overrideApplicationName = endpointObject["overrideApplicationName"].as<std::string>();
                    if (endpointObject["overrideStreamName"]) endpoint.overrideStreamName = endpointObject["overrideStreamName"].as<std::string>();

                    if (endpointObject["metaDataBlacklist"])
                    {
                        const YAML::Node& metaDataBlacklistArray = endpointObject["metaDataBlacklist"];

                        for (size_t metaDataBlacklistIndex = 0; metaDataBlacklistIndex < metaDataBlacklistArray.size(); ++metaDataBlacklistIndex)
                        {
                            endpoint.metaDataBlacklist.insert(metaDataBlacklistArray[metaDataBlacklistIndex].as<std::string>());
                        }
                    }

                    if (endpointObject["video"]) endpoint.videoStream = endpointObject["video"].as<bool>();
                    if (endpointObject["audio"]) endpoint.audioStream = endpointObject["audio"].as<bool>();
                    if (endpointObject["data"]) endpoint.dataStream = endpointObject["data"].as<bool>();
                    if (endpointObject["amfVersion"])
                    {
                        switch (endpointObject["amfVersion"].as<uint32_t>())
                        {
                            case 0: endpoint.amfVersion = amf::Version::AMF0; break;
                            case 3: endpoint.amfVersion = amf::Version::AMF3; break;
                            default:
                                Log(Log::Level::ERR) << "Invalid AMF version";
                                break;
                        }

                    }

                    if (endpoint.connectionType == Connection::Type::CLIENT &&
                        endpoint.streamType == Stream::Type::INPUT)
                    {
                        if (endpoint.applicationName.empty() ||
                            endpoint.streamName.empty())
                        {
                            Log(Log::Level::ERR) << "Client input streams can not have mepty application name or stream name";
                            return false;
                        }
                    }

                    endpoints.push_back(endpoint);
                }
            }

            // start the server
            std::unique_ptr<Server> server(new Server(*this, network));
            server->start(endpoints);
            servers.push_back(std::move(server));
        }

        for (const std::string& address : listenAddresses)
        {
            cppsocket::Socket acceptor(network);
            acceptor.setBlocking(false);
            acceptor.setAcceptCallback(std::bind(&Relay::handleAccept, this, std::placeholders::_1, std::placeholders::_2));
            acceptor.startAccept(address);
            acceptors.push_back(std::move(acceptor));
        }

        return true;
    }

    std::vector<std::pair<Server*, const Endpoint*>> Relay::getEndpoints(const std::pair<uint32_t, uint16_t>& address,
                                                                         Stream::Type type,
                                                                         const std::string& applicationName,
                                                                         const std::string& streamName) const
    {
        std::vector<std::pair<Server*, const Endpoint*>> result;

        for (const std::unique_ptr<Server>& server : servers)
        {
            const std::vector<Endpoint>& endpoints = server->getEndpoints();

            for (const Endpoint& endpoint : endpoints)
            {
                if ((endpoint.applicationName.empty() || endpoint.applicationName == applicationName) &&
                    (endpoint.streamName.empty() || endpoint.streamName == streamName))
                {
                    if (endpoint.streamType == type)
                    {
                        if (std::find(endpoint.ipAddresses.begin(),
                                      endpoint.ipAddresses.end(),
                                      address) != endpoint.ipAddresses.end())
                        {
                            result.push_back(std::make_pair(server.get(), &endpoint));
                        }
                    }
                }
            }
        }

        return result;
    }

    void Relay::close()
    {
        connections.clear();
        status.reset();
        active = false;
    }

    void Relay::run()
    {
        const std::chrono::microseconds sleepTime(10000);

        while (active)
        {
            auto currentTime = std::chrono::steady_clock::now();
            float delta = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - previousTime).count() / 1000.0f;
            previousTime = currentTime;

            network.update();

            if (status) status->update(delta);

            for (const auto& server : servers)
            {
                server->update(delta);
            }

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

            std::this_thread::sleep_for(sleepTime);
        }
    }

    void Relay::getStats(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str = "Connections:\n";
                for (const auto& connection : connections)
                {
                    connection->getStats(str, reportType);
                }

                for (const auto& server : servers)
                {
                    server->getStats(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str = "<html><title>Status</title><body>";
                str += "<table border=\"1\"><tr><th>ID</th><th>Name</th><th>Application</th><th>Status</th><th>Address</th><th>Connection</th><th>State</th><th>Stream</th><th>Server ID</th><th>Meta data</th></tr>";

                for (const auto& connection : connections)
                {
                    connection->getStats(str, reportType);
                }

                for (const auto& server : servers)
                {
                    server->getStats(str, reportType);
                }

                str += "</table>";
                str += "</body></html>";

                break;
            }
            case ReportType::JSON:
            {
                str = "{\"connections\":[";

                bool first = true;

                for (const auto& connection : connections)
                {
                    if (!first) str += ",";
                    first = false;
                    connection->getStats(str, reportType);
                }

                for (const auto& server : servers)
                {
                    if (!first) str += ",";
                    first = false;
                    server->getStats(str, reportType);
                }

                str += "]}";
                
                break;
            }
        }
    }

    void Relay::openLog()
    {
#ifndef _WIN32
        openlog(syslogIdent.empty() ? nullptr : syslogIdent.c_str(), 0, syslogFacility);
#endif
    }

    void Relay::closeLog()
    {
#ifndef _WIN32
        closelog();
#endif
    }

    void Relay::handleAccept(cppsocket::Socket&, cppsocket::Socket& clientSocket)
    {
        std::unique_ptr<Connection> connection(new Connection(*this, clientSocket));

        connections.push_back(std::move(connection));
    }
}
