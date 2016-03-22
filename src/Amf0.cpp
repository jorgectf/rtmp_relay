//
//  rtmp_relay
//

#include <iostream>
#include <stdint.h>
#include "Amf0.h"

namespace amf0
{
    enum class Marker: uint8_t
    {
        Number = 0x00,
        Boolean = 0x01,
        String = 0x02,
        Object = 0x03,
        Null = 0x05,
        Undefined = 0x06,
        ECMAArray = 0x08,
        ObjectEnd = 0x09,
        StrictArray = 0x0a,
        Date = 0x0b,
        LongString = 0x0c,
        XMLDocument = 0x0f,
        TypedObject = 0x10,
        SwitchToAMF3 = 0x11
    };

    enum HeaderType
    {
        TWELVE_BYTE_HEADER = 0x00, // 00
        EIGHT_BYTE_HEADER = 0x01, // 01
        FOUR_BYTE_HEADER = 0x02, // 10
        ONE_BYTE_HEADER = 0x03 // 11
    };

    static uint16_t swap16(uint16_t a)
    {
        a = static_cast<uint16_t>(((a & 0x00FF) << 8) |
                                  ((a & 0xFF00) >> 8));
        return a;
    }

    static uint32_t swap32(uint32_t a)
    {
        a = ((a & 0x000000FF) << 24) |
            ((a & 0x0000FF00) <<  8) |
            ((a & 0x00FF0000) >>  8) |
            ((a & 0xFF000000) >> 24);
        return a;
    }

    static uint64_t swap64(uint64_t a)
    {
        a = ((a & 0x00000000000000FFULL) << 56) |
            ((a & 0x000000000000FF00ULL) << 40) |
            ((a & 0x0000000000FF0000ULL) << 24) |
            ((a & 0x00000000FF000000ULL) <<  8) |
            ((a & 0x000000FF00000000ULL) >>  8) |
            ((a & 0x0000FF0000000000ULL) >> 24) |
            ((a & 0x00FF000000000000ULL) >> 40) |
            ((a & 0xFF00000000000000ULL) >> 56);
        return a;
    }

    bool parseNode(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseNumber(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseBoolean(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseString(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseObject(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseNull(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseUndefined(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseECMAArray(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseStrictArray(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseDate(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseLongString(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseXMLDocument(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseTypedObject(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    bool parseSwitchToAMF3(const uint8_t* buffer, uint32_t size, uint32_t& offset);

    static const char* markerToString(Marker marker)
    {
        switch (marker)
        {
            case Marker::Number: return "Number";
            case Marker::Boolean: return "Boolean";
            case Marker::String: return "String";
            case Marker::Object: return "Object";
            case Marker::Null: return "Null";
            case Marker::Undefined: return "Undefined";
            case Marker::ECMAArray: return "ECMAArray";
            case Marker::ObjectEnd: return "ObjectEnd";
            case Marker::StrictArray: return "StrictArray";
            case Marker::Date: return "Date";
            case Marker::LongString: return "LongString";
            case Marker::XMLDocument: return "XMLDocument";
            case Marker::TypedObject: return "TypedObject";
            case Marker::SwitchToAMF3: return "SwitchToAMF3";
            default: return "Unknown";
        }
    }

    static uint32_t readInt(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        uint32_t result = 0;

        for (uint32_t i = 0; i < size; ++i)
        {
            result <<= 1;
            result += static_cast<uint32_t>(*(buffer + offset));
            offset += 1;
        }

        return result;
    }

    static double readDouble(const uint8_t* buffer, uint32_t& offset)
    {
        uint64_t swapped = swap64(*(uint64_t*)(buffer + offset));
        double result = *(double*)(&swapped);
        offset += sizeof(result);

        return result;
    }

    bool parseHeader(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 1)
        {
            return false;
        }

        uint8_t header = *(buffer + offset);
        offset += 1;

        if ((header & 0xFF) != 0x03)
        {
            std::cerr << "Wrong header version\n";
            return false;
        }

        uint8_t headerType = header >> 6;

        if (headerType != ONE_BYTE_HEADER)
        {
            uint32_t timestamp = readInt(buffer, 3, offset);
            std::cout << "Timestamp: " << timestamp << std::endl;

            if (headerType != FOUR_BYTE_HEADER)
            {
                uint32_t length = readInt(buffer, 3, offset);
                std::cout << "Length: " << length << std::endl;

                uint8_t messageTypeId = *(buffer + offset);
                offset++;
                std::cout << "Message type ID: " << static_cast<uint32_t>(messageTypeId) << std::endl;

                if (headerType != EIGHT_BYTE_HEADER)
                {
                    uint32_t messageStreamId = *reinterpret_cast<const uint32_t*>(buffer + offset);
                    offset += sizeof(messageStreamId);

                    std::cout << "Message stream ID: " << messageStreamId << std::endl;
                }
            }
        }

        return true;
    }

    const Marker* parseMarker(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 1)
        {
            return nullptr;
        }

        const Marker* marker = reinterpret_cast<const Marker*>(buffer + offset);
        offset += sizeof(*marker);

        return marker;
    }

    bool parseNode(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        const Marker* marker = parseMarker(buffer, size, offset);

        if (!marker)
        {
            return false;
        }

        std::cout << "Marker: " << markerToString(*marker) << std::endl;

        switch (*marker)
        {
            case Marker::Number: return parseNumber(buffer, size, offset);
            case Marker::Boolean: return parseBoolean(buffer, size, offset);
            case Marker::String: return parseString(buffer, size, offset);
            case Marker::Object: return parseObject(buffer, size, offset);
            case Marker::Null: return parseNull(buffer, size, offset);
            case Marker::Undefined: return parseUndefined(buffer, size, offset);
            case Marker::ECMAArray: return parseECMAArray(buffer, size, offset);
            case Marker::ObjectEnd: return false; // should not happen
            case Marker::StrictArray: return parseStrictArray(buffer, size, offset);
            case Marker::Date: return parseDate(buffer, size, offset);
            case Marker::LongString: return parseLongString(buffer, size, offset);
            case Marker::XMLDocument: return parseXMLDocument(buffer, size, offset);
            case Marker::TypedObject: return parseTypedObject(buffer, size, offset);
            case Marker::SwitchToAMF3: return parseSwitchToAMF3(buffer, size, offset);
            default: return false;
        }
    }

    bool parseNumber(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 8)
        {
            return false;
        }

        double number = readDouble(buffer, offset);

        std::cout << "Number: " << number << std::endl;

        return true;
    }

    bool parseBoolean(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 1)
        {
            return false;
        }

        uint8_t result = *(buffer + offset);

        std::cout << "Bool: " << static_cast<uint32_t>(result) << std::endl;

        return true;
    }

    bool parseString(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        uint32_t length = readInt(buffer, 2, offset);
        std::cout << "Length: " << length << std::endl;

        char* str = (char*)malloc(length + 1);
        memcpy(str, buffer + offset, length);
        str[length] = '\0';

        offset += length;

        std::cout << "String: " << str << std::endl;

        free(str);

        return true;
    }

    bool parseObject(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        while (size - offset > 0)
        {
            std::cout << "Key: " << std::endl;
            if (!parseString(buffer, size, offset))
            {
                return false;
            }

            Marker marker = *(Marker*)(buffer + offset);

            if (marker == Marker::ObjectEnd)
            {
                offset++;
                std::cout << "Object end" << std::endl;
            }
            else
            {
                std::cout << "Value: " << std::endl;
                if (!parseNode(buffer, size, offset))
                {
                    return false;
                }
            }
        }

        return false;
    }

    bool parseNull(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        std::cout << "Null" << std::endl;

        return true;
    }

    bool parseUndefined(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        std::cout << "Undefined" << std::endl;

        return true;
    }

    bool parseECMAArray(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 4)
        {
            return false;
        }

        uint32_t count = readInt(buffer, 4, offset);

        std::cout << "ECMA array, size: " << count << std::endl;

        for (uint32_t i = 0; i < count; ++i)
        {
            std::cout << "Key: " << std::endl;
            if (!parseString(buffer, size, offset))
            {
                return false;
            }

            std::cout << "Value: " << std::endl;
            if (!parseNode(buffer, size, offset))
            {
                return false;
            }
        }

        return true;
    }

    bool parseStrictArray(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {

        if (size - offset < 4)
        {
            return false;
        }

        uint32_t count = readInt(buffer, 4, offset);

        std::cout << "Strict array, size: " << count << std::endl;

        for (uint32_t i = 0; i < count; ++i)
        {
            std::cout << "Value: " << std::endl;
            if (!parseNode(buffer, size, offset))
            {
                return false;
            }
        }
        
        return true;
    }
    
    bool parseDate(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        double ms = readDouble(buffer, offset); // date in milliseconds from 01/01/1970
        uint32_t timezone = readInt(buffer, 4, offset); // unsupported timezone
        
        std::cout << "Milliseconds: " << ms << ", timezone: " << timezone << std::endl;
        
        return true;
    }
    
    bool parseLongString(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        uint32_t length = readInt(buffer, 4, offset);
        std::cout << "Length: " << length << std::endl;
        
        char* str = static_cast<char*>(malloc(length + 1));
        memcpy(str, buffer + offset, length);
        str[length] = '\0';
        
        offset += length;
        
        std::cout << "String: " << str << std::endl;
        
        free(str);
        
        return true;
    }
    
    bool parseXMLDocument(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        uint32_t length = readInt(buffer, 4, offset);
        std::cout << "Length: " << length << std::endl;
        
        char* str = (char*)malloc(length + 1);
        memcpy(str, buffer + offset, length);
        str[length] = '\0';
        
        offset += length;
        
        std::cout << "XML: " << str << std::endl;
        
        return true;
    }
    
    bool parseTypedObject(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        return true;
    }
    
    bool parseSwitchToAMF3(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        std::cerr << "AMF3 not supported" << std::endl;
        
        return true;
    }
}
