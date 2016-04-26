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

Socket::Socket(Network& network, int socketFd):
    _network(network), _socketFd(socketFd)
{
    if (_socketFd < 0)
    {
        _socketFd = socket(AF_INET, SOCK_STREAM, 0);
        
        if (_socketFd < 0)
        {
            int error = errno;
            std::cerr << "Failed to create socket, error: " << error << std::endl;
        }
    }
    
    _network.addSocket(*this);
}

Socket::~Socket()
{
    _network.removeSocket(*this);
    
    if (_socketFd > 0)
    {
        if (::close(_socketFd) < 0)
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
    _network(other._network),
    _socketFd(other._socketFd),
    _connecting(other._connecting),
    _ready(other._ready),
    _blocking(other._blocking),
    _readCallback(std::move(other._readCallback)),
    _ipAddress(other._ipAddress),
    _port(other._port)
{
    _network.addSocket(*this);
    
    other._socketFd = -1;
    other._connecting = false;
    other._ready = false;
    other._blocking = true;
    other._ipAddress = 0;
    other._port = 0;
}

Socket& Socket::operator=(Socket&& other)
{
    _socketFd = other._socketFd;
    _connecting = other._connecting;
    _ready = other._ready;
    _blocking = other._blocking;
    _readCallback = std::move(other._readCallback);
    _ipAddress = other._ipAddress;
    _port = other._port;
    
    other._socketFd = -1;
    other._connecting = false;
    other._ready = false;
    other._blocking = true;
    other._ipAddress = 0;
    other._port = 0;
    
    return *this;
}

void Socket::close()
{
    if (_socketFd > 0)
    {
        ::close(_socketFd);
        _socketFd = 0;
    }
}

bool Socket::connect(const std::string& address, uint16_t port)
{
    uint32_t ip;
    
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
        portStr = std::to_string(port);
    }
    
    addrinfo* result;
    if (getaddrinfo(addressStr.c_str(), portStr.empty() ? nullptr : portStr.c_str(), nullptr, &result) != 0)
    {
        int error = errno;
        std::cerr << "Failed to get address info, error: " << error << std::endl;
        return false;
    }
    
    struct sockaddr_in* addr = reinterpret_cast<struct sockaddr_in*>(result->ai_addr);
    ip = addr->sin_addr.s_addr;
    port = ntohs(addr->sin_port);
    
    freeaddrinfo(result);
    
    return connect(ip, port);
}

bool Socket::connect(uint32_t ipAddress, uint16_t port)
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

    _ipAddress = ipAddress;
    _port = port;
    
    std::cout << "Connecting to " << ipToString(_ipAddress) << ":" << static_cast<int>(port) << std::endl;
    
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = _ipAddress;
    
    if (::connect(_socketFd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        if (errno == EINPROGRESS)
        {
            _connecting = true;
        }
        else
        {
            int error = errno;
            std::cerr << "Failed to connect to " << ipToString(_ipAddress) << ":" << _port << ", error: " << error << std::endl;
            return false;
        }
    }
    else
    {
        // connected
        _ready = true;
        if (_connectCallback) _connectCallback();
    }
    
    return true;
}

bool Socket::startRead()
{
    if (_socketFd < 0)
    {
        std::cerr << "Can not start reading, invalid socket" << std::endl;
        return false;
    }
    
    _ready = true;
    
    return true;
}

void Socket::setConnectCallback(const std::function<void()>& connectCallback)
{
    _connectCallback = connectCallback;
}

void Socket::setReadCallback(const std::function<void(const std::vector<uint8_t>&)>& readCallback)
{
    _readCallback = readCallback;
}

void Socket::setCloseCallback(const std::function<void()>& closeCallback)
{
    _closeCallback = closeCallback;
}

bool Socket::setBlocking(bool blocking)
{
#ifdef WIN32
    unsigned long mode = blocking ? 0 : 1;
    if (ioctlsocket(_socketFd, FIONBIO, &mode) != 0)
    {
        return false;
    }
#else
    int flags = fcntl(_socketFd, F_GETFL, 0);
    if (flags < 0) return false;
    flags = blocking ? (flags&~O_NONBLOCK) : (flags|O_NONBLOCK);
    
    if (fcntl(_socketFd, F_SETFL, flags) != 0)
    {
        return false;
    }
#endif
    
    _blocking = blocking;
    
    return true;
}

bool Socket::send(std::vector<uint8_t> buffer)
{
    ssize_t size = ::send(_socketFd, buffer.data(), buffer.size(), 0);

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
    ssize_t size = recv(_socketFd, TEMP_BUFFER, sizeof(TEMP_BUFFER), 0);
    
    if (size < 0)
    {
        int error = errno;
        
        if (_connecting)
        {
            std::cerr << "Failed to connect to " << ipToString(_ipAddress) << ":" << _port << ", error: " << error << std::endl;
            _connecting = false;
        }
        else
        {
            std::cerr << "Failed to read from socket, error: " << error << std::endl;
        }
        
        _ready = false;
        
        if (_closeCallback)
        {
            _closeCallback();
        }
        
        return false;
    }
    else if (size == 0)
    {
        std::cout << "Socket disconnected" << std::endl;
        _ready = false;
        
        if (_closeCallback)
        {
            _closeCallback();
        }
        
        return false;
    }

#ifdef DEBUG
    std::cout << "Socket received " << size << " bytes" << std::endl;
#endif

    std::vector<uint8_t> data(TEMP_BUFFER, TEMP_BUFFER + size);

    if (_readCallback)
    {
        _readCallback(data);
    }
    
    return true;
}

bool Socket::write()
{
    if (_connecting)
    {
        _connecting = false;
        _ready = true;
        std::cout << "Socket connected to " << ipToString(_ipAddress) << ":" << _port << std::endl;
        if (_connectCallback) _connectCallback();
    }
    
    return true;
}
