//
//  rtmp_relay
//

#include <ctime>
#include <memory>
#include <functional>
#include <iostream>
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
        
    }

    bool Relay::init(const std::string& config)
    {
        std::unique_ptr<FILE, std::function<void(FILE*)>> file(fopen(config.c_str(), "r"), std::fclose);
        
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
            
            for (uint32_t pushIndex = 0; pushIndex < static_cast<uint32_t>(pushArray.Size()); ++pushIndex)
            {
                const rapidjson::Value& pushObject = pushArray[pushIndex];
                
                pushAddresses.push_back(pushObject.GetString());
            }
            
            std::shared_ptr<Server> server = std::make_shared<Server>(network, application);
            
            if (server->init(static_cast<uint16_t>(serverObject["port"].GetInt()), pushAddresses))
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
        const timespec sleepTime = { 0, 10000000 };
        
        while (true)
        {
            network.update();
            
            for (const auto& server : servers)
            {
                server->update();
            }
            
            nanosleep(&sleepTime, nullptr);
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
