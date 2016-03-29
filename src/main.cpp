//
//  rtmp_relay
//

#include <iostream>
#include "Relay.h"

int main(int argc, const char * argv[])
{
    if (argc < 2)
    {
        std::cerr << "Too few arguments" << std::endl;

        const char* exe = argc >= 1 ? argv[0] : "rtmp_relay";
        std::cerr << "Usage: " << exe << " <path to config file>" << std::endl;

        return 1;
    }
    
    Relay relay;
    
    if (!relay.init(argv[1]))
    {
        std::cerr << "Failed to init relay" << std::endl;
        return 1;
    }
    
    relay.run();
    
    return 0;
}
