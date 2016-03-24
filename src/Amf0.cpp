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

    static bool readNumber(const std::vector<uint8_t>& buffer, uint32_t& offset, double& result)
    {
        if (!readDouble(buffer, offset, result))
        {
            return false;
        }

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

            if (buffer.size() - offset < 1)
            {
                return false;
            }

            Marker marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);

            if (marker == Marker::ObjectEnd)
            {
                offset += 1;
            }
            else
            {
                Node node;

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

        return true;
    }

    static bool readUndefined(const std::vector<uint8_t>& buffer, uint32_t& offset)
    {
        UNUSED(buffer);
        UNUSED(offset);

        return true;
    }

    static bool readECMAArray(const std::vector<uint8_t>& buffer, uint32_t& offset, std::map<std::string, Node>& result)
    {
        uint32_t count;

        if (!readInt(buffer, offset, 4, count))
        {
            return false;
        }

        for (uint32_t i = 0; i < count; ++i)
        {
            std::string key;

            if (!readString(buffer, offset, key))
            {
                return false;
            }

            Node node;

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

        for (uint32_t i = 0; i < count; ++i)
        {
            Node node;

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
