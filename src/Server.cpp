//
//  rtmp_relay
//

#include <queue>
#include <iostream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Server.h"

Server::Server(Network& pNetwork, const std::string& pApplication):
    network(pNetwork), socket(pNetwork), application(pApplication)
{
    socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
}

Server::~Server()
{
    
}

Server::Server(Server&& other):
    network(other.network),
    socket(std::move(other.socket)),
    application(std::move(other.application)),
    senders(std::move(other.senders)),
    receivers(std::move(other.receivers))
{
    socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
}

Server& Server::operator=(Server&& other)
{
    socket = std::move(other.socket);
    application = std::move(other.application);
    senders = std::move(other.senders);
    receivers = std::move(other.receivers);
    
    socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
    
    return *this;
}

bool Server::init(uint16_t port, const std::vector<std::string>& pushAddresses)
{
    socket.startAccept(port);
    
    for (const std::string& address : pushAddresses)
    {
        std::auto_ptr<Sender> sender(new Sender(network, application));
        
        if (sender->init(address))
        {            
            senders.push_back(std::move(sender));
        }
    }
    
    return true;
}

void Server::update()
{
    for (auto sender : senders)
    {
        sender->update();
    }
    
    for (auto receiverIterator = receivers.begin(); receiverIterator != receivers.end();)
    {
        auto receiver = *receiverIterator;

        if (receiver->isConnected())
        {
            receiver->update();
            ++receiverIterator;
        }
        else
        {
            receiverIterator = receivers.erase(receiverIterator);
        }
    }
}

void Server::handleAccept(Socket clientSocket)
{
    // accept only one input
    if (receivers.empty())
    {
        std::auto_ptr<Receiver> receiver(new Receiver(network, std::move(clientSocket), application));
        receivers.push_back(std::move(receiver));
    }
    else
    {
        clientSocket.close();
    }
}

void Server::printInfo() const
{
    std::cout << "Server listening on " << socket.getPort() << ", application: " << application << std::endl;

    std::cout << "Senders:" << std::endl;
    for (const auto& sender : senders)
    {
        sender->printInfo();
    }

    std::cout << "Receivers:" << std::endl;
    for (const auto& receiver : receivers)
    {
        receiver->printInfo();
    }
}
