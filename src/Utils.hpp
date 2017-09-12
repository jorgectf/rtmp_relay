//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>

union IntFloat64
{
    uint64_t i;
    double   f;
};

template <class T>
inline uint32_t decodeIntBE(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, T& result)
{
    if (buffer.size() - offset < size)
    {
        return 0;
    }

    result = 0;

    for (uint32_t i = 0; i < size; ++i)
    {
        result += static_cast<T>(*(buffer.data() + offset)) << 8 * (size - i - 1);
        offset += 1;
    }

    return size;
}

template <>
inline uint32_t decodeIntBE<uint8_t>(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, uint8_t& result)
{
    if (buffer.size() - offset < size)
    {
        return 0;
    }

    result = static_cast<uint8_t>(*(buffer.data() + offset));
    offset += 1;

    return size;
}

template <class T>
inline uint32_t decodeIntLE(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, T& result)
{
    if (buffer.size() - offset < size)
    {
        return 0;
    }

    result = 0;

    for (uint32_t i = 0; i < size; ++i)
    {
        result += static_cast<T>(*(buffer.data() + offset)) << 8 * i;
        offset += 1;
    }

    return size;
}

template <>
inline uint32_t decodeIntLE<uint8_t>(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t size, uint8_t& result)
{
    if (buffer.size() - offset < size)
    {
        return 0;
    }

    result = *(buffer.data() + offset);
    offset += 1;
    
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
        value += *(buffer.data() + offset);
        offset += 1;
    }

    IntFloat64 intFloat64;
    intFloat64.i = value;

    result = intFloat64.f;

    return sizeof(double);
}

inline uint32_t decodeU29(const std::vector<uint8_t>& buffer, uint32_t offset, uint32_t& result)
{
    uint32_t originalOffset = offset;

    result = 0;

    for (uint32_t i = 0; i < 4; ++i)
    {
        uint8_t b = *(buffer.data() + offset);

        if (i == 3)
        {
            result <<= 8;
            result += b;
        }
        else
        {
            result <<= 7;
            result += b & 0x7F; // zero the first bit
        }

        offset += 1;

        if (!(b & (1 << 7))) break;
    }

    return offset - originalOffset;
}

template <class T>
inline uint32_t encodeIntBE(std::vector<uint8_t>& buffer, uint32_t size, T value)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        buffer.push_back(static_cast<uint8_t>(value >> 8 * (size - i - 1)));
    }

    return size;
}

template <class T>
inline uint32_t encodeIntLE(std::vector<uint8_t>& buffer, uint32_t size, T value)
{
    for (uint32_t i = 0; i < size; ++i)
    {
        buffer.push_back(static_cast<uint8_t>(value >> 8 * i));
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

inline uint32_t encodeU29(std::vector<uint8_t>& buffer, uint32_t value)
{
    if (value <= 0x7F) // one byte
    {
        buffer.push_back(static_cast<uint8_t>(value));
        return 1;
    }
    else if (value <= 0x3FFF) // two bytes
    {
        buffer.push_back(static_cast<uint8_t>((value >> 7) | 0x80));
        buffer.push_back(static_cast<uint8_t>(value & 0x7F));
        return 2;
    }
    else if (value <= 0x1FFFFF) // three bytes
    {
        buffer.push_back(static_cast<uint8_t>((value >> 14) | 0x80));
        buffer.push_back(static_cast<uint8_t>((value >> 7) | 0x80));
        buffer.push_back(static_cast<uint8_t>(value & 0x7F));
        return 3;
    }
    else if (value <= 0x1FFFFFFF) // four bytes
    {
        buffer.push_back(static_cast<uint8_t>((value >> 22) | 0x80));
        buffer.push_back(static_cast<uint8_t>((value >> 15) | 0x80));
        buffer.push_back(static_cast<uint8_t>((value >> 8) | 0x80));
        buffer.push_back(static_cast<uint8_t>(value));
        return 4;
    }
    else
    {
        return 0;
    }
}

size_t replaceTokens(std::string& str, const std::map<std::string, std::string>& tokens);

inline void tokenize(const std::string& str, std::vector<std::string>& tokens,
                     const std::string& delimiters = " ",
                     bool trimEmpty = false)
{
    std::string::size_type pos, lastPos = 0, length = str.length();

    while (lastPos < length + 1)
    {
        pos = str.find_first_of(delimiters, lastPos);
        if (pos == std::string::npos) pos = length;

        if (pos != lastPos || !trimEmpty)
        {
            tokens.push_back(std::string(str.data() + lastPos,
                                         static_cast<std::vector<std::string>::size_type>(pos) - lastPos));
        }
        
        lastPos = pos + 1;
    }
}

inline std::string escapeString(const std::string& str)
{
    std::map<char, std::string> replace = {
        {'\b', "\\b"},
        {'\f', "\\f"},
        {'\n', "\\n"},
        {'\r', "\\r"},
        {'"', "\\\""},
        {'\\', "\\\\"}
    };

    std::string result;

    for (char c : str)
    {
        std::map<char, std::string>::iterator i = replace.find(c);

        if (i == replace.end())
        {
            result.push_back(c);
        }
        else
        {
            result += i->second;
        }
    }

    return result;
}

enum class AudioCodec
{
    ADPCM            = 1,
    MP3              = 2,
    LINEAR_LE        = 3,
    NELLY16          = 4,
    NELLY8           = 5,
    NELLY            = 6,
    G711A            = 7,
    G711U            = 8,
    AAC              = 10,
    SPEEX            = 11,
    MP3_8            = 14,
    DEVSPEC          = 15,
    UNCOMPRESSED     = 16,
};

enum class VideoCodec
{
    JPEG             = 1,
    SORENSON_H263    = 2,
    SCREEN           = 3,
    ON2_VP6          = 4,
    ON2_VP6_ALPHA    = 5,
    SCREEN2          = 6,
    H264             = 7
};

std::string getAudioCodec(AudioCodec codecId);
std::string getVideoCodec(VideoCodec codecId);

enum class VideoFrameType
{
    NONE = 0,
    KEY = 1,
    INTER = 2,
    DISPOSABLE = 3,
    GENERATED_KEY = 4,
    VIDEO_INFO = 5
};

inline VideoFrameType getVideoFrameType(const std::vector<uint8_t>& data)
{
    if (data.empty()) return VideoFrameType::NONE;

    return static_cast<VideoFrameType>((data[0] & 0xf0) >> 4);
}

inline bool isCodecHeader(const std::vector<uint8_t>& data)
{
    return data.size() >= 2 && data[1] == 0;
}

inline bool isAlphaNumericUnderscore(const std::string& str)
{
    for (char c : str)
    {
        if (!isalnum(c) && c != '_')
        {
            return false;
        }
    }

    return true;
}
