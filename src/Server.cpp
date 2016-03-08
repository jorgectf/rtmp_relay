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

bool Server::init(uint16_t port, const std::vector<std::string>& pushAddresses)
{
    _socket.startAccept(port);
    
    for (const std::string address : pushAddresses)
    {
        std::unique_ptr<Output> output(new Output(_network));
        
        if (output->init(address))
        {            
            _outputs.push_back(std::move(output));
        }
    }
    
    return true;
}

void Server::update()
{
    for (const std::unique_ptr<Output>& output : _outputs)
    {
        output->update();
    }
    
    for (const std::unique_ptr<Input>& input : _inputs)
    {
        input->update();
    }
}
