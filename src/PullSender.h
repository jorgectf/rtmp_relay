//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <string>
#include <set>
#include "Socket.h"

namespace relay
{
    class PullSender
    {
    public:
        PullSender(cppsocket::Socket& aSocket,
                   const std::string& aApplication,
                   const std::string& aOverrideStreamName,
                   bool videoOutput,
                   bool audioOutput,
                   bool dataOutput,
                   const std::set<std::string>& aMetaDataBlacklist);

    private:
        cppsocket::Socket& socket;

        const std::string application;
        const std::string overrideStreamName;
        const bool videoStream;
        const bool audioStream;
        const bool dataStream;
        std::set<std::string> metaDataBlacklist;
    };
}
