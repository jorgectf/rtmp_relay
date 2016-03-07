//
//  rtmp_relay
//

#include "Output.h"

Output::Output()
{
    
}

Output::~Output()
{
    if (_socket > 0) close(_socket);
}

bool init(const std::string& url)
{
    // TODO: open writing socket
    // TODO: make handshake
    
    return true;
}
