//
//  rtmp_relay
//

#include <iostream>
#include <stdint.h>
#include "Amf0.h"
#include "Utils.h"

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

            if (buffer.size() - offset < 1)
            {
                return false;
            }

            Marker marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);

            if (marker == Marker::ObjectEnd)
            {
                offset += 1;
                std::cout << "Object end" << std::endl;
            }
            else
            {
                Node node;

                std::cout << "Value: " << std::endl;

                uint32_t ret = node.parseBuffer(buffer, offset);

                if (ret == 0)
                {
                    return false;
                }
                offset += ret;

                result[key] = node;
            }
        }

        return false;
    }

    static bool readNull(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        UNUSED(buffer);
        UNUSED(offset);

        std::cout << "Null" << std::endl;

        return true;
    }

    static bool readUndefined(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        UNUSED(buffer);
        UNUSED(offset);

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
        UNUSED(buffer);
        UNUSED(offset);

        std::cerr << "Typed objects are not supported" << std::endl;

        return true;
    }
    
    static bool readSwitchToAMF3(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        UNUSED(buffer);
        UNUSED(offset);

        std::cerr << "AMF3 is not supported" << std::endl;
        
        return true;
    }

    uint32_t Node::parseBuffer(const std::vector<uint8_t>& buffer, uint32_t offset)
    {
        uint32_t originalOffset = offset;

        if (buffer.size() - offset < 1)
        {
            return false;
        }

        _marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);
        offset += 1;

        std::cout << "Marker: " << markerToString(_marker) << std::endl;

        switch (_marker)
        {
            case Marker::Number: if (!readNumber(buffer, offset, _doubleValue)) return 0; break;
            case Marker::Boolean: if (!readBoolean(buffer, offset, _boolValue)) return 0; break;
            case Marker::String: if (!readString(buffer, offset, _stringValue)) return 0; break;
            case Marker::Object: if (!readObject(buffer, offset, _mapValue)) return 0; break;
            case Marker::Null: if (!readNull(buffer, offset)) return 0; break;
            case Marker::Undefined: if (!readUndefined(buffer, offset)) return 0; break;
            case Marker::ECMAArray: if (!readECMAArray(buffer, offset, _mapValue)) return 0; break;
            case Marker::ObjectEnd: break; // should not happen
            case Marker::StrictArray: if (!readStrictArray(buffer, offset, _vectorValue)) return 0; break;
            case Marker::Date: if (!readDate(buffer, offset, _dateValue)) return 0; break;
            case Marker::LongString: if (!readLongString(buffer, offset, _stringValue)) return 0; break;
            case Marker::XMLDocument: if (!readXMLDocument(buffer, offset, _stringValue)) return 0; break;
            case Marker::TypedObject: if (!readTypedObject(buffer, offset)) return 0; break;
            case Marker::SwitchToAMF3: if (!readSwitchToAMF3(buffer, offset)) return 0; break;
            default: return 0;
        }

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

    Node Node::operator[](size_t key) const
    {
        if (key >= _vectorValue.size())
        {
            return Node();
        }
        else
        {
            return _vectorValue[key];
        }
    }

    Node Node::operator[](const std::string& key) const
    {
        std::map<std::string, Node>::const_iterator i = _mapValue.find(key);

        if (i == _mapValue.end())
        {
            return Node();
        }
        else
        {
            return i->second;
        }
    }
}
