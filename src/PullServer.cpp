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

    void PullServer::update(float delta)
    {
        for (auto senderIterator = pullSenders.begin(); senderIterator != pullSenders.end();)
        {
            const auto& pullSender = *senderIterator;

            if (pullSender->isConnected())
            {
                pullSender->update(delta);
                ++senderIterator;
            }
            else
            {
                senderIterator = pullSenders.erase(senderIterator);
            }
        }
    }

    void PullServer::createStream(const std::string& streamName)
    {
        for (const auto& sender : pullSenders)
        {
            sender->createStream(streamName);
        }
    }

    void PullServer::deleteStream()
    {
        for (const auto& sender : pullSenders)
        {
            sender->deleteStream();
        }
    }

    void PullServer::unpublishStream()
    {
        for (const auto& sender : pullSenders)
        {
            sender->unpublishStream();
        }
    }

    void PullServer::sendAudioHeader(const std::vector<uint8_t>& headerData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendAudioHeader(headerData);
        }
    }

    void PullServer::sendVideoHeader(const std::vector<uint8_t>& headerData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendVideoHeader(headerData);
        }
    }

    void PullServer::sendAudio(uint64_t timestamp, const std::vector<uint8_t>& audioData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendAudio(timestamp, audioData);
        }
    }

    void PullServer::sendVideo(uint64_t timestamp, const std::vector<uint8_t>& videoData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendVideo(timestamp, videoData);
        }
    }

    void PullServer::sendMetaData(const amf0::Node& metaData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendMetaData(metaData);
        }
    }
    
    void PullServer::sendTextData(uint64_t timestamp, const amf0::Node& textData)
    {
        for (const auto& sender : pullSenders)
        {
            sender->sendTextData(timestamp, textData);
        }
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
