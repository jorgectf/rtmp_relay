//
//  rtmp_relay
//

#include <iostream>
#include <cstdint>
#include "Amf0.h"
#include "Utils.h"

namespace amf0
{
    static const std::string INDENT = "  ";

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

    // reading
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

        while (true)
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
                break;
            }
            else
            {
                Node node;

                ret = node.decode(buffer, offset);

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

            if (!node.decode(buffer, offset))
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

            if (!node.decode(buffer, offset))
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

    // writing
    static uint32_t writeNumber(std::vector<uint8_t>& buffer, double value)
    {
        uint32_t ret = encodeDouble(buffer, value);

        if (ret == 0)
        {
            return 0;
        }

        return ret;
    }

    static uint32_t writeBoolean(std::vector<uint8_t>& buffer, bool value)
    {
        buffer.push_back(static_cast<uint8_t>(value));

        return 1;
    }

    static uint32_t writeString(std::vector<uint8_t>& buffer, const std::string& value)
    {
        uint32_t ret = encodeInt(buffer, 2, value.size());

        if (ret == 0)
        {
            return 0;
        }

        uint32_t size = ret;

        for (char i : value)
        {
            buffer.push_back(static_cast<uint8_t>(i));
            size += 1;
        }

        return size;
    }

    static uint32_t writeObject(std::vector<uint8_t>& buffer, const std::map<std::string, Node>& value)
    {
        uint32_t size = 0;
        uint32_t ret;

        for (const auto& i : value)
        {
            ret = writeString(buffer, i.first);

            if (ret == 0)
            {
                return 0;
            }

            size += ret;

            ret = i.second.encode(buffer);

            if (ret == 0)
            {
                return 0;
            }

            size += ret;
        }

        ret = writeString(buffer, "");

        if (ret == 0)
        {
            return 0;
        }

        size += ret;

        Marker marker = Marker::ObjectEnd;
        buffer.push_back(static_cast<uint8_t>(marker));

        size += 1;

        return size;
    }

    static uint32_t writeECMAArray(std::vector<uint8_t>& buffer, const std::map<std::string, Node>& value)
    {
        uint32_t size = 0;

        uint32_t ret = encodeInt(buffer, 4, value.size());

        if (ret == 0)
        {
            return 0;
        }

        size += ret;

        for (const auto& i : value)
        {
            ret = writeString(buffer, i.first);

            if (ret == 0)
            {
                return 0;
            }

            size += ret;

            ret = i.second.encode(buffer);

            if (ret == 0)
            {
                return 0;
            }

            size += ret;
        }

        return size;
    }

    static uint32_t writeStrictArray(std::vector<uint8_t>& buffer, const std::vector<Node>& value)
    {
        uint32_t size = 0;

        uint32_t ret = encodeInt(buffer, 4, value.size());

        if (ret == 0)
        {
            return 0;
        }

        size += ret;

        for (const auto& i : value)
        {
            ret = i.encode(buffer);

            if (ret == 0)
            {
                return 0;
            }

            size += ret;
        }

        return size;
    }

    static uint32_t writeDate(std::vector<uint8_t>& buffer, const Date& value)
    {
        uint32_t size = 0;

        uint32_t ret = encodeDouble(buffer, value.ms);

        if (ret == 0) // date in milliseconds from 01/01/1970
        {
            return 0;
        }

        size += ret;

        ret = encodeInt(buffer, 4, value.timezone);

        if (ret == 0) // unsupported timezone
        {
            return 0;
        }

        size += ret;

        return size;
    }

    static uint32_t writeLongString(std::vector<uint8_t>& buffer, const std::string& value)
    {
        uint32_t ret = encodeInt(buffer, 4, value.size());

        if (ret == 0)
        {
            return 0;
        }

        uint32_t size = ret;

        for (char i : value)
        {
            buffer.push_back(static_cast<uint8_t>(i));
            size += 1;
        }
        
        return size;
    }

    static uint32_t writeXMLDocument(std::vector<uint8_t>& buffer, const std::string& value)
    {
        uint32_t ret = encodeInt(buffer, 4, value.size());

        if (ret == 0)
        {
            return 0;
        }

        uint32_t size = ret;

        for (char i : value)
        {
            buffer.push_back(static_cast<uint8_t>(i));
            size += 1;
        }

        return size;
    }
    
    static uint32_t writeTypedObject(std::vector<uint8_t>& buffer)
    {
        UNUSED(buffer);
        
        std::cerr << "Typed objects are not supported" << std::endl;
        
        return 0;
    }
    
    static uint32_t writeSwitchToAMF3(std::vector<uint8_t>& buffer)
    {
        UNUSED(buffer);
        
        std::cerr << "AMF3 is not supported" << std::endl;
        
        return 0;
    }

    Node::Node()
    {

    }

    Node::Node(Marker marker):
        _marker(marker)
    {

    }

    Node::Node(double value):
        _marker(Marker::Number), _doubleValue(value)
    {
    }

    Node::Node(bool value):
        _marker(Marker::Boolean), _boolValue(value)
    {
    }

    Node::Node(const std::string& value):
        _stringValue(value)
    {
        if (value.length() <= UINT16_MAX)
        {
            _marker = Marker::String;
        }
        else
        {
            _marker = Marker::LongString;
        }
    }

    Node::Node(const Date& value):
        _marker(Marker::Date), _dateValue(value)
    {
    }

    uint32_t Node::decode(const std::vector<uint8_t>& buffer, uint32_t offset)
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

    uint32_t Node::encode(std::vector<uint8_t>& buffer) const
    {
        uint32_t size = 0;

        uint32_t ret = 0;

        buffer.push_back(static_cast<uint8_t>(_marker));
        size += 1;

        switch (_marker)
        {
            case Marker::Number: ret = writeNumber(buffer, _doubleValue); break;
            case Marker::Boolean: ret = writeBoolean(buffer, _boolValue); break;
            case Marker::String: ret = writeString(buffer, _stringValue); break;
            case Marker::Object: ret = writeObject(buffer, _mapValue); break;
            case Marker::Null: /* Null */; break;
            case Marker::Undefined: /* Undefined */; break;
            case Marker::ECMAArray: ret = writeECMAArray(buffer, _mapValue); break;
            case Marker::ObjectEnd: break; // should not happen
            case Marker::StrictArray: ret = writeStrictArray(buffer, _vectorValue); break;
            case Marker::Date: ret = writeDate(buffer, _dateValue); break;
            case Marker::LongString: ret = writeLongString(buffer, _stringValue); break;
            case Marker::XMLDocument: ret = writeXMLDocument(buffer, _stringValue); break;
            case Marker::TypedObject: ret = writeTypedObject(buffer); break;
            case Marker::SwitchToAMF3: ret = writeSwitchToAMF3(buffer); break;
            default: return 0;
        }

        size += ret;

        return size;
    }

    void Node::setDouble(double value)
    {
        _marker = Marker::Number;
        _doubleValue = value;
    }

    double Node::asDouble() const
    {
        return _doubleValue;
    }

    void Node::setBool(bool value)
    {
        _marker = Marker::Boolean;
        _boolValue = value;
    }

    bool Node::asBool() const
    {
        return _boolValue;
    }

    void Node::setString(const std::string& value)
    {
        if (value.length() <= UINT16_MAX)
        {
            _marker = Marker::String;
        }
        else
        {
            _marker = Marker::LongString;
        }

        _stringValue = value;
    }

    const std::string& Node::asString() const
    {
        return _stringValue;
    }

    void Node::setDate(const Date& value)
    {
        _marker = Marker::Date;

        _dateValue = value;
    }

    const Date& Node::asDate() const
    {
        return _dateValue;
    }

    void Node::setNull()
    {
        _marker = Marker::Null;
    }

    bool Node::isNull() const
    {
        return _marker == Marker::Null;
    }

    void Node::setUndefined()
    {
        _marker = Marker::Undefined;
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

    Node& Node::operator[](size_t key)
    {
        _marker = Marker::StrictArray;
        return _vectorValue[key];
    }

    Node Node::operator[](const std::string& key) const
    {
        auto i = _mapValue.find(key);

        if (i == _mapValue.end())
        {
            return Node();
        }
        else
        {
            return i->second;
        }
    }

    Node& Node::operator[](const std::string& key)
    {
        _marker = Marker::Object;
        return _mapValue[key];
    }

    bool Node::hasElement(const std::string& key)
    {
        return _mapValue.find(key) != _mapValue.end();
    }

    void Node::append(const Node& node)
    {
        _vectorValue.push_back(node);
    }

    void Node::dump(const std::string& indent)
    {
        std::cout << "Type: " << markerToString(_marker) << "(" << static_cast<uint32_t>(_marker) << ")";

        if (_marker == Marker::Object ||
            _marker == Marker::StrictArray ||
            _marker == Marker::ECMAArray)
        {
            std::cout << ", values: " << std::endl;

            if (_marker == Marker::StrictArray)
            {
                for (size_t index = 0; index < _vectorValue.size(); index++)
                {
                    std::cout << indent + INDENT << index << ": ";
                    _vectorValue[index].dump(indent + INDENT);
                }
            }
            else
            {
                for (auto i : _mapValue)
                {
                    std::cout << indent + INDENT << i.first << ": ";
                    i.second.dump(indent + INDENT);
                }
            }
        }
        else if (_marker == Marker::Number ||
                 _marker == Marker::Boolean ||
                 _marker == Marker::String ||
                 _marker == Marker::Date ||
                 _marker == Marker::LongString ||
                 _marker == Marker::XMLDocument)
        {
            std::cout << ", value: ";

            switch (_marker)
            {
                case Marker::Number: std::cout << _doubleValue; break;
                case Marker::Boolean: std::cout << (_boolValue ? "true" : "false"); break;
                case Marker::String: std::cout << _stringValue; break;
                case Marker::Date: std::cout << "ms=" <<  _dateValue.ms << "timezone=" <<  _dateValue.timezone; break;
                case Marker::LongString: std::cout << _stringValue; break;
                case Marker::XMLDocument: std::cout << _stringValue; break;
                default:break;
            }

            std::cout << std::endl;
        }
        else
        {
            std::cout << std::endl;
        }
    }
}
