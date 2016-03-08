//
//  rtmp_relay
//

#include <iostream>
#include <unistd.h>
#include "Output.h"

Output::Output(Network& network):
    _network(network), _socket(_network)
{
    
}

Output::~Output()
{
    
}

Output::Output(Output&& other):
    _network(other._network),
    _socket(std::move(other._socket))
{
    
}

Output& Output::operator=(Output&& other)
{
    _socket = std::move(other._socket);
    
    return *this;
}

bool Output::init(const std::string& address)
{
    if (!_socket.setBlocking(false))
    {
        std::cerr << "Failed to set socket non-blocking" << std::endl;
        return false;
    }
    
    if (!_socket.connect(address))
    {
        return false;
    }
    
    // TODO: make handshake
    
    return true;
}

void Output::update()
{
    
}

void Output::connected()
{
    std::cout << "Connected" << std::endl;
    //_connected = true;
}

bool Output::sendPacket(const std::vector<char>& packet)
{
    _socket.send(packet);
    
    return true;
}
