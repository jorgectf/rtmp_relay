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

                Marker marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);

                if (marker == Marker::ObjectEnd)
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

                Marker marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);

                if (marker == Marker::ObjectEnd)
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

            Marker marker = Marker::ObjectEnd;
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

            Marker marker = Marker::ObjectEnd;
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

                marker = *reinterpret_cast<const Marker*>(buffer.data() + offset);
                offset += 1;

                uint32_t ret = 0;

                switch (marker)
                {
                    case Marker::Number:
                    {
                        if ((ret = readNumber(buffer, offset, doubleValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::Boolean:
                    {
                        if ((ret = readBoolean(buffer, offset, boolValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::String:
                    {
                        if ((ret = readString(buffer, offset, stringValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::Object:
                    {
                        if ((ret = readObject(buffer, offset, mapValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::Null: /* Null */; break;
                    case Marker::Undefined: /* Undefined */; break;
                    case Marker::ECMAArray:
                    {
                        if ((ret = readECMAArray(buffer, offset, mapValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::ObjectEnd: break; // should not happen
                    case Marker::StrictArray:
                    {
                        if ((ret = readStrictArray(buffer, offset, vectorValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::Date:
                    {
                        if ((ret = readDate(buffer, offset, doubleValue, timezone)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::LongString:
                    {
                        if ((ret = readLongString(buffer, offset, stringValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::XMLDocument:
                    {
                        if ((ret = readXMLDocument(buffer, offset, stringValue)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::TypedObject:
                    {
                        if ((ret = readTypedObject(buffer, offset)) == 0)
                        {
                            return 0;
                        }
                        break;
                    }
                    case Marker::SwitchToAMF3:
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

                buffer.push_back(static_cast<uint8_t>(marker));
                size += 1;

                switch (marker)
                {
                    case Marker::Number: ret = writeNumber(buffer, doubleValue); break;
                    case Marker::Boolean: ret = writeBoolean(buffer, boolValue); break;
                    case Marker::String: ret = writeString(buffer, stringValue); break;
                    case Marker::Object: ret = writeObject(buffer, mapValue); break;
                    case Marker::Null: /* Null */; break;
                    case Marker::Undefined: /* Undefined */; break;
                    case Marker::ECMAArray: ret = writeECMAArray(buffer, mapValue); break;
                    case Marker::ObjectEnd: break; // should not happen
                    case Marker::StrictArray: ret = writeStrictArray(buffer, vectorValue); break;
                    case Marker::Date: ret = writeDate(buffer, doubleValue, timezone); break;
                    case Marker::LongString: ret = writeLongString(buffer, stringValue); break;
                    case Marker::XMLDocument: ret = writeXMLDocument(buffer, stringValue); break;
                    case Marker::TypedObject: ret = writeTypedObject(buffer); break;
                    case Marker::SwitchToAMF3: ret = writeSwitchToAMF3(buffer); break;
                    default: return 0;
                }

                size += ret;
            }
            else if (version == Version::AMF3)
            {
                Log(Log::Level::ERR) << "AMF3 not supported";
                return 0;
            }

            return size;
        }

        void Node::dump(cppsocket::Log& log, const std::string& indent)
        {
            log << "Type: " << markerToString(marker) << "(" << static_cast<uint32_t>(marker) << ")";

            if (marker == Marker::Object ||
                marker == Marker::StrictArray ||
                marker == Marker::ECMAArray)
            {
                log << ", values:";

                if (marker == Marker::StrictArray)
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
            else if (marker == Marker::Number ||
                     marker == Marker::Boolean ||
                     marker == Marker::String ||
                     marker == Marker::Date ||
                     marker == Marker::LongString ||
                     marker == Marker::XMLDocument)
            {
                log << ", value: ";

                switch (marker)
                {
                    case Marker::Number: log << doubleValue; break;
                    case Marker::Boolean: log << (boolValue ? "true" : "false"); break;
                    case Marker::String: log << stringValue; break;
                    case Marker::Date: log << "ms=" <<  doubleValue << "timezone=" <<  timezone; break;
                    case Marker::LongString: log << stringValue; break;
                    case Marker::XMLDocument: log << stringValue; break;
                    default:break;
                }
            }
        }
    }
}
