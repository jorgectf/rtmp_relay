//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <poll.h>
#include "Acceptor.h"
#include "Output.h"
#include "Input.h"

class Server
{
public:
    Server() = default;
    Server(Network& network);
    ~Server();
    
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
    Server(Server&& other);
    Server& operator=(Server&& other);
    
    bool init(uint16_t port, const std::vector<std::string>& pushAddresses);
    
    void update();
    
protected:
    void handleAccept(Socket socket);
    
    Network& _network;
    Acceptor _socket;
    
    std::vector<Output> _outputs;
    std::vector<Input> _inputs;
};
