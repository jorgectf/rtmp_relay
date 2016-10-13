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
#include "Relay.h"
#include "Server.h"

namespace relay
{
    Relay::Relay(cppsocket::Network& pNetwork):
        network(pNetwork)
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

        YAML::Node document;

        try
        {
            document = YAML::LoadFile(config);
        }
        catch (std::exception)
        {
            std::cerr << "Failed to open file" << std::endl;
            return false;
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

                const YAML::Node& pushArray = applicationObject["push"];

                for (size_t pushIndex = 0; pushIndex < pushArray.size(); ++pushIndex)
                {
                    SenderDescriptor senderDescriptor;

                    const YAML::Node& pushObject = pushArray[pushIndex];

                    std::vector<std::string> pushAddresses;

                    if (pushObject["address"])
                    {
                        const YAML::Node& addressArray = pushObject["address"];

                        for (size_t index = 0; index < addressArray.size(); ++index)
                        {
                            senderDescriptor.addresses.push_back(addressArray[index].as<std::string>());
                        }
                    }
                    else
                    {
                        senderDescriptor.addresses.push_back("127.0.0.1:1935");
                    }

                    senderDescriptor.videoOutput = pushObject["video"] ? pushObject["video"].as<bool>() : true;
                    senderDescriptor.audioOutput = pushObject["audio"] ? pushObject["audio"].as<bool>() : true;
                    senderDescriptor.dataOutput = pushObject["data"] ? pushObject["data"].as<bool>() : true;

                    if (pushObject["metaDataBlacklist"])
                    {
                        const YAML::Node& metaDataBlacklistArray = pushObject["metaDataBlacklist"];

                        for (size_t index = 0; index < metaDataBlacklistArray.size(); ++index)
                        {
                            const YAML::Node& str = metaDataBlacklistArray[index];
                            senderDescriptor.metaDataBlacklist.insert(str.as<std::string>());
                        }
                    }

                    senderDescriptor.connectionTimeout = pushObject["connectionTimeout"] ? pushObject["connectionTimeout"].as<float>() : 5.0f;
                    senderDescriptor.reconnectInterval = pushObject["reconnectInterval"] ? pushObject["reconnectInterval"].as<float>() : 5.0f;
                    senderDescriptor.reconnectCount = pushObject["reconnectCount"] ? pushObject["reconnectCount"].as<uint32_t>() : 3;
                    
                    applicationDescriptor.senderDescriptors.push_back(senderDescriptor);
                }

                applicationDescriptors.push_back(applicationDescriptor);
            }

            std::string address = serverObject["listen"] ? serverObject["listen"].as<std::string>() : "0.0.0.0:1935";
            float pingInterval = serverObject["pingInterval"] ? serverObject["pingInterval"].as<float>() : 60.0f;

            std::unique_ptr<Server> server(new Server(network, address, pingInterval, applicationDescriptors));
            servers.push_back(std::move(server));
        }
        
        return true;
    }

    Relay::~Relay()
    {
        
    }

    void Relay::run()
    {
        const std::chrono::microseconds sleepTime(10000);
        
        for (;;)
        {
            auto currentTime = std::chrono::steady_clock::now();
            float delta = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - previousTime).count() / 1000000.0f;
            previousTime = currentTime;

            network.update();
            
            for (const auto& server : servers)
            {
                server->update(delta);
            }

            std::this_thread::sleep_for(sleepTime);
        }
    }

    void Relay::printInfo() const
    {
        for (const auto& server : servers)
        {
            server->printInfo();
        }
    }
}
