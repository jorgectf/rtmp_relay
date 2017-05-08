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
            SwitchToAMF3 = 0x11
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
            Dictionary = 0x11
        };

        class Node
        {
        public:
            enum class Type
            {
                Unknown,
                Null,
                Integer,
                Double,
                Boolean,
                String,
                Object,
                Undefined,
                Dictionary,
                Array,
                Date,
                XMLDocument,
                TypedObject,
                SwitchToAMF3
            };

            Node() {}
            Node(Type aType): type(aType) {}
            Node(int32_t value): type(Type::Integer), intValue(value) {}
            Node(double value): type(Type::Double), doubleValue(value) {}
            Node(bool value): type(Type::Boolean), boolValue(value) {}
            Node(const std::vector<Node>& value): type(Type::Array), vectorValue(value) {}
            Node(const std::map<std::string, Node>& value): type(Type::Object), mapValue(value) {}
            Node(const std::string& value): type(Type::String), stringValue(value) {}

            Node(double ms, uint32_t aTimezone): type(Type::Date), doubleValue(ms), timezone(aTimezone) {}

            bool operator!() const
            {
                return type == Type::Null ||
                       type == Type::Undefined ||
                       type == Type::Unknown ||
                       (type == Type::Integer && intValue == 0) ||
                       (type == Type::Double && doubleValue == 0.0f) ||
                       (type == Type::Boolean && boolValue == false);
            }

            Node& operator=(Type newType)
            {
                type = newType;

                switch (type)
                {
                    case Type::Unknown: break; // should not happen
                    case Type::Null: break;
                    case Type::Integer: intValue = 0; break;
                    case Type::Double: doubleValue = 0.0; break;
                    case Type::Boolean: boolValue = false; break;
                    case Type::String:
                    case Type::XMLDocument:
                        stringValue.clear();
                        break;
                    case Type::Object:
                    case Type::Dictionary:
                        mapValue.clear();
                        break;
                    case Type::Undefined: break;
                    case Type::Array: vectorValue.clear(); break;
                    case Type::Date: doubleValue = 0.0; timezone = 0; break;
                    case Type::TypedObject: break;
                    case Type::SwitchToAMF3: break;
                }

                return *this;
            }

            Node& operator=(int32_t value)
            {
                type = Type::Integer;
                intValue = value;
                return *this;
            }

            Node& operator=(double value)
            {
                type = Type::Double;
                doubleValue = value;
                return *this;
            }

            Node& operator=(bool value)
            {
                type = Type::Boolean;
                boolValue = value;
                return *this;
            }

            Node& operator=(const std::string& value)
            {
                type = Type::String;
                stringValue = value;
                return *this;
            }

            Node& operator=(const std::vector<Node>& value)
            {
                type = Type::Array;
                vectorValue = value;
                return *this;
            }

            Node& operator=(const std::map<std::string, Node>& value)
            {
                type = Type::Object;
                mapValue = value;
                return *this;
            }

            Type getType() const { return type; }

            uint32_t decode(Version version, const std::vector<uint8_t>& buffer, uint32_t offset = 0);
            uint32_t encode(Version version, std::vector<uint8_t>& buffer) const;

            double asDouble() const
            {
                assert(type == Type::Integer || type == Type::Double);

                if (type == Type::Integer) return intValue;
                else if (type == Type::Double) return doubleValue;
                else return 0.0;
            }

            int32_t asInt32() const
            {
                assert(type == Type::Integer || type == Type::Double);

                if (type == Type::Integer) return static_cast<int32_t>(intValue);
                else if (type == Type::Double) return static_cast<int32_t>(doubleValue);
                else return 0;
            }

            int64_t asInt64() const
            {
                assert(type == Type::Integer || type == Type::Double);

                if (type == Type::Integer) return static_cast<int64_t>(intValue);
                else if (type == Type::Double) return static_cast<int64_t>(doubleValue);
                else return 0;
            }

            uint32_t asUInt32() const
            {
                assert(type == Type::Integer || type == Type::Double);

                if (type == Type::Integer) return static_cast<uint32_t>(intValue);
                else if (type == Type::Double) return static_cast<uint32_t>(doubleValue);
                else return 0;
            }

            uint64_t asUInt64() const
            {
                assert(type == Type::Integer || type == Type::Double);

                if (type == Type::Integer) return static_cast<uint64_t>(intValue);
                else if (type == Type::Double) return static_cast<uint64_t>(doubleValue);
                else return 0;
            }

            bool asBool() const
            {
                assert(type == Type::Boolean);

                return boolValue;
            }

            const std::string& asString() const
            {
                assert(type == Type::String);

                return stringValue;
            }

            bool isNull() const
            {
                return type == Type::Null;
            }

            bool isUndefined() const
            {
                return type == Type::Undefined;
            }

            const std::vector<Node>& asVector() const
            {
                assert(type == Type::Array);

                return vectorValue;
            }
            
            const std::map<std::string, Node>& asMap() const
            {
                assert(type == Type::Object || type == Type::Dictionary);

                return mapValue;
            }

            std::string toString() const
            {
                switch (type)
                {
                    case Type::Unknown: return "";
                    case Type::Null: return "null";
                    case Type::Integer: return std::to_string(intValue);
                    case Type::Double: return std::to_string(doubleValue);
                    case Type::Boolean: return std::to_string(boolValue);
                    case Type::String: return stringValue;
                    case Type::Object: return "object";
                    case Type::Undefined: return "undefined";
                    case Type::Dictionary: return "dictionary";
                    case Type::Array: return "array";
                    case Type::Date: return std::to_string(doubleValue) + " +" + std::to_string(timezone);
                    case Type::XMLDocument: return stringValue;
                    case Type::TypedObject: return "typed object";
                    case Type::SwitchToAMF3: return "switch to AMF3";
                }
            }

            double getMs() const
            {
                assert(type == Type::Date);

                return doubleValue;
            }

            uint32_t getTimezone() const
            {
                assert(type == Type::Date);

                return timezone;
            }

            uint32_t getSize() const
            {
                assert(type == Type::Array);

                return static_cast<uint32_t>(vectorValue.size());
            }

            Node operator[](size_t key) const
            {
                assert(type == Type::Array);
                
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
                type = Type::Array;
                return vectorValue[key];
            }

            Node operator[](const std::string& key) const
            {
                assert(type == Type::Object || type == Type::Dictionary);

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
                if (type != Type::Object &&
                    type != Type::Dictionary)
                {
                    type = Type::Object;
                }
                return mapValue[key];
            }

            bool hasElement(const std::string& key) const
            {
                assert(type == Type::Object || type == Type::Dictionary);

                return mapValue.find(key) != mapValue.end();
            }
            
            void append(const Node& node)
            {
                assert(type == Type::Array);

                vectorValue.push_back(node);
            }

            void dump(cppsocket::Log& log, const std::string& indent = "");

        private:
            Type type = Type::Unknown;

            union
            {
                int32_t intValue = 0;
                double doubleValue;
                bool boolValue;
            };
            uint32_t timezone;
            std::string stringValue;
            std::vector<Node> vectorValue;
            std::map<std::string, Node> mapValue;
        };
    }
}
