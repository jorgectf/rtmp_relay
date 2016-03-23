//
//  rtmp_relay
//

#include <iostream>
#include <stdint.h>
#include "Amf0.h"

namespace amf0
{
    static std::string markerToString(Marker marker)
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

    template <class T>
    static bool readInt(const std::vector<uint8_t>& buffer, uint32_t& offset, uint32_t bytes, T& result)
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

    static bool readDouble(const std::vector<uint8_t>& buffer, uint32_t& offset, double& result)
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

    static bool readNumber(const std::vector<uint8_t>& buffer, uint32_t& offset, double& result)
    {
        if (!readDouble(buffer, offset, result))
        {
            return false;
        }

        std::cout << "Number: " << result << std::endl;

        return true;
    }

    static bool readBoolean(const std::vector<uint8_t>& buffer, uint32_t& offset, bool& result)
    {
        if (buffer.size() - offset < 1)
        {
            return false;
        }

        result = static_cast<bool>(*(buffer.data() + offset));
        offset += 1;

        std::cout << "Bool: " << result << std::endl;

        return true;
    }

    static bool readString(const std::vector<uint8_t>& buffer, uint32_t& offset, std::string& result)
    {
        uint16_t length;

        if (!readInt(buffer, offset, 2, length))
        {
            return false;
        }

        if (buffer.size() - offset < length)
        {
            return false;
        }

        result.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
        offset += length;

        std::cout << "String: " << result << ", length: " << length << std::endl;

        return true;
    }

    static bool readObject(const std::vector<uint8_t>& buffer, uint32_t& offset, std::map<std::string, Node>& result)
    {
        while (buffer.size() - offset > 0)
        {
            std::string key;

            if (!readString(buffer, offset, key))
            {
                return false;
            }

            std::cout << "Key: " << key << std::endl;

            Marker marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);

            if (marker == Marker::ObjectEnd)
            {
                offset++;
                std::cout << "Object end" << std::endl;
            }
            else
            {
                Node node;

                std::cout << "Value: " << std::endl;
                if (!node.parseBuffer(buffer, offset))
                {
                    return false;
                }

                result[key] = node;
            }
        }

        return false;
    }

    static bool readNull(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        std::cout << "Null" << std::endl;

        return true;
    }

    static bool readUndefined(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        std::cout << "Undefined" << std::endl;

        return true;
    }

    static bool readECMAArray(const std::vector<uint8_t>& buffer, uint32_t& offset, std::map<std::string, Node>& result)
    {
        uint32_t count;

        if (!readInt(buffer, offset, 4, count))
        {
            return false;
        }

        std::cout << "ECMA array, size: " << count << std::endl;

        for (uint32_t i = 0; i < count; ++i)
        {
            std::string key;

            if (!readString(buffer, offset, key))
            {
                return false;
            }

            std::cout << "Key: " << key << std::endl;

            Node node;

            std::cout << "Value: " << std::endl;
            if (!node.parseBuffer(buffer, offset))
            {
                return false;
            }

            result[key] = node;
        }

        return true;
    }

    static bool readStrictArray(const std::vector<uint8_t>& buffer, uint32_t& offset, std::vector<Node>& result)
    {
        uint32_t count;

        if (!readInt(buffer, offset, 4, count))
        {
            return false;
        }

        std::cout << "Strict array, size: " << count << std::endl;

        for (uint32_t i = 0; i < count; ++i)
        {
            Node node;

            std::cout << "Value: " << std::endl;
            if (!node.parseBuffer(buffer, offset))
            {
                return false;
            }

            result.push_back(node);
        }

        return true;
    }

    static bool readDate(const std::vector<uint8_t>& buffer, uint32_t& offset, Date& result)
    {
        if (!readDouble(buffer, offset, result.ms)) // date in milliseconds from 01/01/1970
        {
            return false;
        }

        if (!readInt(buffer, offset, 4, result.timezone)) // unsupported timezone
        {
            return false;
        }

        std::cout << "Milliseconds: " << result.ms << ", timezone: " << result.timezone << std::endl;

        return true;
    }

    static bool readLongString(const std::vector<uint8_t>& buffer, uint32_t& offset, std::string& result)
    {
        uint32_t length;

        if (!readInt(buffer, offset, 4, length))
        {
            return false;
        }

        if (buffer.size() - offset < length)
        {
            return false;
        }

        result.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
        offset += length;

        std::cout << "String: " << result << ", length: " << length << std::endl;

        return true;
    }

    static bool readXMLDocument(const std::vector<uint8_t>& buffer, uint32_t& offset, std::string& result)
    {
        uint32_t length;

        if (!readInt(buffer, offset, 4, length))
        {
            return false;
        }

        if (buffer.size() - offset < length)
        {
            return false;
        }

        result.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
        offset += length;

        std::cout << "XML: " << result << ", length: " << length << std::endl;

        return true;
    }
    
    static bool readTypedObject(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        return true;
    }
    
    static bool readSwitchToAMF3(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        std::cerr << "AMF3 not supported" << std::endl;
        
        return true;
    }

    bool Node::parseBuffer(const std::vector<uint8_t>& buffer, uint32_t offset)
    {
        if (buffer.size() - offset < 1)
        {
            return false;
        }

        _marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);
        offset += 1;

        std::cout << "Marker: " << markerToString(_marker) << std::endl;

        switch (_marker)
        {
            case Marker::Number: return readNumber(buffer, offset, _doubleValue);
            case Marker::Boolean: return readBoolean(buffer, offset, _boolValue);
            case Marker::String: return readString(buffer, offset, _stringValue);
            case Marker::Object: return readObject(buffer, offset, _mapValue);
            case Marker::Null: return readNull(buffer, offset);
            case Marker::Undefined: return readUndefined(buffer, offset);
            case Marker::ECMAArray: return readECMAArray(buffer, offset, _mapValue);
            case Marker::ObjectEnd: return false; // should not happen
            case Marker::StrictArray: return readStrictArray(buffer, offset, _vectorValue);
            case Marker::Date: return readDate(buffer, offset, _dateValue);
            case Marker::LongString: return readLongString(buffer, offset, _stringValue);
            case Marker::XMLDocument: return readXMLDocument(buffer, offset, _stringValue);
            case Marker::TypedObject: return readTypedObject(buffer, offset);
            case Marker::SwitchToAMF3: return readSwitchToAMF3(buffer, offset);
            default: return false;
        }
    }
}
