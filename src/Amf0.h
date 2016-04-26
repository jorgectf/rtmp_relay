//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <map>

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
        Node();
        Node(Marker marker);
        Node(double value);
        Node(bool value);
        Node(const std::string& value);
        Node(const Date& value);

        Node& operator=(Marker marker);
        Node& operator=(double value);
        Node& operator=(bool value);
        Node& operator=(const std::string& value);
        Node& operator=(const Date& value);

        Marker getMarker() const { return _marker; }

        uint32_t decode(const std::vector<uint8_t>& buffer, uint32_t offset = 0);
        uint32_t encode(std::vector<uint8_t>& buffer) const;

        void setDouble(double value);
        double asDouble() const;

        void setBool(bool value);
        bool asBool() const;

        void setString(const std::string& value);
        const std::string& asString() const;

        void setDate(const Date& value);
        const Date& asDate() const;

        void setNull();
        bool isNull() const;

        void setUndefined();
        bool isUndefined() const;

        uint32_t getSize() const;

        Node operator[](size_t key) const;
        Node& operator[](size_t key);

        Node operator[](const std::string& key) const;
        Node& operator[](const std::string& key);

        bool hasElement(const std::string& key);
        void append(const Node& node);

        void dump(const std::string& indent = "");

    private:
        Marker _marker = Marker::Unknown;

        bool _boolValue = false;
        double _doubleValue = 0.0;
        std::string _stringValue;
        std::vector<Node> _vectorValue;
        std::map<std::string, Node> _mapValue;
        Date _dateValue;
    };
}
