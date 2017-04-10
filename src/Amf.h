//
//  rtmp_relay
//

#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <vector>
#include <map>
#include "Log.h"

namespace relay
{
    namespace amf
    {
        enum class Version
        {
            AMF0 = 0,
            AMF3 = 1
        };

        enum class AMF0Marker: uint8_t
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
            SwitchToAMF3 = 0x11,

            Unknown = 0xff
        };

        enum class AMF3Marker: uint8_t
        {
            Undefined = 0x00,
            Null = 0x01,
            False = 0x02,
            True = 0x03,
            Integer = 0x04,
            Double = 0x05,
            String = 0x06,
            XMLDocument = 0x07,
            Date = 0x08,
            Array = 0x09,
            Object = 0x0a,
            XML = 0x0b,
            ByteArray = 0x0c,
            VectorInt = 0x0e,
            VectorDouble = 0x0f,
            VectorObject = 0x10,
            Disctionary = 0x11,

            Unknown = 0xff
        };

        std::string markerToString(AMF0Marker marker);

        class Node
        {
        public:
            enum class Type
            {
                Unknown,
                Null,
                Number,
                Boolean,
                String,
                Object,
                Undefined,
                ECMAArray,
                StrictArray,
                Date,
                XMLDocument,
                TypedObject
            };

            Node() {}
            Node(AMF0Marker aMarker): marker(aMarker) {}
            Node(double value): marker(AMF0Marker::Number), doubleValue(value) {}
            Node(bool value): marker(AMF0Marker::Boolean), boolValue(value) {}
            Node(const std::vector<Node>& value): marker(AMF0Marker::StrictArray), vectorValue(value) {}
            Node(const std::map<std::string, Node>& value): marker(AMF0Marker::Object), mapValue(value) {}
            Node(const std::string& value):
                marker((value.length() <= std::numeric_limits<uint16_t>::max()) ? AMF0Marker::String : AMF0Marker::LongString),
                stringValue(value)
            {
            }

            Node(double ms, uint32_t aTimezone): marker(AMF0Marker::Date), doubleValue(ms), timezone(aTimezone) {}

            bool operator!() const
            {
                return marker == AMF0Marker::Null ||
                       marker == AMF0Marker::Undefined ||
                       marker == AMF0Marker::Unknown ||
                       (marker == AMF0Marker::Number && doubleValue == 0.0f) ||
                       (marker == AMF0Marker::Boolean && boolValue == false);
            }

            Node& operator=(AMF0Marker newMarker)
            {
                marker = newMarker;

                switch (marker)
                {
                    case AMF0Marker::Number: doubleValue = 0.0; break;
                    case AMF0Marker::Boolean: boolValue = false; break;
                    case AMF0Marker::String:
                    case AMF0Marker::LongString:
                    case AMF0Marker::XMLDocument:
                        stringValue.clear();
                        break;
                    case AMF0Marker::Object: mapValue.clear(); break;
                    case AMF0Marker::Null: break;
                    case AMF0Marker::Undefined: break;
                    case AMF0Marker::ECMAArray: mapValue.clear(); break;
                    case AMF0Marker::ObjectEnd: break;
                    case AMF0Marker::StrictArray: vectorValue.clear(); break;
                    case AMF0Marker::Date: doubleValue = 0.0; timezone = 0; break;
                    case AMF0Marker::TypedObject: break;
                    case AMF0Marker::SwitchToAMF3: break;
                    case AMF0Marker::Unknown: break;
                }

                return *this;
            }

            Node& operator=(double value)
            {
                marker = AMF0Marker::Number;
                doubleValue = value;
                return *this;
            }

            Node& operator=(bool value)
            {
                marker = AMF0Marker::Boolean;
                boolValue = value;
                return *this;
            }

            Node& operator=(const std::string& value)
            {
                if (value.length() <= std::numeric_limits<uint16_t>::max())
                {
                    marker = AMF0Marker::String;
                }
                else
                {
                    marker = AMF0Marker::LongString;
                }
                stringValue = value;
                return *this;
            }

            Node& operator=(const std::vector<Node>& value)
            {
                marker = AMF0Marker::StrictArray;
                vectorValue = value;
                return *this;
            }

            Node& operator=(const std::map<std::string, Node>& value)
            {
                marker = AMF0Marker::Object;
                mapValue = value;
                return *this;
            }

            AMF0Marker getMarker() const { return marker; }

            uint32_t decode(Version version, const std::vector<uint8_t>& buffer, uint32_t offset = 0);
            uint32_t encode(Version version, std::vector<uint8_t>& buffer) const;

            double asDouble() const
            {
                assert(marker == AMF0Marker::Number);

                return doubleValue;
            }

            uint32_t asUInt32() const
            {
                assert(marker == AMF0Marker::Number);

                return static_cast<uint32_t>(doubleValue);
            }

            uint64_t asUInt64() const
            {
                assert(marker == AMF0Marker::Number);

                return static_cast<uint64_t>(doubleValue);
            }

            bool asBool() const
            {
                assert(marker == AMF0Marker::Boolean);

                return boolValue;
            }

            const std::string& asString() const
            {
                assert(marker == AMF0Marker::String || marker == AMF0Marker::LongString);

                return stringValue;
            }

            bool isNull() const
            {
                return marker == AMF0Marker::Null;
            }

            bool isUndefined() const
            {
                return marker == AMF0Marker::Undefined;
            }

            const std::vector<Node>& asVector() const
            {
                assert(marker == AMF0Marker::StrictArray);

                return vectorValue;
            }
            
            const std::map<std::string, Node>& asMap() const
            {
                assert(marker == AMF0Marker::Object || marker == AMF0Marker::ECMAArray);

                return mapValue;
            }

            std::string toString() const
            {
                switch (marker)
                {
                    case AMF0Marker::Number: return std::to_string(doubleValue);
                    case AMF0Marker::Boolean: return std::to_string(boolValue);
                    case AMF0Marker::String: return stringValue;
                    case AMF0Marker::Object: return "object";
                    case AMF0Marker::Null: return "null";
                    case AMF0Marker::Undefined: return "undefined";
                    case AMF0Marker::ECMAArray: return "ECMA array";
                    case AMF0Marker::ObjectEnd: return ""; // should not happen
                    case AMF0Marker::StrictArray: return "strict array";
                    case AMF0Marker::Date: return std::to_string(doubleValue) + " +" + std::to_string(timezone);
                    case AMF0Marker::LongString: return stringValue;
                    case AMF0Marker::XMLDocument: return stringValue;
                    case AMF0Marker::TypedObject: return "typed object";
                    case AMF0Marker::SwitchToAMF3: return "switch to AMF3";
                    default: return "";
                }
            }

            double getMs() const
            {
                assert(marker == AMF0Marker::Date);

                return doubleValue;
            }

            uint32_t getTimezone() const
            {
                assert(marker == AMF0Marker::Date);

                return timezone;
            }

            uint32_t getSize() const
            {
                assert(marker == AMF0Marker::StrictArray);

                return static_cast<uint32_t>(vectorValue.size());
            }

            Node operator[](size_t key) const
            {
                assert(marker == AMF0Marker::StrictArray);
                
                if (key >= vectorValue.size())
                {
                    return Node();
                }
                else
                {
                    return vectorValue[key];
                }
            }

            Node& operator[](size_t key)
            {
                marker = AMF0Marker::StrictArray;
                return vectorValue[key];
            }

            Node operator[](const std::string& key) const
            {
                assert(marker == AMF0Marker::Object || marker == AMF0Marker::ECMAArray);

                auto i = mapValue.find(key);

                if (i == mapValue.end())
                {
                    return Node();
                }
                else
                {
                    return i->second;
                }
            }

            Node& operator[](const std::string& key)
            {
                if (marker != AMF0Marker::Object &&
                    marker != AMF0Marker::ECMAArray)
                {
                    marker = AMF0Marker::Object;
                }
                return mapValue[key];
            }

            bool hasElement(const std::string& key) const
            {
                assert(marker == AMF0Marker::Object || marker == AMF0Marker::ECMAArray);

                return mapValue.find(key) != mapValue.end();
            }
            
            void append(const Node& node)
            {
                assert(marker == AMF0Marker::StrictArray);

                vectorValue.push_back(node);
            }

            void dump(cppsocket::Log& log, const std::string& indent = "");

        private:
            AMF0Marker marker = AMF0Marker::Unknown;

            union
            {
                double doubleValue = 0.0;
                bool boolValue;
            };
            uint32_t timezone;
            std::string stringValue;
            std::vector<Node> vectorValue;
            std::map<std::string, Node> mapValue;
        };
    }
}
