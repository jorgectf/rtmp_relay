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
    outputs(std::move(other.outputs)),
    inputs(std::move(other.inputs)),
    application(std::move(other.application))
{
    socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
}

Server& Server::operator=(Server&& other)
{
    socket = std::move(other.socket);
    outputs = std::move(other.outputs);
    inputs = std::move(other.inputs);
    application = std::move(other.application);
    
    socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
    
    return *this;
}

bool Server::init(uint16_t port, const std::vector<std::string>& pushAddresses)
{
    socket.startAccept(port);
    
    for (const std::string& address : pushAddresses)
    {
        Output output(network);
        
        if (output.init(address))
        {            
            outputs.push_back(std::move(output));
        }
    }
    
    return true;
}

void Server::update()
{
    for (Output& output : outputs)
    {
        output.update();
    }
    
    for (auto inputIterator = inputs.begin(); inputIterator != inputs.end();)
    {
        if (inputIterator->isConnected())
        {
            inputIterator->update();
            ++inputIterator;
        }
        else
        {
            inputIterator = inputs.erase(inputIterator);
        }
    }
}

void Server::handleAccept(Socket clientSocket)
{
    // accept only one input
    if (inputs.empty())
    {
        Input input(network, std::move(clientSocket), application);
        inputs.push_back(std::move(input));
    }
    else
    {
        clientSocket.close();
    }
}
