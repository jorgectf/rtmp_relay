//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <poll.h>
#include "Noncopyable.h"
#include "Acceptor.h"
#include "Output.h"
#include "Input.h"

class Server: public Noncopyable
{
public:
    Server(Network& network);
    ~Server();
    
    bool init(uint16_t port, const std::vector<std::string>& pushAddresses);
    
    void update();
    
private:
    Network& _network;
    Acceptor _socket;
    
    std::vector<std::unique_ptr<Output>> _outputs;
    std::vector<std::unique_ptr<Input>> _inputs;
};
