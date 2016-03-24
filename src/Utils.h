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
inline uint32_t decodeInt(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, T& value)
{
    if (buffer.size() - offset < size)
    {
        return 0;
    }

    value = 0;

    for (uint32_t i = 0; i < size; ++i)
    {
        value <<= 1;
        value += static_cast<T>(*(buffer.data() + offset));
        offset += 1;
    }

    return size;
}

inline uint32_t decodeDouble(const std::vector<uint8_t>& buffer, uint32_t offset, double& result)
{
    if (buffer.size() - offset < 8)
    {
        return 0;
    }

    uint64_t value = 0;

    for (uint32_t i = 0; i < 8; ++i)
    {
        value <<= 1;
        value += static_cast<uint64_t>(*(buffer.data() + offset));
        offset += 1;
    }

    result = *reinterpret_cast<double*>(&result);

    return 8;
}
