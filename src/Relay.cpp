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

    Relay::Relay(Relay&& other):
        network(other.network),
        previousTime(other.previousTime),
        connections(std::move(other.connections))
    {
    }

    Relay& Relay::operator=(Relay&& other)
    {
        previousTime = other.previousTime;
        connections = std::move(other.connections);

        return *this;
    }

    bool Relay::init(const std::string& config)
    {
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

            if (statusPageObject["listen"])
            {
                status.reset(new Status(network, *this, statusPageObject["listen"].as<std::string>()));
            }
        }

        const YAML::Node& applicationsArray = document["applications"];

        for (size_t applicationIndex = 0; applicationIndex < applicationsArray.size(); ++applicationIndex)
        {
            const YAML::Node& applicationObject = applicationsArray[applicationIndex];

            const YAML::Node& inputArray = applicationObject["inputs"];

            for (size_t inputIndex = 0; inputIndex < inputArray.size(); ++inputIndex)
            {
                const YAML::Node& inputObject = inputArray[inputIndex];
            }

            const YAML::Node& outputArray = applicationObject["outputs"];

            for (size_t outputIndex = 0; outputIndex < outputArray.size(); ++outputIndex)
            {
                const YAML::Node& outputObject = outputArray[outputIndex];
            }
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
                str = "Connections:\n";
                for (const auto& connection : connections)
                {
                    connection->getInfo(str, reportType);
                }
                break;
            }
            case ReportType::HTML:
            {
                str = "<html><title>Status</title><body>";

                for (const auto& connection : connections)
                {
                    connection->getInfo(str, reportType);
                }

                str += "</body></html>";

                break;
            }
            case ReportType::JSON:
            {
                str = "{\"servers\":[";

                bool first = true;

                for (const auto& connection : connections)
                {
                    if (!first) str += ",";
                    first = false;
                    connection->getInfo(str, reportType);
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

    void Relay::handleAccept(cppsocket::Acceptor&, cppsocket::Socket& clientSocket)
    {
        std::unique_ptr<Connection> connection(new Connection(*this, clientSocket));

        connections.push_back(std::move(connection));
    }
}
