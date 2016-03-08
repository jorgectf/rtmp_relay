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
