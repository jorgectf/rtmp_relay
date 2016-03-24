//
//  rtmp_relay
//

#include <iostream>
#include <cstdint>
#include "Amf0.h"
#include "Utils.h"

namespace amf0
{
    std::string markerToString(Marker marker)
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

    static uint32_t readNumber(const std::vector<uint8_t>& buffer, uint32_t offset, double& result)
    {
        uint32_t originalOffset = offset;

        uint32_t ret = decodeDouble(buffer, offset, result);

        if (ret == 0)
        {
            return 0;
        }

        offset += ret;

        return offset - originalOffset;
    }

    static uint32_t readBoolean(const std::vector<uint8_t>& buffer, uint32_t offset, bool& result)
    {
        uint32_t originalOffset = offset;

        if (buffer.size() - offset < 1)
        {
            return 0;
        }

        result = static_cast<bool>(*(buffer.data() + offset));
        offset += 1;

        return offset - originalOffset;
    }

    static uint32_t readString(const std::vector<uint8_t>& buffer, uint32_t offset, std::string& result)
    {
        uint32_t originalOffset = offset;

        uint16_t length;

        uint32_t ret = decodeInt(buffer, offset, 2, length);

        if (ret == 0)
        {
            return 0;
        }

        offset += ret;

        if (buffer.size() - offset < length)
        {
            return 0;
        }

        result.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
        offset += length;

        return offset - originalOffset;
    }

    static uint32_t readObject(const std::vector<uint8_t>& buffer, uint32_t offset, std::map<std::string, Node>& result)
    {
        uint32_t originalOffset = offset;

        while (buffer.size() - offset > 0)
        {
            std::string key;

            uint32_t ret = readString(buffer, offset, key);

            if (ret == 0)
            {
                return 0;
            }

            offset += ret;

            if (buffer.size() - offset < 1)
            {
                return 0;
            }

            Marker marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);

            if (marker == Marker::ObjectEnd)
            {
                offset += 1;
            }
            else
            {
                Node node;

                ret = node.parseBuffer(buffer, offset);

                if (ret == 0)
                {
                    return 0;
                }
                offset += ret;

                result[key] = node;
            }
        }

        return offset - originalOffset;
    }

    static uint32_t readECMAArray(const std::vector<uint8_t>& buffer, uint32_t offset, std::map<std::string, Node>& result)
    {
        uint32_t originalOffset = offset;

        uint32_t count;

        uint32_t ret = decodeInt(buffer, offset, 4, count);

        if (ret == 0)
        {
            return 0;
        }

        offset += ret;

        for (uint32_t i = 0; i < count; ++i)
        {
            std::string key;

            ret = readString(buffer, offset, key);

            if (ret == 0)
            {
                return 0;
            }

            offset += ret;

            Node node;

            if (!node.parseBuffer(buffer, offset))
            {
                return 0;
            }

            result[key] = node;
        }

        return offset - originalOffset;
    }

    static uint32_t readStrictArray(const std::vector<uint8_t>& buffer, uint32_t offset, std::vector<Node>& result)
    {
        uint32_t originalOffset = offset;

        uint32_t count;

        uint32_t ret = decodeInt(buffer, offset, 4, count);

        if (ret == 0)
        {
            return 0;
        }

        offset += ret;

        for (uint32_t i = 0; i < count; ++i)
        {
            Node node;

            if (!node.parseBuffer(buffer, offset))
            {
                return 0;
            }

            result.push_back(node);
        }

        return offset - originalOffset;
    }

    static uint32_t readDate(const std::vector<uint8_t>& buffer, uint32_t offset, Date& result)
    {
        uint32_t originalOffset = offset;

        uint32_t ret = decodeDouble(buffer, offset, result.ms);

        if (ret == 0) // date in milliseconds from 01/01/1970
        {
            return 0;
        }

        offset += ret;

        ret = decodeInt(buffer, offset, 4, result.timezone);

        if (ret == 0) // unsupported timezone
        {
            return 0;
        }

        offset += ret;

        return offset - originalOffset;
    }

    static uint32_t readLongString(const std::vector<uint8_t>& buffer, uint32_t offset, std::string& result)
    {
        uint32_t originalOffset = offset;

        uint32_t length;

        uint32_t ret = decodeInt(buffer, offset, 4, length);

        if (ret == 0)
        {
            return 0;
        }

        offset += ret;

        if (buffer.size() - offset < length)
        {
            return 0;
        }

        result.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
        offset += length;

        return offset - originalOffset;
    }

    static uint32_t readXMLDocument(const std::vector<uint8_t>& buffer, uint32_t offset, std::string& result)
    {
        uint32_t originalOffset = offset;

        uint32_t length;

        uint32_t ret = decodeInt(buffer, offset, 4, length);

        if (ret == 0)
        {
            return 0;
        }

        offset += ret;

        if (buffer.size() - offset < length)
        {
            return 0;
        }

        result.assign(reinterpret_cast<const char*>(buffer.data() + offset), length);
        offset += length;

        return offset - originalOffset;
    }
    
    static uint32_t readTypedObject(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        UNUSED(buffer);
        UNUSED(offset);

        std::cerr << "Typed objects are not supported" << std::endl;

        return 0;
    }
    
    static uint32_t readSwitchToAMF3(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        UNUSED(buffer);
        UNUSED(offset);

        std::cerr << "AMF3 is not supported" << std::endl;
        
        return 0;
    }

    uint32_t Node::parseBuffer(const std::vector<uint8_t>& buffer, uint32_t offset)
    {
        uint32_t originalOffset = offset;

        if (buffer.size() - offset < 1)
        {
            return 0;
        }

        _marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);
        offset += 1;

        uint32_t ret = 0;

        switch (_marker)
        {
            case Marker::Number: ret = readNumber(buffer, offset, _doubleValue); break;
            case Marker::Boolean: ret = readBoolean(buffer, offset, _boolValue); break;
            case Marker::String: ret = readString(buffer, offset, _stringValue); break;
            case Marker::Object: ret = readObject(buffer, offset, _mapValue); break;
            case Marker::Null: /* Null */; break;
            case Marker::Undefined: /* Undefined */; break;
            case Marker::ECMAArray: ret = readECMAArray(buffer, offset, _mapValue); break;
            case Marker::ObjectEnd: break; // should not happen
            case Marker::StrictArray: ret = readStrictArray(buffer, offset, _vectorValue); break;
            case Marker::Date: ret = readDate(buffer, offset, _dateValue); break;
            case Marker::LongString: ret = readLongString(buffer, offset, _stringValue); break;
            case Marker::XMLDocument: ret = readXMLDocument(buffer, offset, _stringValue); break;
            case Marker::TypedObject: ret = readTypedObject(buffer, offset); break;
            case Marker::SwitchToAMF3: ret = readSwitchToAMF3(buffer, offset); break;
            default: return 0;
        }

        offset += ret;

        return offset - originalOffset;
    }

    double Node::asDouble() const
    {
        return _doubleValue;
    }

    bool Node::asBool() const
    {
        return _boolValue;
    }

    const std::string& Node::asString() const
    {
        return _stringValue;
    }

    const Date& Node::asDate() const
    {
        return _dateValue;
    }

    bool Node::isNull() const
    {
        return _marker == Marker::Null;
    }

    bool Node::isUndefined() const
    {
        return _marker == Marker::Undefined;
    }

    uint32_t Node::getSize() const
    {
        return static_cast<uint32_t>(_vectorValue.size());
    }

    Node& Node::operator[](size_t key)
    {
        return _vectorValue[key];
    }

    Node& Node::operator[](const std::string& key)
    {
        return _mapValue[key];
    }
}
