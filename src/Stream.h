//
//  rtmp_relay
//

#pragma once

#include <string>
#include <vector>
#include "Amf.h"

namespace relay
{
    class Connection;

    class Stream
    {
    public:
        const std::string& getApplicationName() const { return applicationName; }
        const std::string& getStreamName() const { return streamName; }

    private:
        std::string applicationName;
        std::string streamName;

        bool streaming = false;
        std::vector<uint8_t> audioHeader;
        std::vector<uint8_t> videoHeader;
        amf::Node metaData;

        std::vector<Connection> connections;
    };
}
