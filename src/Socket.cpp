//
//  rtmp_relay
//

#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include "Socket.h"
#include "Network.h"
#include "Utils.h"

static uint8_t TEMP_BUFFER[65536];

Socket::Socket(Network& pNetwork, int pSocketFd):
    network(pNetwork), socketFd(pSocketFd)
{
    if (socketFd < 0)
    {
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        
        if (socketFd < 0)
        {
            int error = errno;
            std::cerr << "Failed to create socket, error: " << error << std::endl;
        }
    }
    
    network.addSocket(*this);
}

Socket::~Socket()
{
    network.removeSocket(*this);
    
    if (socketFd > 0)
    {
        if (::close(socketFd) < 0)
        {
            int error = errno;
            std::cerr << "Failed to close socket, error: " << error << std::endl;
        }
        else
        {
            std::cout << "Socket closed" << std::endl;
        }
    }
}

Socket::Socket(Socket&& other):
    network(other.network),
    socketFd(other.socketFd),
    connecting(other.connecting),
    ready(other.ready),
    blocking(other.blocking),
    ipAddress(other.ipAddress),
    port(other.port),
    readCallback(std::move(other.readCallback))
{
    network.addSocket(*this);
    
    other.socketFd = -1;
    other.connecting = false;
    other.ready = false;
    other.blocking = true;
    other.ipAddress = 0;
    other.port = 0;
}

Socket& Socket::operator=(Socket&& other)
{
    socketFd = other.socketFd;
    connecting = other.connecting;
    ready = other.ready;
    blocking = other.blocking;
    readCallback = std::move(other.readCallback);
    ipAddress = other.ipAddress;
    port = other.port;
    
    other.socketFd = -1;
    other.connecting = false;
    other.ready = false;
    other.blocking = true;
    other.ipAddress = 0;
    other.port = 0;
    
    return *this;
}

void Socket::close()
{
    if (socketFd > 0)
    {
        ::close(socketFd);
        socketFd = 0;
    }
}

bool Socket::connect(const std::string& address, uint16_t newPort)
{
    size_t i = address.find(':');
    std::string addressStr;
    std::string portStr;
    
    if (i != std::string::npos)
    {
        addressStr = address.substr(0, i);
        portStr = address.substr(i + 1);
    }
    else
    {
        addressStr = address;
        portStr = std::to_string(newPort);
    }
    
    addrinfo* result;
    if (getaddrinfo(addressStr.c_str(), portStr.empty() ? nullptr : portStr.c_str(), nullptr, &result) != 0)
    {
        int error = errno;
        std::cerr << "Failed to get address info, error: " << error << std::endl;
        return false;
    }
    
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    uint32_t ip = addr->sin_addr.s_addr;
    newPort = ntohs(addr->sin_port);
    
    freeaddrinfo(result);
    
    return connect(ip, newPort);
}

bool Socket::connect(uint32_t address, uint16_t newPort)
{
    if (socketFd < 0)
    {
        socketFd = socket(AF_INET, SOCK_STREAM, 0);
        
        if (socketFd < 0)
        {
            int error = errno;
            std::cerr << "Failed to create socket, error: " << error << std::endl;
            return false;
        }
    }

    ipAddress = address;
    port = newPort;
    ready = false;
    connecting = false;
    
    std::cout << "Connecting to " << ipToString(ipAddress) << ":" << static_cast<int>(port) << std::endl;
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = ipAddress;
    
    if (::connect(socketFd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        if (errno == EINPROGRESS)
        {
            connecting = true;
        }
        else
        {
            int error = errno;
            std::cerr << "Failed to connect to " << ipToString(ipAddress) << ":" << port << ", error: " << error << std::endl;
            return false;
        }
    }
    else
    {
        // connected
        ready = true;
        if (connectCallback) connectCallback();
    }
    
    return true;
}

bool Socket::startRead()
{
    if (socketFd < 0)
    {
        std::cerr << "Can not start reading, invalid socket" << std::endl;
        return false;
    }
    
    ready = true;
    
    return true;
}

void Socket::setConnectCallback(const std::function<void()>& newConnectCallback)
{
    connectCallback = newConnectCallback;
}

void Socket::setReadCallback(const std::function<void(const std::vector<uint8_t>&)>& newReadCallback)
{
    readCallback = newReadCallback;
}

void Socket::setCloseCallback(const std::function<void()>& newCloseCallback)
{
    closeCallback = newCloseCallback;
}

bool Socket::setBlocking(bool newBlocking)
{
#ifdef WIN32
    unsigned long mode = newBlocking ? 0 : 1;
    if (ioctlsocket(socketFd, FIONBIO, &mode) != 0)
    {
        return false;
    }
#else
    int flags = fcntl(socketFd, F_GETFL, 0);
    if (flags < 0) return false;
    flags = newBlocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
    
    if (fcntl(socketFd, F_SETFL, flags) != 0)
    {
        return false;
    }
#endif
    
    blocking = newBlocking;
    
    return true;
}

bool Socket::send(std::vector<uint8_t> buffer)
{
    ssize_t size = ::send(socketFd, buffer.data(), buffer.size(), 0);

    if (size < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            int error = errno;
            std::cerr << "Failed to send data, error: " << error << std::endl;
            return false;
        }
    }

#ifdef DEBUG
    std::cout << "Socket sent " << size << " bytes" << std::endl;
#endif
    
    return true;
}

bool Socket::read()
{
    ssize_t size = recv(socketFd, TEMP_BUFFER, sizeof(TEMP_BUFFER), 0);
    
    if (size < 0)
    {
        int error = errno;
        
        if (connecting)
        {
            std::cerr << "Failed to connect to " << ipToString(ipAddress) << ":" << port << ", error: " << error << std::endl;
            connecting = false;
        }
        else
        {
            if (error == ECONNRESET)
            {
                std::cerr << "Connection reset by peer" << std::endl;
            }
            else
            {
                std::cerr << "Failed to read from socket, error: " << error << std::endl;
            }
        }
        
        ready = false;
        
        if (closeCallback)
        {
            closeCallback();
        }
        
        return false;
    }
    else if (size == 0)
    {
        std::cout << "Socket disconnected" << std::endl;
        ready = false;
        
        if (closeCallback)
        {
            closeCallback();
        }
        
        return false;
    }

#ifdef DEBUG
    std::cout << "Socket received " << size << " bytes" << std::endl;
#endif

    std::vector<uint8_t> data(TEMP_BUFFER, TEMP_BUFFER + size);

    if (readCallback)
    {
        readCallback(data);
    }
    
    return true;
}

bool Socket::write()
{
    if (connecting)
    {
        connecting = false;
        ready = true;
        std::cout << "Socket connected to " << ipToString(ipAddress) << ":" << port << std::endl;
        if (connectCallback) connectCallback();
    }
    
    return true;
}
