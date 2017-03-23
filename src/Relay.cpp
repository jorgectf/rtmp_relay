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
        network(aNetwork)
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

#if !defined(_MSC_VER)
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
            std::vector<Connection::Description> connectionDescriptions;
            
            const YAML::Node& serverObject = serversArray[serverIndex];

            if (serverObject["connections"])
            {
                const YAML::Node& connectionsArray = serverObject["connections"];

                for (size_t connectionsIndex = 0; connectionsIndex < connectionsArray.size(); ++connectionsIndex)
                {
                    const YAML::Node& connectionObject = connectionsArray[connectionsIndex];

                    Connection::Description connectionDescription;

                    if (connectionObject["type"].as<std::string>() == "host") connectionDescription.type = Connection::Type::HOST;
                    else if (connectionObject["type"].as<std::string>() == "client") connectionDescription.type = Connection::Type::CLIENT;

                    if (connectionObject["stream"].as<std::string>() == "input") connectionDescription.streamType = Connection::StreamType::INPUT;
                    else if (connectionObject["stream"].as<std::string>() == "output") connectionDescription.streamType = Connection::StreamType::OUTPUT;

                    if (connectionObject["address"].IsSequence())
                    {
                        const YAML::Node& addressArray = connectionObject["address"];

                        for (size_t addressIndex = 0; addressIndex < addressArray.size(); ++addressIndex)
                        {
                            std::string address = addressArray[addressIndex].as<std::string>();
                            std::pair<uint32_t, uint16_t> addr = Socket::getAddress(address);

                            connectionDescription.ipAddresses.push_back(std::make_pair(addr.first, addr.second));
                            connectionDescription.addresses.push_back(address);

                            if (connectionDescription.type == Connection::Type::HOST)
                            {
                                listenAddresses.insert(address);
                            }
                        }
                    }
                    else
                    {
                        std::string address = connectionObject["address"].as<std::string>();
                        std::pair<uint32_t, uint16_t> addr = Socket::getAddress(address);

                        connectionDescription.ipAddresses.push_back(std::make_pair(addr.first, addr.second));
                        connectionDescription.addresses.push_back(address);
                    }

                    if (connectionObject["connectionTimeout"]) connectionDescription.connectionTimeout = connectionObject["connectionTimeout"].as<float>();
                    if (connectionObject["reconnectInterval"]) connectionDescription.reconnectInterval = connectionObject["reconnectInterval"].as<float>();
                    if (connectionObject["reconnectCount"]) connectionDescription.reconnectCount = connectionObject["reconnectCount"].as<uint32_t>();
                    if (connectionObject["pingInterval"]) connectionDescription.pingInterval = connectionObject["pingInterval"].as<float>();
                    if (connectionObject["bufferSize"]) connectionDescription.bufferSize = connectionObject["bufferSize"].as<uint32_t>();

                    if (connectionObject["applicationName"]) connectionDescription.applicationName = connectionObject["applicationName"].as<std::string>();
                    if (connectionObject["streamName"]) connectionDescription.streamName = connectionObject["streamName"].as<std::string>();
                    if (connectionObject["overrideApplicationName"]) connectionDescription.overrideApplicationName = connectionObject["overrideApplicationName"].as<std::string>();
                    if (connectionObject["overrideStreamName"]) connectionDescription.overrideStreamName = connectionObject["overrideStreamName"].as<std::string>();
                    if (connectionObject["video"]) connectionDescription.video = connectionObject["video"].as<bool>();
                    if (connectionObject["audio"]) connectionDescription.audio = connectionObject["audio"].as<bool>();
                    if (connectionObject["data"]) connectionDescription.data = connectionObject["data"].as<bool>();

                    connectionDescriptions.push_back(connectionDescription);
                }
            }

            std::unique_ptr<Server> server(new Server(*this, network));

            for (Connection::Description& description : connectionDescriptions)
            {
                description.server = server.get();
            }

            server->start(connectionDescriptions);

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

    const Connection::Description* Relay::getConnectionDescription(const std::pair<uint32_t, uint16_t>& address, Connection::StreamType type, const std::string& applicationName, const std::string& streamName) const
    {
        for (const std::unique_ptr<Server>& server : servers)
        {
            const std::vector<Connection::Description>& serverDescription = server->getConnectionDescriptions();

            for (const Connection::Description& connectionDescription : serverDescription)
            {
                if ((connectionDescription.applicationName.empty() || connectionDescription.applicationName == applicationName) &&
                    (connectionDescription.streamName.empty() || connectionDescription.streamName == streamName))
                {
                    if (connectionDescription.streamType == type)
                    {
                        if (std::find(connectionDescription.ipAddresses.begin(),
                                      connectionDescription.ipAddresses.end(),
                                      address) != connectionDescription.ipAddresses.end())
                        {
                            return &connectionDescription;
                        }
                    }
                }
            }
        }

        return nullptr;
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

    void Relay::getInfo(std::string& str, ReportType reportType) const
    {
        switch (reportType)
        {
            case ReportType::TEXT:
            {
                str = "Connections:\n";
                for (const auto& connection : connections)
                {
                    connection->getInfo(str, reportType);
                }

                for (const auto& server : servers)
                {
                    server->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str = "<html><title>Status</title><body>";
                str += "<table><tr><th>ID</th><th>Name</th><th>Application</th><th>Status</th><th>Address</th><th>Connection</th><th>State</th><th>Stream</th></tr>";

                for (const auto& connection : connections)
                {
                    connection->getInfo(str, reportType);
                }

                for (const auto& server : servers)
                {
                    server->getInfo(str, reportType);
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
                    connection->getInfo(str, reportType);
                }

                for (const auto& server : servers)
                {
                    if (!first) str += ",";
                    first = false;
                    server->getInfo(str, reportType);
                }

                str += "]}";
                
                break;
            }
        }
    }

    void Relay::openLog()
    {
#if !defined(_MSC_VER)
        openlog(syslogIdent.empty() ? nullptr : syslogIdent.c_str(), 0, syslogFacility);
#endif
    }

    void Relay::closeLog()
    {
#if !defined(_MSC_VER)
        closelog();
#endif
    }

    void Relay::handleAccept(cppsocket::Socket&, cppsocket::Socket& clientSocket)
    {
        std::unique_ptr<Connection> connection(new Connection(*this, clientSocket));

        connections.push_back(std::move(connection));
    }
}
