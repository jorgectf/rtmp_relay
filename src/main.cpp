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
