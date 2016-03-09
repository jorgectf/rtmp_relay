//
//  rtmp_relay
//

#include "Utils.h"

std::string ipToString(uint32_t ip)
{
    uint8_t* ptr = reinterpret_cast<uint8_t*>(&ip);
    
    return std::to_string(static_cast<uint32_t>(ptr[0])) + "." +
    std::to_string(static_cast<uint32_t>(ptr[1])) + "." +
    std::to_string(static_cast<uint32_t>(ptr[2])) + "." +
    std::to_string(static_cast<uint32_t>(ptr[3]));
}
