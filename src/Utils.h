//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#define UNUSED(x) (void)(x)

union IntFloat64 {
    uint64_t i;
    double   f;
};

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

template <>
inline uint32_t decodeInt<uint8_t>(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, uint8_t& result)
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

inline uint32_t decodeDouble(const std::vector<uint8_t>& buffer, uint32_t offset, double& result)
{
    if (buffer.size() - offset < 8)
    {
        return 0;
    }

    uint64_t value = 0;

    for (uint32_t i = 0; i < sizeof(double); ++i)
    {
        value <<= 8;
        value += static_cast<uint64_t>(*(buffer.data() + offset));
        offset += 1;
    }

    IntFloat64 intFloat64;
    intFloat64.i = value;

    result = intFloat64.f;

    return sizeof(double);
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
    IntFloat64 intFloat64;
    intFloat64.f = value;

    uint64_t data = intFloat64.i;

    for (uint32_t i = 0; i < sizeof(double); ++i)
    {
        buffer.push_back(static_cast<uint8_t>(data >> 8 * (sizeof(value) - i - 1)));
    }
    
    return sizeof(double);
}

enum LogLevel
{
    LOG_LEVEL_ERROR,
    LOG_LEVEL_WARNING,
    LOG_LEVEL_INFO,
    LOG_LEVEL_VERBOSE
};

void log(LogLevel level, const char* format, ...);
