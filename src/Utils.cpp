//
//  rtmp_relay
//

#include "Utils.h"

std::string ipToString(uint32_t ip)
{
    unsigned char* ptr = reinterpret_cast<unsigned char*>(&ip);
    
    return std::to_string(static_cast<int>(ptr[0])) + "." +
    std::to_string(static_cast<int>(ptr[1])) + "." +
    std::to_string(static_cast<int>(ptr[2])) + "." +
    std::to_string(static_cast<int>(ptr[3]));
}
