//
//  rtmp_relay
//

#include <iostream>
#include "Relay.h"

int main(int argc, const char * argv[])
{
    Relay relay;
    
    if (!relay.init())
    {
        std::cerr << "Failed to init relay" << std::endl;
        return 1;
    }
    
    relay.run();
    
    return 0;
}
