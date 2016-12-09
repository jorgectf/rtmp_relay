//
//  rtmp_relay
//

#include "PullSender.h"

namespace relay
{
    PullSender::PullSender(cppsocket::Socket& aSocket,
                           const std::string& aApplication,
                           const std::string& aOverrideStreamName,
                           bool videoOutput,
                           bool audioOutput,
                           bool dataOutput,
                           const std::set<std::string>& aMetaDataBlacklist):
        socket(aSocket),
        application(aApplication),
        overrideStreamName(aOverrideStreamName),
        videoStream(videoOutput),
        audioStream(audioOutput),
        dataStream(dataOutput),
        metaDataBlacklist(aMetaDataBlacklist)
    {
    }

    void PullSender::update(float delta)
    {
    }
}
