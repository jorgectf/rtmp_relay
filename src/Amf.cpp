//
//  rtmp_relay
//

#include <iostream>
#include "Amf.h"
#include "Utils.h"

using namespace cppsocket;

namespace relay
{
    namespace amf
    {
        static const std::string INDENT = "  ";

        static std::string markerToString(AMF0Marker marker)
        {
            switch (marker)
            {
                case AMF0Marker::Number: return "Number";
                case AMF0Marker::Boolean: return "Boolean";
                case AMF0Marker::String: return "String";
                case AMF0Marker::Object: return "Object";
                case AMF0Marker::Null: return "Null";
                case AMF0Marker::Undefined: return "Undefined";
                case AMF0Marker::ECMAArray: return "ECMAArray";
                case AMF0Marker::ObjectEnd: return "ObjectEnd";
                case AMF0Marker::StrictArray: return "StrictArray";
                case AMF0Marker::Date: return "Date";
                case AMF0Marker::LongString: return "LongString";
                case AMF0Marker::XMLDocument: return "XMLDocument";
                case AMF0Marker::TypedObject: return "TypedObject";
                case AMF0Marker::SwitchToAMF3: return "SwitchToAMF3";
                default: return "Unknown";
            }
        }

        static std::string typeToString(Node::Type type)
        {
            switch (type)
            {
                case Node::Type::Unknown: return "Unknwonw";
                case Node::Type::Null: return "Null";
                case Node::Type::Integer: return "Integer";
                case Node::Type::Double: return "Double";
                case Node::Type::Boolean: return "Boolean";
                case Node::Type::String: return "String";
                case Node::Type::Object: return "Object";
                case Node::Type::Undefined: return "Undefined";
                case Node::Type::Dictionary: return "Dictionary";
                case Node::Type::StrictArray: return "StrictArray";
                case Node::Type::Date: return "Date";
                case Node::Type::XMLDocument: return "XMLDocument";
                case Node::Type::TypedObject: return "TypedObject";
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

            result = (*(buffer.data() + offset)) > 0;
            offset += 1;

            return offset - originalOffset;
        }

        static uint32_t readString(const std::vector<uint8_t>& buffer, uint32_t offset, std::string& result)
        {
            uint32_t originalOffset = offset;

            uint16_t length;

            uint32_t ret = decodeIntBE(buffer, offset, 2, length);

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

                AMF0Marker marker = *reinterpret_cast<const AMF0Marker*>(buffer.data() + offset);

                if (marker == AMF0Marker::ObjectEnd)
                {
                    offset += 1;
                    break;
                }
                else
                {
                    Node node;

                    ret = node.decode(amf::Version::AMF0, buffer, offset);

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

            uint32_t ret = decodeIntBE(buffer, offset, 4, count);

            if (ret == 0)
            {
                return 0;
            }

            offset += ret;

            std::string key;

            uint32_t currentCount = 0;

            while (true)
            {
                key.clear();
                ret = readString(buffer, offset, key);

                if (ret == 0)
                {
                    return 0;
                }

                offset += ret;

                if (buffer.size() - offset < 1)
                {
                    return 0;
                }

                AMF0Marker marker = *reinterpret_cast<const AMF0Marker*>(buffer.data() + offset);

                if (marker == AMF0Marker::ObjectEnd)
                {
                    offset += 1;
                    break;
                }
                else
                {
                    Node node;
                    ret = node.decode(amf::Version::AMF0, buffer, offset);

                    if (ret == 0)
                    {
                        return 0;
                    }

                    offset += ret;

                    result[key] = node;

                    ++currentCount;
                }
            }

            if (count != 0 && count != currentCount) // Wowza sends count 0 for ECMA arrays
            {
                return 0;
            }

            return offset - originalOffset;
        }

        static uint32_t readStrictArray(const std::vector<uint8_t>& buffer, uint32_t offset, std::vector<Node>& result)
        {
            uint32_t originalOffset = offset;

            uint32_t count;

            uint32_t ret = decodeIntBE(buffer, offset, 4, count);

            if (ret == 0)
            {
                return 0;
            }

            offset += ret;

            for (uint32_t i = 0; i < count; ++i)
            {
                Node node;
                ret = node.decode(amf::Version::AMF0, buffer, offset);

                if (ret == 0)
                {
                    return 0;
                }

                offset += ret;

                result.push_back(node);
            }

            return offset - originalOffset;
        }

        static uint32_t readDate(const std::vector<uint8_t>& buffer, uint32_t offset, double& ms, uint32_t& timezone)
        {
            uint32_t originalOffset = offset;

            uint32_t ret = decodeDouble(buffer, offset, ms);

            if (ret == 0) // date in milliseconds from 01/01/1970
            {
                return 0;
            }

            offset += ret;

            ret = decodeIntBE(buffer, offset, 4, timezone);

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

            uint32_t ret = decodeIntBE(buffer, offset, 4, length);

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

            uint32_t ret = decodeIntBE(buffer, offset, 4, length);

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

            Log(Log::Level::ERR) << "Typed objects are not supported";

            return 0;
        }

        static uint32_t readSwitchToAMF3(const std::vector<uint8_t>& buffer, uint32_t& offset)
        {
            UNUSED(buffer);
            UNUSED(offset);

            Log(Log::Level::ERR) << "AMF3 is not supported";

            return 0;
        }

        // writing
        static uint32_t writeNumber(std::vector<uint8_t>& buffer, double value)
        {
            uint32_t ret = encodeDouble(buffer, value);

            return ret;
        }

        static uint32_t writeBoolean(std::vector<uint8_t>& buffer, bool value)
        {
            buffer.push_back(static_cast<uint8_t>(value));

            return 1;
        }

        static uint32_t writeString(std::vector<uint8_t>& buffer, const std::string& value)
        {
            uint32_t ret = encodeIntBE(buffer, 2, value.size());

            if (ret == 0)
            {
                return 0;
            }

            uint32_t size = ret;

            buffer.insert(buffer.end(),
                          reinterpret_cast<const uint8_t*>(value.data()),
                          reinterpret_cast<const uint8_t*>(value.data()) + value.length());
            size += static_cast<uint32_t>(value.length());

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

                ret = i.second.encode(amf::Version::AMF0, buffer);

                if (ret == 0)
                {
                    return 0;
                }

                size += ret;
            }

            if ((ret = writeString(buffer, "")) == 0)
            {
                return 0;
            }

            size += ret;

            AMF0Marker marker = AMF0Marker::ObjectEnd;
            buffer.push_back(static_cast<uint8_t>(marker));

            size += 1;

            return size;
        }

        static uint32_t writeECMAArray(std::vector<uint8_t>& buffer, const std::map<std::string, Node>& value)
        {
            uint32_t size = 0;

            uint32_t ret = encodeIntBE(buffer, 4, value.size());

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

                ret = i.second.encode(amf::Version::AMF0, buffer);

                if (ret == 0)
                {
                    return 0;
                }

                size += ret;
            }

            if ((ret = writeString(buffer, "")) == 0)
            {
                return 0;
            }

            size += ret;

            AMF0Marker marker = AMF0Marker::ObjectEnd;
            buffer.push_back(static_cast<uint8_t>(marker));
            
            size += 1;

            return size;
        }

        static uint32_t writeStrictArray(std::vector<uint8_t>& buffer, const std::vector<Node>& value)
        {
            uint32_t size = 0;

            uint32_t ret = encodeIntBE(buffer, 4, value.size());

            if (ret == 0)
            {
                return 0;
            }

            size += ret;

            for (const auto& i : value)
            {
                ret = i.encode(amf::Version::AMF0, buffer);

                if (ret == 0)
                {
                    return 0;
                }

                size += ret;
            }

            return size;
        }

        static uint32_t writeDate(std::vector<uint8_t>& buffer, double ms, uint32_t timezone)
        {
            uint32_t size = 0;

            uint32_t ret = encodeDouble(buffer, ms);

            if (ret == 0) // date in milliseconds from 01/01/1970
            {
                return 0;
            }

            size += ret;

            ret = encodeIntBE(buffer, 4, timezone);

            if (ret == 0) // unsupported timezone
            {
                return 0;
            }

            size += ret;

            return size;
        }

        static uint32_t writeLongString(std::vector<uint8_t>& buffer, const std::string& value)
        {
            uint32_t ret = encodeIntBE(buffer, 4, value.size());

            if (ret == 0)
            {
                return 0;
            }

            uint32_t size = ret;

            buffer.insert(buffer.end(),
                          reinterpret_cast<const uint8_t*>(value.data()),
                          reinterpret_cast<const uint8_t*>(value.data()) + value.length());
            size += static_cast<uint32_t>(value.length());

            return size;
        }

        static uint32_t writeXMLDocument(std::vector<uint8_t>& buffer, const std::string& value)
        {
            uint32_t ret = encodeIntBE(buffer, 4, value.size());

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

            Log(Log::Level::ERR) << "Typed objects are not supported";

            return 0;
        }

        static uint32_t writeSwitchToAMF3(std::vector<uint8_t>& buffer)
        {
            UNUSED(buffer);

            Log(Log::Level::ERR) << "AMF3 is not supported";

            return 0;
        }

        uint32_t Node::decode(Version version, const std::vector<uint8_t>& buffer, uint32_t offset)
        {
            uint32_t originalOffset = offset;

            if (version == Version::AMF0)
            {
                if (buffer.size() - offset < 1)
                {
                    return 0;
                }

                AMF0Marker marker = *reinterpret_cast<const AMF0Marker*>(buffer.data() + offset);
                offset += 1;

                uint32_t ret = 0;

                switch (marker)
                {
                    case AMF0Marker::Number:
                    {
                        if ((ret = readNumber(buffer, offset, doubleValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::Double;
                        break;
                    }
                    case AMF0Marker::Boolean:
                    {
                        if ((ret = readBoolean(buffer, offset, boolValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::Boolean;
                        break;
                    }
                    case AMF0Marker::String:
                    {
                        if ((ret = readString(buffer, offset, stringValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::String;
                        break;
                    }
                    case AMF0Marker::Object:
                    {
                        if ((ret = readObject(buffer, offset, mapValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::Object;
                        break;
                    }
                    case AMF0Marker::Null: type = Type::Null; break;
                    case AMF0Marker::Undefined: type = Type::Undefined; break;
                    case AMF0Marker::ECMAArray:
                    {
                        if ((ret = readECMAArray(buffer, offset, mapValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::Dictionary;
                        break;
                    }
                    case AMF0Marker::ObjectEnd: break; // should not happen
                    case AMF0Marker::StrictArray:
                    {
                        if ((ret = readStrictArray(buffer, offset, vectorValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::StrictArray;
                        break;
                    }
                    case AMF0Marker::Date:
                    {
                        if ((ret = readDate(buffer, offset, doubleValue, timezone)) == 0)
                        {
                            return 0;
                        }
                        type = Type::Date;
                        break;
                    }
                    case AMF0Marker::LongString:
                    {
                        if ((ret = readLongString(buffer, offset, stringValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::String;
                        break;
                    }
                    case AMF0Marker::XMLDocument:
                    {
                        if ((ret = readXMLDocument(buffer, offset, stringValue)) == 0)
                        {
                            return 0;
                        }
                        type = Type::XMLDocument;
                        break;
                    }
                    case AMF0Marker::TypedObject:
                    {
                        if ((ret = readTypedObject(buffer, offset)) == 0)
                        {
                            return 0;
                        }
                        type = Type::TypedObject;
                        break;
                    }
                    case AMF0Marker::SwitchToAMF3:
                    {
                        if ((ret = readSwitchToAMF3(buffer, offset)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    default: return 0;
                }

                offset += ret;
            }
            else if (version == Version::AMF3)
            {
                Log(Log::Level::ERR) << "AMF3 not supported";
                return 0;
            }

            return offset - originalOffset;
        }

        uint32_t Node::encode(Version version, std::vector<uint8_t>& buffer) const
        {
            uint32_t size = 0;

            if (version == Version::AMF0)
            {
                uint32_t ret = 0;

                AMF0Marker marker = AMF0Marker::Unknown;

                switch (type)
                {
                    case Type::Unknown: marker = AMF0Marker::Unknown; break; // should not happen
                    case Type::Null: marker = AMF0Marker::Null; break;
                    case Type::Integer: marker = AMF0Marker::Number; break;
                    case Type::Double: marker = AMF0Marker::Number; break;
                    case Type::Boolean: marker = AMF0Marker::Boolean; break;
                    case Type::String:
                    {
                        marker = ((stringValue.length() <= std::numeric_limits<uint16_t>::max()) ? AMF0Marker::String : AMF0Marker::LongString);
                        break;
                    }
                    case Type::Object: marker = AMF0Marker::Object; break;
                    case Type::Undefined: marker = AMF0Marker::Undefined; break;
                    case Type::Dictionary: marker = AMF0Marker::ECMAArray; break;
                    case Type::StrictArray: marker = AMF0Marker::StrictArray; break;
                    case Type::Date: marker = AMF0Marker::Date; break;
                    case Type::XMLDocument: marker = AMF0Marker::XMLDocument; break;
                    case Type::TypedObject: marker = AMF0Marker::TypedObject; break;
                }

                buffer.push_back(static_cast<uint8_t>(marker));
                size += 1;

                switch (type)
                {
                    case Type::Unknown: break; // should not happen
                    case Type::Null: break;
                    case Type::Integer:
                    {
                        ret = writeNumber(buffer, static_cast<double>(intValue));
                        break;
                    }
                    case Type::Double:
                    {
                        ret = writeNumber(buffer, doubleValue);
                        break;
                    }
                    case Type::Boolean:
                    {
                        ret = writeBoolean(buffer, boolValue);
                        break;
                    }
                    case Type::String:
                    {
                        if (stringValue.length() <= std::numeric_limits<uint16_t>::max())
                        {
                            writeString(buffer, stringValue); break;
                        }
                        else
                        {
                            writeLongString(buffer, stringValue); break;
                        }
                        break;
                    }
                    case Type::Object:
                    {
                        ret = writeObject(buffer, mapValue);
                        break;
                    }
                    case Type::Undefined: break;
                    case Type::Dictionary:
                    {
                        ret = writeECMAArray(buffer, mapValue);
                        break;
                    }
                    case Type::StrictArray:
                    {
                        ret = writeStrictArray(buffer, vectorValue);
                        break;
                    }
                    case Type::Date:
                    {
                        ret = writeDate(buffer, doubleValue, timezone);
                        break;
                    }
                    case Type::XMLDocument:
                    {
                        ret = writeXMLDocument(buffer, stringValue);
                        break;
                    }
                    case Type::TypedObject:
                    {
                        ret = writeTypedObject(buffer);
                        break;
                    }
                }

                size += ret;
            }
            else if (version == Version::AMF3)
            {
                AMF3Marker marker = AMF3Marker::Unknown;

                switch (type)
                {
                    case Type::Unknown: marker = AMF3Marker::Unknown; break; // should not happen
                    case Type::Null: marker = AMF3Marker::Null; break;
                    case Type::Integer: marker = AMF3Marker::Integer; break;
                    case Type::Double: marker = AMF3Marker::Double; break;
                    case Type::Boolean: marker = (boolValue) ? AMF3Marker::True : AMF3Marker::False; break;
                    case Type::String: marker = AMF3Marker::String; break;
                    case Type::Object: marker = AMF3Marker::Object; break;
                    case Type::Undefined: marker = AMF3Marker::Undefined; break;
                    case Type::Dictionary: marker = AMF3Marker::Dictionary; break;
                    case Type::StrictArray: /*marker = AMF3Marker::StrictArray;*/ break;
                    case Type::Date: marker = AMF3Marker::Date; break;
                    case Type::XMLDocument: marker = AMF3Marker::XMLDocument; break;
                    case Type::TypedObject: /*marker = AMF3Marker::TypedObject;*/ break;
                }

                Log(Log::Level::ERR) << "AMF3 not supported";
                return 0;
            }

            return size;
        }

        void Node::dump(cppsocket::Log& log, const std::string& indent)
        {
            log << "Type: " << typeToString(type) << "(" << static_cast<uint32_t>(type) << ")";

            if (type == Type::Object ||
                type == Type::StrictArray ||
                type == Type::Dictionary)
            {
                log << ", values:";

                if (type == Type::StrictArray)
                {
                    for (size_t index = 0; index < vectorValue.size(); index++)
                    {
                        log << "\n" << indent + INDENT << index << ": ";
                        vectorValue[index].dump(log, indent + INDENT);
                    }
                }
                else
                {
                    for (auto i : mapValue)
                    {
                        log << "\n" << indent + INDENT << i.first << ": ";
                        i.second.dump(log, indent + INDENT);
                    }
                }
            }
            else if (type == Type::Integer ||
                     type == Type::Double ||
                     type == Type::Boolean ||
                     type == Type::String ||
                     type == Type::Date ||
                     type == Type::XMLDocument)
            {
                log << ", value: ";

                switch (type)
                {
                    case Type::Integer: log << intValue; break;
                    case Type::Double: log << doubleValue; break;
                    case Type::Boolean: log << (boolValue ? "true" : "false"); break;
                    case Type::String: log << stringValue; break;
                    case Type::Date: log << "ms=" <<  doubleValue << "timezone=" <<  timezone; break;
                    case Type::XMLDocument: log << stringValue; break;
                    default:break;
                }
            }
        }
    }
}
