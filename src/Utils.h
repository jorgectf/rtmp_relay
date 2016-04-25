//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define UNUSED(x) (void)(x)

std::string ipToString(uint32_t ip);


template <uint8_t>
inline uint32_t decodeInt(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, uint8_t& result)
{
    if (buffer.size() - offset < size)
    {
        return 0;
    }

    result = 0;

    for (uint32_t i = 0; i < size; ++i)
    {
        result += static_cast<uint8_t>(*(buffer.data() + offset));
        offset += 1;
    }

    return size;
}

template <class T>
inline uint32_t decodeInt(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, T& result)
{
    if (buffer.size() - offset < size)
    {
        return 0;
    }

    result = 0;

    for (uint32_t i = 0; i < size; ++i)
    {
        result <<= 8;

        result += static_cast<T>(*(buffer.data() + offset));
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
        value <<= 8;
        value += static_cast<uint64_t>(*(buffer.data() + offset));
        offset += 1;
    }

    result = *reinterpret_cast<double*>(&result);

    return 8;
}

template <class T>
inline uint32_t encodeInt(std::vector<uint8_t>& buffer, uint32_t size, T value)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        buffer.push_back(static_cast<uint8_t>(value >> 8 * (size - i - 1)));
    }

    return size;
}

inline uint32_t encodeDouble(std::vector<uint8_t>& buffer, double value)
{
    uint64_t data = *reinterpret_cast<uint64_t*>(&value);

    for (uint32_t i = 0; i < 8; ++i)
    {
        buffer.push_back(static_cast<uint8_t>(data >> 8 * (sizeof(value) - i - 1)));
    }
    
    return 8;
}
