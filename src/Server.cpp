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

Server::Server(Network& network):
    _network(network), _socket(network)
{
    _socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
}

Server::~Server()
{
    
}

Server::Server(Server&& other):
    _network(other._network),
    _socket(std::move(other._socket)),
    _outputs(std::move(other._outputs)),
    _inputs(std::move(other._inputs))
{
    _socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
}

Server& Server::operator=(Server&& other)
{
    _socket = std::move(other._socket);
    _outputs = std::move(other._outputs);
    _inputs = std::move(other._inputs);
    
    _socket.setAcceptCallback(std::bind(&Server::handleAccept, this, std::placeholders::_1));
    
    return *this;
}

bool Server::init(uint16_t port, const std::vector<std::string>& pushAddresses)
{
    _socket.startAccept(port);
    
    for (const std::string address : pushAddresses)
    {
        Output output(_network);
        
        if (output.init(address))
        {            
            _outputs.push_back(std::move(output));
        }
    }
    
    return true;
}

void Server::update()
{
    for (Output& output : _outputs)
    {
        output.update();
    }
    
    for (std::vector<Input>::iterator inputIterator = _inputs.begin(); inputIterator != _inputs.end();)
    {
        if ((*inputIterator).isConnected())
        {
            (*inputIterator).update();
            ++inputIterator;
        }
        else
        {
            inputIterator = _inputs.erase(inputIterator);
        }
    }
}

void Server::handleAccept(Socket socket)
{
    // accept only one input
    if (_inputs.empty())
    {
        Input input(_network, std::move(socket));
        _inputs.push_back(std::move(input));
    }
    else
    {
        socket.close();
    }
}
