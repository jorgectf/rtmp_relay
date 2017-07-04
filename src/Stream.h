//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Connection.h"
#include "Amf.h"

namespace relay
{
    class Stream
    {
    public:
        Stream(Connection::StreamType aType,
               const std::string& aApplicationName,
               const std::string& aStreamName);

        Connection::StreamType getType() const { return type; }
        const std::string& getApplicationName() const { return applicationName; }
        const std::string& getStreamName() const { return streamName; }

    private:
        Connection::StreamType type;
        std::string applicationName;
        std::string streamName;

        bool streaming = false;
        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf::Node metaData;

        std::vector<Connection> connections;
    };
}
