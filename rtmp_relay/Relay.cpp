//
//  rtmp_relay
//

#include <ctime>
#include <memory>
#include <functional>
#include <rapidjson/rapidjson.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/document.h>
#include "Relay.h"
#include "Server.h"

Relay::Relay()
{
    
}

bool Relay::init()
{
    char TEMP_BUFFER[65536];
    
    std::unique_ptr<FILE, std::function<void(FILE*)>> file(fopen("/Users/elviss/Projects/rtmp_relay/config.json", "r"), std::fclose);
    
    if (!file)
    {
        fprintf(stderr, "Failed to open file\n");
        return false;
    }
    
    rapidjson::FileReadStream is(file.get(), TEMP_BUFFER, sizeof(TEMP_BUFFER));
    
    rapidjson::Document document;
    document.ParseStream<0>(is);
    
    if (document.HasParseError())
    {
        fprintf(stderr, "Failed to open file\n");
        return false;
    }
    
    const rapidjson::Value& serversArray = document["servers"];
    
    for (uint32_t serverIndex = 0; serverIndex < static_cast<uint32_t>(serversArray.Size()); ++serverIndex)
    {
        const rapidjson::Value& serverObject = serversArray[serverIndex];
        
        std::vector<std::string> pushUrls;
        
        const rapidjson::Value& pushArray = serverObject["push"];
        
        for (uint32_t pushIndex = 0; pushIndex < static_cast<uint32_t>(pushArray.Size()); ++pushIndex)
        {
            const rapidjson::Value& pushObject = pushArray[pushIndex];
            
            pushUrls.push_back(pushObject.GetString());
        }
        
        std::unique_ptr<Server> server(new Server(serverObject["port"].GetInt(), pushUrls));
        
        _servers.push_back(std::move(server));
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
        for (const std::unique_ptr<Server>& server : _servers)
        {
            server->update();
        }
        
        nanosleep(&sleepTime, nullptr);
    }
}
