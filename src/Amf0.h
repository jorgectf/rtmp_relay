//
//  rtmp_relay
//

#pragma once

#include <cassert>
#include <limits>
#include <vector>
#include <map>
#include "Log.h"

namespace relay
{
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
            SwitchToAMF3 = 0x11,

            Unknown = 0xff
        };

        std::string markerToString(Marker marker);

        struct Date
        {
            double ms = 0.0;
            uint32_t timezone = 0;
        };

        class Node
        {
        public:
            Node() {}
            Node(Marker aMarker): marker(aMarker) {}
            Node(double value): marker(Marker::Number), doubleValue(value) {}
            Node(bool value): marker(Marker::Boolean), boolValue(value) {}
            Node(const std::vector<Node>& value): marker(Marker::StrictArray), vectorValue(value) {}
            Node(const std::map<std::string, Node>& value): marker(Marker::Object), mapValue(value) {}
            Node(const std::string& value):
                stringValue(value)
            {
                if (value.length() <= std::numeric_limits<uint16_t>::max())
                {
                    marker = Marker::String;
                }
                else
                {
                    marker = Marker::LongString;
                }
            }

            Node(const Date& value): marker(Marker::Date), dateValue(value) {}

            Node& operator=(Marker newMarker)
            {
                marker = newMarker;
                return *this;
            }

            Node& operator=(double value)
            {
                marker = Marker::Number;
                doubleValue = value;
                return *this;
            }

            Node& operator=(bool value)
            {
                marker = Marker::Boolean;
                boolValue = value;
                return *this;
            }

            Node& operator=(const std::string& value)
            {
                stringValue = value;
                if (value.length() <= std::numeric_limits<uint16_t>::max())
                {
                    marker = Marker::String;
                }
                else
                {
                    marker = Marker::LongString;
                }
                return *this;
            }

            Node& operator=(const std::vector<Node>& value)
            {
                marker = Marker::StrictArray;
                vectorValue = value;
                return *this;
            }

            Node& operator=(const std::map<std::string, Node>& value)
            {
                marker = Marker::Object;
                mapValue = value;
                return *this;
            }

            Node& operator=(const Date& value)
            {
                marker = Marker::Date;
                dateValue = value;
                return *this;
            }

            Marker getMarker() const { return marker; }

            uint32_t decode(const std::vector<uint8_t>& buffer, uint32_t offset = 0);
            uint32_t encode(std::vector<uint8_t>& buffer) const;

            double asDouble() const
            {
                assert(marker == Marker::Number);

                return doubleValue;
            }

            bool asBool() const
            {
                assert(marker == Marker::Boolean);

                return boolValue;
            }

            const std::string& asString() const
            {
                assert(marker == Marker::String || marker == Marker::LongString);

                return stringValue;
            }

            const Date& asDate() const
            {
                assert(marker == Marker::Date);

                return dateValue;
            }

            bool isNull() const
            {
                return marker == Marker::Null;
            }

            bool isUndefined() const
            {
                return marker == Marker::Undefined;
            }

            const std::vector<Node>& asVector() const
            {
                assert(marker == Marker::StrictArray);

                return vectorValue;
            }
            
            const std::map<std::string, Node>& asMap() const
            {
                assert(marker == Marker::Object || marker == Marker::ECMAArray);

                return mapValue;
            }

            std::string toString() const
            {
                switch (marker)
                {
                    case Marker::Number: return std::to_string(doubleValue);
                    case Marker::Boolean: return std::to_string(boolValue);
                    case Marker::String: return stringValue;
                    case Marker::Object: return "object";
                    case Marker::Null: return "null";
                    case Marker::Undefined: return "undefined";
                    case Marker::ECMAArray: return "ECMA array";
                    case Marker::ObjectEnd: return ""; // should not happen
                    case Marker::StrictArray: return "strict array";
                    case Marker::Date: return std::to_string(dateValue.ms) + " +" + std::to_string(dateValue.timezone);
                    case Marker::LongString: return stringValue;
                    case Marker::XMLDocument: return stringValue;
                    case Marker::TypedObject: return "typed object";
                    case Marker::SwitchToAMF3: return "switch to AMF3";
                    default: return "";
                }
            }

            uint32_t getSize() const
            {
                assert(marker == Marker::StrictArray);

                return static_cast<uint32_t>(vectorValue.size());
            }

            Node operator[](size_t key) const
            {
                assert(marker == Marker::StrictArray);
                
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
                marker = Marker::StrictArray;
                return vectorValue[key];
            }

            Node operator[](const std::string& key) const
            {
                assert(marker == Marker::Object || marker == Marker::ECMAArray);

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
                if (marker != Marker::Object &&
                    marker != Marker::ECMAArray)
                {
                    marker = Marker::Object;
                }
                return mapValue[key];
            }

            bool hasElement(const std::string& key) const
            {
                assert(marker == Marker::Object || marker == Marker::ECMAArray);

                return mapValue.find(key) != mapValue.end();
            }
            
            void append(const Node& node)
            {
                assert(marker == Marker::StrictArray);

                vectorValue.push_back(node);
            }

            void dump(cppsocket::Log& log, const std::string& indent = "");

        private:
            Marker marker = Marker::Unknown;

            bool boolValue = false;
            double doubleValue = 0.0;
            std::string stringValue;
            std::vector<Node> vectorValue;
            std::map<std::string, Node> mapValue;
            Date dateValue;
        };
    }
}
