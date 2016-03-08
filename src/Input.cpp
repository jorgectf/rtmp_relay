//
//  rtmp_relay
//

#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include "Input.h"
#include "Utils.h"

static const uint32_t BUFFER_SIZE = 65536;

Input::Input()
{
    
}

Input::~Input()
{
    if (_socket > 0) close(_socket);
}

bool Input::init(int serverSocket)
{
    sockaddr_in address;
    socklen_t addressLength;
    
    _socket = accept(serverSocket, reinterpret_cast<sockaddr*>(&address), &addressLength);
    
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
    
    _data.resize(BUFFER_SIZE);
    
    return true;
}

bool Input::readPacket(std::vector<char>& packet)
{
    ssize_t size = recv(_socket, _data.data() + _dataSize, _data.size() - _dataSize, 0);
    
    if (size < 0)
    {
        int error = errno;
        std::cerr << "Failed to read from socket, error: " << error << std::endl;
        _closed = true;
        return false;
    }
    else if (size == 0)
    {
        int error = errno;
        std::cerr << "Socket disconnected, error: " << error << std::endl;
        _closed = true;
        return false;
    }
    
    _dataSize += size;
    
    if (_dataSize)
    {
        packet.resize(_dataSize);
        std::copy(_data.begin(), _data.begin() + _dataSize, packet.begin());
        _dataSize = 0;
        return true;
    }
    
    return false;
}
