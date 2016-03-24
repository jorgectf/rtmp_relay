//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define UNUSED(x) (void)(x)

std::string ipToString(uint32_t ip);

template <class T>
inline bool readInt(const std::vector<uint8_t>& buffer, uint32_t& offset, uint32_t bytes, T& result)
{
    if (sizeof(T) < bytes ||
        buffer.size() - offset < bytes)
    {
        return false;
    }

    result = 0;

    for (uint32_t i = 0; i < bytes; ++i)
    {
        result <<= 1;
        result += static_cast<uint16_t>(*(buffer.data() + offset));
        offset += 1;
    }

    return bytes;
}

inline bool readDouble(const std::vector<uint8_t>& buffer, uint32_t& offset, double& result)
{
    if (buffer.size() - offset < sizeof(double))
    {
        return false;
    }

    uint64_t intValue = 0;

    if (!readInt(buffer, offset, sizeof(double), intValue))
    {
        return false;
    }

    result = *reinterpret_cast<double*>(&result);

    return true;
}
