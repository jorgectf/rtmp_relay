//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <memory>
#include "Acceptor.h"
#include "Sender.h"
#include "Receiver.h"

namespace relay
{
    struct SenderDescriptor
    {
        std::vector<std::string> addresses;
        bool videoOutput;
        bool audioOutput;
        bool dataOutput;
        std::set<std::string> metaDataBlacklist;
        float connectionTimeout;
        float reconnectInterval;
        uint32_t reconnectCount;
    };

    struct ApplicationDescriptor
    {
        std::string name;
        std::vector<SenderDescriptor> senderDescriptors;
    };

    class Application;
    
    class Server
    {
    public:
        Server(cppsocket::Network& pNetwork,
               const std::string& address,
               float newPingInterval,
               const std::vector<ApplicationDescriptor>& newApplicationDescriptors);
        ~Server();
        
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        
        Server(Server&& other) = delete;
        Server& operator=(Server&& other) = delete;
        
        void update(float delta);

        bool connect(const std::string& applicationName);

        void createStream(const std::string& streamName);
        void deleteStream();
        void unpublishStream();
        void sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData);
        void sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData);
        void sendMetaData(const amf0::Node& metaData);
        void sendTextData(const amf0::Node& textData);

        void printInfo() const;
        
    protected:
        void handleAccept(cppsocket::Socket& clientSocket);
        
        cppsocket::Network& network;
        cppsocket::Acceptor socket;
        const std::vector<ApplicationDescriptor> applicationDescriptors;
        const float pingInterval;
        
        std::vector<std::unique_ptr<Receiver>> receivers;

        std::unique_ptr<Application> application;
    };
}
