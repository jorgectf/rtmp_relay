//
//  rtmp_relay
//

#include <netdb.h>
#include <iostream>
#include "Output.h"
#include "Utils.h"

Output::Output()
{
    
}

Output::~Output()
{
    if (_socket > 0) close(_socket);
}

bool Output::init(const std::string& address)
{
    std::string addressStr = address;
    size_t i = address.find(':');
    std::string portStr;
    
    if (i != -1)
    {
        addressStr = address.substr(0, i);
        portStr = address.substr(i + 1);
    }
    
    addrinfo* result;
    if (getaddrinfo(addressStr.c_str(), portStr.empty() ? nullptr : portStr.c_str(), nullptr, &result) != 0)
    {
        std::cerr << "Failed to get address info" << std::endl;
        return false;
    }
    
    _socket = socket(AF_INET, SOCK_STREAM, 0);
    
    if (_socket < 0)
    {
        std::cerr << "Failed to create socket" << std::endl;
        freeaddrinfo(result);
        return false;
    }
    
    if (!setBlocking(_socket, false))
    {
        std::cerr << "Failed to set socket non-blocking" << std::endl;
        return false;
    }
    
    struct sockaddr_in* addr = (struct sockaddr_in*)result->ai_addr;
    unsigned char* ip = (unsigned char*)&addr->sin_addr.s_addr;
    
    std::cout << "Connecting to " << static_cast<int>(ip[0]) << "." << static_cast<int>(ip[1]) << "." << static_cast<int>(ip[2]) << "." << static_cast<int>(ip[3]) << ":" << static_cast<int>(ntohs(addr->sin_port)) << std::endl;
    
    if (connect(_socket, result->ai_addr, result->ai_addrlen) < 0)
    {
        if (errno != EINPROGRESS)
        {
            std::cerr << "Connection failed" << std::endl;
            freeaddrinfo(result);
            return false;
        }
    }
    
    freeaddrinfo(result);
    
    // TODO: make handshake
    
    return true;
}

void Output::connected()
{
    std::cout << "Connected" << std::endl;
    _connected = true;
}

bool Output::sendPacket(const std::vector<char>& packet)
{
    if (send(_socket, packet.data(), packet.size(), 0) < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            std::cerr << "Failed to send data" << std::endl;
            return false;
        }
    }
    
    return true;
}
