//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <vector>
#include "Connection.h"
#include "Stream.h"
#include "Amf.h"

namespace relay
{
    class Server;

    struct Endpoint
    {
        Connection::Type connectionType;
        Stream::Type streamType;
        std::vector<std::pair<uint32_t, uint16_t>> ipAddresses;
        std::vector<std::string> addresses;
        float connectionTimeout = 5.0f;
        float reconnectInterval = 5.0f;
        uint32_t reconnectCount = 0;
        float pingInterval = 60.0f;
        uint32_t bufferSize = 3000;
        amf::Version amfVersion = amf::Version::AMF0;

        bool videoStream = true;
        bool audioStream = true;
        bool dataStream = true;
        std::string applicationName;
        std::string streamName;
        std::string overrideApplicationName;
        std::string overrideStreamName;
        std::set<std::string> metaDataBlacklist;
    };
}
