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
    Server(Network& pNetwork, const std::string& pApplication);
    ~Server();
    
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
    
    Server(Server&& other);
    Server& operator=(Server&& other);
    
    bool init(uint16_t port, const std::vector<std::string>& pushAddresses);
    
    void update();
    
protected:
    void handleAccept(Socket clientSocket);
    
    Network& network;
    Acceptor socket;
    std::string application;
    
    std::vector<Output> outputs;
    std::vector<Input> inputs;
};
