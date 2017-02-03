//
//  rtmp_relay
//

#include <ctime>
#include <memory>
#include <functional>
#include <iostream>
#include <chrono>
#include <thread>
#include "yaml-cpp/yaml.h"
#include "Log.h"
#include "Relay.h"
#include "Server.h"
#include "Status.h"
#include "Application.h"

using namespace cppsocket;

namespace relay
{
    uint64_t Relay::currentId = 0;

    Relay::Relay(Network& aNetwork):
        network(aNetwork)
    {
        previousTime = std::chrono::steady_clock::now();
    }

    Relay::Relay(Relay&& other):
        network(other.network),
        servers(std::move(other.servers)),
        previousTime(other.previousTime)
    {
    }

    Relay& Relay::operator=(Relay&& other)
    {
        servers = std::move(other.servers);
        previousTime = other.previousTime;

        return *this;
    }

    bool Relay::init(const std::string& config)
    {
        servers.clear();
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

            if (statusPageObject["listen"])
            {
                status.reset(new Status(network, *this, statusPageObject["listen"].as<std::string>()));
            }
        }

        const YAML::Node& serversArray = document["servers"];

        for (size_t serverIndex = 0; serverIndex < serversArray.size(); ++serverIndex)
        {
            const YAML::Node& serverObject = serversArray[serverIndex];

            std::vector<ApplicationDescriptor> applicationDescriptors;

            const YAML::Node& applicationArray = serverObject["applications"];

            for (size_t applicationIndex = 0; applicationIndex < applicationArray.size(); ++applicationIndex)
            {
                const YAML::Node& applicationObject = applicationArray[applicationIndex];

                ApplicationDescriptor applicationDescriptor;

                if (applicationObject["name"]) applicationDescriptor.name = applicationObject["name"].as<std::string>();
                if (applicationObject["overrideApplicationName"]) applicationDescriptor.overrideApplicationName = applicationObject["overrideApplicationName"].as<std::string>();

                const YAML::Node& pushArray = applicationObject["push"];

                if (pushArray)
                {
                    for (size_t pushIndex = 0; pushIndex < pushArray.size(); ++pushIndex)
                    {
                        PushDescriptor pushDescriptor;

                        const YAML::Node& pushObject = pushArray[pushIndex];

                        if (pushObject["overrideStreamName"])
                        {
                            pushDescriptor.overrideStreamName = pushObject["overrideStreamName"].as<std::string>();
                        }

                        std::vector<std::string> pushAddresses;

                        if (pushObject["address"])
                        {
                            if (pushObject["address"].IsSequence())
                            {
                                const YAML::Node& addressArray = pushObject["address"];

                                for (size_t index = 0; index < addressArray.size(); ++index)
                                {
                                    pushDescriptor.addresses.push_back(addressArray[index].as<std::string>());
                                }
                            }
                            else
                            {
                                pushDescriptor.addresses.push_back(pushObject["address"].as<std::string>());
                            }
                        }
                        else
                        {
                            pushDescriptor.addresses.push_back("127.0.0.1:1935");
                        }

                        pushDescriptor.videoOutput = pushObject["video"] ? pushObject["video"].as<bool>() : true;
                        pushDescriptor.audioOutput = pushObject["audio"] ? pushObject["audio"].as<bool>() : true;
                        pushDescriptor.dataOutput = pushObject["data"] ? pushObject["data"].as<bool>() : true;

                        const YAML::Node& metaDataBlacklistArray = pushObject["metaDataBlacklist"];

                        if (metaDataBlacklistArray)
                        {
                            for (size_t index = 0; index < metaDataBlacklistArray.size(); ++index)
                            {
                                const YAML::Node& str = metaDataBlacklistArray[index];
                                pushDescriptor.metaDataBlacklist.insert(str.as<std::string>());
                            }
                        }

                        pushDescriptor.connectionTimeout = pushObject["connectionTimeout"] ? pushObject["connectionTimeout"].as<float>() : 5.0f;
                        pushDescriptor.reconnectInterval = pushObject["reconnectInterval"] ? pushObject["reconnectInterval"].as<float>() : 5.0f;
                        pushDescriptor.reconnectCount = pushObject["reconnectCount"] ? pushObject["reconnectCount"].as<uint32_t>() : 3;

                        applicationDescriptor.pushDescriptors.push_back(pushDescriptor);
                    }
                }

                const YAML::Node& pullArray = applicationObject["pull"];

                if (pullArray)
                {
                    for (size_t pullIndex = 0; pullIndex < pullArray.size(); ++pullIndex)
                    {
                        PullDescriptor pullDescriptor;

                        const YAML::Node& pullObject = pullArray[pullIndex];

                        if (pullObject["overrideStreamName"])
                        {
                            pullDescriptor.overrideStreamName = pullObject["overrideStreamName"].as<std::string>();
                        }

                        if (pullObject["listen"])
                        {
                            pullDescriptor.address = pullObject["listen"].as<std::string>();
                        }

                        pullDescriptor.videoOutput = pullObject["video"] ? pullObject["video"].as<bool>() : true;
                        pullDescriptor.audioOutput = pullObject["audio"] ? pullObject["audio"].as<bool>() : true;
                        pullDescriptor.dataOutput = pullObject["data"] ? pullObject["data"].as<bool>() : true;
                        pullDescriptor.pingInterval = pullObject["pingInterval"] ? pullObject["pingInterval"].as<float>() : 60.0f;

                        const YAML::Node& metaDataBlacklistArray = pullObject["metaDataBlacklist"];

                        if (metaDataBlacklistArray)
                        {
                            for (size_t index = 0; index < metaDataBlacklistArray.size(); ++index)
                            {
                                const YAML::Node& str = metaDataBlacklistArray[index];
                                pullDescriptor.metaDataBlacklist.insert(str.as<std::string>());
                            }
                        }

                        applicationDescriptor.pullDescriptors.push_back(pullDescriptor);
                    }
                }

                applicationDescriptors.push_back(applicationDescriptor);
            }

            std::string address = serverObject["listen"] ? serverObject["listen"].as<std::string>() : "0.0.0.0:1935";
            float pingInterval = serverObject["pingInterval"] ? serverObject["pingInterval"].as<float>() : 60.0f;

            std::unique_ptr<Server> server(new Server(network, address, pingInterval, applicationDescriptors));
            servers.push_back(std::move(server));
        }

        // TODO: create one connection instance for every connection in config

        return true;
    }

    std::vector<Connection*> Relay::getConnections(Connection::StreamType streamType,
                                                   const std::string& address,
                                                   const std::string& applicationName,
                                                   const std::string& streamName)
    {
        std::vector<Connection*> result;

        for (const auto& connection : connections)
        {
            // TODO: check if connection matches the request
        }

        return result;
    }

    void Relay::close()
    {
        servers.clear();
        status.reset();
    }

    void Relay::run()
    {
        const std::chrono::microseconds sleepTime(10000);

        for (;;)
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

            for (const auto& connection : connections)
            {
                connection->update();
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
                str = "Status:\n";
                for (const auto& server : servers)
                {
                    server->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str = "<html><title>Status</title><body>";

                for (const auto& server : servers)
                {
                    server->getInfo(str, reportType);
                }

                str += "</body></html>";

                break;
            }
            case ReportType::JSON:
            {
                str = "{\"servers\":[";

                bool first = true;

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
}
