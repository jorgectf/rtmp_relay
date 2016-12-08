//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <string>
#include <set>

namespace relay
{
    struct PullDescriptor
    {
        std::string overrideStreamName;
        std::vector<std::string> addresses;
        bool videoOutput;
        bool audioOutput;
        bool dataOutput;
        std::set<std::string> metaDataBlacklist;
    };

    class PullSender
    {
    public:
    };
}
