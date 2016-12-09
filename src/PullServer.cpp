//
//  rtmp_relay
//

#include "PullServer.h"

namespace relay
{
    PullServer::PullServer(cppsocket::Network& aNetwork,
                           const std::string& aApplication,
                           const std::string& aOverrideStreamName,
                           const std::string& aAddress,
                           bool videoOutput,
                           bool audioOutput,
                           bool dataOutput,
                           const std::set<std::string>& aMetaDataBlacklist):
        network(aNetwork),
        socket(aNetwork),
        application(aApplication),
        overrideStreamName(aOverrideStreamName),
        address(aAddress),
        videoStream(videoOutput),
        audioStream(audioOutput),
        dataStream(dataOutput),
        metaDataBlacklist(aMetaDataBlacklist)
    {
        socket.setAcceptCallback(std::bind(&PullServer::handleAccept, this, std::placeholders::_1));

        socket.startAccept(address);
    }

    void PullServer::handleAccept(cppsocket::Socket& clientSocket)
    {
        std::auto_ptr<PullSender> pullSender(new PullSender(clientSocket,
                                                            application,
                                                            overrideStreamName,
                                                            videoStream,
                                                            audioStream,
                                                            dataStream,
                                                            metaDataBlacklist));

        pullSenders.push_back(std::move(pullSender));
    }
}
