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
    class Server: public std::enable_shared_from_this<Server>
    {
    public:
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

        Server(cppsocket::Network& pNetwork, const std::string& pApplication);
        ~Server();
        
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        
        Server(Server&& other) = delete;
        Server& operator=(Server&& other) = delete;
        
        bool init(uint16_t port, const std::vector<SenderDescriptor>& newSenderDescriptors);
        
        void update(float delta);

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
        std::string application;
        
        std::vector<std::unique_ptr<Sender>> senders;
        std::vector<std::unique_ptr<Receiver>> receivers;

        std::vector<SenderDescriptor> senderDescriptors;
    };
}
