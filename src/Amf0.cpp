//
//  rtmp_relay
//

#include <iostream>
#include <stdint.h>
#include "Amf0.h"

namespace amf0
{
    enum HeaderType
    {
        TWELVE_BYTE_HEADER = 0x00, // 00
        EIGHT_BYTE_HEADER = 0x01, // 01
        FOUR_BYTE_HEADER = 0x02, // 10
        ONE_BYTE_HEADER = 0x03 // 11
    };

    static bool parseNode(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseNumber(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseBoolean(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseString(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseObject(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseNull(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseUndefined(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseECMAArray(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseStrictArray(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseDate(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseLongString(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseXMLDocument(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseTypedObject(const uint8_t* buffer, uint32_t size, uint32_t& offset);
    static bool parseSwitchToAMF3(const uint8_t* buffer, uint32_t size, uint32_t& offset);

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
        uint32_t result = 0;

        for (uint32_t i = 0; i < 8; ++i)
        {
            result <<= 1;
            result += static_cast<uint64_t>(*(buffer + offset));
            offset += 1;
        }

        return *reinterpret_cast<double*>(&result);
    }

    static bool parseHeader(const uint8_t* buffer, uint32_t size, uint32_t& offset)
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

    static const Marker* parseMarker(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 1)
        {
            return nullptr;
        }

        const Marker* marker = reinterpret_cast<const Marker*>(buffer + offset);
        offset += sizeof(*marker);

        return marker;
    }

    static bool parseNode(const uint8_t* buffer, uint32_t size, uint32_t& offset)
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

    static bool parseNumber(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 8)
        {
            return false;
        }

        double number = readDouble(buffer, offset);

        std::cout << "Number: " << number << std::endl;

        return true;
    }

    static bool parseBoolean(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 1)
        {
            return false;
        }

        uint8_t result = *(buffer + offset);

        std::cout << "Bool: " << static_cast<uint32_t>(result) << std::endl;

        return true;
    }

    static bool parseString(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 2)
        {
            return false;
        }

        uint32_t length = readInt(buffer, 2, offset);

        if (size - offset < length)
        {
            return false;
        }

        std::cout << "Length: " << length << std::endl;

        char* str = (char*)malloc(length + 1);
        memcpy(str, buffer + offset, length);
        str[length] = '\0';

        offset += length;

        std::cout << "String: " << str << std::endl;

        free(str);

        return true;
    }

    static bool parseObject(const uint8_t* buffer, uint32_t size, uint32_t& offset)
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

    static bool parseNull(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        std::cout << "Null" << std::endl;

        return true;
    }

    static bool parseUndefined(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        std::cout << "Undefined" << std::endl;

        return true;
    }

    static bool parseECMAArray(const uint8_t* buffer, uint32_t size, uint32_t& offset)
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

    static bool parseStrictArray(const uint8_t* buffer, uint32_t size, uint32_t& offset)
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
    
    static bool parseDate(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        double ms = readDouble(buffer, offset); // date in milliseconds from 01/01/1970
        uint32_t timezone = readInt(buffer, 4, offset); // unsupported timezone
        
        std::cout << "Milliseconds: " << ms << ", timezone: " << timezone << std::endl;
        
        return true;
    }
    
    static bool parseLongString(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 4)
        {
            return false;
        }

        uint32_t length = readInt(buffer, 4, offset);

        if (size - offset < length)
        {
            return false;
        }

        std::cout << "Length: " << length << std::endl;
        
        char* str = static_cast<char*>(malloc(length + 1));
        memcpy(str, buffer + offset, length);
        str[length] = '\0';
        
        offset += length;
        
        std::cout << "String: " << str << std::endl;
        
        free(str);
        
        return true;
    }
    
    static bool parseXMLDocument(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        if (size - offset < 4)
        {
            return false;
        }

        uint32_t length = readInt(buffer, 4, offset);

        if (size - offset < length)
        {
            return false;
        }
        
        std::cout << "Length: " << length << std::endl;
        
        char* str = (char*)malloc(length + 1);
        memcpy(str, buffer + offset, length);
        str[length] = '\0';
        
        offset += length;
        
        std::cout << "XML: " << str << std::endl;
        
        return true;
    }
    
    static bool parseTypedObject(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        return true;
    }
    
    static bool parseSwitchToAMF3(const uint8_t* buffer, uint32_t size, uint32_t& offset)
    {
        std::cerr << "AMF3 not supported" << std::endl;
        
        return true;
    }

    Node parseBuffer(const std::vector<uint8_t>& buffer)
    {
        Node result;

        return result;
    }
}
