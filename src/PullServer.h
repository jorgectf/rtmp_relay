//
//  rtmp_relay
//

#pragma once

#include <vector>
#include <string>
#include <set>
#include "Acceptor.h"
#include "PullSender.h"

namespace relay
{
    struct PullDescriptor
    {
        std::string overrideStreamName;
        std::string address;
        bool videoOutput;
        bool audioOutput;
        bool dataOutput;
        std::set<std::string> metaDataBlacklist;
    };
    
    class PullServer
    {
    public:
        PullServer(cppsocket::Network& aNetwork,
                   const std::string& aApplication,
                   const std::string& aOverrideStreamName,
                   const std::string& aAddress,
                   bool videoOutput,
                   bool audioOutput,
                   bool dataOutput,
                   const std::set<std::string>& aMetaDataBlacklist);

    private:
        void handleAccept(cppsocket::Socket& clientSocket);
        
        cppsocket::Network& network;
        cppsocket::Acceptor socket;

        const std::string application;
        const std::string overrideStreamName;
        const std::string address;
        const bool videoStream;
        const bool audioStream;
        const bool dataStream;
        std::set<std::string> metaDataBlacklist;

        std::vector<std::unique_ptr<PullSender>> pullSenders;
    };
}
