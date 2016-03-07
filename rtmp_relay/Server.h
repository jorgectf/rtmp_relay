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
#include "Output.h"
#include "Input.h"

class Server: public Noncopyable
{
public:
    Server();
    ~Server();
    
    bool init(uint16_t port, const std::vector<std::string>& pushUrls);
    
    void update();
    
private:
    uint16_t _port;
    
    int _socket = 0;
    
    std::vector<std::string> _pushUrls;
    std::vector<std::unique_ptr<Output>> _outputs;
    
    std::vector<pollfd> _pollFds;
    std::vector<std::unique_ptr<Input>> _inputs;
};
