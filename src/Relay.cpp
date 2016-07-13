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
            
            std::shared_ptr<Server> server = std::make_shared<Server>(network, application);
            
            if (server->init(static_cast<uint16_t>(serverObject["port"].GetInt()), pushArray))
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
            network.update();
            
            for (const auto& server : servers)
            {
                server->update();
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
