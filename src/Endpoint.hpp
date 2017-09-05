//
//  rtmp_relay
//

#pragma once

#include <cstdint>
#include <vector>
#include "Connection.hpp"
#include "Stream.hpp"
#include "Amf.hpp"

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
        std::set<std::string> metaDataBlacklist;

        bool isNameKnown() const
        {
            return !applicationName.empty() && !streamName.empty();
        }
    };
}
