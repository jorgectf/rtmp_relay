//
//  rtmp_relay
//

#include <ctime>
#include <memory>
#include <functional>
#include <iostream>
#include <chrono>
#include <thread>
#include <rapidjson/rapidjson.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/document.h>
#include "Relay.h"
#include "Server.h"

static char TEMP_BUFFER[65536];

namespace relay
{
    Relay::Relay()
    {
        previousTime = cppsocket::Network::getTime();
    }

    bool Relay::init(const std::string& config)
    {
        std::unique_ptr<FILE, std::function<int(FILE*)>> file(fopen(config.c_str(), "r"), std::fclose);
        
        if (!file)
        {
            std::cerr << "Failed to open file" << std::endl;
            return false;
        }
        
        rapidjson::FileReadStream is(file.get(), TEMP_BUFFER, sizeof(TEMP_BUFFER));
        
        rapidjson::Document document;
        document.ParseStream<0>(is);
        
        if (document.HasParseError())
        {
            std::cerr << "Failed to open file" << std::endl;
            return false;
        }
        
        const rapidjson::Value& serversArray = document["servers"];
        
        for (rapidjson::SizeType serverIndex = 0; serverIndex < serversArray.Size(); ++serverIndex)
        {
            const rapidjson::Value& serverObject = serversArray[serverIndex];
            
            std::vector<std::string> pushAddresses;

            std::string application = serverObject["application"].GetString();
            const rapidjson::Value& pushArray = serverObject["push"];
            
            std::shared_ptr<Server> server = std::make_shared<Server>(network, application);

            std::vector<Server::SenderDescriptor> senderDescriptors;

            for (uint32_t pushIndex = 0; pushIndex < static_cast<uint32_t>(pushArray.Size()); ++pushIndex)
            {
                Server::SenderDescriptor senderDescriptor;

                const rapidjson::Value& pushObject = pushArray[pushIndex];

                if (pushObject.HasMember("address"))
                {
                    const rapidjson::Value& addressArray = pushObject["address"];

                    for (rapidjson::SizeType index = 0; index < addressArray.Size(); ++index)
                    {
                        senderDescriptor.addresses.push_back(addressArray[index].GetString());
                    }
                }
                else
                {
                    senderDescriptor.addresses.push_back("127.0.0.1:1935");
                }

                senderDescriptor.videoOutput = pushObject.HasMember("video") ? pushObject["video"].GetBool() : true;
                senderDescriptor.audioOutput = pushObject.HasMember("audio") ? pushObject["audio"].GetBool() : true;
                senderDescriptor.dataOutput = pushObject.HasMember("data") ? pushObject["data"].GetBool() : true;

                if (pushObject.HasMember("metaDataBlacklist"))
                {
                    const rapidjson::Value& metaDataBlacklistArray = pushObject["metaDataBlacklist"];

                    for (rapidjson::SizeType index = 0; index < metaDataBlacklistArray.Size(); ++index)
                    {
                        const rapidjson::Value& str = metaDataBlacklistArray[index];
                        senderDescriptor.metaDataBlacklist.insert(str.GetString());
                    }
                }

                senderDescriptor.connectionTimeout = pushObject.HasMember("connectionTimeout") ? pushObject["connectionTimeout"].GetFloat() : 5.0f;
                senderDescriptor.reconnectInterval = pushObject.HasMember("reconnectInterval") ? pushObject["reconnectInterval"].GetFloat() : 5.0f;
                senderDescriptor.reconnectCount = pushObject.HasMember("reconnectCount") ? pushObject["reconnectCount"].GetUint() : 3;
                
                senderDescriptors.push_back(senderDescriptor);
            }
            
            if (server->init(static_cast<uint16_t>(serverObject["port"].GetInt()), senderDescriptors))
            {
                servers.push_back(std::move(server));
            }
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
            uint64_t currentTime = cppsocket::Network::getTime();
            float delta = static_cast<float>((currentTime - previousTime)) / 1000000.0f;
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
