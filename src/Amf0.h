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
        SwitchToAMF3 = 0x11
    };

    struct Date
    {
        double ms = 0.0;
        uint32_t timezone = 0;
    };

    class Node
    {
    public:
        bool parseBuffer(const std::vector<uint8_t>& buffer, uint32_t offset = 0);

        double asDouble() const;
        bool asBool() const;
        const std::string& asString() const;
        const Date& asDate() const;

        bool isNull() const;
        bool isUndefined() const;

        uint32_t getSize() const;

        Node operator[](size_t key) const;
        Node operator[](const std::string& key) const;

    private:
        Marker _marker;

        bool _boolValue = false;
        double _doubleValue = 0.0;
        std::string _stringValue;
        std::vector<Node> _vectorValue;
        std::map<std::string, Node> _mapValue;
        Date _dateValue;
    };
}
