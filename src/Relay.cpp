//
//  rtmp_relay
//

#include <ctime>
#include <memory>
#include <algorithm>
#include <functional>
#include <iostream>
#include <chrono>
#include <regex>
#include <thread>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "yaml-cpp/yaml.h"
#include "Log.hpp"
#include "Relay.hpp"
#include "Status.hpp"
#include "Connection.hpp"

namespace relay
{
    uint64_t Relay::currentId = 0;

    Relay::Relay(Network& aNetwork):
        generator(static_cast<unsigned int>(std::chrono::high_resolution_clock::now().time_since_epoch().count())),
        network(aNetwork)
    {
        previousTime = std::chrono::steady_clock::now();
    }

    Relay::~Relay()
    {
        for (auto& a : servers)
        {
            a->stop();
        }

        for (auto& c : connections)
        {
            c->close(true);
        }
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
        catch (YAML::ParserException& e)
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

        if (document["timeout"])
        {
            float ts = document["timeout"].as<float>();
            timeout = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(ts * 1000));
            hasTimeout = true;
        }

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

                    if (!endpointObject["type"] || !endpointObject["direction"] || !endpointObject["address"])
                    {
                        Log(Log::Level::ERR) << "Endpoint configuration is missing field";
                        return false;
                    }

                    if (endpointObject["type"].as<std::string>() == "host") endpoint.connectionType = Connection::Type::HOST;
                    else if (endpointObject["type"].as<std::string>() == "client") endpoint.connectionType = Connection::Type::CLIENT;

                    if (endpointObject["direction"].as<std::string>() == "input") endpoint.direction = Connection::Direction::INPUT;
                    else if (endpointObject["direction"].as<std::string>() == "output") endpoint.direction = Connection::Direction::OUTPUT;

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

                            Endpoint::Address endpointAddress;
                            endpointAddress.url = address;
                            endpointAddress.ipAddresses = std::make_pair(addr.first, addr.second);
                            endpoint.addresses.push_back(endpointAddress);

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

                        Endpoint::Address endpointAddress;
                        endpointAddress.url = address;
                        endpointAddress.ipAddresses = std::make_pair(addr.first, addr.second);
                        endpoint.addresses.push_back(endpointAddress);
                    }

                    if (endpointObject["connectionTimeout"]) endpoint.connectionTimeout = endpointObject["connectionTimeout"].as<float>();
                    if (endpointObject["reconnectInterval"]) endpoint.reconnectInterval = endpointObject["reconnectInterval"].as<float>();
                    if (endpointObject["reconnectCount"]) endpoint.reconnectCount = endpointObject["reconnectCount"].as<uint32_t>();
                    if (endpointObject["pingInterval"]) endpoint.pingInterval = endpointObject["pingInterval"].as<float>();
                    if (endpointObject["bufferSize"]) endpoint.bufferSize = endpointObject["bufferSize"].as<uint32_t>();

                    if (endpointObject["applicationName"]) endpoint.applicationName = endpointObject["applicationName"].as<std::string>();
                    if (endpointObject["streamName"]) endpoint.streamName = endpointObject["streamName"].as<std::string>();

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

                    endpoints.push_back(endpoint);
                }
            }

            // check if configuration is valid
            {
                bool hasName = false;
                for (const auto& e : endpoints)
                {
                    hasName |= (e.direction == Connection::Direction::INPUT &&
                                ((e.connectionType == Connection::Type::CLIENT && e.isNameKnown()) || e.connectionType == Connection::Type::HOST))
                                || (e.direction == Connection::Direction::OUTPUT && e.connectionType == Connection::Type::HOST);
                }

                if (!hasName)
                {
                    Log(Log::Level::ERR) << "Server configuration is invalid";
                    return false;
                }
            }

            // start the server
            std::unique_ptr<Server> server(new Server(*this, network));
            server->start(endpoints);
            servers.push_back(std::move(server));
        }

        for (const std::string& address : listenAddresses)
        {
            Socket acceptor(network);
            acceptor.setBlocking(false);
            acceptor.setAcceptCallback(std::bind(&Relay::handleAccept, this, std::placeholders::_1, std::placeholders::_2));
            acceptor.startAccept(address);
            acceptors.push_back(std::move(acceptor));
        }

        return true;
    }

    std::vector<std::pair<Server*, const Endpoint*>> Relay::getEndpoints(const std::pair<uint32_t, uint16_t>& address,
                                                                         Connection::Direction direction,
                                                                         const std::string& applicationName,
                                                                         const std::string& streamName) const
    {
        std::vector<std::pair<Server*, const Endpoint*>> result;

        for (const std::unique_ptr<Server>& server : servers)
        {
            for (const Endpoint& endpoint : server->getEndpoints())
            {
                try
                {
                    if (endpoint.connectionType == Connection::Type::HOST &&
                        (endpoint.applicationName.empty() || std::regex_match(applicationName, std::regex(endpoint.applicationName))) &&
                        (endpoint.streamName.empty() || std::regex_match(streamName, std::regex(endpoint.streamName))))
                    {
                        Log(Log::Level::ALL) << "Application \"" << applicationName << "\", stream \"" << streamName << "\" matched endpoint application \"" << endpoint.applicationName << "\", stream \"" << endpoint.streamName << "\"";

                        if (endpoint.direction == direction)
                        {
                            bool found = false;

                            for (auto endpointAddress : endpoint.addresses)
                            {
                                if ((endpointAddress.ipAddresses.first == ANY_ADDRESS ||
                                     address.first == ANY_ADDRESS ||
                                     endpointAddress.ipAddresses.first == address.first) &&
                                    endpointAddress.ipAddresses.second == address.second)
                                {
                                    Log(Log::Level::ALL) << "Address " << ipToString(address.first) << ":" << address.second << " matched address " << ipToString(endpointAddress.ipAddresses.first) << ":" << endpointAddress.ipAddresses.second;

                                    found = true;
                                    break;
                                }
                                else
                                {
                                    Log(Log::Level::ALL) << "Address " << ipToString(address.first) << ":" << address.second << " did not match address " << ipToString(endpointAddress.ipAddresses.first) << ":" << endpointAddress.ipAddresses.second;
                                }
                            }

                            if (found)
                            {
                                result.push_back(std::make_pair(server.get(), &endpoint));
                            }
                        }
                    }
                    else
                    {
                        Log(Log::Level::ALL) << "Application: \"" << applicationName << "\", stream: \"" << streamName << "\" did not match endpoint application: \"" << endpoint.applicationName << "\", stream: \"" << endpoint.streamName << "\"";
                    }
                }
                catch (std::regex_error e)
                {
                    Log(Log::Level::ERR) << "Configuration error: Invalid regex for output connection";
                    exit(1);
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
        const std::chrono::microseconds sleepTime(5000);

        while (active)
        {
            auto currentTime = std::chrono::steady_clock::now();
            if (hasTimeout && currentTime > timeout)
            {
                break;
            }

            float delta = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - previousTime).count() / 1000.0f;
            previousTime = currentTime;

            network.update();

            if (status) status->update(delta);

            for (auto i = connections.begin(); i != connections.end();)
            {
                const std::unique_ptr<Connection>& connection = *i;

                if (connection->isClosed())
                {
                    i = connections.erase(i);
                    continue;
                }
                else
                {
                    ++i;
                }

                connection->update(delta);
            }

            for (const auto& server : servers)
            {
                server->update(delta);
            }

            std::this_thread::sleep_for(sleepTime);
        }
    }

    void Relay::getStats(std::string& str, ReportType reportType) const
    {
        std::map<Connection*, Stream*> cons;
        std::map<Stream*, Connection*> streams;

        for (auto& c : connections)
        {
            cons[c.get()] = c->getStream();
        }

        for (auto& s : servers)
        {
            s->getConnections(cons);
        }

        switch (reportType)
        {
            case ReportType::TEXT:
            {
                std::stringstream ss;

                ss
                << std::setw(8) << " "
                << std::setw(5) << "ID" << " "
                << std::setw(20) << "Application" << " "
                << std::setw(20) << "Stream name" << " "

                << std::setw(15) << "Status" << " "
                << std::setw(22) << "Address" << " "
                << std::setw(7) << "Type" << " "
                << std::setw(20) << "State" << " "
                << std::setw(10) << "Direction" << " "

                << std::setw(6) << "Server" << " " << " Metadata\n";

                auto header = ss.str();


                str = "Pending connections:\n";
                for (const auto& c : cons)
                {
                    if (c.second == nullptr)
                    {
                        c.first->getStats(str, reportType);
                    }
                }

                str += "\nStreams:\n";
                for (auto it = cons.begin(); it != cons.end(); ++it)
                {
                    if (it->second != nullptr)
                    {
                        Stream* stream = it->second;
                        stream->getStats(str, reportType);
                        str += header;

                        if (stream->getInputConnection())
                        {
                            stream->getInputConnection()->getStats(str, reportType);
                            cons[stream->getInputConnection()] = nullptr;
                        }
                        for (auto cit = it; cit != cons.end(); ++cit)
                        {
                            if (cons[cit->first] == stream && cit->first != stream->getInputConnection())
                            {
                                cons[cit->first] = nullptr;
                                cit->first->getStats(str, reportType);
                            }
                        }
                    }
                }

                break;
            }
            case ReportType::HTML:
            {
                auto header = "<table border=\"1\" cellspacing=\"0\" cellpadding=\"5\"><tr><th>ID</th><th>Name</th><th>Application</th><th>Status</th><th>Address</th><th>Connection</th><th>State</th><th>Direction</th><th>Server ID</th><th>Meta data</th></tr>";

                str = "<html><title>Status</title><body>";

                str = "<b>Pending connections</b>";
                str += header;
                for (const auto& c : cons)
                {
                    if (c.second == nullptr)
                    {
                        c.first->getStats(str, reportType);
                    }
                }
                str += "</table>";

                str = "<b>Streams</b><br>";
                for (auto it = cons.begin(); it != cons.end(); ++it)
                {
                    if (it->second)
                    {
                        Stream* stream = it->second;
                        stream->getStats(str, reportType);

                        str += header;
                        if (stream->getInputConnection())
                        {
                            stream->getInputConnection()->getStats(str, reportType);
                            cons[stream->getInputConnection()] = nullptr;
                        }
                        for (auto cit = it; cit != cons.end(); ++cit)
                        {
                            if (cons[cit->first] == stream && cit->first != stream->getInputConnection())
                            {
                                cons[cit->first] = nullptr;
                                cit->first->getStats(str, reportType);
                            }
                        }
                        str += "</table>";
                    }
                }

                str += "</body></html>";

                break;
            }
            case ReportType::JSON:
            {
                bool first = true;
                str = "{\"pending_connections\":[";
                for (const auto& c : cons)
                {
                    if (c.second == nullptr)
                    {
                        if (!first) str += ",";
                        first = false;
                        c.first->getStats(str, reportType);
                    }
                }
                str += "], \"streams\":[";
                bool firstStream = true;
                for (auto it = cons.begin(); it != cons.end(); ++it)
                {
                    if (it->second != nullptr)
                    {
                        Stream* stream = it->second;
                        if (!firstStream) str += ",";
                        firstStream = false;
                        stream->getStats(str, reportType);

                        first = true;
                        if (stream->getInputConnection())
                        {
                            first = false;
                            stream->getInputConnection()->getStats(str, reportType);
                            cons[stream->getInputConnection()] = nullptr;
                        }
                        for (auto cit = it; cit != cons.end(); ++cit)
                        {
                            if (cons[cit->first] == stream && cit->first != stream->getInputConnection())
                            {
                                if (!first) str += ",";
                                first = false;
                                
                                cons[cit->first] = nullptr;
                                cit->first->getStats(str, reportType);
                            }
                        }

                        str += "]}";
                    }
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

    void Relay::handleAccept(Socket&, Socket& clientSocket)
    {
        std::unique_ptr<Connection> connection(new Connection(*this, clientSocket));

        connections.push_back(std::move(connection));
    }
}
