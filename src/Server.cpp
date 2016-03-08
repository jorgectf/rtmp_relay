//
//  rtmp_relay
//

#include <queue>
#include <iostream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include "Server.h"

Server::Server(Network& network):
    _network(network), _socket(network)
{
    
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

}

Server& Server::operator=(Server&& other)
{
    _socket = std::move(other._socket);
    _outputs = std::move(other._outputs);
    _inputs = std::move(other._inputs);
    
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
    
    for (Input& input : _inputs)
    {
        input.update();
    }
}
