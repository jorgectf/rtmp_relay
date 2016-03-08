//
//  rtmp_relay
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Acceptor.h"

static const int WAITING_QUEUE_SIZE = 5;

Acceptor::Acceptor(Network& network):
    Socket(network)
{

}

Acceptor::~Acceptor()
{
    for (int sock : _sockets)
    {
        close(sock);
    }
}

bool Acceptor::startAccept(uint16_t port)
{
    _port = port;
    
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (_socket < 0)
    {
        int error = errno;
        std::cerr << "Failed to create server socket, error: " << error << std::endl;
        return false;
    }
    
    sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(_socket, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0)
    {
        int error = errno;
        std::cerr << "Failed to bind server socket, error: " << error << std::endl;
        return false;
    }
    
    if (listen(_socket, WAITING_QUEUE_SIZE) < 0)
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
    
    int socket = ::accept(_socket, reinterpret_cast<sockaddr*>(&address), &addressLength);
    
    if (_socket < 0)
    {
        int error = errno;
        std::cerr << "Failed to accept client, error: " << error << std::endl;
        return false;
    }
    else
    {
        unsigned char* ip = reinterpret_cast<unsigned char*>(&address.sin_addr.s_addr);
        
        std::cout << "Client connected from " << static_cast<int>(ip[0]) << "." << static_cast<int>(ip[1]) << "." << static_cast<int>(ip[2]) << "." << static_cast<int>(ip[3]) << std::endl;
    }
    
    return true;
}
