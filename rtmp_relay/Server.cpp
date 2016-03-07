//
//  rtmp_relay
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include "Server.h"

static const int WAITING_QUEUE_SIZE = 5;

Server::Server()
{
    
}

Server::~Server()
{
    if (_socket > 0) close(_socket);
}

bool Server::init(uint16_t port, const std::vector<std::string>& pushUrls)
{
    _port = port;
    _pushUrls = pushUrls;
    
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (_socket < 0)
    {
        std::cerr << "Failed to create server socket" << std::endl;
        return false;
    }
    
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(_socket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
    {
        std::cerr << "Failed to bind server socket" << std::endl;
        return false;
    }
    
    if (listen(_socket, WAITING_QUEUE_SIZE) < 0)
    {
        std::cerr << "Failed to listen on port " << _port << std::endl;
        return false;
    }
    
    std::cout << "Server listening on port " << _port << std::endl;
    
    // TODO: read incoming data and push to outgoing
    
    return true;
}

void Server::update()
{
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(_socket, &readSet);
    
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    
    int maxSocket = _socket + 1;
    
    for (const std::unique_ptr<Input>& input : _inputs)
    {
        FD_SET(input->getSocket(), &readSet);
        
        if (input->getSocket() > maxSocket)
        {
            maxSocket = input->getSocket();
        }
    }
    
    if (select(maxSocket + 1, &readSet, NULL, NULL, &timeout) < 0)
    {
        std::cerr << "Error occurred" << std::endl;
        return;
    }
    
    if (FD_ISSET(_socket, &readSet))
    {
        std::unique_ptr<Input> input(new Input());
        
        if (input->init(_socket))
        {
            _inputs.push_back(std::move(input));
        }
    }
    
    for (std::vector<std::unique_ptr<Input>>::iterator i = _inputs.begin(); i != _inputs.end();)
    {
        if (FD_ISSET((*i)->getSocket(), &readSet))
        {
            if (!(*i)->readData())
            {
                // Failed to read from socket, disconnect it
                i = _inputs.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }
}
