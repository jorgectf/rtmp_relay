//
//  rtmp_relay
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Acceptor.h"
#include "Utils.h"

static const int WAITING_QUEUE_SIZE = 5;

Acceptor::Acceptor(Network& network, int socketFd):
    Socket(network, socketFd)
{

}

Acceptor::~Acceptor()
{
    for (int clientSocket : _clientSockets)
    {
        close(clientSocket);
    }
}

Acceptor::Acceptor(Acceptor&& other):
    Socket(std::move(other)),
    _clientSockets(std::move(other._clientSockets))
{
    other._port = 0;
}

Acceptor& Acceptor::operator=(Acceptor&& other)
{
    Socket::operator=(std::move(other));
    _port = other._port;
    _clientSockets = std::move(other._clientSockets);
    
    other._port = 0;
    
    return *this;
}

bool Acceptor::startAccept(uint16_t port)
{
    if (_socketFd < 0)
    {
        _socketFd = socket(AF_INET, SOCK_STREAM, 0);
        
        if (_socketFd < 0)
        {
            int error = errno;
            std::cerr << "Failed to create socket, error: " << error << std::endl;
            return false;
        }
    }
    
    _port = port;
    
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(_socketFd, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
    {
        int error = errno;
        std::cerr << "Failed to bind server socket, error: " << error << std::endl;
        return false;
    }
    
    if (listen(_socketFd, WAITING_QUEUE_SIZE) < 0)
    {
        int error = errno;
        std::cerr << "Failed to listen on port " << _port << ", error: " << error << std::endl;
        return false;
    }
    
    std::cout << "Server listening on port " << _port << std::endl;
    _ready = true;
    
    return true;
}

bool Acceptor::read()
{
    sockaddr_in address;
    socklen_t addressLength;
    
    int socketFd = ::accept(_socketFd, reinterpret_cast<sockaddr*>(&address), &addressLength);
    
    if (_socketFd < 0)
    {
        int error = errno;
        std::cerr << "Failed to accept client, error: " << error << std::endl;
        return false;
    }
    else
    {
        std::cout << "Client connected from " << ipToString(address.sin_addr.s_addr) << std::endl;
        
        Socket socket(_network, socketFd);
    }
    
    return true;
}
