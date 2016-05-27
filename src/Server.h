//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <poll.h>
#include "Acceptor.h"
#include "Sender.h"
#include "Receiver.h"

namespace relay
{
    class Server: public std::enable_shared_from_this<Server>
    {
    public:
        Server(Network& pNetwork, const std::string& pApplication);
        ~Server();
        
        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;
        
        Server(Server&& other) = delete;
        Server& operator=(Server&& other) = delete;
        
        bool init(uint16_t port, const std::vector<std::string>& pushAddresses);
        
        void update();

        void createStream(const std::string& streamName);

        void printInfo() const;
        
    protected:
        void handleAccept(Socket clientSocket);
        
        Network& network;
        Acceptor socket;
        std::string application;
        
        std::vector<std::shared_ptr<Sender>> senders;
        std::vector<std::shared_ptr<Receiver>> receivers;
    };
}
